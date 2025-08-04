#ifndef CLI_GROUND_H
#define CLI_GROUND_H
#define START_BYTE 0xAA
#define END_BYTE 0x55
#define MAX_DATA 20
#define FIXED_POINT_SCALE 1000
#define MAXBUF 256
#define MAXLINE 1024
#define OPH 0
#define SAG 1
#define BOTH 2

#include <math.h>
#include <stdint.h>

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

typedef struct gs_conf_params{
        char* cmd_log;
	char* sag_ip;
	char* oph_ip;
	int sag_port;
	int oph_port;
	int ls_enabled;
	int timeout;
}gs_conf_params;


void cmdprompt();
void exec_command(char* input);
char* get_input();
uint8_t compute_checksum(const uint8_t* bytes, size_t length);
void rfsoc_boot_timer();
void backend_startup_timer();
void print_help();
void create_packet(Packet* pkt, const uint8_t cmd1, const int16_t* data, const uint8_t small_len, const double_t* bigpack, const uint8_t big_len, const uint8_t dest);
int send_packet(Packet pkt);
int bcp_is_alive();
void get_config_params(char* filepath);
void print_config();
void connect_to_sock();
void print_bvex_banner();

extern gs_conf_params gs_conf;
extern char* hostname;
extern FILE* cmd_log;
#endif

//add command counter
