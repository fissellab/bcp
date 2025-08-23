#ifndef PR59_INTERFACE_H
#define PR59_INTERFACE_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Fan status enumeration
typedef enum {
    FAN_AUTO = 0,        // Automatic mode (mode 2)
    FAN_FORCED_ON = 1,   // Forced ON (mode 1)
    FAN_FORCED_OFF = 2,  // Forced OFF (mode 0)
    FAN_ERROR = 3        // Error reading status
} pr59_fan_status_t;

// PID update command structure
typedef struct {
    bool update_kp;
    bool update_ki;
    bool update_kd;
    float new_kp;
    float new_ki;
    float new_kd;
} pr59_pid_update_t;

// PR59 telemetry data structure
typedef struct {
    // PID Configuration parameters
    float kp;                    // PID proportional gain
    float ki;                    // PID integral gain  
    float kd;                    // PID derivative gain
    float setpoint_temp;         // Target temperature setpoint in °C
    
    // Real-time telemetry values
    time_t timestamp;            // Timestamp of last update
    float temperature;           // Current temperature in °C
    float fet_temperature;       // FET temperature in °C
    float current;               // Current in Amperes
    float voltage;               // Voltage in Volts
    float power;                 // Power in Watts (calculated as current * voltage)
    
    // Status information
    bool is_running;             // Whether PR59 controller is active
    bool is_heating;             // True if heating, false if cooling
    bool is_at_setpoint;         // True if temperature is within deadband
    pr59_fan_status_t fan_status; // Current fan status
    
    // PID update mechanism
    pr59_pid_update_t pid_update; // Pending PID updates
    bool pid_update_pending;     // Flag for pending PID updates
    
    // Thread safety
    pthread_mutex_t mutex;       // Mutex for thread-safe access
    time_t last_update;          // Timestamp of last data update
} pr59_data_t;

// Initialize PR59 data interface
int pr59_interface_init(void);

// Update PR59 data (called by TEC controller)
void pr59_update_data(float temp, float fet_temp, float current, float voltage, 
                      float kp, float ki, float kd, float setpoint);

// Update fan status (called by TEC controller)
void pr59_update_fan_status(pr59_fan_status_t status);

// Set PID parameter update (called by main process)
void pr59_set_pid_update(float kp, float ki, float kd, bool update_kp, bool update_ki, bool update_kd);

// Get pending PID updates (called by TEC controller)
bool pr59_get_pid_update(pr59_pid_update_t *update);

// Clear pending PID updates (called by TEC controller after processing)
void pr59_clear_pid_update(void);

// Get current PR59 data (thread-safe)
bool pr59_get_data(pr59_data_t *data);

// Check if PR59 is running
bool pr59_is_running(void);

// Get fan status string for telemetry
const char* pr59_get_fan_status_string(pr59_fan_status_t status);

// Cleanup PR59 interface
void pr59_interface_cleanup(void);

#endif // PR59_INTERFACE_H 