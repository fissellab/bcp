#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli_ground.h"

extern gs_conf_params gs_conf;
extern char* hostname;
int main(int argc, char* argv[]){
	
	if(argc<3){
		printf("Usage: %s <config_file> <host_name>\n", argv[0]);
		return 1;
	}
	
	// Display the beautiful BVEX banner
	print_bvex_banner();
	
	printf("This is BCP on %s\n", argv[2]);
	printf("======================\n");
	hostname = strdup(argv[2]);
	get_config_params(argv[1]);
	print_config();
	
	connect_to_sock();
        
	printf("Connecting to %s and %s .....\n",gs_conf.sag_ip,gs_conf.oph_ip);
	
	while(1){
		if(bcp_is_alive(OPH)){
			break;
		}
	}
	printf("Connected to Ophiuchus, accepting commands now\n");
	
	cmdprompt();

}
