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
#include "spectrometer_server.h"
#include "telemetry_server.h"
#include "pbob_client.h"
#include "vlbi_client.h"
#include "rfsoc_client.h"
#include "ticc_client.h"

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
    printf("  Power Control: PBOB %d, Relay %d\n", config.rfsoc.pbob_id, config.rfsoc.relay_id);
    printf("\nGPS settings:\n");
    printf("  Enabled: %s\n", config.gps.enabled ? "Yes" : "No");
    printf("  Port: %s\n", config.gps.port);
    printf("  Baud Rate: %d\n", config.gps.baud_rate);
    printf("  Data Save Path: %s\n", config.gps.data_save_path);
    printf("  File Rotation Interval: %d seconds\n", config.gps.file_rotation_interval);
    printf("  UDP Server Enabled: %s\n", config.gps.udp_server_enabled ? "Yes" : "No");
    printf("  UDP Server Port: %d\n", config.gps.udp_server_port);
    printf("  UDP Buffer Size: %d\n", config.gps.udp_buffer_size);
    printf("  Power Control: PBOB %d, Relay %d\n", config.gps.pbob_id, config.gps.relay_id);
    printf("\nSpectrometer Server settings:\n");
    printf("  Enabled: %s\n", config.spectrometer_server.enabled ? "Yes" : "No");
    printf("  UDP Server Port: %d\n", config.spectrometer_server.udp_server_port);
    printf("  UDP Buffer Size: %d\n", config.spectrometer_server.udp_buffer_size);
    printf("  Max Request Rate: %d req/sec\n", config.spectrometer_server.max_request_rate);
    printf("  Water Maser Freq: %.3f GHz\n", config.spectrometer_server.water_maser_freq);
    printf("  Zoom Window Width: %.3f GHz\n", config.spectrometer_server.zoom_window_width);
    printf("  IF Range: %.5f - %.5f GHz\n", config.spectrometer_server.if_lower, config.spectrometer_server.if_upper);
    printf("\nTelemetry Server settings:\n");
    printf("  Enabled: %s\n", config.telemetry_server.enabled ? "Yes" : "No");
    printf("  Server IP: %s\n", config.telemetry_server.ip);
    printf("  Server Port: %d\n", config.telemetry_server.port);
    printf("  Timeout: %d us\n", config.telemetry_server.timeout);
    printf("  UDP Buffer Size: %d\n", config.telemetry_server.udp_buffer_size);
    printf("  Authorized Clients: %d\n", config.telemetry_server.udp_client_count);
    printf("\nPBoB Client settings:\n");
    printf("  Enabled: %s\n", config.pbob_client.enabled ? "Yes" : "No");
    printf("  Server IP: %s\n", config.pbob_client.ip);
    printf("  Server Port: %d\n", config.pbob_client.port);
    printf("  Timeout: %d ms\n", config.pbob_client.timeout);
    printf("\nVLBI Client settings:\n");
    printf("  Enabled: %s\n", config.vlbi.enabled ? "Yes" : "No");
    printf("  Aquila IP: %s\n", config.vlbi.aquila_ip);
    printf("  Aquila Port: %d\n", config.vlbi.aquila_port);
    printf("  Timeout: %d ms\n", config.vlbi.timeout);
    printf("  Ping Timeout: %d s\n", config.vlbi.ping_timeout);
    printf("  Status Check Interval: %d s\n", config.vlbi.status_check_interval);
    printf("\nRFSoC Daemon Client settings:\n");
    printf("  Enabled: %s\n", config.rfsoc_daemon.enabled ? "Yes" : "No");
    printf("  RFSoC IP: %s\n", config.rfsoc_daemon.rfsoc_ip);
    printf("  RFSoC Port: %d\n", config.rfsoc_daemon.rfsoc_port);
    printf("  Timeout: %d ms\n", config.rfsoc_daemon.timeout);
    printf("\nTICC Client settings:\n");
    printf("  Enabled: %s\n", config.ticc.enabled ? "Yes" : "No");
    printf("  Serial Port: %s\n", config.ticc.port);
    printf("  Baud Rate: %d\n", config.ticc.baud_rate);
    printf("  Data Save Path: %s\n", config.ticc.data_save_path);
    printf("  File Rotation Interval: %d seconds\n", config.ticc.file_rotation_interval);
    printf("  Power Control: PBOB %d, Relay %d\n", config.ticc.pbob_id, config.ticc.relay_id);
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

    // Initialize GPS configuration if enabled (but don't start it automatically)
    if (config.gps.enabled) {
        gps_config_t gps_config;
        strncpy(gps_config.port, config.gps.port, sizeof(gps_config.port) - 1);
        gps_config.port[sizeof(gps_config.port) - 1] = '\0';
        gps_config.baud_rate = config.gps.baud_rate;
        strncpy(gps_config.data_path, config.gps.data_save_path, sizeof(gps_config.data_path) - 1);
        gps_config.data_path[sizeof(gps_config.data_path) - 1] = '\0';
        gps_config.file_rotation_interval = config.gps.file_rotation_interval;
        
        // Set UDP server configuration
        gps_config.udp_server_enabled = config.gps.udp_server_enabled;
        gps_config.udp_server_port = config.gps.udp_server_port;
        gps_config.udp_client_count = config.gps.udp_client_count;
        for (int i = 0; i < config.gps.udp_client_count; i++) {
            strncpy(gps_config.udp_client_ips[i], config.gps.udp_client_ips[i], sizeof(gps_config.udp_client_ips[i]) - 1);
            gps_config.udp_client_ips[i][sizeof(gps_config.udp_client_ips[i]) - 1] = '\0';
        }
        gps_config.udp_buffer_size = config.gps.udp_buffer_size;

        int gps_init_result = gps_init(&gps_config);
        if (gps_init_result == 0) {
            printf("GPS initialized (use 'gps_start' to power on and begin logging).\n");
            write_to_log(main_log, "main_Sag.c", "main", "GPS initialized - awaiting gps_start command");
        } else {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Failed to initialize GPS. Error code: %d", gps_init_result);
            write_to_log(main_log, "main_Sag.c", "main", error_msg);
        }
    } else {
        write_to_log(main_log, "main_Sag.c", "main", "GPS disabled in configuration");
    }

    // Initialize and start Spectrometer Server if enabled
    if (config.spectrometer_server.enabled) {
        spec_server_config_t spec_config;
        spec_config.udp_server_enabled = 1;  // Always enabled if spectrometer_server is enabled
        spec_config.udp_server_port = config.spectrometer_server.udp_server_port;
        spec_config.udp_client_count = config.spectrometer_server.udp_client_count;
        for (int i = 0; i < config.spectrometer_server.udp_client_count; i++) {
            strncpy(spec_config.udp_client_ips[i], config.spectrometer_server.udp_client_ips[i], 
                    sizeof(spec_config.udp_client_ips[i]) - 1);
            spec_config.udp_client_ips[i][sizeof(spec_config.udp_client_ips[i]) - 1] = '\0';
        }
        spec_config.udp_buffer_size = config.spectrometer_server.udp_buffer_size;
        spec_config.max_request_rate = config.spectrometer_server.max_request_rate;
        
        // Water maser filtering parameters
        spec_config.water_maser_freq = config.spectrometer_server.water_maser_freq;
        spec_config.zoom_window_width = config.spectrometer_server.zoom_window_width;
        spec_config.if_lower = config.spectrometer_server.if_lower;
        spec_config.if_upper = config.spectrometer_server.if_upper;

        int spec_init_result = spec_server_init(&spec_config);
        if (spec_init_result == 0) {
            write_to_log(main_log, "main_Sag.c", "main", "Spectrometer server initialized");
            
            // Start spectrometer UDP server
            if (spec_server_start()) {
                char spec_msg[256];
                snprintf(spec_msg, sizeof(spec_msg), "Spectrometer UDP server started on port %d", 
                        config.spectrometer_server.udp_server_port);
                printf("%s\n", spec_msg);
                write_to_log(main_log, "main_Sag.c", "main", spec_msg);
            } else {
                printf("Failed to start Spectrometer UDP server.\n");
                write_to_log(main_log, "main_Sag.c", "main", "Failed to start Spectrometer UDP server");
            }
        } else {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Failed to initialize Spectrometer server. Error code: %d", spec_init_result);
            write_to_log(main_log, "main_Sag.c", "main", error_msg);
        }
    } else {
        write_to_log(main_log, "main_Sag.c", "main", "Spectrometer server disabled in configuration");
    }

    // Initialize PBoB client if enabled
    if (config.pbob_client.enabled) {
        pbob_client_config_t pbob_config;
        pbob_config.enabled = config.pbob_client.enabled;
        strncpy(pbob_config.ip, config.pbob_client.ip, sizeof(pbob_config.ip) - 1);
        pbob_config.ip[sizeof(pbob_config.ip) - 1] = '\0';
        pbob_config.port = config.pbob_client.port;
        pbob_config.timeout = config.pbob_client.timeout;

        int pbob_init_result = pbob_client_init(&pbob_config);
        if (pbob_init_result == 0) {
            printf("PBoB client initialized successfully (Server: %s:%d)\n", 
                   config.pbob_client.ip, config.pbob_client.port);
            write_to_log(main_log, "main_Sag.c", "main", "PBoB client initialized successfully");
        } else {
            printf("Failed to initialize PBoB client\n");
            write_to_log(main_log, "main_Sag.c", "main", "Failed to initialize PBoB client");
        }
    } else {
        write_to_log(main_log, "main_Sag.c", "main", "PBoB client disabled in configuration");
    }

    // Initialize VLBI client
    if (config.vlbi.enabled) {
        vlbi_client_config_t vlbi_config;
        vlbi_config.enabled = config.vlbi.enabled;
        strncpy(vlbi_config.aquila_ip, config.vlbi.aquila_ip, sizeof(vlbi_config.aquila_ip) - 1);
        vlbi_config.aquila_ip[sizeof(vlbi_config.aquila_ip) - 1] = '\0';
        vlbi_config.aquila_port = config.vlbi.aquila_port;
        vlbi_config.timeout = config.vlbi.timeout;
        vlbi_config.ping_timeout = config.vlbi.ping_timeout;
        vlbi_config.status_check_interval = config.vlbi.status_check_interval;

        int vlbi_init_result = vlbi_client_init(&vlbi_config);
        if (vlbi_init_result == 0) {
            printf("VLBI client initialized successfully (Aquila: %s:%d)\n", 
                   config.vlbi.aquila_ip, config.vlbi.aquila_port);
            write_to_log(main_log, "main_Sag.c", "main", "VLBI client initialized successfully");
        } else {
            printf("Failed to initialize VLBI client\n");
            write_to_log(main_log, "main_Sag.c", "main", "Failed to initialize VLBI client");
        }
    } else {
        write_to_log(main_log, "main_Sag.c", "main", "VLBI client disabled in configuration");
    }

    // Initialize RFSoC daemon client
    if (config.rfsoc_daemon.enabled) {
        rfsoc_client_config_t rfsoc_config;
        rfsoc_config.enabled = config.rfsoc_daemon.enabled;
        strncpy(rfsoc_config.rfsoc_ip, config.rfsoc_daemon.rfsoc_ip, sizeof(rfsoc_config.rfsoc_ip) - 1);
        rfsoc_config.rfsoc_ip[sizeof(rfsoc_config.rfsoc_ip) - 1] = '\0';
        rfsoc_config.rfsoc_port = config.rfsoc_daemon.rfsoc_port;
        rfsoc_config.timeout = config.rfsoc_daemon.timeout;

        int rfsoc_init_result = rfsoc_client_init(&rfsoc_config);
        if (rfsoc_init_result == 0) {
            printf("RFSoC daemon client initialized successfully (RFSoC: %s:%d)\n", 
                   config.rfsoc_daemon.rfsoc_ip, config.rfsoc_daemon.rfsoc_port);
            write_to_log(main_log, "main_Sag.c", "main", "RFSoC daemon client initialized successfully");
        } else {
            printf("Failed to initialize RFSoC daemon client\n");
            write_to_log(main_log, "main_Sag.c", "main", "Failed to initialize RFSoC daemon client");
        }
    } else {
        write_to_log(main_log, "main_Sag.c", "main", "RFSoC daemon client disabled in configuration");
    }

    // Initialize TICC client if enabled
    if (config.ticc.enabled) {
        ticc_client_config_t ticc_config;
        ticc_config.enabled = config.ticc.enabled;
        strncpy(ticc_config.port, config.ticc.port, sizeof(ticc_config.port) - 1);
        ticc_config.port[sizeof(ticc_config.port) - 1] = '\0';
        ticc_config.baud_rate = config.ticc.baud_rate;
        strncpy(ticc_config.data_save_path, config.ticc.data_save_path, sizeof(ticc_config.data_save_path) - 1);
        ticc_config.data_save_path[sizeof(ticc_config.data_save_path) - 1] = '\0';
        ticc_config.file_rotation_interval = config.ticc.file_rotation_interval;
        ticc_config.pbob_id = config.ticc.pbob_id;
        ticc_config.relay_id = config.ticc.relay_id;

        int ticc_init_result = ticc_client_init(&ticc_config);
        if (ticc_init_result == 0) {
            printf("TICC client initialized successfully (Port: %s, Baud: %d)\n", 
                   config.ticc.port, config.ticc.baud_rate);
            write_to_log(main_log, "main_Sag.c", "main", "TICC client initialized successfully");
        } else {
            printf("Failed to initialize TICC client\n");
            write_to_log(main_log, "main_Sag.c", "main", "Failed to initialize TICC client");
        }
    } else {
        write_to_log(main_log, "main_Sag.c", "main", "TICC client disabled in configuration");
    }

    // Initialize and start Telemetry Server if enabled
    if (config.telemetry_server.enabled) {
        telemetry_server_config_t tel_config;
        tel_config.enabled = config.telemetry_server.enabled;
        strncpy(tel_config.ip, config.telemetry_server.ip, sizeof(tel_config.ip) - 1);
        tel_config.ip[sizeof(tel_config.ip) - 1] = '\0';
        tel_config.port = config.telemetry_server.port;
        tel_config.timeout = config.telemetry_server.timeout;
        tel_config.udp_buffer_size = config.telemetry_server.udp_buffer_size;
        tel_config.udp_client_count = config.telemetry_server.udp_client_count;
        for (int i = 0; i < config.telemetry_server.udp_client_count; i++) {
            strncpy(tel_config.udp_client_ips[i], config.telemetry_server.udp_client_ips[i], 
                    sizeof(tel_config.udp_client_ips[i]) - 1);
            tel_config.udp_client_ips[i][sizeof(tel_config.udp_client_ips[i]) - 1] = '\0';
        }

        int tel_init_result = telemetry_server_init(&tel_config);
        if (tel_init_result == 0) {
            write_to_log(main_log, "main_Sag.c", "main", "Telemetry server initialized");
            
            // Start telemetry server
            if (telemetry_server_start()) {
                char tel_msg[256];
                snprintf(tel_msg, sizeof(tel_msg), "Telemetry server started on %s:%d", 
                        config.telemetry_server.ip, config.telemetry_server.port);
                printf("%s\n", tel_msg);
                write_to_log(main_log, "main_Sag.c", "main", tel_msg);
            } else {
                printf("Failed to start Telemetry server.\n");
                write_to_log(main_log, "main_Sag.c", "main", "Failed to start Telemetry server");
            }
        } else {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Failed to initialize Telemetry server. Error code: %d", tel_init_result);
            write_to_log(main_log, "main_Sag.c", "main", error_msg);
        }
    } else {
        write_to_log(main_log, "main_Sag.c", "main", "Telemetry server disabled in configuration");
    }

    // Start the command prompt
    cmdprompt(cmd_log, config.main.logpath, config.rfsoc.ip_address, config.rfsoc.mode, 
              config.rfsoc.data_save_interval, config.rfsoc.data_save_path);

    // Cleanup
    if (config.spectrometer_server.enabled) {
        // Stop Spectrometer UDP server if running
        if (spec_server_is_running()) {
            spec_server_stop();
            printf("Spectrometer UDP server stopped during cleanup\n");
            write_to_log(main_log, "main_Sag.c", "main", "Spectrometer UDP server stopped during cleanup");
        }
    }
    
    if (config.gps.enabled) {
        // Stop GPS UDP server if running
        if (gps_is_udp_server_running()) {
            gps_stop_udp_server();
            printf("GPS UDP server stopped during cleanup\n");
            write_to_log(main_log, "main_Sag.c", "main", "GPS UDP server stopped during cleanup");
        }
        
        // Stop GPS logging if running
        if (gps_is_logging()) {
            gps_stop_logging();
            write_to_log(main_log, "main_Sag.c", "main", "GPS logging stopped during cleanup");
        }
    }

    // Cleanup PBoB client
    if (config.pbob_client.enabled) {
        pbob_client_cleanup();
        write_to_log(main_log, "main_Sag.c", "main", "PBoB client cleaned up");
    }

    // Cleanup VLBI client
    if (config.vlbi.enabled) {
        vlbi_client_cleanup();
        write_to_log(main_log, "main_Sag.c", "main", "VLBI client cleaned up");
    }

    // Cleanup RFSoC daemon client
    if (config.rfsoc_daemon.enabled) {
        rfsoc_client_cleanup();
        write_to_log(main_log, "main_Sag.c", "main", "RFSoC daemon client cleaned up");
    }

    // Cleanup TICC client
    if (config.ticc.enabled) {
        ticc_client_cleanup();
        write_to_log(main_log, "main_Sag.c", "main", "TICC client cleaned up");
    }

    // Cleanup Telemetry server
    if (config.telemetry_server.enabled) {
        // Stop telemetry server if running
        if (telemetry_server_is_running()) {
            telemetry_server_stop();
            printf("Telemetry server stopped during cleanup\n");
            write_to_log(main_log, "main_Sag.c", "main", "Telemetry server stopped during cleanup");
        }
        telemetry_server_cleanup();
    }

    fclose(cmd_log);
    fclose(main_log);
    return 0;
}
