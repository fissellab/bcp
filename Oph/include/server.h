#ifndef SERVER_H
#define SERVER_H

#define MAXLEN 1024

void * do_server();

extern int tel_server_running;
extern int stop_tel;
extern FILE* server_log;


#endif
