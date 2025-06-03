#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <libconfig.h>
#include <string.h>
#include <errno.h>

#include "file_io_Oph.h"
#include "cli_Oph.h"
#include "bvexcam.h"
#include "accelerometer.h"
#include "ec_motor.h"
#include "motor_control.h"
#include "lazisusan.h"
#include "lockpin.h"

// This is the main struct that stores all the config parameters
struct conf_params config;
extern pthread_t astro_thread_id; // Star Camera thread id
extern pthread_t motors;//Motor pthread
extern int sockfd; // Star Camera Socket port
extern int * astro_ptr; // Pointer for returning from astrometry thread
extern FILE* motor_log; //motor log
extern FILE* ls_log;
extern AxesModeStruct axes_mode;//Pointing mode
extern int ready;//This flag keeps track of the motor thread being ready or not
extern int stop;//This flag is the queue to shut down the motor
extern int fd_az;
extern int az_is_ready;
extern int motor_off;
extern int exit_lock;
pthread_t ls_thread;
pthread_t lock_thread;

int main(int argc, char* argv[]) {
    printf("This is BCP on Ophiuchus\n");
    printf("========================\n");

    FILE* main_log; // main log file
    FILE* cmd_log; // command log
    FILE* bvexcam_log; // Star camera log

    // get config file from command line and read it into the struct
    if (argc < 2) {
        printf("Usage: %s <config_file>\n", argv[0]);
        return 1;
    }
    read_in_config(argv[1]);
    printf("Reading config parameters from: %s\n", argv[1]);
    print_config();

    printf("Starting main log\n");
    main_log = fopen(config.main.logpath, "w");

    if (main_log == NULL) {
        printf("Error opening logfile %s: No such file or directory\n", config.main.logpath);
        exit(1);
    }

    write_to_log(main_log, "main_Oph.c", "main", "Started logfile");

    cmd_log = fopen(config.main.cmdlog, "w");
    printf("Starting command log\n");
    write_to_log(main_log, "main_Oph.c", "main", "Starting command log");

    if (cmd_log == NULL) {
        printf("Error opening command log %s: No such file or directory\n", config.main.cmdlog);
        write_to_log(main_log, "main_Oph.c", "main", "Error opening command log: No such file or directory");
        fclose(main_log);
        exit(1);
    }

    if (config.bvexcam.enabled) {
        printf("Starting bvexcam\n");
        write_to_log(main_log, "main_Oph.c", "main", "Starting bvexcam");
        printf("Starting bvexcam log\n");
        write_to_log(main_log, "main_Oph.c", "main", "Starting bvexcam log");
        bvexcam_log = fopen(config.bvexcam.logfile, "w");

        if (bvexcam_log == NULL) {
            printf("Error opening bvexcam log %s: No such file or directory\n", config.bvexcam.logfile);
            write_to_log(main_log, "main_Oph.c", "main", "Error opening bvexcam log: No such file or directory");
        } else {
            init_bvexcam(bvexcam_log);

            // start star camera pthread
            if (pthread_create(&astro_thread_id, NULL, run_bvexcam, (void *)bvexcam_log)) {
                fprintf(stderr, "Error creating Astrometry thread: %s.\n", strerror(errno));
                printf("Starting bvexcam was not successful.\n");
                close(sockfd);
            } else {
                printf("Successfully started bvexcam.\n");
            }
        }
    }

    // Initialize accelerometer if enabled
    if (config.accelerometer.enabled) {
        printf("Starting accelerometer\n");
        write_to_log(main_log, "main_Oph.c", "main", "Starting accelerometer");
        
        if (accelerometer_init() != 0) {
            printf("Error initializing accelerometer.\n");
            write_to_log(main_log, "main_Oph.c", "main", "Error initializing accelerometer");
        } else {
            printf("Successfully started accelerometer.\n");
            write_to_log(main_log, "main_Oph.c", "main", "Successfully started accelerometer");
        }
    }

    if (config.motor.enabled){
	printf("Starting motor log.\n");
    	write_to_log(main_log,"main_Oph.c","main","Starting motor log");
    	motor_log = fopen(config.motor.logfile,"w");

        if(motor_log == NULL){
		printf("Error opening motor log %s: No such file or directory\n", config.motor.logfile);
                write_to_log(main_log, "main_Oph.c", "main", "Error opening motor log: No such file or directory");
	}else{
		printf("Starting motors\n");
		write_to_log(main_log,"main_Oph.c","main","Starting motors");
		if(start_motor()){
			printf("Motor startup successful\n");
			write_to_log(main_log,"main_Oph.c","main","Motor startup successful");
		}else{
			printf("Error starting up motor see motor log\n");
			write_to_log(main_log,"main_Oph.c","main","Error starting up motor see motor log");
		}

	}
    }
    if (config.lazisusan.enabled){
	printf("Starting lazisusan log.\n");
	write_to_log(main_log,"main_Oph.c","main","Starting motor log");
	ls_log = fopen(config.lazisusan.logfile,"w");

	if(ls_log == NULL){
		printf("Error opening lazisusan log %s: No such file or directory", config.lazisusan.logfile);
		write_to_log(main_log, "main_Oph.c", "main", "Error opening lazisusan log: No such file or directory");
	}else{
		printf("Starting lazisusan\n");
		pthread_create(&ls_thread,NULL,do_az_motor,NULL);
		write_to_log(main_log,"main_Oph.c","main","Starting lazisusan");
		while(!az_is_ready){
			if(fd_az <0){
				printf("Error starting lazisusan\n");
				write_to_log(main_log,"main_Oph.c","main","Error starting lazisusan");
				break;
			}
		}
		if(az_is_ready){
			printf("Successfully started lazisusan\n");
			write_to_log(main_log,"main_Oph.c","main","Successfully started lazisusan");
		}
	}
    }
    if(config.lockpin.enabled){
	printf("Starting lockpin...");
	write_to_log(main_log,"main_Oph.c","main","Starting lockpin");
	pthread_create(&lock_thread,NULL,do_lockpin,NULL);
	while(!lockpin_ready){
		if(lockpin_ready){
			printf("Successfully started lockpin\n");
			write_to_log(main_log,"main_Oph.c","main","Successfully started lockpin");
			break;
		}
	}
    }
    printf("\n");
    // Start command-line
    cmdprompt(cmd_log);

    // Shutdown procedures
    if (config.bvexcam.enabled) {
        pthread_join(astro_thread_id, (void **) &(astro_ptr));

        if (*astro_ptr == 1) {
            printf("Successfully shut down bvexcam.\n");
            write_to_log(main_log, "main_Oph.c", "main", "Successfully shut down bvexcam");
        } else {
            printf("bvexcam shut down unsuccessful.\n");
            write_to_log(main_log, "main_Oph.c", "main", "bvexcam shut down unsuccessful");
        }

        fclose(bvexcam_log);
    }

    // Shutdown accelerometer if it was enabled
    if (config.accelerometer.enabled) {
        printf("Shutting down accelerometer\n");
        write_to_log(main_log, "main_Oph.c", "main", "Shutting down accelerometer");
        accelerometer_shutdown();
        printf("Accelerometer shut down complete.\n");
        write_to_log(main_log, "main_Oph.c", "main", "Accelerometer shut down complete");
    }

    if (config.motor.enabled){
	if(!stop){
	    printf("Shutting down motor\n");
            write_to_log(main_log,"main_Oph.c","main","Shutting down motor");
	    stop=1;
            pthread_join(motors,NULL);
            printf("Motor shutdown complete\n");
	    write_to_log(main_log,"main_Oph.c","main","Motor shutdown complete");
	    fclose(motor_log);
        }else{
            printf("Motor already shutdown \n");
            write_to_log(main_log,"main_Oph.c","main","Motor already shutdown\n");
	}
    }

    if (config.lazisusan.enabled){
	printf("Shutting down lazisusan\n");
	write_to_log(main_log,"main_Oph.c","main","Shutting down lazisusan");
	motor_off = 1;
	pthread_join(ls_thread,NULL);
	printf("Lazisusan shutdown complete\n");
	write_to_log(main_log,"main_Oph.c","main","Lazisusan shutdown complete");
	fclose(ls_log);
    }

    if (config.lockpin.enabled){
	printf("Shutting down lockpin\n");
	write_to_log(main_log,"Main_Oph.c","main","Shutting down lockpin");
	exit_lock = 1;
	pthread_join(lock_thread,NULL);
	printf("Lockpin shutdown complete\n");
	write_to_log(main_log,"main_Oph.c","main","Lockpin shutdown complete");
    }

    fclose(cmd_log);
    fclose(main_log);
    return 0;
}
