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
#define TEC_FAN_DELAY 5  // Minimum delay in seconds between TEC and fan activation
#define CURRENT_STABILIZATION_THRESHOLD 0.1  // Amps - consider stable when change is below this
#define CURRENT_STABILIZATION_TIME 3    // Seconds of stability required before fan activation
#define SOFT_START_STEPS 5              // Number of steps for power ramp-up
#define SOFT_START_INITIAL_POWER 10.0   // Initial power percentage (10%)
#define SOFT_START_MAX_POWER 50.0       // Maximum power during soft start (50%)
#define SOFT_START_STEP_DELAY 1         // Delay between power steps in seconds

// BCP-compatible configuration structure
typedef struct {
    int enabled;
    char port[256];
    float setpoint_temp;
    float kp;
    float ki;
    float kd;
    float deadband;
    char data_save_path[256];
} PR59_Config;

// Global variables
int serial_port = -1;
FILE* log_file = NULL;
volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t fan_override_enable = 0;  // 0=automatic, 1=force ON, -1=force OFF

PR59_Config config;

// Function prototypes
void signal_handler(int signum);
void fan_enable_handler(int signum);
void fan_disable_handler(int signum);

void cleanup(void);
int open_serial_port(const char *port);
void configure_serial_port(int fd);
int send_command(int fd, const char *cmd, char *response);
float read_temperature(int fd);
float read_fet_temperature(int fd);
float read_voltage(int fd);
float read_current(int fd);
int load_bcp_config(const char* config_file_path, PR59_Config* pr59_config);
int load_runtime_config(PR59_Config* pr59_config);
int save_runtime_config(const PR59_Config* pr59_config);
void clear_line(void);
void set_fan_mode(int fd, int mode, char *response);
void set_regulation_mode(int fd, int mode, char *response);
void set_output_power(int fd, float power, char *response);
void set_output_limit(int fd, float limit, char *response);
int initialize_with_register_clear_and_soft_start(int fd, PR59_Config* config);
int clear_all_registers(int fd);
void apply_fan_override(int fd);
void update_pid_parameters(int fd, const pr59_pid_update_t* update);
pr59_fan_status_t read_fan_status(int fd);
void process_pid_updates(int fd);

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    keep_running = 0;
}

// Signal handler for fan enable (SIGUSR1)
void fan_enable_handler(int signum) {
    fan_override_enable = 1;  // Force fan ON
    printf("\n=== Fan Override: ENABLED ===\n");
    if (log_file != NULL) {
        fprintf(log_file, "# Fan override enabled by ground station command\n");
        fflush(log_file);
    }
}

// Signal handler for fan disable (SIGUSR2)
void fan_disable_handler(int signum) {
    fan_override_enable = -1;  // Force fan OFF
    printf("\n=== Fan Override: DISABLED ===\n");
    if (log_file != NULL) {
        fprintf(log_file, "# Fan override disabled by ground station command\n");
        fflush(log_file);
    }
}



// Cleanup function
void cleanup(void) {
    if (serial_port >= 0) {
        char response[BUFFER_SIZE];
        // Graceful shutdown: stop regulation first
        send_command(serial_port, "$Q", response);
        close(serial_port);
    }
    if (log_file != NULL) {
        fclose(log_file);
    }
    // Cleanup shared memory interface
    pr59_interface_cleanup();
}

// Initialize serial port
int open_serial_port(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening serial port %s: %s\n", port, strerror(errno));
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

// Clear all TEC controller registers (CRITICAL for clean initialization)
int clear_all_registers(int fd) {
    char response[BUFFER_SIZE];
    char cmd[20];
    
    printf("=== CLEARING ALL TEC CONTROLLER REGISTERS ===\n");
    
    // Stop any current regulation first
    printf("Stopping all regulation...\n");
    send_command(fd, "$Q", response);
    usleep(500000); // 500ms delay
    
    // Clear critical control registers
    printf("Clearing control registers...\n");
    
    // Clear setpoint temperature (register 0)
    send_command(fd, "$R0=0", response);
    usleep(50000);
    
    // Clear PID parameters (registers 1, 2, 3)
    send_command(fd, "$R1=0", response);
    usleep(50000);
    send_command(fd, "$R2=0", response);
    usleep(50000);
    send_command(fd, "$R3=0", response);
    usleep(50000);
    
    // Clear output limit (register 6)
    send_command(fd, "$R6=0", response);
    usleep(50000);
    
    // Clear deadband (register 7)
    send_command(fd, "$R7=0", response);
    usleep(50000);
    
    // Set regulation mode to OFF (register 13)
    printf("Setting regulation mode to OFF...\n");
    send_command(fd, "$R13=0", response);
    usleep(50000);
    
    // Clear fan modes (registers 16, 23) - but allow automatic control later
    printf("Resetting fan control modes to automatic...\n");
    send_command(fd, "$R16=0", response); // Fan1 off initially
    usleep(50000);
    send_command(fd, "$R23=0", response); // Fan2 off initially  
    usleep(50000);
    
    // Save cleared state to EEPROM
    printf("Saving cleared state to EEPROM...\n");
    send_command(fd, "$RW", response);
    usleep(1000000); // 1 second delay for EEPROM write
    
    printf("=== REGISTER CLEARING COMPLETE ===\n");
    return 0;
}

// Set regulation mode (0=off, 1=power mode, 6=PID mode)
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

// Set fan mode (0=off, 2=cooling mode - AUTOMATIC)
void set_fan_mode(int fd, int mode, char *response) {
    char cmd[20];
    
    // Set Fan1 mode - use mode 2 (cooling mode) for automatic operation
    snprintf(cmd, sizeof(cmd), "$R16=%d", mode);
    send_command(fd, cmd, response);
    
    // Set Fan2 mode (if present) - use mode 2 (cooling mode) for automatic operation
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

// Load configuration from BCP config file (libconfig format)
int load_bcp_config(const char* config_file_path, PR59_Config* pr59_config) {
    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, config_file_path)) {
        fprintf(stderr, "TEC Controller: Config file error %s:%d - %s\n", 
                config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    // Read PR59 section from BCP config
    const char* tmpstr;
    double temp_double;
    
    // Check if PR59 is enabled
    if (!config_lookup_int(&cfg, "pr59.enabled", &pr59_config->enabled)) {
        printf("TEC Controller: pr59.enabled not found in config\n");
        pr59_config->enabled = 0;
    }
    
    if (!pr59_config->enabled) {
        printf("TEC Controller: PR59 is disabled in configuration\n");
        config_destroy(&cfg);
        return -1;
    }
    
    // Read serial port
    if (config_lookup_string(&cfg, "pr59.port", &tmpstr)) {
        strncpy(pr59_config->port, tmpstr, sizeof(pr59_config->port) - 1);
        pr59_config->port[sizeof(pr59_config->port) - 1] = '\0';
    } else {
        strcpy(pr59_config->port, DEFAULT_PORT);
    }
    
    // Read PID parameters
    if (config_lookup_float(&cfg, "pr59.setpoint_temp", &temp_double)) {
        pr59_config->setpoint_temp = (float)temp_double;
    } else {
        pr59_config->setpoint_temp = 25.0;
    }
    
    if (config_lookup_float(&cfg, "pr59.kp", &temp_double)) {
        pr59_config->kp = (float)temp_double;
    } else {
        pr59_config->kp = 30.0;
    }
    
    if (config_lookup_float(&cfg, "pr59.ki", &temp_double)) {
        pr59_config->ki = (float)temp_double;
    } else {
        pr59_config->ki = 0.031;
    }
    
    if (config_lookup_float(&cfg, "pr59.kd", &temp_double)) {
        pr59_config->kd = (float)temp_double;
    } else {
        pr59_config->kd = 360.0;
    }
    
    if (config_lookup_float(&cfg, "pr59.deadband", &temp_double)) {
        pr59_config->deadband = (float)temp_double;
    } else {
        pr59_config->deadband = 0.1;
    }
    
    // Read data save path
    if (config_lookup_string(&cfg, "pr59.data_save_path", &tmpstr)) {
        strncpy(pr59_config->data_save_path, tmpstr, sizeof(pr59_config->data_save_path) - 1);
        pr59_config->data_save_path[sizeof(pr59_config->data_save_path) - 1] = '\0';
    } else {
        strcpy(pr59_config->data_save_path, "/tmp/PR59_data");
    }

    config_destroy(&cfg);
    return 0;
}

// Load runtime configuration overrides (if they exist)
int load_runtime_config(PR59_Config* pr59_config) {
    config_t cfg;
    config_init(&cfg);
    
    const char* runtime_config_path = "pr59_runtime.config";
    
    // Check if runtime config file exists
    if (access(runtime_config_path, F_OK) != 0) {
        printf("No runtime config file found. Using default values.\n");
        return 0; // No runtime config is fine
    }

    if (!config_read_file(&cfg, runtime_config_path)) {
        fprintf(stderr, "TEC Controller: Runtime config file error %s:%d - %s\n", 
                config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    printf("Loading runtime PID parameter overrides...\n");
    
    // Override PID parameters if they exist in runtime config
    double temp_double;
    if (config_lookup_float(&cfg, "kp", &temp_double)) {
        pr59_config->kp = (float)temp_double;
        printf("Runtime override: Kp = %.3f\n", pr59_config->kp);
    }
    
    if (config_lookup_float(&cfg, "ki", &temp_double)) {
        pr59_config->ki = (float)temp_double;
        printf("Runtime override: Ki = %.3f\n", pr59_config->ki);
    }
    
    if (config_lookup_float(&cfg, "kd", &temp_double)) {
        pr59_config->kd = (float)temp_double;
        printf("Runtime override: Kd = %.3f\n", pr59_config->kd);
    }

    config_destroy(&cfg);
    return 0;
}

// Save runtime configuration
int save_runtime_config(const PR59_Config* pr59_config) {
    FILE* file = fopen("pr59_runtime.config", "w");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not create runtime config file\n");
        return -1;
    }
    
    fprintf(file, "# PR59 Runtime Configuration\n");
    fprintf(file, "# This file contains runtime overrides for PID parameters\n");
    fprintf(file, "# Generated automatically by TEC controller\n\n");
    fprintf(file, "kp = %.6f;\n", pr59_config->kp);
    fprintf(file, "ki = %.6f;\n", pr59_config->ki);
    fprintf(file, "kd = %.6f;\n", pr59_config->kd);
    
    fclose(file);
    printf("Runtime configuration saved to pr59_runtime.config\n");
    return 0;
}

// Clear the current line for updating display
void clear_line(void) {
    printf("\r\033[K"); // Carriage return and clear line
}

// Initialize the system with complete register clearing and soft start
int initialize_with_register_clear_and_soft_start(int fd, PR59_Config* config) {
    char response[BUFFER_SIZE];
    char cmd[20];
    int stable_readings = 0;
    float previous_current = 0.0;
    float current_diff = 0.0;
    float step_size = (SOFT_START_MAX_POWER - SOFT_START_INITIAL_POWER) / SOFT_START_STEPS;

    // STEP 1: CLEAR ALL REGISTERS (CRITICAL!)
    if (clear_all_registers(fd) != 0) {
        printf("ERROR: Failed to clear registers!\n");
        return -1;
    }

    // STEP 2: Set a conservative output limit initially
    printf("Setting initial output limit to %.1f%%...\n", SOFT_START_MAX_POWER);
    set_output_limit(fd, SOFT_START_MAX_POWER, response);

    // STEP 3: Set regulation to power mode for soft start
    printf("Switching to power mode for soft start...\n");
    set_regulation_mode(fd, 1, response); // 1 = POWER mode

    // Save to EEPROM
    send_command(fd, "$RW", response);

    // STEP 4: Start with low power
    printf("Starting soft power ramp-up...\n");
    printf("Power: %.1f%%\n", SOFT_START_INITIAL_POWER);
    set_output_power(fd, SOFT_START_INITIAL_POWER, response);
    send_command(fd, "$W", response); // Start regulation

    // STEP 5: Gradually increase power
    for (int step = 1; step <= SOFT_START_STEPS; step++) {
        sleep(SOFT_START_STEP_DELAY);
        float power = SOFT_START_INITIAL_POWER + (step * step_size);
        printf("Power: %.1f%%\n", power);
        set_output_power(fd, power, response);
    }

    // STEP 6: Switch to PID mode
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
    
    // STEP 7: Wait for minimum delay regardless of current stability
    printf("Waiting for minimum delay of %d seconds...\n", TEC_FAN_DELAY);
    sleep(TEC_FAN_DELAY);
    
    // STEP 8: Monitor current until it stabilizes
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
    
    printf("Current has stabilized!\n");
    
    // STEP 9: Enable AUTOMATIC fan control (mode 2 = cooling mode)
    printf("Enabling AUTOMATIC fan control...\n");
    set_fan_mode(fd, 2, response);  // 2 = COOLING mode (automatic)
    
    return 0;
}

// Apply fan override settings based on ground station commands
void apply_fan_override(int fd) {
    char response[BUFFER_SIZE];
    
    if (fan_override_enable == 1) {
        // Force fan ON (mode 1 = always on)
        set_fan_mode(fd, 1, response);
        printf(" [FAN: FORCED ON]");
    } else if (fan_override_enable == -1) {
        // Force fan OFF (mode 0 = always off)
        set_fan_mode(fd, 0, response);
        printf(" [FAN: FORCED OFF]");
    }
    // If fan_override_enable == 0, we keep the automatic mode (mode 2) that was set during initialization
}

// Read current fan status from TEC controller registers
pr59_fan_status_t read_fan_status(int fd) {
    char response[BUFFER_SIZE];
    float fan1_mode = -1, fan2_mode = -1;
    
    // Read Fan1 mode (register 16)
    if (send_command(fd, "$R16?", response) > 0) {
        sscanf(response, "$R16?\r\n%f", &fan1_mode);
    }
    
    // Read Fan2 mode (register 23) - if present
    if (send_command(fd, "$R23?", response) > 0) {
        sscanf(response, "$R23?\r\n%f", &fan2_mode);
    }
    
    // Determine status based on fan modes
    // Mode 0 = OFF, Mode 1 = ON, Mode 2 = AUTOMATIC
    if (fan1_mode < 0) {
        return FAN_ERROR; // Could not read status
    }
    
    if (fan1_mode == 0) {
        return FAN_FORCED_OFF;
    } else if (fan1_mode == 1) {
        return FAN_FORCED_ON;
    } else if (fan1_mode == 2) {
        return FAN_AUTO;
    } else {
        return FAN_ERROR; // Unknown mode
    }
}

// Update PID parameters in the TEC controller
void update_pid_parameters(int fd, const pr59_pid_update_t* update) {
    char cmd[50];
    char response[BUFFER_SIZE];
    
    printf("=== Updating PID Parameters ===\n");
    
    // Stop regulation temporarily
    send_command(fd, "$Q", response);
    usleep(100000); // 100ms delay
    
    if (update->update_kp) {
        snprintf(cmd, sizeof(cmd), "$R1=%.6f", update->new_kp);
        send_command(fd, cmd, response);
        config.kp = update->new_kp;
        printf("Updated Kp = %.6f\n", update->new_kp);
        if (log_file != NULL) {
            fprintf(log_file, "# PID Update: Kp = %.6f\n", update->new_kp);
        }
    }
    
    if (update->update_ki) {
        snprintf(cmd, sizeof(cmd), "$R2=%.6f", update->new_ki);
        send_command(fd, cmd, response);
        config.ki = update->new_ki;
        printf("Updated Ki = %.6f\n", update->new_ki);
        if (log_file != NULL) {
            fprintf(log_file, "# PID Update: Ki = %.6f\n", update->new_ki);
        }
    }
    
    if (update->update_kd) {
        snprintf(cmd, sizeof(cmd), "$R3=%.6f", update->new_kd);
        send_command(fd, cmd, response);
        config.kd = update->new_kd;
        printf("Updated Kd = %.6f\n", update->new_kd);
        if (log_file != NULL) {
            fprintf(log_file, "# PID Update: Kd = %.6f\n", update->new_kd);
        }
    }
    
    // Save to EEPROM
    send_command(fd, "$RW", response);
    usleep(500000); // 500ms delay for EEPROM write
    
    // Restart regulation
    send_command(fd, "$W", response);
    
    // Save runtime configuration for persistence
    save_runtime_config(&config);
    
    printf("PID parameters updated and saved successfully!\n");
}

// Process pending PID updates from shared memory
void process_pid_updates(int fd) {
    pr59_pid_update_t update;
    
    if (pr59_get_pid_update(&update)) {
        update_pid_parameters(fd, &update);
        pr59_clear_pid_update();
    }
}

int main(int argc, char *argv[]) {
    // Initialize shared memory interface first
    if (pr59_interface_init() != 0) {
        printf("ERROR: Failed to initialize PR59 shared memory interface\n");
        return 1;
    }

    if (argc < 2) {
        printf("Usage: %s <bcp_config_file>\n", argv[0]);
        pr59_interface_cleanup();
        return 1;
    }

    // Load configuration from BCP config file
    if (load_bcp_config(argv[1], &config) != 0) {
        printf("ERROR: Failed to load BCP configuration\n");
        pr59_interface_cleanup();
        return 1;
    }
    
    // Load runtime configuration overrides (if they exist)
    if (load_runtime_config(&config) != 0) {
        printf("WARNING: Failed to load runtime configuration\n");
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGUSR1, fan_enable_handler);   // Fan enable signal from BCP
    signal(SIGUSR2, fan_disable_handler);  // Fan disable signal from BCP

    atexit(cleanup);

    serial_port = open_serial_port(config.port);
    if (serial_port < 0) {
        pr59_interface_cleanup();
        return 1;
    }
    configure_serial_port(serial_port);

    // Create log file in BCP's data save path
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/tec_log_%04d%02d%02d_%02d%02d%02d.txt", 
             config.data_save_path, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    
    log_file = fopen(filename, "w");
    if (log_file == NULL) {
        printf("Warning: Could not create log file %s: %s\n", filename, strerror(errno));
        printf("Continuing without file logging...\n");
    } else {
        printf("Logging to: %s\n", filename);
        fprintf(log_file, "Timestamp,Temperature(°C),Status,FET_Temp(°C),Current(A),Voltage(V),Power(W)\n");
    }

    // Display header once
    printf("\n=== BCP-COMPATIBLE TEC CONTROLLER ===\n");
    printf("Configuration loaded from: %s\n", argv[1]);
    printf("Serial Port: %s\n", config.port);
    printf("Temperature Setpoint: %.2f°C\n", config.setpoint_temp);
    printf("PID Parameters:\n");
    printf("  Kp: %.3f\n", config.kp);
    printf("  Ki: %.3f\n", config.ki);
    printf("  Kd: %.3f\n", config.kd);
    printf("Deadband: ±%.2f°C\n", config.deadband);
    printf("Data Save Path: %s\n", config.data_save_path);
    printf("Fan Control: AUTOMATIC (cooling mode)\n");
    printf("Shared Memory: ENABLED\n\n");

    printf("Press Ctrl+C to stop\n");
    
    // Perform complete initialization with register clearing and soft start
    if (initialize_with_register_clear_and_soft_start(serial_port, &config) != 0) {
        printf("ERROR: Initialization failed!\n");
        return 1;
    }

    // Main monitoring loop
    printf("\nTimestamp               Temp(°C)  FET(°C)  Current(A)  Voltage(V)  Power(W)  Fan Override\n");
    printf("--------------------------------------------------------------------------------------------\n");

    while (keep_running) {
        float temp = read_temperature(serial_port);
        float fet_temp = read_fet_temperature(serial_port);
        float current = read_current(serial_port);
        float voltage = read_voltage(serial_port);
        float power = current * voltage;

        // Check for pending PID updates from shared memory
        process_pid_updates(serial_port);

        // Apply any fan override commands from ground station
        apply_fan_override(serial_port);
        
        // Read current fan status from hardware
        pr59_fan_status_t fan_status = read_fan_status(serial_port);
        pr59_update_fan_status(fan_status);

        // Update shared memory for BCP telemetry
        pr59_update_data(temp, fet_temp, current, voltage, 
                        config.kp, config.ki, config.kd, config.setpoint_temp);

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

        const char *status = "UNKNOWN";
        if (temp < (config.setpoint_temp - config.deadband)) status = "HEATING";
        else if (temp > (config.setpoint_temp + config.deadband)) status = "COOLING";
        else status = "REACHED";

        // Clear previous line and update with new values
        clear_line();
        printf("%s  %6.2f   %6.2f   %8.3f    %8.2f   %7.2f",
               timestamp, temp, fet_temp, current, voltage, power);
        
        // Show fan override status
        if (fan_override_enable == 1) {
            printf("  FORCED ON");
        } else if (fan_override_enable == -1) {
            printf("  FORCED OFF");
        } else {
            printf("  AUTOMATIC");
        }
        
        fflush(stdout);

        if (log_file != NULL) {
            fprintf(log_file, "%s,%.2f,%s,%.2f,%.3f,%.2f,%.2f\n",
                    timestamp, temp, status, fet_temp, current, voltage, power);
            fflush(log_file);
        }

        sleep(1);
    }

    printf("\nShutdown initiated...\n");
    return 0;
}