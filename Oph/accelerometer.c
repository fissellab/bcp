#include "accelerometer.h"
#include "file_io_Oph.h"
#include <sys/wait.h>

AccelerometerData accel_data;
FILE *accelerometer_log_file = NULL;
pid_t pi_script_pid = -1;

void accelerometer_log_message(const char *message) {
    time_t now;
    char timestamp[64];
    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(accelerometer_log_file, "[%s] %s\n", timestamp, message);
    fflush(accelerometer_log_file);
}

void accelerometer_create_log_file() {
    accelerometer_log_file = fopen(config.accelerometer.logfile, "w");
    if (accelerometer_log_file == NULL) {
        fprintf(stderr, "Error creating accelerometer log file '%s': %s\n", config.accelerometer.logfile, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void accelerometer_create_output_folders(char *base_output_folder, char output_folders[3][256]) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char folder_name[256];
    
    strftime(folder_name, sizeof(folder_name), "%Y-%m-%d-%H-%M-accl-output", tm);
    snprintf(base_output_folder, 256, "%s/%s", config.accelerometer.output_dir, folder_name);
    
    mkdir(config.accelerometer.output_dir, 0777);
    mkdir(base_output_folder, 0777);
    
    for (int i = 0; i < config.accelerometer.num_accelerometers; i++) {
        snprintf(output_folders[i], 256, "%s/accl%d", base_output_folder, i+1);
        mkdir(output_folders[i], 0777);
    }
    
    accelerometer_log_message("Created output folders");
}

FILE* accelerometer_open_new_file(const char *output_folder, int chunk_number, double start_time) {
    char filename[256];
    sprintf(filename, "%s/%.6f_chunk_%04d.bin", output_folder, start_time, chunk_number);
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Error opening file: %s", strerror(errno));
        accelerometer_log_message(error_msg);
        exit(EXIT_FAILURE);
    }
    setvbuf(file, NULL, _IOFBF, BUFFER_SIZE);  // Set to full buffering
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Opened new file: %s", filename);
    accelerometer_log_message(log_msg);
    return file;
}

int accelerometer_init() {
    accelerometer_create_log_file();
    accelerometer_log_message("Initializing accelerometer data collection");
    
    memset(&accel_data, 0, sizeof(AccelerometerData));
    accel_data.keep_running = 1;
    
    // Start the Raspberry Pi script
    pi_script_pid = fork();
    if (pi_script_pid == 0) {
        // Child process
        execl("/bin/bash", "bash", "start_pi_accelerometer.sh", NULL);
        exit(1);  // If execl fails
    } else if (pi_script_pid < 0) {
        accelerometer_log_message("Failed to start Raspberry Pi script");
        return -1;
    }
    
    // Parent process continues here
    accelerometer_log_message("Started Raspberry Pi accelerometer script");
    
    // Wait for the Raspberry Pi to start up (adjust the sleep time if needed)
    sleep(5);
    
    // Set up socket to connect to Pi
    if ((accel_data.sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        accelerometer_log_message("Socket creation error");
        return -1;
    }
    
    accel_data.serv_addr.sin_family = AF_INET;
    accel_data.serv_addr.sin_port = htons(config.accelerometer.port);
    
    if (inet_pton(AF_INET, config.accelerometer.raspberry_pi_ip, &accel_data.serv_addr.sin_addr) <= 0) {
        accelerometer_log_message("Invalid address/ Address not supported");
        return -1;
    }
    
    accelerometer_log_message("Attempting to connect to Raspberry Pi...");
    
    int retry_count = 0;
    while (retry_count < MAX_RETRIES) {
        if (connect(accel_data.sock, (struct sockaddr *)&accel_data.serv_addr, sizeof(accel_data.serv_addr)) < 0) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Connection Failed. Retrying... (%d/%d)", retry_count + 1, MAX_RETRIES);
            accelerometer_log_message(error_msg);
            sleep(1);
            retry_count++;
        } else {
            accelerometer_log_message("Connected to Raspberry Pi");
            break;
        }
    }
    
    if (retry_count == MAX_RETRIES) {
        accelerometer_log_message("Failed to connect after max retries");
        return -1;
    }
    
    if (pthread_create(&accel_data.thread_id, NULL, accelerometer_run, NULL) != 0) {
        accelerometer_log_message("Failed to create accelerometer thread");
        return -1;
    }
    
    return 0;
}

void *accelerometer_run(void *arg) {
    char buffer[BUFFER_SIZE] = {0};
    char base_output_folder[256];
    char output_folders[3][256];
    char incomplete_line[256] = {0};
    FILE *current_files[3] = {NULL};
    int chunk_numbers[3] = {1, 1, 1};
    double chunk_start_times[3] = {0, 0, 0};
    long samples_received[3] = {0, 0, 0};
    double start_times[3] = {0, 0, 0};
    time_t start_time_t, current_time_t;
    
    accelerometer_create_output_folders(base_output_folder, output_folders);
    
    time(&start_time_t);
    
    while (accel_data.keep_running) {
        int valread = recv(accel_data.sock, buffer, BUFFER_SIZE - 1, 0);
        if (valread <= 0) {
            if (valread == 0) {
                accelerometer_log_message("Raspberry Pi closed the connection");
            } else {
                accelerometer_log_message("recv failed");
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
                    if (accel_id >= 0 && accel_id < config.accelerometer.num_accelerometers) {
                        if (start_times[accel_id] == 0) start_times[accel_id] = timestamp;
                        
                        if (current_files[accel_id] == NULL) {
                            chunk_start_times[accel_id] = timestamp;
                            current_files[accel_id] = accelerometer_open_new_file(output_folders[accel_id], chunk_numbers[accel_id], chunk_start_times[accel_id]);
                        }
                        
                        fwrite(&timestamp, sizeof(double), 1, current_files[accel_id]);
                        fwrite(&x, sizeof(double), 1, current_files[accel_id]);
                        fwrite(&y, sizeof(double), 1, current_files[accel_id]);
                        fwrite(&z, sizeof(double), 1, current_files[accel_id]);
                        
                        samples_received[accel_id]++;
                        
                        if (samples_received[accel_id] % config.accelerometer.print_interval == 0) {
                            time(&current_time_t);
                            double elapsed_time = difftime(current_time_t, start_time_t);
                            double average_rate = samples_received[accel_id] / (timestamp - start_times[accel_id]);
                            
                            char status_msg[512];
                            snprintf(status_msg, sizeof(status_msg), "Accelerometer %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                                    accel_id + 1, samples_received[accel_id], elapsed_time, average_rate);
                            accelerometer_log_message(status_msg);
                        }
                        
                        if (timestamp - chunk_start_times[accel_id] >= config.accelerometer.chunk_duration) {
                            fclose(current_files[accel_id]);
                            chunk_numbers[accel_id]++;
                            current_files[accel_id] = accelerometer_open_new_file(output_folders[accel_id], chunk_numbers[accel_id], timestamp);
                            chunk_start_times[accel_id] = timestamp;
                        }
                    }
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
                if (accel_id >= 0 && accel_id < config.accelerometer.num_accelerometers) {
                    if (start_times[accel_id] == 0) start_times[accel_id] = timestamp;
                    
                    if (current_files[accel_id] == NULL) {
                        chunk_start_times[accel_id] = timestamp;
                        current_files[accel_id] = accelerometer_open_new_file(output_folders[accel_id], chunk_numbers[accel_id], chunk_start_times[accel_id]);
                    }
                    
                    fwrite(&timestamp, sizeof(double), 1, current_files[accel_id]);
                    fwrite(&x, sizeof(double), 1, current_files[accel_id]);
                    fwrite(&y, sizeof(double), 1, current_files[accel_id]);
                    fwrite(&z, sizeof(double), 1, current_files[accel_id]);
                    
                    samples_received[accel_id]++;
                    
                    if (samples_received[accel_id] % config.accelerometer.print_interval == 0) {
                        time(&current_time_t);
                        double elapsed_time = difftime(current_time_t, start_time_t);
                        double average_rate = samples_received[accel_id] / (timestamp - start_times[accel_id]);
                        
                        char status_msg[512];
                        snprintf(status_msg, sizeof(status_msg), "Accelerometer %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                                accel_id + 1, samples_received[accel_id], elapsed_time, average_rate);
                        accelerometer_log_message(status_msg);
                    }
                    
                    if (timestamp - chunk_start_times[accel_id] >= config.accelerometer.chunk_duration) {
                        fclose(current_files[accel_id]);
                        chunk_numbers[accel_id]++;
                        current_files[accel_id] = accelerometer_open_new_file(output_folders[accel_id], chunk_numbers[accel_id], timestamp);
                        chunk_start_times[accel_id] = timestamp;
                    }
                }
            }
            
            line_start = line_end + 1;  // Move to the start of the next line
        }
        
        // If there's any remaining incomplete line, save it for the next iteration
        if (*line_start != '\0') {
            strcpy(incomplete_line, line_start);
        }
    }
    
    for (int i = 0; i < config.accelerometer.num_accelerometers; i++) {
        if (current_files[i] != NULL) {
            fclose(current_files[i]);
        }
    }
    
    return NULL;
}

void accelerometer_shutdown() {
    accelerometer_log_message("Attempting to shut down accelerometer data collection");
    
    if (!accel_data.keep_running) {
        accelerometer_log_message("Accelerometer is already shut down");
        return;
    }
    
    accel_data.keep_running = 0;
    
    // Signal the thread to stop and wait for it to finish
    if (accel_data.thread_id != 0) {
        pthread_join(accel_data.thread_id, NULL);
        accel_data.thread_id = 0;
    }
    
    // Close the socket
    if (accel_data.sock != 0) {
        close(accel_data.sock);
        accel_data.sock = 0;
    }
    
    // Terminate the Raspberry Pi script
    if (pi_script_pid > 0) {
        kill(pi_script_pid, SIGTERM);
        waitpid(pi_script_pid, NULL, 0);
        pi_script_pid = -1;
    }
    
    // Close any open files
    for (int i = 0; i < config.accelerometer.num_accelerometers; i++) {
        if (accel_data.current_files[i] != NULL) {
            fclose(accel_data.current_files[i]);
            accel_data.current_files[i] = NULL;
        }
    }
    
    accelerometer_log_message("Accelerometer shutdown complete");
}

void accelerometer_get_status(AccelerometerStatus *status) {
    if (status == NULL) {
        accelerometer_log_message("Error: Null pointer passed to accelerometer_get_status");
        return;
    }
    
    status->is_running = accel_data.keep_running;
    
    for (int i = 0; i < config.accelerometer.num_accelerometers; i++) {
        status->samples_received[i] = accel_data.samples_received[i];
        status->chunk_numbers[i] = accel_data.chunk_numbers[i];
        status->start_times[i] = accel_data.start_times[i];
        status->chunk_start_times[i] = accel_data.chunk_start_times[i];
    }
}
