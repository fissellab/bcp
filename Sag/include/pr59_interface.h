#ifndef PR59_INTERFACE_H
#define PR59_INTERFACE_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>

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
    
    // Thread safety
    pthread_mutex_t mutex;       // Mutex for thread-safe access
    time_t last_update;          // Timestamp of last data update
} pr59_data_t;

// Initialize PR59 data interface
int pr59_interface_init(void);

// Update PR59 data (called by TEC controller)
void pr59_update_data(float temp, float fet_temp, float current, float voltage, 
                      float kp, float ki, float kd, float setpoint);

// Get current PR59 data (thread-safe)
bool pr59_get_data(pr59_data_t *data);

// Check if PR59 is running
bool pr59_is_running(void);

// Cleanup PR59 interface
void pr59_interface_cleanup(void);

#endif // PR59_INTERFACE_H 