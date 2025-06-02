#ifndef CLI_SAG_H
#define CLI_SAG_H

#include <stdio.h>

void cmdprompt(FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path);
void exec_command(char* input, FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path);
char* get_input();

#endif
