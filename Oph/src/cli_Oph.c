#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "file_io_Oph.h"
#include "cli_Oph.h"
#include "bvexcam.h"
#include "lens_adapter.h"
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

FILE* main_log;
FILE* cmd_log;
int exiting = 0;
extern int shutting_down; // set this to one to shutdown star camera
extern int sockfd;
extern int * astro_ptr;
int bvexcam_on = 0;
int lockpin_on = 0;
extern struct camera_params all_camera_params;
extern struct astrometry all_astro_params;
extern FILE* bvexcam_log;
extern FILE* notor_log;
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
// This executes the commands, we can simply add commands by adding if statements
void exec_command(char* input) {
    char* arg;
    char* cmd;
    int scan;
    arg = (char*)malloc(strlen(input) * sizeof(char));
    cmd = (char*)malloc(strlen(input) * sizeof(char));
    scan = sscanf(input, "%s %[^\t\n]", cmd, arg);

    if (strcmp(cmd, "print") == 0) {
        if (scan == 1) {
            printf("print is missing argument usage is print <string>\n");
        } else {
            printf("%s\n", arg);
        }
    } else if (strcmp(cmd, "exit") == 0) {
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
        free(arg);
        free(cmd);
        return; // Exit the command loop immediately

    } else if (strcmp(cmd, "bvexcam_start") == 0){
	if(config.bvexcam.enabled && !bvexcam_on){
                shutting_down = 0;
		printf("Starting bvexcam\n");
        	write_to_log(main_log, "cli_Oph.c", "exec_command", "Starting bvexcam");
		// Put PBoB command here
			set_toggle(config.bvexcam.pbob,config.bvexcam.relay);
		//
		if (bvexcam_log != NULL){
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
        	}else{
			printf("Invalid log file \n");
		}
	}else if(!config.bvexcam.enabled) {
		printf("bvexcam is not enabled\n");
	}else if(bvexcam_on){
		printf("bvexcam already running\n");
	}
    }else if (strcmp(cmd, "bvexcam_stop") == 0){
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
    } else if (strcmp(cmd, "bvexcam_status") == 0) {
        if (config.bvexcam.enabled) {
            SCREEN* s;
            char input = 0;

            s = newterm(NULL, stdin, stdout);
            noecho();
            timeout(500);
            while (input != '\n') {
                if (all_camera_params.focus_mode == 1) {
                    mvprintw(0, 0, "bvexcam mode: Autofocus\n");
                    mvprintw(1, 0, "Auto-focus start: %d\n", all_camera_params.start_focus_pos);
                    mvprintw(2, 0, "Auto-focus stop: %d\n", all_camera_params.end_focus_pos);
                    mvprintw(3, 0, "Auto-focus step: %d\n", all_camera_params.focus_step);
                    mvprintw(4, 0, "Current focus position: %d\n", all_camera_params.focus_position);
		    mvprintw(5, 0, "Exposure Time (msec): %f\n", all_camera_params.exposure_time);
                } else {
                    if(all_camera_params.solve_img){
                    	mvprintw(0, 0, "bvexcam mode: Solving  \b\b\n");
		    }else{
			mvprintw(0, 0, "bvexcam mode: Taking Images\n");
		    }
		    mvprintw(1, 50, "Image saving: %s", all_camera_params.save_image ? "ON" : "OFF");
                    mvprintw(1, 0, "Raw time (sec): %.1f\n", all_astro_params.rawtime);
                    mvprintw(2, 0, "Observed RA (deg): %lf\n", all_astro_params.ra);
                    mvprintw(3, 0, "Observed DEC (deg): %lf\n", all_astro_params.dec);
                    mvprintw(4, 0, "Field rotation (deg): %f\n", all_astro_params.fr);
                    mvprintw(5, 0, "Image rotation (deg): %lf\n", all_astro_params.ir);
                    mvprintw(6, 0, "Pixel scale (arcsec/px): %lf\n", all_astro_params.ps);
                    mvprintw(7, 0, "Altitude (deg): %.15f\n", all_astro_params.alt);
                    mvprintw(8, 0, "Azimuth (deg): %.15f\n", all_astro_params.az);
                    mvprintw(9, 0, "Exposure Time (msec): %f\n", all_camera_params.exposure_time);
		    mvprintw(10,0, "Star Camera Longitude (deg): %f\n", all_astro_params.longitude);
		    mvprintw(11,0, "Star Camera Latitude (deg): %f\n", all_astro_params.latitude);
		    mvprintw(12,0, "Star Camera Altitude (m): %f\n", all_astro_params.hm);
		    
		    // Add starcam downlink status
		    int images_available, server_running, transmission_active;
		    uint32_t bandwidth_usage;
		    time_t latest_timestamp;
		    if (getStarcamStatus(&images_available, &server_running, &bandwidth_usage, &transmission_active, &latest_timestamp)) {
		        mvprintw(13,0, "Downlink Server: %s\n", server_running ? "ACTIVE" : "INACTIVE");
		        mvprintw(14,0, "Images Available: %d\n", images_available);
		        mvprintw(15,0, "Latest Image: %ld\n", latest_timestamp);
		        mvprintw(16,0, "Bandwidth Usage: %u kbps\n", bandwidth_usage);
		        mvprintw(17,0, "Transmission: %s\n", transmission_active ? "ACTIVE" : "IDLE");
		    } else {
		        mvprintw(13,0, "Downlink Server: DISABLED\n");
		    }
                }
                input = getch();
            }
            endwin();
            delscreen(s);
        } else {
            printf("bvexcam is not enabled.\n");
        }
    } else if (strcmp(cmd, "focus_bvexcam") == 0) {
        if (config.bvexcam.enabled) {
            all_camera_params.focus_mode = 1;
            all_camera_params.begin_auto_focus = 1;
        } else {
            printf("bvexcam is not enabled.\n");
        }
    } else if (strcmp(cmd, "bvexcam_solve_start")==0){
	if (config.bvexcam.enabled){
	   all_camera_params.solve_img = 1;
	   printf("bvexcam astrometry solving started.\n");
	}else{
	   printf("bvexcam is not enabled.\n");
	}
    } else if (strcmp(cmd, "bvexcam_solve_stop")==0){
	if (config.bvexcam.enabled){
	   all_camera_params.solve_img = 0;
	   printf("bvexcam astrometry solving stopped.\n");
	}else{
	   printf("bvexcam is not enabled. \n");
	}
    } else if (strcmp(cmd, "bvexcam_save_start")==0){
	if (config.bvexcam.enabled){
	   all_camera_params.save_image = 1;
	   printf("bvexcam image saving started.\n");
	}else{
	   printf("bvexcam is not enabled.\n");
	}
    } else if (strcmp(cmd, "bvexcam_save_stop")==0){
	if (config.bvexcam.enabled){
	   all_camera_params.save_image = 0;
	   printf("bvexcam image saving stopped.\n");
	}else{
	   printf("bvexcam is not enabled.\n");
	}
    }else if (strcmp(cmd, "bvexcam_set_exp")==0){
	double texp;
	if(config.bvexcam.enabled){
	   if(sscanf(arg,"%lf",&texp)==1){
	   	all_camera_params.exposure_time = texp; 
		all_camera_params.change_exposure_bool = 1;
		if (adjustCameraHardware(bvexcam_log)==1){
			printf("Successfully adjusted camera hardware\n");
		}else{
			printf("Error adjusting camera hardware\n");
		}
	   }else{
		printf("Invalid asrgument\n");
	   }
        }else{
		printf("bvexcam is not enabled.\n");

	}

    }else if (strcmp(cmd, "accl_status") == 0) {
        if (config.accelerometer.enabled) {
            AccelerometerStatus status;
            accelerometer_get_status(&status);
            printf("Accelerometer Status:\n");
            for (int i = 0; i < config.accelerometer.num_accelerometers; i++) {
                printf("Accelerometer %d:\n", i + 1);
                printf("  Samples received: %ld\n", status.samples_received[i]);
                printf("  Current chunk: %d\n", status.chunk_numbers[i]);
                printf("  Start time: %.6f\n", status.start_times[i]);
                printf("  Current chunk start time: %.6f\n", status.chunk_start_times[i]);
                printf("\n");
            }
            printf("Accelerometer is %s\n", status.is_running ? "running" : "stopped");
        } else {
            printf("Accelerometer is not enabled.\n");
        }
    } else if (strcmp(cmd, "accl_start") == 0) {
        if (config.accelerometer.enabled) {
            AccelerometerStatus status;
            accelerometer_get_status(&status);
            if (status.is_running) {
                printf("Accelerometer is already running.\n");
            } else {
                if (accelerometer_init() == 0) {
                    printf("Accelerometer data collection started.\n");
                } else {
                    printf("Failed to start accelerometer data collection.\n");
                }
            }
        } else {
            printf("Accelerometer is not enabled in configuration.\n");
        }
    } else if (strcmp(cmd, "accl_stop") == 0) {
        if (config.accelerometer.enabled) {
            AccelerometerStatus status;
            accelerometer_get_status(&status);
            if (status.is_running) {
                accelerometer_shutdown();
                printf("Accelerometer data collection stopped.\n");
            } else {
                printf("Accelerometer is not currently running.\n");
            }
        } else {
            printf("Accelerometer is not enabled in configuration.\n");
        }
    } else if (strcmp(cmd,"motor_start")==0){
	if(config.motor.enabled){
	    if(stop){
		stop = 0;
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
    } else if (strcmp(cmd,"motor_stop")==0){
	if(config.motor.enabled){
            if(!stop){
	    	stop = 1;
		ready = 0;
		comms_ok = 0;
           	printf("Shutting down motor.\n");
            	pthread_join(motors,NULL);
		set_toggle(config.motor.pbob,config.motor.relay);
           	printf("Motor shutdown complete.\n");
	    }else{
		printf("Motor already shutdown.\n");
	    }
	}else{
	    printf("Motor is not enabled in configuration.\n");
	}
    } else if (strcmp(cmd,"motor_control")==0 &&(config.motor.enabled || config.lazisusan.enabled)){
	SCREEN* s;
	char c =0;
	char* input;
	char* cmd;
	char* arg;
	int motor_i = 0;
	int inputlen = 0;
	int ret = 0;
	double az;
	double el;

	s=newterm(NULL,stdin,stdout);
	noecho();
	timeout(100);
	input = (char*)malloc(sizeof(char));
	mvprintw(36,0,"Command:_");

	while(!ret){
		motor_i = GETREADINDEX(motor_index);

		print_motor_data();
                print_motor_PID();
		if(axes_mode.mode == POS){
			mvprintw(14,0,"Commanded pointing(deg): %lf\n", axes_mode.dest);
		}else if(axes_mode.mode == VEL){
			mvprintw(14,0,"Commanded velocity(dps): %lf\n", axes_mode.vel);
		}
		if(scan_mode.scanning){
			if(scan_mode.mode == ENC_DITHER){
				mvprintw(15,0,"Scan mode: Dither Scan\n");
				mvprintw(16,0,"Start elevation(deg): %lf\n", scan_mode.start_el);
				mvprintw(17,0,"Stop elevation(deg): %lf\n", scan_mode.stop_el);
				mvprintw(18,0,"Dither speed(dps): %lf\n", scan_mode.vel);
				mvprintw(19,0,"Number of scans: %d\n", scan_mode.nscans);
				mvprintw(20,0,"Current Scan: %d\n", scan_mode.scan+1);
			}else if(scan_mode.mode == ENC_TRACK){
				mvprintw(15,0,"Scan mode: Track\n");
                                mvprintw(16,0,"Right Ascension (deg): %lf\n", target.lon);
                                mvprintw(17,0,"Declination (deg): %lf\n", target.lat);
                                mvprintw(18,0,"On Target: %d\n", axes_mode.on_target);
                                mvprintw(19,0,"                                    ");
                                mvprintw(20,0,"                                    ");
			}else if(scan_mode.mode == EL_ONOFF){
				mvprintw(15,0,"Scan mode: Elevation On Off\n");
                                mvprintw(16,0,"Right Ascension (deg): %lf\n", target.lon);
                                mvprintw(17,0,"Declination (deg): %lf\n", target.lat);
                                if(scan_mode.on_position == 1){
					mvprintw(18,0,"Position: On\n");
				}else if (scan_mode.on_position == -1){
					mvprintw(18,0,"Position: Off\n");
				}else{
					mvprintw(18,0,"Position: Moving\n");
				}
                                mvprintw(19,0,"Elevation offset (deg): %lf\n",scan_mode.offset);
                                mvprintw(20,0,"Integration time (s): %lf\n",scan_mode.time);
			}
		}else{
			mvprintw(15,0,"                                      ");
                        mvprintw(16,0,"                                      ");
                        mvprintw(17,0,"                                      ");
                        mvprintw(18,0,"                                      ");
                        mvprintw(19,0,"                                      ");
                        mvprintw(20,0,"                                      ");
		}
		if(config.lazisusan.enabled){
			mvprintw(21,0,"Lazisusan angle (degrees): %lf\n",get_angle());
			mvprintw(22,0,"Arduino counts (degrees): %d\n",count_now);
			if(config.gps_server.enabled){
				mvprintw(23,0,"GPS_heading (dgrees): %lf\n",curr_gps.gps_head);
			}
			if (axes_mode.mode==POS){
				mvprintw(24,0,"Lazisusan commanded angle (degrees):%lf\n",axes_mode.dest_az);
			}else if (axes_mode.mode==VEL){
				mvprintw(24,0,"Lazisusan commanded velocity (degrees):%lf\n",axes_mode.vel_az);
			}
			mvprintw(25,0,"Azimuth offset (degrees):%lf\n",az_offset);
			//print_direction();
		}
		if(config.bvexcam.enabled){
			if (all_camera_params.focus_mode == 1) {
                    		mvprintw(26, 0, "bvexcam mode: Autofocus\n");
                    		mvprintw(27, 0, "Auto-focus start: %d\n", all_camera_params.start_focus_pos);
                    		mvprintw(28, 0, "Auto-focus stop: %d\n", all_camera_params.end_focus_pos);
                    		mvprintw(29, 0, "Auto-focus step: %d\n", all_camera_params.focus_step);
                    		mvprintw(30, 0, "Current focus position: %d\n", all_camera_params.focus_position);
                	} else {
                    		mvprintw(26, 0, "bvexcam mode: Solving  \b\b\n");
                    		mvprintw(27, 0, "Raw time (sec): %.1f\n", all_astro_params.rawtime);
                    		mvprintw(28, 0, "Observed RA (deg): %lf\n", all_astro_params.ra);
                    		mvprintw(29, 0, "Observed DEC (deg): %lf\n", all_astro_params.dec);
                    		mvprintw(30, 0, "Field rotation (deg): %f\n", all_astro_params.fr);
                    		mvprintw(31, 0, "Image rotation (deg): %lf\n", all_astro_params.ir);
                    		mvprintw(32, 0, "Pixel scale (arcsec/px): %lf\n", all_astro_params.ps);
                    		mvprintw(33, 0, "Altitude (deg): %.15f\n", all_astro_params.alt);
                    		mvprintw(34, 0, "Azimuth (deg): %.15f\n", all_astro_params.az);
                	}
                	mvprintw(35, 0, "Image saving: %s | Astrometry: %s", 
                         all_camera_params.save_image ? "ON" : "OFF",
                         all_camera_params.solve_img ? "ON" : "OFF");
		}

		c = getch();
		if((c != '\n') && (c != EOF)){
			mvprintw(36,8+inputlen,"%c_",c);
			input[inputlen++] = c;
			input = (char*)realloc(input,inputlen+1);
		}else if(c == '\n'){
			input[inputlen] = '\0';
			mvprintw(36,8,"_");
			for(int i = 1; i<inputlen+1; i++){
				mvprintw(36,8+i," ");
			}

			cmd = (char*) malloc(strlen(input) * sizeof(char));
			arg = (char*) malloc(strlen(input) * sizeof(char));

			sscanf(input, "%s %[^\t\n]", cmd, arg);

			if(strcmp(cmd,"exit") == 0){
				ret = 1;
			}else if(strcmp(cmd,"gotoenc") == 0){
				scan_mode.scanning = 0;
				if(config.lazisusan.enabled && config.motor.enabled){
					sscanf(arg, "%lf,%lf",&az,&el);
					go_to_enc(el);
					move_to(az);
				}else if(!config.lazisusan.enabled){
					go_to_enc(atof(arg));
				}else if(!config.motor.enabled){
					move_to(atof(arg));
				}
			}else if(strcmp(cmd,"encdither") == 0){
				sscanf(arg, "%lf,%lf,%lf,%d",&scan_mode.start_el, &scan_mode.stop_el, &scan_mode.vel, &scan_mode.nscans);
				scan_mode.mode = ENC_DITHER;
				scan_mode.scanning = 1;
			}else if(strcmp(cmd,"enctrack") == 0){
				sscanf(arg,"%lf,%lf",&target.lon,&target.lat);
				scan_mode.mode = ENC_TRACK;
				scan_mode.scanning = 1;
			}else if(strcmp(cmd,"enconoff")==0){
				sscanf(arg,"%lf,%lf,%lf,%lf",&target.lon,&target.lat,&scan_mode.offset,&scan_mode.time);
				scan_mode.mode = EL_ONOFF;
				scan_mode.scanning = 1;
			}else if(strcmp(cmd,"stop") == 0){
				scan_mode.scanning = 0;
				scan_mode.nscans = 0;
				scan_mode.scan = 0;
				scan_mode.turnaround = 1;
				scan_mode.time = 0;
				scan_mode.on_position = 0;
				scan_mode.offset = 0;
				axes_mode.mode = VEL;
				axes_mode.vel = 0.0;
				axes_mode.vel_az = 0.0;
				axes_mode.on_target = 0;
			}else if(strcmp(cmd,"set_offset_az") == 0){
				if(config.lazisusan.enabled){
                                        set_offset(atof(arg));
				}
			}else if(strcmp(cmd,"set_offset_el") == 0){
				if(config.motor.enabled){
					set_el_offset(atof(arg));
				}
			}else if (strcmp(cmd,"park")==0){
				go_to_park();
			}
			input = (char*)malloc(sizeof(char));
			inputlen = 0;
		}
	}
	endwin();
	delscreen(s);
    }else if(strcmp(cmd,"lockpin_start")==0){
	if (config.lockpin.enabled){
		if(!lockpin_on){
			exit_lock = 0;
			printf("Starting Lockpin....\n");
			write_to_log(main_log,"cli_Oph.c","exec_command","Starting lockpin");
			//PBoB command goes here

			//
			pthread_create(&lock_thread,NULL,do_lockpin,NULL);
			while(!lockpin_ready){
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
    }else if(strcmp(cmd,"lockpin_stop")==0){
	if (config.lockpin.enabled){
		if(lockpin_on){
			printf("Shutting down lockpin\n");
        		write_to_log(main_log,"cli_Oph.c","exec_command","Shutting down lockpin");
        		exit_lock = 1;
        		pthread_join(lock_thread,NULL);
			//PBoB command goes here

			//
        		printf("Lockpin shutdown complete\n");
       		 	write_to_log(main_log,"cli_Oph.c","exec_command","Lockpin shutdown complete");

		}else{
			printf("Lockpin already shutdown\n");
		}
	}else{
		printf("Lockpin not enabled\n");
	}
    }else if(strcmp(cmd,"lock_tel")==0){
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

    }else if(strcmp(cmd,"unlock_tel")==0){
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
    }else if (strcmp(cmd,"reset_lock")==0){
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

    }else if (strcmp(cmd,"print_gps")==0){
	if(config.gps_server.enabled){
		printf("Longitude:%f\n",curr_gps.gps_lat);
		printf("Latitude:%f\n",curr_gps.gps_lon);
		printf("Altitude:%f\n",curr_gps.gps_alt);
		printf("Heading:%f\n",curr_gps.gps_head);
	}else{
		printf("GPS server not enabled\n");
	}

    }else if (strcmp(cmd,"starcam_downlink_status")==0){
	int images_available, server_running, transmission_active;
	uint32_t bandwidth_usage;
	time_t latest_timestamp;
	if (getStarcamStatus(&images_available, &server_running, &bandwidth_usage, &transmission_active, &latest_timestamp)) {
		printf("Starcam Downlink Status:\n");
		printf("  Server: %s\n", server_running ? "ACTIVE" : "INACTIVE");
		printf("  Images Available: %d\n", images_available);
		printf("  Latest Image Timestamp: %ld\n", latest_timestamp);
		printf("  Bandwidth Usage: %u kbps\n", bandwidth_usage);
		printf("  Current Transmission: %s\n", transmission_active ? "ACTIVE" : "IDLE");
	} else {
		printf("Starcam downlink is not enabled.\n");
	}

    }else{
        printf("%s: Unknown command\n", cmd);
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
        printf("[BCP@Ophiuchus]<%d>$ ", count);
        input = get_input();
        if (strlen(input) != 0) {
            write_to_log(cmd_log, "cli.c", "cmdprompt", input);
            exec_command(input);
        }
        free(input);
        count++;
    }
}
