#ifndef FILE_IO_SAG_H
#define FILE_IO_SAG_H

#include <stdio.h>
#include "gps.h"

#define MAX_UDP_CLIENTS 10  // Maximum number of UDP clients supported

void write_to_log(FILE* logfile, const char* file, const char* function, const char* message);

typedef struct conf_params {
    struct {
        char logpath[256];
        char cmdlog[256];
    } main;
    struct {
        int enabled;
        char ip_address[16];
        char mode[5];
        int data_save_interval;
        char data_save_path[256];
        char fpga_bitstream[256];
        int adc_channel;
        int accumulation_length;
        int num_channels;
        int num_fft_points;
        // Power control settings
        int pbob_id;
        int relay_id;
    } rfsoc;
    struct {
        int enabled;
        char port[256];
        int baud_rate;
        char data_save_path[256];
        int file_rotation_interval;
        
        // UDP server settings
        int udp_server_enabled;
        int udp_server_port;
        char udp_client_ips[MAX_UDP_CLIENTS][16];  // Array of client IPs
        int udp_client_count;                      // Number of authorized clients
        int udp_buffer_size;
        
        // Power control settings
        int pbob_id;
        int relay_id;
    } gps;
    struct {
        int enabled;
        int udp_server_port;
        char udp_client_ips[MAX_UDP_CLIENTS][16];  // Array of client IPs
        int udp_client_count;                      // Number of authorized clients
        int udp_buffer_size;
        int max_request_rate;                      // Max requests per second per client
        
        // High-res filtering parameters for water maser
        double water_maser_freq;                   // 22.235 GHz
        double zoom_window_width;                  // 0.010 GHz (±10 MHz)
        double if_lower;                           // 20.96608 GHz
        double if_upper;                           // 22.93216 GHz
    } spectrometer_server;
    struct {
        int enabled;
        char ip[16];
        int port;
        int timeout;
        int udp_buffer_size;
        char udp_client_ips[MAX_UDP_CLIENTS][16];  // Array of authorized client IPs
        int udp_client_count;                      // Number of authorized clients
    } telemetry_server;
    struct {
        int enabled;
        char ip[16];
        int port;
        int timeout;
    } pbob_client;
    
    struct {
        int enabled;
        char aquila_ip[16];
        int aquila_port;
        int timeout;
        int ping_timeout;
        int status_check_interval;
    } vlbi;
    
    struct {
        int enabled;
        char rfsoc_ip[16];
        int rfsoc_port;
        int timeout;
    } rfsoc_daemon;
    
    struct {
        int enabled;
        char port[256];
        int baud_rate;
        char data_save_path[256];
        int file_rotation_interval;
        
        // Power control settings
        int pbob_id;
        int relay_id;
    } ticc;

    struct {
        int enabled;
        char logfile[16];
        char heater_ip[16];
        int pbob_id;
        int relay_id;
        int port;
        char server_ip[16];
        char workdir[256];
        int current_cap;
        int timeout;
    } heaters;
    
    struct {
        int enabled;
        char port[256];               // Serial port path (e.g., "/dev/tec-controller")
        float setpoint_temp;          // Target temperature in °C
        float kp;                     // PID proportional gain
        float ki;                     // PID integral gain  
        float kd;                     // PID derivative gain
        float deadband;               // Temperature deadband in °C
        char data_save_path[256];     // Directory for log files
    } pr59;
} conf_params_t;

extern conf_params_t config;

void read_in_config(const char* filepath);

#endif
