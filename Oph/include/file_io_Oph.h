#ifndef FILE_IO_OPH_H
#define FILE_IO_OPH_H

#include <stdio.h>

void write_to_log(FILE* fptr, char* cfile, char* func, char* msg);
void read_in_config(char* filepath);
char* join_path(char* path1, char* path2);
void print_config();

typedef struct main_conf {
    char* logpath;
    char* cmdlog;
} main_conf;

typedef struct bvexcam_conf {
    int enabled;
    char* logfile;
    int camera_handle;
    char* lens_desc;
    int port;
    char* workdir;
    char* configdir;
} bvexcam_conf;

typedef struct accelerometer_conf {
    int enabled;
    char* raspberry_pi_ip;
    int port;
    int num_accelerometers;
    char* output_dir;
    char* logfile;
    int chunk_duration;
    int print_interval;
} accelerometer_conf;

typedef struct motor_conf {
    int enabled;
    char* logfile;
    char* port;
    char* datafile;
    float velP;
    float velI;
    float velD;
    float velI_db;
    float velfactor;
    int max_delta;
    double friction;
    int friction_db;
    double vel_gain;
    double tel_offset;
    int  max_current;
    float  max_velocity;
    double pos_tol;
    
} motor_conf;

typedef struct conf_params {
    struct main_conf main;
    struct bvexcam_conf bvexcam;
    struct accelerometer_conf accelerometer;
    struct motor_conf motor;
} conf_params;

extern struct conf_params config;

#endif
