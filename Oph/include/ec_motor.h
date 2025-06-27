#ifndef EC_MOTOR_H
#define EC_MOTOR_H

#include <stdint.h>

#define GETREADINDEX(i) ((i+2) % 3)  /* i - 1 modulo 3 */
#define INC_INDEX(i) ((i + 1) %3)    /* i + 1 modulo 3 */

#define MAP(_index, _subindex, _map) {\
    _map.index = _index;\
    _map.subindex = _subindex;\
}

#define ECAT_RXPDO_ASSIGNMENT 0x1c12
#define ECAT_TXPDO_ASSIGNMENT 0x1c13

#define ECAT_TXPDO_MAPPING 0x1a00
#define ECAT_RXPDO_MAPPING 0x1600

#define PEAK_CURRENT 1400 //Peak current of motor is higher than that of the controller (14A)
#define CONT_CURRENT 389 //Continuous current of motor (3.89 A)

#define TELESCOPE_ANGLE 0.7 //Telescope angle below horizontal

#define DEFAULT_CURRENT_P 4126
#define DEFAULT_CURRENT_I 80
#define DEFAULT_CURRENT_OFF (0)

#define MOTOR_ENCODER_COUNTS (1 << 19) 
#define MOTOR_COUNTS_PER_REV (1 << 19)

#define MOTOR_ENCODER_SCALING 360 / MOTOR_ENCODER_COUNTS

#define HEARTBEAT_MS 0
#define LIFETIME_FACTOR_EC 10

#define ECAT_DC_CYCLE_NS 1000000

typedef enum {
    ECAT_MOTOR_COLD,           //!< ECAT_MOTOR_COLD
    ECAT_MOTOR_INIT,           //!< ECAT_MOTOR_INIT
    ECAT_MOTOR_FOUND_PARTIAL,  //!< ECAT_MOTOR_FOUND_PARTIAL
    ECAT_MOTOR_FOUND,          //!< ECAT_MOTOR_FOUND
    ECAT_MOTOR_RUNNING_PARTIAL,//!< ECAT_MOTOR_RUNNING_PARTIAL
    ECAT_MOTOR_RUNNING         //!< ECAT_MOTOR_RUNNING
} ec_control_status_t;


typedef struct {
    uint8_t comms_ok;
    uint8_t has_dc;
    uint8_t slave_error;
    uint16_t network_error_count;
    ec_control_status_t status;
} ec_device_state_t;

typedef struct {
	uint16_t network_error_count;
	ec_control_status_t status;
} ec_state_t;

typedef union {
    uint32_t val;
    struct {
        uint8_t size;
        uint8_t subindex;
        uint16_t index;
    };
} pdo_mapping_t;

static inline void map_pdo(pdo_mapping_t *m_map, uint16_t m_index, uint8_t m_subindex, uint8_t m_bits)
{
    m_map->index = m_index;
    m_map->subindex = m_subindex;
    m_map->size = m_bits;
}

static inline uint16_t object_index(uint16_t m_index, uint8_t m_subindex)
{
    return m_index;
}

static inline uint8_t object_subindex(uint16_t m_index, uint8_t m_subindex)
{
    return m_subindex;
}

#define PDO_NAME_LEN 32

typedef struct {
    char        name[PDO_NAME_LEN];
    uint16_t    index;
    uint8_t     subindex;
    int         offset;
} pdo_channel_map_t;

typedef struct {
	double velocity;
	int16_t temp;
	double current;
	double position;
	int32_t status;
	uint32_t fault_reg;
	uint16_t drive_info;
	uint16_t control_word_read;
	uint16_t control_word_write;
	uint16_t phase_angle;
	uint8_t network_problem;
}motor_data_t;

#define ECAT_PEAK_CURRENT 0x2110, 0
#define ECAT_CONT_CURRENT 0x2111, 0

#define ECAT_COUNTS_PER_REV 0x2383, 23 /* Encoder counts per revolution INT32 */

#define ECAT_ENCODER_WRAP 0x2220, 0 /* Encoder wrap position INT32 */

#define ECAT_CURRENT_LOOP_CP 0x2380, 1 /* Proportional Gain UINT16 */
#define ECAT_CURRENT_LOOP_CI 0x2380, 2 /* Integral Gain UINT16 */
#define ECAT_CURRENT_LOOP_OFFSET 0x2380, 3 /* Current Offset INT16 */

#define ECAT_CTL_WORD 0x6040, 0
# define ECAT_CTL_ON                        (1<<0)
# define ECAT_CTL_ENABLE_VOLTAGE             (1<<1)
# define ECAT_CTL_QUICK_STOP                (1<<2)
# define ECAT_CTL_ENABLE                    (1<<3)
# define ECAT_CTL_RESET_FAULT               (1<<7)
# define ECAT_CTL_HALT                      (1<<8)

#define ECAT_NET_STATUS 0x21B4, 0 //Network status
# define ECAT_NET_NODE_STATUS               (1<<0)
# define ECAT_NET_SYNC_MISSING              (1<<4)
# define ECAT_NET_GUARD_ERROR               (1<<5)
# define ECAT_NET_BUS_OFF                   (1<<8)
# define ECAT_NET_TRANSMIT_ERROR            (1<<9) 
# define ECAT_NET_RECEIVE_ERROR             (1<<10)
# define ECAT_NET_TRANSMIT_WARNING          (1<<11)
# define ECAT_NET_RECEIVE_WARNING           (1<<12)

#define ECAT_DRIVE_STATUS 0x1002, 0 //Drive status
#  define ECAT_STATUS_SHORTCIRCUIT          (1<<0)
#  define ECAT_STATUS_DRIVE_OVERTEMP        (1<<1)
#  define ECAT_STATUS_OVERVOLTAGE           (1<<2)
#  define ECAT_STATUS_UNDERVOLTAGE          (1<<3)
#  define ECAT_STATUS_TEMP_SENS_ACTIVE      (1<<4)
#  define ECAT_STATUS_ENCODER_FEEDBACK_ERR  (1<<5)
#  define ECAT_STATUS_PHASING_ERROR         (1<<6)
#  define ECAT_STATUS_CURRENT_LIMITED       (1<<7)
#  define ECAT_STATUS_VOLTAGE_LIMITED       (1<<8)
#  define ECAT_STATUS_POS_LIMIT_SW          (1<<9)
#  define ECAT_STATUS_NEG_LIMIT_SW          (1<<10)
#  define ECAT_STATUS_ENABLE_NOT_ACTIVE     (1<<11)
#  define ECAT_STATUS_SW_DISABLE            (1<<12)
#  define ECAT_STATUS_STOPPING              (1<<13)
#  define ECAT_STATUS_BRAKE_ON              (1<<14)
#  define ECAT_STATUS_PWM_DISABLED          (1<<15)
#  define ECAT_STATUS_POS_SW_LIMIT          (1<<16)
#  define ECAT_STATUS_NEG_SW_LIMIT          (1<<17)
#  define ECAT_STATUS_TRACKING_ERROR        (1<<18)
#  define ECAT_STATUS_TRACKING_WARNING      (1<<19)
#  define ECAT_STATUS_DRIVE_RESET           (1<<20)
#  define ECAT_STATUS_POS_WRAPPED           (1<<21)
#  define ECAT_STATUS_DRIVE_FAULT           (1<<22)
#  define ECAT_STATUS_VEL_LIMIT             (1<<23)
#  define ECAT_STATUS_ACCEL_LIMIT           (1<<24)
#  define ECAT_STATUS_TRACK_WINDOW          (1<<25)
#  define ECAT_STATUS_HOME_SWITCH_ACTIVE    (1<<26)
#  define ECAT_STATUS_IN_MOTION             (1<<27)
#  define ECAT_STATUS_VEL_WINDOW            (1<<28)
#  define ECAT_STATUS_PHASE_UNINIT          (1<<29)
#  define ECAT_STATUS_CMD_FAULT             (1<<30)

#define ECAT_LATCHED_DRIVE_FAULT 0x2183, 0 //Latched faults
#  define ECAT_FAULT_DATA_CRC               (1<<0)
#  define ECAT_FAULT_INT_ERR                (1<<1)
#  define ECAT_FAULT_SHORT_CIRCUIT          (1<<2)
#  define ECAT_FAULT_DRIVE_OVER_TEMP        (1<<3)
#  define ECAT_FAULT_MOTOR_OVER_TEMP        (1<<4)
#  define ECAT_FAULT_OVER_VOLT              (1<<5)
#  define ECAT_FAULT_UNDER_VOLT             (1<<6)
#  define ECAT_FAULT_FEEDBACK_FAULT         (1<<7)
#  define ECAT_FAULT_PHASING_ERR            (1<<8)
#  define ECAT_FAULT_TRACKING_ERR           (1<<9)
#  define ECAT_FAULT_CURRENT_LIMIT          (1<<10)
#  define ECAT_FAULT_FPGA_ERR1              (1<<11)
#  define ECAT_FAULT_CMD_LOST               (1<<12)
#  define ECAT_FAULT_FPGA_ERR2              (1<<13)
#  define ECAT_FAULT_SAFETY_CIRCUIT_FAULT   (1<<14)
#  define ECAT_FAULT_CANT_CONTROL_CURRENT   (1<<15)
#  define ECAT_FAULT_WIRING_DISCONNECT      (1<<16)

#define ECAT_CTL_STATUS 0x6041, 0 //Status word
#  define ECAT_CTL_STATUS_READY             (1<<0)
#  define ECAT_CTL_STATUS_ON                (1<<1)
#  define ECAT_CTL_STATUS_OP_ENABLED        (1<<2)
#  define ECAT_CTL_STATUS_FAULT             (1<<3)
#  define ECAT_CTL_STATUS_VOLTAGE_ENABLED   (1<<4)
#  define ECAT_CTL_STATUS_QUICK_STOP        (1<<5)
#  define ECAT_CTL_STATUS_DISABLED          (1<<6)
#  define ECAT_CTL_STATUS_WARNING           (1<<7)
#  define ECAT_CTL_STATUS_LAST_ABORTED      (1<<8)
#  define ECAT_CTL_STATUS_USING_CANOPEN     (1<<9)
#  define ECAT_CTL_STATUS_TARGET_REACHED    (1<<10)
#  define ECAT_CTL_STATUS_LIMIT             (1<<11)
#  define ECAT_CTL_STATUS_MOVING            (1<<14)
#  define ECAT_CTL_STATUS_HOME_CAP          (1<<15)

#define ECAT_MOTOR_POSITION 0x2240, 0 //Encoder position in counts
#define ECAT_ACTUAL_POSITION 0x6063, 0 //Encoder position for loops in counts
#define ECAT_DRIVE_TEMP 0x2202, 0 //Drive temperature in Celsius
#define ECAT_COMMUTATION_ANGLE 0x60EA, 0 //Commutation Angle in 2^16 counts per 360 degrees
#define ECAT_PHASING_MODE 0x21C0, 0

#define ECAT_VEL_ACTUAL 0x6069, 0 //Actual Velocity in 0.1 counts/s

#define ECAT_DRIVE_STATE 0x2300, 0 /* Desired state of the drive UINT16 */
#  define ECAT_DRIVE_STATE_DISABLED 0
#  define ECAT_DRIVE_STATE_PROG_CURRENT 1
#  define ECAT_DRIVE_STATE_PROG_VELOCITY 11

#define ECAT_CURRENT_LOOP_CMD 0x2340, 0 //Commanded current in 10 mA
#define ECAT_CURRENT_ACTUAL 0x221C, 0 //Actual motor current in 10 mA

#define ECAT_HEART_BEAT_TIME 0x1017, 0
#define ECAT_LIFETIME_FACTOR 0x100D, 0

#define ECAT_DC_SYNCH_MNG_2_TYPE 0x1C32, 1

int configure_ec_motor(void);

void enable(void);

int16_t get_current(void);
uint16_t get_status_word(void);
uint32_t get_latched(void);
uint16_t get_status_register(void);
int32_t get_position(void);
int16_t get_amp_temp(void);
int32_t get_velocity(void);
uint16_t get_ctl_word(void);
uint16_t get_ctl_word_write(void);
int16_t get_phase_angle(void);
uint8_t is_el_motor_ready(void);
void read_motor_data(void);
void print_motor_data(void);
int reset_ec_motor(void);
void set_current(int16_t m_cur);
void *do_motors(void*);
int close_ec_motor(void);

extern int motor_index;

extern motor_data_t MotorData[3];
extern int stop;
extern int ready;
extern FILE* motor_log;
extern int comms_ok;
extern double motor_offset;
extern double parking_pos;
#endif
