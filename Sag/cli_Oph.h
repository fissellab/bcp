#ifndef CLI_OPH_H
#define CLI_OPH_H
#define START_BYTE 0xAA
#define END_BYTE 0x55
#define MAX_DATA 20
#define MAXLEN 1024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>


enum commands {
    test,
    lock_tel_cmd,
    unlock_tel_cmd,
    reset_lock,
    lockpin_start,
    lockpin_stop,
    motor_stop,
    motor_start,
    bvexcam_solve_stop,
    bvexcam_solve_start,
    bvexcam_save_stop,
    bvexcam_save_start,
    bvexcam_set_exp,
    focus_bvexcam,
    bvexcam_start,
    bvexcam_stop,
    gotoenc,
    encdither,
    enctrack,
    enconoff,
    trackdither,
    stop_scan,
    set_offset_az,
    set_offset_el,
    park,
    receiver_start,
    receiver_stop,
    stop_gps,
    stop_spec,
    stop_spec_120kHz,
    start_gps,
    start_spec,
    start_spec_120kHz,
    rfsoc_on,
    rfsoc_off,
    start_vlbi,
    stop_vlbi,
    start_backend,
    stop_backend,
    rfsoc_configure_ocxo,
    start_timing_chain,
    stop_timing_chain,
    start_ticc,
    stop_ticc,
    start_heater_box,
    start_heaters,
    stop_heaters,
    start_pr59,
    stop_pr59,
    start_position_box,
    stop_position_box,
    position_box_on,
    position_box_off,
    exit_both

};


typedef struct {
    uint8_t start;
    uint8_t num_data;
    uint8_t num_bigpack;
    uint32_t utc;
    uint8_t cmd_primary;
    int16_t destination;
    int16_t data[MAX_DATA];  // or int32_t, if needed
    double_t bigpack[MAX_DATA];
    uint8_t checksum;
    uint8_t end;
} Packet;


void do_commands();
void exec_command(Packet pkt);

extern FILE* main_log;
extern FILE* cmd_log;
extern int bvexcam_on;
extern int lockpin_on;
extern int receiver_on;
extern pthread_t lock_thread;
#endif

