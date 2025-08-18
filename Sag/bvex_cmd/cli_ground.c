/*

Last Updated: 2025-06-25, evening

A few things:
1. Add delay between send and listen
2. Decision to be made: Apply start, stop, exit commands to both computers? See line 175 (or about there) for how exit command is currently handled
3. Currently, the code is NOT logging any command, but the architecture to do so has been preserved
4. Although each transmission contains a timestamp, the program currently does not check it after receiving a packet
5. It is assumed that the server (not implemented here) handles the uplink and downlink, the sole responsibility of this program is to pass the packet to the server for transmission, and listen for feedback from the server code

Key ideas:

To issue commands to the balloon, there are three pieces of code: cli_ground, cli_Oph, cli_sag
cli_ground functions in a similar fashion as before it was modified (as far as the user is concerned):
	1. It takes in a string command
	Here is where it differs:
	2. It translates the string command into a number according to the enum list in the .h file
	3. The translated commands, and any subcommands, are packaged into a packet by create_packet (more on this later)
	4. The packet is then passed onto send(), which in turn passes the packet to the server
	5. If the command requires a feedback, the program evokes listen(), which listen for an incoming packet from the server
	6. Once received, it verifies the integrity of the packet (also more later)
	7. Once verified, it either displays the feedback, or does some computation with it before displaying
	8. Upon termination of step 7 (if no loops, the "else if" block should terminate after displaying the feedback), the program is free to listen to another user command

Packet creation, transfer, and verification
	- A packet (any packet) has the following parts, whether a part is used or left empty is up to the type of command/feedback:
		- Start byte, same for all packet, to indicate the beginning of a packet
		- num_data, the number of integer parameters associated with the command itself, NOT associated with the packet (i.e.  not the start byte, checksum, etc)
		- num_data, the number of double parameters associated with the command itself
		- destination, 0 to oph, 1 to sag, 2 to both
		- utc, the time when the packet is created
		- cmd_primary, primary command, as listed in the enum list
		- data[], to hold integers
		- bigpack[], to hold doubles
		- checksum, using xor logic to verify packet integrity
		- end byte, to indicate the end of a packet, same for all packet

	- To transfer a packet
		- It is passed onto a function named send(), the specific of the function has NOT been coded as of the last update
		- The listen() function listens for an incoming packet from the server

	- To verify a packet
		- For each packet creation, a check sum is computed using xor logic
		- Upon receiving a feedback, it computes the checksum of the received packet, then compares it against the received checksum
		- If there is a discrepancy between the computed and the received checksum, the packet is invalid

	IMPORTANT NOTICE
		For each uplinking command, the balloon expects a strict format that varies per command, but in general:
			- cmd_primary is a number in the enum list
			- any integers are transmitted in the integer array. when the balloon opens the packet, it expects to find a specific parameter in a particular spot. This should be documented as comments under each "else if" and as it creates a packet
			- the same goes for parameters as doubles
		For each command from the downlink channel, the ballon expects a strict format that varies per command, but in general:
			- Whenever possible, the cmd_primary has been used to indicate if the subsystem is activated. But this might not be the case for all
			- The program to find specific parameters in specific spots in the integer and/or double arrays, see comments within each "else if"

		Key takeaway: when changing the uplink command format, remember to change the way oph/sag decodes the message. The same applies for when the feedback format from oph/sag is changed. Same logic when creating "else if" for new commands

The ideas above are applicable to the codes in oph and sag, just in the opposite fashion
(i.e. it receives the commands from the onboard uplink server, then creates the necessary feedback packets to be passed on to the onboard downlink server)


GOOD LUCK!
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cli_ground.h"
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <libconfig.h>

// ANSI Color Codes
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define BRIGHT  "\033[1m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define MAGENTA "\033[35m"
#define WHITE   "\033[37m"
#define BRIGHT_BLUE    "\033[94m"
#define BRIGHT_CYAN    "\033[96m"
#define BRIGHT_GREEN   "\033[92m"
#define BRIGHT_YELLOW  "\033[93m"
#define BRIGHT_MAGENTA "\033[95m"
#define BRIGHT_WHITE   "\033[97m"

int exiting = 0;
Packet pkt;
enum commands com;
int sock_Oph, sock_Sag;
struct sockaddr_in servaddr_Oph;
struct sockaddr_in servaddr_Sag;
gs_conf_params gs_conf;
int count_Oph = 0;
int count_Sag = 0;
char* hostname;

void print_bvex_banner() {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Clear screen and move cursor to top
    printf("\033[2J\033[H");
    
    printf("\n");
    printf("    " BRIGHT_BLUE BOLD "██████╗ ██╗   ██╗███████╗██╗  ██╗" RESET "\n");
    printf("    " BRIGHT_BLUE BOLD "██╔══██╗██║   ██║██╔════╝╚██╗██╔╝" RESET "\n");
    printf("    " BRIGHT_BLUE BOLD "██████╔╝██║   ██║█████╗   ╚███╔╝ " RESET "\n");
    printf("    " BRIGHT_BLUE BOLD "██╔══██╗╚██╗ ██╔╝██╔══╝   ██╔██╗ " RESET "\n");
    printf("    " BRIGHT_BLUE BOLD "██████╔╝ ╚████╔╝ ███████╗██╔╝ ██╗" RESET "\n");
    printf("    " DIM "╚═════╝  ╚═══╝  ╚══════╝╚═╝  ╚═╝" RESET "\n");
    printf("\n");
    printf("    " BRIGHT_WHITE BOLD "The Balloon-borne VLBI Experiment" RESET "\n");
    printf("\n");
    printf("    " BRIGHT_YELLOW "Ground Station Control System" RESET "\n");
    printf("\n");
    printf("    " DIM "Session started: " RESET BRIGHT_YELLOW "%s" RESET "\n", timestamp);
    printf("\n\n");
}

void get_config_params(char* filepath){
	config_t conf;
	const char* tmpstr = NULL;
	int tmpint;
	double tmpfloat;
	
	config_init(&conf);

	if(!config_read_file(&conf,filepath)){
		printf("Error reading config file %s\n", filepath);
		config_destroy(&conf);
		exit(0);
	}
	
	if(!config_lookup_string(&conf, "main.sag_ip", &tmpstr)) {
		printf("Missing main.sag_ip in %s\n",filepath);
		config_destroy(&conf);
		exit(0);
	}
	
	gs_conf.sag_ip = strdup(tmpstr);
	
	if(!config_lookup_string(&conf, "main.oph_ip", &tmpstr)) {
		printf("Missing main.oph_ip in %s\n",filepath);
		config_destroy(&conf);
		exit(0);
	}
	
	gs_conf.oph_ip = strdup(tmpstr);
	
	if(!config_lookup_int(&conf, "main.sag_port", &tmpint)) {
		printf("Missing main.sag_port in %s\n",filepath);
		config_destroy(&conf);
		exit(0);
	}
	
	gs_conf.sag_port = tmpint;
	
	if(!config_lookup_int(&conf, "main.oph_port", &tmpint)) {
		printf("Missing main.oph_port in %s\n",filepath);
		config_destroy(&conf);
		exit(0);
	}
	
	gs_conf.oph_port = tmpint;
	
	if(!config_lookup_int(&conf, "main.timeout", &tmpint)) {
		printf("Missing main.timeout in %s\n",filepath);
		config_destroy(&conf);
		exit(0);
	}
	
	gs_conf.timeout = tmpint;
	
	if(!config_lookup_int(&conf, "main.ls_enabled", &tmpint)) {
		printf("Missing main.ls_enabled in %s\n",filepath);
		config_destroy(&conf);
		exit(0);
	}
	
	gs_conf.ls_enabled = tmpint;
	
	config_destroy(&conf);
}

void print_config(){
	printf("main:\n");
	printf("{\n");
	printf(" sag_ip: %s;\n",gs_conf.sag_ip);
	printf(" oph_ip: %s;\n",gs_conf.oph_ip);
	printf(" sag_port: %d;\n",gs_conf.sag_port);
	printf(" oph_port: %d;\n",gs_conf.oph_port);
	printf(" timeout: %d;\n",gs_conf.timeout);
	printf(" ls_enabled: %d;\n",gs_conf.ls_enabled);
	printf("};\n");
	
}
void connect_to_sock(){
	
	struct timeval tv;
	
	tv.tv_sec = gs_conf.timeout;
	tv.tv_usec = 0;

	if((sock_Oph = socket(AF_INET, SOCK_DGRAM,0)) < 0){
		printf("Socket creation failed for Ophiuchus\n");
		exit(1);
	}
	
	if((sock_Sag = socket(AF_INET, SOCK_DGRAM,0)) < 0){
                printf("Socket creation failed for Saggitarius\n");
                exit(1);
        }

	memset(&servaddr_Oph, 0, sizeof(servaddr_Oph));

	servaddr_Oph.sin_family = AF_INET;
	servaddr_Oph.sin_port = htons(gs_conf.oph_port);
	servaddr_Oph.sin_addr.s_addr = inet_addr(gs_conf.oph_ip);

	memset(&servaddr_Sag, 0, sizeof(servaddr_Sag));

        servaddr_Sag.sin_family = AF_INET;
        servaddr_Sag.sin_port = htons(gs_conf.sag_port);
        servaddr_Sag.sin_addr.s_addr = inet_addr(gs_conf.sag_ip);
        
        if(connect(sock_Oph, (struct sockaddr *)&servaddr_Oph, sizeof(servaddr_Oph))<0){
        	printf("Could not connect to Ophiuchus\n");
        	exit(1);
        }
        
        if(connect(sock_Sag, (struct sockaddr *)&servaddr_Sag, sizeof(servaddr_Sag))<0){
        	printf("Could not connect to Saggitarius\n");
        	exit(1);
        }
        
        setsockopt(sock_Oph,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        setsockopt(sock_Sag,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));

}




//send the packet to the server code
int send_packet(Packet pkt){
	char buf[MAXLINE];
	char big_buf[MAXBUF];
	char buffer_oph[MAXBUF];
	char buffer_sag[MAXBUF];
	char small_buf[MAXBUF];
	char intstr[10];
	char floatstr[10];
	int n_Oph,len_Oph,n_Sag,len_Sag;
	
	if(pkt.num_data>0){
		snprintf(small_buf,MAXBUF,"%d",pkt.data[0]);
		for (int i=1;i<pkt.num_data;i++){
			snprintf(intstr,10,",%d",pkt.data[i]);
			strcat(small_buf,intstr);
			//sprintf(&small_buf[strlen(small_buf)],"%d,",pkt.data[i]);
		}
		//sprintf(&small_buf[strlen(small_buf)],"%d",pkt.data[pkt.num_data-1]);
	}else{
		snprintf(small_buf,MAXBUF,"%s","null");
	}
	
	if(pkt.num_bigpack>0){
		snprintf(big_buf,MAXBUF,"%lf",pkt.bigpack[0]);
                for (int i=1;i<pkt.num_bigpack;i++){
			snprintf(floatstr,10,",%lf",pkt.bigpack[i]);
			strcat(big_buf,floatstr);
                	//sprintf(&big_buf[strlen(big_buf)],"%lf,",pkt.bigpack[i]);
		}
		//sprintf(&big_buf[strlen(big_buf)],"%lf",pkt.bigpack[pkt.num_bigpack-1]);
        }else{
		sprintf(big_buf,"%s","null");
	}
	snprintf(buf,MAXLINE,"%d\n%d\n%d\n%d\n%d\n%d\n%s\n%s\n%d\n%d\n",pkt.start, pkt.num_data, pkt.num_bigpack, pkt.utc, pkt.cmd_primary, pkt.destination, small_buf, big_buf, pkt.checksum, pkt.end);
	if(pkt.destination ==OPH){
		sendto(sock_Oph,(const char *) buf, strlen(buf), MSG_CONFIRM, (const struct sockaddr*) &servaddr_Oph, sizeof(servaddr_Oph));
		n_Oph = recvfrom(sock_Oph, (char *)buffer_oph,MAXLINE,MSG_WAITALL, (struct sockaddr *) &servaddr_Oph, &len_Oph);
		if (count_Oph < atoi(buffer_oph)){
                        count_Oph = atoi(buffer_oph);
                        printf("Command received on Ophiuchus current command count: %d\n",count_Oph);
                }else{
                        printf("Command not received on Ophiuchus try again\n");
                        return -1;
                }
	}else if (pkt.destination == SAG){
		sendto(sock_Sag,(const char *) buf, strlen(buf), MSG_CONFIRM, (const struct sockaddr*) &servaddr_Sag, sizeof(servaddr_Sag));
		n_Sag = recvfrom(sock_Sag, (char *)buffer_sag,MAXLINE,MSG_WAITALL, (struct sockaddr *) &servaddr_Sag, &len_Sag);
                if (count_Sag < atoi(buffer_sag)){
                        count_Sag = atoi(buffer_sag);
                        printf("Command received on Saggitarius current command count: %d\n",count_Sag);
                }else{
                        printf("Command not received on Saggitarius try again\n");
                        return -2;
                }

	}else if (pkt.destination == BOTH){
		sendto(sock_Oph,(const char *) buf, strlen(buf), MSG_CONFIRM, (const struct sockaddr*) &servaddr_Oph, sizeof(servaddr_Oph));
		n_Oph = recvfrom(sock_Oph, (char *)buffer_oph,MAXLINE,MSG_WAITALL, (struct sockaddr *) &servaddr_Oph, &len_Oph);

		if (count_Oph < atoi(buffer_oph)){
                        count_Oph = atoi(buffer_oph);
                        printf("Command received on Ophiuchus current command count: %d\n",count_Oph);
                }else{
                        printf("Command not received on Ophiuchus try again\n");
			sendto(sock_Sag,(const char *) buf, strlen(buf), MSG_CONFIRM, (const struct sockaddr*) &servaddr_Sag, sizeof(servaddr_Sag));
                	n_Sag = recvfrom(sock_Sag, (char *)buffer_sag,MAXLINE,MSG_WAITALL, (struct sockaddr *) &servaddr_Sag, &len_Sag);

                	if (count_Sag < atoi(buffer_sag)){
                        	count_Sag = atoi(buffer_sag);
                        	printf("Command received on Saggitarius current command count: %d\n",count_Sag);
                	}else{
                        	printf("Command not received on both computers try again\n");
                        	return -3;
                	}
                        return -1;
                }

		sendto(sock_Sag,(const char *) buf, strlen(buf), MSG_CONFIRM, (const struct sockaddr*) &servaddr_Sag, sizeof(servaddr_Sag));
		n_Sag = recvfrom(sock_Sag, (char *)buffer_sag,MAXLINE,MSG_WAITALL, (struct sockaddr *) &servaddr_Sag, &len_Sag);
		
		if (count_Sag < atoi(buffer_sag)){
			count_Sag = atoi(buffer_sag);
			printf("Command received on Saggitarius current command count: %d\n",count_Sag);
		}else{
			printf("Command not received on Saggitarius try again\n");
			return -2;
		}

	}
	return 1;
}

//XOR checksum
uint8_t compute_checksum(const uint8_t* bytes, const size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum ^= bytes[i];
    }
    return sum;
}
void print_packet(Packet pkt){
        printf("start:%d\n",pkt.start);
        printf("cmd_primary:%d\n",pkt.cmd_primary);
        printf("utc:%d\n",pkt.utc);
        printf("num_data:%d\n",pkt.num_data);
        printf("data:[");

        for(int i=0;i<pkt.num_data;i++){
                printf("%d,",pkt.data[i]);
        }

        printf("]\n");
        printf("num_bigpack:%d\n",pkt.num_bigpack);
        printf("bigpack:[");

        for(int i=0;i<pkt.num_bigpack;i++){
                printf("%lf,",pkt.bigpack[i]);
        }

        printf("]\n");
        printf("checksum:%d\n",pkt.checksum);
        printf("end:%d\n",pkt.end);

}
void clear_packet(Packet* pkt){
	pkt->start = 0;
	pkt->num_data = 0;
	pkt->num_bigpack = 0;
	pkt->cmd_primary = 0;
	pkt-> utc = 0;
	pkt->destination =0;
	memset(pkt->data,0,MAX_DATA*sizeof(int16_t));
	memset(pkt->bigpack,0,MAX_DATA*sizeof(double_t));
	pkt->checksum = 0;
	pkt->end=0;


}
//create packets for transmission
void create_packet(Packet* pkt, const uint8_t cmd1, const int16_t* data, const uint8_t small_len, const double_t* bigpack, const uint8_t big_len, const uint8_t dest) {
    clear_packet(pkt);
    pkt->start = START_BYTE;
    pkt->num_data = small_len;		//payload length
    pkt->num_bigpack = big_len;
    pkt->cmd_primary = cmd1;
    pkt->utc = (uint32_t)time(NULL);
    pkt->destination = dest;  // Set destination BEFORE checksum calculation
    memcpy(pkt->data, data, small_len * sizeof(int16_t));
    memcpy(pkt->bigpack, bigpack, big_len * sizeof(double_t));
    // Header size: start(1) + num_data(1) + num_bigpack(1) + utc(4) + cmd_primary(1) + destination(2) = 10 bytes
    const size_t base_size = 10 + small_len * sizeof(int16_t) + big_len * sizeof(double_t); // fields before checksum
    const uint8_t* byte_ptr = (uint8_t*)pkt;
    pkt->checksum = compute_checksum(byte_ptr, base_size);
    pkt->end = END_BYTE;
}

int bcp_is_alive(int fc){
	int16_t* payload;
    	double_t* big_payload;
	com = test;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, fc);
    	if(send_packet(pkt)== 1){
    		return 1;
    	}else{
    		return 0;
    	}
}

int confirm_send(){
	printf("Send command (y/n)?");
	
	char* input = get_input();
	char* cmd = (char*)malloc(strlen(input) * sizeof(char));
	sscanf(input, "%s\n", cmd);
	
	if(strcmp(cmd,"y")==0){
		return 1;
	}else{
		return 0;
	}
	


}

// This executes the commands, we can simply add commands by adding if statements
void exec_command(char* input) {
    char* arg;
    char* cmd;
    int scan;
    int ret;
    int cmd_unknown = 0;
    int16_t* payload;
    double_t* big_payload;
    uint8_t large_len;
    uint8_t small_len;
    
    arg = (char*)malloc(strlen(input) * sizeof(char));
    cmd = (char*)malloc(strlen(input) * sizeof(char));
    scan = sscanf(input, "%s %[^\t\n]", cmd, arg);

    if (strcmp(cmd, "test_Oph") == 0) {
        com = test;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);   
    } else if (strcmp(cmd, "test_Sag") == 0){
	com = test;
	create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    } else if (strcmp(cmd, "exit")==0){
    	exiting = 1;
    	close(sock_Sag);
    	close(sock_Oph);
    	return;
    } else if (strcmp(cmd, "exit_bcp") == 0) {
        printf("Exiting BCP\n");

        com = exit_both;
        create_packet(&pkt, com, payload, 0, big_payload, 0, BOTH);
        
        if(confirm_send()){
		int send_status = send_packet(pkt);

        	while(1){
        		if(send_status==-3){
				if(!bcp_is_alive(SAG) && !bcp_is_alive(OPH)){
                                        exiting = 1;
                                        close(sock_Sag);
                                        close(sock_Oph);
                                        printf("BCP already shutdown\n");
                                }
				break;
			}else if(send_status==-1){
				if(!bcp_is_alive(SAG)){
                                        printf("BCP shutdown on Saggitarius\n");
                                        break;

                                }
			}else if(send_status==-2){
				if(!bcp_is_alive(OPH)){
					printf("BCP shutdown on Ophiuchus\n");
                                        break;

				}
			}else{
				if(!bcp_is_alive(SAG) && !bcp_is_alive(OPH)){
					exiting = 1;
					close(sock_Sag);
    					close(sock_Oph);
					printf("BCP shutdown\n");
					break;
				}
			}
			sleep(gs_conf.timeout);
		}
		return;
        }else{
        	printf("Not sending command\n");
        }
        
    } else if (strcmp(cmd, "bvexcam_start") == 0) {
        com = bvexcam_start;
        
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
        

    }else if (strcmp(cmd, "bvexcam_stop") == 0) {
        com = bvexcam_stop;
        
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
        

    }else if (strcmp(cmd, "focus_bvexcam") == 0) {
        com = focus_bvexcam;
        
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
        

    } else if (strcmp(cmd, "bvexcam_solve_start")==0){
        com = bvexcam_solve_start;
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);

    } else if (strcmp(cmd, "bvexcam_solve_stop")==0){
        com = bvexcam_solve_stop;
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
    } else if (strcmp(cmd, "bvexcam_save_start")==0){
        com = bvexcam_save_start;
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);

    } else if (strcmp(cmd, "bvexcam_save_stop")==0){
        com = bvexcam_save_stop;
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
        
    } else if (strcmp(cmd, "bvexcam_set_exp")==0){
    
        com = bvexcam_set_exp;
        big_payload = (double_t *)malloc(2*sizeof(double_t));
        ret = sscanf(arg, "%lf",&big_payload[0]);

	if(ret != 1){
		printf("Invalid format, usage is bvexcam_set_exp <t_exp> where t_exp is the exposure time in ms. Accpeted values: 9.6 to 1000 ms\n");
		return;
	}

	if((big_payload[0] < 9.6) || (big_payload[0] > 1000)){
		printf("Invalid exposure time, should be between 9.6 and 1000\n");
		return;
	}
        create_packet(&pkt, com, payload, 0, big_payload, 1, OPH);
        
    } else if (strcmp(cmd,"motor_start")==0){
    	com = motor_start;
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);

    } else if (strcmp(cmd,"motor_stop")==0) {
	com = motor_stop;
        create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
    } else if(strcmp(cmd,"gotoenc") == 0){
    
    	if(gs_conf.ls_enabled){
    		big_payload = (double_t *)malloc(2*sizeof(double_t));
    		ret = sscanf(arg, "%lf,%lf",&big_payload[0],&big_payload[1]);
		large_len = 2;

		if(ret !=2){
			printf("Invalid format, usage is gotoenc <az>,<el> where az is the azimuth(0,360) in degrees and el(-0.7,55) is the elevation in degrees\n");
			return;
		}

		if((big_payload[0]<0) || (big_payload[0]>360)){
			printf("Invalid azimuth, should be between 0 and 360 degrees\n");
			return;
		}
		if((big_payload[1]<-0.7) || (big_payload[1]>55)){
			printf("Invalid elevation, should be between -0.7 and 55 degrees\n");
			return;
		}
    	}else{
    		big_payload = (double_t *)malloc(1*sizeof(double_t));
    		ret = sscanf(arg, "%lf",&big_payload[0]);

		if(ret!=1){
			printf("Invalid format, usage is gotoenc <az>,<el> where az is the azimuth(0,360) in degrees and el(-0.7,55) is the elevation in degrees\n");
			return;
		}

		if((big_payload[0]<-0.7) || (big_payload[0]>55)){
                        printf("Invalid elevation, should be between -0.7 and 55 degrees\n");
                        return;
                }
		large_len = 1;
	}

	com = gotoenc;
    	create_packet(&pkt, com, payload, 0, big_payload, large_len, OPH);

    }else if(strcmp(cmd,"encdither") == 0){
    
    	big_payload = (double_t *)malloc(3*sizeof(double_t));
    	payload = (int16_t *)malloc(1*sizeof(int16_t));
    	ret = sscanf(arg, "%lf,%lf,%lf,%hd",&big_payload[0], &big_payload[1], &big_payload[2], &payload[0]);
	if(ret != 4){
		printf("Invalid format, usage is encdither <start_el>,<stop_el>,<scan_vel>,<nscans> where start_el is the start elevation (0,55) in degrees, stop_el is the stop elevation in degrees (0, 55), scan_vel is the scan speed in degrees per second (0.5,3) and nscans is the number of scans\n");
		return;
	}
	if((big_payload[0]<0) || (big_payload[0]>55)){
		printf("Invalid start elevation should be between 0 and 55 degrees\n");
		return;
	}
	if((big_payload[1]<0) || (big_payload[1]>55)){
                printf("Invalid stop elevation should be between 0 and 55 degrees\n");
                return;
        }
	if((big_payload[2]<0.5) || (big_payload[2]>3)){
                printf("Invalid scan speed should be between 0.5 and 3 degrees per second\n");
                return;
        }
	if(big_payload[3]<=0){
		printf("Invalid scan number should be greater than 0\n");
		return;
	}

    	com = encdither;
    	create_packet(&pkt, com, payload, 1, big_payload, 3, OPH);

    }else if(strcmp(cmd,"enctrack") == 0){

    	big_payload = (double_t *)malloc(2*sizeof(double_t));
    	ret = sscanf(arg,"%lf,%lf",&big_payload[0],&big_payload[1]);
	if(ret != 2){
                printf("Invalid format, usage is enctrack <ra>,<dec> where ra is the right ascension (0,360) in degrees and dec is the declination (-90,90) in degrees\n");
		return;
        }
	if((big_payload[0] < 0) || (big_payload[0]>360)){
		printf("Invalid right ascension should be between 0 and 360 degrees\n");
		return;
	}
	if((big_payload[1] < -90) || (big_payload[1]>90)){
		printf("Invalid declination, should be between -90 and 90 degrees\n");
		return;
	}

    	com = enctrack;
    	create_packet(&pkt, com, payload, 0, big_payload, 2, OPH);

    }else if(strcmp(cmd,"enconoff")==0){
    	big_payload = (double_t *)malloc(4*sizeof(double_t));
    	ret = sscanf(arg,"%lf,%lf,%lf,%lf",&big_payload[0],&big_payload[1],&big_payload[2],&big_payload[3]);
	if(ret !=4){
		printf("Invalid format, usage is enconoff <ra>,<dec>,<offset>,<time> where ra is the target right ascension (0,360) in degrees, dec is the declination (-90,90) in degrees, offset is the elevation offset of the first off position (-5,5) and time is the on-source integration time in seconds\n");
		return;
	}
	if((big_payload[0] < 0) || (big_payload[0]>360)){
                printf("Invalid right ascension should be between 0 and 360 degrees\n");
                return;
        }
        if((big_payload[1] < -90) || (big_payload[1]>90)){
                printf("Invalid declination, should be between -90 and 90 degrees\n");
                return;
        }
	if((big_payload[2] < -5) || (big_payload[2] > 5)){
		printf("Invalid offset, should be between -5 and 5 degrees\n");
		return;
	}
	if(big_payload[2] == 0){
		printf("Invalid offset, must be non-zero\n");
		return;
	}
	if(big_payload[3] <= 0){
		printf("Invalid integration time, must be greater than zero\n");
		return;
	}
    	com = enconoff;
    	create_packet(&pkt, com, payload, 0, big_payload, 4, OPH);

    }else if(strcmp(cmd,"stop") == 0){
    
    	com = stop_scan;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, 0);

    }else if((strcmp(cmd,"set_offset_az") == 0) && gs_conf.ls_enabled){
    
    	big_payload = (double_t *)malloc(1*sizeof(double_t));
    	ret = sscanf(arg,"%lf",&big_payload[0]);
	if(ret !=1){
		printf("Invalid format, usage is set_offset_az <az> where az is the current actual azimuth the telescope is at (0,360) in degrees\n");
		return;
	}
	if((big_payload[0]<0) || (big_payload[0]>360)){
                printf("Invalid azimuth, should be between 0 and 360 degrees\n");
                return;
        }

    	com = set_offset_az;
    	create_packet(&pkt, com, payload, 0, big_payload, 1, OPH);
    
    }else if(strcmp(cmd,"set_offset_el") == 0){
    
    	big_payload = (double_t *)malloc(1*sizeof(double_t));
    	ret = sscanf(arg,"%lf",&big_payload[0]);
	if(ret !=1){
                printf("Invalid format, usage is set_offset_el <el> where el is the current actual elevation the telescope is at (0,90) in degrees\n");
                return;
        }
        if((big_payload[0]<0) || (big_payload[0]>90)){
                printf("Invalid elevation, should be between 0 and 90 degrees\n");
                return;
        }

    	com = set_offset_el;
    	create_packet(&pkt, com, payload, 0, big_payload, 1, OPH);

    }else if(strcmp(cmd,"park") == 0){

    	com = park;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, 0);

    }else if(strcmp(cmd,"lockpin_start")==0){
	//expected feedback format: cmd_primary-> system enable, param0 -> lock state
	com = lockpin_start;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);

    }else if(strcmp(cmd,"lockpin_stop")==0){
	//expected feedback format: cmd_primary-> system enable, param0 -> lock state
	com = lockpin_stop;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);

    }else if(strcmp(cmd,"lock_tel")==0){
	
	com = lock_tel_cmd;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);

    }else if(strcmp(cmd,"unlock_tel")==0){
    	
	com = unlock_tel_cmd;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
 
    }else if (strcmp(cmd,"reset_lock")==0){
	com = reset_lock;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);

    }else if(strcmp(cmd,"receiver_start")==0){
    	
	com = receiver_start;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
 
    }else if(strcmp(cmd,"receiver_stop")==0){
    	
	com = receiver_stop;
    	create_packet(&pkt, com, payload, 0, big_payload, 0, OPH);
 
    } else if (strcmp(cmd, "stop_gps") == 0) {
    	com = stop_gps;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "start_gps") == 0) {
    	com = stop_gps;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "start_spec") == 0) {
    	com = start_spec;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "stop_spec") == 0) {
    	com = stop_spec;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "start_spec_120kHz") == 0) {
    	com = start_spec_120kHz;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "stop_spec_120kHz") == 0) {
    	com = stop_spec_120kHz;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "rfsoc_on") == 0) {
    	com = rfsoc_on;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "rfsoc_off") == 0) {
    	com = rfsoc_off;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    }else if (strcmp(cmd, "start_vlbi") == 0) {
    	com = start_vlbi;
        
        // Parse VLBI storage parameter (1 or 2, default to 1)
        int vlbi_storage = 1; // Default to storage 1
        if (scan == 2) { // Command has argument
            vlbi_storage = atoi(arg);
            if (vlbi_storage != 1 && vlbi_storage != 2) {
                printf("Invalid VLBI storage parameter: %s. Use 1 or 2.\n", arg);
                return;
            }
        }
        
        payload = (int16_t*)malloc(sizeof(int16_t));
        payload[0] = (int16_t)vlbi_storage;
        create_packet(&pkt, com, payload, 1, big_payload, 0, SAG);
        free(payload);
    	
    } else if (strcmp(cmd, "stop_vlbi") == 0) {
    	com = stop_vlbi;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "start_backend") == 0) {
    	com = start_backend;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "stop_backend") == 0) {
    	com = stop_backend;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "start_timing_chain") == 0) {
    	com = start_timing_chain;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    } else if (strcmp(cmd, "stop_timing_chain") == 0) {
    	com = stop_timing_chain;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    }else if (strcmp(cmd, "start_ticc") == 0) {
    	com = start_ticc;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    }else if (strcmp(cmd, "stop_ticc") == 0) {
    	com = stop_ticc;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    }else if (strcmp(cmd, "start_heater_box") == 0) {
    	com = start_heater_box;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    	
    }else if (strcmp(cmd,"start_heaters") ==0){
        com = start_heaters;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"stop_heaters") ==0){
        com = stop_heaters;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"start_pr59") ==0){
        com = start_pr59;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"stop_pr59") ==0){
        com = stop_pr59;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"start_pr59_fan") ==0){
        com = start_pr59_fan;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"stop_pr59_fan") ==0){
        com = stop_pr59_fan;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"start_position_box") ==0){
        com = start_position_box;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"stop_position_box") ==0){
        com = stop_position_box;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"position_box_on") ==0){
        com = position_box_on;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else if (strcmp(cmd,"position_box_off") ==0){
        com = position_box_off;
        create_packet(&pkt, com, payload, 0, big_payload, 0, SAG);
    }else {
    	cmd_unknown=1;
        printf("%s: Unknown command\n", cmd);
    }
    
    if(!cmd_unknown){
    	if(confirm_send()){
    		printf("Sending command...\n");
    		send_packet(pkt);
    	}else{
    		printf("Not sending command\n");
    	}
    }
    
    free(arg);
    free(cmd);
}

// This gets an input of arbitrary length from the command line
char* get_input() {
    char* input;
    char c;
    int i = 0;

    input = (char*)malloc(1 * sizeof(char));

    while ((c = getchar()) != '\n' && c != EOF) {
        input[i++] = c;
        input = (char*)realloc(input, i + 1);
    }

    input[i] = '\0';
    return input;
}

// This is the main function for the command line
void cmdprompt() {
    int count = 1;
    char* input;

    while (exiting != 1) {
        printf("[BCP@%s]<%d>$ ", hostname,count);
        input = get_input();
        if (strlen(input) != 0) {
            exec_command(input);
        }
        free(input);
        count++;
    }
}
