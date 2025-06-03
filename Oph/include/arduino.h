#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>   // Standard types 

//This is an arduino serial C library from https://github.com/todbot/arduino-serial/blob/main/

int serialport_init(const char* serialport, int baud);
int serialport_close(int fd);
int serialport_write(int fd, const char* str);
int serialport_read_until(int fd, char* buf, char until, int buf_max,int timeout);
int serialport_flush(int fd);

#endif
