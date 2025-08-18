#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
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

// Global variable to store the timestamped log directory path
static char timestamped_log_dir[512] = {0};

char* create_timestamped_log_directory(void) {
    // If already created, return the existing path
    if (timestamped_log_dir[0] != '\0') {
        return timestamped_log_dir;
    }
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // Create timestamp in format YYYY-MM-DD_HH-MM-SS
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", t);
    
    // Create the timestamped directory path
    snprintf(timestamped_log_dir, sizeof(timestamped_log_dir), 
             "/home/mayukh/bcp/Sag/log/%s_session", timestamp);
    
    // Create the directory (and parent log directory if needed)
    mkdir("/home/mayukh/bcp/Sag/log", 0755);
    mkdir(timestamped_log_dir, 0755);
    
    printf("Created timestamped log directory: %s\n", timestamped_log_dir);
    return timestamped_log_dir;
}

void get_timestamped_log_path(const char* filename, char* full_path, size_t path_size) {
    char* log_dir = create_timestamped_log_directory();
    snprintf(full_path, path_size, "%s/%s", log_dir, filename);
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
    double temp_double;
    if (config_lookup_float(&cfg, "spectrometer_server.water_maser_freq", &temp_double)) {
        config.spectrometer_server.water_maser_freq = temp_double;
    }
    if (config_lookup_float(&cfg, "spectrometer_server.zoom_window_width", &temp_double)) {
        config.spectrometer_server.zoom_window_width = temp_double;
    }
    if (config_lookup_float(&cfg, "spectrometer_server.if_lower", &temp_double)) {
        config.spectrometer_server.if_lower = temp_double;
    }
    if (config_lookup_float(&cfg, "spectrometer_server.if_upper", &temp_double)) {
        config.spectrometer_server.if_upper = temp_double;
    }

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

    // Read heaters section
    config_lookup_int(&cfg, "heaters.enabled", &config.heaters.enabled);
    config_lookup_int(&cfg, "heaters.pbob_id", &config.heaters.pbob_id);
    config_lookup_int(&cfg, "heaters.relay_id", &config.heaters.relay_id);
    config_lookup_int(&cfg, "heaters.port", &config.heaters.port);
    config_lookup_int(&cfg, "heaters.current_cap", &config.heaters.current_cap);
    config_lookup_int(&cfg, "heaters.timeout", &config.heaters.timeout);

    // Read temperature thresholds
    config_lookup_float(&cfg, "heaters.temp_low_starcam", &config.heaters.temp_low_starcam);
    config_lookup_float(&cfg, "heaters.temp_high_starcam", &config.heaters.temp_high_starcam);
    config_lookup_float(&cfg, "heaters.temp_low_motor", &config.heaters.temp_low_motor);
    config_lookup_float(&cfg, "heaters.temp_high_motor", &config.heaters.temp_high_motor);
    config_lookup_float(&cfg, "heaters.temp_low_ethernet", &config.heaters.temp_low_ethernet);
    config_lookup_float(&cfg, "heaters.temp_high_ethernet", &config.heaters.temp_high_ethernet);
    config_lookup_float(&cfg, "heaters.temp_low_lockpin", &config.heaters.temp_low_lockpin);
    config_lookup_float(&cfg, "heaters.temp_high_lockpin", &config.heaters.temp_high_lockpin);

    if (config_lookup_string(&cfg, "heaters.logfile", &tmpstr)) {
        strncpy(config.heaters.logfile, tmpstr, sizeof(config.heaters.logfile) - 1);
        config.heaters.logfile[sizeof(config.heaters.logfile) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "heaters.heater_ip", &tmpstr)) {
        strncpy(config.heaters.heater_ip, tmpstr, sizeof(config.heaters.heater_ip) - 1);
        config.heaters.heater_ip[sizeof(config.heaters.heater_ip) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "heaters.server_ip", &tmpstr)) {
        strncpy(config.heaters.server_ip, tmpstr, sizeof(config.heaters.server_ip) - 1);
        config.heaters.server_ip[sizeof(config.heaters.server_ip) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "heaters.workdir", &tmpstr)) {
        strncpy(config.heaters.workdir, tmpstr, sizeof(config.heaters.workdir) - 1);
        config.heaters.workdir[sizeof(config.heaters.workdir) - 1] = '\0';
    }

    // Read position_sensors section
    config_lookup_int(&cfg, "position_sensors.enabled", &config.position_sensors.enabled);

    if (config_lookup_string(&cfg, "position_sensors.pi_ip", &tmpstr)) {
        strncpy(config.position_sensors.pi_ip, tmpstr, sizeof(config.position_sensors.pi_ip) - 1);
        config.position_sensors.pi_ip[sizeof(config.position_sensors.pi_ip) - 1] = '\0';
    }

    config_lookup_int(&cfg, "position_sensors.pi_port", &config.position_sensors.pi_port);
    config_lookup_int(&cfg, "position_sensors.connection_timeout", &config.position_sensors.connection_timeout);
    config_lookup_int(&cfg, "position_sensors.retry_attempts", &config.position_sensors.retry_attempts);
    config_lookup_int(&cfg, "position_sensors.telemetry_rate_hz", &config.position_sensors.telemetry_rate_hz);

    if (config_lookup_string(&cfg, "position_sensors.data_save_path", &tmpstr)) {
        strncpy(config.position_sensors.data_save_path, tmpstr, sizeof(config.position_sensors.data_save_path) - 1);
        config.position_sensors.data_save_path[sizeof(config.position_sensors.data_save_path) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "position_sensors.script_path", &tmpstr)) {
        strncpy(config.position_sensors.script_path, tmpstr, sizeof(config.position_sensors.script_path) - 1);
        config.position_sensors.script_path[sizeof(config.position_sensors.script_path) - 1] = '\0';
    }
    
    // Read position sensors power control settings
    config_lookup_int(&cfg, "position_sensors.pbob_id", &config.position_sensors.pbob_id);
    config_lookup_int(&cfg, "position_sensors.relay_id", &config.position_sensors.relay_id);

    // Read pr59 section
    config_lookup_int(&cfg, "pr59.enabled", &config.pr59.enabled);

    if (config_lookup_string(&cfg, "pr59.port", &tmpstr)) {
        strncpy(config.pr59.port, tmpstr, sizeof(config.pr59.port) - 1);
        config.pr59.port[sizeof(config.pr59.port) - 1] = '\0';
    }

    if (config_lookup_float(&cfg, "pr59.setpoint_temp", &temp_double)) {
        config.pr59.setpoint_temp = (float)temp_double;
    }
    if (config_lookup_float(&cfg, "pr59.kp", &temp_double)) {
        config.pr59.kp = (float)temp_double;
    }
    if (config_lookup_float(&cfg, "pr59.ki", &temp_double)) {
        config.pr59.ki = (float)temp_double;
    }
    if (config_lookup_float(&cfg, "pr59.kd", &temp_double)) {
        config.pr59.kd = (float)temp_double;
    }
    if (config_lookup_float(&cfg, "pr59.deadband", &temp_double)) {
        config.pr59.deadband = (float)temp_double;
    }

    if (config_lookup_string(&cfg, "pr59.data_save_path", &tmpstr)) {
        strncpy(config.pr59.data_save_path, tmpstr, sizeof(config.pr59.data_save_path) - 1);
        config.pr59.data_save_path[sizeof(config.pr59.data_save_path) - 1] = '\0';
    }

    // Read backend section
    config_lookup_int(&cfg, "backend.enabled", &config.backend.enabled);
    
    // Read backend power control settings
    config_lookup_int(&cfg, "backend.pbob_id", &config.backend.pbob_id);
    config_lookup_int(&cfg, "backend.relay_id", &config.backend.relay_id);

    if(!config_lookup_int(&cfg,"cmd_server.port", &config.cmd_server.port)){
		printf("Missing config.cmd_server.port\n");
		config_destroy(&cfg);
		exit(0);

    }

    if(!config_lookup_int(&cfg,"cmd_server.timeout", &config.cmd_server.timeout)){
                printf("Missing config.cmd_server.timeout\n");
                config_destroy(&cfg);
                exit(0);

    }

    // Read system_monitor section
    config_lookup_int(&cfg, "system_monitor.enabled", &config.system_monitor.enabled);
    config_lookup_int(&cfg, "system_monitor.update_interval_sec", &config.system_monitor.update_interval_sec);

    config_destroy(&cfg);
}
