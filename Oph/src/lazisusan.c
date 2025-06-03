#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include <curses.h>
#include "arduino.h"
#include "lazisusan.h"
#include "file_io_Oph.h"
#include "motor_control.h"
#include "astrometry.h"

extern struct conf_params config;
extern struct astrometry all_astro_params;

int motor_enabled = 0;
int cmd_available = 0;
int count_now = 0;
int motor_off = 0;
int az_is_ready = 0;
char motor_cmd[15];
double az_offset=0;
int fd_az = 0;
double cmd_vel;
extern AxesModeStruct axes_mode;
static int turning = 1;
static int first_cmd = 1;
static int forward = 0;
static int backward = 0;
double buff_ang = 0;
FILE *ls_log;

void print_direction(){
	mvprintw(23,0,"Forward: %d", forward);
	mvprintw(24,0,"Backward: %d",backward);
	mvprintw(25,0,"Turning: %d",turning);
	mvprintw(26,0,"Buffer: %lf",buff_ang);
	mvprintw(27,0,"Az offset: %lf",az_offset);
}

double get_angle(){
    double angle;
    double dtheta;
    angle = count_now*0.45/config.lazisusan.gear_ratio+config.lazisusan.offset+az_offset;

    if(angle < 0){
        angle += 360;
    }else if (angle >360){
        angle -= 360;
    }

    return angle;
}

void stretch_belt(){

	axes_mode.mode = POS;
	if(first_cmd){
		if(forward){
			axes_mode.dest_az = get_angle()+config.lazisusan.stretch;
		}else if(backward){
			axes_mode.dest_az = get_angle()-config.lazisusan.stretch;
		}
	}else{
		if(forward){
			axes_mode.dest_az = get_angle()+2*config.lazisusan.stretch;
		}else if(backward){
			axes_mode.dest_az = get_angle()-2*config.lazisusan.stretch;
		}
	}
	if(axes_mode.dest_az > 360){
		axes_mode.dest_az = axes_mode.dest_az - 360;
	}else if(axes_mode.dest_az < 0){
		axes_mode.dest_az = axes_mode.dest_az + 360;
	}

}

double get_act_angle(){
	static double prev = 0;
	double act_angle = 0;
	if(turning){
		if(first_cmd && (axes_mode.mode != POS)){
			prev = get_angle();
		}
		return prev;
	}else{
		act_angle = get_angle();
		prev = act_angle;
		return act_angle;
	}
}

int is_on_target(){

	if(fabs(axes_mode.dest_az-get_angle())<0.06){
		return 1;
	}else{
		return 0;
	}
}

void send_command(int cmd,double freq){

    int tries = 0;

    if(freq>150){
      printf("Frequency too high not sending command\n");
      return;
    }
    while(cmd_available ==1){
      if(tries == 0){
        //printf("Waiting for previous command to send\n");
      }
      tries++;
    }
    snprintf(motor_cmd,15,"%d;%.2f\n",cmd,freq);
    cmd_available = 1;
}

void set_velocity(double velocity){
    double freq;
    static double prev_v = 0;
    double cmd_v;
    double delta_v;
    static double ang_buf = 0;
    static double act_ang = 0;

    if(first_cmd){
        if(velocity>0){
		forward = 1;
		act_ang = get_angle();
		ang_buf = axes_mode.dest_az;
		buff_ang = axes_mode.dest_az;
		stretch_belt();
		first_cmd = 0;
	}else if (velocity < 0){
		backward = 1;
		act_ang = get_angle();
		ang_buf = axes_mode.dest_az;
		buff_ang = axes_mode.dest_az;
		stretch_belt();
		first_cmd = 0;
	}
    }
    if(!turning){
        if((velocity > 0) && backward){
		forward = 1;
                backward = 0;
                turning = 1;
		act_ang = get_angle();
		ang_buf = axes_mode.dest_az;
		buff_ang = axes_mode.dest_az;
		stretch_belt();
	}else if((velocity < 0) && forward){
		forward = 0;
		backward = 1;
		turning = 1;
		act_ang = get_angle();
		ang_buf = axes_mode.dest_az;
		buff_ang = axes_mode.dest_az;
		stretch_belt();
	}
    }else{
        if(is_on_target()&& (axes_mode.mode == POS)){
		set_offset(act_ang);
		axes_mode.dest_az = ang_buf;
		turning = 0;
	}
    }


    delta_v = velocity-prev_v;

    if(delta_v > config.lazisusan.deltav){
       cmd_v = prev_v + config.lazisusan.deltav;
    }else if(delta_v < (-1)*config.lazisusan.deltav){
       cmd_v = prev_v - config.lazisusan.deltav;
    }else{
       cmd_v = velocity;
    }

    freq = fabs(cmd_v) * config.lazisusan.gear_ratio/0.45;
    if (cmd_v>0){
        send_command(1,freq);
    }else{
        send_command(2,freq);
    }
    prev_v = cmd_v;
}

double calculate_velocity(){
    double dphi;
    double vel;
    
    if (axes_mode.mode == VEL){
        vel=axes_mode.vel_az;
    }else if(axes_mode.mode == POS){
        dphi = axes_mode.dest_az - get_angle();
        if (dphi > 180 ){
          dphi -= 360;
        }else if (dphi < -180){
          dphi += 360;
        }
        if(dphi < -0.06){
           vel = (-1)*config.lazisusan.g_az*sqrt(fabs(dphi)); 
        }else if (dphi > 0.06) {
          vel = config.lazisusan.g_az*sqrt(fabs(dphi));
        }else{
          vel = 0;
        }
        if (vel < -3.0){
          vel = -3.0;
        }else if (vel > 3.0){
          vel = 3.0;
        }
    }else{
        vel=0;
    }

    return vel;
}

void move_to(double angle){
    axes_mode.mode = POS;
    axes_mode.dest_az = angle;
}

void enable_disable_motor(){
  serialport_write(fd_az,"3;0\n");
  if (motor_enabled){
    motor_enabled = 0;
  }else{
    motor_enabled = 1;
  }
}

int start_az_motor(char* port, int baud){
    int fd = serialport_init(port,baud);
    int buf_max = 1;
    char buf[buf_max];
    if (fd<0) {
      write_to_log(ls_log,"lazisusan.c","start_motor","Error opening serial port for lazisusan\n");
    }
    sleep(2);
    return fd;
    
}


void stop_motor(){
    if (motor_enabled){
      serialport_write(fd_az,"3;0\n");
      motor_enabled = 0;
    }
    
    serialport_flush(fd_az);
    serialport_close(fd_az);
}

void set_offset(double cal_angle){
    double delta;
    
    delta = cal_angle - get_angle();
    
    az_offset = az_offset + delta;
    
}

int check_sc(){
	static double t_prev;
	double t_now;
	static int firsttime = 1;

	if (firsttime){
		firsttime = 0;
		t_now = all_astro_params.rawtime;
		t_prev = t_now;
		if(all_astro_params.az == 0){
			return 0;
		}else{
			return 1;
		}
	}
	t_now = all_astro_params.rawtime;
	if((t_now>t_prev) && (all_astro_params.az != 0)){
		return 1;
	}else{
		return 0;
	}
}

void * do_az_motor(void*){
  FILE *ls_data;
  struct timeval current_time;
  int buf_max = 10;
  char buf[buf_max];
  int delta = 0;
  fd_az = start_az_motor(config.lazisusan.port,9600);
  int cmd_i = 0;
  double t;
  int flen;
  
  flen = strlen(config.lazisusan.datadir)+26;

  char datafile[flen];
  
  if (fd_az>0){
  	enable_disable_motor();
  	write_to_log(ls_log,"lazisusan.c","do_az_motor","Starting datafile");
	snprintf(datafile,flen,"%s/lazisusan_%ld.txt",config.lazisusan.datadir,time(NULL));
  	ls_data = fopen(datafile,"w");
	if(ls_data == NULL){
		write_to_log(ls_log,"lazisusan.c","do_az_motor","Error opening datafile\n");
	}else{
  		az_is_ready=1;

  		while(!motor_off){
			if(config.bvexcam.enabled){
				if(check_sc()){
					//set_offset(all_astro_params.az);
				}
			}
    			if(motor_enabled && !motor_off){
      			   if(cmd_i==10){
        			cmd_vel = calculate_velocity();
        			set_velocity(cmd_vel);
        			cmd_i = 0;
      			   }else{
        			cmd_i++;
      			   }
    			}
    			if(cmd_available){
      			   if(serialport_write(fd_az,motor_cmd)==0){
          			cmd_available = 0;
      			   }
    			}
    			if (serialport_read_until(fd_az,buf,'\n',buf_max,833)==0){
      			   if (sscanf(buf,"%d",&delta)==1){
        			count_now = delta;
      			   }
    			}
			gettimeofday(&current_time,NULL);
			t = current_time.tv_sec+current_time.tv_usec/1e6;
			fprintf(ls_data,"%lf;%lf\n",t,get_act_angle());
    			usleep(833); 
  		}
  	}
  }
  fclose(ls_data);
  stop_motor();
  fd_az = -1;

}

