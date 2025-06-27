#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <libconfig.h>
#include "file_io_Oph.h"

extern struct conf_params config;

void write_to_log(FILE* fptr, char* cfile, char* func, char* msg) {
    time_t time_m;
    time_m = time(NULL);
    fprintf(fptr, "[%ld][%s][%s] %s\n", time_m, cfile, func, msg);
}

void read_in_config(char* filepath) {
    config_t conf;
    const char* tmpstr = NULL;
    int tmpint;
    double tmpfloat;

    config_init(&conf);

    if (!config_read_file(&conf, filepath)) {
        printf("%s: No such file or directory\n", filepath);
        config_destroy(&conf);
        exit(0);
    }

    // Main configuration
    if (!config_lookup_string(&conf, "main.logpath", &tmpstr)) {
        printf("Missing main.logpath in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.main.logpath = strdup(tmpstr);

    if (!config_lookup_string(&conf, "main.cmdlog", &tmpstr)) {
        printf("Missing main.cmdlog in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.main.cmdlog = strdup(tmpstr);

    // BVEXCAM configuration
    if (!config_lookup_int(&conf, "bvexcam.enabled", &tmpint)) {
        printf("Missing bvexcam.enabled in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.enabled = tmpint;

    if (!config_lookup_string(&conf, "bvexcam.logfile", &tmpstr)) {
        printf("Missing bvexcam.logfile in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.logfile = strdup(tmpstr);

    if (!config_lookup_int(&conf, "bvexcam.camera_handle", &tmpint)) {
        printf("Missing bvexcam.camera_handle in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.camera_handle = tmpint;

    if (!config_lookup_string(&conf, "bvexcam.lens_desc", &tmpstr)) {
        printf("Missing bvexcam.lens_desc in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.lens_desc = strdup(tmpstr);

    if (!config_lookup_int(&conf, "bvexcam.port", &tmpint)) {
        printf("Missing bvexcam.port in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.port = tmpint;

    if (!config_lookup_string(&conf, "bvexcam.workdir", &tmpstr)) {
        printf("Missing bvexcam.workdir in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.workdir = strdup(tmpstr);

    if(!config_lookup_string(&conf, "bvexcam.configdir", &tmpstr)){
        printf("Missing bvexcam.configdir in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.configdir = strdup(tmpstr);

    if(!config_lookup_int(&conf, "bvexcam.t_exp",&tmpint)){
        printf("Missing bvexcam.t_exp in %s\n", filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.bvexcam.t_exp = tmpint;

    if(!config_lookup_int(&conf, "bvexcam.save_image",&tmpint)){
        printf("Missing bvexcam.save_image in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.save_image = tmpint;

    if(!config_lookup_float(&conf, "bvexcam.lon",&tmpfloat)){
        printf("Missing bvexcam.lon in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.lon = tmpfloat;

    if(!config_lookup_float(&conf, "bvexcam.lat",&tmpfloat)){
        printf("Missing bvexcam.lat in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.lat = tmpfloat;

    if(!config_lookup_float(&conf, "bvexcam.alt",&tmpfloat)){
        printf("Missing bvexcam.alt in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.alt = tmpfloat;

    if(!config_lookup_int(&conf, "bvexcam.pbob",&tmpint)){
        printf("Missing bvexcam.pbob in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.pbob = tmpint;

    if(!config_lookup_int(&conf, "bvexcam.relay",&tmpint)){
        printf("Missing bvexcam.relay in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.bvexcam.relay = tmpint;

    // Accelerometer configuration
    if (!config_lookup_int(&conf, "accelerometer.enabled", &tmpint)) {
        printf("Missing accelerometer.enabled in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.accelerometer.enabled = tmpint;

    if (!config_lookup_string(&conf, "accelerometer.raspberry_pi_ip", &tmpstr)) {
        printf("Missing accelerometer.raspberry_pi_ip in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.accelerometer.raspberry_pi_ip = strdup(tmpstr);

    if (!config_lookup_int(&conf, "accelerometer.port", &tmpint)) {
        printf("Missing accelerometer.port in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.accelerometer.port = tmpint;

    if (!config_lookup_int(&conf, "accelerometer.num_accelerometers", &tmpint)) {
        printf("Missing accelerometer.num_accelerometers in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.accelerometer.num_accelerometers = tmpint;

    if (!config_lookup_string(&conf, "accelerometer.output_dir", &tmpstr)) {
        printf("Missing accelerometer.output_dir in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.accelerometer.output_dir = strdup(tmpstr);

    if (!config_lookup_string(&conf, "accelerometer.logfile", &tmpstr)) {
        printf("Missing accelerometer.logfile in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.accelerometer.logfile = strdup(tmpstr);

    if (!config_lookup_int(&conf, "accelerometer.chunk_duration", &tmpint)) {
        printf("Missing accelerometer.chunk_duration in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.accelerometer.chunk_duration = tmpint;

    if (!config_lookup_int(&conf, "accelerometer.print_interval", &tmpint)) {
        printf("Missing accelerometer.print_interval in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.accelerometer.print_interval = tmpint;

    //Motor Config

    if (!config_lookup_int(&conf,"motor.enabled", &tmpint)){
        printf("Missing motor.enabled in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.enabled = tmpint;

    if(!config_lookup_string(&conf,"motor.logfile",&tmpstr)){
       printf("Missing motor.logfile in %s\n", filepath);
       config_destroy(&conf);
       exit(0);
    }
    config.motor.logfile = strdup(tmpstr);

    if(!config_lookup_string(&conf,"motor.port",&tmpstr)){
        printf("Missing motor.port in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.port = strdup(tmpstr);

    if(!config_lookup_string(&conf,"motor.datadir",&tmpstr)){
        printf("Missing motor.datadir in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.datadir = strdup(tmpstr);

    if(!config_lookup_float(&conf,"motor.velP",&tmpfloat)){
	printf("Missing motor.velP in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.velP = tmpfloat;

    if(!config_lookup_float(&conf,"motor.velI",&tmpfloat)){
	printf("Missing motor.velI in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.velI = tmpfloat;

    if(!config_lookup_float(&conf,"motor.velD", &tmpfloat)){
        printf("Missing motor.velD in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.velD = tmpfloat;

    if(!config_lookup_float(&conf,"motor.velI_db",&tmpfloat)){
        printf("Missing motor.velI_db in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.velI_db = tmpfloat;

    if(!config_lookup_int(&conf,"motor.max_delta",&tmpint)){
        printf("Missing motor.max_delta in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.max_delta = tmpint;

    if(!config_lookup_float(&conf,"motor.friction",&tmpfloat)){
        printf("Missing motor.friction in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.friction = tmpfloat;

    if(!config_lookup_int(&conf,"motor.friction_db",&tmpint)){
        printf("Missing motor.friction in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.friction_db = tmpint;

    if(!config_lookup_float(&conf,"motor.vel_gain",&tmpfloat)){
        printf("Missing motor.vel_gain in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.vel_gain = tmpfloat;

    if(!config_lookup_float(&conf,"motor.tel_offset",&tmpfloat)){
        printf("Missing motor.tel_offset in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.tel_offset = tmpfloat;

    if(!config_lookup_int(&conf,"motor.max_current",&tmpint)){
        printf("Missing motor.max_current in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.max_current = tmpint;

    if(!config_lookup_float(&conf,"motor.max_velocity",&tmpfloat)){
        printf("Missing motor.max_velocity in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.max_velocity = tmpfloat;

    if(!config_lookup_float(&conf,"motor.pos_tol", &tmpfloat)){
        printf("Missing motor.pos_tol in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.pos_tol = tmpfloat;

    if(!config_lookup_int(&conf,"motor.pbob",&tmpint)){
        printf("Missing motor.pbob in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.pbob = tmpint;

    if(!config_lookup_int(&conf,"motor.relay",&tmpint)){
        printf("Missing motor.relay in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.relay = tmpint;

    //Lazisusan config

    if(!config_lookup_int(&conf,"lazisusan.enabled",&tmpint)){
        printf("Missing lazisusan.enabled in %s\n", filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.enabled = tmpint;

    if(!config_lookup_string(&conf,"lazisusan.logfile",&tmpstr)){
	printf("Missing lazisusan.logfile in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.logfile = strdup(tmpstr);

    if(!config_lookup_string(&conf,"lazisusan.datadir",&tmpstr)){
        printf("Missing lazisusan.datadir in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.datadir = strdup(tmpstr);

    if(!config_lookup_string(&conf,"lazisusan.port",&tmpstr)){
	printf("Missing lazisusan.port in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.port = strdup(tmpstr);

    if(!config_lookup_float(&conf,"lazisusan.gear_ratio",&tmpfloat)){
	printf("Missing lazisusan.gear_ratio in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.gear_ratio = tmpfloat;

    if(!config_lookup_float(&conf,"lazisusan.offset",&tmpfloat)){
	printf("Missing lazisusan.offset in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.offset = tmpfloat;

    if(!config_lookup_float(&conf,"lazisusan.g_az",&tmpfloat)){
	printf("Missing lazisusan.g_az in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.g_az = tmpfloat;

    if(!config_lookup_float(&conf,"lazisusan.max_freq",&tmpfloat)){
	printf("Missing lazisusan.max_freq in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.max_freq = tmpfloat;

    if(!config_lookup_float(&conf,"lazisusan.deltav",&tmpfloat)){
        printf("Missing lazisusan.deltav in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.lazisusan.deltav = tmpfloat;

    if(!config_lookup_float(&conf,"lazisusan.vmax",&tmpfloat)){
	printf("Missing lazisusan.vmax in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.vmax = tmpfloat;

    if(!config_lookup_float(&conf,"lazisusan.db",&tmpfloat)){
	printf("Missing lazisusan.db in %s\n",filepath);
        config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.db = tmpfloat;

    if(!config_lookup_float(&conf,"lazisusan.gps_db",&tmpfloat)){
        printf("Missing lazisusan.gps_db in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.lazisusan.gps_db = tmpfloat;

    //Lockpin
    if(!config_lookup_string(&conf,"lockpin.logfile",&tmpstr)){
	printf("Missing lockpin.logfile in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lockpin.logfile = strdup(tmpstr);

    if(!config_lookup_int(&conf,"lockpin.enabled",&tmpint)){
	printf("Missing lockpin.enabledin %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lockpin.enabled = tmpint;

    if(!config_lookup_int(&conf,"lockpin.baud",&tmpint)){
        printf("Missing lockpin.baud in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lockpin.baud = tmpint;

    if(!config_lookup_string(&conf,"lockpin.serialport",&tmpstr)){
	printf("Missing lockpin.serialport in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lockpin.serialport = strdup(tmpstr);

    if(!config_lookup_int(&conf,"lockpin.duration",&tmpint)){
	printf("Missing lockpin.duration in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lockpin.duration = tmpint;

    //GPS server config
    if(!config_lookup_int(&conf,"gps_server.enabled",&tmpint)){
        printf("Missing gps_server.enabled in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.gps_server.enabled = tmpint;

    if(!config_lookup_string(&conf,"gps_server.logfile",&tmpstr)){
        printf("Missing gps_server.logfile in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.gps_server.logfile = strdup(tmpstr);

    if(!config_lookup_string(&conf,"gps_server.ip",&tmpstr)){
        printf("Missing gps_server.ip in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.gps_server.ip = strdup(tmpstr);

    if(!config_lookup_int(&conf,"gps_server.port",&tmpint)){
        printf("Missing gps_server.port in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.gps_server.port = tmpint;

    if(!config_lookup_int(&conf,"gps_server.timeout",&tmpint)){
        printf("Missing gps_server.timeout in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.gps_server.timeout = tmpint;

    // Starcam downlink configuration
    if(!config_lookup_int(&conf,"starcam_downlink.enabled",&tmpint)){
        printf("Missing starcam_downlink.enabled in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.enabled = tmpint;

    if(!config_lookup_string(&conf,"starcam_downlink.logfile",&tmpstr)){
        printf("Missing starcam_downlink.logfile in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.logfile = strdup(tmpstr);

    if(!config_lookup_int(&conf,"starcam_downlink.port",&tmpint)){
        printf("Missing starcam_downlink.port in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.port = tmpint;



    if(!config_lookup_int(&conf,"starcam_downlink.compression_quality",&tmpint)){
        printf("Missing starcam_downlink.compression_quality in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.compression_quality = tmpint;

    if(!config_lookup_int(&conf,"starcam_downlink.chunk_size",&tmpint)){
        printf("Missing starcam_downlink.chunk_size in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.chunk_size = tmpint;

    if(!config_lookup_int(&conf,"starcam_downlink.max_bandwidth_kbps",&tmpint)){
        printf("Missing starcam_downlink.max_bandwidth_kbps in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.max_bandwidth_kbps = tmpint;

    if(!config_lookup_int(&conf,"starcam_downlink.image_timeout_sec",&tmpint)){
        printf("Missing starcam_downlink.image_timeout_sec in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.image_timeout_sec = tmpint;

    if(!config_lookup_string(&conf,"starcam_downlink.workdir",&tmpstr)){
        printf("Missing starcam_downlink.workdir in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.workdir = strdup(tmpstr);

    if(!config_lookup_string(&conf,"starcam_downlink.notification_file",&tmpstr)){
        printf("Missing starcam_downlink.notification_file in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.starcam_downlink.notification_file = strdup(tmpstr);

    // Read UDP client IPs array
    config_setting_t *client_ips_setting = config_lookup(&conf, "starcam_downlink.udp_client_ips");
    if (client_ips_setting != NULL && config_setting_is_array(client_ips_setting)) {
        config.starcam_downlink.num_client_ips = config_setting_length(client_ips_setting);
        config.starcam_downlink.udp_client_ips = malloc(config.starcam_downlink.num_client_ips * sizeof(char*));
        
        for (int i = 0; i < config.starcam_downlink.num_client_ips; i++) {
            config_setting_t *ip_setting = config_setting_get_elem(client_ips_setting, i);
            if (ip_setting && config_setting_type(ip_setting) == CONFIG_TYPE_STRING) {
                config.starcam_downlink.udp_client_ips[i] = strdup(config_setting_get_string(ip_setting));
            } else {
                printf("Invalid IP address in starcam_downlink.udp_client_ips[%d]\n", i);
                config_destroy(&conf);
                exit(0);
            }
        }
    } else {
        // Default: no client IPs configured (request-response mode only)
        config.starcam_downlink.num_client_ips = 0;
        config.starcam_downlink.udp_client_ips = NULL;
    }

    // Server configuration
    if(!config_lookup_int(&conf,"server.enabled",&tmpint)){
        printf("Missing server.enabled in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.server.enabled = tmpint;

    if(!config_lookup_string(&conf,"server.logfile",&tmpstr)){
        printf("Missing server.logfile in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.server.logfile = strdup(tmpstr);

    if(!config_lookup_string(&conf,"server.ip",&tmpstr)){
        printf("Missing server.ip in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.server.ip = strdup(tmpstr);

    if(!config_lookup_int(&conf,"server.port",&tmpint)){
        printf("Missing server.port in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.server.port = tmpint;

    if(!config_lookup_int(&conf,"server.timeout",&tmpint)){
        printf("Missing server.timeout in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.server.timeout = tmpint;

    //power config

    if(!config_lookup_int(&conf,"power.enabled",&tmpint)){
        printf("Missing power.enabled in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.enabled = tmpint;

    if(!config_lookup_string(&conf,"power.ip",&tmpstr)){
        printf("Missing power.ip in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.ip = strdup(tmpstr);

    if(!config_lookup_string(&conf,"power.logfile",&tmpstr)){
        printf("Missing power.logfile in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.logfile = strdup(tmpstr);

    if(!config_lookup_int(&conf,"power.port",&tmpint)){
        printf("Missing power.port in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.port = tmpint;

    if(!config_lookup_int(&conf,"power.timeout",&tmpint)){
        printf("Missing power.timeout in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.timeout = tmpint;

    //pbob0
    if(!config_lookup_int(&conf,"power.pbob0.enabled",&tmpint)){
        printf("Missing power.pbob0.enabled in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob0.enabled = tmpint;

    if(!config_lookup_int(&conf,"power.pbob0.id",&tmpint)){
        printf("Missing power.pbob0.id in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob0.id = tmpint;

    if(!config_lookup_string(&conf,"power.pbob0.ip",&tmpstr)){
        printf("Missing power.pbob0.ip in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob0.ip = strdup(tmpstr);

    if(!config_lookup_int(&conf,"power.pbob0.num_relays",&tmpint)){
        printf("Missing power.pbob0.id in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob0.num_relays = tmpint;

    //pbob1
 
    if(!config_lookup_int(&conf,"power.pbob1.enabled",&tmpint)){
        printf("Missing power.pbob1.enabled in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob1.enabled = tmpint;

    if(!config_lookup_int(&conf,"power.pbob1.id",&tmpint)){
        printf("Missing power.pbob1.id in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob1.id = tmpint;

    if(!config_lookup_string(&conf,"power.pbob1.ip",&tmpstr)){
        printf("Missing power.pbob1.ip in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob1.ip = strdup(tmpstr);

    if(!config_lookup_int(&conf,"power.pbob1.num_relays",&tmpint)){
        printf("Missing power.pbob1.id in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob1.num_relays = tmpint;

    //pbob2
    if(!config_lookup_int(&conf,"power.pbob2.enabled",&tmpint)){
        printf("Missing power.pbob2.enabled in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob2.enabled = tmpint;

    if(!config_lookup_int(&conf,"power.pbob2.id",&tmpint)){
        printf("Missing power.pbob2.id in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob2.id = tmpint;


    if(!config_lookup_string(&conf,"power.pbob2.ip",&tmpstr)){
        printf("Missing power.pbob2.ip in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob2.ip = strdup(tmpstr);

    if(!config_lookup_int(&conf,"power.pbob2.num_relays",&tmpint)){
        printf("Missing power.pbob2.id in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.power.pbob2.num_relays = tmpint;

    config_destroy(&conf);
}

char* join_path(char* path1, char* path2) {
    char* joined = (char*)malloc((strlen(path1) + strlen(path2) + 1) * sizeof(char));
    strcpy(joined, path1);
    strcat(joined, path2);
    return joined;
}

void print_config() {
    printf("Found following config parameters:\n\n");
    printf("main:{\n");
    printf("  logpath = %s;\n", config.main.logpath);
    printf("  cmdlog = %s;\n", config.main.cmdlog);
    printf("};\n\n");
    printf("bvexcam:{\n");
    printf("  enabled = %d;\n", config.bvexcam.enabled);
    printf("  logfile = %s;\n", config.bvexcam.logfile);
    printf("  camera_handle = %d;\n", config.bvexcam.camera_handle);
    printf("  lens_desc = %s;\n", config.bvexcam.lens_desc);
    printf("  port = %d;\n", config.bvexcam.port);
    printf("  workdir = %s;\n", config.bvexcam.workdir);
    printf("  configdir = %s;\n",config.bvexcam.configdir);
    printf("  t_exp = %d;\n", config.bvexcam.t_exp);
    printf("  save_image = %d;\n", config.bvexcam.save_image);
    printf("};\n\n");
    printf("accelerometer:{\n");
    printf("  enabled = %d;\n", config.accelerometer.enabled);
    printf("  raspberry_pi_ip = %s;\n", config.accelerometer.raspberry_pi_ip);
    printf("  port = %d;\n", config.accelerometer.port);
    printf("  num_accelerometers = %d;\n", config.accelerometer.num_accelerometers);
    printf("  output_dir = %s;\n", config.accelerometer.output_dir);
    printf("  logfile = %s;\n", config.accelerometer.logfile);
    printf("  chunk_duration = %d;\n", config.accelerometer.chunk_duration);
    printf("  print_interval = %d;\n", config.accelerometer.print_interval);
    printf("};\n\n");
    printf("motor:{\n");
    printf(" enabled = %d;\n", config.motor.enabled);
    printf(" logfile = %s;\n", config.motor.logfile);
    printf(" port = %s;\n", config.motor.port);
    printf(" datadir = %s;\n", config.motor.datadir);
    printf(" velP = %lf;\n", config.motor.velP);
    printf(" velI = %lf;\n", config.motor.velI);
    printf(" velD = %lf;\n", config.motor.velD);
    printf(" velI_db = %lf;\n", config.motor.velI_db);
    printf(" max_delta = %d;\n", config.motor.max_delta);
    printf(" friction = %lf;\n", config.motor.friction);
    printf(" friction_db = %d;\n", config.motor.friction_db);
    printf(" vel_gain = %lf;\n", config.motor.vel_gain);
    printf(" tel_offset = %lf;\n", config.motor.tel_offset);
    printf(" max_current = %d;\n", config.motor.max_current);
    printf(" max_velocity = %lf;\n", config.motor.max_velocity);
    printf(" pos_tol = %lf;\n", config.motor.pos_tol);
    printf("};\n\n");
    printf("lazisusan:{\n");
    printf(" enabled = %d;\n",config.lazisusan.enabled);
    printf(" logfile = %s;\n",config.lazisusan.logfile);
    printf(" datadir = %s;\n",config.lazisusan.datadir);
    printf(" port = %s;\n", config.lazisusan.port);
    printf(" gear_ratio = %lf;\n",config.lazisusan.gear_ratio);
    printf(" offset = %lf;\n",config.lazisusan.offset);
    printf(" g_az = %lf;\n", config.lazisusan.g_az);
    printf(" max_freq = %lf\n",config.lazisusan.max_freq);
    printf(" deltav = %lf\n",config.lazisusan.deltav);
    printf(" vmax = %lf\n",config.lazisusan.vmax);
    printf(" db = %lf\n",config.lazisusan.db);
    printf(" gps_db = %lf\n",config.lazisusan.gps_db);
    printf("};\n\n");
    printf("lockpin:{\n");
    printf(" enabled = %d;\n",config.lockpin.enabled);
    printf(" logfile = %s;\n",config.lockpin.logfile);
    printf(" baud = %d;\n",config.lockpin.baud);
    printf(" serialport = %s;\n",config.lockpin.serialport);
    printf(" duration = %d;\n",config.lockpin.duration);
    printf("};\n\n");
    printf("gps_server:{\n");
    printf(" enabled = %d;\n",config.gps_server.enabled);
    printf(" logfile = %s;\n",config.gps_server.logfile);
    printf(" ip = %s;\n",config.gps_server.ip);
    printf(" port = %d;\n",config.gps_server.port);
    printf(" timeout = %d;\n",config.gps_server.timeout);
    printf("};\n\n"); 

    printf("starcam_downlink:{\n");
    printf(" enabled = %d;\n",config.starcam_downlink.enabled);
    printf(" logfile = %s;\n",config.starcam_downlink.logfile);
    printf(" port = %d;\n",config.starcam_downlink.port);
    printf(" compression_quality = %d;\n",config.starcam_downlink.compression_quality);
    printf(" chunk_size = %d;\n",config.starcam_downlink.chunk_size);
    printf(" max_bandwidth_kbps = %d;\n",config.starcam_downlink.max_bandwidth_kbps);
    printf(" image_timeout_sec = %d;\n",config.starcam_downlink.image_timeout_sec);
    printf(" workdir = %s;\n",config.starcam_downlink.workdir);
    printf(" notification_file = %s;\n",config.starcam_downlink.notification_file);
    printf(" udp_client_ips = [");
    for (int i = 0; i < config.starcam_downlink.num_client_ips; i++) {
        printf("\"%s\"", config.starcam_downlink.udp_client_ips[i]);
        if (i < config.starcam_downlink.num_client_ips - 1) printf(", ");
    }
    printf("];\n");
    printf("};\n\n"); 

    printf("server:{\n");
    printf(" enabled = %d;\n",config.server.enabled);
    printf(" logfile = %s;\n",config.server.logfile);
    printf(" ip = %s;\n",config.server.ip);
    printf(" port = %d;\n",config.server.port);
    printf(" timeout = %d;\n",config.server.timeout);
    printf("};\n\n"); 

    printf("power:{\n");
    printf(" enabled = %d;\n",config.power.enabled);
    printf(" logfile = %s;\n", config.power.logfile);
    printf(" ip = %s;\n",config.power.ip);
    printf(" port = %d\n",config.power.port);
    printf(" timeout = %d\n",config.power.timeout);
    printf(" pbob0:{\n");
    printf("  enabled = %d;\n", config.power.pbob0.enabled);
    printf("  id = %d\n", config.power.pbob0.id);
    printf("  ip = %s;\n", config.power.pbob0.ip);
    printf("  num_relays = %d;\n", config.power.pbob0.num_relays);
    printf(" };\n");
    printf(" pbob1:{\n");
    printf("  enabled = %d;\n", config.power.pbob1.enabled);
    printf("  id = %d\n", config.power.pbob1.id);
    printf("  ip = %s;\n", config.power.pbob1.ip);
    printf("  num_relays = %d;\n", config.power.pbob1.num_relays);
    printf(" };\n");
    printf(" pbob2:{\n");
    printf("  enabled = %d;\n", config.power.pbob2.enabled);
    printf("  id = %d\n", config.power.pbob2.id);
    printf("  ip = %s;\n", config.power.pbob2.ip);
    printf("  num_relays = %d;\n", config.power.pbob2.num_relays);
    printf(" };\n");
    printf("};\n\n");
}
