#ifndef LAZISUSAN_H
#define LAZISUSAN_H


void move_to(double angle);
void enable_disable_motor();
void * do_az_motor(void*);
double get_angle();

void set_offset(double cal_angle);

extern int fd_az;
extern int motor_enabled;
extern int motor_off;
extern int az_is_ready;
extern int count_now;
extern double az_offset;
extern FILE* ls_log;

#endif

