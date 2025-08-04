#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <libconfig.h>
#include "pr59_interface.h"
#define BUFFER_SIZE 256
#define LOG_BUFFER_SIZE 1024
#define DEFAULT_PORT "/dev/tec-controller"
#define CONFIG_FILE "tec_config.ini"
#define TEC_FAN_DELAY 5  // Minimum delay in seconds between TEC and fan activation
#define CURRENT_STABILIZATION_THRESHOLD 0.1  // Amps - consider stable when change is below this
#define CURRENT_STABILIZATION_TIME 3    // Seconds of stability required before fan activation
#define SOFT_START_STEPS 5              // Number of steps for power ramp-up
#define SOFT_START_INITIAL_POWER 10.0   // Initial power percentage (10%)
#define SOFT_START_MAX_POWER 50.0       // Maximum power during soft start (50%)
#define SOFT_START_STEP_DELAY 1         // Delay between power steps in seconds

// Configuration structure
typedef struct {
    char port[256];
    float setpoint_temp;
    float kp;
    float ki;
    float kd;
    float deadband;
    char data_save_path[256];
} TEC_Config;

// Global variables
int serial_port = -1;
FILE* log_file = NULL;
volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t show_status = 0;  // Flag to control status display


// Function prototypes
void signal_handler(int signum);
void cleanup(void);
int open_serial_port(const char *port);
void configure_serial_port(int fd);
int send_command(int fd, const char *cmd, char *response);
float read_temperature(int fd);
float read_fet_temperature(int fd);
float read_voltage(int fd);
float read_current(int fd);
void load_config(TEC_Config* config, const char* config_file_path);
void save_config(TEC_Config* config);
void clear_line(void);
void set_fan_mode(int fd, int mode, char *response);
void set_regulation_mode(int fd, int mode, char *response);
void set_output_power(int fd, float power, char *response);
void set_output_limit(int fd, float limit, char *response);
int initialize_with_soft_start(int fd, TEC_Config* config);

// Signal handler for graceful shutdown and status control
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        keep_running = 0;
    } else if (signum == SIGUSR1) {
        show_status = !show_status;  // Toggle status display
    }
}

// Cleanup function
void cleanup(void) {
    if (serial_port >= 0) {
        char response[BUFFER_SIZE];
        send_command(serial_port, "$Q", response);
        close(serial_port);
    }
    if (log_file != NULL) {
        fclose(log_file);
    }
    
    // Cleanup telemetry interface
    pr59_interface_cleanup();
}

// Initialize serial port
int open_serial_port(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening serial port: %s\n", strerror(errno));
        return -1;
    }
    return fd;
}

// Configure serial port settings
void configure_serial_port(int fd) {
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        printf("Error getting serial port attributes: %s\n", strerror(errno));
        return;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error setting serial port attributes: %s\n", strerror(errno));
        return;
    }
}

// Send command and read response
int send_command(int fd, const char *cmd, char *response) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "%s\r", cmd);
    
    int written = write(fd, command, strlen(command));
    if (written < 0) {
        printf("Error writing to serial port: %s\n", strerror(errno));
        return -1;
    }

    usleep(100000);  // 100ms delay

    int bytes_read = read(fd, response, BUFFER_SIZE - 1);
    if (bytes_read < 0) {
        printf("Error reading from serial port: %s\n", strerror(errno));
        return -1;
    }

    response[bytes_read] = '\0';
    return bytes_read;
}

// Set regulation mode (1=power mode, 6=PID mode)
void set_regulation_mode(int fd, int mode, char *response) {
    char cmd[20];
    snprintf(cmd, sizeof(cmd), "$R13=%d", mode);
    send_command(fd, cmd, response);
}

// Set output power (for power mode)
void set_output_power(int fd, float power, char *response) {
    char cmd[20];
    // In power mode, register 0 sets the output power (-100 to +100)
    snprintf(cmd, sizeof(cmd), "$R0=%.1f", power);
    send_command(fd, cmd, response);
}

// Set output limit (register 6)
void set_output_limit(int fd, float limit, char *response) {
    char cmd[20];
    snprintf(cmd, sizeof(cmd), "$R6=%.1f", limit);
    send_command(fd, cmd, response);
}

// Set fan mode (0=off, 1=on, etc.)
void set_fan_mode(int fd, int mode, char *response) {
    char cmd[20];
    
    // Set Fan1 mode
    snprintf(cmd, sizeof(cmd), "$R16=%d", mode);
    send_command(fd, cmd, response);
    
    // Set Fan2 mode (if present)
    snprintf(cmd, sizeof(cmd), "$R23=%d", mode);
    send_command(fd, cmd, response);
    
    // Save to EEPROM
    send_command(fd, "$RW", response);
}

// Read temperature from device
float read_temperature(int fd) {
    char response[BUFFER_SIZE];
    float temp = 0.0;
    
    if (send_command(fd, "$R100?", response) > 0) {
        sscanf(response, "$R100?\r\n%f", &temp);
    }
    return temp;
}

// Read FET temperature from device
float read_fet_temperature(int fd) {
    char response[BUFFER_SIZE];
    float temp = 0.0;
    
    if (send_command(fd, "$R103?", response) > 0) {
        sscanf(response, "$R103?\r\n%f", &temp);
    }
    return temp;
}

// Read voltage from device
float read_voltage(int fd) {
    char response[BUFFER_SIZE];
    float voltage = 0.0;
    
    if (send_command(fd, "$R150?", response) > 0) {
        sscanf(response, "$R150?\r\n%f", &voltage);
    }
    return voltage;
}

// Read current from device
float read_current(int fd) {
    char response[BUFFER_SIZE];
    float current = 0.0;
    
    if (send_command(fd, "$R152?", response) > 0) {
        sscanf(response, "$R152?\r\n%f", &current);
    }
    return current;
}

// Load configuration from BCP config file
void load_config(TEC_Config* config, const char* config_file_path) {
    config_t cfg;
    config_init(&cfg);

    // Set default values first
    strncpy(config->port, DEFAULT_PORT, sizeof(config->port) - 1);
    config->port[sizeof(config->port) - 1] = '\0';
    config->setpoint_temp = 20.0;
    config->kp = 30.0;
    config->ki = 0.031;
    config->kd = 360.0;
    config->deadband = 0.1;
    strncpy(config->data_save_path, "/tmp/PR59_data", sizeof(config->data_save_path) - 1);
    config->data_save_path[sizeof(config->data_save_path) - 1] = '\0';

    if (!config_read_file(&cfg, config_file_path)) {
        fprintf(stderr, "Error reading config file %s:%d - %s\n", 
                config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
        fprintf(stderr, "Using default values\n");
        config_destroy(&cfg);
        return;
    }

    const char* tmpstr;
    
    // Read PR59 configuration section
    if (config_lookup_string(&cfg, "pr59.port", &tmpstr)) {
        strncpy(config->port, tmpstr, sizeof(config->port) - 1);
        config->port[sizeof(config->port) - 1] = '\0';
    }

    config_lookup_float(&cfg, "pr59.setpoint_temp", &config->setpoint_temp);
    config_lookup_float(&cfg, "pr59.kp", &config->kp);
    config_lookup_float(&cfg, "pr59.ki", &config->ki);
    config_lookup_float(&cfg, "pr59.kd", &config->kd);
    config_lookup_float(&cfg, "pr59.deadband", &config->deadband);

    if (config_lookup_string(&cfg, "pr59.data_save_path", &tmpstr)) {
        strncpy(config->data_save_path, tmpstr, sizeof(config->data_save_path) - 1);
        config->data_save_path[sizeof(config->data_save_path) - 1] = '\0';
    }

    config_destroy(&cfg);
}

// Save configuration to file
void save_config(TEC_Config* config) {
    FILE* fp = fopen(CONFIG_FILE, "w");
    if (fp == NULL) {
        printf("Error creating config file\n");
        return;
    }
    fprintf(fp, "setpoint_temp=%.2f\n", config->setpoint_temp);
    fprintf(fp, "kp=%.3f\n", config->kp);
    fprintf(fp, "ki=%.3f\n", config->ki);
    fprintf(fp, "kd=%.3f\n", config->kd);
    fprintf(fp, "deadband=%.3f\n", config->deadband);
    fclose(fp);
}

// Clear the current line for updating display
void clear_line(void) {
    printf("\r\033[K"); // Carriage return and clear line
}

// Initialize the system with soft start
int initialize_with_soft_start(int fd, TEC_Config* config) {
    char response[BUFFER_SIZE];
    char cmd[20];
    int stable_readings = 0;
    float previous_current = 0.0;
    float current_diff = 0.0;
    float step_size = (SOFT_START_MAX_POWER - SOFT_START_INITIAL_POWER) / SOFT_START_STEPS;

    // Make sure fan is OFF
    printf("Ensuring fan is off...\n");
    set_fan_mode(fd, 0, response);

    // Stop any current regulation
    printf("Stopping any current regulation...\n");
    send_command(fd, "$Q", response);

    // Set a conservative output limit initially
    printf("Setting initial output limit to %.1f%%...\n", SOFT_START_MAX_POWER);
    set_output_limit(fd, SOFT_START_MAX_POWER, response);

    // Set regulation to power mode
    printf("Switching to power mode for soft start...\n");
    set_regulation_mode(fd, 1, response); // 1 = POWER mode

    // Save to EEPROM
    send_command(fd, "$RW", response);

    // Start with low power
    printf("Starting soft power ramp-up...\n");
    printf("Power: %.1f%%\n", SOFT_START_INITIAL_POWER);
    set_output_power(fd, SOFT_START_INITIAL_POWER, response);
    send_command(fd, "$W", response); // Start regulation

    // Gradually increase power
    for (int step = 1; step <= SOFT_START_STEPS; step++) {
        sleep(SOFT_START_STEP_DELAY);
        float power = SOFT_START_INITIAL_POWER + (step * step_size);
        printf("Power: %.1f%%\n", power);
        set_output_power(fd, power, response);
    }

    // Switch to PID mode
    printf("Switching to PID mode...\n");
    
    // Stop regulation temporarily
    send_command(fd, "$Q", response);
    
    // Set PID parameters
    snprintf(cmd, sizeof(cmd), "$R0=%.2f", config->setpoint_temp);
    send_command(fd, cmd, response);
    
    snprintf(cmd, sizeof(cmd), "$R1=%.3f", config->kp);
    send_command(fd, cmd, response);
    
    snprintf(cmd, sizeof(cmd), "$R2=%.3f", config->ki);
    send_command(fd, cmd, response);
    
    snprintf(cmd, sizeof(cmd), "$R3=%.3f", config->kd);
    send_command(fd, cmd, response);
    
    snprintf(cmd, sizeof(cmd), "$R7=%.3f", config->deadband);
    send_command(fd, cmd, response);
    
    // Set regulation to PID mode
    set_regulation_mode(fd, 6, response); // 6 = PID mode
    
    // Save to EEPROM
    send_command(fd, "$RW", response);
    
    // Restart in PID mode
    send_command(fd, "$W", response);
    
    // Wait for minimum delay regardless of current stability
    printf("Waiting for minimum delay of %d seconds...\n", TEC_FAN_DELAY);
    sleep(TEC_FAN_DELAY);
    
    // Now monitor current until it stabilizes
    printf("Monitoring current stability...\n");
    previous_current = read_current(fd);
    
    while (stable_readings < CURRENT_STABILIZATION_TIME) {
        sleep(1);
        float current = read_current(fd);
        current_diff = fabs(current - previous_current);
        
        if (current_diff < CURRENT_STABILIZATION_THRESHOLD) {
            stable_readings++;
            printf("Current stable for %d/%d seconds (%.3fA, diff: %.3fA)\n", 
                  stable_readings, CURRENT_STABILIZATION_TIME, current, current_diff);
        } else {
            stable_readings = 0;
            printf("Current still fluctuating: %.3fA, diff: %.3fA\n", current, current_diff);
        }
        
        previous_current = current;
    }
    
    printf("Current has stabilized! Starting fan...\n");
    
    // Start fan
    set_fan_mode(fd, 1, response);  // 1 = always ON
    
    printf("PR59 running...\n");
    
    return 0;
}

int main(int argc, char *argv[]) {
    TEC_Config config;
    
    // Check for config file argument
    const char* config_file = (argc > 1) ? argv[1] : CONFIG_FILE;
    load_config(&config, config_file);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGUSR1, signal_handler);
    atexit(cleanup);

    // Initialize PR59 telemetry interface
    if (pr59_interface_init() != 0) {
        printf("Warning: Could not initialize PR59 telemetry interface\n");
    }

    serial_port = open_serial_port(config.port);
    if (serial_port < 0) {
        return 1;
    }
    configure_serial_port(serial_port);

    // Create log file
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[512];
    char log_name[100];
    strftime(log_name, sizeof(log_name), "tec_log_%Y%m%d_%H%M%S.txt", t);
    snprintf(filename, sizeof(filename), "%s/%s", config.data_save_path, log_name);
    // Create directory if it doesn't exist
    char mkdir_cmd[600];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", config.data_save_path);
    system(mkdir_cmd);
    
    log_file = fopen(filename, "w");
    if (log_file == NULL) {
        printf("Error creating log file at %s\n", filename);
        return 1;
    }
    
    printf("Logging to: %s\n", filename);

    fprintf(log_file, "Timestamp,Temperature(°C),Status,FET_Temp(°C),Current(A),Voltage(V),Power(W)\n");

    // Display header once
    printf("\nCurrent Controller Settings:\n");
    printf("----------------------------\n");
    printf("Temperature Setpoint: %.2f°C\n", config.setpoint_temp);
    printf("PID Parameters:\n");
    printf("  Kp: %.3f\n", config.kp);
    printf("  Ki: %.3f\n", config.ki);
    printf("  Kd: %.3f\n", config.kd);
    printf("Deadband: ±%.2f°C\n\n", config.deadband);
    printf("Soft Start Parameters:\n");
    printf("  Initial Power: %.1f%%\n", SOFT_START_INITIAL_POWER);
    printf("  Maximum Power: %.1f%%\n", SOFT_START_MAX_POWER);
    printf("  Steps: %d\n", SOFT_START_STEPS);
    printf("  Current Stabilization Threshold: %.3fA\n", CURRENT_STABILIZATION_THRESHOLD);
    printf("  Required Stability Time: %d seconds\n\n", CURRENT_STABILIZATION_TIME);

    printf("Press Ctrl+C to stop\n");
    
    // Perform soft start and initialization
    initialize_with_soft_start(serial_port, &config);

    // Main monitoring loop
    int header_shown = 0;

    while (keep_running) {
        float temp = read_temperature(serial_port);
        float fet_temp = read_fet_temperature(serial_port);
        float current = read_current(serial_port);
        float voltage = read_voltage(serial_port);
        float power = current * voltage;

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

        const char *status = "UNKNOWN";
        if (temp < (config.setpoint_temp - config.deadband)) status = "HEATING";
        else if (temp > (config.setpoint_temp + config.deadband)) status = "COOLING";
        else status = "REACHED";

        // Show status only when requested
        if (show_status) {
            if (!header_shown) {
                printf("\nTimestamp               Temp(°C)  FET(°C)  Current(A)  Voltage(V)  Power(W)\n");
                printf("------------------------------------------------------------------------\n");
                header_shown = 1;
            }
            // Clear previous line and update with new values
            clear_line();
            printf("%s  %6.2f   %6.2f   %8.3f    %8.2f   %7.2f",
                   timestamp, temp, fet_temp, current, voltage, power);
            fflush(stdout);
        } else {
            header_shown = 0;  // Reset header flag when status is hidden
        }

        // Always log to file
        fprintf(log_file, "%s,%.2f,%s,%.2f,%.3f,%.2f,%.2f\n",
                timestamp, temp, status, fet_temp, current, voltage, power);
        fflush(log_file);

        // Update telemetry interface for remote access
        pr59_update_data(temp, fet_temp, current, voltage, 
                        config.kp, config.ki, config.kd, config.setpoint_temp);

        sleep(1);
    }

    printf("\nShutdown initiated...\n");
    return 0;
}
