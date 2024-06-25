#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <libconfig.h>
#include "file_io_Oph.h"
#include "cli_Oph.h"

//This is the main struct that stores all the config parameters
struct conf_params config;

int main(int argc, char* argv[]){
//This is main please put all your pthreads here

	printf("This is BCP on Ophiuchus\n");
	printf("========================\n");

	FILE* main_log;//main log file
	FILE* cmd_log;//command log

	//get config file from command line and read it into the struct
	read_in_config(argv[1]);
	printf("Reading config paramters from:%s\n",argv[1]);
	print_config();
	printf("Starting main log\n");
	main_log = fopen(config.main.logpath, "w");

	if (main_log == NULL)
	{

		printf("Error opening logfile %s:No such file or directory\n",config.main.logpath);
		exit(0);
	}

	write_to_log(main_log,"main_Oph.c","main","Started logfile");

	cmd_log = fopen(config.main.cmdlog,"w");
	printf("Starting command log\n");
	write_to_log(main_log,"main_Oph.c","main","Starting command log");

	if (cmd_log == NULL)
	{

		printf("Error opening command log %s: No such file or directory\n",config.main.cmdlog);
		write_to_log(main_log,"main_Oph.c","main","Error oppening command log: No such file or directory");
		exit(0);
	}

	cmdprompt(cmd_log);

	fclose(cmd_log);
	fclose(main_log);
	return 0;
}
