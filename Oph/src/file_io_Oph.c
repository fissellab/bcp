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

    if(!config_lookup_float(&conf,"lazisusan.stretch",&tmpfloat)){
	printf("Missing lazisusan.stretch in %s\n",filepath);
	config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.stretch = tmpfloat;

    if(!config_lookup_float(&conf,"lazisusan.vdb",&tmpfloat)){
	printf("Missing lazisusan.vdb in %s\n",filepath);
        config_destroy(&conf);
	exit(0);
    }
    config.lazisusan.vdb = tmpfloat;
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
    printf(" stretch = %lf\n",config.lazisusan.stretch);
    printf(" vdb = %lf\n",config.lazisusan.vdb);
    printf("};\n\n");
    printf("lockpin:{\n");
    printf(" enabled = %d;\n",config.lockpin.enabled);
    printf(" logfile = %s;\n",config.lockpin.logfile);
    printf(" baud = %d;\n",config.lockpin.baud);
    printf(" serialport = %s;\n",config.lockpin.serialport);
    printf(" duration = %d;\n",config.lockpin.duration);
    printf("};\n\n"); 
}
