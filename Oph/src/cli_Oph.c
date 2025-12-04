#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>
#include <time.h>

#include "file_io_Oph.h"
#include "cli_Oph.h"
#include "bvexcam.h"
#include "lens_adapter.h"
extern int housekeeping_on;
#include "astrometry.h"
#include "accelerometer.h"
#include "ec_motor.h"
#include "motor_control.h"
#include "lazisusan.h"
#include "coords.h"
#include "lockpin.h"
#include "gps_server.h"
#include "starcam_downlink.h"
#include "pbob.h"
#include "housekeeping.h"

FILE* main_log;
FILE* cmd_log;
extern int housekeeping_on;
extern int housekeeping_running;
extern int stop_housekeeping;
int exiting = 0;
extern int shutting_down; // set this to one to shutdown star camera
extern int sockfd;
extern int * astro_ptr;
int bvexcam_on = 0;
int lockpin_on = 0;
extern struct camera_params all_camera_params;
extern struct astrometry all_astro_params;
extern FILE* bvexcam_log;
extern FILE* motor_log;
extern AccelerometerData accel_data;
extern int stop;//flag that determines on/off state of motor
extern pthread_t motors;
extern AxesModeStruct axes_mode;
extern ScanModeStruct scan_mode;
extern int motor_index;
extern int ready; //This determines if the motor setup has completed
extern int comms_ok; //This determines if the motor setup was successful
extern SkyCoord target;
extern int lock_tel;
extern int unlock_tel;
extern int is_locked;
extern int exit_lock;
extern int reset;
extern int stop_server;
extern GPS_data curr_gps;
extern double az_offset;
extern int count_now;
extern pthread_t astro_thread_id;
pthread_t lock_thread;
extern RelayController controller[NUM_PBOB];
int receiver_on = 0;
struct sockaddr_in cliaddr_cmd;
Packet pkt;
enum commands com;
int cmd_count = 0;
// This executes the commands, we can simply add commands by adding if statements

int init_cmd_socket(){
        int sockfd = socket(AF_INET,SOCK_DGRAM,0);
        struct sockaddr_in servaddr;
        struct timeval tv;

        tv.tv_sec = config.cmd_server.timeout;
        tv.tv_usec = 0;

        if(sockfd < 0){
                printf("Socket creation failed\n");
        }else{
                memset(&servaddr,0, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = htons(config.cmd_server.port);
                servaddr.sin_addr.s_addr = INADDR_ANY;

                if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
                        printf("Socket bind failed\n");
                        close(sockfd);
                        return -1;
                }
                setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        }

        return sockfd;
}

int cmd_sock_listen(int sockfd, char* buffer){
        int n;
        socklen_t cliaddr_len = sizeof(cliaddr_cmd);

        n = recvfrom(sockfd, buffer, MAXLEN, MSG_WAITALL, (struct sockaddr *) &cliaddr_cmd, &cliaddr_len);

        if(n > 0){
                buffer[n] = '\0';
        }else{
                buffer[0] = '\0';
                //if(n < 0){
                        //printf("Error receiving data\n");
                //}
        }

        return n;
}

void cmd_sock_send(int sockfd){
        char msg[10];

        snprintf(msg,10,"%d",cmd_count);
        sendto(sockfd, (const char*) msg, strlen(msg), MSG_CONFIRM,(const struct sockaddr *) &cliaddr_cmd, sizeof(cliaddr_cmd));
        return;

}

uint8_t compute_checksum(const uint8_t* bytes, const size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum ^= bytes[i];
    }
    return sum;
}

int verify_packet(const Packet* pkt) {

        size_t header_size = 8 + pkt->num_data * sizeof(int16_t) + pkt->num_bigpack * sizeof(double_t); // fixed + data
        uint8_t expected = compute_checksum((uint8_t*)pkt, header_size);

        if (expected != pkt->checksum) {
                printf("ERROR: Checksum mismatch: expected 0x%02X, got 0x%02X\n", expected, pkt->checksum);
                return 0;
        }
        return 1;
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

int decode_msg(Packet* pkt, char* msg){
        char big_buf[MAXLEN];
        char small_buf[MAXLEN];
        char* fmt;
        int ret;
        int len_count = 0;

        uint8_t start;
        uint8_t num_data;
        uint8_t num_bigpack;
        int16_t* data;
        double_t* bigpack;
        uint32_t utc;
        uint8_t cmd_primary;
        uint8_t checksum;
        uint8_t end;

        ret = sscanf(msg,"%hhd\n%hhd\n%d\n%s\n%hhd\n%s\n%hhd\n%hhd\n%hhd\n",&start, &cmd_primary, &utc, small_buf,&num_data, big_buf,&num_bigpack,&checksum,&end);

        clear_packet(pkt);
        pkt->start = start;
        pkt->num_data = num_data;
        pkt->num_bigpack = num_bigpack;
        pkt->cmd_primary = cmd_primary;
        pkt->utc = utc;
        pkt->checksum = checksum;
        pkt->end = end;

        if(ret != 9){

                printf("Received corrupt string\n");
                return 0;
        }

        if(pkt->start != START_BYTE){
                printf("Missing start byte\n");
                return 0;
        }

        if(end != END_BYTE){
                printf("Missing stop byte\n");
                return 0;
        }

	if (pkt->num_data>0){ 
                data = (int16_t *) malloc(sizeof(int16_t)*pkt->num_data);
                fmt=strtok(small_buf,",");
                while(fmt != NULL){
                        if(len_count==pkt->num_data){
                                printf("Too many arguments in data\n");
                                return 0;
                        }

                        data[len_count]=atoi(fmt);
                        fmt = strtok(NULL, ",");
                        len_count++;

                }

                if(len_count < (pkt->num_data-1)){

                        printf("Too few arguments in data\n");
                        return 0;

                }
                memcpy(pkt->data, data, pkt->num_data * sizeof(int16_t));
        }

	if (pkt->num_bigpack>0){
                bigpack = (double_t *) malloc(sizeof(double_t)*pkt->num_bigpack);
                len_count = 0;
                fmt=strtok(big_buf,",");
                while(fmt != NULL){
                        if(len_count==pkt->num_bigpack){
                                printf("Too many arguments in bigpack \n");
                                return 0;
                        }

                        bigpack[len_count]=atof(fmt);
                        fmt = strtok(NULL, ",");
                        len_count++;

                }

                if(len_count < (pkt->num_data-1)){

                        printf("Too few arguments in bigpack\n");
                        return 0;

                }
                memcpy(pkt->bigpack, bigpack,pkt->num_bigpack * sizeof(double_t));


        }
	//print_packet(*pkt);
	//printf("%s\n",msg);
        if(!verify_packet(pkt)){
                return 0;
        }


        return 1;

}

void exec_command(Packet pkt) {

    int flen = 1024;
    char fname[flen];

    if (pkt.cmd_primary==exit_both) {
        exiting = 1;
        printf("Exiting BCP\n");

        if (config.bvexcam.enabled) {
            shutting_down = 1;
            printf("Shutting down bvexcam\n");
        }

        if (config.accelerometer.enabled) {
            printf("Shutting down accelerometer\n");
            accelerometer_shutdown();
        }
        return; // Exit the command loop immediately

    } else if (pkt.cmd_primary==bvexcam_start){
	if(config.bvexcam.enabled && !bvexcam_on){
                shutting_down = 0;
		printf("Starting bvexcam\n");
        	write_to_log(main_log, "cli_Oph.c", "exec_command", "Starting bvexcam");
		// Put PBoB command here
			set_toggle(config.bvexcam.pbob,config.bvexcam.relay);
		//
		printf("Starting bvexcam log\n");
        	write_to_log(main_log, "cli_Oph.c", "exec_command", "Starting bvexcam log");
		snprintf(fname,flen,"%s/bvexcam_%ld.log",config.bvexcam.logfile,time(NULL));
    		bvexcam_log = fopen(fname, "w");

        	if (bvexcam_log == NULL) {
               		printf("Error opening bvexcam log %s: No such file or directory\n", config.bvexcam.logfile);
                     	write_to_log(main_log, "main_Oph.c", "main", "Error opening bvexcam log: No such file or directory");
        	}else{
            		init_bvexcam(bvexcam_log);

            // start star camera pthread
            		if (pthread_create(&astro_thread_id, NULL, run_bvexcam, (void *)bvexcam_log)) {
                		fprintf(stderr, "Error creating Astrometry thread: %s.\n", strerror(errno));
                		printf("Starting bvexcam was not successful.\n");
                		close(sockfd);
            		} else {
				bvexcam_on = 1;
                		printf("Successfully started bvexcam.\n");
            		}
        	}
	}else if(!config.bvexcam.enabled) {
		printf("bvexcam is not enabled\n");
	}else if(bvexcam_on){
		printf("bvexcam already running\n");
	}
    }else if (pkt.cmd_primary == bvexcam_stop){
        if (config.bvexcam.enabled && bvexcam_on) {
            shutting_down = 1;
            pthread_join(astro_thread_id, (void **) &(astro_ptr));
            if (*astro_ptr == 1) {
                bvexcam_on = 0;
                printf("Successfully shut down bvexcam.\n");
                write_to_log(main_log, "cli_Oph.c", "exec_command", "Successfully shut down bvexcam");
            } else {
                printf("bvexcam shut down unsuccessful.\n");
                write_to_log(main_log, "cli_Oph.c", "exec_command", "bvexcam shut down unsuccessful");
            }
	    set_toggle(config.bvexcam.pbob,config.bvexcam.relay);
    	}else if (!config.bvexcam.enabled){
		printf("bvexcam not enabled");
	}else if (!bvexcam_on){
		printf("bvexcam already shutdown");
	}
	fclose(bvexcam_log);
    } else if (pkt.cmd_primary == focus_bvexcam) {
        if (config.bvexcam.enabled) {
            all_camera_params.focus_mode = 1;
            all_camera_params.begin_auto_focus = 1;
	    printf("bvexcam auto-focus started\n");
        } else {
            printf("bvexcam is not enabled.\n");
        }
    } else if (pkt.cmd_primary == bvexcam_set_focus_inf){

	if(config.bvexcam.enabled){
		all_camera_params.focus_inf = 1;
		if (adjustCameraHardware(bvexcam_log)==1){
            		printf("Successfully adjusted camera hardware\n");
        	}else{
            		printf("Error adjusting camera hardware\n");
        	}
	}else{
		printf("bvexcam is not enabled\n");
	}

    } else if(pkt.cmd_primary == bvexcam_set_focus){
	if(config.bvexcam.enabled){
		all_camera_params.focus_position = pkt.data[0];
		if (adjustCameraHardware(bvexcam_log)==1){
                        printf("Successfully adjusted camera hardware\n");
                }else{
                        printf("Error adjusting camera hardware\n");
                }
	}else{
                printf("bvexcam is not enabled\n");
        }
    } else if (pkt.cmd_primary == bvexcam_solve_start){
	if (config.bvexcam.enabled){
	   all_camera_params.solve_img = 1;
	   printf("bvexcam astrometry solving started.\n");
	}else{
	   printf("bvexcam is not enabled.\n");
	}
    } else if (pkt.cmd_primary == bvexcam_solve_stop){
	if (config.bvexcam.enabled){
	   all_camera_params.solve_img = 0;
	   printf("bvexcam astrometry solving stopped.\n");
	}else{
	   printf("bvexcam is not enabled. \n");
	}
    } else if (pkt.cmd_primary == bvexcam_save_start){
	if (config.bvexcam.enabled){
	   all_camera_params.save_image = 1;
	   printf("bvexcam image saving started.\n");
	}else{
	   printf("bvexcam is not enabled.\n");
	}
    } else if (pkt.cmd_primary == bvexcam_save_stop){
	if (config.bvexcam.enabled){
	   all_camera_params.save_image = 0;
	   printf("bvexcam image saving stopped.\n");
	}else{
	   printf("bvexcam is not enabled.\n");
	}
    }else if (pkt.cmd_primary == bvexcam_set_exp){
	double texp;
	if(config.bvexcam.enabled){
	    all_camera_params.exposure_time = pkt.bigpack[0];
            all_camera_params.change_exposure_bool = 1;
	    if (adjustCameraHardware(bvexcam_log)==1){
		printf("Successfully adjusted camera hardware\n");
	    }else{
		printf("Error adjusting camera hardware\n");
	    }
        }else{
		printf("bvexcam is not enabled.\n");

	}

    }else if (pkt.cmd_primary == motor_start){
	if(config.motor.enabled){
	    if(stop){
		stop = 0;
		printf("Starting motor log.\n");
        	write_to_log(main_log,"main_Oph.c","main","Starting motor log");
        	snprintf(fname,flen,"%s/motor_%ld.log",config.motor.logfile,time(NULL));
                motor_log = fopen(fname, "w");

        	if(motor_log == NULL){
                	printf("Error opening motor log %s: No such file or directory\n", config.motor.logfile);
                	write_to_log(main_log, "main_Oph.c", "main", "Error opening motor log: No such file or directory");
        	}

		printf("Starting motor\n");
		write_to_log(main_log,"cli_Oph.c","exec_command","Starting motors");
		if(motor_log != NULL){
			//Put PBoB command here
			set_toggle(config.motor.pbob,config.motor.relay);
			//
			usleep(5000000);
			if (start_motor()){
				printf("Successfully started motor\n");
				write_to_log(main_log,"cli_Oph.c","exec_command","Motor startup successful");
			}else{
				printf("Error starting motor please see motor log\n");
				write_to_log(main_log,"cli_Oph.c","exec_command","Error starting up motor see motor log");
			}
		}else{
			printf("Invalid log file\n");

		}
            }else{
                printf("Motor is already running\n");

            }
	}else{
		printf("Motor is not enabled in configuration.\n");
	}
    } else if (pkt.cmd_primary == motor_stop){
	if(config.motor.enabled){
            if(!stop){
	    	stop = 1;
		ready = 0;
		comms_ok = 0;
           	printf("Shutting down motor.\n");
            	pthread_join(motors,NULL);
		set_toggle(config.motor.pbob,config.motor.relay);
           	printf("Motor shutdown complete.\n");
		fclose(motor_log);
	    }else{
		printf("Motor already shutdown.\n");
	    }
	}else{
	    printf("Motor is not enabled in configuration.\n");
	}
    }else if(pkt.cmd_primary == gotoenc){
	scan_mode.scanning = 0;
	if(config.lazisusan.enabled && config.motor.enabled){
		go_to_enc(pkt.bigpack[1]);
		move_to(pkt.bigpack[0]);
	}else if(!config.lazisusan.enabled){
		go_to_enc(pkt.bigpack[0]);
	}
    }else if(pkt.cmd_primary == encdither){

	scan_mode.start_el = pkt.bigpack[0];
	scan_mode.stop_el = pkt.bigpack[1];
	scan_mode.vel = pkt.bigpack[2];
	scan_mode.nscans = pkt.data[0];
	scan_mode.mode = ENC_DITHER;
	scan_mode.scanning = 1;

    }else if(pkt.cmd_primary == enctrack){

	target.lon = pkt.bigpack[0];
	target.lat = pkt.bigpack[1];
	scan_mode.mode = ENC_TRACK;
	scan_mode.scanning = 1;

    }else if(pkt.cmd_primary == enconoff){

	target.lon = pkt.bigpack[0];
	target.lat = pkt.bigpack[1];
	scan_mode.offset = pkt.bigpack[2];
	scan_mode.time = pkt.bigpack[3];
	scan_mode.mode = EL_ONOFF;
	scan_mode.scanning = 1;

    }else if(pkt.cmd_primary == trackdither){
	target.lon = pkt.bigpack[0];
	target.lat = pkt.bigpack[1];
	scan_mode.scan_len = pkt.bigpack[2];
	scan_mode.vel = pkt.bigpack[3];
	scan_mode.nscans = pkt.data[0];
	scan_mode.mode = TRACK_DITHER;
	scan_mode.scanning = 1;
    }else if(pkt.cmd_primary == stop_scan){
	scan_mode.scanning = 0;
        scan_mode.mode = NONE;
	scan_mode.nscans = 0;
	scan_mode.scan = 0;
	scan_mode.turnaround = 1;
	scan_mode.time = 0;
	scan_mode.on_position = 0;
	scan_mode.offset = 0;
	scan_mode.firsttime = 1;
	axes_mode.mode = VEL;
	axes_mode.vel = 0.0;
	axes_mode.vel_az = 0.0;
	axes_mode.on_target = 0;

    }else if(pkt.cmd_primary == set_offset_az){

	if(config.lazisusan.enabled){
              set_offset(pkt.bigpack[0]);
	}
    }else if(pkt.cmd_primary == set_offset_el){
	if(config.motor.enabled){
              set_el_offset(pkt.bigpack[0]);
	}
    }else if (pkt.cmd_primary == park){
	go_to_park();
    }else if(pkt.cmd_primary == lockpin_start){
	if (config.lockpin.enabled){
		if(!lockpin_on){
			exit_lock = 0;
			printf("Starting Lockpin....\n");
			write_to_log(main_log,"cli_Oph.c","exec_command","Starting lockpin");
			//PBoB command goes here
			set_toggle(config.lockpin.pbob,config.lockpin.relay);
			//
			pthread_create(&lock_thread,NULL,do_lockpin,NULL);
			while(!lockpin_on){
                 		if(lockpin_ready){
                        		printf("Successfully started lockpin\n");
                        		write_to_log(main_log,"cli_Oph.c","exec_command","Successfully started lockpin");
					lockpin_on = 1;
                        		break;
                		}
        		}
		}else{
			printf("Lockpin already running\n");
		}
	}else{
		printf("Lockpin not enabled\n");
	}
    }else if(pkt.cmd_primary == lockpin_stop){
	if (config.lockpin.enabled){
		if(lockpin_on){
			printf("Shutting down lockpin\n");
        		write_to_log(main_log,"cli_Oph.c","exec_command","Shutting down lockpin");
        		exit_lock = 1;
        		pthread_join(lock_thread,NULL);
			//PBoB command goes here
			set_toggle(config.lockpin.pbob,config.lockpin.relay);
			//
			lockpin_on = 0;
        		printf("Lockpin shutdown complete\n");
       		 	write_to_log(main_log,"cli_Oph.c","exec_command","Lockpin shutdown complete");

		}else{
			printf("Lockpin already shutdown\n");
		}
	}else{
		printf("Lockpin not enabled\n");
	}
    }else if(pkt.cmd_primary == lock_tel_cmd){
	if(config.lockpin.enabled){
		if(lockpin_on && !is_locked){
			lock_tel = 1;
			printf("Locking telescope...\n");
			while(1){
				if(is_locked){
					printf("Telescope locked\n");
					break;
				}
			}
		}else if(!lockpin_on){
			printf("Lockpin not running\n");
		}else if(is_locked){
			printf("Telescope already locked\n");
		}
	}else{
		printf("Lockpin not enabled\n");
	}

    }else if(pkt.cmd_primary == unlock_tel_cmd){
	if(config.lockpin.enabled){
		if(lockpin_on && is_locked){
			unlock_tel = 1;
			printf("Unlocking telescope...\n");
			while(1){
				if(!is_locked){
					printf("Telescope unlocked\n");
					break;
				}
			}
		}else if(!lockpin_on){
			printf("Lockpin not running\n");
		}else if(!is_locked){
			printf("Telescope already unlocked\n");
		}
	}else{
		printf("Lockpin not enabled\n");
	}
    }else if (pkt.cmd_primary == reset_lock){
	if(config.lockpin.enabled){
		if(lockpin_on){
			reset = 1;
			printf("Resetting lockpin...\n");
			while(1){
				if(!reset){
					printf("Lockpin reset");
					break;
				}
			}
		}else{
			printf("Lockpin not running\n");
		}
	}else{
		printf("Lockpin not enabled\n");
	}

    }else if (pkt.cmd_primary == receiver_start){
	if(config.lna.enabled && config.mixer.enabled){
		if(!receiver_on){
			printf("Powering on receiver");
                        write_to_log(main_log,"cli_Oph.c","exec_command","Powering on receiver");
			set_toggle(config.lna.pbob,config.lna.relay);
			set_toggle(config.mixer.pbob,config.mixer.relay);
			receiver_on = 1;
		}else{
			printf("Receiver already powered on\n");
		}
	}else{
		printf("Receiver not enabled\n");
	}
    }else if (pkt.cmd_primary == receiver_stop){
	if(config.lna.enabled && config.mixer.enabled){
		if(receiver_on){
			printf("Powering on receiver");
                        write_to_log(main_log,"cli_Oph.c","exec_command","Powering on receiver");
			set_toggle(config.lna.pbob,config.lna.relay);
			set_toggle(config.mixer.pbob,config.mixer.relay);
			receiver_on = 0;
		}else{
			printf("Receiver already powered off\n");
		}
	}else{
		printf("Receiver not enabled\n");
	}
    }else if (pkt.cmd_primary == cmd_housekeeping_on){
	if(config.housekeeping.enabled){
		printf("Starting housekeeping log....\n");
        	write_to_log(main_log,"main_Oph.c","main","Starting housekeeping log");
		snprintf(fname,flen,"%s/housekeeping_%ld.log",config.housekeeping.logfile,time(NULL));
        	housekeeping_log = fopen(fname,"w");
        	if(housekeeping_log == NULL){
                	printf("Error starting housekeeping log %s: No such file or directory \n", config.housekeeping.logfile);
                	write_to_log(main_log, "main_Oph.c", "main", "Error opening housekeeping log: No such file or directory");
        	}else{
                	printf("Successfully started housekeeping log\n");
                	write_to_log(main_log,"main_Oph.c","main","Successfully started housekeeping log");
               		write_to_log(housekeeping_log,"main_Oph.c","main","Housekeeping log initialized");
        	}
		if(!housekeeping_on){
			printf("Powering on housekeeping system\n");
			write_to_log(main_log,"cli_Oph.c","exec_command","Powering on housekeeping system");
			set_toggle(config.housekeeping.pbob,config.housekeeping.relay);
			housekeeping_on = 1;
			printf("Housekeeping system powered on\n");
		}else{
			printf("Housekeeping system already powered on\n");
		}
	}else{
		printf("Housekeeping system not enabled\n");
	}
    }else if (pkt.cmd_primary == cmd_housekeeping_off){
	if(config.housekeeping.enabled){
		if(housekeeping_on){
			// Stop data collection first if running
			if(housekeeping_running){
				printf("Stopping housekeeping data collection before power off\n");
				write_to_log(main_log,"cli_Oph.c","exec_command","Stopping housekeeping data collection before power off");
				stop_housekeeping = 1;
				pthread_join(housekeeping_thread, NULL);
				stop_housekeeping = 0;
				printf("Housekeeping data collection stopped\n");
			}
			
			printf("Powering off housekeeping system\n");
			write_to_log(main_log,"cli_Oph.c","exec_command","Powering off housekeeping system");
			set_toggle(config.housekeeping.pbob,config.housekeeping.relay);
			housekeeping_on = 0;
			printf("Housekeeping system powered off\n");
			fclose(housekeeping_log);
		}else{
			printf("Housekeeping system already powered off\n");
		}
	}else{
		printf("Housekeeping system not enabled\n");
	}
    }else if (pkt.cmd_primary == cmd_start_housekeeping){
	if(config.housekeeping.enabled){
		if(housekeeping_on && !housekeeping_running){
			printf("Starting housekeeping data collection\n");
			write_to_log(main_log,"cli_Oph.c","exec_command","Starting housekeeping data collection");
			
			// Initialize housekeeping system
			if(init_housekeeping_system() == 0){
				// Start housekeeping thread
				if(pthread_create(&housekeeping_thread, NULL, run_housekeeping_thread, NULL) == 0){
					// Wait for thread to start properly
					sleep(2);
					if(housekeeping_running){
						printf("Housekeeping data collection started successfully\n");
						write_to_log(main_log,"cli_Oph.c","exec_command","Housekeeping data collection started successfully");
					}else{
						printf("Failed to start housekeeping data collection\n");
						write_to_log(main_log,"cli_Oph.c","exec_command","Failed to start housekeeping data collection");
					}
				}else{
					printf("Failed to create housekeeping thread\n");
					write_to_log(main_log,"cli_Oph.c","exec_command","Failed to create housekeeping thread");
				}
			}else{
				printf("Failed to initialize housekeeping system\n");
				write_to_log(main_log,"cli_Oph.c","exec_command","Failed to initialize housekeeping system");
			}
		}else if(!housekeeping_on){
			printf("Housekeeping system not powered on. Use housekeeping_on command first.\n");
		}else if(housekeeping_running){
			printf("Housekeeping data collection already running\n");
		}
	}else{
		printf("Housekeeping system not enabled\n");
	}
    }else if (pkt.cmd_primary == cmd_stop_housekeeping){
	if(config.housekeeping.enabled){
		if(housekeeping_running){
			printf("Stopping housekeeping data collection\n");
			write_to_log(main_log,"cli_Oph.c","exec_command","Stopping housekeeping data collection");
			stop_housekeeping = 1;
			pthread_join(housekeeping_thread, NULL);
			stop_housekeeping = 0;
			printf("Housekeeping data collection stopped\n");
			write_to_log(main_log,"cli_Oph.c","exec_command","Housekeeping data collection stopped");
		}else{
			printf("Housekeeping data collection not running\n");
		}
	}else{
		printf("Housekeeping system not enabled\n");
	}
    }else if (pkt.cmd_primary == set_lon){
	curr_gps.gps_lat = pkt.bigpack[0];
    }else if (pkt.cmd_primary == set_lat){
        curr_gps.gps_lon = pkt.bigpack[0];
    }else if (pkt.cmd_primary == set_alt){
        curr_gps.gps_alt = pkt.bigpack[0];
    }else if (pkt.cmd_primary == set_motor_P){
	config.motor.velP = pkt.bigpack[0];
    }else if (pkt.cmd_primary == set_motor_I){
        config.motor.velI = pkt.bigpack[0];
    }else if (pkt.cmd_primary == set_motor_D){
        config.motor.velD = pkt.bigpack[0];
    }else if (pkt.cmd_primary == set_motor_Gv){ 
        config.motor.vel_gain = pkt.bigpack[0];
    }else if (pkt.cmd_primary == set_motor_Imax){ 
        config.motor.max_current = pkt.data[0];
    }else if (pkt.cmd_primary == set_lock_duration){
	config.lockpin.duration = pkt.data[0];
    }
}

void do_commands(){

        int cmd_sock;
        char buffer[MAXLEN];
        Packet pkt;

        cmd_sock = init_cmd_socket();

        if(cmd_sock < 0){
                printf("Socket creation failed\n");
                return;
        }

        while(!exiting){
                if(cmd_sock_listen(cmd_sock,buffer)<0){
                        continue;
                }
                if(!decode_msg(&pkt,buffer)){
                        printf("Failed to decode message\n");

                }else{
                        cmd_count++;
                        cmd_sock_send(cmd_sock);
                        exec_command(pkt);
                }
        }

        printf("Exiting\n");
        close(cmd_sock);
}

