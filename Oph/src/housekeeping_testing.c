#include <LabJackM.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

// ===== PIN CONFIGURATION =====
// LabJack Network Configuration
#define LJ_IP                  "172.20.4.179"

// I2C Sensor Pins
#define TMP117_SDA_PIN         2  // FIO2 for TMP117
#define TMP117_SCL_PIN         3  // FIO3 for TMP117
#define MPR_SDA_PIN            0  // FIO0 for MPR pressure sensor
#define MPR_SCL_PIN            1  // FIO1 for MPR pressure sensor

// Power Control Pins
#define MPR_POWER_PIN          4  // FIO4 - MPR sensor power
#define TMP_POWER_PIN          5  // FIO5 - TMP117 sensor power

// Analog Temperature Sensor Pins (from sensors_logger.c)
#define AIN_SENSOR1_PIN        0  // AIN0
#define AIN_SENSOR2_PIN        3  // AIN3
#define AIN_SENSOR3_PIN        123  // AIN123

// Analog Temperature Sensor Pins (from backend_temp.c)
#define AIN_BACKEND1_PIN       122  // AIN122
#define AIN_BACKEND2_PIN       121  // AIN121
#define AIN_BACKEND3_PIN       126  // AIN126
#define AIN_BACKEND4_PIN       127  // AIN127

// LNA Box Temperature Sensor Pins (LM335)
#define AIN_LNA1_PIN           124  // AIN124
#define AIN_LNA2_PIN           125  // AIN125

// ===== SENSOR CONSTANTS =====
// TMP117 Registers and Constants
#define TMP117_ADDRESS         0x48
#define TMP117_REG_TEMPERATURE 0x00
#define TMP117_REG_CONFIG      0x01

// MPR Sensor Constants
#define MPR_ADDRESS            0x18
#define PRESSURE_MAX           1.72369  // 25 PSI in bar
#define PRESSURE_MIN           0.0
#define OUTPUT_MAX             15099494 // 90% of 2^24 counts
#define OUTPUT_MIN             1677722  // 10% of 2^24 counts
#define BAR_TO_TORR            750.062
#define BAR_TO_PSI             14.5038

// Other Constants
#define MAX_NAME_SIZE          64

// Global variables
int g_handle = -1;
FILE *g_csv_file = NULL;
volatile sig_atomic_t g_running = 1;

// Structure to hold sensor readings
typedef struct {
    double timestamp;
    double temperature_c;
    int temp_data_ready;
    double pressure_bar;
    double pressure_psi;
    double pressure_torr;
    unsigned char pressure_status;
    int pressure_valid;
    double ain1_temp_c;
    double ain2_temp_c;
    double ain3_temp_c;
    double backend1_temp_c;
    double backend2_temp_c;
    double backend3_temp_c;
    double backend4_temp_c;
    double lna1_temp_c;
    double lna2_temp_c;
} SensorData;

void signal_handler(int sig) {
    g_running = 0;
}

int open_labjack() {
    int handle, err;
    char errStr[MAX_NAME_SIZE];
    
    err = LJM_OpenS("T7", "ETHERNET", LJ_IP, &handle);
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error opening LabJack: %s\n", errStr);
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to LabJack T7 (IP: %s)\n", LJ_IP);
    
    // Set power pins high
    err = LJM_eWriteName(handle, "FIO4", 1); // MPR power
    if (err == LJME_NOERROR) {
        printf("FIO4 pin set high (MPR sensor power enabled)\n");
    } else {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error setting FIO4 high: %s\n", errStr);
    }
    
    err = LJM_eWriteName(handle, "FIO5", 1); // TMP117 power
    if (err == LJME_NOERROR) {
        printf("FIO5 pin set high (TMP117 sensor power enabled)\n");
    } else {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error setting FIO5 high: %s\n", errStr);
    }
    
    return handle;
}

void close_labjack(int handle) {
    int err;
    char errStr[MAX_NAME_SIZE];
    
    // Set power pins low
    LJM_eWriteName(handle, "FIO4", 0); // MPR power off
    printf("FIO4 pin set low (MPR sensor power disabled)\n");
    
    LJM_eWriteName(handle, "FIO5", 0); // TMP117 power off
    printf("FIO5 pin set low (TMP117 sensor power disabled)\n");
    
    err = LJM_Close(handle);
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error closing LabJack: %s\n", errStr);
    } else {
        printf("LabJack connection closed\n");
    }
}

int read_tmp117_temperature(int handle, double *temperature, int *data_ready) {
    int err, frames;
    char errStr[MAX_NAME_SIZE];
    
    // Configure I2C for TMP117 reading
    err = LJM_eWriteName(handle, "I2C_SDA_DIONUM", TMP117_SDA_PIN);
    err |= LJM_eWriteName(handle, "I2C_SCL_DIONUM", TMP117_SCL_PIN);
    err |= LJM_eWriteName(handle, "I2C_SPEED_THROTTLE", 0);
    err |= LJM_eWriteName(handle, "I2C_OPTIONS", 0);
    err |= LJM_eWriteName(handle, "I2C_SLAVE_ADDRESS", TMP117_ADDRESS);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error configuring I2C for TMP117: %s\n", errStr);
        return -1;
    }
    
    // Read config register to check data ready flag
    unsigned char cmd = TMP117_REG_CONFIG;
    unsigned char cfgBuf[2];
    
    err = LJM_eWriteName(handle, "I2C_NUM_BYTES_TX", 1);
    err |= LJM_eWriteNameByteArray(handle, "I2C_DATA_TX", 1, &cmd, &frames);
    err |= LJM_eWriteName(handle, "I2C_NUM_BYTES_RX", 2);
    err |= LJM_eWriteName(handle, "I2C_GO", 1);
    err |= LJM_eReadNameByteArray(handle, "I2C_DATA_RX", 2, cfgBuf, &frames);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error reading TMP117 config: %s\n", errStr);
        return -1;
    }
    
    unsigned short cfgReg = (cfgBuf[0] << 8) | cfgBuf[1];
    *data_ready = (cfgReg >> 13) & 1;
    
    // Read temperature register
    cmd = TMP117_REG_TEMPERATURE;
    unsigned char tempBuf[2];
    
    err = LJM_eWriteName(handle, "I2C_NUM_BYTES_TX", 1);
    err |= LJM_eWriteNameByteArray(handle, "I2C_DATA_TX", 1, &cmd, &frames);
    err |= LJM_eWriteName(handle, "I2C_NUM_BYTES_RX", 2);
    err |= LJM_eWriteName(handle, "I2C_GO", 1);
    err |= LJM_eReadNameByteArray(handle, "I2C_DATA_RX", 2, tempBuf, &frames);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error reading TMP117 temperature: %s\n", errStr);
        return -1;
    }
    
    // Convert to temperature
    unsigned short raw = (tempBuf[0] << 8) | tempBuf[1];
    *temperature = (raw & 0x8000) ? -(((~raw) + 1) * 0.0078125) : raw * 0.0078125;
    
    return 0;
}

int read_mpr_pressure(int handle, double *pressure_bar, double *pressure_psi, 
                     double *pressure_torr, unsigned char *status_byte) {
    int err, frames;
    char errStr[MAX_NAME_SIZE];
    
    // Configure I2C for MPR reading
    err = LJM_eWriteName(handle, "I2C_SDA_DIONUM", MPR_SDA_PIN);
    err |= LJM_eWriteName(handle, "I2C_SCL_DIONUM", MPR_SCL_PIN);
    err |= LJM_eWriteName(handle, "I2C_SPEED_THROTTLE", 65516);
    err |= LJM_eWriteName(handle, "I2C_OPTIONS", 0);
    err |= LJM_eWriteName(handle, "I2C_SLAVE_ADDRESS", MPR_ADDRESS);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error configuring I2C for MPR: %s\n", errStr);
        return -1;
    }
    
    // Send measurement command (0xAA)
    unsigned char cmd = 0xAA;
    err = LJM_eWriteName(handle, "I2C_NUM_BYTES_TX", 1);
    err |= LJM_eWriteName(handle, "I2C_NUM_BYTES_RX", 0);
    err |= LJM_eWriteNameByteArray(handle, "I2C_DATA_TX", 1, &cmd, &frames);
    err |= LJM_eWriteName(handle, "I2C_GO", 1);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error sending MPR command: %s\n", errStr);
        return -1;
    }
    
    // Wait for measurement
    usleep(50000); // 50ms
    
    // Read the result
    unsigned char result[4];
    err = LJM_eWriteName(handle, "I2C_NUM_BYTES_TX", 0);
    err |= LJM_eWriteName(handle, "I2C_NUM_BYTES_RX", 4);
    err |= LJM_eWriteName(handle, "I2C_GO", 1);
    err |= LJM_eReadNameByteArray(handle, "I2C_DATA_RX", 4, result, &frames);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error reading MPR data: %s\n", errStr);
        return -1;
    }
    
    // Process data
    *status_byte = result[0];
    unsigned int pressure_counts = (result[1] << 16) | (result[2] << 8) | result[3];
    
    // Calculate pressure in bar
    *pressure_bar = ((pressure_counts - OUTPUT_MIN) * 
                    (PRESSURE_MAX - PRESSURE_MIN) / 
                    (OUTPUT_MAX - OUTPUT_MIN)) + PRESSURE_MIN;
    
    // Convert to other units
    *pressure_torr = *pressure_bar * BAR_TO_TORR;
    *pressure_psi = *pressure_bar * BAR_TO_PSI;
    
    return 0;
}

// Read analog temperature (sensors_logger.c logic)
double read_analog_temperature(int handle, int ain_pin) {
    char channel[16];
    double voltage = 0.0;
    int err;
    char errStr[MAX_NAME_SIZE];

    snprintf(channel, sizeof(channel), "AIN%d", ain_pin);
    err = LJM_eReadName(handle, channel, &voltage);

    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error reading %s: %s\n", channel, errStr);
        return -999.0; // Error value
    }

    // LM335 outputs 10mV per degree Kelvin
    return (voltage * 100.0); // Convert to Celsius (sensors_logger.c logic)
}

// Read backend analog temperature (backend_temp.c logic)
double read_backend_analog_temperature(int handle, int ain_pin) {
    char channel[16];
    double voltage = 0.0;
    int err;
    char errStr[MAX_NAME_SIZE];

    snprintf(channel, sizeof(channel), "AIN%d", ain_pin);
    err = LJM_eReadName(handle, channel, &voltage);

    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error reading %s: %s\n", channel, errStr);
        return -999.0; // Error value
    }

    // Convert voltage to temperature (LM335: 10mV/K, convert K to C)
    return voltage * 100.0 - 273.15; // backend_temp.c logic
}

FILE* create_csv_file() {
    time_t rawtime;
    struct tm *timeinfo;
    char filename[256];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    strftime(filename, sizeof(filename), "housekeeping_data_%Y%m%d_%H%M%S.csv", timeinfo);
    
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "Error creating CSV file: %s\n", filename);
        return NULL;
    }
    
    // Write CSV header
    fprintf(file, "Timestamp,OCXO_Temp_C,Temp_DataReady,PV_Pressure_Bar,PV_Pressure_PSI,PV_Pressure_Torr,Pressure_Status,Pressure_Valid,IF_AMP_Temp_C,LO_Temp_C,TEC_Temp_C,Backend_Chassis_Temp_C,NIC_Temp_C,RFSoC_Chassis_Temp_C,RFSoC_Chip_Temp_C,LNA1_Temp_C,LNA2_Temp_C\n");
    
    printf("Created CSV file: %s\n", filename);
    return file;
}

int initialize_sensors(int handle) {
    int err, frames;
    
    // Configure I2C for TMP117 initialization
    err = LJM_eWriteName(handle, "I2C_SDA_DIONUM", TMP117_SDA_PIN);
    err |= LJM_eWriteName(handle, "I2C_SCL_DIONUM", TMP117_SCL_PIN);
    err |= LJM_eWriteName(handle, "I2C_SPEED_THROTTLE", 0);
    err |= LJM_eWriteName(handle, "I2C_OPTIONS", 0);
    err |= LJM_eWriteName(handle, "I2C_SLAVE_ADDRESS", TMP117_ADDRESS);
    
    if (err != LJME_NOERROR) {
        char errStr[MAX_NAME_SIZE];
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error configuring I2C for TMP117 init: %s\n", errStr);
        return -1;
    }
    
    unsigned short cfg = 0x0000; // Continuous, default averaging
    unsigned char txCfg[3] = {
        TMP117_REG_CONFIG, (unsigned char)(cfg >> 8), (unsigned char)(cfg & 0xFF)
    };
    
    err = LJM_eWriteName(handle, "I2C_NUM_BYTES_TX", 3);
    err |= LJM_eWriteNameByteArray(handle, "I2C_DATA_TX", 3, txCfg, &frames);
    err |= LJM_eWriteName(handle, "I2C_NUM_BYTES_RX", 0);
    err |= LJM_eWriteName(handle, "I2C_GO", 1);
    
    if (err != LJME_NOERROR) {
        char errStr[MAX_NAME_SIZE];
        LJM_ErrorToString(err, errStr);
        fprintf(stderr, "Error initializing TMP117: %s\n", errStr);
        return -1;
    }
    
    printf("TMP117 initialized in continuous conversion mode\n");
    sleep(1); // Allow initial conversion
    
    return 0;
}

int main() {
    // Set up signal handler for graceful exit
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Housekeeping Sensor Logger - Combined TMP117, MPR & Analog Temperature Sensors\n");
    printf("=============================================================================\n");
    
    // Open LabJack connection
    g_handle = open_labjack();
    
    // Initialize sensors
    if (initialize_sensors(g_handle) != 0) {
        close_labjack(g_handle);
        return EXIT_FAILURE;
    }
    
    // Create CSV file
    g_csv_file = create_csv_file();
    if (g_csv_file == NULL) {
        close_labjack(g_handle);
        return EXIT_FAILURE;
    }
    
    printf("\nStarting sensor readings... Press Ctrl+C to stop\n");
    printf("Time(s)  | OCXO(°C)   | DR | PV(bar)    | PV(PSI)    | IF_AMP(°C) | LO(°C)     | TEC(°C)    | Backend(°C)| NIC(°C)    | RFSoC_Ch(°C)| RFSoC_Cp(°C)| LNA1(°C)   | LNA2(°C)\n");
    printf("---------|------------|----|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------\n");
    
    time_t start_time = time(NULL);
    
    while (g_running) {
        SensorData data;
        data.timestamp = difftime(time(NULL), start_time);
        
        // Read TMP117 temperature
        if (read_tmp117_temperature(g_handle, &data.temperature_c, &data.temp_data_ready) == 0) {
            // Temperature read successfully
        } else {
            data.temperature_c = -999.0;
            data.temp_data_ready = 0;
        }
        
        // Read MPR pressure
        if (read_mpr_pressure(g_handle, &data.pressure_bar, &data.pressure_psi, 
                             &data.pressure_torr, &data.pressure_status) == 0) {
            data.pressure_valid = 1;
        } else {
            data.pressure_bar = -999.0;
            data.pressure_psi = -999.0;
            data.pressure_torr = -999.0;
            data.pressure_status = 0;
            data.pressure_valid = 0;
        }
        
        // Read analog temperature sensors (sensors_logger.c style)
        data.ain1_temp_c = read_analog_temperature(g_handle, AIN_SENSOR1_PIN);
        data.ain2_temp_c = read_analog_temperature(g_handle, AIN_SENSOR2_PIN);
        data.ain3_temp_c = read_analog_temperature(g_handle, AIN_SENSOR3_PIN);
        
        // Read backend analog temperature sensors (backend_temp.c style)
        data.backend1_temp_c = read_backend_analog_temperature(g_handle, AIN_BACKEND1_PIN);
        data.backend2_temp_c = read_backend_analog_temperature(g_handle, AIN_BACKEND2_PIN);
        data.backend3_temp_c = read_backend_analog_temperature(g_handle, AIN_BACKEND3_PIN);
        data.backend4_temp_c = read_backend_analog_temperature(g_handle, AIN_BACKEND4_PIN);
        
        // Read LNA Box analog temperature sensors (LM335 style)
        data.lna1_temp_c = read_backend_analog_temperature(g_handle, AIN_LNA1_PIN);
        data.lna2_temp_c = read_backend_analog_temperature(g_handle, AIN_LNA2_PIN);
        
        // Print to console
        char status_str[16];
        if (data.pressure_valid && (data.pressure_status & 0x40)) {
            strcpy(status_str, "ON,RDY");
        } else {
            strcpy(status_str, "OFF/ERR");
        }
        
        printf("\r%7.1f  | %10.4f | %2d | %9.4f | %9.2f | %9.2f | %9.2f | %9.2f | %9.2f | %9.2f | %9.2f | %9.2f | %9.2f | %9.2f",
               data.timestamp, data.temperature_c, data.temp_data_ready,
               data.pressure_bar, data.pressure_psi,
               data.ain1_temp_c == -999.0 ? 0.0 : data.ain1_temp_c,      // IF_AMP (AIN0)
               data.ain2_temp_c == -999.0 ? 0.0 : data.ain2_temp_c,      // LO (AIN3)
               data.ain3_temp_c == -999.0 ? 0.0 : data.ain3_temp_c,      // TEC (AIN123)
               data.backend1_temp_c == -999.0 ? 0.0 : data.backend1_temp_c, // Backend (AIN122)
               data.backend2_temp_c == -999.0 ? 0.0 : data.backend2_temp_c, // NIC (AIN121)
               data.backend3_temp_c == -999.0 ? 0.0 : data.backend3_temp_c, // RFSoC Chassis (AIN126)
               data.backend4_temp_c == -999.0 ? 0.0 : data.backend4_temp_c, // RFSoC Chip (AIN127)
               data.lna2_temp_c == -999.0 ? 0.0 : data.lna2_temp_c,      // LNA1 (AIN125)
               data.lna1_temp_c == -999.0 ? 0.0 : data.lna1_temp_c);     // LNA2 (AIN124)
        fflush(stdout);
        
        // Write to CSV
        fprintf(g_csv_file, "%.1f,%.4f,%d,%.4f,%.2f,%.1f,0x%02X,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                data.timestamp, data.temperature_c, data.temp_data_ready,
                data.pressure_bar, data.pressure_psi, data.pressure_torr,
                data.pressure_status, data.pressure_valid,
                data.ain1_temp_c == -999.0 ? 0.0 : data.ain1_temp_c,      // IF_AMP (AIN0)
                data.ain2_temp_c == -999.0 ? 0.0 : data.ain2_temp_c,      // LO (AIN3)
                data.ain3_temp_c == -999.0 ? 0.0 : data.ain3_temp_c,      // TEC (AIN123)
                data.backend1_temp_c == -999.0 ? 0.0 : data.backend1_temp_c, // Backend Chassis (AIN122)
                data.backend2_temp_c == -999.0 ? 0.0 : data.backend2_temp_c, // NIC (AIN121)
                data.backend3_temp_c == -999.0 ? 0.0 : data.backend3_temp_c, // RFSoC Chassis (AIN126)
                data.backend4_temp_c == -999.0 ? 0.0 : data.backend4_temp_c, // RFSoC Chip (AIN127)
                data.lna2_temp_c == -999.0 ? 0.0 : data.lna2_temp_c,      // LNA1 (AIN125)
                data.lna1_temp_c == -999.0 ? 0.0 : data.lna1_temp_c);     // LNA2 (AIN124)
        
        fflush(g_csv_file); // Ensure data is written immediately
        
        sleep(1);
    }
    
    printf("\n\nProgram interrupted by user\n");
    
    // Cleanup
    if (g_csv_file) {
        fclose(g_csv_file);
        printf("CSV file closed\n");
    }
    
    close_labjack(g_handle);
    
    return EXIT_SUCCESS;
}