#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <LabJackM.h>
#include <pthread.h>

#include "file_io_Oph.h"
#include "housekeeping.h"

// Global variables
int g_hk_handle = -1;
FILE* g_hk_binary_file = NULL;
volatile sig_atomic_t g_hk_running = 1;
time_t g_file_start_time = 0;
char g_session_dir[256];
char g_current_filename[512];

// External configuration
extern struct conf_params config;

// Thread control variables
int housekeeping_running = 0;
int housekeeping_on = 0;
int stop_housekeeping = 0;
pthread_t housekeeping_thread;
FILE* housekeeping_log = NULL;
HousekeepingData latest_housekeeping_data;
pthread_mutex_t housekeeping_data_mutex = PTHREAD_MUTEX_INITIALIZER;

int open_housekeeping_labjack() {
    int handle, err;
    char errStr[HK_MAX_NAME_SIZE];
    
    err = LJM_OpenS("T7", "ETHERNET", HK_LJ_IP, &handle);
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "open_housekeeping_labjack", errStr);
        return -1;
    }
    
    write_to_log(housekeeping_log, "housekeeping.c", "open_housekeeping_labjack", "Connected to LabJack T7");
    
    // Set power pins high
    err = LJM_eWriteName(handle, "FIO4", 1); // MPR power
    if (err == LJME_NOERROR) {
        write_to_log(housekeeping_log, "housekeeping.c", "open_housekeeping_labjack", "FIO4 pin set high (MPR sensor power enabled)");
    } else {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "open_housekeeping_labjack", errStr);
    }
    
    err = LJM_eWriteName(handle, "FIO5", 1); // TMP117 power
    if (err == LJME_NOERROR) {
        write_to_log(housekeeping_log, "housekeeping.c", "open_housekeeping_labjack", "FIO5 pin set high (TMP117 sensor power enabled)");
    } else {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "open_housekeeping_labjack", errStr);
    }
    
    return handle;
}

void close_housekeeping_labjack(int handle) {
    int err;
    char errStr[HK_MAX_NAME_SIZE];
    
    // Set power pins low
    LJM_eWriteName(handle, "FIO4", 0); // MPR power off
    write_to_log(housekeeping_log, "housekeeping.c", "close_housekeeping_labjack", "FIO4 pin set low (MPR sensor power disabled)");
    
    LJM_eWriteName(handle, "FIO5", 0); // TMP117 power off
    write_to_log(housekeeping_log, "housekeeping.c", "close_housekeeping_labjack", "FIO5 pin set low (TMP117 sensor power disabled)");
    
    err = LJM_Close(handle);
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "close_housekeeping_labjack", errStr);
    } else {
        write_to_log(housekeeping_log, "housekeeping.c", "close_housekeeping_labjack", "LabJack connection closed");
    }
}

int read_tmp117_temperature(int handle, double *temperature, int *data_ready) {
    int err, frames;
    char errStr[HK_MAX_NAME_SIZE];
    
    // Configure I2C for TMP117 reading
    err = LJM_eWriteName(handle, "I2C_SDA_DIONUM", HK_TMP117_SDA_PIN);
    err |= LJM_eWriteName(handle, "I2C_SCL_DIONUM", HK_TMP117_SCL_PIN);
    err |= LJM_eWriteName(handle, "I2C_SPEED_THROTTLE", 0);
    err |= LJM_eWriteName(handle, "I2C_OPTIONS", 0);
    err |= LJM_eWriteName(handle, "I2C_SLAVE_ADDRESS", HK_TMP117_ADDRESS);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "read_tmp117_temperature", errStr);
        return -1;
    }
    
    // Read config register to check data ready flag
    unsigned char cmd = HK_TMP117_REG_CONFIG;
    unsigned char cfgBuf[2];
    
    err = LJM_eWriteName(handle, "I2C_NUM_BYTES_TX", 1);
    err |= LJM_eWriteNameByteArray(handle, "I2C_DATA_TX", 1, &cmd, &frames);
    err |= LJM_eWriteName(handle, "I2C_NUM_BYTES_RX", 2);
    err |= LJM_eWriteName(handle, "I2C_GO", 1);
    err |= LJM_eReadNameByteArray(handle, "I2C_DATA_RX", 2, cfgBuf, &frames);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "read_tmp117_temperature", errStr);
        return -1;
    }
    
    unsigned short cfgReg = (cfgBuf[0] << 8) | cfgBuf[1];
    *data_ready = (cfgReg >> 13) & 1;
    
    // Read temperature register
    cmd = HK_TMP117_REG_TEMPERATURE;
    unsigned char tempBuf[2];
    
    err = LJM_eWriteName(handle, "I2C_NUM_BYTES_TX", 1);
    err |= LJM_eWriteNameByteArray(handle, "I2C_DATA_TX", 1, &cmd, &frames);
    err |= LJM_eWriteName(handle, "I2C_NUM_BYTES_RX", 2);
    err |= LJM_eWriteName(handle, "I2C_GO", 1);
    err |= LJM_eReadNameByteArray(handle, "I2C_DATA_RX", 2, tempBuf, &frames);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "read_tmp117_temperature", errStr);
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
    char errStr[HK_MAX_NAME_SIZE];
    
    // Configure I2C for MPR reading
    err = LJM_eWriteName(handle, "I2C_SDA_DIONUM", HK_MPR_SDA_PIN);
    err |= LJM_eWriteName(handle, "I2C_SCL_DIONUM", HK_MPR_SCL_PIN);
    err |= LJM_eWriteName(handle, "I2C_SPEED_THROTTLE", 65516);
    err |= LJM_eWriteName(handle, "I2C_OPTIONS", 0);
    err |= LJM_eWriteName(handle, "I2C_SLAVE_ADDRESS", HK_MPR_ADDRESS);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "read_mpr_pressure", errStr);
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
        write_to_log(housekeeping_log, "housekeeping.c", "read_mpr_pressure", errStr);
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
        write_to_log(housekeeping_log, "housekeeping.c", "read_mpr_pressure", errStr);
        return -1;
    }
    
    // Process data
    *status_byte = result[0];
    unsigned int pressure_counts = (result[1] << 16) | (result[2] << 8) | result[3];
    
    // Calculate pressure in bar
    *pressure_bar = ((pressure_counts - HK_OUTPUT_MIN) * 
                    (HK_PRESSURE_MAX - HK_PRESSURE_MIN) / 
                    (HK_OUTPUT_MAX - HK_OUTPUT_MIN)) + HK_PRESSURE_MIN;
    
    // Convert to other units
    *pressure_torr = *pressure_bar * HK_BAR_TO_TORR;
    *pressure_psi = *pressure_bar * HK_BAR_TO_PSI;
    
    return 0;
}

// Read analog temperature (sensors_logger.c logic)
double read_analog_temperature(int handle, int ain_pin) {
    char channel[16];
    double voltage = 0.0;
    int err;
    char errStr[HK_MAX_NAME_SIZE];

    snprintf(channel, sizeof(channel), "AIN%d", ain_pin);
    err = LJM_eReadName(handle, channel, &voltage);

    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "read_analog_temperature", errStr);
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
    char errStr[HK_MAX_NAME_SIZE];

    snprintf(channel, sizeof(channel), "AIN%d", ain_pin);
    err = LJM_eReadName(handle, channel, &voltage);

    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "read_backend_analog_temperature", errStr);
        return -999.0; // Error value
    }

    // Convert voltage to temperature (LM335: 10mV/K, convert K to C)
    return voltage * 100.0 - 273.15; // backend_temp.c logic
}

int initialize_housekeeping_sensors(int handle) {
    int err, frames;
    char errStr[HK_MAX_NAME_SIZE];
    
    // Configure I2C for TMP117 initialization
    err = LJM_eWriteName(handle, "I2C_SDA_DIONUM", HK_TMP117_SDA_PIN);
    err |= LJM_eWriteName(handle, "I2C_SCL_DIONUM", HK_TMP117_SCL_PIN);
    err |= LJM_eWriteName(handle, "I2C_SPEED_THROTTLE", 0);
    err |= LJM_eWriteName(handle, "I2C_OPTIONS", 0);
    err |= LJM_eWriteName(handle, "I2C_SLAVE_ADDRESS", HK_TMP117_ADDRESS);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "initialize_housekeeping_sensors", errStr);
        return -1;
    }
    
    unsigned short cfg = 0x0000; // Continuous, default averaging
    unsigned char txCfg[3] = {
        HK_TMP117_REG_CONFIG, (unsigned char)(cfg >> 8), (unsigned char)(cfg & 0xFF)
    };
    
    err = LJM_eWriteName(handle, "I2C_NUM_BYTES_TX", 3);
    err |= LJM_eWriteNameByteArray(handle, "I2C_DATA_TX", 3, txCfg, &frames);
    err |= LJM_eWriteName(handle, "I2C_NUM_BYTES_RX", 0);
    err |= LJM_eWriteName(handle, "I2C_GO", 1);
    
    if (err != LJME_NOERROR) {
        LJM_ErrorToString(err, errStr);
        write_to_log(housekeeping_log, "housekeeping.c", "initialize_housekeeping_sensors", errStr);
        return -1;
    }
    
    write_to_log(housekeeping_log, "housekeeping.c", "initialize_housekeeping_sensors", "TMP117 initialized in continuous conversion mode");
    sleep(1); // Allow initial conversion
    
    return 0;
}

FILE* create_housekeeping_binary_file() {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[64];
    char filepath[512];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    // Create session directory if it doesn't exist (only once per session)
    if (strlen(g_session_dir) == 0) {
        strftime(g_session_dir, sizeof(g_session_dir), "%Y%m%d_%H%M%S", timeinfo);
        char full_session_path[512];
        snprintf(full_session_path, sizeof(full_session_path), "%s/%s", 
                config.housekeeping.data_path, g_session_dir);
        
        if (mkdir(full_session_path, 0755) != 0) {
            write_to_log(housekeeping_log, "housekeeping.c", "create_housekeeping_binary_file", 
                        "Failed to create session directory");
        } else {
            write_to_log(housekeeping_log, "housekeeping.c", "create_housekeeping_binary_file", 
                        "Created session directory");
        }
    }
    
    // Create timestamped filename
    strftime(timestamp, sizeof(timestamp), "housekeeping_%Y%m%d_%H%M%S.bin", timeinfo);
    snprintf(filepath, sizeof(filepath), "%s/%s/%s", 
            config.housekeeping.data_path, g_session_dir, timestamp);
    
    FILE *file = fopen(filepath, "wb");
    if (file == NULL) {
        write_to_log(housekeeping_log, "housekeeping.c", "create_housekeeping_binary_file", 
                    "Error creating binary file");
        return NULL;
    }
    
    // Store current filename and start time for rotation
    strcpy(g_current_filename, filepath);
    g_file_start_time = rawtime;
    
    write_to_log(housekeeping_log, "housekeeping.c", "create_housekeeping_binary_file", 
                "Created binary file");
    return file;
}

void write_housekeeping_data_to_file(FILE* file, const HousekeepingData* data) {
    if (file == NULL || data == NULL) {
        return;
    }
    
    size_t written = fwrite(data, sizeof(HousekeepingData), 1, file);
    if (written != 1) {
        write_to_log(housekeeping_log, "housekeeping.c", "write_housekeeping_data_to_file", 
                    "Error writing data to binary file");
    }
    fflush(file); // Ensure data is written immediately
}

void rotate_housekeeping_file() {
    if (g_hk_binary_file) {
        fclose(g_hk_binary_file);
        write_to_log(housekeeping_log, "housekeeping.c", "rotate_housekeeping_file", 
                    "Closed previous binary file");
    }
    
    g_hk_binary_file = create_housekeeping_binary_file();
    if (g_hk_binary_file == NULL) {
        write_to_log(housekeeping_log, "housekeeping.c", "rotate_housekeeping_file", 
                    "Failed to create new binary file");
    }
}

int init_housekeeping_system() {
    // Initialize mutex
    if (pthread_mutex_init(&housekeeping_data_mutex, NULL) != 0) {
        write_to_log(housekeeping_log, "housekeeping.c", "init_housekeeping_system", 
                    "Failed to initialize mutex");
        return -1;
    }
    
    // Clear session directory string
    memset(g_session_dir, 0, sizeof(g_session_dir));
    
    write_to_log(housekeeping_log, "housekeeping.c", "init_housekeeping_system", 
                "Housekeeping system initialized");
    return 0;
}

void* run_housekeeping_thread(void* arg) {
    write_to_log(housekeeping_log, "housekeeping.c", "run_housekeeping_thread", 
                "Housekeeping thread started");
    
    // Open LabJack connection
    g_hk_handle = open_housekeeping_labjack();
    if (g_hk_handle < 0) {
        write_to_log(housekeeping_log, "housekeeping.c", "run_housekeeping_thread", 
                    "Failed to open LabJack connection");
        housekeeping_running = 0;
        return NULL;
    }
    
    // Initialize sensors
    if (initialize_housekeeping_sensors(g_hk_handle) != 0) {
        close_housekeeping_labjack(g_hk_handle);
        housekeeping_running = 0;
        write_to_log(housekeeping_log, "housekeeping.c", "run_housekeeping_thread", 
                    "Failed to initialize sensors");
        return NULL;
    }
    
    // Create initial binary file
    g_hk_binary_file = create_housekeeping_binary_file();
    if (g_hk_binary_file == NULL) {
        close_housekeeping_labjack(g_hk_handle);
        housekeeping_running = 0;
        write_to_log(housekeeping_log, "housekeeping.c", "run_housekeeping_thread", 
                    "Failed to create binary file");
        return NULL;
    }
    
    housekeeping_running = 1;
    time_t start_time = time(NULL);
    
    write_to_log(housekeeping_log, "housekeeping.c", "run_housekeeping_thread", 
                "Starting sensor readings");
    
    while (!stop_housekeeping) {
        HousekeepingData data;
        memset(&data, 0, sizeof(HousekeepingData));
        data.timestamp = difftime(time(NULL), start_time);
        
        // Read TMP117 temperature (OCXO)
        if (read_tmp117_temperature(g_hk_handle, &data.ocxo_temp_c, &data.temp_data_ready) != 0) {
            data.ocxo_temp_c = -999.0;
            data.temp_data_ready = 0;
        }
        
        // Read MPR pressure (Pump-down valve)
        if (read_mpr_pressure(g_hk_handle, &data.pv_pressure_bar, &data.pv_pressure_psi, 
                             &data.pv_pressure_torr, &data.pressure_status) == 0) {
            data.pressure_valid = 1;
        } else {
            data.pv_pressure_bar = -999.0;
            data.pv_pressure_psi = -999.0;
            data.pv_pressure_torr = -999.0;
            data.pressure_status = 0;
            data.pressure_valid = 0;
        }
        
        // Read analog temperature sensors (frontend)
        data.ifamp_temp_c = read_analog_temperature(g_hk_handle, HK_AIN_IFAMP_PIN);
        data.lo_temp_c = read_analog_temperature(g_hk_handle, HK_AIN_LO_PIN);
        data.tec_temp_c = read_analog_temperature(g_hk_handle, HK_AIN_TEC_PIN);
        
        // Read backend analog temperature sensors
        data.backend_chassis_temp_c = read_backend_analog_temperature(g_hk_handle, HK_AIN_BACKEND_CHASSIS_PIN);
        data.nic_temp_c = read_backend_analog_temperature(g_hk_handle, HK_AIN_NIC_PIN);
        data.rfsoc_chassis_temp_c = read_backend_analog_temperature(g_hk_handle, HK_AIN_RFSOC_CHASSIS_PIN);
        data.rfsoc_chip_temp_c = read_backend_analog_temperature(g_hk_handle, HK_AIN_RFSOC_CHIP_PIN);
        
        // Read LNA Box analog temperature sensors
        data.lna1_temp_c = read_backend_analog_temperature(g_hk_handle, HK_AIN_LNA1_PIN);
        data.lna2_temp_c = read_backend_analog_temperature(g_hk_handle, HK_AIN_LNA2_PIN);
        
        // Update shared data structure
        pthread_mutex_lock(&housekeeping_data_mutex);
        latest_housekeeping_data = data;
        pthread_mutex_unlock(&housekeeping_data_mutex);
        
        // Write to binary file
        write_housekeeping_data_to_file(g_hk_binary_file, &data);
        
        // Check for file rotation (every 10 minutes)
        time_t current_time = time(NULL);
        if (difftime(current_time, g_file_start_time) >= config.housekeeping.file_rotation_interval) {
            rotate_housekeeping_file();
        }
        
        sleep(1);
    }
    
    write_to_log(housekeeping_log, "housekeeping.c", "run_housekeeping_thread", 
                "Housekeeping thread stopping");
    
    // Cleanup
    if (g_hk_binary_file) {
        fclose(g_hk_binary_file);
        g_hk_binary_file = NULL;
        write_to_log(housekeeping_log, "housekeeping.c", "run_housekeeping_thread", 
                    "Binary file closed");
    }
    
    close_housekeeping_labjack(g_hk_handle);
    housekeeping_running = 0;
    
    write_to_log(housekeeping_log, "housekeeping.c", "run_housekeeping_thread", 
                "Housekeeping thread ended");
    
    return NULL;
}

int cleanup_housekeeping_system() {
    // Clean up mutex
    pthread_mutex_destroy(&housekeeping_data_mutex);
    
    write_to_log(housekeeping_log, "housekeeping.c", "cleanup_housekeeping_system", 
                "Housekeeping system cleaned up");
    return 0;
}

void shutdown_housekeeping() {
    if (housekeeping_running) {
        stop_housekeeping = 1;
        pthread_join(housekeeping_thread, NULL);
        stop_housekeeping = 0;
    }
    cleanup_housekeeping_system();
}
