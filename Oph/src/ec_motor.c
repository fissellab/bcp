#include <stdio.h>
#include <stdlib.h>
#include <ethercat.h>
#include <ethercatmain.h>
#include <ethercattype.h>
#include <nicdrv.h>
#include <ethercatconfig.h>
#include <ethercatprint.h>
#include <ethercatcoe.h>
#include <ethercatdc.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <glib.h>
#include <unistd.h>
#include <curses.h>


#include "ec_motor.h"
#include "motor_control.h"
#include "file_io_Oph.h"

FILE* motor_log;
static int32_t dummy_var = 0;
static char io_map[4096]; //Memory mapping for PDO

static GSList *pdo_list;

static int32_t *motor_position = &dummy_var;
static int32_t *motor_velocity = &dummy_var;
static int16_t *motor_current = (int16_t*) &dummy_var;
static int32_t *actual_position = &dummy_var;
static uint32_t *status_register = (uint32_t*) &dummy_var;
static int16_t *amp_temp = (int16_t*) &dummy_var;
static uint16_t *status_word = (uint16_t*) &dummy_var;
static uint32_t *latched_register = (uint32_t*) &dummy_var;
static int16_t *phase_angle = (int16_t*) &dummy_var;
static uint16_t *network_status_word = (uint16_t*) &dummy_var;
static uint16_t *control_word_read = (uint16_t*) &dummy_var;
static uint16_t *control_word = (uint16_t*) &dummy_var;
static int16_t *target_current = (int16_t*) &dummy_var;

static ec_device_state_t controller_state = {0};
double motor_offset = 0.0;
static int firsttime = 1;

extern ScanModeStruct scan_mode;

motor_data_t MotorData[3] = {{0}};
int motor_index = 0;
int stop = 0;
int ready = 0;
int comms_ok = 0;

int check_slave_comm_ready(){
	if (!controller_state.comms_ok){
		return 0;
	}else{
		return 1;
	}

}

int16_t get_current(void){
	if (check_slave_comm_ready()){
		return *motor_current;
	}else{
		return 0;
	}
}

uint16_t get_status_word(void){
	if (check_slave_comm_ready()){
		return *status_word;
	}else{
		return 0;
	}
}

uint32_t get_latched(void){
	if(check_slave_comm_ready()){
		return *latched_register;
	}else{
		return 0;
	}
}

uint16_t get_status_register(void){
	if(check_slave_comm_ready()){
		return *status_register;
	}else{
		return 0;
	}
}

int32_t get_position(void){
	if(check_slave_comm_ready()){
		return *motor_position;
	}else{
		return 0;
	}
}

double get_position_deg(void){
	if(check_slave_comm_ready()){
		int32_t pos_counts = get_position();
		return (double)pos_counts * MOTOR_ENCODER_SCALING - motor_offset;
	}else{
		return 0;
	}
}

int16_t get_amp_temp(void){
	if(check_slave_comm_ready()){
		return *amp_temp;
	}else{
		return 0;
	}
}

int32_t get_velocity(void){
	if(check_slave_comm_ready()){
		return *motor_velocity;
	}else{
		return 0;
	}		
}

double get_velocity_dps(void){
	int32_t velocity_cps = get_velocity();
	
	return velocity_cps * 0.1 * MOTOR_ENCODER_SCALING;

}

uint16_t get_ctl_word(void){
	if(check_slave_comm_ready()){
		return *control_word_read;
	}else{
		return 0;
	}
}

uint16_t get_ctl_word_write(void){
	if(check_slave_comm_ready()){
		return *control_word;
	}else{
		return 0;
	}
}

int16_t get_phase_angle(void){
	if(check_slave_comm_ready()){
		return *phase_angle;
	}else{
		return 0;
	}
}

void set_current(int16_t m_cur){
	if(check_slave_comm_ready()){
		*target_current = m_cur;
	}

}

static uint8_t check_ec_ready(void){

	uint16_t m_state = 0;
	
	if(!controller_state.comms_ok){
		return 0;
	}
	if (check_slave_comm_ready()){
		if (!(*control_word_read == *control_word) ||!(*status_word & ECAT_CTL_STATUS_READY)|| (*status_register & ECAT_STATUS_PHASE_UNINIT) ){
			return 0;
		}
	}else{
		return 0;
	}
	
	return 1;
}

uint8_t is_motor_ready() {
	return(check_ec_ready());
}

static void ec_init_heartbeat(void){
	
	write_to_log(motor_log,"ec_motor.c","ec_init_heartbeat","Initializing heartbeat");	
	if(!ec_SDOwrite16(1, ECAT_HEART_BEAT_TIME, HEARTBEAT_MS)){
		write_to_log(motor_log,"ec_motor.c","ec_init_heartbeat","Error initializing hearbeat");
	}
	if(!ec_SDOwrite8(1, ECAT_LIFETIME_FACTOR, LIFETIME_FACTOR_EC)){
		write_to_log(motor_log,"ec_motor.c","ec_init_heartbeat","Error initializing lifetime factor");
	}

}

void init_current_limit(void){
	
	write_to_log(motor_log,"ec_motor.c","init_current_pid","Initializing current limit");
	if(!ec_SDOwrite16(1, ECAT_PEAK_CURRENT, PEAK_CURRENT)){
		write_to_log(motor_log,"ec_motor.c","init_current_pid","Error initializing peak current limit");
	}
	if(!ec_SDOwrite16(1, ECAT_CONT_CURRENT, CONT_CURRENT)){
		write_to_log(motor_log,"ec_motor.c","init_current_pid","Error initializing continuous current limit");
	}

}

void init_current_pid(void){
	write_to_log(motor_log,"ec_motor.c","init_current_pid","Initializing current PID ");
	if(!ec_SDOwrite16(1, ECAT_CURRENT_LOOP_CP, DEFAULT_CURRENT_P)){
		write_to_log(motor_log,"ec_motor.c","init_current_pid","Error initializing current P");
	}
        if(!ec_SDOwrite16(1, ECAT_CURRENT_LOOP_CI, DEFAULT_CURRENT_I)){
        	write_to_log(motor_log,"ec_motor.c","init_current_pid","Error initializing current I");
        }
        if(!ec_SDOwrite16(1, ECAT_CURRENT_LOOP_OFFSET, DEFAULT_CURRENT_OFF)){
        	write_to_log(motor_log,"ec_motor.c","init_currnet_pid","Error initializing current offset");
        }
}

static void init_encoder(void){
	write_to_log(motor_log,"ec_motor.c","init_encoder","Initializing encoder");
	if(!ec_SDOwrite32(1, ECAT_ENCODER_WRAP, MOTOR_ENCODER_COUNTS)){
		write_to_log(motor_log,"ec_motor.c","init_encoder","Error initializing encoder wrap");
	}
        if(!ec_SDOwrite32(1, ECAT_COUNTS_PER_REV, MOTOR_COUNTS_PER_REV)){
        	write_to_log(motor_log,"ec_motor,c","init_encoder","Error initializing counts per revolution");
        }

}

void enable(void){

	
	if (check_slave_comm_ready()) {
		//printf("Enabling drive\n");
		*control_word = ECAT_CTL_ON | ECAT_CTL_ENABLE_VOLTAGE | ECAT_CTL_QUICK_STOP | ECAT_CTL_ENABLE;
	}

}

int reset_ec_motor(){
	controller_state.has_dc = 0;
	controller_state.comms_ok = 0;
	controller_state.slave_error = 0;
	configure_ec_motor();
	return 1;

}

void read_motor_data(){

	MotorData[motor_index].current = get_current() / 100.0; //Current in Amps
	MotorData[motor_index].drive_info = get_status_word();
	MotorData[motor_index].fault_reg = get_latched();
	MotorData[motor_index].status = get_status_register();
	MotorData[motor_index].position = get_position_deg();
	MotorData[motor_index].temp = get_amp_temp();
	MotorData[motor_index].velocity = get_velocity_dps();
	MotorData[motor_index].control_word_read = get_ctl_word();
	MotorData[motor_index].control_word_write = get_ctl_word_write();
	MotorData[motor_index].network_problem = !is_motor_ready();
	MotorData[motor_index].phase_angle = get_phase_angle();
	
	motor_index = INC_INDEX(motor_index);
}

void print_motor_data(void){
	
	int motor_i = GETREADINDEX(motor_index);
	mvprintw(0,0,"Current sent to controller (mA): %d\n",*target_current*10);
	mvprintw(1,0,"Motor Current(A): %lf\n",MotorData[motor_i].current);
	mvprintw(2,0,"Motor Status Word: %d\n", MotorData[motor_i].drive_info);
	mvprintw(3,0,"Motor Latched Fault: %d\n",MotorData[motor_i].fault_reg);
	mvprintw(4,0,"Motor Status register: %d\n",MotorData[motor_i].status);
	mvprintw(5,0,"Motor Position(deg): %lf\n", MotorData[motor_i].position);
	mvprintw(6,0,"Motor Temperature: %d\n", MotorData[motor_i].temp);
	mvprintw(7,0,"Motor Velocity(dps): %lf\n", MotorData[motor_i].velocity);
	mvprintw(8,0,"Motor Control Word read: %d\n", MotorData[motor_i].control_word_read);
	mvprintw(9,0,"Motor Control Word write: %d\n", MotorData[motor_i].control_word_write);
	mvprintw(10,0,"Motor network_problem: %d\n", MotorData[motor_i].network_problem);
	
}

//This finds the controller in the network
static int find_controller(void){

	
	if (ec_config(false, &io_map) <= 0) {
		write_to_log(motor_log,"ec_motor.c","find_controller","No motor controller slave found in the network");
		return -1;
	}
	fprintf(motor_log,"[%ld][ec_motor.c][find_controller] %d slave found and configured:",time(NULL),ec_slavecount);
   	fprintf(motor_log,"[%ld][ec_motor.c][find_controller] %s\n",time(NULL),ec_slave[1].name);
   	
   	
   	
   	if (ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE) != EC_STATE_SAFE_OP) {
   		write_to_log(motor_log,"ec_motor.c","find_controller","Slave has not reached safe operational mode yet");
   		ec_readstate();
                fprintf(motor_log,"[%ld][ec_motor.c][find_controller] State=%2x StatusCode=%4x : %s\n", time(NULL), ec_slave[1].state, ec_slave[1].ALstatuscode, ec_ALstatuscode2string(ec_slave[1].ALstatuscode));
        }
    	
    	return ec_slavecount;
}

//This does the PDO mapping and address assignment
static int motor_pdo_init(void){

	pdo_mapping_t map;
    	int retval = 0;

    	if (ec_slave[1].state != EC_STATE_SAFE_OP && ec_slave[1].state != EC_STATE_PRE_OP) {
        	write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Motor Controller is not in pre-operational state!  Cannot configure.");
        	return -1;
    	}
	
	write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Configuring PDO Mappings.");
	
	if (!ec_SDOwrite8(1, ECAT_TXPDO_ASSIGNMENT, 0, 0)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to clear old state  on assignment TxPDO");
		return -1;
	}
	
	for (int i=0; i<4; i++){
		if (!ec_SDOwrite8(1, ECAT_TXPDO_MAPPING+i, 0,0)){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] Failed to clear old state on mapping TxPDO %d\n",time(NULL),i);
			return -1;
		}
	}
	
	//0x1a00 register
	map_pdo(&map, ECAT_MOTOR_POSITION, 32);
	
	if (!ec_SDOwrite32(1,ECAT_TXPDO_MAPPING, 1, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map motor position");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	map_pdo(&map, ECAT_VEL_ACTUAL, 32);
	
	if (!ec_SDOwrite32(1,ECAT_TXPDO_MAPPING, 2, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map motor velocity");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if(!ec_SDOwrite8(1, ECAT_TXPDO_MAPPING,0, 2)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to set number of elements on 0x1a00");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if(!ec_SDOwrite16(1, ECAT_TXPDO_ASSIGNMENT, 1, ECAT_TXPDO_MAPPING)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed mapping 0x1a00 to first PDO");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
		
	}
	
	
	
	//0x1a01 register
	
	map_pdo(&map, ECAT_ACTUAL_POSITION, 32);
	
	if (!ec_SDOwrite32(1,ECAT_TXPDO_MAPPING+1, 1, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map actual position");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	map_pdo(&map, ECAT_CTL_WORD, 16);
	
	if (!ec_SDOwrite32(1, ECAT_TXPDO_MAPPING+1, 2, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map control word");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	/*map_pdo(&map, ECAT_NET_STATUS, 16);
	
	if (!ec_SDOwrite32(1, ECAT_TXPDO_MAPPING+1, 3, map.val)){
		printf("Failed to map network status\n");
	}*/
	
	if (!ec_SDOwrite8(1, ECAT_TXPDO_MAPPING+1, 0, 2)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init", "Failed to set number of elements on 0x1a01");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite16(1, ECAT_TXPDO_ASSIGNMENT, 2, ECAT_TXPDO_MAPPING+1)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed mapping 0x1a01 to second PDO");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	while (ec_iserror()){
		fprintf(motor_log,"[%ld][ec_motor.c][motor_pso_init] %s\n", time(NULL), ec_elist2string());
	}
	
	// 0x1a02 register
	
	map_pdo(&map,ECAT_DRIVE_STATUS, 32);
	
	if (!ec_SDOwrite32(1, ECAT_TXPDO_MAPPING+2, 1, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map drive status");	
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	map_pdo(&map, ECAT_CTL_STATUS, 16);
	
	if (!ec_SDOwrite32(1, ECAT_TXPDO_MAPPING+2, 2, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map control status");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	map_pdo(&map,ECAT_DRIVE_TEMP, 16);
	
	if (!ec_SDOwrite32(1, ECAT_TXPDO_MAPPING+2, 3, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map drive temperature");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite8(1, ECAT_TXPDO_MAPPING+2, 0, 3)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to set number of elements on 0x1a02");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite16(1, ECAT_TXPDO_ASSIGNMENT, 3, ECAT_TXPDO_MAPPING + 2)){
        	write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed mapping 0x1a02 to third PDO");
        	while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
        }
        
        while (ec_iserror()) {
        	fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
	}
	
	// 0x1a03 register
	
	map_pdo(&map, ECAT_LATCHED_DRIVE_FAULT, 32);
	
	if (!ec_SDOwrite32(1, ECAT_TXPDO_MAPPING+3, 1, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map latched fault");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	map_pdo(&map, ECAT_CURRENT_ACTUAL, 16);
	
	if (!ec_SDOwrite32(1, ECAT_TXPDO_MAPPING+3, 2, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map actual current");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	map_pdo(&map, ECAT_COMMUTATION_ANGLE, 16);
	
	if(!ec_SDOwrite32(1, ECAT_TXPDO_MAPPING+3, 3, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map commutation angle");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite8(1, ECAT_TXPDO_MAPPING+3, 0, 3)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to set number of elements on 0x1a03");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite16(1, ECAT_TXPDO_ASSIGNMENT, 4, ECAT_TXPDO_MAPPING+3)){
		write_to_log(motor_log,"ec_motor.c","motor_init_pdo","Failed mapping 0x1a03 to third PDO");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite8(1, ECAT_TXPDO_ASSIGNMENT, 0, 4)){
		write_to_log(motor_log,"ec_motor.c","motor_init_pdo","Failed assigning number of elements to TxPDO");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	
	//RxPDO Assignments
	
	for(int i = 0; i<4; i++){
		if (!ec_SDOwrite8(1,ECAT_RXPDO_MAPPING+i, 0, 0)){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] Failed to clear old state on mapping RxPDO %d\n", time(NULL), i);
			while (ec_iserror()){
				fprintf(motor_log, "[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
			}
			return -1;
		}
	}
	
	if(!ec_SDOwrite8(1, ECAT_RXPDO_ASSIGNMENT, 0, 0)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to clear old state on assignment RxPDO");
		while (ec_iserror()){
			fprintf(motor_log, "[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	
	
	//0x1600 register
	
	map_pdo(&map, ECAT_CTL_WORD, 16);
	
	if (!ec_SDOwrite32(1, ECAT_RXPDO_MAPPING, 1, map.val)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map control word");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	map_pdo(&map, ECAT_CURRENT_LOOP_CMD, 16);
	
	if (!ec_SDOwrite32(1, ECAT_RXPDO_MAPPING, 2, map.val)){
		write_to_log(motor_log, "ec_motor.c","motor_pdo_init","Failed to map commanded current");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	//map_pdo(&map, ECAT_PHASING_MODE, 16);
	
	//if (!ec_SDOwrite32(1, ECAT_RXPDO_MAPPING, 3, map.val)){
	//	printf("Failed to map phasing mode\n");
	//}
	
	if (!ec_SDOwrite8(1, ECAT_RXPDO_MAPPING, 0, 2)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to set number of elements for 0x1600");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite16(1, ECAT_RXPDO_ASSIGNMENT, 1, ECAT_RXPDO_MAPPING)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to map 0x1600 to first PDO");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite8(1,ECAT_RXPDO_ASSIGNMENT,0,1)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed assigning number of elements to RxPDO");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite32(1, 0x2420, 0, 8)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to set misc. drive options register");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	if (!ec_SDOwrite32(1, 0x1010, 1, 0x65766173)){
		write_to_log(motor_log,"ec_motor.c","motor_pdo_init","Failed to set save parameters register");
		while (ec_iserror()){
			fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
		}
		return -1;
	}
	
	while (ec_iserror()){
		fprintf(motor_log,"[%ld][ec_motor.c][motor_pdo_init] %s\n", time(NULL), ec_elist2string());
	}
	
	return 0;
}

void mc_readPDOassign(){

	uint16_t idxloop, nidx, subidxloop, rdat, idx, subidx;
	uint8_t subcnt;
	
	int wkc = 0;
	int len =0;
	int offset = 0;
	
	len = sizeof(rdat);
	rdat = 0;
	
	wkc = ec_SDOread(1, ECAT_TXPDO_ASSIGNMENT, 0x00, FALSE, &len, &rdat, EC_TIMEOUTRXM);
	rdat = etohs(rdat);
	
	fprintf(motor_log,"[%ld][ec_motor.c][mc_readPDOassign] Result from ec_SDOread at index %04x, wkc = %i, len = %i, rdat = %04x \n", time(NULL), ECAT_TXPDO_ASSIGNMENT, wkc, len, rdat);
	
	if ((wkc <= 0) || (rdat <= 0))  {
		write_to_log(motor_log, "ec_motor.c","mc_readPDOassign","No data returned from ec_SDOread ... returning.");
    		return;
    	}
    	
    	nidx = rdat;
    	
    	for (idxloop = 1; idxloop <= nidx; idxloop++){
    		
    		len = sizeof(rdat);
    		rdat = 0;
    		
    		wkc = ec_SDOread(1, ECAT_TXPDO_ASSIGNMENT, (uint8) idxloop, FALSE, &len, &rdat, EC_TIMEOUTRXM);
    		
    		idx = etohl(rdat);
    		
    		if (idx <= 0){
    			continue;
    		}else{
    			fprintf(motor_log,"[%ld][ec_motor.c][mc_readPDOassign] Found idx = %2x at wkc = %i, idxloop = %i\n", time(NULL), idx, wkc, idxloop);	
    		}
    		
    		len = sizeof(subcnt);
    		subcnt =0;
    		
    		wkc = ec_SDOread(1, idx, 0x00, FALSE, &len, &subcnt, EC_TIMEOUTRXM);
    		subidx = subcnt;
    		
    		for (subidxloop = 1; subidxloop <= subidx; subidxloop++){
    			
    			pdo_channel_map_t *channel = NULL;
    			pdo_mapping_t pdo_map = {0};
    			
    			len = sizeof(pdo_map);
    			
    			wkc = ec_SDOread(1, idx, (uint8) subidxloop,  FALSE, &len, &pdo_map, EC_TIMEOUTRXM);
    			
    			channel = malloc(sizeof(pdo_channel_map_t));
    			channel->index = pdo_map.index;
    			channel->subindex = pdo_map.subindex;
    			channel->offset = offset;
    			
    			pdo_list = g_slist_prepend(pdo_list, channel);
    			
    			fprintf(motor_log, "[%ld][ec_motor.c][mc_readPDOassign] Read SDO subidxloop = %i, wkc = %i, idx = %i, len = %i\n", time(NULL), subidxloop, wkc, idx, len);
    			fprintf(motor_log, "[%ld][ec_motor.c][mc_readPDOassign] Appending channel to m_pdo_list = %p: index = %2x, subindex = %i,offset = %i\n", time(NULL), pdo_list, channel->index, channel->subindex, channel->offset);
    			
    			offset += (pdo_map.size / 8);
    			
    			
    			
    		}
	}
}

static void map_index_vars(){

	bool found;
	
	write_to_log(motor_log,"ec_motor.c","map_index_vars","Starting map_index_vars");
	
	#define PDO_SEARCH_LIST(_obj, _map){ \
		found = false; \
		for (GSList *el = pdo_list; (el); el = g_slist_next(el)) { \
			pdo_channel_map_t *ch = (pdo_channel_map_t*)el->data; \
			if (ch->index == object_index(_obj) && ch->subindex == object_subindex(_obj)){\
				_map = (typeof(_map)) (ec_slave[1].inputs + ch->offset);\
				found = true; \
			} \
		}\
		if (!found) { \
			fprintf(motor_log, "[%ld][ec_motor.c][map_index_vars] Could not find PDO map for channel index = %2x, subindex = %d\n", time(NULL), object_index(_obj), object_subindex(_obj)); \
		} \
	}
	
	PDO_SEARCH_LIST(ECAT_MOTOR_POSITION, motor_position);
	PDO_SEARCH_LIST(ECAT_VEL_ACTUAL, motor_velocity);
	PDO_SEARCH_LIST(ECAT_ACTUAL_POSITION, actual_position);
	PDO_SEARCH_LIST(ECAT_DRIVE_STATUS, status_register);
	PDO_SEARCH_LIST(ECAT_CTL_STATUS, status_word);
	PDO_SEARCH_LIST(ECAT_DRIVE_TEMP, amp_temp);
	PDO_SEARCH_LIST(ECAT_LATCHED_DRIVE_FAULT, latched_register);
	PDO_SEARCH_LIST(ECAT_CURRENT_ACTUAL, motor_current);
	PDO_SEARCH_LIST(ECAT_COMMUTATION_ANGLE, phase_angle);
	//PDO_SEARCH_LIST(ECAT_NET_STATUS, network_status_word);
	PDO_SEARCH_LIST(ECAT_CTL_WORD, control_word_read);
	
	while (ec_iserror()){
		fprintf(motor_log,"[%ld][ec_motor.c][map_index_vars] %s\n", time(NULL), ec_elist2string());
	}

	#undef PDO_SEARCH_LIST
	
	control_word = (uint16_t*) ec_slave[1].outputs;
	target_current = (int16_t*) (control_word+1);
	
	if(!ec_slave[1].outputs){
		write_to_log(motor_log,"ec_motor.c","map_index_vars","Error: IOMAP was not configured correctly! Setting slave_error = 1");
		controller_state.slave_error = 1;
	}else{
		controller_state.slave_error = 0;
	}
	
}

void set_ec_motor_defaults(void){
	
	init_current_limit();
	init_current_pid();
	init_encoder();
	ec_init_heartbeat();
	while (ec_iserror()) {
		fprintf(motor_log,"[%ld][ec_motor.c][set_ec_motor_defaults] %s\n", time(NULL), ec_elist2string());
	}
}

static void motor_configure_timing(void){

	int found_dc_master = 0;
	ec_configdc();
	
	while(ec_iserror()){
		fprintf(motor_log,"[%ld][ec_motor.c][motor_configure_timing] %s\n", time(NULL), ec_elist2string());
	}
	
	if(!found_dc_master && ec_slave[1].hasdc){
		ec_dcsync0(1,true,ECAT_DC_CYCLE_NS, ec_slave[1].pdelay);
		found_dc_master =1;
	}else{
		ec_dcsync0(1, false, ECAT_DC_CYCLE_NS, ec_slave[1].pdelay);
	}
	
	while (ec_iserror()){
		fprintf(motor_log,"[%ld][ec_motor.c][motor_configure_timing] %s\n", time(NULL), ec_elist2string());
	}
	
	ec_SDOwrite16(1,ECAT_DC_SYNCH_MNG_2_TYPE, 0);
	
	while (ec_iserror()){
		fprintf(motor_log,"[%ld][ec_motor.c][motor_configure_timing] %s\n", time(NULL), ec_elist2string());
	}
	if (ec_slave[1].hasdc && found_dc_master){
		controller_state.has_dc = 1;
		write_to_log(motor_log,"ec_motor.c","motor_configure_timing","Successfully configured timing");
	}else{
		controller_state.has_dc = 0;
		write_to_log(motor_log,"ec_motor.c","motor_configure_timing","Error configuring timing");	
	}

}

static int motor_set_operational(){
	ec_send_processdata();
	ec_receive_processdata(EC_TIMEOUTRET);
	
	ec_slave[0].state = EC_STATE_OPERATIONAL;
	
	ec_send_processdata();
	ec_receive_processdata(EC_TIMEOUTRET);
	
	ec_writestate(0);
	
	for (int i=0; i<40; i++){
		ec_send_processdata();
		ec_receive_processdata(EC_TIMEOUTRET);
		if (ec_statecheck(0, EC_STATE_OPERATIONAL, 50000) == EC_STATE_OPERATIONAL){
			break;
		}	
	}
	
	if (ec_slave[0].state == EC_STATE_OPERATIONAL){
		write_to_log(motor_log,"ec_motor.c","motor_set_operational", "We have reached the fully operational state");
		return 0;
	}
	
	if (ec_slave[1].state != EC_STATE_OPERATIONAL){
		fprintf(motor_log,"[%ld][ec_motor.c][motor_set_operational] State=%2x Statuscode=%4x : %s\n", time(NULL), ec_slave[1].state, ec_slave[1].ALstatuscode, ec_ALstatuscode2string(ec_slave[1].ALstatuscode));
	}
	while (ec_iserror()) {
		fprintf(motor_log,"[%ld][ec_motor.c][motor_set_operational] %s\n", time(NULL), ec_elist2string());
	}
	
	return -1;

}

int configure_ec_motor(){

	int nslaves;
	
	nslaves = find_controller();
	

	if(nslaves <= 0){
		write_to_log(motor_log,"ec_motor.c","configure_ec_motor","No slave found");
		return -1;
	}else if(nslaves>1){
		write_to_log(motor_log,"ec_motor.c","configure_ec_motor","Unexpected number of slaves found");
		return -1;
	}
	
	if(motor_pdo_init()<0){
		write_to_log(motor_log,"ec_motor.c","configure_ec_motor","Failed to initialize PDO's");
		return -1;
	}
	
	mc_readPDOassign();
	
	if (ec_config_map(&io_map) <=0){
		write_to_log(motor_log,"ec_motor.c","configure_ec_motor", "Warning ec_config_map returned null map size");
	}
	
	write_to_log(motor_log,"ec_motor.c","configure_ec_motor","Mapping index variables");
	map_index_vars();
	if(!controller_state.slave_error){
		*target_current = 0;
		*control_word = ECAT_CTL_ON | ECAT_CTL_ENABLE_VOLTAGE | ECAT_CTL_QUICK_STOP | ECAT_CTL_ENABLE;
	}

	write_to_log(motor_log,"ec_motor.c","configure_ec_motor","Setting motor defaults");
	set_ec_motor_defaults();
	
	motor_configure_timing();
	
	write_to_log(motor_log,"ec_motor.c","configure_ec_motor","Setting the EtherCAT devices in operational mode.");
	
	motor_set_operational();

	
	if ((controller_state.slave_error == 0) && (controller_state.has_dc == 1)) {
		write_to_log(motor_log,"ec_motor.c","configure_ec_motor","Controller comms OK");
		controller_state.comms_ok = 1;
		comms_ok = 1;
	}else{
		controller_state.comms_ok = 0;
		comms_ok = 0;
	}
	
	write_to_log(motor_log,"ec_motor.c","configure_ec_motor","Writing the drive state to start current mode.");
	ec_SDOwrite16(1, ECAT_DRIVE_STATE, ECAT_DRIVE_STATE_PROG_CURRENT);
	

	
	return 0;

}

int close_ec_motor(void){
    write_to_log(motor_log,"ec_motor.c","close_ec_motor","Closing motors");
    set_current(0.0);
    ec_send_processdata();
    ec_receive_processdata(EC_TIMEOUTRET);
    ec_close(); // Attempt to close down the motors
    return(1);
}

void *do_motors(void*){
	int expectedWKC, wkc;
	int ret;
	int count = 0;
	double t;
	struct timeval current_time;
	char * ifname = config.motor.port;
	int flen = strlen(config.motor.datadir)+25;
	char fname[flen];
	FILE* outfile;
	
	write_to_log(motor_log,"ec_motor.c","do_motors","Initializing NIC...");
	if(!(ec_init(ifname))){
		fprintf(motor_log,"[%ld][ec_motor.c][do_motors] Could not initialize %s\n", time(NULL), ifname);
		comms_ok = -1;
		return (void*) -1;
	}
	fprintf(motor_log,"[%ld][ec_motor.c][do_motors] Initialized %s\n", time(NULL), ifname);
	
	if (configure_ec_motor()<0){
		comms_ok = -1;
		return (void*) -1;
	}
	//reset_ec_motor();
	
	
	expectedWKC = (ec_group[0].outputsWKC*2) + ec_group[0].inputsWKC;
	
	snprintf(fname,flen,"%s/motor_pv_%ld.txt",config.motor.datadir,time(NULL));
	
	outfile = fopen(fname, "w");
	
	start_loop:
		while(!stop){
			gettimeofday(&current_time, NULL);
			t = current_time.tv_sec+current_time.tv_usec/1e6;
			enable();
			ec_send_processdata();
			wkc = ec_receive_processdata(EC_TIMEOUTRET);
			if(*control_word_read != *control_word){
				ready = 0;
				goto reset;
			
			}else{
				ready = 1;
			}
			read_motor_data();
			if (firsttime){
				motor_offset = MotorData[GETREADINDEX(motor_index)].position + TELESCOPE_ANGLE;
				firsttime = 0;
			}
			command_motor();
			fprintf(outfile,"%lf;%lf;%lf;%lf;%d;%d;%d\n",t,MotorData[GETREADINDEX(motor_index)].position,MotorData[GETREADINDEX(motor_index)].velocity,MotorData[GETREADINDEX(motor_index)].current,scan_mode.scan,scan_mode.turnaround,scan_mode.scanning);
			usleep(4600);
		}
	fclose(outfile);
	close_ec_motor();
	
	
	reset:
	if(!stop){
		reset_ec_motor();
		ready = 1;
		goto start_loop;
	}

}
