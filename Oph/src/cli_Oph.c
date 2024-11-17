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

int exiting = 0;
extern int shutting_down; // set this to one to shutdown star camera
extern struct camera_params all_camera_params;
extern struct astrometry all_astro_params;
extern AccelerometerData accel_data;
extern int stop;//flag that determines on/off state of motor
extern pthread_t motors;
extern AxesModeStruct axes_mode;
extern ScanModeStruct scan_mode;
extern int motor_index;
extern int ready; //This determines if the motor setup has completed
extern int comms_ok; //This determines if the motor setup was successful
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
                } else {
                    mvprintw(0, 0, "bvexcam mode: Solving  \b\b\n");
                    mvprintw(1, 0, "Raw time (sec): %.1f\n", all_astro_params.rawtime);
                    mvprintw(2, 0, "Observed RA (deg): %lf\n", all_astro_params.ra);
                    mvprintw(3, 0, "Observed DEC (deg): %lf\n", all_astro_params.dec);
                    mvprintw(4, 0, "Field rotation (deg): %f\n", all_astro_params.fr);
                    mvprintw(5, 0, "Image rotation (deg): %lf\n", all_astro_params.ir);
                    mvprintw(6, 0, "Pixel scale (arcsec/px): %lf\n", all_astro_params.ps);
                    mvprintw(7, 0, "Altitude (deg): %.15f\n", all_astro_params.alt);
                    mvprintw(8, 0, "Azimuth (deg): %.15f\n", all_astro_params.az);
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
    } else if (strcmp(cmd, "accl_status") == 0) {
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
		if (start_motor()){
			printf("Successfully started motor\n");
		}else{
			printf("Error starting motor please see motor log\n");
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
           	printf("Motor shutdown complete.\n");
	    }else{
		printf("Motor already shutdown.\n");
	    }
	}else{
	    printf("Motor is not enabled in configuration.\n");
	}
    } else if (strcmp(cmd,"motor_control")==0){
	SCREEN* s;
	char c =0;
	char* input;
	char* cmd;
	char* arg;
	int motor_i = 0;
	int inputlen = 0;
	int ret = 0;

	s=newterm(NULL,stdin,stdout);
	noecho();
	timeout(100);
	input = (char*)malloc(sizeof(char));
	mvprintw(20,0,"Command:_");

	while(!ret){
		motor_i = GETREADINDEX(motor_index);

		print_motor_data();
		if((axes_mode.mode == POS) && !scan_mode.scanning){
			mvprintw(11,0,"Commanded pointing(deg): %lf\n", axes_mode.dest);
			mvprintw(12,0,"                                      ");
			mvprintw(13,0,"                                      ");
                        mvprintw(14,0,"                                      ");
			mvprintw(15,0,"                                      ");
			mvprintw(16,0,"                                      ");
			mvprintw(17,0,"                                      ");
                        mvprintw(18,0,"                                      ");
		}else if((axes_mode.mode == VEL) && !scan_mode.scanning){
			mvprintw(11,0,"Commanded velocity(dps): %lf\n", axes_mode.vel);
			mvprintw(12,0,"                                      ");
			mvprintw(13,0,"                                      ");
			mvprintw(14,0,"                                      ");
			mvprintw(15,0,"                                      ");
                        mvprintw(16,0,"                                      ");
			mvprintw(17,0,"                                      ");
                        mvprintw(18,0,"                                      ");
		}else if(scan_mode.scanning){
			mvprintw(11,0,"Commanded velocity(dps): %lf\n", axes_mode.vel);
			mvprintw(12,0,"Scan mode: %d\n", scan_mode.mode);
			mvprintw(13,0,"Start elevation(deg): %lf\n", scan_mode.start_el);
			mvprintw(14,0,"Stop elevation(deg): %lf\n", scan_mode.stop_el);
			mvprintw(15,0,"Dither speed(dps): %lf\n", scan_mode.vel);
			mvprintw(16,0,"Number of scans: %d\n", scan_mode.nscans);
			mvprintw(17,0,"Current Scan: %d\n", scan_mode.scan+1);
		}

		c = getch();
		if((c != '\n') && (c != EOF)){
			mvprintw(20,8+inputlen,"%c_",c);
			input[inputlen++] = c;
			input = (char*)realloc(input,inputlen+1);
		}else if(c == '\n'){
			input[inputlen] = '\0';
			mvprintw(20,8,"_");
			for(int i = 1; i<inputlen+1; i++){
				mvprintw(20,8+i," ");
			}

			cmd = (char*) malloc(strlen(input) * sizeof(char));
			arg = (char*) malloc(strlen(input) * sizeof(char));

			sscanf(input, "%s %[^\t\n]", cmd, arg);

			if(strcmp(cmd,"exit") == 0){
				ret = 1;
			}else if(strcmp(cmd,"gotoenc") == 0){
				go_to_enc(atof(arg));
			}else if(strcmp(cmd,"encdither") == 0){
				sscanf(arg, "%lf,%lf,%lf,%d",&scan_mode.start_el, &scan_mode.stop_el, &scan_mode.vel, &scan_mode.nscans);
				scan_mode.mode = ENC_DITHER;
				scan_mode.scanning = 1;
			}else if(strcmp(cmd,"stop") == 0){
				scan_mode.scanning = 0;
				axes_mode.mode = VEL;
				axes_mode.vel = 0.0;
				axes_mode.on_target = 0;
			}

			input = (char*)malloc(sizeof(char));
			inputlen = 0;
		}
	}
	endwin();
	delscreen(s);

    } else {
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
void cmdprompt(FILE* cmdlog) {
    int count = 1;
    char* input;

    while (exiting != 1) {
        printf("[BCP@Ophiuchus]<%d>$ ", count);
        input = get_input();
        if (strlen(input) != 0) {
            write_to_log(cmdlog, "cli.c", "cmdprompt", input);
            exec_command(input);
        }
        free(input);
        count++;
    }
}
