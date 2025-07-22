#include <LabJackM.h>
#include <stdio.h>   
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h> 
#include <signal.h>  
#include <math.h>
#include <errno.h> 
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
pthread_t main_heaters_thread;  // Main heater control thread
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
    heaters[0].manual_override_time = 0;

    // Relay 1 ← AIN1
    heaters[1].eio_dir      = EIO1_DIR;
    heaters[1].eio_state    = EIO1_STATE;
    heaters[1].ain_channel  = "AIN1";
    heaters[1].state        = false;
    heaters[1].current_temp = 0.0;
    heaters[1].temp_valid   = false;
    heaters[1].enabled = true;
    heaters[1].current = 0.0;
    heaters[1].id = 8;
    heaters[1].current_offset = 0.0;
    heaters[1].toggle = false;
    heaters[1].manual_override_time = 0;

    // Relay 2 ← AIN4
    heaters[2].eio_dir      = EIO2_DIR;
    heaters[2].eio_state    = EIO2_STATE;
    heaters[2].ain_channel  = "AIN4";
    heaters[2].state        = false;
    heaters[2].current_temp = 0.0;
    heaters[2].temp_valid   = false;
    heaters[2].enabled = true;
    heaters[2].current = 0.0;
    heaters[2].id = 7;
    heaters[2].current_offset = 0.0;
    heaters[2].toggle = false;
    heaters[2].manual_override_time = 0;

    // Relay 3 ← AIN3
    heaters[3].eio_dir      = EIO3_DIR;
    heaters[3].eio_state    = EIO3_STATE;
    heaters[3].ain_channel  = "AIN3";
    heaters[3].state        = false;
    heaters[3].current_temp = 0.0;
    heaters[3].temp_valid   = false;
    heaters[3].enabled = true;
    heaters[3].current = 0.0;
    heaters[3].id = 6;
    heaters[3].current_offset = 0.0;
    heaters[3].toggle = false;
    heaters[3].manual_override_time = 0;

    // Relay 4 ← AIN0
    heaters[4].eio_dir      = EIO4_DIR;
    heaters[4].eio_state    = EIO4_STATE;
    heaters[4].ain_channel  = "AIN0";
    heaters[4].state        = false;
    heaters[4].current_temp = 0.0;
    heaters[4].temp_valid   = false;
    heaters[4].enabled = true;
    heaters[4].current = 0.0;
    heaters[4].id = 9;
    heaters[4].current_offset = 0.0;
    heaters[4].toggle = false;
    heaters[4].manual_override_time = 0;
}

/**
 * @brief Open connection to LabJack T7 by IP address
 * @param ip IP address of the LabJack
 * @return LabJack handle on success, -1 on failure
 * @note This function no longer exits the program on failure
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
        return -1; // Return error instead of exiting
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
        return handle;
    } else {
        LJM_ErrorToString(err, err_string);
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "Could not get LabJack handle info: %s", err_string);
        write_to_log(heaters_log_file, "heaters.c", "open_labjack", log_msg);
        fflush(heaters_log_file);
        
        // Close the handle since we can't get proper info
        LJM_Close(handle);
        return -1;
    }
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
		// Don't log timeout errors - they're normal when no data is received
		if(n < 0 && errno != EAGAIN && errno != EWOULDBLOCK){
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
            
            // Skip processing if no data received (timeout or empty)
            if (strlen(buffer) == 0) {
                continue;
            }
            
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
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Unknown command received: '%s'", buffer);
                write_to_log(heaters_log_file, "heaters.c", "do_server_heaters", log_msg);
                sendInt_heaters(heaters_sockfd, 0); // send failure response
                continue;
            }
            
            if (relay_id < 0 || relay_id >= NUM_HEATERS) {
                write_to_log(heaters_log_file, "heaters.c", "do_server_heaters", "Invalid relay id received");
                sendInt_heaters(heaters_sockfd, 0); // send failure response
            } else {
                // Set toggle flag for the specified heater
                if (set_toggle(relay_id)) {
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg), "Toggle command received for heater %d (%s)", relay_id, buffer);
                    write_to_log(heaters_log_file, "heaters.c", "do_server_heaters", log_msg);
                    sendInt_heaters(heaters_sockfd, 1); // Send success response
                } else {
                    sendInt_heaters(heaters_sockfd, 0); // Send failure response
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

    return NULL;
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

    // Create directory if it doesn't exist
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", config.heaters.workdir);
    int mkdir_result = system(mkdir_cmd);
    if (mkdir_result != 0) {
        fprintf(stderr, "Warning: Failed to create heaters directory: %s\n", config.heaters.workdir);
    }

    // Initialize log file
    snprintf(path, sizeof(path), "%s/heaters_log_%ld.txt", config.heaters.workdir, time(NULL));
    heaters_log_file = fopen(path, "w");
    if (!heaters_log_file) {
        fprintf(stderr, "Failed to open heaters log file: %s\n", path);
        heaters_running = 0;
        return NULL;
    }

    printf("Heaters log file opened: %s\n", path);

    gettimeofday(&tv_now, NULL);
    t_prev = tv_now.tv_sec;
    
    int result = pthread_create(&heaters_server_thread, NULL, do_server_heaters, NULL);
    
    if (result == 0) {
        heaters_server_running = 1;
    } else {
        goto cleanup;
    }

    const char* labjack_ip = config.heaters.heater_ip;
    snprintf(message, sizeof(message), "Attempting to connect to LabJack T7 at %s", labjack_ip);
    write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
    
    int handle = open_labjack(labjack_ip);
    bool labjack_connected = (handle > 0);
    
    if (!labjack_connected) {
        snprintf(message, sizeof(message), "WARNING: Failed to connect to LabJack T7 at %s. Running UDP server only (no heater control).", labjack_ip);
        write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
        printf("WARNING: Cannot connect to LabJack T7 at %s - UDP server will run but heater control disabled!\n", labjack_ip);
        handle = -1; // Ensure it's clearly invalid
    }

    // Initialize heaters
    initialize_heaters(heaters);

    if (labjack_connected) {
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
        write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", "LabJack initialization complete - heater control active");
    } else {
        write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", "LabJack not connected - heater control disabled, UDP server only");
    }
    
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

        float total_current = 0.0;

        // Read all temperatures and current for each heater (only if LabJack connected)
        if (labjack_connected) {
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
                
                // Sum up actual current from all heaters
                if (heaters[i].state && heaters[i].current > 0) {
                    total_current += heaters[i].current;
                }
            }
        } else {
            // No LabJack - set dummy values and process UDP commands only
            for (int i = 0; i < NUM_HEATERS; i++) {
                heaters[i].current_temp = 20.0; // Dummy temperature
                heaters[i].temp_valid = false;
                heaters[i].current = 0.0;
            }
        }

        // ======== IMPROVED TEMPERATURE-BASED PRIORITY CONTROL LOGIC ========
        
        // Step 1: Handle manual toggle commands and emergency shutoffs for heaters 0-3
        for (int i = 0; i < 4; i++) {  // Only heaters 0-3 for temperature control
            if (heaters[i].toggle) {
                // Manual toggle overrides temperature control
                bool new_state = !heaters[i].state;
                heaters[i].state = new_state;
                heaters[i].toggle = false;
                heaters[i].manual_override_time = tv_now.tv_sec; // Set override timestamp
                
                if (labjack_connected) {
                    set_relay_state(handle, i, new_state);
                    snprintf(message, sizeof(message), "Heater %d MANUALLY toggled %s at %.1f°C (auto-control disabled for 30s)", 
                            i, new_state ? "ON" : "OFF", heaters[i].current_temp);
                } else {
                    snprintf(message, sizeof(message), "Heater %d MANUALLY toggled %s (SIMULATED) at %.1f°C (auto-control disabled for 30s)", 
                            i, new_state ? "ON" : "OFF", heaters[i].current_temp);
                }
                write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
                
            } else if (heaters[i].temp_valid && heaters[i].current_temp > TEMP_HIGH && heaters[i].state) {
                // Emergency shutdown: turn off immediately if too hot
                heaters[i].state = false;
                if (labjack_connected) {
                    set_relay_state(handle, i, false);
                    snprintf(message, sizeof(message), "Heater %d turned OFF (too hot) at %.1f°C", 
                            i, heaters[i].current_temp);
                } else {
                    snprintf(message, sizeof(message), "Heater %d turned OFF (too hot, SIMULATED) at %.1f°C", 
                            i, heaters[i].current_temp);
                }
                write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
            }
        }
        
        // Step 2: Handle heater 4 (manual control only)
        if (heaters[4].toggle) {
            bool new_state = !heaters[4].state;
            heaters[4].state = new_state;
            heaters[4].toggle = false;
            
            if (labjack_connected) {
                set_relay_state(handle, 4, new_state);
                snprintf(message, sizeof(message), "Heater 4 (manual) toggled %s", new_state ? "ON" : "OFF");
            } else {
                snprintf(message, sizeof(message), "Heater 4 (manual) toggled %s (SIMULATED)", new_state ? "ON" : "OFF");
            }
            write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
        }
        
        // Step 3: Create priority list for heaters 0-3 that need heating
        typedef struct {
            int heater_id;
            double temp_deficit;  // How far below TEMP_LOW (higher = more urgent)
            double current_temp;
            bool currently_on;
        } HeaterPriority;
        
        HeaterPriority priority_list[4];
        int need_heating_count = 0;
        
        for (int i = 0; i < 4; i++) {
            // Skip heaters under manual override (30 second protection)
            bool under_manual_override = (heaters[i].manual_override_time > 0) && 
                                        (tv_now.tv_sec - heaters[i].manual_override_time < 30);
            
            if (heaters[i].enabled && heaters[i].temp_valid && heaters[i].current_temp < TEMP_LOW && !under_manual_override) {
                priority_list[need_heating_count].heater_id = i;
                priority_list[need_heating_count].temp_deficit = TEMP_LOW - heaters[i].current_temp;
                priority_list[need_heating_count].current_temp = heaters[i].current_temp;
                priority_list[need_heating_count].currently_on = heaters[i].state;
                need_heating_count++;
            }
        }
        
        // Step 4: Sort by temperature deficit (most urgent first)
        for (int i = 0; i < need_heating_count - 1; i++) {
            for (int j = i + 1; j < need_heating_count; j++) {
                if (priority_list[j].temp_deficit > priority_list[i].temp_deficit) {
                    HeaterPriority temp = priority_list[i];
                    priority_list[i] = priority_list[j];
                    priority_list[j] = temp;
                }
            }
        }
        
        // Step 5: Calculate available current budget
        float current_budget = (float)config.heaters.current_cap;
        float estimated_heater_current = 0.6; // Estimated current per heater
        
        // Account for heater 4 if it's on (manual heater)
        if (heaters[4].state) {
            current_budget -= estimated_heater_current;
        }
        
        // Step 6: Smart heater allocation based on priority and current budget
        bool heater_should_be_on[4] = {false, false, false, false};
        float budget_used = 0.0;
        int heaters_allocated = 0;
        
        // First pass: allocate to highest priority heaters within budget
        for (int p = 0; p < need_heating_count; p++) {
            int heater_id = priority_list[p].heater_id;
            
            if (budget_used + estimated_heater_current <= current_budget) {
                heater_should_be_on[heater_id] = true;
                budget_used += estimated_heater_current;
                heaters_allocated++;
                
                static time_t last_priority_log = 0;
                if (tv_now.tv_sec - last_priority_log >= 30) {  // Log every 30 seconds
                    snprintf(message, sizeof(message), "Priority %d: Heater %d at %.1f°C (deficit: %.1f°C) - ALLOCATED", 
                            p+1, heater_id, priority_list[p].current_temp, priority_list[p].temp_deficit);
                    write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
                    if (p == need_heating_count - 1) last_priority_log = tv_now.tv_sec;
                }
            } else {
                static time_t last_denied_log = 0;
                if (tv_now.tv_sec - last_denied_log >= 60) {  // Log every 60 seconds
                    snprintf(message, sizeof(message), "Priority %d: Heater %d at %.1f°C (deficit: %.1f°C) - DENIED (budget exceeded)", 
                            p+1, heater_id, priority_list[p].current_temp, priority_list[p].temp_deficit);
                    write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
                    last_denied_log = tv_now.tv_sec;
                }
            }
        }
        
        // Step 7: Apply the calculated heater states
        for (int i = 0; i < 4; i++) {
            bool should_be_on = heater_should_be_on[i];
            bool currently_on = heaters[i].state;
            
            if (should_be_on && !currently_on) {
                // Turn ON heater
                heaters[i].state = true;
                if (labjack_connected) {
                    set_relay_state(handle, i, true);
                    snprintf(message, sizeof(message), "Heater %d turned ON (priority) at %.1f°C (deficit: %.1f°C)", 
                            i, heaters[i].current_temp, TEMP_LOW - heaters[i].current_temp);
                } else {
                    snprintf(message, sizeof(message), "Heater %d turned ON (priority, SIMULATED) at %.1f°C", 
                            i, heaters[i].current_temp);
                }
                write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
                
            } else if (!should_be_on && currently_on && heaters[i].temp_valid && heaters[i].current_temp < TEMP_LOW) {
                // Turn OFF heater (due to current budget, not temperature)
                heaters[i].state = false;
                if (labjack_connected) {
                    set_relay_state(handle, i, false);
                    snprintf(message, sizeof(message), "Heater %d turned OFF (budget reallocation) at %.1f°C", 
                            i, heaters[i].current_temp);
                } else {
                    snprintf(message, sizeof(message), "Heater %d turned OFF (budget reallocation, SIMULATED) at %.1f°C", 
                            i, heaters[i].current_temp);
                }
                write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
            }
        }
        
        // Log periodic status (every 10 seconds)
        static time_t last_status_log = 0;
        if (tv_now.tv_sec - last_status_log >= 10) {
            last_status_log = tv_now.tv_sec;
            
            // Check for manual overrides
            char override_info[256] = "";
            for (int i = 0; i < 4; i++) {
                bool under_manual_override = (heaters[i].manual_override_time > 0) && 
                                            (tv_now.tv_sec - heaters[i].manual_override_time < 30);
                if (under_manual_override) {
                    char temp_str[64];
                    int remaining = 30 - (tv_now.tv_sec - heaters[i].manual_override_time);
                    snprintf(temp_str, sizeof(temp_str), " H%d:MANUAL(%ds)", i, remaining);
                    strncat(override_info, temp_str, sizeof(override_info) - strlen(override_info) - 1);
                }
            }
            
            snprintf(message, sizeof(message), "Status: %s | Total current %.2fA/%.1fA | Active heaters: %s %s %s %s %s%s", 
                    labjack_connected ? "LabJack OK" : "LabJack OFFLINE",
                    total_current, (float)config.heaters.current_cap,
                    heaters[0].state ? "H0" : "--",
                    heaters[1].state ? "H1" : "--", 
                    heaters[2].state ? "H2" : "--",
                    heaters[3].state ? "H3" : "--",
                    heaters[4].state ? "H4" : "--",
                    strlen(override_info) > 0 ? override_info : "");
            write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
        }
        
        usleep(500000); // 0.5 seconds
    }

cleanup:
    // Turn off all heaters
    if (labjack_connected && handle > 0) {
        for (int i = 0; i < NUM_HEATERS; i++) {
            LJM_eWriteAddress(handle, heaters[i].eio_state, STATE_TYPE, 1.0);
        }
        close_labjack(handle);
        snprintf(message, sizeof(message), "LabJack connection closed and all heaters turned off");
        write_to_log(heaters_log_file, "heaters.c", "run_heaters_thread", message);
    } else {
        snprintf(message, sizeof(message), "No LabJack connection to close");
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