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
#include "gps_server.h"
#include "starcam_downlink.h"
#include "server.h"
#include "pbob.h"

// This is the main struct that stores all the config parameters
struct conf_params config;
extern pthread_t motors;//Motor pthread
extern int * astro_ptr; // Pointer for returning from astrometry thread
extern FILE* motor_log; //motor log
extern FILE* ls_log;
extern FILE* gps_server_log;
extern FILE* server_log;
extern FILE* bvexcam_log;
extern FILE* main_log;
extern FILE* pbob_log_file;
extern AxesModeStruct axes_mode;//Pointing mode
extern int ready;//This flag keeps track of the motor thread being ready or not
extern int stop;//This flag is the queue to shut down the motor
extern int bvexcam_on;
extern int lockpin_on;
extern int fd_az;
extern int az_is_ready;
extern int motor_off;
extern int exit_lock;
extern int stop_server;
extern int server_running;
extern int stop_tel;
extern int tel_server_running;
extern int shutdown_pbob;
extern int pbob_enabled;
pthread_t ls_thread;
extern pthread_t lock_thread;
extern pthread_t astro_thread_id;
pthread_t gps_server_thread;
pthread_t server_thread;
pthread_t pbob_thread;

int main(int argc, char* argv[]) {
    printf("This is BCP on Ophiuchus\n");
    printf("========================\n");

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
    if (config.power.enabled){
        pbob_log_file = fopen(config.power.logfile,"w");
	if (pbob_log_file == NULL){
        	write_to_log(main_log, "main_Oph.c","main", "Error opening pbob log: No such file or directory");
        	fclose(main_log);
        	fclose(cmd_log);
        	exit(1);
	}else{
		printf("Starting PBoBs....\n");
		write_to_log(main_log,"main_Oph.c","main","Starting PBoBs");
		pthread_create(&pbob_thread,NULL,run_pbob_thread,NULL);
		while(!pbob_enabled){
			if(pbob_enabled){
				printf("PBoBs started successfully\n");
				write_to_log(main_log,"main_Oph.c","main","PBoBs started successfully");
				break;
			}
		}
	}
    }
    if (config.bvexcam.enabled){
        printf("Starting bvexcam log\n");
        write_to_log(main_log, "main_Oph.c", "main", "Starting bvexcam log");
        bvexcam_log = fopen(config.bvexcam.logfile, "w");

        if (bvexcam_log == NULL) {
               printf("Error opening bvexcam log %s: No such file or directory\n", config.bvexcam.logfile);
                      write_to_log(main_log, "main_Oph.c", "main", "Error opening bvexcam log: No such file or directory");
        }

    }

    // Initialize starcam downlink if enabled
    if (config.starcam_downlink.enabled) {
        printf("Starting starcam downlink\n");
        write_to_log(main_log, "main_Oph.c", "main", "Starting starcam downlink");
        
        // Copy config to starcam_config
        starcam_config.enabled = config.starcam_downlink.enabled;
        strncpy(starcam_config.logfile, config.starcam_downlink.logfile, sizeof(starcam_config.logfile) - 1);
        starcam_config.port = config.starcam_downlink.port;
        starcam_config.compression_quality = config.starcam_downlink.compression_quality;
        starcam_config.chunk_size = config.starcam_downlink.chunk_size;
        starcam_config.max_bandwidth_kbps = config.starcam_downlink.max_bandwidth_kbps;
        starcam_config.image_timeout_sec = config.starcam_downlink.image_timeout_sec;
        strncpy(starcam_config.workdir, config.starcam_downlink.workdir, sizeof(starcam_config.workdir) - 1);
        strncpy(starcam_config.notification_file, config.starcam_downlink.notification_file, sizeof(starcam_config.notification_file) - 1);
        starcam_config.udp_client_ips = config.starcam_downlink.udp_client_ips;
        starcam_config.num_client_ips = config.starcam_downlink.num_client_ips;
        
        if (initStarcamDownlink() != 0) {
            printf("Error initializing starcam downlink.\n");
            write_to_log(main_log, "main_Oph.c", "main", "Error initializing starcam downlink");
        } else {
            if (startStarcamServer() != 0) {
                printf("Error starting starcam downlink server.\n");
                write_to_log(main_log, "main_Oph.c", "main", "Error starting starcam downlink server");
            } else {
                printf("Successfully started starcam downlink.\n");
                write_to_log(main_log, "main_Oph.c", "main", "Successfully started starcam downlink");
            }
        }
    }

    /* Initialize accelerometer if enabled
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
    }*/

    if (config.motor.enabled){
	printf("Starting motor log.\n");
    	write_to_log(main_log,"main_Oph.c","main","Starting motor log");
    	motor_log = fopen(config.motor.logfile,"w");

        if(motor_log == NULL){
		printf("Error opening motor log %s: No such file or directory\n", config.motor.logfile);
                write_to_log(main_log, "main_Oph.c", "main", "Error opening motor log: No such file or directory");
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
    if (config.gps_server.enabled){
	printf("Starting GPS server....\n");
	write_to_log(main_log,"main_Oph.c","main","Starting GPS server");
	printf("Starting GPS server log....\n");
	write_to_log(main_log,"main_Oph.c","main","Starting GPS server log");
	gps_server_log = fopen(config.gps_server.logfile,"w");
	if(gps_server_log == NULL){
		printf("Error starting GPS server log %s: No such file or directory \n", config.gps_server.logfile);
		write_to_log(main_log, "main_Oph.c", "main", "Error opening GPS server log: No such file or directory");
	}else{
		pthread_create(&gps_server_thread,NULL,do_GPS_server,NULL);
		while(server_running==0){
			if(server_running==1){
				printf("Successfully started GPS server\n");
				write_to_log(main_log,"main_Oph.c","main","Successfully started GPS_server");
			}
		}
    	}
    }
    
    if (config.server.enabled){
	printf("Starting telemetry server....\n");
	write_to_log(main_log,"main_Oph.c","main","Starting telemetry server");
	printf("Starting telemetry server log....\n");
	write_to_log(main_log,"main_Oph.c","main","Starting telemetry server log");
	server_log = fopen(config.server.logfile,"w");
	if(server_log == NULL){
		printf("Error starting telemetry server log %s: No such file or directory \n", config.server.logfile);
		write_to_log(main_log, "main_Oph.c", "main", "Error opening telemetry server log: No such file or directory");
	}else{
		pthread_create(&server_thread,NULL,do_server,NULL);
		while(tel_server_running==0){
			if(tel_server_running==1){
				printf("Successfully started telemetry server\n");
				write_to_log(main_log,"main_Oph.c","main","Successfully started telemetry server");
			}
		}
    	}
    }
    
    printf("\n");
    // Start command-line
    cmdprompt();

    // Shutdown procedures
    if (config.bvexcam.enabled) {
	if (bvexcam_on){
        	pthread_join(astro_thread_id, (void **) &(astro_ptr));

        	if (*astro_ptr == 1) {
            		printf("Successfully shut down bvexcam.\n");
            		write_to_log(main_log, "main_Oph.c", "main", "Successfully shut down bvexcam");
			bvexcam_on = 0;
        	} else {
           		printf("bvexcam shut down unsuccessful.\n");
            	 	write_to_log(main_log, "main_Oph.c", "main", "bvexcam shut down unsuccessful");
		}
		//Put PBoB command here
                set_toggle(config.bvexcam.pbob,config.bvexcam.relay);
		//
        }

        fclose(bvexcam_log);
    }

    /* Shutdown accelerometer if it was enabled
    if (config.accelerometer.enabled) {
        printf("Shutting down accelerometer\n");
        write_to_log(main_log, "main_Oph.c", "main", "Shutting down accelerometer");
        accelerometer_shutdown();
        printf("Accelerometer shut down complete.\n");
        write_to_log(main_log, "main_Oph.c", "main", "Accelerometer shut down complete");
    }*/

    if (config.motor.enabled){
	if(!stop){
	    printf("Shutting down motor\n");
            write_to_log(main_log,"main_Oph.c","main","Shutting down motor");
	    stop=1;
            pthread_join(motors,NULL);
            printf("Motor shutdown complete\n");
	    write_to_log(main_log,"main_Oph.c","main","Motor shutdown complete");
	    //Put PBob command here
            set_toggle(config.motor.pbob,config.motor.relay);
	    //
        }else{
            printf("Motor already shutdown \n");
            write_to_log(main_log,"main_Oph.c","main","Motor already shutdown\n");
	}
	fclose(motor_log);
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
	//put PBoB command here

	//
	printf("Lockpin shutdown complete\n");
	write_to_log(main_log,"main_Oph.c","main","Lockpin shutdown complete");
    }
    if (config.gps_server.enabled){
        printf("Shutting down GPS server\n");
	write_to_log(main_log,"Main_Oph.c","main","Shutting down GPS_server");
        if (server_running == 1){
		stop_server = 1;
		pthread_join(gps_server_thread,NULL);
		printf("GPS server shutdown\n");
		write_to_log(main_log,"main_Oph.c","main","GPS server shutdown complete");
	}else{
		printf("GPS server already down\n");
		write_to_log(main_log,"main_Oph.c","main","GPS server already down");
	}
    }
    
    if (config.server.enabled){
        printf("Shutting down telemetry server\n");
	write_to_log(main_log,"Main_Oph.c","main","Shutting down telemetry_server");
        if (tel_server_running == 1){
		stop_tel = 1;
		pthread_join(server_thread,NULL);
		printf("Telemetry server shutdown\n");
		write_to_log(main_log,"main_Oph.c","main","Telemetry server shutdown complete");
	}else{
		printf("Telemetry server already down\n");
		write_to_log(main_log,"main_Oph.c","main","Telemetry server already down");
	}
    }

    // Shutdown starcam downlink if it was enabled
    if (config.starcam_downlink.enabled) {
        printf("Shutting down starcam downlink\n");
        write_to_log(main_log, "main_Oph.c", "main", "Shutting down starcam downlink");
        cleanupStarcamDownlink();
        printf("Starcam downlink shutdown complete.\n");
        write_to_log(main_log, "main_Oph.c", "main", "Starcam downlink shutdown complete");
    }

    //This should be the last thing to shut down
     if (config.power.enabled) {
	printf("Shutting down PBoB\n");
	write_to_log(main_log, "main_Oph.c", "main", "Shutting down PBoB");
	shutdown_pbob = 1;
	pthread_join(pbob_thread,NULL);
	printf("PBoB shutdown complete\n");
	write_to_log(main_log, "main_Oph.c", "main", " PBoB shutdown complete");
     }

    fclose(cmd_log);
    fclose(main_log);
    return 0;
}
