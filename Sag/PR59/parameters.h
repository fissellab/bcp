// -----------------------------
// PARAMETER.H
// -----------------------------
//
// Laird Parameter file
//
// 2005-03-02, ver 1.6d
// * Add FAN gain variable
//
// 2005-02-28, ver 1.6c
// * Change _stain_ to _coff_ values
// * Support of the PT sensor mode
//
// 2004-12-20, ver 1.6b
// * add filter type selections
//
// 2004-11-08, ver 1.6d
// * adding alarm mask variabel
//
// 2006-11-29, ver 1.6e
// * add current gain calibration value
//
#ifndef _PARAMETER_H
#define _PARAMETER_H
#define VER_INTERFACE "SSCI_v1.6d"
#ifdef _WIN32
typedef unsigned int uns16;
#endif
// ---------------------------------------------------------
typedef union {
uns16 ALL;
struct {
unsigned STARTUP_DELAY:1; // mark startup delay
unsigned DOWNLOAD_ERROR:1; // we got a timeout while downloading parameters
unsigned C_ERROR:1; // critical error, outside voltage,
outside current
unsigned REG_OVERLOAD_ERROR:1; // regulator overload error
unsigned HIGH_VOLT:1; // we have high voltage error
unsigned LOW_VOLT:1; // we have low voltage error
unsigned HIGH_12V:1; // we have high 12 voltage in the system
unsigned LOW_12V:1; // we have low 12 voltage in the system
unsigned CURRENT_HIGH:1; // over or under current check
unsigned CURRENT_LOW:1; // over or under current check
unsigned FAN1_HIGH:1;
unsigned FAN1_LOW:1;
unsigned FAN2_HIGH:1;
unsigned FAN2_LOW:1;
unsigned TEMP_SENSOR_ALARM_STOP:1; // sensor alarm to stop regulator
unsigned TEMP_SENSOR_ALARM_IND:1; // sensor alarm indication
// ### OBS! max 16 bits
}BIT;
} ERROR_BITS; // g_error, g_error_old
// This bits will indicate an alarm,
// and when used with mask, will cause error
typedef union {
uns16 ALL;
struct {
unsigned TEMP1_HIGH:1;
unsigned TEMP1_LOW:1;
unsigned TEMP1_SHORT:1;
unsigned TEMP1_MISSING:1;
unsigned TEMP2_HIGH:1;
unsigned TEMP2_LOW:1;
unsigned TEMP2_SHORT:1;

unsigned TEMP2_MISSING:1;
unsigned TEMP3_HIGH:1;
unsigned TEMP3_LOW:1;
unsigned TEMP3_SHORT:1;
unsigned TEMP3_MISSING:1;
unsigned TEMP4_HIGH:1;
unsigned TEMP4_LOW:1;
unsigned TEMP4_SHORT:1;
unsigned TEMP4_MISSING:1;
// ### OBS! max 16 bits
}BIT;
} TEMP_ALARM_BITS; // g_error_tempsensor
// ---------------------------------------------------------
// Alarm enable bits
// ---------------------------------------------------------
typedef union {
uns16 ALL;
struct {
unsigned OVER_VIN:1;
unsigned UNDER_VIN:1;
unsigned OVER_12V:1;
unsigned UNDER_12V:1;
unsigned OVER_CURR:1;
unsigned UNDER_CURR:1;
unsigned OVER_FAN1:1;
unsigned UNDER_FAN1:1;
unsigned OVER_FAN2:1;
unsigned UNDER_FAN2:1;
// ### obs! max 16 bits
} BIT;
} E_ALARM_L; // used in G_ui_alarm_enable_L
typedef union {
uns16 ALL;
struct {
unsigned HIGH_T1:1;
unsigned LOW_T1:1;
unsigned HIGH_T2:1;
unsigned LOW_T2:1;
unsigned HIGH_T3:1;
unsigned LOW_T3:1;
unsigned HIGH_T4:1;
unsigned LOW_T4:1;
// ### obs! max 16 bits
} BIT;
} E_ALARM_H; // used in G_ui_alarm_enable_H

// ---------------------------------------------------------
// G_ui_R_Mode
// ---------------------------------------------------------
#define R_MODE_OFF 0 // no regulator
#define R_MODE_POWER 1 // POWER mode
#define R_MODE_ALGO 2 // ON/OFF mode
#define R_MODE_P 3 // P regulator
#define R_MODE_PI 4 // PI regulator
#define R_MODE_PD 5 // PD regulator
#define R_MODE_PID 6 // PID regulator
#define R_MODE_MASK 0x000f // bit 0..3
#define R_MODE_TrExtSelect 0x0010 // bit 4
#define R_MODE_TcPowerInt 0x0020 // bit 5
#define R_MODE_DownloadParameters 0x0040 // bit 6
#define R_MODE_AutoStart 0x0080 // bit 7
#define R_MODE_LoopMode 0x0100 // bit 8
#define R_MODE_InvertOutput 0x0200 // bit 9
//...
#define R_MODE_FilterAMask 0x3000
#define R_MODE_FilterA_OFF 0 // off
#define R_MODE_FilterA_MUL 0x1000 // multiplication type
#define R_MODE_FilterA_LIN 0x2000 // linjear type, not implemented yet
#define R_MODE_FilterA_LEAD_LAG 0x3000 // lead / lag type, not implemented yet

#define R_MODE_FilterBMask 0xc000
#define R_MODE_FilterB_OFF 0 // off
#define R_MODE_FilterB_MUL 0x4000 // multiplication type
#define R_MODE_FilterB_LIN 0x8000 // linjear type, not implemented yet
#define R_MODE_FilterB_LEAD_LAG 0xc000 // lead / lag type, not implemented yet
//## max 16 bits
// ---------------------------------------------------------
// G_ui_Fx_Mode
// ---------------------------------------------------------
#define RFx_MODE_MASK 0x0f // bit 0..3
#define RFx_MODE_OFF 0 // FAN off
#define RFx_MODE_ALWAYS_ON 1
#define RFx_MODE_COOL 2
#define RFx_MODE_HEAT 3
#define RFx_MODE_COOL_HEAT 4
#define RFx_MODE_ALGO 5
//## max 8 bits
// ---------------------------------------------------------
// G_ui_temp1_mode
// ---------------------------------------------------------
//#define TEMPx_MODE_bVrefPlus 0x0001 // ## removed, not in use
//#define TEMPx_MODE_bVrefMinus 0x0002 // ## removed, not in use
#define TEMPx_MODE_bNTC 0x0004 // NTC sensor with stainhart algorithm
#define TEMPx_MODE_bZoom 0x0008 // ZOOM mode
#define TEMPx_MODE_bPT 0x0010 // Platina sensor, type PT100, PT1000 etc
//## max 16 bits
// ---------------------------------------------------------
// g_param[] index list
// ---------------------------------------------------------
#define REG_VERSION 0x04 // ## change this if you edit in the list
//
// REGISTER LIST, used to save data to FLASH
// Will be saved in FLASH if issued the command
//
// 32bits values
//
enum {
G_TrInt = 0, // (0)
G_Kp, // (1)
G_Ki, // (2)
G_Kd, // (3)
G_KLP_A, // (4)
G_KLP_B, // (5)
G_RegLim, // (6)
G_Deadband, // (7)
G_ILim, // (8)
G_Ts, // (9)
G_CoolGain, // (10)
G_HeatGain, // (11)
G_Decay, // (12)
G_ui_R_Mode, // (13) *16*
G_ON_TDb, // (14)
G_ON_THyst, // (15)
G_ui_F1_Mode, // (16) *8* FAN 1
G_F1_Tr, // (17)
G_F1_Db, // (18)
G_F1_Hyst, // (19)
G_F1_Fh, // (20)
G_F1_LSV, // (21)
G_F1_HSV, // (22)
G_ui_F2_Mode, // (23) *8* FAN 2
G_F2_Tr, // (24)
G_F2_Db, // (25)
G_F2_Hyst, // (26)
G_F2_Fh, // (27)
G_F2_LSV, // (28)
G_F2_HSV, // (29)
G_ScaleAin_AD_offset, // (30)
G_ScaleAin_offset, // (31)
G_ScaleAin_gain, // (32)
G_ScaleAout_offset, // (33)
G_ScaleAout_gain, // (34)
G_Temp1_gain, // (35)
G_Temp1_offset, // (36)
G_Temp2_gain, // (37)
G_Temp2_offset, // (38)
G_Temp3_gain, // (39)
G_Temp3_offset, // (40)
G_Temp4_gain, // (41)
G_Temp4_offset, // (42)
G_ui_pot_offset, // (43) *8*
G_ui_pot_gain, // (44) *8*
G_v_high, // (45) error level check
G_v_low, // (46)
G_curr_high, // (47)
G_curr_low, // (48)
G_fan1_high, // (49)
G_fan1_low, // (50)
G_fan2_high, // (51)
G_fan2_low, // (52)
G_v12_high, // (53)
G_v12_low, // (54)
G_ui_temp1_mode, // (55) *8*
G_ui_temp2_mode, // (56) *8*
G_ui_temp3_mode, // (57) *8*
G_ui_temp4_mode, // (58) *8*
// in NTC mode:
// coff A, B, C is the stainhart coff's
// and res_H, M, L is the resistor values for the points
//
// in PT mode
// coff A, B, and res_H (Ro) is used in the algorithm
//
G_t1_coff_A, // (59)
G_t1_coff_B, // (60)
G_t1_coff_C, // (61)
G_t2_coff_A, // (62)
G_t2_coff_B, // (63)
G_t2_coff_C, // (64)
G_t3_coff_A, // (65)
G_t3_coff_B, // (66)
G_t3_coff_C, // (67)
G_t4_coff_A, // (68)
G_t4_coff_B, // (69)
G_t4_coff_C, // (70)
G_alarm_temp1_high, // (71)
G_alarm_temp1_low, // (72)
G_alarm_temp2_high, // (73)
G_alarm_temp2_low, // (74)
G_alarm_temp3_high, // (75)
G_alarm_temp3_low, // (76)
G_alarm_temp4_high, // (77)
G_alarm_temp4_low, // (78)
G_t1_res_H, // (79) stain resistans (high/mid/low) point for coff A, B, C
G_t1_res_M, // (80)
G_t1_res_L, // (81)
G_t2_res_H, // (82)
G_t2_res_M, // (83)
G_t2_res_L, // (84)
G_t3_res_H, // (85)
G_t3_res_M, // (86)
G_t3_res_L, // (87)
G_t4_res_H, // (88)
G_t4_res_M, // (89)
G_t4_res_L, // (90)
G_ui_alarm_enable_L, // (91) *16* [E_ALARM_L] enable bit for alarm setting
G_ui_alarm_enable_H, // (92) *16* [E_ALARM_H]
G_TrInt2, // (93) Setpoint 2, when in loop mode low value
G_uiTimeHigh, // (94) *16* time, 0xffff=65535 loops / 20 = 3276 sekunder
G_uiTimeLow, // (95) *16* time, 0xffff=65535 loops / 20 = 3276 sekunder

G_ui_sensor_alarm_mask, // (96) *16* enable which alarm that should stop
regulator
G_fMainCurrGain, // (97) float32 main gain constant
G_SIZE // must be last in list, max value 255
} PARAM_NUM;
// RUNTIME DATA (READ)
// and (READ_WRITE) temporary adjustments value
#define GR_event_count 99
#define GR_temp1 100
#define GR_temp2 101
#define GR_temp3 102
#define GR_temp4 103
#define GR_pot_input 104
#define GR_tref 105
#define GR_tc_main 106
#define GR_fan1_main 107
#define GR_fan2_main 108
#define GR_pid_Ta 110
#define GR_pid_Te 111
#define GR_pid_Tp 112
#define GR_pid_Ti 113
#define GR_pid_Td 114
#define GR_pid_TLPa 117
#define GR_pid_TLPb 118
#define GR_onoff_runtime_state 122 // int8
#define GR_onoff_runtime_max 123 // float
#define GR_onoff_runtime_min 124 // float
#define GR_fan1_runtime_state 125 // int8
#define GR_fan1_runtime_max 126 // float
#define GR_fan1_runtime_min 127 // float
#define GR_fan2_runtime_state 128 // int8
#define GR_fan2_runtime_max 129 // float
#define GR_fan2_runtime_min 130 // float
#define GR_inputvolt 150 // float (Volt)
#define GR_internalvolt 151 // float (Volt)
#define GR_main_current 152 // float (Amp)
#define GR_fan1_current 153 // float (Amp)
#define GR_fan2_current 154 // float (Amp)
#define GRW_fan_gain 155 // float (gain)
#endif // _PARAMETER_H



