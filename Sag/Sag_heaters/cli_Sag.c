#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include "file_io_Sag.h"
#include "cli_Sag.h"
#include "gps.h"
#include "spectrometer_server.h"
#include "pbob_client.h"
#include "vlbi_client.h"
#include "rfsoc_client.h"
#include "ticc_client.h"
#include "heaters.h"

extern conf_params_t config;  // Access to global configuration

int exiting = 0;
int spec_running = 0;
int spec_120khz = 0; // Flag to track if 120kHz spectrometer is running
pid_t python_pid;

void run_python_script(const char* script_name, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    char interval_str[20];
    snprintf(interval_str, sizeof(interval_str), "%d", data_save_interval);
    execlp("python3", "python3", script_name, hostname, logpath, mode, "-i", interval_str, "-p", data_save_path, (char*)NULL);
    perror("execlp failed");
    exit(1);
}

void exec_command(char* input, FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    char* arg = (char*) malloc(strlen(input) * sizeof(char));
    char* cmd = (char*) malloc(strlen(input) * sizeof(char));
    
    // Additional argument to capture optional "120khz" parameter
    char* sub_arg = (char*) malloc(strlen(input) * sizeof(char));
    sub_arg[0] = '\0'; // Initialize to empty string
    
    int scan = sscanf(input, "%s %[^\t\n]", cmd, arg);

    if (strcmp(cmd, "print") == 0) {
        if (scan == 1) {
            printf("print is missing argument usage is print <string>\n");
        } else {
            printf("%s\n", arg);
        }
    } else if (strcmp(cmd, "exit") == 0) {
        printf("Exiting BCP\n");
        
        // Stop heaters if running
        if (heaters_running) {
            printf("Stopping heaters...\n");
            shutdown_heaters = 1;
            pthread_join(main_heaters_thread, NULL);
            printf("Stopped heaters\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped heaters on exit");
        }
        
        if (spec_running) {
            spec_running = 0;
            kill(python_pid, SIGTERM);
            waitpid(python_pid, NULL, 0);
            printf("Stopped spec script\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped spec script");
        
        // Stop GPS UDP server first if running
        if (gps_is_udp_server_running()) {
            gps_stop_udp_server();
            printf("Stopped GPS UDP server\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS UDP server");
        }
        
        if (gps_is_logging()) {
            gps_stop_logging();
            printf("Stopped GPS logging\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS logging");
            }
        }
        exiting = 1;
    } else if (strcmp(cmd, "start") == 0) {
        // Check for "start spec" or "start spec 120khz"
        sscanf(arg, "%s %s", sub_arg, arg + strlen(sub_arg) + 1);
        
        if (strcmp(sub_arg, "spec") == 0) {
            if (!spec_running) {
                spec_running = 1;
                
                // Check if we're starting the 120khz version
                if (strlen(arg) > strlen(sub_arg) && strstr(arg, "120khz") != NULL) {
                    spec_120khz = 1;
                    python_pid = fork();
                    if (python_pid == 0) {
                        // Child process for 120khz spectrometer
                        run_python_script("rfsoc_spec_120khz.py", logpath, hostname, mode, data_save_interval, data_save_path);
                        exit(0);  // Should never reach here
                    } else if (python_pid < 0) {
                        perror("fork failed");
                        spec_running = 0;
                        spec_120khz = 0;
                    } else {
                        // Parent process
                        printf("Started 120kHz spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started 120kHz spec script");
                        
                        // Set spectrometer server type
                        spec_server_set_active_type(SPEC_TYPE_120KHZ);
                    }
                } else {
                    // Standard spectrometer (960kHz)
                    spec_120khz = 0;
                    python_pid = fork();
                    if (python_pid == 0) {
                        // Child process
                        run_python_script("rfsoc_spec.py", logpath, hostname, mode, data_save_interval, data_save_path);
                        exit(0);  // Should never reach here
                    } else if (python_pid < 0) {
                        perror("fork failed");
                        spec_running = 0;
                    } else {
                        // Parent process
                        printf("Started spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started spec script");
                        
                        // Set spectrometer server type
                        spec_server_set_active_type(SPEC_TYPE_STANDARD);
                    }
                }
            } else {
                printf("Spec script is already running\n");
            }
        } else if (strcmp(sub_arg, "gps") == 0) {
            if (!gps_is_logging()) {
                if (gps_start_logging()) {
                    printf("Started GPS logging\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started GPS logging");
                } else {
                    printf("Failed to start GPS logging\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to start GPS logging");
                }
            } else {
                printf("GPS logging is already active\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to start GPS logging, but it was already active");
            }
        } else if (strcmp(sub_arg, "heaters") == 0) {
            if (!heaters_running && config.heaters.enabled) {
                if (pbob_client_is_enabled()) {
                    printf("Powering up heaters (PBOB %d, Relay %d)...\n", config.heaters.pbob_id, config.heaters.relay_id);
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to power up heaters");
                    
                    int pbob_result = pbob_send_command(config.heaters.pbob_id, config.heaters.relay_id);
                    if (pbob_result == 1) {
                        printf("Heater box power ON successful!\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Heater box powered ON successfully");

                        int heaters_result = pthread_create(&main_heaters_thread, NULL, run_heaters_thread, NULL);
                        if(heaters_result == 1) {
                            printf("Heaters power ON successful!\n");
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Heaters powered ON successfully");
                        } else{
                            printf("Failed to power up heaters.\n");
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power up heaters");
                        }
                    } else {
                        printf("Failed to power up heater box.\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power up heater box");
                    }
                } else {
                    printf("PBoB client is not enabled or initialized.\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted heaters start but PBoB client not available");
                }
            } else if(!config.heaters.enabled) {
                printf("Heaters are not enabled\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to start heaters, but they are not enabled");
            } else {
                printf("Heaters are already running\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to start heaters, but they were already running");
            }
        } else {
            printf("Unknown start command: %s\n", sub_arg);
        }
    } else if(strcmp(cmd, "heaters_stop") == 0) {
        if (heaters_running) {
            printf("Stopping heaters...\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to stop heaters");
            
            // Set shutdown flag
            shutdown_heaters = 1;
            
            int join_result = pthread_join(main_heaters_thread, NULL);
            if (join_result == 0) {
                printf("Heater control thread stopped.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Heater control thread stopped");
                printf("Heaters powered OFF and shutdown complete.\n");
            } else {
                printf("Warning: Error waiting for heater thread to finish (error %d)\n", join_result);
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Error joining heater thread");
            }
            
            int pbob_result = pbob_send_command(config.heaters.pbob_id, config.heaters.relay_id);
            if (pbob_result == 1) {
                printf("Heater box power OFF successful!\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Heater box powered OFF successfully");
            } else {
                printf("Failed to power OFF heater box!\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power OFF heater box");
            }
        } else if (!pbob_client_is_enabled()) {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted heaters_stop but PBoB client not available");
        } else {
            printf("Heaters are not running.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted heaters_stop but heaters were not running");
        }
    }
    
    else if (strcmp(cmd, "rfsoc_on") == 0) {
        if (pbob_client_is_enabled()) {
            printf("Powering up RFSoC (PBOB %d, Relay %d)...\n", config.rfsoc.pbob_id, config.rfsoc.relay_id);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to power up RFSoC");
            
            int result = pbob_send_command(config.rfsoc.pbob_id, config.rfsoc.relay_id);
            if (result == 1) {
                printf("RFSoC power ON successful!\n");
                printf("Please wait 40 seconds for complete RFSoC bootup before starting spectrometer...\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "RFSoC powered ON successfully");
                
                // Show countdown
                for (int i = 40; i > 0; i--) {
                    printf("\rBootup countdown: %2d seconds remaining...", i);
                    fflush(stdout);
                    sleep(1);
                }
                printf("\nRFSoC bootup complete! You can now use 'start spec' commands.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "RFSoC bootup countdown completed");
            } else {
                printf("Failed to power up RFSoC. Check PBoB server connection.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power up RFSoC");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted rfsoc_on but PBoB client not available");
        }
    } else if (strcmp(cmd, "rfsoc_off") == 0) {
        if (pbob_client_is_enabled()) {
            printf("Powering down RFSoC (PBOB %d, Relay %d)...\n", config.rfsoc.pbob_id, config.rfsoc.relay_id);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to power down RFSoC");
            
            int result = pbob_send_command(config.rfsoc.pbob_id, config.rfsoc.relay_id);
            if (result == 1) {
                printf("RFSoC powered OFF successfully.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "RFSoC powered OFF successfully");
            } else {
                printf("Failed to power down RFSoC. Check PBoB server connection.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power down RFSoC");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted rfsoc_off but PBoB client not available");
        }
    } else if (strcmp(cmd, "gps_start") == 0) {
        if (pbob_client_is_enabled()) {
            printf("Powering up GPS (PBOB %d, Relay %d)...\n", config.gps.pbob_id, config.gps.relay_id);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to power up GPS");
            
            int result = pbob_send_command(config.gps.pbob_id, config.gps.relay_id);
            if (result == 1) {
                printf("GPS power ON successful!\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "GPS powered ON successfully");
                
                // Start GPS logging
                if (gps_start_logging()) {
                    printf("GPS logging started.\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "GPS logging started");
                    
                    // Start GPS UDP server if enabled
                    if (config.gps.udp_server_enabled) {
                        if (gps_start_udp_server()) {
                            printf("GPS UDP server started on port %d.\n", 
                                   config.gps.udp_server_port);
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "GPS UDP server started");
                        } else {
                            printf("Failed to start GPS UDP server.\n");
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to start GPS UDP server");
                        }
                    }
                } else {
                    printf("Failed to start GPS logging.\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to start GPS logging");
                }
            } else {
                printf("Failed to power up GPS. Check PBoB server connection.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power up GPS");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted gps_start but PBoB client not available");
        }
    } else if (strcmp(cmd, "gps_stop") == 0) {
        if (pbob_client_is_enabled()) {
            printf("Stopping GPS services...\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to stop GPS services");
            
            // Stop GPS UDP server if running
            if (gps_is_udp_server_running()) {
                gps_stop_udp_server();
                printf("GPS UDP server stopped.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "GPS UDP server stopped");
            }
            
            // Stop GPS logging if running
            if (gps_is_logging()) {
                gps_stop_logging();
                printf("GPS logging stopped.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "GPS logging stopped");
            }
            
            // Power down GPS
            printf("Powering down GPS (PBOB %d, Relay %d)...\n", config.gps.pbob_id, config.gps.relay_id);
            int result = pbob_send_command(config.gps.pbob_id, config.gps.relay_id);
            if (result == 1) {
                printf("GPS powered OFF successfully.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "GPS powered OFF successfully");
            } else {
                printf("Failed to power down GPS. Check PBoB server connection.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power down GPS");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted gps_stop but PBoB client not available");
        }
    } else if (strcmp(cmd, "start_vlbi") == 0) {
        if (vlbi_client_is_enabled()) {
            printf("Starting VLBI logging on aquila (%s)...\n", config.vlbi.aquila_ip);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to start VLBI logging");
            
            // First check connectivity
            if (vlbi_check_connectivity()) {
                int result = vlbi_start_logging();
                if (result == 1) {
                    printf("VLBI logging started successfully!\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "VLBI logging started successfully");
                } else {
                    printf("Failed to start VLBI logging. Check VLBI daemon status.\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to start VLBI logging");
                }
            } else {
                printf("Cannot reach VLBI daemon. Check network connection and daemon status.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "VLBI daemon not reachable");
            }
        } else {
            printf("VLBI client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted start_vlbi but VLBI client not available");
        }
    } else if (strcmp(cmd, "stop_vlbi") == 0) {
        if (vlbi_client_is_enabled()) {
            printf("Stopping VLBI logging on aquila (%s)...\n", config.vlbi.aquila_ip);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to stop VLBI logging");
            
            int result = vlbi_stop_logging();
            if (result == 1) {
                printf("VLBI logging stopped successfully!\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "VLBI logging stopped successfully");
            } else {
                printf("Failed to stop VLBI logging. Check VLBI daemon status.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to stop VLBI logging");
            }
        } else {
            printf("VLBI client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted stop_vlbi but VLBI client not available");
        }
    } else if (strcmp(cmd, "vlbi_status") == 0) {
        if (vlbi_client_is_enabled()) {
            vlbi_status_t status;
            printf("Checking VLBI status on aquila (%s)...\n", config.vlbi.aquila_ip);
            
            int result = vlbi_get_status(&status);
            if (result == 1) {
                printf("VLBI Status:\n");
                printf("  Running: %s\n", status.is_running ? "Yes" : "No");
                if (status.is_running && status.pid > 0) {
                    printf("  Process ID: %d\n", status.pid);
                }
                if (strlen(status.timestamp) > 0) {
                    printf("  Last Update: %s\n", status.timestamp);
                }
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "VLBI status check successful");
            } else {
                printf("Failed to get VLBI status.\n");
                if (strlen(status.last_error) > 0) {
                    printf("Error: %s\n", status.last_error);
                }
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to get VLBI status");
            }
        } else {
            printf("VLBI client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted vlbi_status but VLBI client not available");
        }
    } else if (strcmp(cmd, "vlbi_check") == 0) {
        if (vlbi_client_is_enabled()) {
            printf("Checking VLBI daemon connectivity...\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Checking VLBI daemon connectivity");
            
            int result = vlbi_check_connectivity();
            if (result == 1) {
                printf("VLBI daemon connectivity check passed!\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "VLBI connectivity check successful");
            } else {
                printf("VLBI daemon connectivity check failed!\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "VLBI connectivity check failed");
            }
        } else {
            printf("VLBI client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted vlbi_check but VLBI client not available");
        }
    } else if (strcmp(cmd, "rfsoc_configure_ocxo") == 0) {
        if (rfsoc_client_is_enabled()) {
            printf("Configuring RFSoC clock on %s...\n", config.rfsoc_daemon.rfsoc_ip);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to configure RFSoC clock");
            
            // First check connectivity
            if (rfsoc_check_connectivity()) {
                int result = rfsoc_configure_clock();
                if (result == 1) {
                    printf("RFSoC clock configuration completed successfully!\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "RFSoC clock configuration successful");
                } else {
                    printf("Failed to configure RFSoC clock. Check daemon status and script.\n");
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to configure RFSoC clock");
                }
            } else {
                printf("Cannot reach RFSoC daemon. Check network connection and daemon status.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "RFSoC daemon not reachable");
            }
        } else {
            printf("RFSoC client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted rfsoc_configure_ocxo but RFSoC client not available");
        }
    } else if (strcmp(cmd, "rfsoc_clock_status") == 0) {
        if (rfsoc_client_is_enabled()) {
            rfsoc_clock_status_t status;
            printf("Checking RFSoC clock status on %s...\n", config.rfsoc_daemon.rfsoc_ip);
            
            int result = rfsoc_get_clock_status(&status);
            if (result == 1) {
                printf("RFSoC Clock Status:\n");
                printf("  Script Available: %s\n", status.script_available ? "Yes" : "No");
                printf("  Script Executable: %s\n", status.script_executable ? "Yes" : "No");
                printf("  Script Path: %s\n", status.script_path);
                if (strlen(status.timestamp) > 0) {
                    printf("  Last Check: %s\n", status.timestamp);
                }
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "RFSoC clock status check successful");
            } else {
                printf("Failed to get RFSoC clock status.\n");
                if (strlen(status.last_error) > 0) {
                    printf("Error: %s\n", status.last_error);
                }
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to get RFSoC clock status");
            }
        } else {
            printf("RFSoC client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted rfsoc_clock_status but RFSoC client not available");
        }
    } else if (strcmp(cmd, "rfsoc_check") == 0) {
        if (rfsoc_client_is_enabled()) {
            printf("Checking RFSoC daemon connectivity...\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Checking RFSoC daemon connectivity");
            
            int result = rfsoc_check_connectivity();
            if (result == 1) {
                printf("RFSoC daemon connectivity check passed!\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "RFSoC connectivity check successful");
            } else {
                printf("RFSoC daemon connectivity check failed!\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "RFSoC connectivity check failed");
            }
        } else {
            printf("RFSoC client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted rfsoc_check but RFSoC client not available");
        }
    } else if (strcmp(cmd, "start_timing_chain") == 0) {
        if (pbob_client_is_enabled()) {
            printf("Powering up timing chain (PBOB %d, Relay %d)...\n", config.ticc.pbob_id, config.ticc.relay_id);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to power up timing chain");
            
            int result = pbob_send_command(config.ticc.pbob_id, config.ticc.relay_id);
            if (result == 1) {
                printf("Timing chain power ON successful!\n");
                printf("The timing chain is now powered and ready for TICC measurements.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Timing chain powered ON successfully");
            } else {
                printf("Failed to power up timing chain. Check PBoB server connection.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power up timing chain");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted start_timing_chain but PBoB client not available");
        }
    } else if (strcmp(cmd, "stop_timing_chain") == 0) {
        if (pbob_client_is_enabled()) {
            printf("Powering down timing chain (PBOB %d, Relay %d)...\n", config.ticc.pbob_id, config.ticc.relay_id);
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to power down timing chain");
            
            int result = pbob_send_command(config.ticc.pbob_id, config.ticc.relay_id);
            if (result == 1) {
                printf("Timing chain power OFF successful!\n");
                printf("The timing chain has been powered down.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Timing chain powered OFF successfully");
            } else {
                printf("Failed to power down timing chain. Check PBoB server connection.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to power down timing chain");
            }
        } else {
            printf("PBoB client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted stop_timing_chain but PBoB client not available");
        }
    } else if (strcmp(cmd, "start_ticc") == 0) {
        if (ticc_client_is_enabled()) {
            printf("Starting TICC data collection...\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to start TICC data collection");
            
            int result = ticc_start_logging();
            if (result == 0) {
                printf("TICC data collection started successfully!\n");
                printf("Data will be saved to: %s\n", config.ticc.data_save_path);
                printf("Serial port: %s at %d baud\n", config.ticc.port, config.ticc.baud_rate);
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "TICC data collection started successfully");
            } else {
                printf("Failed to start TICC data collection.\n");
                ticc_status_t status;
                if (ticc_get_status(&status) == 0 && strlen(status.last_error) > 0) {
                    printf("Error: %s\n", status.last_error);
                }
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to start TICC data collection");
            }
        } else {
            printf("TICC client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted start_ticc but TICC client not available");
        }
    } else if (strcmp(cmd, "stop_ticc") == 0) {
        if (ticc_client_is_enabled()) {
            printf("Stopping TICC data collection...\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempting to stop TICC data collection");
            
            int result = ticc_stop_logging();
            if (result == 0) {
                printf("TICC data collection stopped successfully.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "TICC data collection stopped successfully");
            } else {
                printf("Failed to stop TICC data collection.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to stop TICC data collection");
            }
        } else {
            printf("TICC client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted stop_ticc but TICC client not available");
        }
    } else if (strcmp(cmd, "ticc_status") == 0) {
        if (ticc_client_is_enabled()) {
            ticc_status_t status;
            printf("Checking TICC status...\n");
            
            int result = ticc_get_status(&status);
            if (result == 0) {
                printf("TICC Status:\n");
                printf("  Data Collection: %s\n", status.is_logging ? "Running" : "Stopped");
                printf("  Device Configured: %s\n", status.is_configured ? "Yes" : "No");
                
                if (status.is_logging) {
                    printf("  Current Data File: %s\n", status.current_file);
                    printf("  Measurements Collected: %d\n", status.measurement_count);
                    printf("  Running Since: %s", ctime(&status.start_time));
                    printf("  Latest Measurement: %+.11f seconds\n", status.last_measurement);
                }
                
                if (strlen(status.last_error) > 0) {
                    printf("  Last Error: %s\n", status.last_error);
                }
                
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "TICC status check successful");
            } else {
                printf("Failed to get TICC status.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to get TICC status");
            }
        } else {
            printf("TICC client is not enabled or initialized.\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted ticc_status but TICC client not available");
        }
    } else if (strcmp(cmd, "stop") == 0) {
        // Check for "stop spec" or "stop spec 120khz"
        sscanf(arg, "%s %s", sub_arg, arg + strlen(sub_arg) + 1);
        
        if (strcmp(sub_arg, "spec") == 0) {
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
                        if (spec_120khz) {
                            printf("Stopped 120kHz spec script\n");
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped 120kHz spec script");
                        } else {
                            printf("Stopped spec script\n");
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped spec script");
                        }
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
                    if (spec_120khz) {
                        printf("Forcefully stopped 120kHz spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped 120kHz spec script");
                    } else {
                        printf("Forcefully stopped spec script\n");
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped spec script");
                    }
                    spec_120khz = 0; // Reset the spectrometer type flag
                    
                    // Clear spectrometer server type
                    spec_server_set_active_type(SPEC_TYPE_NONE);
                }
            } else {
                printf("Spec script is not running\n");
            }
        } else if (strcmp(sub_arg, "gps") == 0) {
            if (gps_is_logging()) {
                gps_stop_logging();
                printf("Stopped GPS logging\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS logging");
            } else {
                printf("GPS logging is not active\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to stop GPS logging, but it was not active");
            }
        } else {
            printf("Unknown stop command: %s\n", sub_arg);
        }
    } else if (strcmp(cmd, "gps") == 0 && scan > 1) {
        // Handle GPS sub-commands
        if (strcmp(arg, "status") == 0) {
            // Command for GPS status display
            if (!gps_is_logging()) {
                printf("GPS logging is not active. Start GPS logging first.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to show GPS status, but GPS logging was not active");
            } else if (gps_is_status_active()) {
                printf("GPS status display is already active.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to show GPS status, but it was already active");
            } else {
                printf("Starting GPS status display. Press 'q' to exit.\n");
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started GPS status display");
                
                // Save terminal settings
                struct termios old_term;
                tcgetattr(STDIN_FILENO, &old_term);
                struct termios new_term = old_term;
                new_term.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
                tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
                
                // Display GPS status (blocks until user exits)
                gps_display_status();
                
                // Restore terminal settings
                tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
                
                // Flush any remaining input to prevent interference with CLI
                fflush(stdin);
                
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS status display");
            }
        } else {
            printf("Unknown GPS command: %s\n", arg);
        }
    } else {
        printf("%s: Unknown command\n", cmd);
    }

    free(arg);
    free(cmd);
    free(sub_arg);
}

char* get_input() {
    char* input = (char*) malloc(1 * sizeof(char));
    char c;
    int i = 0;
    while ((c = getchar()) != '\n' && c != EOF) {
        input[i++] = c;
        input = (char*) realloc(input, i + 1);
    }
    input[i] = '\0';
    return input;
}

void cmdprompt(FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    int count = 1;
    char* input;
    while (exiting != 1) {
        printf("[BCP@Saggitarius]<%d>$ ", count);
        input = get_input();
        if (strlen(input) != 0) {
            write_to_log(cmdlog, "cli_Sag.c", "cmdprompt", input);
            exec_command(input, cmdlog, logpath, hostname, mode, data_save_interval, data_save_path);
        }
        free(input);
        count++;
    }
}
