#include <LabJackM.h>
#include <stdio.h>   
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h> 
#include <signal.h>  
#include <math.h> 
#include <libconfig.h>
#include <stdbool.h>
#include <time.h> 
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "pbob_client.h"
#include "heaters.h"
#include "file_io_Sag.h"

// Global variables
HeaterInfo heaters[NUM_HEATERS];
FILE* heaters_log_file;
pthread_t heaters_server_thread;
extern struct conf_params config;
struct sockaddr_in cliaddr_heaters;
static int heaters_sockfd = -1;
int heaters_running = 0;
int shutdown_heaters = 0;
int heaters_server_running = 0;
int stop_heaters_server = 0;

/**
 * @brief Initialize the heater array with register addresses and channel names
 * @param heaters Array of HeaterInfo structures
 */

void initialize_heaters(HeaterInfo heaters[]) {
    // All fields are explicitly set below, so zeroing is not necessary

    // Relay 0 ← AIN2
    heaters[0].eio_dir      = EIO0_DIR;
    heaters[0].eio_state    = EIO0_STATE;
    heaters[0].ain_channel  = "AIN2";
    heaters[0].state        = false;
    heaters[0].current_temp = 0.0;
    heaters[0].temp_valid   = false;
    heaters[0].enabled = true;
    heaters[0].current = 0.0;
    heaters[0].id = 5;
    heaters[0].current_offset = 0.0;
    heaters[0].toggle = false;

    // Relay 1 ← AIN1
    heaters[1].eio_dir      = EIO1_DIR;
    heaters[1].eio_state    = EIO1_STATE;
    heaters[1].ain_channel  = "AIN1";
    heaters[1].state        = false;
    heaters[1].current_temp = 0.0;
    heaters[1].temp_valid   = false;
    heaters[1].enabled = true;
    heaters[1].current = 0.0;
    heaters[1].id = 7;
    heaters[1].current_offset = 0.0;
    heaters[1].toggle = false;

    // Relay 2 ← AIN4
    heaters[2].eio_dir      = EIO2_DIR;
    heaters[2].eio_state    = EIO2_STATE;
    heaters[2].ain_channel  = "AIN4";
    heaters[2].state        = false;
    heaters[2].current_temp = 0.0;
    heaters[2].temp_valid   = false;
    heaters[2].enabled = true;
    heaters[2].current = 0.0;
    heaters[2].id = 9;
    heaters[2].current_offset = 0.0;
    heaters[2].toggle = false;

    // Relay 3 ← AIN3
    heaters[3].eio_dir      = EIO3_DIR;
    heaters[3].eio_state    = EIO3_STATE;
    heaters[3].ain_channel  = "AIN3";
    heaters[3].state        = false;
    heaters[3].current_temp = 0.0;
    heaters[3].temp_valid   = false;
    heaters[3].enabled = true;
    heaters[3].current = 0.0;
    heaters[3].id = 10;
    heaters[3].current_offset = 0.0;
    heaters[3].toggle = false;

    // Relay 4 ← AIN0
    heaters[4].eio_dir      = EIO4_DIR;
    heaters[4].eio_state    = EIO4_STATE;
    heaters[4].ain_channel  = "AIN0";
    heaters[4].state        = false;
    heaters[4].current_temp = 0.0;
    heaters[4].temp_valid   = false;
    heaters[4].enabled = true;
    heaters[4].current = 0.0;
    heaters[4].id = 12;
    heaters[4].current_offset = 0.0;
    heaters[4].toggle = false;
}

/**
 * @brief Open connection to LabJack T7 by IP address
 * @param ip IP address of the LabJack
 * @return LabJack handle on success
 * @note This function will exit the program if the connection fails.
 */
int open_labjack(const char* ip) {
    int handle = 0;
    int err;
    char err_string[LJM_MAX_NAME_SIZE];
    char ip_address_buffer[LJM_MAX_NAME_SIZE]; // Use a buffer for the IP string

    // Copy the input IP string to a buffer, ensuring null termination
    strncpy(ip_address_buffer, ip, LJM_MAX_NAME_SIZE - 1);
    ip_address_buffer[LJM_MAX_NAME_SIZE - 1] = '\0';

    // Use LJM_OpenS with the provided IP
    err = LJM_OpenS("T7", "ETHERNET", ip_address_buffer, &handle);

    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, err_string);
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "Error opening LabJack: %s", err_string);
        write_to_log(heaters_log_file, "heaters.c", "open_labjack", log_msg);
        fflush(heaters_log_file);
        exit(1); // Exit as in Python
    }

    // Get and print handle info
    int device_type;
    int connection_type;
    int serial_number;
    int ip_address_int;
    int port;
    int max_bytes_per_mb;

    err = LJM_GetHandleInfo(handle, &device_type, &connection_type, &serial_number, &ip_address_int, &port, &max_bytes_per_mb);

    if (err == LJME_NOERROR) {
        printf("Connected to LabJack T7 (IP: %s, Serial: %d)\n", ip, serial_number);
    } else {
        LJM_ErrorToString(err, err_string);
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "Could not get LabJack handle info: %s", err_string);
        write_to_log(heaters_log_file, "heaters.c", "open_labjack", log_msg);
        fflush(heaters_log_file);
    }

    return handle;
}

/**
 * @brief Close LabJack connection
 * @param handle LabJack handle
 */
void close_labjack(int handle) {
    int err;
    char err_string[LJM_MAX_NAME_SIZE];

    err = LJM_Close(handle);

    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, err_string);
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "Error closing LabJack: %s", err_string);
        write_to_log(heaters_log_file, "heaters.c", "close_labjack", log_msg);
        fflush(heaters_log_file);
    } else {
        printf("LabJack connection closed\n");
    }
}

/**
 * @brief Read temperature from LM35 on specified channel
 * @param handle LabJack handle
 * @param channel Analog input channel name (e.g., "AIN0")
 * @param temperature_c Pointer to store the temperature in Celsius
 * @return 0 on success, non-zero on failure (LJM error code)
 */
int read_temperature(int handle, const char* channel, double* temperature_c) {
    int err;
    char err_string[LJM_MAX_NAME_SIZE];
    double voltage = 0.0;

    // Read voltage from specified channel
    err = LJM_eReadName(handle, channel, &voltage);

    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, err_string);
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "Error reading temperature from %s: %s", channel, err_string);
        write_to_log(heaters_log_file, "heaters.c", "read_temperature", log_msg);
        fflush(heaters_log_file);
        return err; // Return error code
    }

    // LM35 outputs 10mV per degree Celsius
    *temperature_c = voltage * 100.0; // Convert to Celsius
    return 0; // Success
}

/**
 * @brief Set relay state (0=ON, 1=OFF)
 * @param handle LabJack handle
 * @param relay_num Relay number (0-4)
 * @param state Desired state (bool: true=ON, false=OFF)
 */
void set_relay_state(int handle, int relay_num, bool state) {
    int err;
    char err_string[LJM_MAX_NAME_SIZE];
    int address;
    double value;

    // Determine the state register address based on relay number
    switch (relay_num) {
        case 0:
            address = EIO0_STATE;
            break;
        case 1:
            address = EIO1_STATE;
            break;
        case 2:
            address = EIO2_STATE;
            break;
        case 3:
            address = EIO3_STATE;
            break;
        case 4:
            address = EIO4_STATE;
            break;
        default:
            char log_msg[128];
            snprintf(log_msg, sizeof(log_msg), "Invalid relay number %d", relay_num);
            write_to_log(heaters_log_file, "heaters.c", "set_relay_state", log_msg);
            fflush(heaters_log_file);
            return;
    }

    // Active LOW: 0 if true, 1 if false
    value = state ? 0.0 : 1.0;

    err = LJM_eWriteAddress(handle, address, STATE_TYPE, value);

    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, err_string);
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "Error setting relay %d state: %s", relay_num, err_string);
        write_to_log(heaters_log_file, "heaters.c", "set_relay_state", log_msg);
        fflush(heaters_log_file);
    }
}

/**
 * @brief Read the current from the specified heater's analog input channel
 * @param handle LabJack handle
 * @param heater Pointer to HeaterInfo structure for the specific heater
 */
static void read_relay_current(int handle, HeaterInfo* heater) {
    double voltage, current;
    char message[256];
    char err_string[LJM_MAX_NAME_SIZE];

    // Use the correct analog input channel from the heater struct
    int err = LJM_eReadName(handle, heater->ain_channel, &voltage);
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, err_string);
        snprintf(message, sizeof(message), "Failed to read voltage from %s: %s", heater->ain_channel, err_string);
        write_to_log(heaters_log_file, "heaters.c", "read_relay_current", message);
        fprintf(stderr, "%s\n", message);
        heater->current = 0.0; // Set to zero or a safe default
        return;
    } else {
        current = voltage / SHUNT_RESISTOR - heater->current_offset;
        snprintf(message, sizeof(message), "Current read from %s: %.6f A", heater->ain_channel, current);
        write_to_log(heaters_log_file, "heaters.c", "read_relay_current", message);
        heater->current = current;
    }
    return;
}

/**
 * @brief Print the status of all heaters
 * @param heaters Array of HeaterInfo structures
 */
static void start_new_files() {
    char path[256];
    char message[256];

    if (heaters_log_file) {
        fclose(heaters_log_file);
    }

    snprintf(path, sizeof(path), "%s/heater_log_%ld.txt", config.heaters.workdir, time(NULL));
    heaters_log_file = fopen(path, "w");
            
    if(heaters_log_file == NULL) {
        snprintf(message, sizeof(message), "Failed to open new log file: %s", path);
        fprintf(stderr, "%s\n", message);
    } else {
        snprintf(message, sizeof(message), "Started new log file: %s", path);
        write_to_log(heaters_log_file, "heaters.c", "start_new_files", message);
    }
}

/**
 * @brief Calibrate the current readings for all heaters
 * @param handle LabJack handle
 * @param heaters Array of HeaterInfo structures
 */
static void calibrate_current(int handle, HeaterInfo heaters[]) {
    char message[256];
    
    write_to_log(heaters_log_file, "heaters.c", "calibrate_current", "Starting current calibration");
    
    for(int i = 0; i < NUM_HEATERS; i++) {
        if(heaters[i].enabled) {
            double summed = 0;
            int valid_readings = 0;
            
            // Ensure relay is OFF for calibration
            set_relay_state(handle, i, false);
            usleep(500000); // Wait 500ms for relay to settle
            
            for (int k = 0; k < CAL_ITER; k++) {
                double voltage;
                
                int err = LJM_eReadName(handle, heaters[i].ain_channel, &voltage);
                if (err == LJME_NOERROR) {
                    double current = voltage / SHUNT_RESISTOR;
                    summed += current;
                    valid_readings++;
                } else {
                    char err_string[LJM_MAX_NAME_SIZE];
                    LJM_ErrorToString(err, err_string);
                    snprintf(message, sizeof(message), "Calibration read error for heater %d: %s", i, err_string);
                    write_to_log(heaters_log_file, "heaters.c", "calibrate_current", message);
                    fprintf(stderr, "%s\n", message);
                }
                usleep(200000); // 200ms between readings
            }
            
            if (valid_readings > 0) {
                double offset = summed / valid_readings;
                snprintf(message, sizeof(message), "Heater %d calibration complete. Offset: %.6f A (%d readings)", 
                        i, offset, valid_readings);
                write_to_log(heaters_log_file, "heaters.c", "calibrate_current", message);
                
                heaters[i].current_offset = offset;
            } else {
                snprintf(message, sizeof(message), "Heater %d calibration failed - no valid readings", i);
                write_to_log(heaters_log_file, "heaters.c", "calibrate_current", message);
            }
        }
    }
    
    write_to_log(heaters_log_file, "heaters.c", "calibrate_current", "Current calibration complete");
}

/**
 * @brief Send an integer value over a UDP socket to the client.
 * The integer is converted to a string and sent as a message.
 * The client address is specified in the global variable cliaddr_heaters.
 */
static void sendInt_heaters(int sockfd, int sample) {
    char string_sample[12];

    snprintf(string_sample,12,"%d",sample);
        sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,(const struct sockaddr *) &cliaddr_heaters, sizeof(cliaddr_heaters));
        return;
}

/**
 * @brief Send a double value over a UDP socket to the client.
 * The double is converted to a string and sent as a message.
 * The client address is specified in the global variable cliaddr_heaters.
 */
static void sendDouble_heaters(int sockfd,double sample) {
    char string_sample[32];

    snprintf(string_sample, sizeof(string_sample), "%lf", sample);
    sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,(const struct sockaddr *) &cliaddr_heaters, sizeof(cliaddr_heaters));
    return;
}

/**
 * @brief Initialize the heater server socket and start listening for commands.
 * It creates a UDP socket, binds it to the specified IP and port,
 * and sets a receive timeout.
 * Returns the socket file descriptor on success, or -1 on failure.
 */
static int init_socket_heaters() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = config.heaters.timeout;

    if(sockfd < 0){
        write_to_log(heaters_log_file,"heaters.c","init_socket_heaters","Socket creation failed");
        return -1;
    }

    memset(&servaddr,0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(config.heaters.port);
    
    if(strcmp(config.heaters.server_ip, "0.0.0.0") == 0){
        servaddr.sin_addr.s_addr = INADDR_ANY;
    }else{
        servaddr.sin_addr.s_addr = inet_addr(config.heaters.server_ip);
    }
    
    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        write_to_log(heaters_log_file,"heaters.c","init_socket_heaters","Socket bind failed");
        close(sockfd);
        return -1;
    }
    
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    write_to_log(heaters_log_file, "heaters.c", "init_socket_heaters", "UDP socket initialized successfully");
    return sockfd;
}

/**
 * @brief Listen for incoming commands on the heater socket.
 * It receives data from the socket and stores it in the provided buffer.
 * If no data is received, it sets the buffer to an empty string.
 */
static void sock_listen_heaters(int sockfd, char* buffer) {
	int n;
	socklen_t cliaddr_len = sizeof(cliaddr_heaters);
	
	n = recvfrom(sockfd, buffer, MAXLEN-1, MSG_WAITALL, (struct sockaddr *) &cliaddr_heaters, &cliaddr_len);

	if(n > 0){
		buffer[n] = '\0';
	}else{
		buffer[0] = '\0';
		if(n < 0){
			write_to_log(heaters_log_file,"heaters.c","sock_listen_heater","Error receiving data");
		}
	}

	return;
}

/**
 * @brief Set the toggle state for a specific relay
 * @param relay_id ID of the relay to toggle
 * @return 1 if successful, 0 if relay not found or PBOB not found
 */
int set_toggle(int relay_id){
	for(int j = 0; j<NUM_HEATERS; j++){
		if(j == relay_id){
			heaters[j].toggle = 1;
            return 1;
		}
	}
    return 0; // Relay not found or PBOB not found
}

/**
 * @brief Thread function to run the heaters control logic.
 * It initializes the LabJack, sets up the heaters, and enters the main control loop.
 */
static void *do_server_heaters() {

	heaters_sockfd = init_socket_heaters();
	char buffer[MAXLEN];

    if(heaters_server_running){
        while(!stop_heaters_server){
            sock_listen_heaters(heaters_sockfd, buffer);
            
            int relay_id = -1;

            if (strcmp(buffer, "toggle_lockpin") == 0) {
                relay_id = 0;    
            } else if(strcmp(buffer, "toggle_starcamera") == 0) {
                relay_id = 1; 
            } else if(strcmp(buffer, "toggle_PV") == 0) {
                relay_id = 2;
            } else if(strcmp(buffer, "toggle_motor") == 0) {
                relay_id = 3;
            } else if(strcmp(buffer, "toggle_ethernet") == 0) {
                relay_id = 4;
            } else{
                write_to_log(heaters_log_file, "heaters.c", "do_server_heaters", "Malformed input: missing relay id");
            }
            
            if (relay_id < 0 || relay_id >= NUM_HEATERS-1) {
                write_to_log(heaters_log_file, "heaters.c", "do_server_heaters", "Invalid relay id received");
                sendInt_heaters(heaters_sockfd, 0); // send failure response
            } else {
                if (heaters[relay_id].state == true) {
                    heaters[relay_id].enabled = 0;
                } else if (heaters[relay_id].state == false) {
                    heaters[relay_id].enabled = 1;
                } if (set_toggle(relay_id)) {
                    sendInt_heaters(heaters_sockfd, 1); // Send success response
                }
            }
        }

        write_to_log(heaters_log_file,"heaters.c","do_server_heaters","Shutting down server");
        heaters_server_running = 0;
        stop_heaters_server = 0;
        close(heaters_sockfd);
        
    } else{
        write_to_log(heaters_log_file,"heaters.c","do_server_heaters","Could not start server");
    }

    return;
}

/**
 * @brief Thread function to run the heaters control logic.
 * It initializes the LabJack, sets up the heaters, and enters the main control loop.
 * This function is called from the main thread to start the heaters control thread.
 */
void* run_heaters_thread(void* arg) {
    char message[256];
    static int t_prev = 0;
    struct timeval tv_now;
    char path[256];

    // Initialize log file
    snprintf(path, sizeof(path), "%s/heaters_log_%ld.txt", config.heaters.workdir, time(NULL));
    heaters_log_file = fopen(path, "w");
    if (!heaters_log_file) {
        fprintf(stderr, "Failed to open heaters log file\n");
        heaters_running = 0;
        return NULL;
    }

    gettimeofday(&tv_now, NULL);
    t_prev = tv_now.tv_sec;
    
    int result = pthread_create(&heaters_server_thread, NULL, do_server_heaters, NULL);
    
    if (result == 0) {
        heaters_server_running = 1;
    } else {
        goto cleanup;
    }

    const char* labjack_ip = config.heaters.heater_ip;
    int handle = open_labjack(labjack_ip);
    if (handle <= 0) {
        write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", "Failed to open LabJack connection");
        goto cleanup;
    }

    // Initialize heaters
    initialize_heaters(heaters);

    // Configure EIO pins
    char err_string[LJM_MAX_NAME_SIZE];
    int err;
    for (int i = 0; i < NUM_HEATERS; i++) {
        err = LJM_eWriteAddress(handle, heaters[i].eio_dir, DIR_TYPE, 1.0);
        if (err != LJME_NOERROR) {
            LJM_ErrorToString(err, err_string);
            snprintf(message, sizeof(message), "Error setting EIO%d direction: %s", i, err_string);
            write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
            goto cleanup;
        }
    }

    usleep(50000);

    // Ensure all relays start OFF
    for (int i = 0; i < NUM_HEATERS; i++) {
        err = LJM_eWriteAddress(handle, heaters[i].eio_state, STATE_TYPE, 1.0);
        if (err != LJME_NOERROR) {
            LJM_ErrorToString(err, err_string);
            snprintf(message, sizeof(message), "Error setting initial EIO%d state: %s", i, err_string);
            write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
            goto cleanup;
        }
    }

    calibrate_current(handle, heaters);
    heaters_running = 1;

    // Main control loop - now with integrated UDP server functionality
    while (!shutdown_heaters) {
        gettimeofday(&tv_now, NULL);
        float time = tv_now.tv_sec + tv_now.tv_usec / 1e6;
        
        // Log file rotation
        if (tv_now.tv_sec - t_prev > 600) {
            t_prev = tv_now.tv_sec;
            start_new_files();
        }

        float sum = 0.0;

        // Read all temperatures and current for each heater
        for (int i = 0; i < NUM_HEATERS; i++) {
            double temp;
            int temp_read_error = read_temperature(handle, heaters[i].ain_channel, &temp);
            read_relay_current(handle, &heaters[i]);
            
            if (temp_read_error == LJME_NOERROR) {
                heaters[i].current_temp = temp;
                heaters[i].temp_valid = true;
            } else {
                heaters[i].temp_valid = false;
            }
            
            // Sum up current from active heaters
            if (heaters[i].state) {
                sum += 1.0;
            }
        }

        // Control logic for each heater
        for (int i = 0; i < NUM_HEATERS; i++) {
            bool should_turn_on = false;
            bool should_turn_off = false;
            
            if (i == 4) {  // heater 4 is manually controlled only
                if (heaters[i].toggle) {
                    // Toggle current state
                    should_turn_on = !heaters[i].state;
                    should_turn_off = heaters[i].state;
                    heaters[i].toggle = false;  // Reset toggle flag
                }
            } else {  // Heaters 0-3: temperature controlled with manual override
                if (heaters[i].toggle) {
                    // Toggle current state
                    should_turn_on = !heaters[i].state;
                    should_turn_off = heaters[i].state;
                    heaters[i].toggle = false;  // Reset toggle flag
                } else if (heaters[i].temp_valid) {
                    // Normal temperature-based control
                    should_turn_on = (heaters[i].current_temp < TEMP_LOW && heaters[i].enabled);
                    should_turn_off = (heaters[i].current_temp > TEMP_HIGH);
                }
            }
            
            // Apply state changes
            if (should_turn_on && !heaters[i].state && (sum + 1.0) <= CURRENT_CAP) {
                heaters[i].state = true;
                set_relay_state(handle, i, true);
                snprintf(message, sizeof(message), "Heater %d turned ON at %.1f°C", i, heaters[i].current_temp);
                write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
            } else if (should_turn_off && heaters[i].state) {
                heaters[i].state = false;
                set_relay_state(handle, i, false);
                snprintf(message, sizeof(message), "Heater %d turned OFF at %.1f°C", i, heaters[i].current_temp);
                write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
            }
        }
        usleep(500000); // 0.5 seconds
    }

cleanup:
    // Turn off all heaters
    if (handle > 0) {
        for (int i = 0; i < NUM_HEATERS; i++) {
            LJM_eWriteAddress(handle, heaters[i].eio_state, STATE_TYPE, 1.0);
        }
        close_labjack(handle);
        snprintf(message, sizeof(message), "LabJack connection closed");
        write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
    }

    if (heaters_server_running) {
        stop_heaters_server = 1;
        pthread_join(heaters_server_thread, NULL);
    }

    snprintf(message, sizeof(message), "Heaters shutdown complete");
    write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);

    if (heaters_log_file) {
        fclose(heaters_log_file);
        heaters_log_file = NULL;
    }

    shutdown_heaters = 0;
    heaters_running = 0;
    return NULL;
}