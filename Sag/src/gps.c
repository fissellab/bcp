#include "gps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/select.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <fcntl.h>
#include <termios.h>

#define GPSD_HOST "127.0.0.1"
#define GPSD_PORT 2947
#define GPSD_BUFFER_SIZE 4096
#define MAX_UDP_CLIENTS 10

// UDP Server configuration - will be set from config file
static int gps_udp_port = 8080;
static char gps_udp_client_ips[MAX_UDP_CLIENTS][16];
static int gps_udp_client_count = 0;
static int gps_udp_buffer_size = 1024;

static int gpsd_socket = -1;
static FILE *logfile = NULL;
static bool logging = false;
static pthread_t gps_thread;
static pthread_t status_thread;
static gps_config_t current_config;
static time_t last_file_rotation;
static char session_folder[512];
static gps_data_t current_gps_data;
static bool status_active = false;
static int nmea_fd = -1;
static pthread_t nmea_thread;
static bool nmea_thread_running = false;
static int flush_counter = 0;
static int nmea_flush_counter = 0;

// UDP Server variables
static int udp_server_socket = -1;
static pthread_t udp_server_thread;
static bool udp_server_running = false;
static FILE *udp_log_file = NULL;

// Forward declarations
static int open_nmea_device(void);
static void *nmea_reading_thread(void *arg);

// UDP Server functions
static void *udp_server_thread_func(void *arg);
static void format_gps_response(char *buffer, size_t buffer_size);
static void log_udp_message(const char *message);
static bool is_authorized_client(const char *client_ip);

static void create_timestamp(char *buffer, size_t size) {
    time_t now;
    struct tm *tm_info;
    time(&now);
    tm_info = localtime(&now);
    strftime(buffer, size, "%Y%m%d_%H%M%S", tm_info);
}

static void create_session_folder(void) {
    char timestamp[20];
    create_timestamp(timestamp, sizeof(timestamp));
    snprintf(session_folder, sizeof(session_folder), "%s/%s_GPS_data", current_config.data_path, timestamp);
    mkdir(session_folder, 0777);
}

static void rotate_logfile(void) {
    if (logfile != NULL) {
        fclose(logfile);
    }

    char filename[576];
    char timestamp[20];
    create_timestamp(timestamp, sizeof(timestamp));
    snprintf(filename, sizeof(filename), "%s/gps_log_%s.bin", session_folder, timestamp);

    logfile = fopen(filename, "wb");  // Open in binary write mode
    if (logfile == NULL) {
        fprintf(stderr, "Error opening new log file: %s\n", strerror(errno));
        logging = false;
        return;
    }

    last_file_rotation = time(NULL);
}

// Connect to gpsd
static int connect_to_gpsd(void) {
    struct sockaddr_in server_addr;
    
    gpsd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (gpsd_socket < 0) {
        fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(GPSD_PORT);
    server_addr.sin_addr.s_addr = inet_addr(GPSD_HOST);
    
    if (connect(gpsd_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error connecting to gpsd: %s\n", strerror(errno));
        close(gpsd_socket);
        gpsd_socket = -1;
        return -1;
    }
    
    // Send WATCH command to start receiving GPS data
    const char *watch_cmd = "?WATCH={\"enable\":true,\"json\":true}\n";
    if (send(gpsd_socket, watch_cmd, strlen(watch_cmd), 0) < 0) {
        fprintf(stderr, "Error sending WATCH command: %s\n", strerror(errno));
        close(gpsd_socket);
        gpsd_socket = -1;
        return -1;
    }
    
    // Set socket to non-blocking mode to prevent recv() from hanging during shutdown
    int flags = fcntl(gpsd_socket, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(gpsd_socket, F_SETFL, flags | O_NONBLOCK);
    }
    
    return 0;
}

// Parse TPV (Time-Position-Velocity) message from gpsd
static void parse_tpv_message(json_object *json_obj) {
    (void)json_obj; // Suppress unused parameter warning
    
    // We're now using NMEA GPRMC and HEHDT messages for all GPS data
    // This function is kept for compatibility but doesn't update any data
    // All GPS data will come from the NMEA thread parsing GPRMC and HEHDT messages
}

// Process JSON message from gpsd
static void process_gpsd_message(const char *message) {
    json_object *json_obj = json_tokener_parse(message);
    if (!json_obj) {
        return;
    }
    
    json_object *class_obj;
    if (json_object_object_get_ex(json_obj, "class", &class_obj)) {
        const char *msg_class = json_object_get_string(class_obj);
        
        if (strcmp(msg_class, "TPV") == 0) {
            parse_tpv_message(json_obj);
        }
        // We can add handling for other message types (SKY, DEVICE, etc.) if needed
    }
    
    json_object_put(json_obj);
}

// Parse HEHDT (True Heading) NMEA sentence
static void parse_hehdt_sentence(const char *sentence) {
    // Expected format: $HEHDT,heading,T*checksum
    // Example: $HEHDT,26.566,T*1E
    
    char *sentence_copy = strdup(sentence);
    char *token = strtok(sentence_copy, ",");
    
    if (token && strcmp(token, "$HEHDT") == 0) {
        token = strtok(NULL, ","); // Get heading value
        if (token) {
            pthread_mutex_lock(&current_gps_data.mutex);
            // Add +90 degree offset and wrap around if needed
            current_gps_data.heading = fmod(atof(token) + 90.0, 360.0);
            current_gps_data.valid_heading = true;
            current_gps_data.last_update = time(NULL);
            pthread_mutex_unlock(&current_gps_data.mutex);
        }
    }
    
    free(sentence_copy);
}

// Parse GPRMC (Recommended Minimum Course) NMEA sentence
static void parse_gprmc_sentence(const char *sentence) {
    // Expected format: $GPRMC,time,status,lat,N/S,lon,E/W,speed,track,date,variation,E/W*checksum
    // Example: $GPRMC,173822.20,A,4413.46481485,N,07629.84993844,W,0.04,348.24,280525,12.0,W,D,C*6F
    
    char *sentence_copy = strdup(sentence);
    char *tokens[15]; // GPRMC has up to 12 fields plus extras
    int token_count = 0;
    
    char *token = strtok(sentence_copy, ",");
    while (token != NULL && token_count < 15) {
        tokens[token_count++] = token;
        token = strtok(NULL, ",");
    }
    
    if (token_count >= 10 && strcmp(tokens[0], "$GPRMC") == 0) {
        // Check if data is valid (status field)
        if (tokens[2][0] == 'A') { // 'A' = valid, 'V' = invalid
            pthread_mutex_lock(&current_gps_data.mutex);
            
            // Parse time (HHMMSS.SSS)
            if (strlen(tokens[1]) >= 6) {
                char time_str[3];
                strncpy(time_str, tokens[1], 2); time_str[2] = '\0';
                current_gps_data.hour = atoi(time_str);
                strncpy(time_str, tokens[1] + 2, 2); time_str[2] = '\0';
                current_gps_data.minute = atoi(time_str);
                strncpy(time_str, tokens[1] + 4, 2); time_str[2] = '\0';
                current_gps_data.second = atoi(time_str);
            }
            
            // Parse latitude (DDMM.MMMM)
            if (strlen(tokens[3]) > 0) {
                double lat_raw = atof(tokens[3]);
                int lat_degrees = (int)(lat_raw / 100);
                double lat_minutes = lat_raw - (lat_degrees * 100);
                current_gps_data.latitude = lat_degrees + (lat_minutes / 60.0);
                if (tokens[4][0] == 'S') {
                    current_gps_data.latitude = -current_gps_data.latitude;
                }
            }
            
            // Parse longitude (DDDMM.MMMM)
            if (strlen(tokens[5]) > 0) {
                double lon_raw = atof(tokens[5]);
                int lon_degrees = (int)(lon_raw / 100);
                double lon_minutes = lon_raw - (lon_degrees * 100);
                current_gps_data.longitude = lon_degrees + (lon_minutes / 60.0);
                if (tokens[6][0] == 'W') {
                    current_gps_data.longitude = -current_gps_data.longitude;
                }
            }
            
            // Parse speed (field 7, in knots) and convert to m/s
            if (token_count >= 8 && strlen(tokens[7]) > 0) {
                double speed_knots = atof(tokens[7]);
                // Convert knots to m/s: 1 knot = 0.514444 m/s
                current_gps_data.speed_ms = speed_knots * 0.514444;
                current_gps_data.valid_speed = true;
            } else {
                current_gps_data.valid_speed = false;
            }
            
            // Parse date (DDMMYY)
            if (strlen(tokens[9]) == 6) {
                char date_str[3];
                strncpy(date_str, tokens[9], 2); date_str[2] = '\0';
                current_gps_data.day = atoi(date_str);
                strncpy(date_str, tokens[9] + 2, 2); date_str[2] = '\0';
                current_gps_data.month = atoi(date_str);
                strncpy(date_str, tokens[9] + 4, 2); date_str[2] = '\0';
                current_gps_data.year = 2000 + atoi(date_str);
            }
            
            current_gps_data.valid_position = true;
            current_gps_data.last_update = time(NULL);
            
            pthread_mutex_unlock(&current_gps_data.mutex);
        } else {
            // Invalid fix
            pthread_mutex_lock(&current_gps_data.mutex);
            current_gps_data.valid_position = false;
            current_gps_data.valid_speed = false;
            pthread_mutex_unlock(&current_gps_data.mutex);
        }
    }
    
    free(sentence_copy);
}

// Parse GPGGA (Global Positioning System Fix Data) NMEA sentence
static void parse_gpgga_sentence(const char *sentence) {
    // Expected format: $GPGGA,time,lat,N/S,lon,E/W,quality,numSV,HDOP,alt,M,height,M,dgpsTime,dgpsID*checksum
    // Example: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
    // We only care about the altitude field (field 9)
    
    char *sentence_copy = strdup(sentence);
    char *tokens[15]; // GPGGA has about 14 fields
    int token_count = 0;
    
    char *token = strtok(sentence_copy, ",");
    while (token != NULL && token_count < 15) {
        tokens[token_count++] = token;
        token = strtok(NULL, ",");
    }
    
    if (token_count >= 10 && strcmp(tokens[0], "$GPGGA") == 0) {
        // Check if we have a fix (quality > 0)
        if (token_count >= 6 && tokens[6][0] != '0' && strlen(tokens[6]) > 0) {
            pthread_mutex_lock(&current_gps_data.mutex);
            
            // Parse number of satellites (field 7, index 7)
            if (token_count >= 8 && strlen(tokens[7]) > 0) {
                current_gps_data.num_satellites = atoi(tokens[7]);
                current_gps_data.valid_satellites = true;
            } else {
                current_gps_data.valid_satellites = false;
            }
            
            // Parse altitude (field 9, index 9)
            if (strlen(tokens[9]) > 0) {
                current_gps_data.altitude = atof(tokens[9]);
            }
            
            current_gps_data.last_update = time(NULL);
            pthread_mutex_unlock(&current_gps_data.mutex);
        } else {
            // No fix - invalidate satellite data
            pthread_mutex_lock(&current_gps_data.mutex);
            current_gps_data.valid_satellites = false;
            pthread_mutex_unlock(&current_gps_data.mutex);
        }
    }
    
    free(sentence_copy);
}

// Process NMEA sentence
static void process_nmea_sentence(const char *sentence) {
    if (strncmp(sentence, "$HEHDT", 6) == 0) {
        parse_hehdt_sentence(sentence);
    } else if (strncmp(sentence, "$GPRMC", 6) == 0) {
        parse_gprmc_sentence(sentence);
    } else if (strncmp(sentence, "$GPGGA", 6) == 0) {
        parse_gpgga_sentence(sentence);
    }
    // We can add other NMEA sentence parsers here if needed
}

static void *gps_logging_thread(void *arg) {
    (void)arg;
    char buffer[GPSD_BUFFER_SIZE];
    char line_buffer[GPSD_BUFFER_SIZE * 2] = {0};
    int line_buffer_pos = 0;

    while (logging && gpsd_socket >= 0) {
        ssize_t n = recv(gpsd_socket, buffer, sizeof(buffer) - 1, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);  // 1ms - reduced for 50Hz operation
                continue;
            }
            fprintf(stderr, "Error reading from gpsd socket: %s\n", strerror(errno));
            break;
        } else if (n == 0) {
            fprintf(stderr, "gpsd connection closed\n");
            break;
        }

        buffer[n] = '\0';
        
        // Write raw data to log file
        if (logfile != NULL) {
            fwrite(buffer, 1, n, logfile);
            flush_counter++;
            // Only flush every 50 writes (1 second at 50Hz) to improve performance
            if (flush_counter >= 50) {
                fflush(logfile);
                flush_counter = 0;
            }
        }
        
        // Process data line by line (JSON messages and NMEA sentences are line-delimited)
        for (ssize_t i = 0; i < n; i++) {
            char c = buffer[i];
            
            if (c == '\n') {
                // Process complete line
                line_buffer[line_buffer_pos] = '\0';
                if (line_buffer_pos > 0) {
                    if (line_buffer[0] == '{') {
                        // JSON message from gpsd
                        process_gpsd_message(line_buffer);
                    } else if (line_buffer[0] == '$') {
                        // NMEA sentence
                        process_nmea_sentence(line_buffer);
                    }
                }
                line_buffer_pos = 0;
            } else if (line_buffer_pos < (int)sizeof(line_buffer) - 1) {
                line_buffer[line_buffer_pos++] = c;
            }
        }

        // Check if it's time to rotate the file
        if (logfile != NULL) {
            time_t now = time(NULL);
            if (now - last_file_rotation >= current_config.file_rotation_interval) {
                rotate_logfile();
            }
        }
    }

    return NULL;
}

// Function to clear the current line and move cursor to beginning
static void clear_line() {
    printf("\r\033[K");
    fflush(stdout);
}

static void *status_display_thread(void *arg) {
    (void)arg;
    char status_buffer[256];
    
    while (status_active) {
        gps_data_t data;
        if (gps_get_data(&data)) {
            if (data.valid_position) {
                if (data.valid_heading) {
                    snprintf(status_buffer, sizeof(status_buffer),
                             "GPS Status: %04d-%02d-%02d %02d:%02d:%02d | Lat: %.6f | Lon: %.6f | Alt: %.1f | Heading: %.2f° | Press 'q' to exit",
                             data.year, data.month, data.day,
                             data.hour, data.minute, data.second,
                             data.latitude, data.longitude, data.altitude,
                             data.heading);
                } else {
                    snprintf(status_buffer, sizeof(status_buffer),
                             "GPS Status: %04d-%02d-%02d %02d:%02d:%02d | Lat: %.6f | Lon: %.6f | Alt: %.1f | Heading: N/A | Press 'q' to exit",
                             data.year, data.month, data.day,
                             data.hour, data.minute, data.second,
                             data.latitude, data.longitude, data.altitude);
                }
            } else if (data.valid_heading) {
                snprintf(status_buffer, sizeof(status_buffer),
                         "GPS Status: No valid position fix | Heading: %.2f° | Press 'q' to exit",
                         data.heading);
            } else {
                snprintf(status_buffer, sizeof(status_buffer),
                         "GPS Status: No valid position fix | Heading: N/A | Press 'q' to exit");
            }
            
            clear_line();
            printf("%s", status_buffer);
            fflush(stdout);
        } else {
            clear_line();
            printf("GPS Status: No data available | Press 'q' to exit");
            fflush(stdout);
        }
        
        usleep(1000000);  // Update every second
    }
    
    return NULL;
}

int gps_init(const gps_config_t *config) {
    memcpy(&current_config, config, sizeof(gps_config_t));

    // Store UDP configuration
    gps_udp_port = config->udp_server_port;
    for (int i = 0; i < config->udp_client_count; i++) {
        strncpy(gps_udp_client_ips[i], config->udp_client_ips[i], sizeof(gps_udp_client_ips[i]) - 1);
        gps_udp_client_ips[i][sizeof(gps_udp_client_ips[i]) - 1] = '\0';
    }
    gps_udp_client_count = config->udp_client_count;
    gps_udp_buffer_size = config->udp_buffer_size;

    if (connect_to_gpsd() < 0) {
        return -1;
    }

    // Initialize the GPS data structure
    memset(&current_gps_data, 0, sizeof(gps_data_t));
    pthread_mutex_init(&current_gps_data.mutex, NULL);
    current_gps_data.valid_position = false;
    current_gps_data.valid_heading = false;
    current_gps_data.valid_speed = false;
    current_gps_data.valid_satellites = false;

    create_session_folder();
    printf("GPS initialized via gpsd connection\n");
    return 0;
}

bool gps_start_logging(void) {
    if (logging) {
        printf("GPS logging is already active.\n");
        return false;
    }

    rotate_logfile();
    if (logfile == NULL) {
        return false;
    }

    // Open direct NMEA connection for heading data
    if (open_nmea_device() < 0) {
        fprintf(stderr, "Warning: Could not open direct NMEA connection for heading data\n");
        // Continue anyway, we'll still get position data from gpsd
    }

    logging = true;
    flush_counter = 0;
    nmea_flush_counter = 0;
    
    // Start GPS logging thread (for position data via gpsd)
    if (pthread_create(&gps_thread, NULL, gps_logging_thread, NULL) != 0) {
        fprintf(stderr, "Error creating GPS logging thread: %s\n", strerror(errno));
        logging = false;
        fclose(logfile);
        logfile = NULL;
        if (nmea_fd >= 0) {
            close(nmea_fd);
            nmea_fd = -1;
        }
        return false;
    }
    
    // Start NMEA reading thread (for heading data)
    if (nmea_fd >= 0) {
        if (pthread_create(&nmea_thread, NULL, nmea_reading_thread, NULL) != 0) {
            fprintf(stderr, "Warning: Error creating NMEA reading thread: %s\n", strerror(errno));
            // Continue anyway, we'll still get position data
        } else {
            nmea_thread_running = true;
        }
    }

    printf("GPS logging started.\n");
    return true;
}

void gps_stop_logging(void) {
    if (!logging) {
        printf("GPS logging is not active.\n");
        return;
    }

    logging = false;
    
    // Close file descriptors first to unblock any pending reads
    if (gpsd_socket >= 0) {
        close(gpsd_socket);
        gpsd_socket = -1;
    }
    
    if (nmea_fd >= 0) {
        close(nmea_fd);
        nmea_fd = -1;
    }
    
    // Wait for both threads to finish
    pthread_join(gps_thread, NULL);
    
    if (nmea_thread_running) {
        pthread_join(nmea_thread, NULL);
        nmea_thread_running = false;
    }

    if (logfile != NULL) {
        fclose(logfile);
        logfile = NULL;
    }

    printf("GPS logging stopped.\n");
}

bool gps_is_logging(void) {
    return logging;
}

bool gps_get_data(gps_data_t *data) {
    if (!data) {
        return false;
    }
    
    pthread_mutex_lock(&current_gps_data.mutex);
    
    // Copy the current GPS data to the provided structure
    memcpy(data, &current_gps_data, sizeof(gps_data_t));
    
    // Don't copy the mutex
    pthread_mutex_init(&data->mutex, NULL);
    
    // Determine if the data is fresh (within the last 5 seconds)
    time_t now = time(NULL);
    bool is_fresh = (now - current_gps_data.last_update) <= 5;
    

    
    pthread_mutex_unlock(&current_gps_data.mutex);
    
    return is_fresh && (current_gps_data.valid_position || current_gps_data.valid_heading);
}

void gps_display_status(void) {
    if (status_active) {
        printf("GPS status display is already active.\n");
        return;
    }
    
    status_active = true;
    
    if (pthread_create(&status_thread, NULL, status_display_thread, NULL) != 0) {
        fprintf(stderr, "Error creating GPS status thread: %s\n", strerror(errno));
        status_active = false;
        return;
    }
    
    // Wait for user to press 'q' to exit
    while (status_active) {
        fd_set readfds;
        struct timeval tv;
        
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 0.1 seconds
        
        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            char c = getchar();
            if (c == 'q' || c == 'Q') {
                gps_stop_status_display();
                break;
            }
        }
    }
}

bool gps_is_status_active(void) {
    return status_active;
}

void gps_stop_status_display(void) {
    if (!status_active) {
        return;
    }
    
    status_active = false;
    pthread_join(status_thread, NULL);
    
    printf("\nGPS status display stopped.\n");
}

// Open direct connection to GPS device for NMEA data
static int open_nmea_device(void) {
    nmea_fd = open(current_config.port, O_RDONLY | O_NOCTTY);
    if (nmea_fd < 0) {
        fprintf(stderr, "Error opening GPS device %s: %s\n", current_config.port, strerror(errno));
        return -1;
    }
    
    // Configure serial port
    struct termios tty;
    if (tcgetattr(nmea_fd, &tty) != 0) {
        fprintf(stderr, "Error getting serial port attributes: %s\n", strerror(errno));
        close(nmea_fd);
        nmea_fd = -1;
        return -1;
    }
    
    // Set baud rate
    speed_t baud_speed;
    switch (current_config.baud_rate) {
        case 115200: baud_speed = B115200; break;
        case 57600:  baud_speed = B57600; break;
        case 38400:  baud_speed = B38400; break;
        case 19200:  baud_speed = B19200; break;
        case 9600:   baud_speed = B9600; break;
        default:     baud_speed = B115200; break;  // Default to 115200 for GPS
    }
    cfsetospeed(&tty, baud_speed);
    cfsetispeed(&tty, baud_speed);
    
    // Configure for raw input
    tty.c_cflag &= ~PARENB;        // No parity
    tty.c_cflag &= ~CSTOPB;        // One stop bit
    tty.c_cflag &= ~CSIZE;         // Clear size bits
    tty.c_cflag |= CS8;            // 8 data bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable reading, ignore control lines
    
    tty.c_lflag &= ~ICANON;        // Raw input
    tty.c_lflag &= ~ECHO;          // No echo
    tty.c_lflag &= ~ECHOE;         // No echo erase
    tty.c_lflag &= ~ECHONL;        // No echo newline
    tty.c_lflag &= ~ISIG;          // No signal processing
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // No software flow control
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
    
    tty.c_oflag &= ~OPOST;         // No output processing
    tty.c_oflag &= ~ONLCR;         // No newline conversion
    
    // Set timeout
    tty.c_cc[VTIME] = 10;          // 1 second timeout
    tty.c_cc[VMIN] = 0;            // Non-blocking read
    
    if (tcsetattr(nmea_fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "Error setting serial port attributes: %s\n", strerror(errno));
        close(nmea_fd);
        nmea_fd = -1;
        return -1;
    }
    
    return 0;
}

// Thread to read raw NMEA data for heading information
static void *nmea_reading_thread(void *arg) {
    (void)arg;
    char buffer[4096];      // Increased from 1024 for 115200 baud rate
    char line_buffer[8192] = {0};  // Increased from 2048 for 115200 baud rate
    int line_buffer_pos = 0;
    
    while (logging && nmea_fd >= 0) {
        ssize_t n = read(nmea_fd, buffer, sizeof(buffer) - 1);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);  // 1ms - reduced for 50Hz operation
                continue;
            }
            fprintf(stderr, "Error reading from NMEA device: %s\n", strerror(errno));
            break;
        } else if (n == 0) {
            usleep(1000);  // 1ms - reduced for 50Hz operation
            continue;
        }
        
        buffer[n] = '\0';
        
        // Write raw NMEA data to log file (same as gpsd data)
        if (logfile != NULL) {
            fwrite(buffer, 1, n, logfile);
            nmea_flush_counter++;
            // Only flush every 50 writes (1 second at 50Hz) to improve performance
            if (nmea_flush_counter >= 50) {
                fflush(logfile);
                nmea_flush_counter = 0;
            }
        }
        
        // Process data line by line
        for (ssize_t i = 0; i < n; i++) {
            char c = buffer[i];
            
            if (c == '\n' || c == '\r') {
                // Process complete line
                line_buffer[line_buffer_pos] = '\0';
                if (line_buffer_pos > 0 && line_buffer[0] == '$') {
                    process_nmea_sentence(line_buffer);
                }
                line_buffer_pos = 0;
            } else if (line_buffer_pos < (int)sizeof(line_buffer) - 1) {
                line_buffer[line_buffer_pos++] = c;
            }
        }
    }
    
    return NULL;
}

// UDP Server functions
static void *udp_server_thread_func(void *arg) {
    (void)arg;
    struct sockaddr_in server_addr, client_addr;
    char *buffer = malloc(gps_udp_buffer_size);
    char *response = malloc(gps_udp_buffer_size);
    socklen_t client_len = sizeof(client_addr);
    
    if (!buffer || !response) {
        log_udp_message("Error allocating UDP buffers");
        if (buffer) free(buffer);
        if (response) free(response);
        return NULL;
    }
    
    // Create UDP socket
    udp_server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_server_socket < 0) {
        log_udp_message("Error creating UDP socket");
        free(buffer);
        free(response);
        return NULL;
    }
    
    // Set socket timeout to prevent hanging during shutdown
    struct timeval timeout;
    timeout.tv_sec = 1;  // 1 second timeout
    timeout.tv_usec = 0;
    if (setsockopt(udp_server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_udp_message("Warning: Could not set socket timeout");
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(gps_udp_port);
    
    // Bind socket
    if (bind(udp_server_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_udp_message("Error binding UDP socket");
        close(udp_server_socket);
        udp_server_socket = -1;
        free(buffer);
        free(response);
        return NULL;
    }
    
    char start_msg[256];
    snprintf(start_msg, sizeof(start_msg), "GPS UDP server started and listening on port %d", gps_udp_port);
    log_udp_message(start_msg);
    
    while (udp_server_running) {
        // Receive request from client
        ssize_t n = recvfrom(udp_server_socket, buffer, gps_udp_buffer_size - 1, 0,
                            (struct sockaddr *)&client_addr, &client_len);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout occurred - check if we should still be running
                continue;
            }
            if (udp_server_running) {
                log_udp_message("Error receiving UDP data");
            }
            break;
        }
        
        buffer[n] = '\0';
        
        // Check if request is from expected client
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        if (is_authorized_client(client_ip)) {
            // Removed verbose logging - GPS client connected messages
            // char log_msg[512];  // Increased buffer size
            // snprintf(log_msg, sizeof(log_msg), "GPS client connected from %s", client_ip);
            // log_udp_message(log_msg);
            
            // Process request
            if (strncmp(buffer, "GET_GPS", 7) == 0) {
                format_gps_response(response, gps_udp_buffer_size);
                
                // Send response
                sendto(udp_server_socket, response, strlen(response), 0,
                      (const struct sockaddr *)&client_addr, client_len);
                
                // Removed verbose logging - Sent GPS data messages
                // snprintf(log_msg, sizeof(log_msg), "Sent GPS data to client: %.100s", response);
                // log_udp_message(log_msg);
            } else if (strncmp(buffer, "gps_lat", 7) == 0 || 
                      strncmp(buffer, "gps_lon", 7) == 0 || 
                      strncmp(buffer, "gps_alt", 7) == 0 || 
                      strncmp(buffer, "gps_head", 8) == 0) {
                // Handle individual parameter requests (client compatibility)
                format_gps_response(response, gps_udp_buffer_size);
                
                // Send response
                sendto(udp_server_socket, response, strlen(response), 0,
                      (const struct sockaddr *)&client_addr, client_len);
                
                // Removed verbose logging - Parameter request messages
                // snprintf(log_msg, sizeof(log_msg), "Sent GPS data for parameter request '%.20s': %.100s", buffer, response);
                // log_udp_message(log_msg);
            } else {
                // Unknown request - Keep this logging as it indicates potential issues
                char log_msg[512];
                const char *error_msg = "gps_lat:INVALID_REQUEST,gps_lon:INVALID_REQUEST,gps_alt:INVALID_REQUEST,gps_head:INVALID_REQUEST";
                sendto(udp_server_socket, error_msg, strlen(error_msg), 0,
                      (const struct sockaddr *)&client_addr, client_len);
                
                snprintf(log_msg, sizeof(log_msg), "Invalid request from client: %.100s", buffer);
                log_udp_message(log_msg);
            }
        } else {
            // Keep logging for unauthorized access attempts - this is important for security
            char log_msg[512];  // Increased buffer size
            snprintf(log_msg, sizeof(log_msg), "Rejected request from unauthorized client: %s", client_ip);
            log_udp_message(log_msg);
        }
    }
    
    if (udp_server_socket >= 0) {
        close(udp_server_socket);
        udp_server_socket = -1;
    }
    
    free(buffer);
    free(response);
    log_udp_message("GPS UDP server thread stopped");
    return NULL;
}

static void format_gps_response(char *buffer, size_t buffer_size) {
    gps_data_t gps_data;
    
    if (gps_get_data(&gps_data)) {
        // Build response with all GPS parameters
        char lat_str[32], lon_str[32], alt_str[32], head_str[32], speed_str[32], sats_str[16];
        
        // Format position data
        if (gps_data.valid_position) {
            snprintf(lat_str, sizeof(lat_str), "%.6f", gps_data.latitude);
            snprintf(lon_str, sizeof(lon_str), "%.6f", gps_data.longitude);
            snprintf(alt_str, sizeof(alt_str), "%.1f", gps_data.altitude);
        } else {
            strcpy(lat_str, "N/A");
            strcpy(lon_str, "N/A");
            strcpy(alt_str, "N/A");
        }
        
        // Format heading data
        if (gps_data.valid_heading) {
            snprintf(head_str, sizeof(head_str), "%.2f", gps_data.heading);
        } else {
            strcpy(head_str, "N/A");
        }
        
        // Format speed data
        if (gps_data.valid_speed) {
            snprintf(speed_str, sizeof(speed_str), "%.3f", gps_data.speed_ms);
        } else {
            strcpy(speed_str, "N/A");
        }
        
        // Format satellite data
        if (gps_data.valid_satellites) {
            snprintf(sats_str, sizeof(sats_str), "%d", gps_data.num_satellites);
        } else {
            strcpy(sats_str, "N/A");
        }
        
        snprintf(buffer, buffer_size, 
                "gps_lat:%s,gps_lon:%s,gps_alt:%s,gps_head:%s,gps_speed:%s,gps_sats:%s",
                lat_str, lon_str, alt_str, head_str, speed_str, sats_str);
    } else {
        // No GPS data available
        snprintf(buffer, buffer_size, "gps_lat:N/A,gps_lon:N/A,gps_alt:N/A,gps_head:N/A,gps_speed:N/A,gps_sats:N/A");
    }
}

static void log_udp_message(const char *message) {
    if (udp_log_file != NULL) {
        time_t now;
        time(&now);
        char *date = ctime(&now);
        date[strlen(date) - 1] = '\0'; // Remove newline
        fprintf(udp_log_file, "%s : gps.c : udp_server : %s\n", date, message);
        fflush(udp_log_file);
    }
    
    // Removed stderr logging to prevent CLI spam
    // Only log to file now
}

// Public UDP Server API functions
bool gps_start_udp_server(void) {
    if (udp_server_running) {
        log_udp_message("GPS UDP server is already running");
        return false;
    }
    
    // Open UDP log file in the session folder
    char udp_log_path[576];
    snprintf(udp_log_path, sizeof(udp_log_path), "%s/gps_udp_server.log", session_folder);
    udp_log_file = fopen(udp_log_path, "a");
    if (udp_log_file == NULL) {
        fprintf(stderr, "Warning: Could not open UDP log file: %s\n", strerror(errno));
        // Continue anyway, we'll log to stderr
    }
    
    udp_server_running = true;
    
    if (pthread_create(&udp_server_thread, NULL, udp_server_thread_func, NULL) != 0) {
        log_udp_message("Error creating GPS UDP server thread");
        udp_server_running = false;
        if (udp_log_file != NULL) {
            fclose(udp_log_file);
            udp_log_file = NULL;
        }
        return false;
    }
    
    return true;
}

void gps_stop_udp_server(void) {
    if (!udp_server_running) {
        return;
    }
    
    log_udp_message("GPS UDP server shutdown initiated");
    udp_server_running = false;
    
    // Close socket to unblock recvfrom
    if (udp_server_socket >= 0) {
        close(udp_server_socket);
        udp_server_socket = -1;
    }
    
    // Give the thread a moment to notice the socket closure
    usleep(100000); // 100ms
    
    // Try to join the thread with a reasonable timeout approach
    // Since pthread_join doesn't have a timeout, we'll use pthread_tryjoin_np if available
    #ifdef _GNU_SOURCE
    struct timespec timeout_spec;
    timeout_spec.tv_sec = 2;  // 2 second timeout
    timeout_spec.tv_nsec = 0;
    
    int join_result = pthread_timedjoin_np(udp_server_thread, NULL, &timeout_spec);
    if (join_result == ETIMEDOUT) {
        log_udp_message("GPS UDP server thread join timed out - proceeding anyway");
    } else if (join_result != 0) {
        log_udp_message("GPS UDP server thread join failed - proceeding anyway");
    }
    #else
    // Fallback for non-GNU systems - just join normally
    pthread_join(udp_server_thread, NULL);
    #endif
    
    if (udp_log_file != NULL) {
        log_udp_message("GPS UDP server stopped");
        fclose(udp_log_file);
        udp_log_file = NULL;
    }
}

bool gps_is_udp_server_running(void) {
    return udp_server_running;
}

static bool is_authorized_client(const char *client_ip) {
    (void)client_ip;  // Suppress unused parameter warning
    return true;  // Accept all clients
}
