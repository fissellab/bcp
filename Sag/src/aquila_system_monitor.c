/**
 * Aquila System Monitor Daemon
 * 
 * Monitors system status on the aquila backend computer and sends
 * real-time status updates to Saggitarius via TCP connection.
 * 
 * Key features:
 * - Monitors both SSD 1 (/mnt/vlbi_data) and SSD 2 (/mnt/vlbi_data_2)
 * - Sends disk usage, CPU temp, memory usage to Saggitarius
 * - Starts automatically on boot via systemd
 * - Reconnects automatically if connection is lost
 * - Lightweight C implementation for maximum performance
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

// Configuration
#define SAGGITARIUS_IP "172.20.4.170"  // Saggitarius IP
#define SAGGITARIUS_PORT 8082           // Telemetry server port
#define UPDATE_INTERVAL_SEC 10          // Send updates every 10 seconds
#define RECONNECT_DELAY_SEC 5           // Wait 5 seconds before reconnecting
#define LOG_FILE "/var/log/aquila_monitor.log"
#define PID_FILE "/var/run/aquila_monitor.pid"

// SSD mount points
#define SSD1_MOUNT "/mnt/vlbi_data"
#define SSD2_MOUNT "/mnt/vlbi_data_2"

// Global variables
int running = 1;
int socket_fd = -1;
FILE* log_file = NULL;

// System status structure
typedef struct {
    // SSD 1 status
    int ssd1_mounted;
    float ssd1_used_gb;
    float ssd1_total_gb;
    float ssd1_percent_used;
    
    // SSD 2 status  
    int ssd2_mounted;
    float ssd2_used_gb;
    float ssd2_total_gb;
    float ssd2_percent_used;
    
    // System status
    float cpu_temp_celsius;
    float memory_used_gb;
    float memory_total_gb;
    float memory_percent_used;
    
    time_t timestamp;
} system_status_t;

// Logging function
void log_message(const char* level, const char* message) {
    time_t now = time(NULL);
    char* time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline
    
    if (log_file) {
        fprintf(log_file, "[%s] %s: %s\n", time_str, level, message);
        fflush(log_file);
    }
    
    // Also print to console for debugging
    printf("[%s] %s: %s\n", time_str, level, message);
}

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Received signal %d, shutting down gracefully", signum);
    log_message("INFO", msg);
    
    running = 0;
    
    if (socket_fd >= 0) {
        close(socket_fd);
    }
    
    if (log_file) {
        fclose(log_file);
    }
    
    // Remove PID file
    unlink(PID_FILE);
    
    exit(0);
}

// Get disk usage for a mount point
int get_disk_usage(const char* mount_point, float* used_gb, float* total_gb, float* percent_used) {
    struct statvfs stat;
    
    *used_gb = 0.0;
    *total_gb = 0.0;
    *percent_used = 0.0;
    
    if (statvfs(mount_point, &stat) != 0) {
        return 0; // Mount point not available
    }
    
    // Calculate sizes in GB
    unsigned long block_size = stat.f_frsize;
    unsigned long total_blocks = stat.f_blocks;
    unsigned long free_blocks = stat.f_bavail;
    unsigned long used_blocks = total_blocks - free_blocks;
    
    *total_gb = (total_blocks * block_size) / (1024.0 * 1024.0 * 1024.0);
    *used_gb = (used_blocks * block_size) / (1024.0 * 1024.0 * 1024.0);
    
    if (*total_gb > 0) {
        *percent_used = (*used_gb / *total_gb) * 100.0;
    }
    
    return 1; // Success
}

// Get CPU temperature
float get_cpu_temperature() {
    FILE* fp;
    char buffer[64];
    float temp = 0.0;
    
    // Try thermal zone 0 first
    fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            int temp_millicelsius = atoi(buffer);
            temp = temp_millicelsius / 1000.0;
        }
        fclose(fp);
        return temp;
    }
    
    // Fallback: try sensors command
    fp = popen("sensors 2>/dev/null | grep -E 'Core 0|Package id 0|Tctl' | head -n1 | grep -oE '[0-9]+\\.[0-9]+' | head -n1", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            temp = atof(buffer);
        }
        pclose(fp);
    }
    
    return temp;
}

// Get memory usage
void get_memory_usage(float* used_gb, float* total_gb, float* percent_used) {
    FILE* fp;
    char buffer[256];
    
    *used_gb = 0.0;
    *total_gb = 0.0;
    *percent_used = 0.0;
    
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return;
    }
    
    unsigned long mem_total_kb = 0;
    unsigned long mem_available_kb = 0;
    
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strncmp(buffer, "MemTotal:", 9) == 0) {
            sscanf(buffer, "MemTotal: %lu kB", &mem_total_kb);
        } else if (strncmp(buffer, "MemAvailable:", 13) == 0) {
            sscanf(buffer, "MemAvailable: %lu kB", &mem_available_kb);
        }
    }
    fclose(fp);
    
    if (mem_total_kb > 0) {
        *total_gb = mem_total_kb / (1024.0 * 1024.0);
        unsigned long mem_used_kb = mem_total_kb - mem_available_kb;
        *used_gb = mem_used_kb / (1024.0 * 1024.0);
        *percent_used = (*used_gb / *total_gb) * 100.0;
    }
}

// Collect all system status
void collect_system_status(system_status_t* status) {
    status->timestamp = time(NULL);
    
    // Check SSD 1
    status->ssd1_mounted = get_disk_usage(SSD1_MOUNT, 
                                         &status->ssd1_used_gb, 
                                         &status->ssd1_total_gb, 
                                         &status->ssd1_percent_used);
    
    // Check SSD 2
    status->ssd2_mounted = get_disk_usage(SSD2_MOUNT, 
                                         &status->ssd2_used_gb, 
                                         &status->ssd2_total_gb, 
                                         &status->ssd2_percent_used);
    
    // Get CPU temperature
    status->cpu_temp_celsius = get_cpu_temperature();
    
    // Get memory usage
    get_memory_usage(&status->memory_used_gb, 
                     &status->memory_total_gb, 
                     &status->memory_percent_used);
}

// Create status message in JSON format for telemetry server
int create_status_message(const system_status_t* status, char* buffer, size_t buffer_size) {
    return snprintf(buffer, buffer_size,
        "{"
        "\"type\":\"aquila_system_status\","
        "\"timestamp\":%ld,"
        "\"ssd1\":{"
            "\"mounted\":%d,"
            "\"used_gb\":%.2f,"
            "\"total_gb\":%.2f,"
            "\"percent_used\":%.1f"
        "},"
        "\"ssd2\":{"
            "\"mounted\":%d,"
            "\"used_gb\":%.2f,"
            "\"total_gb\":%.2f,"
            "\"percent_used\":%.1f"
        "},"
        "\"system\":{"
            "\"cpu_temp_celsius\":%.1f,"
            "\"memory_used_gb\":%.2f,"
            "\"memory_total_gb\":%.2f,"
            "\"memory_percent_used\":%.1f"
        "}"
        "}\n",
        status->timestamp,
        status->ssd1_mounted, status->ssd1_used_gb, status->ssd1_total_gb, status->ssd1_percent_used,
        status->ssd2_mounted, status->ssd2_used_gb, status->ssd2_total_gb, status->ssd2_percent_used,
        status->cpu_temp_celsius, status->memory_used_gb, status->memory_total_gb, status->memory_percent_used
    );
}

// Create UDP socket for sending to Saggitarius telemetry server
int create_udp_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        log_message("ERROR", "Failed to create UDP socket");
        return -1;
    }
    
    log_message("INFO", "Created UDP socket for telemetry updates");
    return sock;
}

// Send status message to Saggitarius via UDP
int send_status_update(const system_status_t* status) {
    char message[1024];
    int message_len;
    struct sockaddr_in server_addr;
    
    // Create JSON message
    message_len = create_status_message(status, message, sizeof(message));
    if (message_len <= 0) {
        log_message("ERROR", "Failed to create status message");
        return -1;
    }
    
    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SAGGITARIUS_PORT);
    
    if (inet_pton(AF_INET, SAGGITARIUS_IP, &server_addr.sin_addr) <= 0) {
        log_message("ERROR", "Invalid Saggitarius IP address");
        return -1;
    }
    
    // Send UDP message
    ssize_t bytes_sent = sendto(socket_fd, message, message_len, 0, 
                               (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bytes_sent < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to send status update: %s", strerror(errno));
        log_message("WARNING", error_msg);
        return -1;
    }
    
    // Log status for debugging
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), 
             "Status sent: SSD1=%s(%.1f%%), SSD2=%s(%.1f%%), CPU=%.1f°C, RAM=%.1f%%",
             status->ssd1_mounted ? "mounted" : "unmounted", status->ssd1_percent_used,
             status->ssd2_mounted ? "mounted" : "unmounted", status->ssd2_percent_used,
             status->cpu_temp_celsius, status->memory_percent_used);
    log_message("DEBUG", log_msg);
    
    return 0;
}

// Main monitoring loop
void monitoring_loop() {
    system_status_t status;
    
    log_message("INFO", "Starting monitoring loop");
    
    // Create UDP socket once
    socket_fd = create_udp_socket();
    if (socket_fd < 0) {
        log_message("ERROR", "Failed to create UDP socket, exiting");
        return;
    }
    
    while (running) {
        // Collect system status
        collect_system_status(&status);
        
        // Send status update
        if (send_status_update(&status) < 0) {
            log_message("WARNING", "Failed to send status update");
        }
        
        // Wait for next update
        sleep(UPDATE_INTERVAL_SEC);
    }
    
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

// Write PID file
int write_pid_file() {
    FILE* pid_file = fopen(PID_FILE, "w");
    if (pid_file == NULL) {
        log_message("ERROR", "Failed to create PID file");
        return -1;
    }
    
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
    return 0;
}

// Check if already running
int check_already_running() {
    FILE* pid_file = fopen(PID_FILE, "r");
    if (pid_file == NULL) {
        return 0; // Not running
    }
    
    int pid;
    if (fscanf(pid_file, "%d", &pid) == 1) {
        fclose(pid_file);
        
        // Check if process is still running
        if (kill(pid, 0) == 0) {
            log_message("ERROR", "Aquila monitor already running");
            return 1;
        }
    } else {
        fclose(pid_file);
    }
    
    // Remove stale PID file
    unlink(PID_FILE);
    return 0;
}

int main(int argc, char* argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    // Check if already running
    if (check_already_running()) {
        return 1;
    }
    
    // Open log file
    log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        fprintf(stderr, "Warning: Could not open log file %s\n", LOG_FILE);
    }
    
    log_message("INFO", "Aquila System Monitor starting...");
    
    // Write PID file
    if (write_pid_file() < 0) {
        return 1;
    }
    
    // Set up signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    
    // Start monitoring
    monitoring_loop();
    
    // Cleanup
    log_message("INFO", "Aquila System Monitor shutting down");
    signal_handler(0);
    return 0;
}
