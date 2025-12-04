#ifndef HOUSEKEEPING_H
#define HOUSEKEEPING_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <LabJackM.h>

// ===== PIN CONFIGURATION =====
// LabJack Network Configuration
#define HK_LJ_IP                  "172.20.4.179"

// I2C Sensor Pins
#define HK_TMP117_SDA_PIN         2  // FIO2 for TMP117
#define HK_TMP117_SCL_PIN         3  // FIO3 for TMP117
#define HK_MPR_SDA_PIN            0  // FIO0 for MPR pressure sensor
#define HK_MPR_SCL_PIN            1  // FIO1 for MPR pressure sensor

// Power Control Pins
#define HK_MPR_POWER_PIN          4  // FIO4 - MPR sensor power
#define HK_TMP_POWER_PIN          5  // FIO5 - TMP117 sensor power

// Analog Temperature Sensor Pins (from sensors_logger.c)
#define HK_AIN_IFAMP_PIN          0  // AIN0 - IF Amplifier
#define HK_AIN_LO_PIN             3  // AIN3 - Local Oscillator
#define HK_AIN_TEC_PIN            123  // AIN123 - TEC

// Analog Temperature Sensor Pins (from backend_temp.c)
#define HK_AIN_BACKEND_CHASSIS_PIN    122  // AIN122 - Backend Chassis
#define HK_AIN_NIC_PIN               121  // AIN121 - NIC
#define HK_AIN_RFSOC_CHASSIS_PIN     126  // AIN126 - RFSoC Chassis
#define HK_AIN_RFSOC_CHIP_PIN        127  // AIN127 - RFSoC Chip

// LNA Box Temperature Sensor Pins (LM335)
#define HK_AIN_LNA1_PIN           125  // AIN125 - LNA1
#define HK_AIN_LNA2_PIN           124  // AIN124 - LNA2

// ===== SENSOR CONSTANTS =====
// TMP117 Registers and Constants
#define HK_TMP117_ADDRESS         0x48
#define HK_TMP117_REG_TEMPERATURE 0x00
#define HK_TMP117_REG_CONFIG      0x01

// MPR Sensor Constants
#define HK_MPR_ADDRESS            0x18
#define HK_PRESSURE_MAX           1.72369  // 25 PSI in bar
#define HK_PRESSURE_MIN           0.0
#define HK_OUTPUT_MAX             15099494 // 90% of 2^24 counts
#define HK_OUTPUT_MIN             1677722  // 10% of 2^24 counts
#define HK_BAR_TO_TORR            750.062
#define HK_BAR_TO_PSI             14.5038

// Other Constants
#define HK_MAX_NAME_SIZE          64
#define HK_DATA_RECORD_SIZE       sizeof(HousekeepingData)

// Structure to hold sensor readings
typedef struct {
    double timestamp;
    double ocxo_temp_c;              // OCXO temperature (TMP117 I2C)
    int temp_data_ready;
    double pv_pressure_bar;          // Pump-down valve pressure
    double pv_pressure_psi;
    double pv_pressure_torr;
    unsigned char pressure_status;
    int pressure_valid;
    double ifamp_temp_c;             // IF Amplifier temperature (AIN0)
    double lo_temp_c;                // LO temperature (AIN3)
    double tec_temp_c;               // TEC temperature (AIN123)
    double backend_chassis_temp_c;   // Backend Chassis temperature (AIN122)
    double nic_temp_c;               // NIC temperature (AIN121)
    double rfsoc_chassis_temp_c;     // RFSoC Chassis temperature (AIN126)
    double rfsoc_chip_temp_c;        // RFSoC Chip temperature (AIN127)
    double lna1_temp_c;              // LNA1 temperature (AIN125)
    double lna2_temp_c;              // LNA2 temperature (AIN124)
} HousekeepingData;

// Global variables
extern int housekeeping_running;
extern int housekeeping_on;
extern int stop_housekeeping;
extern pthread_t housekeeping_thread;
extern FILE* housekeeping_log;
extern HousekeepingData latest_housekeeping_data;
extern pthread_mutex_t housekeeping_data_mutex;

// Function prototypes
int init_housekeeping_system();
void* run_housekeeping_thread(void* arg);
int cleanup_housekeeping_system();
void shutdown_housekeeping();

// Sensor reading functions
int open_housekeeping_labjack();
void close_housekeeping_labjack(int handle);
int read_tmp117_temperature(int handle, double *temperature, int *data_ready);
int read_mpr_pressure(int handle, double *pressure_bar, double *pressure_psi, 
                     double *pressure_torr, unsigned char *status_byte);
double read_analog_temperature(int handle, int ain_pin);
double read_backend_analog_temperature(int handle, int ain_pin);
int initialize_housekeeping_sensors(int handle);

// Data logging functions
FILE* create_housekeeping_binary_file();
void write_housekeeping_data_to_file(FILE* file, const HousekeepingData* data);
void rotate_housekeeping_file();

#endif // HOUSEKEEPING_H
