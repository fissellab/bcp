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
    int t_exp;
    int save_image;
    float lat;
    float lon;
    double alt;
    int pbob;
    int relay;
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
    char* datadir;
    float velP;
    float velI;
    float velD;
    float velI_db;
    int max_delta;
    double friction;
    int friction_db;
    double vel_gain;
    double tel_offset;
    int  max_current;
    float  max_velocity;
    double pos_tol;
    int pbob;
    int relay;
} motor_conf;

typedef struct lazisusan_conf{
    int enabled;
    char* logfile;
    char* datadir;
    char* port;
    double gear_ratio;
    double offset;
    double g_az;
    double max_freq;
    double deltav;
    double vmax;
    double db;
    double gps_db;
} lazisusan_conf;

typedef struct lockpin_conf{
	char *logfile;
	int enabled;
	int baud;
	char *serialport;
	int duration;
} lockpin_conf;

typedef struct gps_server_conf{
	int enabled;
	char *logfile;
	char *ip;
	int port;
	int timeout;
} gps_server_conf;

typedef struct starcam_downlink_conf{
	int enabled;
	char *logfile;
	int port;
	int compression_quality;
	int chunk_size;
	int max_bandwidth_kbps;
	int image_timeout_sec;
	char *workdir;
	char *notification_file;
	char **udp_client_ips;
	int num_client_ips;
} starcam_downlink_conf;

typedef struct server_conf{
	int enabled;
	char *logfile;
	char *ip;
	int port;
	int timeout;
} server_conf;

typedef struct pbob_conf{
	int enabled;
	int id;
	char *ip;
        int num_relays;
}pbob_conf;

typedef struct power_conf{
	int enabled;
        char *ip;
        char *logfile;
        int port;
        int timeout;
        struct pbob_conf pbob0;
        struct pbob_conf pbob1;
	struct pbob_conf pbob2;
}power_conf;

typedef struct conf_params {
    struct main_conf main;
    struct bvexcam_conf bvexcam;
    struct accelerometer_conf accelerometer;
    struct motor_conf motor;
    struct lazisusan_conf lazisusan;
    struct lockpin_conf lockpin;
    struct gps_server_conf gps_server;
    struct starcam_downlink_conf starcam_downlink;
    struct server_conf server;
    struct power_conf power;
} conf_params;

extern struct conf_params config;

#endif
