#ifndef FILE_IO_SAG_H
#define FILE_IO_SAG_H

void write_to_log(FILE* fptr, char* cfile, char* func, char* msg);
void read_in_config(char* filepath);
void print_config();

typedef struct main_conf{
	char* logpath;
	char* cmdlog;
} main_conf;

typedef struct conf_params{
	struct main_conf main;
} conf_params;

extern struct conf_params config;
#endif

