#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#define MOTORSR 200.0
#define MAX_DI_EL 20.0
#define MAX_I_EL 800.0
#define LPFILTER_POLES 5
#define LPFILTER_GAIN 427169.429//16723.588 
#define VEL 0
#define POS 1
#define NONE 0
#define ENC_DITHER 1

typedef struct{

	int mode;
	int dir;
	double dest;
	double vel;
	int on_target; 

}AxesModeStruct;

typedef struct{
	int mode;
	double start_el;
	double stop_el;
	double vel;
	int start_to_stop;
	int scan;
	int nscans;
	int turnaround;
	int scanning;
}ScanModeStruct;

void command_motor(void);
void go_to_enc(double angle);
int start_motor(void);

extern AxesModeStruct axes_mode;
extern ScanModeStruct scan_mode;
extern pthread_t motors;




#endif
