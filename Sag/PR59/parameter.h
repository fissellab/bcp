// -----------------------------
// PARAMETER.H
// -----------------------------
//
// Laird Parameter file
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
        unsigned STARTUP_DELAY:1;          // mark startup delay
        unsigned DOWNLOAD_ERROR:1;         // we got a timeout while downloading parameters
        unsigned C_ERROR:1;                // critical error, outside voltage, outside current
        unsigned REG_OVERLOAD_ERROR:1;     // regulator overload error
        unsigned HIGH_VOLT:1;              // we have high voltage error
        unsigned LOW_VOLT:1;               // we have low voltage error
        unsigned HIGH_12V:1;               // we have high 12 voltage in the system
        unsigned LOW_12V:1;                // we have low 12 voltage in the system
        unsigned CURRENT_HIGH:1;           // over or under current check
        unsigned CURRENT_LOW:1;            // over or under current check
        unsigned FAN1_HIGH:1;
        unsigned FAN1_LOW:1;
        unsigned FAN2_HIGH:1;
        unsigned FAN2_LOW:1;
        unsigned TEMP_SENSOR_ALARM_STOP:1; // sensor alarm to stop regulator
        unsigned TEMP_SENSOR_ALARM_IND:1;  // sensor alarm indication
    } BIT;
} ERROR_BITS;                             // g_error, g_error_old

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
    } BIT;
} TEMP_ALARM_BITS;                        // g_error_tempsensor

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
    } BIT;
} E_ALARM_L;                              // used in G_ui_alarm_enable_L

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
    } BIT;
} E_ALARM_H;                              // used in G_ui_alarm_enable_H

// ---------------------------------------------------------
// G_ui_R_Mode
// ---------------------------------------------------------
#define R_MODE_OFF              0         // no regulator
#define R_MODE_POWER           1         // POWER mode
#define R_MODE_ALGO            2         // ON/OFF mode
#define R_MODE_P               3         // P regulator
#define R_MODE_PI              4         // PI regulator
#define R_MODE_PD              5         // PD regulator
#define R_MODE_PID             6         // PID regulator
#define R_MODE_MASK            0x000f    // bit 0..3
#define R_MODE_TrExtSelect     0x0010    // bit 4
#define R_MODE_TcPowerInt      0x0020    // bit 5
#define R_MODE_DownloadParameters 0x0040  // bit 6
#define R_MODE_AutoStart       0x0080    // bit 7
#define R_MODE_LoopMode        0x0100    // bit 8
#define R_MODE_InvertOutput    0x0200    // bit 9

#define R_MODE_FilterAMask     0x3000
#define R_MODE_FilterA_OFF     0         // off
#define R_MODE_FilterA_MUL     0x1000    // multiplication type
#define R_MODE_FilterA_LIN     0x2000    // linear type, not implemented yet
#define R_MODE_FilterA_LEAD_LAG 0x3000   // lead / lag type, not implemented yet

#define R_MODE_FilterBMask     0xc000
#define R_MODE_FilterB_OFF     0         // off
#define R_MODE_FilterB_MUL     0x4000    // multiplication type
#define R_MODE_FilterB_LIN     0x8000    // linear type, not implemented yet
#define R_MODE_FilterB_LEAD_LAG 0xc000   // lead / lag type, not implemented yet

// ---------------------------------------------------------
// Register definitions
// ---------------------------------------------------------
enum {
    G_TrInt = 0,                         // Set Point temperature reference
    G_Kp,                                // PID P constant
    G_Ki,                                // PID I constant
    G_Kd,                                // PID D constant
    G_KLP_A,                             // Low pass filter A
    G_KLP_B,                             // Low pass filter B
    G_RegLim,                            // Limit Tc signal
    G_Deadband,                          // Dead band of Tc signal
    G_ILim,                              // Limit I value
    G_Ts,                                // Sample rate
    G_CoolGain,                          // Cool gain
    G_HeatGain,                          // Heat gain
    G_Decay,                             // Decay of I and low pass filter
    G_ui_R_Mode,                         // Regulator mode control
    G_ON_TDb,                            // ON/OFF Dead band
    G_ON_THyst,                          // ON/OFF Hysteresis
    G_ui_F1_Mode,                        // FAN 1 mode select
    G_F1_Tr,                             // FAN 1 temperature
    G_F1_Db,                             // FAN 1 Dead band
    G_F1_Hyst,                           // FAN 1 Hysteresis
    G_F1_Fh,                             // FAN 1 High speed
    G_F1_LSV,                            // FAN 1 Low Speed voltage
    G_F1_HSV,                            // FAN 1 High Speed voltage
    G_ui_F2_Mode,                        // FAN 2 mode select
    G_F2_Tr,                             // FAN 2 temperature
    G_F2_Db,                             // FAN 2 Dead band
    G_F2_Hyst,                           // FAN 2 Hysteresis
    G_F2_Fh,                             // FAN 2 High speed
    G_F2_LSV,                            // FAN 2 Low Speed voltage
    G_F2_HSV,                            // FAN 2 High Speed voltage
    G_ScaleAin_AD_offset,                // POT input - AD offset
    G_ScaleAin_offset,                   // POT input - offset
    G_ScaleAin_gain,                     // POT input - gain
    G_ScaleAout_offset,                  // Expansion port AD out - offset
    G_ScaleAout_gain,                    // Expansion port AD out - gain
    G_Temp1_gain,                        // Temp 1 gain
    G_Temp1_offset,                      // Temp 1 offset
    G_Temp2_gain,                        // Temp 2 gain
    G_Temp2_offset,                      // Temp 2 offset
    G_Temp3_gain,                        // Temp 3 gain
    G_Temp3_offset,                      // Temp 3 offset
    G_Temp4_gain,                        // Temp 4 gain
    G_Temp4_offset,                      // Temp 4 offset
    G_ui_pot_offset,                     // Temp 1 digital pot offset
    G_ui_pot_gain,                       // Temp 1 digital pot gain
    G_v_high,                            // ALARM level voltage high
    G_v_low,                             // ALARM level voltage low
    G_curr_high,                         // ALARM level main current high
    G_curr_low,                          // ALARM level main current low
    G_fan1_high,                         // ALARM level FAN 1 current high
    G_fan1_low,                          // ALARM level FAN 1 current low
    G_fan2_high,                         // ALARM level FAN 2 current high
    G_fan2_low,                          // ALARM level FAN 2 current low
    G_v12_high,                          // ALARM level internal 12v high
    G_v12_low,                           // ALARM level internal 12v low
    G_ui_temp1_mode,                     // Temperature 1 sensor mode
    G_ui_temp2_mode,                     // Temperature 2 sensor mode
    G_ui_temp3_mode,                     // Temperature 3 sensor mode
    G_ui_temp4_mode,                     // Temperature 4 sensor mode
    G_t1_coff_A,                         // Temp1 Steinhart coeff A
    G_t1_coff_B,                         // Temp1 Steinhart coeff B
    G_t1_coff_C,                         // Temp1 Steinhart coeff C
    G_t2_coff_A,                         // Temp2 Steinhart coeff A
    G_t2_coff_B,                         // Temp2 Steinhart coeff B
    G_t2_coff_C,                         // Temp2 Steinhart coeff C
    G_t3_coff_A,                         // Temp3 Steinhart coeff A
    G_t3_coff_B,                         // Temp3 Steinhart coeff B
    G_t3_coff_C,                         // Temp3 Steinhart coeff C
    G_t4_coff_A,                         // Temp4 Steinhart coeff A
    G_t4_coff_B,                         // Temp4 Steinhart coeff B
    G_t4_coff_C,                         // Temp4 Steinhart coeff C
    G_alarm_temp1_high,                  // ALARM Temp 1 high
    G_alarm_temp1_low,                   // ALARM Temp 1 low
    G_alarm_temp2_high,                  // ALARM Temp 2 high
    G_alarm_temp2_low,                   // ALARM Temp 2 low
    G_alarm_temp3_high,                  // ALARM Temp 3 high
    G_alarm_temp3_low,                   // ALARM Temp 3 low
    G_alarm_temp4_high,                  // ALARM Temp 4 high
    G_alarm_temp4_low,                   // ALARM Temp 4 low
    G_t1_res_H,                          // Temp1 Steinhart resistance value H
    G_t1_res_M,                          // Temp1 Steinhart resistance value M
    G_t1_res_L,                          // Temp1 Steinhart resistance value L
    G_t2_res_H,                          // Temp2 Steinhart resistance value H
    G_t2_res_M,                          // Temp2 Steinhart resistance value M
    G_t2_res_L,                          // Temp2 Steinhart resistance value L
    G_t3_res_H,                          // Temp3 Steinhart resistance value H
    G_t3_res_M,                          // Temp3 Steinhart resistance value M
    G_t3_res_L,                          // Temp3 Steinhart resistance value L
    G_t4_res_H,                          // Temp4 Steinhart resistance value H
    G_t4_res_M,                          // Temp4 Steinhart resistance value M
    G_t4_res_L,                          // Temp4 Steinhart resistance value L
    G_ui_alarm_enable_L,                 // Alarm enable bits low
    G_ui_alarm_enable_H,                 // Alarm enable bits high
    G_TrInt2,                            // Setpoint 2 for test loop mode
    G_uiTimeHigh,                        // TimeHigh for loop mode
    G_uiTimeLow,                         // TimeLow for loop mode
    G_ui_sensor_alarm_mask,              // Sensor Alarm Mask
    G_fMainCurrGain,                     // Main current gain calibration
    G_SIZE                               // Must be last in list
};

// Runtime registers (read-only)
#define GR_event_count         99
#define GR_temp1              100
#define GR_temp2              101
#define GR_temp3              102
#define GR_temp4              103
#define GR_pot_input          104
#define GR_tref               105
#define GR_tc_main            106
#define GR_fan1_main          107
#define GR_fan2_main          108
#define GR_pid_Ta             110
#define GR_pid_Te             111
#define GR_pid_Tp             112
#define GR_pid_Ti             113
#define GR_pid_Td             114
#define GR_pid_TLPa           117
#define GR_pid_TLPb           118
#define GR_onoff_runtime_state 122
#define GR_onoff_runtime_max   123
#define GR_onoff_runtime_min   124
#define GR_fan1_runtime_state  125
#define GR_fan1_runtime_max    126
#define GR_fan1_runtime_min    127
#define GR_fan2_runtime_state  128
#define GR_fan2_runtime_max    129
#define GR_fan2_runtime_min    130
#define GR_inputvolt          150
#define GR_internalvolt       151
#define GR_main_current       152
#define GR_fan1_current       153
#define GR_fan2_current       154
#define GRW_fan_gain          155

#endif // _PARAMETER_H