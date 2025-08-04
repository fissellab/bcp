#ifndef POSITION_SENSORS_H
#define POSITION_SENSORS_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

// Position sensor data structures (matching pos_sensor_tx.c format)
typedef struct __attribute__((packed)) {
    uint32_t magic;           // 0xDEADBEEF for validation
    uint32_t timestamp_sec;   // Unix timestamp seconds
    uint32_t timestamp_nsec;  // Nanoseconds
    uint16_t sequence;        // Packet sequence number
    uint8_t  sensor_mask;     // Bit flags: which sensors have data
    uint8_t  reserved;        // Padding for alignment
} pos_packet_header_t;

typedef struct __attribute__((packed)) {
    uint8_t sensor_id;        // 1-3 for accelerometers
    float x, y, z;           // m/s²
} pos_accel_sample_t;

typedef struct __attribute__((packed)) {
    float rate;              // degrees/sec
} pos_gyro_spi_sample_t;

typedef struct __attribute__((packed)) {
    float x, y, z;           // degrees/sec
    float temperature;       // °C
} pos_gyro_i2c_sample_t;

typedef struct __attribute__((packed)) {
    pos_packet_header_t header;
    pos_accel_sample_t accels[3];          // Always present (1000Hz)
    pos_gyro_i2c_sample_t gyro_i2c;        // Always present (1000Hz)
    pos_gyro_spi_sample_t gyro_spi;        // Present every 4th packet (250Hz)
} pos_sensor_packet_t;

// Position sensor configuration
typedef struct {
    int enabled;
    char pi_ip[16];
    int pi_port;
    char data_save_path[256];
    int connection_timeout;
    int retry_attempts;
    char script_path[256];
    int telemetry_rate_hz;
} pos_sensor_config_t;

// Position sensor status and data
typedef struct {
    bool connected;
    bool script_running;
    bool data_active;
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t total_samples[5];  // 3 accel + 1 i2c gyro + 1 spi gyro
    double last_packet_time;
    char last_error[256];
    
    // Latest SPI gyro data for telemetry (10Hz rate)
    pos_gyro_spi_sample_t latest_spi_gyro;
    double spi_gyro_timestamp;
    pthread_mutex_t spi_gyro_mutex;
    
    // Latest I2C gyro data for telemetry
    pos_gyro_i2c_sample_t latest_i2c_gyro;
    double i2c_gyro_timestamp;
    pthread_mutex_t i2c_gyro_mutex;
    
    // Latest accelerometer data for telemetry
    pos_accel_sample_t latest_accels[3];
    double accel_timestamps[3];
    pthread_mutex_t accel_mutex;
} pos_sensor_status_t;

// Function prototypes
int position_sensors_init(const pos_sensor_config_t *config);
bool position_sensors_start(void);
void position_sensors_stop(void);
bool position_sensors_is_enabled(void);
bool position_sensors_is_running(void);
int position_sensors_get_status(pos_sensor_status_t *status);

// Telemetry data access functions
bool position_sensors_get_spi_gyro_data(pos_gyro_spi_sample_t *data, double *timestamp);
bool position_sensors_get_i2c_gyro_data(pos_gyro_i2c_sample_t *data, double *timestamp);
bool position_sensors_get_accel_data(int sensor_id, pos_accel_sample_t *data, double *timestamp);

// Cleanup function
void position_sensors_cleanup(void);

#endif // POSITION_SENSORS_H 