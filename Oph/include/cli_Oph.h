#ifndef CLI_OPH_H
#define CLI_OPH_H

void cmdprompt();
void exec_command(char* input);
char* get_input();

extern FILE* main_log;
extern FILE* cmd_log;
extern int bvexcam_on;
extern int lockpin_on;
extern pthread_t lock_thread;
#endif

