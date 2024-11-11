#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <time.h>

#define GPS_BUFFER_SIZE 1024
#define GPS_PORT "/dev/ttyGPS"
#define GPS_DATA_PATH "/media/saggitarius/T7/GPS_data"
#define GPS_FILE_ROTATION_INTERVAL 600 // 10 minutes in seconds

typedef struct {
    char port[256];
    int baud_rate;
    char data_path[256];
    int file_rotation_interval;
} gps_config_t;

int gps_init(const gps_config_t *config);
bool gps_start_logging(void);
void gps_stop_logging(void);
bool gps_is_logging(void);

#endif // GPS_H
