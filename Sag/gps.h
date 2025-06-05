#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#define GPS_BUFFER_SIZE 1024
#define GPS_PORT "/dev/ttyGPS"
#define GPS_DATA_PATH "/media/saggitarius/T7/GPS_data"
#define GPS_FILE_ROTATION_INTERVAL 600 // 10 minutes in seconds

// GPS data structure to hold the parsed information
typedef struct {
    // Time and date information
    int hour;
    int minute;
    int second;
    int day;
    int month;
    int year;
    
    // Position information
    double latitude;
    double longitude;
    double altitude;
    
    // Heading information
    double heading;
    
    // Status flags
    bool valid_position;
    bool valid_heading;
    
    // Mutex for thread-safe access
    pthread_mutex_t mutex;
    
    // Timestamp of last update
    time_t last_update;
} gps_data_t;

typedef struct {
    char port[256];
    int baud_rate;
    char data_path[256];
    int file_rotation_interval;
} gps_config_t;

// Initialize the GPS system
int gps_init(const gps_config_t *config);

// Start GPS logging
bool gps_start_logging(void);

// Stop GPS logging
void gps_stop_logging(void);

// Check if GPS is logging
bool gps_is_logging(void);

// Get current GPS data (thread-safe)
bool gps_get_data(gps_data_t *data);

// Display GPS status (blocking call, returns when user exits)
void gps_display_status(void);

// Check if status display is active
bool gps_is_status_active(void);

// Stop status display
void gps_stop_status_display(void);

#endif // GPS_H
