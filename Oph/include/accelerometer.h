#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

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
#include <pthread.h>

#define BUFFER_SIZE 4096
#define MAX_RETRIES 5
#define RETRY_DELAY 1000000 // 1 second in microseconds

typedef struct {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    char incomplete_line[256];
    FILE *current_files[3];  // Assuming max 3 accelerometers
    int chunk_numbers[3];
    double chunk_start_times[3];
    long samples_received[3];
    double start_times[3];
    pthread_t thread_id;
    volatile sig_atomic_t keep_running;
    volatile sig_atomic_t is_initialized;
} AccelerometerData;

typedef struct {
    long samples_received[3];
    int chunk_numbers[3];
    double start_times[3];
    double chunk_start_times[3];
    int is_running;
    int is_initialized;
} AccelerometerStatus;

// Function declarations
void accelerometer_log_message(const char *message);
void accelerometer_create_log_file();
void accelerometer_create_output_folders(char *base_output_folder, char output_folders[3][256]);
FILE* accelerometer_open_new_file(const char *output_folder, int chunk_number, double start_time);
void *accelerometer_run(void *arg);
int accelerometer_init();
void accelerometer_shutdown();
void accelerometer_get_status(AccelerometerStatus *status);

// Global variables
extern AccelerometerData accel_data;
extern FILE *accelerometer_log_file;
extern pid_t pi_script_pid;

#endif // ACCELEROMETER_H
