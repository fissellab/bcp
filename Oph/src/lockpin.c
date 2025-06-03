#include "arduino.h"		//arduino library for serial
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "file_io_Oph.h"
extern struct conf_params config;
int fd = -1;
char* serialport;
int baud;
int is_locked = 1;
int lockpin_ready = 0;
int lock_tel = 0;
int unlock_tel = 0;
int reset = 0;
int exit_lock = 0;
void listen(int duration);
int start(char* serial, int baudrate);

FILE * lockpin_log;






void lock(int duration){
    char message[50] = "1,";
    char time[20];
    sprintf(time, "%d", duration);
    strcat(message, time);
    strcat(message, "\0");
    serialport_write(fd, message);
    listen(duration);
    is_locked = 1;
}

void unlock(int duration){
    char message[50] = "0,";
    char time[20];
    sprintf(time, "%d", duration);
    strcat(message, time);
    strcat(message, "\0");
    serialport_write(fd, message);
    listen(duration);
    is_locked = 0;
}

//STOP MEANS STOP AND RESET
void stop_lock(){
    char message[4] = "2,0";
    serialport_write(fd, message);
    is_locked = 0;
}

void call_lock(){

    if (unlock_tel){
        unlock(config.lockpin.duration);
	fprintf(lockpin_log, "unlocking\n");
		unlock_tel = 0;
    } else if (lock_tel){
        lock(config.lockpin.duration);
	fprintf(lockpin_log, "locking\n");
		lock_tel = 0;
    } else if (reset){
        stop_lock();
	fprintf(lockpin_log, "resetting\n");
		unlock_tel = 0;
		lock_tel = 0;
		reset = 0;
    }
}

void init_lockpin(){
    fd = serialport_init(config.lockpin.serialport, config.lockpin.baud);
    if( fd != -1 )
        serialport_flush(fd);
}

void close_lockpin(){
    fclose(lockpin_log);
    serialport_close(fd);
}

void listen(int duration){
    char buff[50];
    char eol = '\n';
    serialport_read_until(fd, buff, eol, 50, duration * 1.5);
    //printf("%s\n", buff);
}

int start(char* serial, int baudrate){
    lockpin_log = fopen(config.lockpin.logfile, "w");

    if (lockpin_log == NULL){
        printf("Missing lock_pin file\n");
	exit(1);
}
    baud = baudrate;

    init_lockpin();

    if (serialport != NULL) {
        free(serialport);
        serialport = NULL;
    }

    /// Allocate and copy the input string
    serialport = malloc(strlen(serial) + 1);
    if (serialport != NULL) {
        strcpy(serialport, serial);
	lockpin_ready = 1;
        return 1;
    }
    return 0;
}

void *do_lockpin(){
	if (start(config.lockpin.serialport, config.lockpin.baud)){
        while (1){
            if (exit_lock == 1)
                break;
            call_lock();
        }
        close_lockpin();
    } else {
        write_to_log(lockpin_log, "lockpin.c", "do_lockpin", "ERROR: CANNOT OPEN SERIAL PORT");
    }

}
