#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    // for usleep
#include <time.h>
#include <stdbool.h>
#include <LabJackM.h>
#include <libconfig.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "file_io_Oph.h"
#include "pbob.h"

// function, time, C file, message

FILE* pbob_log_file; // Log file for debugging
int shutdown_pbob; // Flag to control shutdown sequence
extern struct conf_params config;
struct sockaddr_in cliaddr_pbob;
int pbob_enabled;
int pbob_ready = 0;
int pbob_server_running = 0;
int stop_pbob_server = 0;
static int pbob_sockfd;  // Make static to avoid naming conflict with bvexcam.c
pthread_t pbob_server_thread;

typedef enum {
    PBOB_ENABLED = 0,
    PBOB_DISABLED = 1
} PbobStatus;

/* Current names for subsystems each PBOB controls
static const char* metric_names[NUM_PBOB][NUM_RELAYS] = {
    // PBOB 0
    { "//", "motor_current", "bvex_cam_current", "//", "//", "//" },
    // PBOB 1
    { "//", "//", "//", "//", "//", NULL },
    // PBOB 2
    { "//", "//", "//", NULL, NULL, NULL }
};
*/
const int regs[NUM_RELAYS] = {2008, 2009, 2010, 2011, 2012, 2013}; // Register addresses for relays
const int pins[NUM_RELAYS] = {0, 1, 2, 3, 4, 5}; // Pin names for relays
RelayController controller[NUM_PBOB]; //I moved the extern flag to the header-FT

//This finds a particular relay and flips a toggle flag. I added this for userfreindliness - FT

int set_toggle(int pbob_id, int relay_id){

	for(int j = 0; j<NUM_PBOB; j++){
		if(controller[j].id == pbob_id){
			for(int i=0; i<controller[j].num_relays; i++){
				if(controller[j].relays[i].relay_id == relay_id){
					controller[j].relays[i].toggle = 1;
					return 1; // Successfully set toggle
				}
			}
		}
	}

    return 0; // Relay not found or PBOB not found
}

double get_relay_current(int pbob_id, int relay_id){
	for(int j = 0; j<NUM_PBOB; j++){
                if(controller[j].id == pbob_id){
                        for(int i=0; i<controller[j].num_relays; i++){
                                if(controller[j].relays[i].relay_id == relay_id){
                                        return controller[j].relays[i].current;
                                }
                        }
                }
        }

    return 0.0;
}

int get_state(int pbob_id, int relay_id){
        for(int j = 0; j<NUM_PBOB; j++){
                if(controller[j].id == pbob_id){
                        for(int i=0; i<controller[j].num_relays; i++){
                                if(controller[j].relays[i].relay_id == relay_id){
                                        return controller[j].relays[i].state;
                                }
                        }
                }
        }

    return 0;
}
/*
* This function handles errors from LabJack operations.
* It logs the error message to the log file and prints it to the console.
* The operation parameter specifies the operation that caused the error,
* and relay_id is used to identify which relay (if any) was involved.
*/

static void handle_ljm_error(int err, const char* operation, int relay_id, const char* function) {
    if (err != LJME_NOERROR) {
        char errorString[LJM_MAX_NAME_SIZE];
        char message[512];
        
        LJM_ErrorToString(err, errorString);
        if (relay_id >= 0) {
            snprintf(message, sizeof(message), "Error in %s for relay %d: %s (%d)", 
                    operation, relay_id, errorString, err);
        } else {
            snprintf(message, sizeof(message), "Error in %s: %s (%d)", 
                    operation, errorString, err);
        }
        
        write_to_log(pbob_log_file, "pbob.c", (char*)function, message);
    }
}

/*
* Initialize the LabJack and relays for a specific PBOB.
* Returns PBOB_ENABLED if successful, PBOB_DISABLED if the PBOB is disabled in the configuration.
*/

static int open_labjack(RelayController* ctrl) {
    int err;
    err = LJM_OpenS("T7", "ETHERNET", ctrl->ip, &ctrl->handle);
    if (err != LJME_NOERROR) {
        handle_ljm_error(err, "opening LabJack", -1, "open_labjack");
        return -1;
    }

    int deviceType, connectionType, serialNumber;
    int ipAddress, port, maxBytesPerMB;
    err = LJM_GetHandleInfo(
        ctrl->handle,
        &deviceType,
        &connectionType,
        &serialNumber,
        &ipAddress,
        &port,
        &maxBytesPerMB
    );

    if (err != LJME_NOERROR) {
        handle_ljm_error(err, "getting handle info", -1, "open_labjack");
        LJM_Close(ctrl->handle);
        return -1;
    }
    printf("Connected to LabJack T7 (IP: %s, Serial: %d)\n", ctrl->ip, serialNumber);
    return 0;
}

/*
 * Close the LabJack connection.
 * This function should be called when the program is done using the LabJack.
 */
static void close_labjack(RelayController* ctrl) {
    if (ctrl->handle <= 0) return;
    
    int err = LJM_Close(ctrl->handle);
    if (err != LJME_NOERROR) {
        handle_ljm_error(err, "closing LabJack", -1, "close_labjack");
    } else {
        printf("LabJack connection closed\n");
    }
    ctrl->handle = 0;
}

/*
 * Initialize all relays to OFF state and set them as outputs.
 * This function should be called after opening the LabJack connection.
 */
static int initialize_relays(RelayController* ctrl) {
    int err;
    
    for (int i = 0; i < ctrl->num_relays; i++) {
        int addr = ctrl->relays[i].registerAddress;
        
        // Set as output
        err = LJM_eWriteAddress(ctrl->handle, addr, DIR_TYPE, 1);
        if (err != LJME_NOERROR) {
            handle_ljm_error(err, "setting relay direction", i, "initialize_relays");
            return -1;
        }
        usleep(DELAY_US);
        
        // Set to OFF (HIGH)
        err = LJM_eWriteAddress(ctrl->handle, addr, STATE_TYPE, 1);
        if (err != LJME_NOERROR) {
            handle_ljm_error(err, "initializing relay state", i, "initialize_relays");
            return -1;
        }
        usleep(DELAY_US);
        
        ctrl->relays[i].state = false;
    }
    printf("All %d relays initialized and set to OFF\n", ctrl->num_relays);
    return 0;
}

/*
 * Toggle the state of a specific relay.
 * If the relay is ON, it will be turned OFF, and vice versa.
 * The relay ID should be between 0 and NUM_RELAYS - 1.
 */
static void toggle_relay(RelayController* ctrl, int relay_id) {
    char message[256];
    
    if (relay_id < 0 || relay_id >= ctrl->num_relays) {
        snprintf(message, sizeof(message), "Invalid relay ID: %d (must be 0-%d)", 
                relay_id, ctrl->num_relays-1);
        write_to_log(pbob_log_file, "pbob.c", "toggle_relay", message);
        return;
    }
    
    ctrl->relays[relay_id].state = !ctrl->relays[relay_id].state;
    bool newState = ctrl->relays[relay_id].state;
    int addr = ctrl->relays[relay_id].registerAddress;
    int value = newState ? 0 : 1;
    
    int err = LJM_eWriteAddress(ctrl->handle, addr, STATE_TYPE, value);
    if (err != LJME_NOERROR) {
        handle_ljm_error(err, "toggling relay", relay_id, "toggle_relay");
        // Revert state on error
        ctrl->relays[relay_id].state = !newState;
    } else {
        printf("Relay %d toggled to %s\n", relay_id, newState ? "ON" : "OFF");
    }
}

/*
 * Set all relays to a specific state (ON or OFF).
 * If state is true, all relays will be turned ON; if false, all will be turned OFF.
 */
void set_all_relays(RelayController* ctrl, bool state) {
    int err;
    int failed_count = 0;
    
    for (int i = 0; i < ctrl->num_relays; i++) {
        int addr = ctrl->relays[i].registerAddress;
        int value = state ? 0 : 1;
        
        err = LJM_eWriteAddress(ctrl->handle, addr, STATE_TYPE, value);
        if (err != LJME_NOERROR) {
            handle_ljm_error(err, "setting relay", i, "set_all_relays");
            failed_count++;
        } else {
            ctrl->relays[i].state = state;
        }
        usleep(DELAY_US);
    }
    
    if (failed_count == 0) {
        printf("All relays set to %s\n", state ? "ON" : "OFF");
    } else {
        char message[256];
        snprintf(message, sizeof(message), "Set relays to %s - %d failures", 
                state ? "ON" : "OFF", failed_count);
        write_to_log(pbob_log_file, "pbob.c", "set_all_relays", message);
    }
}

/* void display_status(RelayController* ctrl) {
    printf("\n╔════════════════════════════════════╗\n");
    printf("║         RELAY STATUS MONITOR      ║\n");
    printf("╠════════════════════════════════════╣\n");
    printf("║  ");
    for (int i = 0; i < 3; i++) {
        printf("Relay %d: [%s]  ", i, ctrl->relays[i].state ? "ON " : "OFF");
    }
    printf("║\n");
    printf("║  ");
    for (int i = 3; i < 6; i++) {
        printf("Relay %d: [%s]  ", i, ctrl->relays[i].state ? "ON " : "OFF");
    }
    printf("║\n");
    printf("╚════════════════════════════════════╝\n");
}

void display_menu() {
    printf("\n╔════════════════════════════════════╗\n");
    printf("║        RELAY CONTROL SYSTEM       ║\n");
    printf("╠════════════════════════════════════╣\n");
    printf("║  0-5: Toggle individual relay      ║\n");
    printf("║  a: Turn ALL relays ON             ║\n");
    printf("║  f: Turn ALL relays OFF            ║\n");
    printf("║  s: Show relay status              ║\n");
    printf("║  q: Quit program                   ║\n");
    printf("╚════════════════════════════════════╝\n");
} 

/*
* Main loop to run the relay controller.
* It prompts the user for input and toggles relays based on the input.
* The user can toggle individual relays, turn all relays ON or OFF, or quit the program.
*/

// We should use this function to perpetually check all relay structs and see if any need to be toggled - FT
void run_controller(RelayController* ctrl) {
    char choice[4];
    while (1) {
        printf("\nEnter command: ");
        if (fgets(choice, sizeof(choice), stdin) == NULL) break;
        // Remove newline
        choice[strcspn(choice, "\n")] = 0;
        if (strlen(choice) == 1 && choice[0] >= '0' && choice[0] <= '5') {
            toggle_relay(ctrl, choice[0] - '0');
            printf("Toggled relay %c\n", choice[0]);
        } else if (strcmp(choice, "a") == 0) {
            set_all_relays(ctrl, true);
            printf("All relays turned ON\n");
        } else if (strcmp(choice, "f") == 0) {
            set_all_relays(ctrl, false);
            printf("All relays turned OFF\n");
        } else if (strcmp(choice, "q") == 0) {
            printf("Turning off all relays before exit...\n");
            set_all_relays(ctrl, false);
            printf("Exiting program...\n");
            break;
        } else {
            printf("Invalid choice. Please try again.\n");
        }
    }
}

/*
 * Initialize parameters for a specific PBOB based on the configuration.
 * Returns PBOB_ENABLED if the PBOB is enabled, PBOB_DISABLED if it is not.
 */
static int initialize_parameters(int pbob_index, RelayController* ctrl) {
    int enabled = 0;
    const char* ip = NULL;
    int num_relays = 0;
    int id = 0;
    char message[256];
    char path[256];

    switch (pbob_index) {
        case 0:
            enabled = config.power.pbob0.enabled;
            ip = config.power.pbob0.ip;
            num_relays = config.power.pbob0.num_relays;
            id = config.power.pbob0.id;
	        snprintf(path,sizeof(path),"%s/pbob%d_current_%ld.txt",config.power.pbob0.workdir,id,time(NULL));
            break;
        case 1:
            enabled = config.power.pbob1.enabled;
            ip = config.power.pbob1.ip;
            num_relays = config.power.pbob1.num_relays;
            id = config.power.pbob1.id;
	        snprintf(path,sizeof(path),"%s/pbob%d_current_%ld.txt",config.power.pbob1.workdir,id,time(NULL));
            break;
        case 2:
            enabled = config.power.pbob2.enabled;
            ip = config.power.pbob2.ip;
            num_relays = config.power.pbob2.num_relays;
            id = config.power.pbob2.id;
            snprintf(path,sizeof(path),"%s/pbob%d_current_%ld.txt",config.power.pbob2.workdir,id,time(NULL));
            break;
        default:
            snprintf(message, sizeof(message), "Invalid PBOB index: %d", pbob_index);
            write_to_log(pbob_log_file, "pbob.c", "initialize_parameters", message);
            return PBOB_DISABLED;
    }
    ctrl->enabled = enabled;
    if (!enabled) {
        printf("PBOB %d is disabled in configuration, skipping initialization.\n", pbob_index);
        return PBOB_DISABLED;
    }

    // Set parameters
    ctrl->ip = ip;
    ctrl->num_relays = num_relays;
    ctrl->id = id;
    ctrl->log = fopen(path,"w");
    
    snprintf(message, sizeof(message), "PBOB %d initialized with IP: %s, Number of Relays: %d", 
            pbob_index, ctrl->ip, ctrl->num_relays);
    write_to_log(pbob_log_file, "pbob.c", "initialize_parameters", message);
    return PBOB_ENABLED;
}

/** Send an integer value over a UDP socket to the client.
* The integer is converted to a string and sent as a message.
* The client address is specified in the global variable cliaddr.
*/
static void sendInt_pbob(int sockfd, int sample) {
	char string_sample[6];

	snprintf(string_sample,6,"%d",sample);
    sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,(const struct sockaddr *) &cliaddr_pbob, sizeof(cliaddr_pbob));
    return;
}

static void sendDouble(int sockfd,double sample) {
	char string_sample[10];

	snprintf(string_sample,10,"%lf",sample);
	sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,(const struct sockaddr *) &cliaddr_pbob, sizeof(cliaddr_pbob));
	return;
}

/*
* Initialize the PBOB socket for receiving commands.
* It creates a UDP socket, binds it to the specified IP and port,
* and sets a receive timeout.
* Returns the socket file descriptor on success, or -1 on failure.
*/
static int init_socket_pbob() {
	int sockfd = socket(AF_INET,SOCK_DGRAM,0);
	struct sockaddr_in servaddr;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = config.power.timeout;

	if(sockfd < 0){
		write_to_log(pbob_log_file,"pbob.c","init_socket_pbob","Socket creation failed");
	}else{
		pbob_server_running = 1;

		memset(&servaddr,0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(config.power.port);
		
		if(strcmp(config.power.ip, "0.0.0.0") == 0){
			servaddr.sin_addr.s_addr = INADDR_ANY;
		}else{
			servaddr.sin_addr.s_addr = inet_addr(config.power.ip);
		}
		
		// Bind the socket to the server address
		if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
			write_to_log(pbob_log_file,"pbob.c","init_socket_pbob","Socket bind failed");
			pbob_server_running = 0;
			close(sockfd);
			return -1;
		}
		
		setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));

		write_to_log(pbob_log_file,"pbob.c","init_socket_pbob","Telemetry started successfully");
	}

	return sockfd;
}

/*
* Listen for incoming messages on the PBOB socket.
* It receives data into the provided buffer and updates the client address.
*/
static void sock_listen_pbob(int sockfd, char* buffer) {
	int n;
	socklen_t cliaddr_len = sizeof(cliaddr_pbob);
	
	n = recvfrom(sockfd, buffer, MAXLEN-1, MSG_WAITALL, (struct sockaddr *) &cliaddr_pbob, &cliaddr_len);

	if(n > 0){
		buffer[n] = '\0';
	}else{
		buffer[0] = '\0';
		if(n < 0){
			write_to_log(pbob_log_file,"pbob.c","sock_listen_pbob","Error receiving data");
		}
	}

	return;
}

/*
* Thread function to handle incoming requests for PBOB relay control.
* It listens for messages, parses the PBOB id and relay id, and toggles the relay accordingly.
* Messages should be sent with the format "PBOB_ID;RELAY_ID".
*/

static void *do_server_pbob() {

	pbob_sockfd = init_socket_pbob();
	char buffer[MAXLEN];

    if(pbob_server_running){
        while(!stop_pbob_server){
            sock_listen_pbob(pbob_sockfd, buffer);
            char* pbob_id_str = strtok(buffer, ";");
            char* relay_id_str = strtok(NULL, ";");

            if (pbob_id_str == NULL || relay_id_str == NULL) {
                write_to_log(pbob_log_file, "pbob.c", "do_server_pbob", "Malformed input: missing PBOB id or relay id");
            } else {
                int pbob_id = atoi(pbob_id_str);
                int relay_id = atoi(relay_id_str);

                if (pbob_id < 0 || pbob_id >= NUM_PBOB) {
                    write_to_log(pbob_log_file, "pbob.c", "do_server_pbob", "Invalid PBOB id received");
                } else if (relay_id < 0 || relay_id >= controller[pbob_id].num_relays) {
                    write_to_log(pbob_log_file, "pbob.c", "do_server_pbob", "Invalid relay id received");
                } else {
                    if (set_toggle(pbob_id, relay_id)) {
                        sendInt_pbob(pbob_sockfd, 1); // Send success response
                    }
                }
            }

        }
        write_to_log(pbob_log_file,"pbob.c","do_server_pbob","Shutting down server");
        pbob_server_running = 0;
        stop_pbob_server = 0;
        close(pbob_sockfd);
    }else{
        write_to_log(pbob_log_file,"pbob.c","do_server_pbob","Could not start server");
    }
}

/*
 * Main function to run the PBOB relay control system.
 * It initializes all PBOBs, opens LabJack connections, initializes relays,
 * and runs the controller for each enabled PBOB.
 */
int run_pbob() {
    char message[256];

    pthread_create(&pbob_server_thread, NULL, do_server_pbob, NULL);

    // Initialize all controllers
    for (int i = 0; i < NUM_PBOB; i++) {
        if (initialize_parameters(i, &controller[i]) == PBOB_ENABLED) {
            // Initialize relay data
            for (int j = 0; j < controller[i].num_relays; j++) {
                controller[i].relays[j].toggle = 0;
                controller[i].relays[j].registerAddress = regs[j];
                controller[i].relays[j].relay_id = j;
                controller[i].relays[j].state = false;
                controller[i].relays[j].curr_offset = 0.0;
                // controller[i].relays[j].pin = pins[j]; // Removed - pin field doesn't exist in Relay struct
            }
            controller[i].handle = 0;
            
            if (open_labjack(&controller[i]) == 0) {
                if (initialize_relays(&controller[i]) == 0) {
                    printf("PBOB %d initialized successfully.\n", i);
                    snprintf(message, sizeof(message), "PBOB %d initialized successfully", i);
                    write_to_log(pbob_log_file, "pbob.c", "run_pbob", message);
                    pbob_enabled = 1; // Set global flag to indicate PBOB is enabled
                } else {
                    snprintf(message, sizeof(message), "Failed to initialize relays for PBOB %d", i);
                    write_to_log(pbob_log_file, "pbob.c", "run_pbob", message);
                }
            } else {
                snprintf(message, sizeof(message), "Failed to open LabJack for PBOB %d", i);
                write_to_log(pbob_log_file, "pbob.c", "run_pbob", message);
            }
        }
    }
    
    return 0;
}

/*
* Check if all relays across all PBOBs are OFF.
* Returns 1 if all relays are OFF, 0 otherwise.
*/
int all_relays_off() {
    int count = 0;
    int which_relays_off[NUM_PBOB] = {0};

    for (int i = 0; i < NUM_PBOB; i++) {
        for (int j = 0; j < controller[i].num_relays; j++) {
            if (!controller[i].relays[j].state) { // Check if relay is OFF
                count++;
            }
        }
        if (count == controller[i].num_relays) {
                which_relays_off[i] = 1; // All relays are OFF for this PBOB
        }
        count = 0; // Reset count for next PBOB
    }

    for (int i = 0; i < NUM_PBOB; i++) {
        if (which_relays_off[i] == 1) {
            count++;
        }
    }

    if (count == NUM_PBOB) {
        return 1; // All relays are OFF
    } else {
        return 0; // Not all relays are OFF
    }
}

/*
 * Reads the current flowing through a relay by measuring the voltage across a shunt resistor.
 * The function reads the voltage from the specified analog input, calculates the current,
 * logs the result, and sends the current value as a metric.
 */
static void read_relay_current(RelayController* ctrl, Relay* rly) {
    double voltage, current;
    char channel_name[10];
    char message[256];
    
    snprintf(channel_name, sizeof(channel_name), "AIN%d", rly->relay_id);
    
    int err = LJM_eReadName(ctrl->handle, channel_name, &voltage);
    if (err != LJME_NOERROR) {
        snprintf(message, sizeof(message), "Failed to read voltage from %s", channel_name);
        handle_ljm_error(err, "reading analog input", rly->relay_id, "read_relay_current");
        return;
    } else {
        current = voltage/SHUNT_RESISTOR - rly->curr_offset;
        snprintf(message, sizeof(message), "Current read from %s: %.6f A", channel_name, current);
        write_to_log(pbob_log_file, "pbob.c", "read_relay_current", message);
    }

    rly->current = current;
    return;
}
void start_new_files(){
	char path[256];
	for(int i = 0; i < NUM_PBOB; i++) {
		if(controller[i].enabled){
			fclose(controller[i].log);
			if(controller[i].id == config.power.pbob0.id){
				snprintf(path,sizeof(path),"%s/pbob%d_current_%ld.txt",config.power.pbob0.workdir,controller[i].id,time(NULL));
			}else if(controller[i].id == config.power.pbob1.id){
				snprintf(path,sizeof(path),"%s/pbob%d_current_%ld.txt",config.power.pbob1.workdir,controller[i].id,time(NULL));
            }else if(controller[i].id == config.power.pbob2.id){
				snprintf(path,sizeof(path),"%s/pbob%d_current_%ld.txt",config.power.pbob2.workdir,controller[i].id,time(NULL));
            }
			controller[i].log = fopen(path,"w");
		}
    }
}

/*
* Thread function to run the PBOB relay control system.
* It continuously checks for relay toggles and performs shutdown if requested.*/

//This calibrates for the dead current
void calibrate_current(){
        for(int i=0;i<NUM_PBOB; i++) {
            if(controller[i].enabled){
                for (int j = 0;j<controller[i].num_relays;j++){
                        double summed = 0;
                        for (int k=0;k<CAL_ITER;k++){
                                read_relay_current(&controller[i], &controller[i].relays[j]);
                                summed += controller[i].relays[j].current;
                                usleep(200000);
                        }
                        controller[i].relays[j].curr_offset = summed/CAL_ITER;
                }
            }
        }
}

void* run_pbob_thread(void* arg) {
    char message[256];
    static int t_prev=0;
    struct timeval tv_now;
    float time;

    gettimeofday(&tv_now,NULL);
    t_prev = tv_now.tv_sec;
    run_pbob();
    calibrate_current();
    pbob_ready = 1;
    while(1) {
	    gettimeofday(&tv_now,NULL);
	    time = tv_now.tv_sec+tv_now.tv_usec/1e6;
	    if(tv_now.tv_sec-t_prev>600){
		    t_prev = tv_now.tv_sec;
		    start_new_files();
	    }
        for(int i = 0; i < NUM_PBOB; i++) {
            if(controller[i].enabled){
	            fprintf(controller[i].log,"%lf;",time);
                for(int j = 0; j < controller[i].num_relays; j++) {
                    read_relay_current(&controller[i], &controller[i].relays[j]);
		            if (j == controller[i].num_relays-1){
			            fprintf(controller[i].log,"%f\n",controller[i].relays[j].current);
		            } else{
			            fprintf(controller[i].log,"%f;",controller[i].relays[j].current);
                    }
                    
                    if (controller[i].relays[j].toggle) {
                        toggle_relay(&controller[i], j);
                        controller[i].relays[j].toggle = 0;
                    }
                }
            }
        }

        if(shutdown_pbob == 1 && all_relays_off() == 1){
            break; // Exit the loop if shutdown is requested and all relays are OFF
        }

	    usleep(200000);
    }

    for (int i = 0; i < NUM_PBOB; i++) {
	    if(controller[i].enabled){
        	close_labjack(&controller[i]); // Close LabJack connection
		    fclose(controller[i].log);//
	    }
    }

    stop_pbob_server = 1; // Signal the server thread to stop
    pthread_join(pbob_server_thread, NULL); // Wait for server thread to finish

    snprintf(message, sizeof(message), "PBOB shutdown complete");
    write_to_log(pbob_log_file, "pbob.c", "run_pbob_thread", message);

    return NULL;
}