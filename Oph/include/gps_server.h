#ifndef GPS_SERVER_H
#define GPS_SERVER_H

#define MAXLINE 1024

typedef struct GPS_data{
	double gps_lat;
	double gps_lon;
	float  gps_alt;
	float  gps_head;
}GPS_data;

void* do_GPS_server();

extern GPS_data curr_gps;
extern FILE *gps_server_log;
extern int stop_server;
extern int server_running;

#endif
