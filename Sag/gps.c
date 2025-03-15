#include "gps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

#include "send_telemetry.h"

#define TELEMETRY_SERVER_IP_ADDR "192.168.2.4"
#define TELEMETRY_SERVER_PORT "8080"

static int fd;
static FILE *logfile = NULL;
static bool logging = false;
static pthread_t gps_thread;
static gps_config_t current_config;
static time_t last_file_rotation;
static char session_folder[512];

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

// loops getting gps data, logging it to file, and sending it to
// the telemetry server, until variable logging is false
static void *gps_logging_thread(void *arg) {
    (void)arg;  // Cast to void to explicitly ignore the parameter
    char buffer[GPS_BUFFER_SIZE];

    // socket configured to send to telemetry server
    int socket_fd = connected_udp_socket(
        TELEMETRY_SERVER_IP_ADDR,
        TELEMETRY_SERVER_PORT
    );

    // Stores copy of gps data with additional
    // byte for null terminator so it can be given
    // as string to send_sample_string
    char gps_data_string[GPS_BUFFER_SIZE + 1];
    
    while (logging) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            fprintf(stderr, "Error reading from GPS port: %s\n", strerror(errno));
            break;
        } else if (n == 0) {
            sleep(1);
            continue;
        }

        // get null terminated string gps_data_string 
        // from buffer containing gps data
        memcpy(gps_data_string, buffer, n);
        gps_data_string[n] = '\0';

        // send gps data to telemetry server
        send_sample_string(socket_fd, "gps", time(NULL), gps_data_string);

        // Write to log file in binary
        fwrite(buffer, 1, n, logfile);
        fflush(logfile);

        // Check if it's time to rotate the file
        time_t now = time(NULL);
        if (now - last_file_rotation >= current_config.file_rotation_interval) {
            rotate_logfile();
        }
    }

    return NULL;
}

int gps_init(const gps_config_t *config) {
    memcpy(&current_config, config, sizeof(gps_config_t));

    fd = open(current_config.port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Error opening GPS port %s: %s\n", current_config.port, strerror(errno));
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "Error from tcgetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, current_config.baud_rate);
    cfsetispeed(&tty, current_config.baud_rate);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "Error from tcsetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    create_session_folder();
    printf("GPS initialized on port %s\n", current_config.port);
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

    logging = true;
    if (pthread_create(&gps_thread, NULL, gps_logging_thread, NULL) != 0) {
        fprintf(stderr, "Error creating GPS logging thread: %s\n", strerror(errno));
        logging = false;
        fclose(logfile);
        logfile = NULL;
        return false;
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
    pthread_join(gps_thread, NULL);

    if (logfile != NULL) {
        fclose(logfile);
        logfile = NULL;
    }

    printf("GPS logging stopped.\n");
}

bool gps_is_logging(void) {
    return logging;
}
