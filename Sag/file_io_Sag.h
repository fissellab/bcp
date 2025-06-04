#ifndef FILE_IO_SAG_H
#define FILE_IO_SAG_H

#include <stdio.h>
#include "gps.h"

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
    } rfsoc;
    struct {
        int enabled;
        char port[256];
        int baud_rate;
        char data_save_path[256];
        int file_rotation_interval;
        
        // Telemetry configuration
        int telemetry_enabled;
        char telemetry_host[64];
        char telemetry_port[8];
    } gps;
} conf_params_t;

extern conf_params_t config;

void read_in_config(const char* filepath);

#endif
