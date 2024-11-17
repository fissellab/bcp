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

    if(!config_lookup_string(&conf,"motor.datafile",&tmpstr)){
        printf("Missing motor.dataflie in %s\n",filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.datafile = strdup(tmpstr);

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

    if(!config_lookup_float(&conf,"motor.velfactor",&tmpfloat)){
	printf("Missing motor.velfactor in %s\n", filepath);
        config_destroy(&conf);
        exit(0);
    }
    config.motor.velfactor = tmpfloat;

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
    printf(" datafile = %s;\n", config.motor.datafile);
    printf(" velP = %lf;\n", config.motor.velP);
    printf(" velI = %lf;\n", config.motor.velI);
    printf(" velD = %lf;\n", config.motor.velD);
    printf(" velI_db = %lf;\n", config.motor.velI_db);
    printf(" velfactor = %lf;\n", config.motor.velfactor);
    printf(" max_delta = %d;\n", config.motor.max_delta);
    printf(" friction = %lf;\n", config.motor.friction);
    printf(" friction_db = %d;\n", config.motor.friction_db);
    printf(" vel_gain = %lf;\n", config.motor.vel_gain);
    printf(" tel_offset = %lf;\n", config.motor.tel_offset);
    printf(" max_current = %d;\n", config.motor.max_current);
    printf(" max_velocity = %lf;\n", config.motor.max_velocity);
    printf(" pos_tol = %lf;\n", config.motor.pos_tol);
    printf("};\n\n");
}
