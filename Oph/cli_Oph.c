#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_io_Oph.h"
#include "cli_Oph.h"

//This executes the commands, we can simply add commands by adding if statements
void exec_command(char* input){

	char* arg;
	char* cmd;
	int scan;
	arg = (char* ) malloc(strlen(input)*sizeof(char));
	cmd = (char* ) malloc(strlen(input)*sizeof(char));
	scan = sscanf(input, "%s %[^\t\n]",cmd,arg);
	if(strcmp(cmd,"print") == 0){

		if(scan == 1){

			printf("print is missing argument usage is print <string>\n");

		}else{

			printf("%s\n",arg);

		}

	}else if(strcmp(cmd,"exit") == 0){

		printf("Exiting BCP\n");
		exit(0);
	}else{

		printf("%s: Unknown commmand\n",cmd);
	}

}

//This gets an input of arbitrary length from the command line
char* get_input(){

	char* input;
	char c;
	int i = 0;

	input = (char *)malloc(1*sizeof(char));

	while((c = getchar()) != '\n' && c != EOF){
		input[i++] = c;
		input = (char* )realloc(input,i+1);

	}

	input[i] = '\0';
	return input;
}

//This is the main function for the command line
void cmdprompt(FILE* cmdlog){

	int count = 1;
	char* input;

	while(1){

		printf("[BCP@Ophiuchus]<%d>$ ",count);
		input = get_input();
		if(strlen(input)!=0){
			write_to_log(cmdlog,"cli.c","cmdprompt",input);
			exec_command(input);
		}
		count++;
	}
}

