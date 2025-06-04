#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <libconfig.h>
#include <string.h>
#include <errno.h>
#include "file_io_Sag.h"
#include "cli_Sag.h"
#include "gps.h"

void print_config() {
    printf("Configuration parameters:\n");
    printf("Main settings:\n");
    printf("  Log path: %s\n", config.main.logpath);
    printf("  Command log: %s\n", config.main.cmdlog);
    printf("\nRFSoC Spectrometer settings:\n");
    printf("  Enabled: %s\n", config.rfsoc.enabled ? "Yes" : "No");
    printf("  IP Address: %s\n", config.rfsoc.ip_address);
    printf("  Mode: %s\n", config.rfsoc.mode);
    printf("  Data Save Interval: %d seconds\n", config.rfsoc.data_save_interval);
    printf("  Data Save Path: %s\n", config.rfsoc.data_save_path);
    printf("  FPGA Bitstream: %s\n", config.rfsoc.fpga_bitstream);
    printf("  ADC Channel: %d\n", config.rfsoc.adc_channel);
    printf("  Accumulation Length: %d\n", config.rfsoc.accumulation_length);
    printf("  Number of Channels: %d\n", config.rfsoc.num_channels);
    printf("  Number of FFT Points: %d\n", config.rfsoc.num_fft_points);
    printf("\nGPS settings:\n");
    printf("  Enabled: %s\n", config.gps.enabled ? "Yes" : "No");
    printf("  Port: %s\n", config.gps.port);
    printf("  Baud Rate: %d\n", config.gps.baud_rate);
    printf("  Data Save Path: %s\n", config.gps.data_save_path);
    printf("  File Rotation Interval: %d seconds\n", config.gps.file_rotation_interval);
    printf("  Telemetry: %s\n", config.gps.telemetry_enabled ? "Enabled" : "Disabled");
    if (config.gps.telemetry_enabled) {
        printf("    Host: %s\n", config.gps.telemetry_host);
        printf("    Port: %s\n", config.gps.telemetry_port);
    }
}

int main(int argc, char* argv[]) {
    printf("This is BCP on Saggitarius\n");
    printf("==========================\n");

    FILE* main_log;  // main log file
    FILE* cmd_log;   // command log

    if (argc < 2) {
        printf("Usage: %s <config_file>\n", argv[0]);
        return 1;
    }

    // Get config file from command line and read it into the struct
    read_in_config(argv[1]);
    printf("Reading config parameters from: %s\n", argv[1]);
    print_config();
    
    printf("Starting main log\n");
    main_log = fopen(config.main.logpath, "w");

    if (main_log == NULL) {
        printf("Error opening logfile %s: %s\n", config.main.logpath, strerror(errno));
        return 1;
    }

    write_to_log(main_log, "main_Sag.c", "main", "Started logfile");

    cmd_log = fopen(config.main.cmdlog, "w");
    printf("Starting command log\n");
    write_to_log(main_log, "main_Sag.c", "main", "Starting command log");

    if (cmd_log == NULL) {
        printf("Error opening command log %s: %s\n", config.main.cmdlog, strerror(errno));
        write_to_log(main_log, "main_Sag.c", "main", "Error opening command log");
        fclose(main_log);
        return 1;
    }

    // Initialize and start GPS if enabled
    if (config.gps.enabled) {
        gps_config_t gps_config;
        strncpy(gps_config.port, config.gps.port, sizeof(gps_config.port) - 1);
        gps_config.port[sizeof(gps_config.port) - 1] = '\0';
        gps_config.baud_rate = config.gps.baud_rate;
        strncpy(gps_config.data_path, config.gps.data_save_path, sizeof(gps_config.data_path) - 1);
        gps_config.data_path[sizeof(gps_config.data_path) - 1] = '\0';
        gps_config.file_rotation_interval = config.gps.file_rotation_interval;
        
        // Set telemetry configuration
        gps_config.telemetry_enabled = config.gps.telemetry_enabled;
        strncpy(gps_config.telemetry_host, config.gps.telemetry_host, sizeof(gps_config.telemetry_host) - 1);
        gps_config.telemetry_host[sizeof(gps_config.telemetry_host) - 1] = '\0';
        strncpy(gps_config.telemetry_port, config.gps.telemetry_port, sizeof(gps_config.telemetry_port) - 1);
        gps_config.telemetry_port[sizeof(gps_config.telemetry_port) - 1] = '\0';

        int gps_init_result = gps_init(&gps_config);
        if (gps_init_result == 0) {
            write_to_log(main_log, "main_Sag.c", "main", "GPS initialized");
            
            // Automatically start GPS logging
            if (gps_start_logging()) {
                printf("GPS logging started automatically.\n");
                write_to_log(main_log, "main_Sag.c", "main", "GPS logging started automatically");
            } else {
                printf("Failed to start GPS logging automatically.\n");
                write_to_log(main_log, "main_Sag.c", "main", "Failed to start GPS logging automatically");
            }
        } else {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Failed to initialize GPS. Error code: %d", gps_init_result);
            write_to_log(main_log, "main_Sag.c", "main", error_msg);
        }
    } else {
        write_to_log(main_log, "main_Sag.c", "main", "GPS disabled in configuration");
    }

    // Start the command prompt
    cmdprompt(cmd_log, config.main.logpath, config.rfsoc.ip_address, config.rfsoc.mode, 
              config.rfsoc.data_save_interval, config.rfsoc.data_save_path);

    // Cleanup
    if (config.gps.enabled && gps_is_logging()) {
        gps_stop_logging();
        write_to_log(main_log, "main_Sag.c", "main", "GPS logging stopped during cleanup");
    }

    fclose(cmd_log);
    fclose(main_log);
    return 0;
}
