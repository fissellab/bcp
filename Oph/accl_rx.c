#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#define PORT 65432
#define BUFFER_SIZE 4096
#define CHUNK_DURATION 600 // 10 minutes in seconds
#define PRINT_INTERVAL 10000 // Print status every 10,000 samples
#define MAX_RETRIES 5
#define RETRY_DELAY 1000000 // 1 second in microseconds
#define NUM_ACCELEROMETERS 3

volatile sig_atomic_t keep_running = 1;
FILE *log_file = NULL;

void signal_handler(int signum) {
    keep_running = 0;
}

void log_message(const char *message) {
    time_t now;
    char timestamp[64];
    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file);
}

void create_log_file() {
    char log_folder[256] = "logs";
    char log_filename[512];
    char full_path[768];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    mkdir(log_folder, 0777);
    
    strftime(log_filename, sizeof(log_filename), "%Y-%m-%d_%H-%M-%S_accl_rx.log", t);
    
    snprintf(full_path, sizeof(full_path), "%s/%s", log_folder, log_filename);
    
    log_file = fopen(full_path, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Error creating log file '%s': %s\n", full_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void create_output_folders(char *base_output_folder, char output_folders[NUM_ACCELEROMETERS][256]) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char folder_name[256];
    
    strftime(folder_name, sizeof(folder_name), "outputs/%d-%m-%Y-%H-%M-accl-output", tm);
    sprintf(base_output_folder, "%s", folder_name);
    
    mkdir("outputs", 0777);
    mkdir(base_output_folder, 0777);
    
    for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
        snprintf(output_folders[i], 256, "%s/accl%d", base_output_folder, i+1);
        mkdir(output_folders[i], 0777);
    }
    
    log_message("Created output folders");
}

FILE* open_new_file(const char *output_folder, int chunk_number, double start_time) {
    char filename[256];
    sprintf(filename, "%s/%.6f_chunk_%04d.bin", output_folder, start_time, chunk_number);
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Error opening file: %s", strerror(errno));
        log_message(error_msg);
        exit(EXIT_FAILURE);
    }
    setvbuf(file, NULL, _IOFBF, BUFFER_SIZE);  // Set to full buffering
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Opened new file: %s", filename);
    log_message(log_msg);
    return file;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char base_output_folder[256];
    char output_folders[NUM_ACCELEROMETERS][256];
    char incomplete_line[256] = {0};
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    create_log_file();
    log_message("Program started");
    
    create_output_folders(base_output_folder, output_folders);
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message("Socket creation error");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, "192.168.0.23", &serv_addr.sin_addr) <= 0) {
        log_message("Invalid address/ Address not supported");
        return -1;
    }
    
    int retry_count = 0;
    while (retry_count < MAX_RETRIES) {
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Connection Failed. Retrying... (%d/%d)", retry_count + 1, MAX_RETRIES);
            log_message(error_msg);
            usleep(RETRY_DELAY);
            retry_count++;
        } else {
            break;
        }
    }
    
    if (retry_count == MAX_RETRIES) {
        log_message("Max retries reached. Exiting.");
        return -1;
    }
    
    log_message("Connected to server. Starting data collection...");
    
    FILE *current_files[NUM_ACCELEROMETERS] = {NULL};
    int chunk_numbers[NUM_ACCELEROMETERS] = {1, 1, 1};
    double chunk_start_times[NUM_ACCELEROMETERS] = {0, 0, 0};
    long samples_received[NUM_ACCELEROMETERS] = {0, 0, 0};
    time_t start_time_t, current_time_t;
    double start_times[NUM_ACCELEROMETERS] = {0, 0, 0};
    time(&start_time_t);
    
    while (keep_running) {
        int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (valread <= 0) {
            if (valread == 0) {
                log_message("Server closed the connection");
            } else {
                log_message("recv failed");
            }
            break;
        }
        
        buffer[valread] = '\0';  // Null-terminate the received data
        
        char *line_start = buffer;
        char *line_end;
        
        // Process any incomplete line from the previous iteration
        if (incomplete_line[0] != '\0') {
            char *newline = strchr(buffer, '\n');
            if (newline) {
                strncat(incomplete_line, buffer, newline - buffer + 1);
                line_start = newline + 1;
                
                int accel_id;
                double timestamp, x, y, z;
                if (sscanf(incomplete_line, "%d,%lf,%lf,%lf,%lf", &accel_id, &timestamp, &x, &y, &z) == 5) {
                    accel_id--; // Convert to 0-based index
                    if (accel_id >= 0 && accel_id < NUM_ACCELEROMETERS) {
                        if (start_times[accel_id] == 0) start_times[accel_id] = timestamp;
                        
                        if (current_files[accel_id] == NULL) {
                            chunk_start_times[accel_id] = timestamp;
                            current_files[accel_id] = open_new_file(output_folders[accel_id], chunk_numbers[accel_id], chunk_start_times[accel_id]);
                        }
                        
                        fwrite(&timestamp, sizeof(double), 1, current_files[accel_id]);
                        fwrite(&x, sizeof(double), 1, current_files[accel_id]);
                        fwrite(&y, sizeof(double), 1, current_files[accel_id]);
                        fwrite(&z, sizeof(double), 1, current_files[accel_id]);
                        
                        samples_received[accel_id]++;
                        
                        if (samples_received[accel_id] % PRINT_INTERVAL == 0) {
                            time(&current_time_t);
                            double elapsed_time = difftime(current_time_t, start_time_t);
                            double average_rate = samples_received[accel_id] / (timestamp - start_times[accel_id]);
                            
                            char status_msg[512];
                            snprintf(status_msg, sizeof(status_msg), "Accelerometer %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                                    accel_id + 1, samples_received[accel_id], elapsed_time, average_rate);
                            log_message(status_msg);
                        }
                        
                        if (timestamp - chunk_start_times[accel_id] >= CHUNK_DURATION) {
                            fclose(current_files[accel_id]);
                            chunk_numbers[accel_id]++;
                            current_files[accel_id] = open_new_file(output_folders[accel_id], chunk_numbers[accel_id], timestamp);
                            chunk_start_times[accel_id] = timestamp;
                        }
                    } else {
                        char error_msg[512];
                        snprintf(error_msg, sizeof(error_msg), "Warning: Invalid accelerometer ID: %d", accel_id + 1);
                        log_message(error_msg);
                    }
                } else {
                    char error_msg[512];
                    snprintf(error_msg, sizeof(error_msg), "Warning: Invalid data format in incomplete line: %s", incomplete_line);
                    log_message(error_msg);
                }
                
                incomplete_line[0] = '\0';  // Clear the incomplete line buffer
            }
        }
        
        while ((line_end = strchr(line_start, '\n')) != NULL) {
            *line_end = '\0';  // Temporarily replace newline with null terminator
            
            int accel_id;
            double timestamp, x, y, z;
            if (sscanf(line_start, "%d,%lf,%lf,%lf,%lf", &accel_id, &timestamp, &x, &y, &z) == 5) {
                accel_id--; // Convert to 0-based index
                if (accel_id >= 0 && accel_id < NUM_ACCELEROMETERS) {
                    if (start_times[accel_id] == 0) start_times[accel_id] = timestamp;
                    
                    if (current_files[accel_id] == NULL) {
                        chunk_start_times[accel_id] = timestamp;
                        current_files[accel_id] = open_new_file(output_folders[accel_id], chunk_numbers[accel_id], chunk_start_times[accel_id]);
                    }
                    
                    fwrite(&timestamp, sizeof(double), 1, current_files[accel_id]);
                    fwrite(&x, sizeof(double), 1, current_files[accel_id]);
                    fwrite(&y, sizeof(double), 1, current_files[accel_id]);
                    fwrite(&z, sizeof(double), 1, current_files[accel_id]);
                    
                    samples_received[accel_id]++;
                    
                    if (samples_received[accel_id] % PRINT_INTERVAL == 0) {
                        time(&current_time_t);
                        double elapsed_time = difftime(current_time_t, start_time_t);
                        double average_rate = samples_received[accel_id] / (timestamp - start_times[accel_id]);
                        
                        char status_msg[512];
                        snprintf(status_msg, sizeof(status_msg), "Accelerometer %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                                accel_id + 1, samples_received[accel_id], elapsed_time, average_rate);
                        log_message(status_msg);
                    }
                    
                    if (timestamp - chunk_start_times[accel_id] >= CHUNK_DURATION) {
                        fclose(current_files[accel_id]);
                        chunk_numbers[accel_id]++;
                        current_files[accel_id] = open_new_file(output_folders[accel_id], chunk_numbers[accel_id], timestamp);
                        chunk_start_times[accel_id] = timestamp;
                    }
                } else {
                    char error_msg[512];
                    snprintf(error_msg, sizeof(error_msg), "Warning: Invalid accelerometer ID: %d", accel_id + 1);
                    log_message(error_msg);
                }
            } else {
                char error_msg[512];
                snprintf(error_msg, sizeof(error_msg), "Warning: Invalid data format: %s", line_start);
                log_message(error_msg);
            }
            
            line_start = line_end + 1;  // Move to the start of the next line
        }
        
        // If there's any remaining incomplete line, save it for the next iteration
        if (*line_start != '\0') {
            strcpy(incomplete_line, line_start);
        }
    }
    
    for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
        if (current_files[i] != NULL) {
            fclose(current_files[i]);
        }
    }
    
    close(sock);
    
    char final_msg[512];
    snprintf(final_msg, sizeof(final_msg), "Data collection complete. Total samples received: Accelerometer 1: %ld, Accelerometer 2: %ld, Accelerometer 3: %ld", 
             samples_received[0], samples_received[1], samples_received[2]);
    log_message(final_msg);
    
    fclose(log_file);
    return 0;
}