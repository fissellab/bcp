#ifndef PBOB_H
#define PBOB_H

#include <stdbool.h>

#define NUM_PBOB 3    
#define NUM_RELAYS 6  
#define DIR_TYPE 1    
#define STATE_TYPE 0  
#define DELAY_US 20000  
#define MAXLEN 1024
#define SHUNT_RESISTOR 0.1 // Ohm
#define CAL_ITER 10 //calibration iterations

typedef struct {
    int relay_id;        
    bool state;   
    int toggle;
    int registerAddress;
    double current; // Current in Amperes
    double curr_offset;
} Relay; 

typedef struct {
    int enabled;
    int handle;
    int num_relays; 
    Relay relays[NUM_RELAYS];
    const char* ip;
    int id;
    FILE* log;
} RelayController; 

// Function prototypes - match the actual implementations
int run_pbob();
int set_toggle(int pbob_id, int relay_id);
int get_state(int pbob_id, int relay_id);
double get_relay_current(int pbob_id, int relay_id);
void* run_pbob_thread(void* arg);
int all_relays_off();

extern RelayController controller[NUM_PBOB];
extern FILE* pbob_log_file;
extern int shutdown_pbob;
extern int pbob_enabled;
extern int pbob_ready;
#endif
