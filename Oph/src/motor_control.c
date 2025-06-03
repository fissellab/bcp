#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <curses.h>
#include <pthread.h>
#include "coords.h"
#include "motor_control.h"
#include "ec_motor.h"
#include "file_io_Oph.h"
#include "lazisusan.h"

AxesModeStruct axes_mode = {
	.dir = 1,
	.on_target = 0,
};
ScanModeStruct scan_mode = {
	.mode = NONE,
	.turnaround = 1,
	.scanning = 0,
};

SkyCoord target = {
	.type = "RaDec",
};

static const double lpfilter_coefs[5] = {256778.303/LPFILTER_GAIN, -1414378.297/LPFILTER_GAIN,//{6019.292/LPFILTER_GAIN, -36208.791/LPFILTER_GAIN, 
							3123497.587/LPFILTER_GAIN, -3457533.033/LPFILTER_GAIN, //87859.296/LPFILTER_GAIN,-107614.067/LPFILTER_GAIN,
							1918772.870/LPFILTER_GAIN};//66635.858/LPFILTER_GAIN}; 

extern int motor_index;
extern FILE* motor_log;
pthread_t motors;
extern int comms_ok;
extern double motor_offset;
float p_pub = 0;
float i_pub = 0;
float d_pub = 0;

void lpfilter(float *lp_in,float *lp_out, float x){
	for(int i=1;i<6;i++){
		lp_in[i-1] = lp_in[i];
		lp_out[i-1] = lp_in[i];
	}
	lp_in[5] = x/LPFILTER_GAIN;
	
	lp_out[5] = (lp_in[0] + lp_in[5]) + 5 * (lp_in[1] + lp_in[4])
        	              + 10 * (lp_in[2] + lp_in[3]) + (lpfilter_coefs[0] * lp_out[0])
        	              + (lpfilter_coefs[1] * lp_out[1]) + (lpfilter_coefs[2] * lp_out[2])
        	              + (lpfilter_coefs[3] * lp_out[3]) + (lpfilter_coefs[4] * lp_out[4]);
}

void print_motor_PID(){
	mvprintw(11,0,"Motor P: %lf\n",p_pub);
	mvprintw(12,0,"Motor I: %lf\n",i_pub);
	mvprintw(13,0,"Motor D: %lf\n",d_pub);
}

static int16_t calculate_current(float v_req){
	
	float K_p = 0.0; //proportional gain
	float T_i = 0.0; //Integral time constant
	float T_d = 0.0; //Derivative time constant
	float I_db = 0.0; //deadband current
	
	float error_pv = 0.0;
	float P_term = 0.0;
	float I_step = 0.0;
	float D_term = 0.0;
	
	static float I_term = 0.0;
    	static float lpfilter_in[6] = { 0.0 };
    	static float lpfilter_out[6] = { 0.0 };
    	static float lpc_in[6] = { 0.0 };
    	static float lpc_out[6] = { 0.0 };
    	static float last_pv = 0.0;
	
	int motor_i = GETREADINDEX(motor_index);
	
	float pv = (float) MotorData[motor_index].velocity;
	
	int16_t milliamp_return;
	static int16_t last_milliamp = 0;
	int16_t max_delta_mA = config.motor.max_delta;//5
	static int count = 0;
	double friction = 0.0;
	static double friction_in[2] = {0.0};
	static double friction_out[2] = {0.0};
	static float prev_v = 0.0;

	if(v_req != prev_v){
		I_term = 0.0;
	}

	prev_v = v_req;
	K_p = config.motor.velP; //25.0 Replace these with actual values once we know what they are.
	T_i = config.motor.velI; //0.5
	T_d = config.motor.velD; //1.0
	I_db = config.motor.velI_db;//10.0



	error_pv = v_req - pv;

	P_term = K_p * error_pv;

	I_step = error_pv * K_p/(T_i * MOTORSR);

	if (fabsf(I_step)< I_db){
		I_step = 0;
	}
	
	if (fabsf(I_step)> MAX_DI_EL){
		I_step = copysignf(MAX_DI_EL, I_step);
	}
	
	I_term += I_step;
	
	if(fabsf(I_term)> MAX_I_EL){
		I_term = copysignf(MAX_I_EL, I_term);
	}
	
    	lpfilter(lpfilter_in,lpfilter_out,pv-last_pv);
    	
	       
        D_term = K_p * T_d * MOTORSR * lpfilter_out[5];
        last_pv = pv;
        
        
        milliamp_return = P_term + I_term + D_term;
        
	p_pub = P_term;
	i_pub = I_term;
	d_pub = D_term;

        if (milliamp_return > config.motor.friction_db){
    		friction = config.motor.friction;
    	}else if(milliamp_return < ((-1)*config.motor.friction_db)){
    		friction = (-1) * config.motor.friction; 
    	}else{
    		friction = 0.0;
    	}
    	friction_in[0] = friction_in[1];
    	friction_in[1] = friction / 637.62;
    	friction_out[0] = friction_out[1];
    	friction_out[1] = friction_in[0]+friction_in[1]+0.996863 * friction_out[0];
    	
    	milliamp_return += friction_out[1];
        
        
        if (milliamp_return > last_milliamp + max_delta_mA) {
        	milliamp_return = last_milliamp + max_delta_mA;
    	} else if (milliamp_return < last_milliamp - max_delta_mA) {
        	milliamp_return = last_milliamp - max_delta_mA;
    	}
    	
    	if(milliamp_return > config.motor.max_current){
    		milliamp_return = config.motor.max_current;
    	} else if(milliamp_return < ((-1)*config.motor.max_current)){
    		milliamp_return = (-1)*config.motor.max_current;
    	}
    	
    	
    	last_milliamp = milliamp_return;
    	count++;
    	
    	
    	return milliamp_return;
}

static float calculate_velocity_enc(void){

	float vel;
	double pos,dy;
	int motor_i;
	double vel_gain = config.motor.vel_gain;//3.0
	
	if(axes_mode.mode == VEL){
		vel = axes_mode.vel;
	}else{
		motor_i = GETREADINDEX(motor_index);
		pos = MotorData[motor_i].position;
	
		dy = axes_mode.dest - pos;
		
		if (fabs(dy) < 0.1){
			axes_mode.on_target = 1;
		}else{
			axes_mode.on_target = 0;
		}
	
		if (dy < 0) {
			vel = -sqrt(-dy)*vel_gain;
		}else{
			vel = sqrt(dy)*vel_gain;
		}
		
		if (vel > config.motor.max_velocity){
			vel = config.motor.max_velocity;
		}else if (vel < ((-1)*config.motor.max_velocity)){
			vel = (-1)*config.motor.max_velocity;
		}
	}
	
	return vel;

}

void go_to_enc(double angle){
	
	axes_mode.mode = POS;
	axes_mode.dest = angle;

}

void do_enc_dither(){

	static int firsttime = 1;
	int motor_i;
	double curr_pos;
        double pos_tol;
	
	motor_i = GETREADINDEX(motor_index);

	curr_pos = MotorData[motor_i].position;

	pos_tol = config.motor.pos_tol;
	
	if(firsttime){
		if(scan_mode.start_el>scan_mode.stop_el){
		
			if((curr_pos > scan_mode.start_el+pos_tol) || (curr_pos < scan_mode.start_el)){
				go_to_enc(scan_mode.start_el);
			}else{
				firsttime = 0;
				axes_mode.mode = VEL;
				scan_mode.scan = 0;
				scan_mode.start_to_stop = -1;	
				scan_mode.turnaround = 1;
			
				axes_mode.vel = scan_mode.vel * scan_mode.start_to_stop;
			}
		}else{
			if((curr_pos < scan_mode.start_el-pos_tol) || (curr_pos > scan_mode.start_el)){
				go_to_enc(scan_mode.start_el);
			}else{
				firsttime = 0;
				axes_mode.mode = VEL;
				scan_mode.scan = 0;
				scan_mode.start_to_stop = 1;	
				scan_mode.turnaround = 1;
			
				axes_mode.vel = scan_mode.vel * scan_mode.start_to_stop;
			}
		}
			
	}else{
		if(scan_mode.start_el > scan_mode.stop_el){
			if ((curr_pos > (scan_mode.start_el+pos_tol)) || (curr_pos < (scan_mode.stop_el-pos_tol))){
				if(!scan_mode.turnaround){
					scan_mode.start_to_stop = (-1)*scan_mode.start_to_stop;
					scan_mode.turnaround = 1;
					scan_mode.scan++;
				}
			}else if((curr_pos < scan_mode.start_el) && (curr_pos>scan_mode.stop_el)){
				scan_mode.turnaround = 0;
			}
			
		}else{
			
			if ((curr_pos < (scan_mode.start_el-pos_tol)) || (curr_pos > (scan_mode.stop_el+pos_tol))){
				if(!scan_mode.turnaround){
					scan_mode.start_to_stop = (-1)*scan_mode.start_to_stop;
					scan_mode.turnaround = 1;
					scan_mode.scan++;
				}
			}else if((curr_pos > scan_mode.start_el) && (curr_pos<scan_mode.stop_el)){
				scan_mode.turnaround = 0;
			}
		}
		
		if(scan_mode.scan < scan_mode.nscans){
			axes_mode.vel = scan_mode.vel * scan_mode.start_to_stop;
		}else{
			axes_mode.vel = 0.0;
			scan_mode.scanning = 0;
			scan_mode.scan = 0;
			firsttime = 1;
		}
	
	}

}
void track(){
	double el_delta;
	double az_delta;
	SkyCoord azel_coord;
	axes_mode.mode = POS;
	AzEl_from_RaDec(&target,&azel_coord);
	axes_mode.dest = azel_coord.lat;
	axes_mode.dest_az = azel_coord.lon;

	if(config.lazisusan.enabled && config.motor.enabled){
		el_delta = fabs(MotorData[GETREADINDEX(motor_index)].position-axes_mode.dest);
		az_delta = fabs(axes_mode.dest_az-get_angle());
		if((az_delta < 0.1) && (el_delta <0.1)){
			axes_mode.on_target = 1;
		}else{
			axes_mode.on_target = 0;
		}
	}else if(!config.lazisusan.enabled){
		el_delta = fabs(MotorData[GETREADINDEX(motor_index)].position-axes_mode.dest);
		if(el_delta < 0.1){
			axes_mode.on_target = 1;
		}else{
			axes_mode.on_target = 0;
		}
	}else if(!config.motor.enabled){
		az_delta = fabs(axes_mode.dest_az-get_angle());
		if(az_delta <0.1){
			axes_mode.on_target = 1;
		}else{
			axes_mode.on_target = 0;
		}

	}

}

void enc_onoff(){
	static int firsttime = 1;
	static double t_start;
	double t_now;
	struct timeval time;
	gettimeofday(&time,NULL);
	t_now = time.tv_sec+time.tv_usec/1e6;
	track();
	if(firsttime){
		scan_mode.on_position = 0;
		if(axes_mode.on_target ){
			firsttime = 0;
			scan_mode.on_position = 1;
			t_start = t_now;
		}
	}else if((t_now-t_start)>scan_mode.time){
		t_start = t_now;
		if(scan_mode.on_position){
			scan_mode.on_position = 0;
			scan_mode.scan++;
		}else{
			scan_mode.on_position = 1;
		}
	}
	if(!scan_mode.on_position){
		axes_mode.dest += scan_mode.offset;
	}

}

void command_motor(void){
	
	int16_t current;
	float v_req;
	
	if(scan_mode.scanning){
		if(scan_mode.mode == ENC_DITHER){
			do_enc_dither();
		}else if(scan_mode.mode == ENC_TRACK){
			track();
		}else if(scan_mode.mode == EL_ONOFF){
			enc_onoff();
		}
	}
	
	v_req = calculate_velocity_enc();
	current = calculate_current(v_req);
	set_current(current);
}

void set_el_offset(double cal_angle){
	double curr_ang = MotorData[GETREADINDEX(motor_index)].position;
	motor_offset = motor_offset -cal_angle+curr_ang;
}

int start_motor(void){
	pthread_create(&motors,NULL,do_motors,NULL);
	while(!ready){
		if(ready && (comms_ok==1)){
			axes_mode.mode = VEL;
			axes_mode.vel = 0.0;
			axes_mode.dest = 0.0;
			return 1;
		}else if((ready && (comms_ok==0)) || (comms_ok == -1)){
			return 0;
		}

	}

}
