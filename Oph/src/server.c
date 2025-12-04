#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "file_io_Oph.h"
#include "server.h"
#include "lens_adapter.h"
#include "astrometry.h"
#include "motor_control.h"
#include "ec_motor.h"
#include "pbob.h"
#include "system_monitor.h"
#include "housekeeping.h"
#include "lockpin.h"

struct sockaddr_in cliaddr;
int tel_server_running = 0;
int stop_tel = 0;
FILE* server_log;

extern struct astrometry all_astro_params;
extern struct camera_params all_camera_params;
extern AxesModeStruct axes_mode;
extern ScanModeStruct scan_mode;
extern SkyCoord target;
extern float p_pub;
extern float i_pub;
extern float d_pub;
extern int is_locked;

void sendString(int sockfd, char* string_sample){

	sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,(const struct sockaddr *) &cliaddr, sizeof(cliaddr));
	return;
}

void sendInt(int sockfd, int sample){
	char string_sample[6];

	snprintf(string_sample,6,"%d",sample);
        sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,(const struct sockaddr *) &cliaddr, sizeof(cliaddr));
        return;
}

void sendFloat(int sockfd, float sample){
        char string_sample[10];

        snprintf(string_sample,10,"%f",sample);
        sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,(const struct sockaddr *) &cliaddr, sizeof(cliaddr));
        return;
}

void sendDouble(int sockfd,double sample){
	char string_sample[10];

	snprintf(string_sample,10,"%lf",sample);
	sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,(const struct sockaddr *) &cliaddr, sizeof(cliaddr));
	return;
}

//Initialize the socket
int init_socket(){
	int sockfd = socket(AF_INET,SOCK_DGRAM,0);
	struct sockaddr_in servaddr;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = config.server.timeout;

	if(sockfd < 0){
		write_to_log(server_log,"server.c","init_socket","Socket creation failed");
	}else{
		tel_server_running = 1;

		memset(&servaddr,0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(config.server.port);
		
		if(strcmp(config.server.ip, "0.0.0.0") == 0){
			servaddr.sin_addr.s_addr = INADDR_ANY;
		}else{
			servaddr.sin_addr.s_addr = inet_addr(config.server.ip);
		}
		
		// Bind the socket to the server address
		if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
			write_to_log(server_log,"server.c","init_socket","Socket bind failed");
			tel_server_running = 0;
			close(sockfd);
			return -1;
		}
		
		setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));

		write_to_log(server_log,"server.c","init_socket","Telemetry started successfully");
	}

	return sockfd;
}

//listen for requests
void sock_listen(int sockfd, char* buffer){
	int n;
	socklen_t cliaddr_len = sizeof(cliaddr);
	
	n = recvfrom(sockfd, buffer, MAXLEN-1, MSG_WAITALL, (struct sockaddr *) &cliaddr, &cliaddr_len);

	if(n > 0){
		buffer[n] = '\0';
	}else{
		buffer[0] = '\0';
		if(n < 0){
			write_to_log(server_log,"server.c","sock_listen","Error receiving data");
		}
	}

	return;
}

//check if metric exists if so send the corresponding value
void send_metric(int sockfd, char* id){
	// add channels using the format below here;
	//Star Camera channels
	int motor_i;
	if(strcmp(id,"sc_ra")==0){
		sendDouble(sockfd,all_astro_params.ra);
	}else if(strcmp(id,"sc_dec")==0){
		sendDouble(sockfd,all_astro_params.dec);
	}else if(strcmp(id,"sc_fr")==0){
                sendDouble(sockfd,all_astro_params.fr);
        }else if(strcmp(id,"sc_ir")==0){
                sendDouble(sockfd,all_astro_params.ir);
        }else if(strcmp(id,"sc_alt")==0){
                sendDouble(sockfd,all_astro_params.alt);
        }else if(strcmp(id,"sc_az")==0){
                sendDouble(sockfd,all_astro_params.az);
        }else if(strcmp(id,"sc_lat")==0){
                sendDouble(sockfd,all_astro_params.latitude);
        }else if(strcmp(id,"sc_lon")==0){
                sendDouble(sockfd,all_astro_params.longitude);
        }else if(strcmp(id,"sc_texp")==0){
                sendDouble(sockfd,all_camera_params.exposure_time);
        }else if(strcmp(id,"sc_start_focus")==0){
                sendInt(sockfd,all_camera_params.start_focus_pos);
        }else if(strcmp(id,"sc_end_focus")==0){
                sendInt(sockfd,all_camera_params.end_focus_pos);
        }else if(strcmp(id,"sc_curr_focus")==0){
                sendInt(sockfd,all_camera_params.focus_position);
        }else if(strcmp(id,"sc_focus_step")==0){
                sendInt(sockfd,all_camera_params.focus_step);
        }else if(strcmp(id,"sc_focus_mode")==0){
                sendInt(sockfd,all_camera_params.focus_mode);
        }else if(strcmp(id,"sc_solve")==0){
                sendInt(sockfd,all_camera_params.solve_img);
        }else if(strcmp(id,"sc_save")==0){
                sendInt(sockfd,all_camera_params.save_image);
        }else if(strcmp(id,"mc_curr")==0){
               motor_i = GETREADINDEX(motor_index);
               sendDouble(sockfd,MotorData[motor_i].current);
       }else if(strcmp(id,"mc_sw")==0){
                motor_i = GETREADINDEX(motor_index);
                sendInt(sockfd,MotorData[motor_i].drive_info);
        }else if(strcmp(id,"mc_lf")==0){
                motor_i = GETREADINDEX(motor_index);
                sendInt(sockfd,MotorData[motor_i].fault_reg);
        }else if(strcmp(id,"mc_sr")==0){
                motor_i = GETREADINDEX(motor_index);
                sendInt(sockfd,MotorData[motor_i].status);
        }else if(strcmp(id,"mc_pos")==0){
                motor_i = GETREADINDEX(motor_index);
                sendDouble(sockfd,MotorData[motor_i].position);
        }else if(strcmp(id,"mc_temp")==0){
                motor_i = GETREADINDEX(motor_index);
                sendInt(sockfd,MotorData[motor_i].temp);
        }else if(strcmp(id,"mc_vel")==0){
                motor_i = GETREADINDEX(motor_index);
                sendDouble(sockfd,MotorData[motor_i].velocity);
        }else if(strcmp(id,"mc_cwr")==0){
                motor_i = GETREADINDEX(motor_index);
                sendInt(sockfd,MotorData[motor_i].control_word_read);
        }else if(strcmp(id,"mc_cww")==0){
                motor_i = GETREADINDEX(motor_index);
                sendInt(sockfd,MotorData[motor_i].control_word_write);
        }else if(strcmp(id,"mc_np")==0){
                motor_i = GETREADINDEX(motor_index);
                sendInt(sockfd,MotorData[motor_i].network_problem);
        }else if(strcmp(id,"mc_pt")==0){
               sendFloat(sockfd,p_pub);
       }else if(strcmp(id,"mc_it")==0){
                sendFloat(sockfd,i_pub);
        }else if(strcmp(id,"mc_dt")==0){
                sendFloat(sockfd,d_pub);
        }else if(strcmp(id,"mc_P")==0){
		sendFloat(sockfd,config.motor.velP);
	}else if(strcmp(id,"mc_I")==0){
                sendFloat(sockfd,config.motor.velI);
        }else if(strcmp(id,"mc_D")==0){
                sendFloat(sockfd,config.motor.velD);
        }else if(strcmp(id,"mc_gv")==0){
		sendFloat(sockfd,config.motor.vel_gain);
	}else if(strcmp(id,"mc_Imax")==0){
                sendInt(sockfd,config.motor.max_current);
        }else if(strcmp(id,"lock_d")==0){
                sendInt(sockfd,config.lockpin.duration);
        }else if(strcmp(id,"lock_state")==0){
                sendInt(sockfd,is_locked);
        }else if(strcmp(id,"ax_mode")==0){
               sendInt(sockfd,axes_mode.mode);
       }else if(strcmp(id,"ax_dest")==0){
                sendDouble(sockfd,axes_mode.dest);
        }else if(strcmp(id,"ax_vel")==0){
                sendDouble(sockfd,axes_mode.vel);
        }else if(strcmp(id,"ax_dest_az")==0){
                sendDouble(sockfd,axes_mode.dest_az);
        }else if(strcmp(id,"ax_vel_az")==0){
                sendDouble(sockfd,axes_mode.vel_az);
        }else if(strcmp(id,"ax_ot")==0){
                sendDouble(sockfd,axes_mode.on_target);
        }else if(strcmp(id,"scan_mode")==0){
                sendInt(sockfd,scan_mode.mode);
        }else if(strcmp(id,"scan_start")==0){
                sendDouble(sockfd,scan_mode.start_el);
        }else if(strcmp(id,"scan_stop")==0){
                sendDouble(sockfd,scan_mode.stop_el);
        }else if(strcmp(id,"scan_vel")==0){
                sendDouble(sockfd,scan_mode.vel);
        }else if(strcmp(id,"scan_scan")==0){
                sendInt(sockfd,scan_mode.scan);
        }else if(strcmp(id,"scan_nscans")==0){
                sendInt(sockfd,scan_mode.nscans);
        }else if(strcmp(id,"scan_offset")==0){
                sendDouble(sockfd,scan_mode.offset);
        }else if(strcmp(id,"scan_time")==0){
               sendDouble(sockfd,scan_mode.time);
        }else if(strcmp(id,"scan_len")==0){
               sendDouble(sockfd,scan_mode.scan_len);
	}else if(strcmp(id,"scan_op")==0){
                sendInt(sockfd,scan_mode.on_position);
        }else if(strcmp(id,"target_lon")==0){
                sendDouble(sockfd,target.lon);
        }else if(strcmp(id,"target_lat")==0){
		sendDouble(sockfd,target.lat);
	}else if(strcmp(id,"target_type")==0){
                sendString(sockfd,target.type);
        }else if(strcmp(id,"sc_state")==0){
		sendInt(sockfd,get_state(config.bvexcam.pbob,config.bvexcam.relay));
        }else if(strcmp(id,"sc_curr")==0){
		sendDouble(sockfd,get_relay_current(config.bvexcam.pbob,config.bvexcam.relay));
	}else if(strcmp(id,"m_state")==0){
                sendInt(sockfd,get_state(config.motor.pbob,config.motor.relay));
        }else if(strcmp(id,"m_curr")==0){
                sendDouble(sockfd,get_relay_current(config.motor.pbob,config.motor.relay));
        }else if(strcmp(id,"lp_state")==0){
                sendInt(sockfd,get_state(config.lockpin.pbob,config.lockpin.relay));
        }else if(strcmp(id,"lp_curr")==0){
                sendDouble(sockfd,get_relay_current(config.lockpin.pbob,config.lockpin.relay));
        }else if(strcmp(id,"lna_state")==0){
                sendInt(sockfd,get_state(config.lna.pbob,config.lna.relay));
        }else if(strcmp(id,"lna_curr")==0){
                sendDouble(sockfd,get_relay_current(config.lna.pbob,config.lna.relay));
        }else if(strcmp(id,"mix_state")==0){
                sendInt(sockfd,get_state(config.mixer.pbob,config.mixer.relay));
        }else if(strcmp(id,"mix_curr")==0){
                sendDouble(sockfd,get_relay_current(config.mixer.pbob,config.mixer.relay));
        }else if(strcmp(id,"rfsoc_state")==0){
                sendInt(sockfd,get_state(config.rfsoc.pbob,config.rfsoc.relay));
        }else if(strcmp(id,"rfsoc_curr")==0){
                sendDouble(sockfd,get_relay_current(config.rfsoc.pbob,config.rfsoc.relay));
        }else if(strcmp(id,"gps_state")==0){
		sendInt(sockfd,get_state(config.gps.pbob,config.gps.relay));
	}else if(strcmp(id,"gps_curr")==0){
                sendDouble(sockfd,get_relay_current(config.gps.pbob,config.gps.relay));
        }else if(strcmp(id,"bkd_state")==0){
                sendInt(sockfd,get_state(config.backend.pbob,config.backend.relay));
        }else if(strcmp(id,"bkd_curr")==0){
                sendDouble(sockfd,get_relay_current(config.backend.pbob,config.backend.relay));
        }else if(strcmp(id,"timing_state")==0){
		sendInt(sockfd,get_state(config.timing_box.pbob,config.timing_box.relay));
	}else if(strcmp(id,"timing_curr")==0){
		sendDouble(sockfd,get_relay_current(config.timing_box.pbob,config.timing_box.relay));
	}else if(strcmp(id,"heat_state")==0){
                sendInt(sockfd,get_state(config.heaters.pbob,config.heaters.relay));
        }else if(strcmp(id,"heat_curr")==0){
                sendDouble(sockfd,get_relay_current(config.heaters.pbob,config.heaters.relay));
        }else if(strcmp(id,"hk_state")==0){
                sendInt(sockfd,get_state(config.housekeeping.pbob,config.housekeeping.relay));
        }else if(strcmp(id,"hk_curr")==0){
                sendDouble(sockfd,get_relay_current(config.housekeeping.pbob,config.housekeeping.relay));
	}else if(strcmp(id,"pos_state")==0){
                sendInt(sockfd,get_state(config.position_box.pbob,config.position_box.relay));
        }else if(strcmp(id,"pos_curr")==0){
                sendDouble(sockfd,get_relay_current(config.position_box.pbob,config.position_box.relay));
        }else if(strcmp(id,"oph_sys_cpu_temp")==0){
                // CPU temperature in Celsius
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendFloat(sockfd, sys_monitor.cpu_temp_celsius);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendFloat(sockfd, -1.0);  // Indicate not available
                }
        }else if(strcmp(id,"oph_sys_cpu_usage")==0){
                // CPU usage percentage
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendFloat(sockfd, sys_monitor.cpu_usage_percent);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendFloat(sockfd, -1.0);  // Indicate not available
                }
        }else if(strcmp(id,"oph_sys_mem_used")==0){
                // Memory used in GB
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendFloat(sockfd, sys_monitor.memory_used_gb);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendFloat(sockfd, -1.0);  // Indicate not available
                }
        }else if(strcmp(id,"oph_sys_mem_total")==0){
                // Total memory in GB
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendFloat(sockfd, sys_monitor.memory_total_gb);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendFloat(sockfd, -1.0);  // Indicate not available
                }
        }else if(strcmp(id,"oph_sys_mem_used_str")==0){
                // Memory used as string with units
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendString(sockfd, sys_monitor.memory_used_str);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendString(sockfd, "N/A");
                }
        }else if(strcmp(id,"oph_sys_mem_total_str")==0){
                // Total memory as string with units
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendString(sockfd, sys_monitor.memory_total_str);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendString(sockfd, "N/A");
                }
        }else if(strcmp(id,"oph_sys_ssd_mounted")==0){
                // SSD mounted status (1 = mounted, 0 = not mounted)
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendInt(sockfd, sys_monitor.ssd_mounted);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendInt(sockfd, -1);  // Indicate not available
                }
        }else if(strcmp(id,"oph_sys_ssd_used")==0){
                // SSD used space as string with units
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendString(sockfd, sys_monitor.ssd_used);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendString(sockfd, "N/A");
                }
        }else if(strcmp(id,"oph_sys_ssd_total")==0){
                // SSD total space as string with units
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendString(sockfd, sys_monitor.ssd_total);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendString(sockfd, "N/A");
                }
        }else if(strcmp(id,"oph_sys_ssd_path")==0){
                // SSD mount path
                if (config.system_monitor.enabled) {
                    pthread_mutex_lock(&sys_monitor.data_mutex);
                    sendString(sockfd, sys_monitor.ssd_mount_path);
                    pthread_mutex_unlock(&sys_monitor.data_mutex);
                } else {
                    sendString(sockfd, "N/A");
                }
        }else if(strcmp(id,"hk_ocxo_temp")==0){
                // OCXO temperature from TMP117 I2C sensor
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.ocxo_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);  // Indicate not available
                }
        }else if(strcmp(id,"hk_ocxo_temp_ready")==0){
                // OCXO temperature data ready flag
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendInt(sockfd, latest_housekeeping_data.temp_data_ready);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendInt(sockfd, 0);
                }
        }else if(strcmp(id,"hk_pv_pressure_bar")==0){
                // Pump-down valve pressure in bar
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.pv_pressure_bar);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_pv_pressure_psi")==0){
                // Pump-down valve pressure in PSI
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.pv_pressure_psi);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_pv_pressure_torr")==0){
                // Pump-down valve pressure in Torr
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.pv_pressure_torr);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_pressure_valid")==0){
                // Pressure measurement validity flag
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendInt(sockfd, latest_housekeeping_data.pressure_valid);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendInt(sockfd, 0);
                }
        }else if(strcmp(id,"hk_ifamp_temp")==0){
                // IF Amplifier temperature (AIN0)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.ifamp_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_lo_temp")==0){
                // Local Oscillator temperature (AIN3)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.lo_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_tec_temp")==0){
                // TEC temperature (AIN123)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.tec_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_backend_chassis_temp")==0){
                // Backend Chassis temperature (AIN122)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.backend_chassis_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_nic_temp")==0){
                // NIC temperature (AIN121)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.nic_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_rfsoc_chassis_temp")==0){
                // RFSoC Chassis temperature (AIN126)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.rfsoc_chassis_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_rfsoc_chip_temp")==0){
                // RFSoC Chip temperature (AIN127)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.rfsoc_chip_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_lna1_temp")==0){
                // LNA1 temperature (AIN125)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.lna1_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_lna2_temp")==0){
                // LNA2 temperature (AIN124)
                if (config.housekeeping.enabled && housekeeping_running) {
                    pthread_mutex_lock(&housekeeping_data_mutex);
                    sendFloat(sockfd, latest_housekeeping_data.lna2_temp_c);
                    pthread_mutex_unlock(&housekeeping_data_mutex);
                } else {
                    sendFloat(sockfd, -999.0);
                }
        }else if(strcmp(id,"hk_running")==0){
                // Housekeeping system running status
                sendInt(sockfd, housekeeping_running);
        }else if(strcmp(id,"hk_powered")==0){
                // Housekeeping system power status
                sendInt(sockfd, housekeeping_on);
        }else{
		fprintf(server_log,"[%ld][server.c][send_metric] Received unknown request: '%s'\n",time(NULL),id);
		fflush(server_log);
	}

}

void *do_server(){

	int sockfd = init_socket();
	char buffer[MAXLEN];

	if(tel_server_running){
		while(!stop_tel){
			sock_listen(sockfd, buffer);
			send_metric(sockfd, buffer);
		}
		write_to_log(server_log,"server.c","do_server","Shutting down server");
		tel_server_running = 0;
		stop_tel = 0;
		fclose(server_log);
		close(sockfd);
	}else{
		write_to_log(server_log,"server.c","do_server","Could not start server");
		fclose(server_log);
	}
}
