#ifndef LOCKPIN_H
#define LOCKPIN_H

void lock(int duration);
void unlock(int duration);
void init_lockpin();
void close_lockpin();
void stop_lock();
void* do_lockpin();
extern int is_locked;
extern int lockpin_ready;
extern int baud;
extern char* serialport;
extern int lock_tel;
extern int unlock_tel;
extern int reset;
extern int exit_lock;
extern int duration;
extern FILE * lockpin_log;


#endif
