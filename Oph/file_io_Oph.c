#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <libconfig.h>
#include "file_io_Oph.h"

//this file is for basic file operations so we can all stick to the dame format

//this takes the file pointer cfile is the name of the cfile it is called from 
//func is the name of the function it is called from amd msg
//is the message you wish to print to the log
void write_to_log(FILE* fptr, char* cfile, char* func, char* msg){

	time_t time_m;

	time_m = time(NULL);

	fprintf(fptr, "[%ld][%s][%s] %s\n", time_m, cfile, func, msg);
}
//This reads the config file and takes the config file path
//We just add parameters by adding ifstatements as below
void read_in_config(char* filepath){

	config_t conf;
	struct conf_params;
	const char* tmpstr = NULL;
	int status;

	config_init(&conf);

	if (!config_read_file(&conf,filepath)){

		printf("%s: No such file or directory\n",filepath);
		config_destroy(&conf);
		exit(0);

	}
	if (!config_lookup_string(&conf,"main.logpath",&tmpstr)){

		printf("Missing main.logpath in %s\n",filepath);
		config_destroy(&conf);
		exit(0);

	}

	config.main.logpath = (char*)malloc((strlen(tmpstr)+1)*sizeof(char));
	strcpy(config.main.logpath, tmpstr);

	if(!config_lookup_string(&conf,"main.cmdlog",&tmpstr)){

		printf("Missing main.cmdlog in %s\n",filepath);
		config_destroy(&conf);
		exit(0);

	}

	config.main.cmdlog = (char*)malloc((strlen(tmpstr)+1)*sizeof(char));
	strcpy(config.main.cmdlog, tmpstr);
	config_destroy(&conf);

}
//This prints the config file to screen
void print_config(){
	printf("Found following config parameters:\n\n");
	printf("main:{\n");
	printf("  logpath = %s;\n",config.main.logpath);
	printf("  cmdlog = %s;\n",config.main.cmdlog);
	printf("};\n\n");
}
