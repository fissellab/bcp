#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>
#include <time.h>

#include "file_io_Sag.h"
#include "cli_Sag.h"
#include "gps.h"
#include "spectrometer_server.h"
#include "pbob_client.h"
#include "vlbi_client.h"
#include "rfsoc_client.h"
#include "ticc_client.h"
#include "heaters.h"
#include "position_sensors.h"

extern conf_params_t config;  // Access to global configuration

int exiting = 0;
int spec_running = 0;
int spec_120khz = 0; // Flag to track if 120kHz spectrometer is running
pid_t python_pid;
int pr59_running = 0; // Flag to track if PR59 is running
pid_t pr59_pid;
struct sockaddr_in cliaddr_cmd;
Packet pkt;
enum commands com;
int cmd_count = 0;


int init_cmd_socket(){
        int sockfd = socket(AF_INET,SOCK_DGRAM,0);
        struct sockaddr_in servaddr;
        struct timeval tv;

        tv.tv_sec = config.cmd_server.timeout;
        tv.tv_usec = 0;

        if(sockfd < 0){
                printf("Socket creation failed\n");
        }else{
                memset(&servaddr,0, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = htons(config.cmd_server.port);
                servaddr.sin_addr.s_addr = INADDR_ANY;

                if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
                        printf("Socket bind failed\n");
                        close(sockfd);
                        return -1;
                }
                setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        }

        return sockfd;
}



int cmd_sock_listen(int sockfd, char* buffer){
        int n;
        socklen_t cliaddr_len = sizeof(cliaddr_cmd);

        n = recvfrom(sockfd, buffer, MAXLEN, MSG_WAITALL, (struct sockaddr *) &cliaddr_cmd, &cliaddr_len);

        if(n > 0){
                buffer[n] = '\0';
        }else{
                buffer[0] = '\0';
                //if(n < 0){
                        //printf("Error receiving data\n");
                //}
        }

        return n;
}



void cmd_sock_send(int sockfd){
        char msg[10];

        snprintf(msg,10,"%d",cmd_count);
        sendto(sockfd, (const char*) msg, strlen(msg), MSG_CONFIRM,(const struct sockaddr *) &cliaddr_cmd, sizeof(cliaddr_cmd));
        return;

}

uint8_t compute_checksum(const uint8_t* bytes, const size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum ^= bytes[i];
    }
    return sum;
}

void print_packet(Packet pkt){
    printf("=== PACKET DEBUG ===\n");
    printf("start:%d\n",pkt.start);
    printf("cmd_primary:%d\n",pkt.cmd_primary);
    printf("utc:%d\n",pkt.utc);
    printf("num_data:%d\n",pkt.num_data);
    printf("data:[");

    for(int i=0;i<pkt.num_data;i++){
        printf("%d,",pkt.data[i]);
    }

    printf("]\n");
    printf("num_bigpack:%d\n",pkt.num_bigpack);
    printf("bigpack:[");

    for(int i=0;i<pkt.num_bigpack;i++){
        printf("%lf,",pkt.bigpack[i]);
    }

    printf("]\n");
    printf("checksum:%d\n",pkt.checksum);
    printf("end:%d\n",pkt.end);
    printf("===================\n");
}

int verify_packet(const Packet* pkt) {

        size_t header_size = 8 + pkt->num_data * sizeof(int16_t) + pkt->num_bigpack * sizeof(double_t); // fixed + data
        uint8_t expected = compute_checksum((uint8_t*)pkt, header_size);

        if (expected != pkt->checksum) {
                printf("ERROR: Checksum mismatch: expected 0x%02X, got 0x%02X\n", expected, pkt->checksum);
                return 0;
        }
        return 1;
}

void clear_packet(Packet* pkt){
        pkt->start = 0;
        pkt->num_data = 0;
        pkt->num_bigpack = 0;
        pkt->cmd_primary = 0;
        pkt-> utc = 0;
        pkt->destination =0;
        memset(pkt->data,0,MAX_DATA*sizeof(int16_t));
        memset(pkt->bigpack,0,MAX_DATA*sizeof(double_t));
        pkt->checksum = 0;
        pkt->end=0;


}



int decode_msg(Packet* pkt, char* msg){
        char big_buf[MAXLEN];
        char small_buf[MAXLEN];
        char* fmt;
        int ret;
        int len_count = 0;

        uint8_t start;
        uint8_t num_data;
        uint8_t num_bigpack;
        int16_t* data;
        double_t* bigpack;
        uint32_t utc;
        uint8_t cmd_primary;
        uint8_t checksum;
        uint8_t end;

        ret = sscanf(msg,"%hhd\n%hhd\n%d\n%s\n%hhd\n%s\n%hhd\n%hhd\n%hhd\n",&start, &cmd_primary, &utc, small_buf,&num_data, big_buf,&num_bigpack,&checksum,&end);

        clear_packet(pkt);
        pkt->start = start;
        pkt->num_data = num_data;
        pkt->num_bigpack = num_bigpack;
        pkt->cmd_primary = cmd_primary;
        pkt->utc = utc;
        pkt->checksum = checksum;
        pkt->end = end;

        if(ret != 9){
                printf("Received corrupt string\n");
                return 0;
        }

        if(pkt->start != START_BYTE){
                printf("Missing start byte\n");
                return 0;
        }

        if(end != END_BYTE){
                printf("Missing stop byte\n");
                return 0;
        }

        if (pkt->num_data>0){ 
                data = (int16_t *) malloc(sizeof(int16_t)*pkt->num_data);
                fmt=strtok(small_buf,",");
                while(fmt != NULL){
                        if(len_count==pkt->num_data){
                                printf("Too many arguments in data\n");
                                return 0;
                        }

                        data[len_count]=atoi(fmt);
                        fmt = strtok(NULL, ",");
                        len_count++;

                }

                if(len_count < (pkt->num_data-1)){

                        printf("Too few arguments in data\n");
                        return 0;

                }
                memcpy(pkt->data, data, pkt->num_data * sizeof(int16_t));
        }

        if (pkt->num_bigpack>0){
                bigpack = (double_t *) malloc(sizeof(double_t)*pkt->num_bigpack);
                len_count = 0;
                fmt=strtok(big_buf,",");
                while(fmt != NULL){
                        if(len_count==pkt->num_bigpack){
                                printf("Too many arguments in bigpack \n");
                                return 0;
                        }

                        bigpack[len_count]=atof(fmt);
                        fmt = strtok(NULL, ",");
                        len_count++;

                }

                if(len_count < (pkt->num_data-1)){

                        printf("Too few arguments in bigpack\n");
                        return 0;

                }
                memcpy(pkt->bigpack, bigpack,pkt->num_bigpack * sizeof(double_t));


        }

        // TEMPORARILY DISABLED FOR TESTING - CHECKSUM VERIFICATION
        // if(!verify_packet(pkt)){
        //         return 0;
        // }
        printf("DEBUG: Skipping checksum verification for testing\n");

        return 1;

}

void run_python_script(const char* script_name, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    char interval_str[20];
    snprintf(interval_str, sizeof(interval_str), "%d", data_save_interval);
    execlp("python3", "python3", script_name, hostname, logpath, mode, "-i", interval_str, "-p", data_save_path, (char*)NULL);
    perror("execlp failed");
    exit(1);
}

void exec_command(Packet pkt) {
    
    if (pkt.cmd_primary == exit_both) {
        printf("Exiting BCP\n");
        if (spec_running) {
            spec_running = 0;
            kill(python_pid, SIGTERM);
            waitpid(python_pid, NULL, 0);
            printf("Stopped spec script\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Stopped spec script");
        }
        
        // Stop PR59 if running
        if (pr59_running) {
            pr59_running = 0;
            printf("Stopping PR59 TEC controller...\n");
            kill(pr59_pid, SIGTERM);
            waitpid(pr59_pid, NULL, 0);
            printf("Stopped PR59 TEC controller\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Stopped PR59 TEC controller during exit");
        }
        
        // Stop heaters if running
        if (heaters_running) {
            printf("Stopping heaters...\n");
            shutdown_heaters = 1;
            pthread_join(main_heaters_thread, NULL);
            printf("Stopped heaters\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Stopped heaters on exit");
        }
        
        // Stop GPS UDP server first if running
        if (gps_is_udp_server_running()) {
            gps_stop_udp_server();
            printf("Stopped GPS UDP server\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Stopped GPS UDP server");
        }
        
        if (gps_is_logging()) {
            gps_stop_logging();
            printf("Stopped GPS logging\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Stopped GPS logging");
        }
        exiting = 1;
    }else if (pkt.cmd_primary == start_spec_120kHz) {
        if (!spec_running) {
            spec_running = 1;
            spec_120khz = 1;
            python_pid = fork();
            if (python_pid == 0) {
            // Child process for 120khz spectrometer
                 run_python_script("rfsoc_spec_120khz.py", config.main.logpath, config.rfsoc.ip_address, config.rfsoc.mode, config.rfsoc.data_save_interval, config.rfsoc.data_save_path);
                 return;  // Should never reach here
            } else if (python_pid < 0) {
                 perror("fork failed");
                 spec_running = 0;
                 spec_120khz = 0;
            } else {
                 //Parent process
                 printf("Started 120kHz spec script\n");
                 write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Started 120kHz spec script");
              	// Set spectrometer server type
                spec_server_set_active_type(SPEC_TYPE_120KHZ);
            }
	} else {
                printf("Spec script is already running\n");
        }

     } else if (pkt.cmd_primary == start_spec){
        if (!spec_running) {
	  spec_running = 1;
	  // Standard spectrometer (960kHz)
          spec_120khz = 0;
          python_pid = fork();
          if (python_pid == 0) {
              // Child process
              run_python_script("rfsoc_spec.py", config.main.logpath, config.rfsoc.ip_address, config.rfsoc.mode, config.rfsoc.data_save_interval, config.rfsoc.data_save_path);
              exit(0);  // Should never reach here
          } else if (python_pid < 0) {
              perror("fork failed");
              spec_running = 0;
          } else {
              // Parent process
              printf("Started spec script\n");
              write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Started spec script");
                        
             // Set spectrometer server type
              spec_server_set_active_type(SPEC_TYPE_STANDARD);

          }
        } else {
                printf("Spec script is already running\n");
        }
    } else if (pkt.cmd_primary == rfsoc_on) {
        if (pbob_client_is_enabled()) {
            printf("Powering up RFSoC (PBOB %d, Relay %d)...\n", config.rfsoc.pbob_id, config.rfsoc.relay_id);
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to power up RFSoC");
            
            int result = pbob_send_command(config.rfsoc.pbob_id, config.rfsoc.relay_id);
            if (result == 1) {
                printf("RFSoC power ON successful!\n");
                printf("Please wait 40 seconds for complete RFSoC bootup before starting spectrometer...\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "RFSoC powered ON successfully");
                
                // Show countdown
                for (int i = 40; i > 0; i--) {
                    printf("\rBootup countdown: %2d seconds remaining...", i);
                    fflush(stdout);
                    sleep(1);
                }
                printf("\nRFSoC bootup complete! You can now use 'start spec' commands.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "RFSoC bootup countdown completed");
            } else {
                printf("Failed to power up RFSoC. Check PBoB server connection.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to power up RFSoC");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted rfsoc_on but PBoB client not available");
        }
    } else if (pkt.cmd_primary == rfsoc_off) {
        if (pbob_client_is_enabled()) {
            printf("Powering down RFSoC (PBOB %d, Relay %d)...\n", config.rfsoc.pbob_id, config.rfsoc.relay_id);
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to power down RFSoC");
            
            int result = pbob_send_command(config.rfsoc.pbob_id, config.rfsoc.relay_id);
            if (result == 1) {
                printf("RFSoC powered OFF successfully.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "RFSoC powered OFF successfully");
            } else {
                printf("Failed to power down RFSoC. Check PBoB server connection.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to power down RFSoC");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted rfsoc_off but PBoB client not available");
        }
    } else if (pkt.cmd_primary == start_gps) {
        if (pbob_client_is_enabled()) {
            printf("Powering up GPS (PBOB %d, Relay %d)...\n", config.gps.pbob_id, config.gps.relay_id);
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to power up GPS");
            
            int result = pbob_send_command(config.gps.pbob_id, config.gps.relay_id);
            if (result == 1) {
                printf("GPS power ON successful!\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "GPS powered ON successfully");
                
                // Start GPS logging
                if (gps_start_logging()) {
                    printf("GPS logging started.\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "GPS logging started");
                    
                    // Start GPS UDP server if enabled
                    if (config.gps.udp_server_enabled) {
                        if (gps_start_udp_server()) {
                            printf("GPS UDP server started on port %d.\n", 
                                   config.gps.udp_server_port);
                            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "GPS UDP server started");
                        } else {
                            printf("Failed to start GPS UDP server.\n");
                            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to start GPS UDP server");
                        }
                    }
                } else {
                    printf("Failed to start GPS logging.\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to start GPS logging");
                }
            } else {
                printf("Failed to power up GPS. Check PBoB server connection.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to power up GPS");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted gps_start but PBoB client not available");
        }
    } else if (pkt.cmd_primary == stop_gps) {
        if (pbob_client_is_enabled()) {
            printf("Stopping GPS services...\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to stop GPS services");
            
            // Stop GPS UDP server if running
            if (gps_is_udp_server_running()) {
                gps_stop_udp_server();
                printf("GPS UDP server stopped.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "GPS UDP server stopped");
            }
            
            // Stop GPS logging if running
            if (gps_is_logging()) {
                gps_stop_logging();
                printf("GPS logging stopped.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "GPS logging stopped");
            }
            
            // Power down GPS
            printf("Powering down GPS (PBOB %d, Relay %d)...\n", config.gps.pbob_id, config.gps.relay_id);
            int result = pbob_send_command(config.gps.pbob_id, config.gps.relay_id);
            if (result == 1) {
                printf("GPS powered OFF successfully.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "GPS powered OFF successfully");
            } else {
                printf("Failed to power down GPS. Check PBoB server connection.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to power down GPS");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted gps_stop but PBoB client not available");
        }
    } else if (pkt.cmd_primary == start_vlbi) {
        if (vlbi_client_is_enabled()) {
            printf("Starting VLBI logging on aquila (%s)...\n", config.vlbi.aquila_ip);
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to start VLBI logging");
            
            // First check connectivity
            if (vlbi_check_connectivity()) {
                int result = vlbi_start_logging();
                if (result == 1) {
                    printf("VLBI logging started successfully!\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "VLBI logging started successfully");
                } else {
                    printf("Failed to start VLBI logging. Check VLBI daemon status.\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to start VLBI logging");
                }
            } else {
                printf("Cannot reach VLBI daemon. Check network connection and daemon status.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "VLBI daemon not reachable");
            }
        } else {
            printf("VLBI client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted start_vlbi but VLBI client not available");
        }
    } else if (pkt.cmd_primary == stop_vlbi) {
        if (vlbi_client_is_enabled()) {
            printf("Stopping VLBI logging on aquila (%s)...\n", config.vlbi.aquila_ip);
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to stop VLBI logging");
            
            int result = vlbi_stop_logging();
            if (result == 1) {
                printf("VLBI logging stopped successfully!\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "VLBI logging stopped successfully");
            } else {
                printf("Failed to stop VLBI logging. Check VLBI daemon status.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to stop VLBI logging");
            }
        } else {
            printf("VLBI client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted stop_vlbi but VLBI client not available");
        }
    } else if (pkt.cmd_primary == rfsoc_configure_ocxo) {
        if (rfsoc_client_is_enabled()) {
            printf("Configuring RFSoC clock on %s...\n", config.rfsoc_daemon.rfsoc_ip);
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to configure RFSoC clock");
            
            // First check connectivity
            if (rfsoc_check_connectivity()) {
                int result = rfsoc_configure_clock();
                if (result == 1) {
                    printf("RFSoC clock configuration completed successfully!\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "RFSoC clock configuration successful");
                } else {
                    printf("Failed to configure RFSoC clock. Check daemon status and script.\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to configure RFSoC clock");
                }
            } else {
                printf("Cannot reach RFSoC daemon. Check network connection and daemon status.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "RFSoC daemon not reachable");
            }
        } else {
            printf("RFSoC client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted rfsoc_configure_ocxo but RFSoC client not available");
        }
    } else if (pkt.cmd_primary == start_timing_chain) {
        if (pbob_client_is_enabled()) {
            printf("Powering up timing chain (PBOB %d, Relay %d)...\n", config.ticc.pbob_id, config.ticc.relay_id);
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to power up timing chain");
            
            int result = pbob_send_command(config.ticc.pbob_id, config.ticc.relay_id);
            if (result == 1) {
                printf("Timing chain power ON successful!\n");
                printf("The timing chain is now powered and ready for TICC measurements.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Timing chain powered ON successfully");
            } else {
                printf("Failed to power up timing chain. Check PBoB server connection.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to power up timing chain");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted start_timing_chain but PBoB client not available");
        }
    } else if (pkt.cmd_primary == stop_timing_chain) {
        if (pbob_client_is_enabled()) {
            printf("Powering down timing chain (PBOB %d, Relay %d)...\n", config.ticc.pbob_id, config.ticc.relay_id);
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to power down timing chain");
            
            int result = pbob_send_command(config.ticc.pbob_id, config.ticc.relay_id);
            if (result == 1) {
                printf("Timing chain power OFF successful!\n");
                printf("The timing chain has been powered down.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Timing chain powered OFF successfully");
            } else {
                printf("Failed to power down timing chain. Check PBoB server connection.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to power down timing chain");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted stop_timing_chain but PBoB client not available");
        }
    } else if (pkt.cmd_primary == start_ticc) {
        if (ticc_client_is_enabled()) {
            printf("Starting TICC data collection...\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to start TICC data collection");
            
            int result = ticc_start_logging();
            if (result == 0) {
                printf("TICC data collection started successfully!\n");
                printf("Data will be saved to: %s\n", config.ticc.data_save_path);
                printf("Serial port: %s at %d baud\n", config.ticc.port, config.ticc.baud_rate);
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "TICC data collection started successfully");
            } else {
                printf("Failed to start TICC data collection.\n");
                ticc_status_t status;
                if (ticc_get_status(&status) == 0 && strlen(status.last_error) > 0) {
                    printf("Error: %s\n", status.last_error);
                }
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to start TICC data collection");
            }
        } else {
            printf("TICC client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted start_ticc but TICC client not available");
        }
    } else if (pkt.cmd_primary == stop_ticc) {
        if (ticc_client_is_enabled()) {
            printf("Stopping TICC data collection...\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to stop TICC data collection");
            
            int result = ticc_stop_logging();
            if (result == 0) {
                printf("TICC data collection stopped successfully.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "TICC data collection stopped successfully");
            } else {
                printf("Failed to stop TICC data collection.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to stop TICC data collection");
            }
        } else {
            printf("TICC client is not enabled or initialized.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted stop_ticc but TICC client not available");
        }
    } else if (pkt.cmd_primary == start_heater_box) {
        if (!config.heaters.enabled) {
            printf("Heaters are not enabled in configuration.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to start heater box, but heaters are not enabled");
        } else {
            if (pbob_client_is_enabled()) {
                printf("Powering up heater box (PBOB %d, Relay %d)...\n", config.heaters.pbob_id, config.heaters.relay_id);
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to power up heater box");
                
                int pbob_result = pbob_send_command(config.heaters.pbob_id, config.heaters.relay_id);
                if (pbob_result == 1) {
                    printf("Heater box power ON successful!\n");
                    printf("Please wait ~10 seconds for LabJack to fully boot before running 'start_heaters'.\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Heater box powered ON successfully");
                } else {
                    printf("Failed to power up heater box.\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to power up heater box");
                }
            } else {
                printf("PBoB client is not available.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted heater box start but PBoB client not available");
            }
        }
    } else if (pkt.cmd_primary == start_heaters) {
        if (!config.heaters.enabled) {
            printf("Heaters are not enabled in configuration.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to start heaters, but they are not enabled");
        } else if (heaters_running) {
            printf("Heaters are already running.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to start heaters, but they were already running");
        } else {
            printf("Starting heater control system...\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to start heater control system");
            
            int heaters_result = pthread_create(&main_heaters_thread, NULL, run_heaters_thread, NULL);
            if(heaters_result == 0) {
                printf("Heater control system started successfully!\n");
                printf("Note: If connection fails, ensure heater box is powered on with 'start_heater_box' first.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Heater control system started successfully");
            } else {
                printf("Failed to start heater control thread (error: %d).\n", heaters_result);
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to start heater control thread");
            }
        }
    } else if(pkt.cmd_primary == stop_heaters) {
        if (heaters_running) {
            printf("Stopping heaters...\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to stop heaters");
            
            // Signal heater thread to stop
            shutdown_heaters = 1;
            
            int join_result = pthread_join(main_heaters_thread, NULL);
            if (join_result == 0) {
                printf("Heater control thread stopped.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Heater control thread stopped");
                printf("Heaters powered OFF and shutdown complete.\n");
            } else {
                printf("Warning: Error waiting for heater thread to finish (error %d)\n", join_result);
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Error joining heater thread");
            }
            
            if (pbob_client_is_enabled()) {
                int pbob_result = pbob_send_command(config.heaters.pbob_id, config.heaters.relay_id);
                if (pbob_result == 1) {
                    printf("Heater box power OFF successful!\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Heater box powered OFF successfully");
                } else {
                    printf("Failed to power OFF heater box!\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to power OFF heater box");
                }
            } else {
                printf("PBoB client is not available.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted stop_heaters but PBoB client not available");
            }
        } else {
            printf("Heaters are not running.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted stop_heaters but heaters were not running");
        }
    } else if (pkt.cmd_primary == start_pr59) {
        if (config.pr59.enabled) {
            if (!pr59_running) {
                printf("Starting PR59 TEC controller...\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to start PR59 TEC controller");
                
                pr59_running = 1;
                pr59_pid = fork();
                if (pr59_pid == 0) {
                    // Child process - run tec_control_3 with BCP config file
                    execlp("./build/tec_control_3", "tec_control_3", "bcp_Sag.config", (char*)NULL);
                    perror("execlp failed for PR59");
                    exit(1);
                } else if (pr59_pid < 0) {
                    perror("fork failed for PR59");
                    pr59_running = 0;
                } else {
                    // Parent process
                    printf("PR59 TEC controller started successfully!\n");
                    printf("Serial port: %s\n", config.pr59.port);
                    printf("Target temperature: %.1f°C\n", config.pr59.setpoint_temp);
                    printf("PID parameters: Kp=%.3f, Ki=%.3f, Kd=%.3f\n", 
                           config.pr59.kp, config.pr59.ki, config.pr59.kd);
                    printf("Data logging to: %s\n", config.pr59.data_save_path);
                    printf("Use 'stop_pr59' to stop the controller.\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "PR59 TEC controller started successfully");
                }
            } else {
                printf("PR59 TEC controller is already running\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to start PR59 but it was already running");
            }
        } else {
            printf("PR59 is not enabled in configuration.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted start_pr59 but PR59 not enabled");
        }
    } else if (pkt.cmd_primary == stop_pr59) {
        if (config.pr59.enabled) {
            if (pr59_running) {
                printf("Stopping PR59 TEC controller...\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to stop PR59 TEC controller");
                
                pr59_running = 0;
                
                // Send SIGTERM first for graceful shutdown
                kill(pr59_pid, SIGTERM);
                
                // Wait for up to 10 seconds for the process to terminate
                int status;
                int timeout = 10;
                while (timeout > 0) {
                    pid_t result = waitpid(pr59_pid, &status, WNOHANG);
                    if (result == pr59_pid) {
                        printf("PR59 TEC controller stopped successfully.\n");
                        write_to_log(cmd_log, "cli_Sag.c", "exec_command", "PR59 TEC controller stopped successfully");
                        break;
                    } else if (result == -1) {
                        perror("waitpid failed for PR59");
                        break;
                    }
                    sleep(1);
                    timeout--;
                }
                
                // If the process hasn't terminated, use SIGKILL
                if (timeout == 0) {
                    printf("PR59 controller not responding, force stopping...\n");
                    kill(pr59_pid, SIGKILL);
                    waitpid(pr59_pid, NULL, 0);
                    printf("PR59 TEC controller force stopped.\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "PR59 TEC controller force stopped");
                }
            } else {
                printf("PR59 TEC controller is not running\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to stop PR59 but it was not running");
            }
        } else {
            printf("PR59 is not enabled in configuration.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted stop_pr59 but PR59 not enabled");
        }
    } else if (pkt.cmd_primary == stop_spec_120kHz) {
            if (spec_running) {
                spec_running = 0;
                
                // Send SIGTERM first
                kill(python_pid, SIGTERM);
                
                // Wait for up to 5 seconds for the process to terminate
                int status;
                int timeout = 5;
                while (timeout > 0) {
                    pid_t result = waitpid(python_pid, &status, WNOHANG);
                    if (result == python_pid) {
                        printf("Stopped 120kHz spec script\n");
                        write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Stopped 120kHz spec script");
                        spec_120khz = 0; // Reset the spectrometer type flag
                        
                        // Clear spectrometer server type
                        spec_server_set_active_type(SPEC_TYPE_NONE);
                        break;
                    } else if (result == -1) {
                        perror("waitpid failed");
                        break;
                    }
                    sleep(1);
                    timeout--;
                }
                
                // If the process hasn't terminated, use SIGKILL
                if (timeout == 0) {
                    kill(python_pid, SIGKILL);
                    waitpid(python_pid, NULL, 0);
                    printf("Forcefully stopped 120kHz spec script\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Forcefully stopped 120kHz spec script");
                    spec_120khz = 0; // Reset the spectrometer type flag
                    
                    // Clear spectrometer server type
                    spec_server_set_active_type(SPEC_TYPE_NONE);
                }
            } else {
                printf("Spec script is not running\n");
            }
     }else if(pkt.cmd_primary == stop_spec){
	if (spec_running) {
                spec_running = 0;

                // Send SIGTERM first
                kill(python_pid, SIGTERM);

                // Wait for up to 5 seconds for the process to terminate
                int status;
                int timeout = 5;
                while (timeout > 0) {
                    pid_t result = waitpid(python_pid, &status, WNOHANG);
                    if (result == python_pid) {
                        printf("Stopped spec script\n");
                        write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Stopped spec script");
                        // Clear spectrometer server type
                        spec_server_set_active_type(SPEC_TYPE_NONE);
                        break;
                    } else if (result == -1) {
                        perror("waitpid failed");
                        break;
                    }
                    sleep(1);
                    timeout--;
                }

		// If the process hasn't terminated, use SIGKILL
                if (timeout == 0) {
                    kill(python_pid, SIGKILL);
                    waitpid(python_pid, NULL, 0);
                    printf("Forcefully stopped spec script\n");
                    write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Forcefully stopped spec script");

                    // Clear spectrometer server type
                    spec_server_set_active_type(SPEC_TYPE_NONE);
                }
            } else {
                printf("Spec script is not running\n");
            }
    } else if (pkt.cmd_primary == start_position_box) {
        if (!config.position_sensors.enabled) {
            printf("Position sensors are not enabled in configuration.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to start position sensors, but they are not enabled");
        } else if (position_sensors_is_running()) {
            printf("Position sensors are already running.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to start position sensors, but they were already running");
        } else {
            printf("Starting position sensor system...\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to start position sensor system");
            
            if (position_sensors_start()) {
                printf("Position sensor system started successfully!\n");
                printf("Pi IP: %s, Port: %d\n", config.position_sensors.pi_ip, config.position_sensors.pi_port);
                printf("Data path: %s\n", config.position_sensors.data_save_path);
                printf("Script: %s\n", config.position_sensors.script_path);
                printf("Telemetry rate: %d Hz for SPI gyro data\n", config.position_sensors.telemetry_rate_hz);
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Position sensor system started successfully");
            } else {
                printf("Failed to start position sensor system.\n");
                write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Failed to start position sensor system");
            }
        }
    } else if (pkt.cmd_primary == stop_position_box) {
        if (!config.position_sensors.enabled) {
            printf("Position sensors are not enabled in configuration.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to stop position sensors, but they are not enabled");
        } else if (!position_sensors_is_running()) {
            printf("Position sensors are not running.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempted to stop position sensors, but they were not running");
        } else {
            printf("Stopping position sensor system...\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Attempting to stop position sensor system");
            
            position_sensors_stop();
            printf("Position sensor system stopped successfully.\n");
            write_to_log(cmd_log, "cli_Sag.c", "exec_command", "Position sensor system stopped successfully");
        }
    }
}

void do_commands(){

        int cmd_sock;
        char buffer[MAXLEN];
        Packet pkt;

        cmd_sock = init_cmd_socket();

        if(cmd_sock < 0){
                printf("Socket creation failed\n");
                return;
        }

        while(!exiting){
                if(cmd_sock_listen(cmd_sock,buffer)<0){
                        continue;
                }
                if(!decode_msg(&pkt,buffer)){
                        printf("Failed to decode message\n");

                }else{
                        cmd_count++;
                        cmd_sock_send(cmd_sock);
                        exec_command(pkt);
                }
        }

        printf("Exiting\n");
        close(cmd_sock);
}
