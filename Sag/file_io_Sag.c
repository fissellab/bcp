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

    // Read GPS telemetry configuration
    config_lookup_int(&cfg, "gps.telemetry_enabled", &config.gps.telemetry_enabled);

    if (config_lookup_string(&cfg, "gps.telemetry_host", &tmpstr)) {
        strncpy(config.gps.telemetry_host, tmpstr, sizeof(config.gps.telemetry_host) - 1);
        config.gps.telemetry_host[sizeof(config.gps.telemetry_host) - 1] = '\0';
    }

    if (config_lookup_string(&cfg, "gps.telemetry_port", &tmpstr)) {
        strncpy(config.gps.telemetry_port, tmpstr, sizeof(config.gps.telemetry_port) - 1);
        config.gps.telemetry_port[sizeof(config.gps.telemetry_port) - 1] = '\0';
    }

    config_destroy(&cfg);
}
