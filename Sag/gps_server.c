#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
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

void* do_GPS_server(){

	int GPS_fd;
	char buffer[MAXLINE];

	char *request = "GET_GPS"

	struct sockaddr_in servaddr;
	int n, len;

	// Initialize GPS data
	curr_gps.gps_lat = 0;
	curr_gps.gps_lon = 0;
	curr_gps.gps_alt = 0;

	server_running = 0;
	stop_server = 0;
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		write_to_log(gps_server_log,"gps_server.c","do_GPS_server","Socket creation failed");
	}else{
	// All requesters initialized successfully
		server_running = 1;

		memset(&servaddr, 0, sizeof(servaddr));

		servaddr.sin_family = AF_INET; //IP address type in this case IPv4
		servaddr.sin_port = htons(config.gps_server.port);
		servaddr.sin_addr.s_addr = inet_addr(config.gps_server.ip);

		write_to_log(gps_server_log,"gps_server.c","do_GPS_server","GPS client started successfully, requesting data...");

		while(!stop_server){
			//sends request
			sendto(sockfd, (const char *)request, strlen(request),MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));

			n = rcvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);

			buffer[n] = '\0';
			if(sscanf(buffer,"gps_lat:%lf,gps_lon:%lf,gps_alt:%lf,gps_head:%lf", &curr_gps.gps_lat, &curr_gps.gps_lon, &curr_gps.gps_alt, &curr_gps.gps_head) != 1){
				write_to_log(gps_server_log,"gps_server.c","do_GPS_server","Recieved corrupted message");
			}
		}
		// Add delay to prevent overwhelming the server
		usleep(50000); // 100ms delay between request cycles
	}
	if(server_running < 0){
		write_to_log(gps_server_log,"gps_server.c","do_GPS_server","Could not start server");
		fclose(gps_server_log);
	}else{
		write_to_log(gps_server_log,"gps_server.c","do_GPS_server","Shutting down GPS server");
		server_running = 0;
		fclose(gps_server_log);
	}
}
