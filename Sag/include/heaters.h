#ifndef HEATER_H
#define HEATER_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Number of heaters
#define NUM_HEATERS 5
#define SHUNT_RESISTOR 200 
#define CAL_ITER 10 //calibration iterations
#define MAXLEN 1024
#define CURRENT_CAP 3 // amps

// Digital I/O registers for heaters (EIO0-EIO4)
#define EIO0_DIR 2008    // Direction register for EIO0 (Heater 0)
#define EIO0_STATE 2008  // State register for EIO0
#define EIO1_DIR 2009    // Direction register for EIO1 (Heater 1)
#define EIO1_STATE 2009  // State register for EIO1
#define EIO2_DIR 2010    // Direction register for EIO2 (Heater 2)
#define EIO2_STATE 2010  // State register for EIO2
#define EIO3_DIR 2011    // Direction register for EIO3 (Heater 3)
#define EIO3_STATE 2011  // State register for EIO3
#define EIO4_DIR 2012    // Direction register for EIO4 (Heater 4)
#define EIO4_STATE 2012  // State register for EIO4

// For direction: dataType = 1, For state: dataType = 0
#define DIR_TYPE 1
#define STATE_TYPE 0

// Temperature thresholds
#define TEMP_LOW 28.0   // Turn ON heater below this temperature
#define TEMP_HIGH 30.0  // Turn OFF heater above this temperature

// Structure to hold heater information
typedef struct {
    bool enabled;
    int eio_dir;      // Direction register address
    int eio_state;    // State register address
    int id;
    const char* ain_channel;  // Analog input channel name
    bool state;       // Current state (ON/OFF)
    double current_temp;  // Current temperature reading
    bool temp_valid;      // Whether the temperature reading is valid
    double current;
    double current_offset;
    bool toggle;
    time_t manual_override_time; // Time when manual command was issued (0 = no override)
} HeaterInfo;

// Function prototypes
void signal_handler(int sig);
int open_labjack(const char* ip);
void close_labjack(int handle);
int read_temperature(int handle, const char* channel, double* temperature_c);
void set_relay_state(int handle, int relay_num, bool state);
void initialize_heaters(HeaterInfo heaters[]);
void print_all_heater_statuses(HeaterInfo heaters[]);
bool heaters_start(void);  // Fixed function name
void* run_heaters_thread(void* arg);
int set_toggle(int relay_id);

extern HeaterInfo heaters[NUM_HEATERS];
extern int shutdown_heaters;
extern int heaters_running;
extern pthread_t main_heaters_thread;  // Changed name for clarity

#endif
