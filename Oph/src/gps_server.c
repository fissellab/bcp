#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "file_io_Oph.h"
#include "gps_server.h"

extern struct conf_params config;

GPS_data curr_gps;

FILE *gps_server_log;
int stop_server = 0;
int server_running = 0;	

int decode_GPS(char * gps_d){
	int ret;
	double lon;
	double lat;
	double alt;
	double head;
        ret = sscanf(gps_d,"gps_lat:%lf,gps_lon:%lf,gps_alt:%lf,gps_head:%lf",&lat, &lon, &alt, &head);

	curr_gps.gps_lat = lat;
	curr_gps.gps_lon = lon;
	curr_gps.gps_alt = alt;
	curr_gps.gps_head = head;

	return ret;
}


void* do_GPS_server(){

	int GPS_fd;
	char buffer[MAXLINE];

	char *request = "GET_GPS";

	struct sockaddr_in servaddr;
	struct timeval tv;
	int n, len;

	// Initialize GPS data
	curr_gps.gps_lat = 0;
	curr_gps.gps_lon = 0;
	curr_gps.gps_alt = 0;
	curr_gps.gps_head = 0;

	server_running = 0;
	stop_server = 0;

	tv.tv_sec = 0;
	tv.tv_usec = config.gps_server.timeout;

	if((GPS_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		write_to_log(gps_server_log,"gps_server.c","do_GPS_server","Socket creation failed");
	}else{
	// All requesters initialized successfully
		server_running = 1;

		memset(&servaddr, 0, sizeof(servaddr));

		servaddr.sin_family = AF_INET; //IP address type in this case IPv4
		servaddr.sin_port = htons(config.gps_server.port);
		servaddr.sin_addr.s_addr = inet_addr(config.gps_server.ip);
		setsockopt(GPS_fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv));
		write_to_log(gps_server_log,"gps_server.c","do_GPS_server","GPS client started successfully, requesting data...");

		while(!stop_server){
			//sends request
			sendto(GPS_fd, (const char *)request, strlen(request),MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));

			n = recvfrom(GPS_fd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);

			buffer[n] = '\0';
			if(decode_GPS(buffer) != 4){
				write_to_log(gps_server_log,"gps_server.c","do_GPS_server","Recieved corrupted message");
			}
		}
		// Add delay to prevent overwhelming the server
		usleep(50000); // 50ms delay between request cycles
	}
	if(server_running < 0){
		write_to_log(gps_server_log,"gps_server.c","do_GPS_server","Could not start server");
		fclose(gps_server_log);
	}else{
		write_to_log(gps_server_log,"gps_server.c","do_GPS_server","Shutting down GPS server");
		server_running = 0;
		fclose(gps_server_log);
		close(GPS_fd);
	}
}
