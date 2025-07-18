#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libconfig.h>
#include "file_io_Sag.h"

conf_params_t config;

void write_to_log(FILE* logfile, const char* file, const char* function, const char* message) {
    time_t now;
    time(&now);
    char* date = ctime(&now);
    date[strlen(date) - 1] = '\0';
    fprintf(logfile, "%s : %s : %s : %s\n", date, file, function, message);
    fflush(logfile);
}

void read_in_config(const char* filepath) {
    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, filepath)) {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        exit(1);
    }

    const char* tmpstr;
    // Read main section
    if (config_lookup_string(&cfg, "main.logpath", &tmpstr)) {
        strncpy(config.main.logpath, tmpstr, sizeof(config.main.logpath) - 1);
        config.main.logpath[sizeof(config.main.logpath) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "main.cmdlog", &tmpstr)) {
        strncpy(config.main.cmdlog, tmpstr, sizeof(config.main.cmdlog) - 1);
        config.main.cmdlog[sizeof(config.main.cmdlog) - 1] = '\0';
    }

    // Read rfsoc_spectrometer section
    config_lookup_int(&cfg, "rfsoc_spectrometer.enabled", &config.rfsoc.enabled);

    if (config_lookup_string(&cfg, "rfsoc_spectrometer.ip_address", &tmpstr)) {
        strncpy(config.rfsoc.ip_address, tmpstr, sizeof(config.rfsoc.ip_address) - 1);
        config.rfsoc.ip_address[sizeof(config.rfsoc.ip_address) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "rfsoc_spectrometer.mode", &tmpstr)) {
        strncpy(config.rfsoc.mode, tmpstr, sizeof(config.rfsoc.mode) - 1);
        config.rfsoc.mode[sizeof(config.rfsoc.mode) - 1] = '\0';
    }

    config_lookup_int(&cfg, "rfsoc_spectrometer.data_save_interval", &config.rfsoc.data_save_interval);

    if (config_lookup_string(&cfg, "rfsoc_spectrometer.data_save_path", &tmpstr)) {
        strncpy(config.rfsoc.data_save_path, tmpstr, sizeof(config.rfsoc.data_save_path) - 1);
        config.rfsoc.data_save_path[sizeof(config.rfsoc.data_save_path) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "rfsoc_spectrometer.fpga_bitstream", &tmpstr)) {
        strncpy(config.rfsoc.fpga_bitstream, tmpstr, sizeof(config.rfsoc.fpga_bitstream) - 1);
        config.rfsoc.fpga_bitstream[sizeof(config.rfsoc.fpga_bitstream) - 1] = '\0';
    }

    config_lookup_int(&cfg, "rfsoc_spectrometer.adc_channel", &config.rfsoc.adc_channel);
    config_lookup_int(&cfg, "rfsoc_spectrometer.accumulation_length", &config.rfsoc.accumulation_length);
    config_lookup_int(&cfg, "rfsoc_spectrometer.num_channels", &config.rfsoc.num_channels);
    config_lookup_int(&cfg, "rfsoc_spectrometer.num_fft_points", &config.rfsoc.num_fft_points);
    
    // Read RFSoC power control settings
    config_lookup_int(&cfg, "rfsoc_spectrometer.pbob_id", &config.rfsoc.pbob_id);
    config_lookup_int(&cfg, "rfsoc_spectrometer.relay_id", &config.rfsoc.relay_id);

    // Read gps section
    config_lookup_int(&cfg, "gps.enabled", &config.gps.enabled);

    if (config_lookup_string(&cfg, "gps.port", &tmpstr)) {
        strncpy(config.gps.port, tmpstr, sizeof(config.gps.port) - 1);
        config.gps.port[sizeof(config.gps.port) - 1] = '\0';
    }

    config_lookup_int(&cfg, "gps.baud_rate", &config.gps.baud_rate);

    if (config_lookup_string(&cfg, "gps.data_save_path", &tmpstr)) {
        strncpy(config.gps.data_save_path, tmpstr, sizeof(config.gps.data_save_path) - 1);
        config.gps.data_save_path[sizeof(config.gps.data_save_path) - 1] = '\0';
    }

    config_lookup_int(&cfg, "gps.file_rotation_interval", &config.gps.file_rotation_interval);

    // Read UDP server settings
    config_lookup_int(&cfg, "gps.udp_server_enabled", &config.gps.udp_server_enabled);
    config_lookup_int(&cfg, "gps.udp_server_port", &config.gps.udp_server_port);
    
    // Read UDP client IPs array
    config_setting_t *client_ips_array = config_lookup(&cfg, "gps.udp_client_ips");
    if (client_ips_array != NULL && config_setting_is_array(client_ips_array)) {
        int count = config_setting_length(client_ips_array);
        if (count > MAX_UDP_CLIENTS) {
            count = MAX_UDP_CLIENTS;  // Limit to max supported clients
        }
        config.gps.udp_client_count = count;
        
        for (int i = 0; i < count; i++) {
            const char *ip = config_setting_get_string_elem(client_ips_array, i);
            if (ip != NULL) {
                strncpy(config.gps.udp_client_ips[i], ip, 15);
                config.gps.udp_client_ips[i][15] = '\0';
            }
        }
    } else {
        config.gps.udp_client_count = 0;
    }
    
    config_lookup_int(&cfg, "gps.udp_buffer_size", &config.gps.udp_buffer_size);
    
    // Read GPS power control settings
    config_lookup_int(&cfg, "gps.pbob_id", &config.gps.pbob_id);
    config_lookup_int(&cfg, "gps.relay_id", &config.gps.relay_id);

    // Read spectrometer_server section
    config_lookup_int(&cfg, "spectrometer_server.enabled", &config.spectrometer_server.enabled);
    config_lookup_int(&cfg, "spectrometer_server.udp_server_port", &config.spectrometer_server.udp_server_port);
    config_lookup_int(&cfg, "spectrometer_server.udp_buffer_size", &config.spectrometer_server.udp_buffer_size);
    config_lookup_int(&cfg, "spectrometer_server.max_request_rate", &config.spectrometer_server.max_request_rate);
    
    // Read spectrometer UDP client IPs array
    config_setting_t *spec_client_ips_array = config_lookup(&cfg, "spectrometer_server.udp_client_ips");
    if (spec_client_ips_array != NULL && config_setting_is_array(spec_client_ips_array)) {
        int count = config_setting_length(spec_client_ips_array);
        if (count > MAX_UDP_CLIENTS) {
            count = MAX_UDP_CLIENTS;  // Limit to max supported clients
        }
        config.spectrometer_server.udp_client_count = count;
        
        for (int i = 0; i < count; i++) {
            const char *ip = config_setting_get_string_elem(spec_client_ips_array, i);
            if (ip != NULL) {
                strncpy(config.spectrometer_server.udp_client_ips[i], ip, 15);
                config.spectrometer_server.udp_client_ips[i][15] = '\0';
            }
        }
    } else {
        config.spectrometer_server.udp_client_count = 0;
    }
    
    // Read water maser filtering parameters
    config_lookup_float(&cfg, "spectrometer_server.water_maser_freq", &config.spectrometer_server.water_maser_freq);
    config_lookup_float(&cfg, "spectrometer_server.zoom_window_width", &config.spectrometer_server.zoom_window_width);
    config_lookup_float(&cfg, "spectrometer_server.if_lower", &config.spectrometer_server.if_lower);
    config_lookup_float(&cfg, "spectrometer_server.if_upper", &config.spectrometer_server.if_upper);

    // Read telemetry_server section
    config_lookup_int(&cfg, "telemetry_server.enabled", &config.telemetry_server.enabled);
    config_lookup_int(&cfg, "telemetry_server.port", &config.telemetry_server.port);
    config_lookup_int(&cfg, "telemetry_server.timeout", &config.telemetry_server.timeout);
    config_lookup_int(&cfg, "telemetry_server.udp_buffer_size", &config.telemetry_server.udp_buffer_size);
    
    if (config_lookup_string(&cfg, "telemetry_server.ip", &tmpstr)) {
        strncpy(config.telemetry_server.ip, tmpstr, sizeof(config.telemetry_server.ip) - 1);
        config.telemetry_server.ip[sizeof(config.telemetry_server.ip) - 1] = '\0';
    }
    
    // Read telemetry UDP client IPs array
    config_setting_t *tel_client_ips_array = config_lookup(&cfg, "telemetry_server.udp_client_ips");
    if (tel_client_ips_array != NULL && config_setting_is_array(tel_client_ips_array)) {
        int count = config_setting_length(tel_client_ips_array);
        if (count > MAX_UDP_CLIENTS) {
            count = MAX_UDP_CLIENTS;  // Limit to max supported clients
        }
        config.telemetry_server.udp_client_count = count;
        
        for (int i = 0; i < count; i++) {
            const char *ip = config_setting_get_string_elem(tel_client_ips_array, i);
            if (ip != NULL) {
                strncpy(config.telemetry_server.udp_client_ips[i], ip, 15);
                config.telemetry_server.udp_client_ips[i][15] = '\0';
            }
        }
    } else {
        config.telemetry_server.udp_client_count = 0;
    }

    // Read pbob_client section
    config_lookup_int(&cfg, "pbob_client.enabled", &config.pbob_client.enabled);
    config_lookup_int(&cfg, "pbob_client.port", &config.pbob_client.port);
    config_lookup_int(&cfg, "pbob_client.timeout", &config.pbob_client.timeout);
    
    if (config_lookup_string(&cfg, "pbob_client.ip", &tmpstr)) {
        strncpy(config.pbob_client.ip, tmpstr, sizeof(config.pbob_client.ip) - 1);
        config.pbob_client.ip[sizeof(config.pbob_client.ip) - 1] = '\0';
    }

    // Read vlbi section
    config_lookup_int(&cfg, "vlbi.enabled", &config.vlbi.enabled);
    config_lookup_int(&cfg, "vlbi.aquila_port", &config.vlbi.aquila_port);
    config_lookup_int(&cfg, "vlbi.timeout", &config.vlbi.timeout);
    config_lookup_int(&cfg, "vlbi.ping_timeout", &config.vlbi.ping_timeout);
    config_lookup_int(&cfg, "vlbi.status_check_interval", &config.vlbi.status_check_interval);
    
    if (config_lookup_string(&cfg, "vlbi.aquila_ip", &tmpstr)) {
        strncpy(config.vlbi.aquila_ip, tmpstr, sizeof(config.vlbi.aquila_ip) - 1);
        config.vlbi.aquila_ip[sizeof(config.vlbi.aquila_ip) - 1] = '\0';
    }

    // Read rfsoc_daemon section
    config_lookup_int(&cfg, "rfsoc_daemon.enabled", &config.rfsoc_daemon.enabled);
    config_lookup_int(&cfg, "rfsoc_daemon.rfsoc_port", &config.rfsoc_daemon.rfsoc_port);
    config_lookup_int(&cfg, "rfsoc_daemon.timeout", &config.rfsoc_daemon.timeout);
    
    if (config_lookup_string(&cfg, "rfsoc_daemon.rfsoc_ip", &tmpstr)) {
        strncpy(config.rfsoc_daemon.rfsoc_ip, tmpstr, sizeof(config.rfsoc_daemon.rfsoc_ip) - 1);
        config.rfsoc_daemon.rfsoc_ip[sizeof(config.rfsoc_daemon.rfsoc_ip) - 1] = '\0';
    }

    // Read ticc section
    config_lookup_int(&cfg, "ticc.enabled", &config.ticc.enabled);
    config_lookup_int(&cfg, "ticc.baud_rate", &config.ticc.baud_rate);
    config_lookup_int(&cfg, "ticc.file_rotation_interval", &config.ticc.file_rotation_interval);

    if (config_lookup_string(&cfg, "ticc.port", &tmpstr)) {
        strncpy(config.ticc.port, tmpstr, sizeof(config.ticc.port) - 1);
        config.ticc.port[sizeof(config.ticc.port) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "ticc.data_save_path", &tmpstr)) {
        strncpy(config.ticc.data_save_path, tmpstr, sizeof(config.ticc.data_save_path) - 1);
        config.ticc.data_save_path[sizeof(config.ticc.data_save_path) - 1] = '\0';
    }
    
    // Read TICC power control settings
    config_lookup_int(&cfg, "ticc.pbob_id", &config.ticc.pbob_id);
    config_lookup_int(&cfg, "ticc.relay_id", &config.ticc.relay_id);

    // Read pr59 section
    config_lookup_int(&cfg, "pr59.enabled", &config.pr59.enabled);

    if (config_lookup_string(&cfg, "pr59.port", &tmpstr)) {
        strncpy(config.pr59.port, tmpstr, sizeof(config.pr59.port) - 1);
        config.pr59.port[sizeof(config.pr59.port) - 1] = '\0';
    }

    config_lookup_float(&cfg, "pr59.setpoint_temp", &config.pr59.setpoint_temp);
    config_lookup_float(&cfg, "pr59.kp", &config.pr59.kp);
    config_lookup_float(&cfg, "pr59.ki", &config.pr59.ki);
    config_lookup_float(&cfg, "pr59.kd", &config.pr59.kd);
    config_lookup_float(&cfg, "pr59.deadband", &config.pr59.deadband);

    if (config_lookup_string(&cfg, "pr59.data_save_path", &tmpstr)) {
        strncpy(config.pr59.data_save_path, tmpstr, sizeof(config.pr59.data_save_path) - 1);
        config.pr59.data_save_path[sizeof(config.pr59.data_save_path) - 1] = '\0';
    }

    config_destroy(&cfg);
}
