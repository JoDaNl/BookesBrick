
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

#include "config.h"

// ====================================
// CONTROLLER 
// ====================================

typedef enum 
{
    e_msg_timer_hydro,
    e_msg_timer_iotapi,
    e_msg_timer_proapi,
    e_msg_timer_ntp, 
    e_msg_timer_time
} controllerQTimerMesgType_t;

typedef struct 
{
  controllerQTimerMesgType_t mesgId;
  uint8_t data;
} controllerQTimerMesg_t;


// ====================================
// BUTTON 
// ====================================

// typedef enum 
// {
//     e_msg_button_unknown,
//     e_msg_button_short,
//     e_msg_button_long
// } controllerQButtonMesgType_t;

// typedef struct 
// {
//   controllerQButtonMesgType_t mesgId;
//   uint8_t data;
// } controllerQButtonMesg_t;


// ====================================
// SENSOR
// ====================================

typedef enum 
{
    e_msg_sensor_unknown,
    e_msg_sensor_numSensors,
    e_msg_sensor_temperature
} controllerQSensorMesgType_t;

typedef struct 
{
  controllerQSensorMesgType_t mesgId;
  int16_t data;                // temperature multiplied by 10 | number of sensors
} controllerQSensorMesg_t;


// ====================================
// BACKEND : actuators / relays
// ====================================

typedef enum 
{
    e_msg_backend_unknown,
    e_msg_backend_actuators,
    e_msg_backend_act_delay,   // MUST CHANGE!!!!!!
    e_msg_backend_heartbeat,
    e_msg_backend_temp_setpoint,
    e_msg_backend_device_name,
    e_msg_backend_time_update
} controllerQBackendMesgType_t;

typedef struct 
{
  controllerQBackendMesgType_t mesgId;
  bool valid;
  uint8_t number;
  int16_t data;
  uint32_t nextRequestInterval;
  String * stringPtr;
} controllerQBackendMesg_t;

// ====================================
// WiFi
// ====================================

typedef enum 
{
    e_msg_wifi_unknown,
    e_msg_wifi_unconnected,
    e_msg_wifi_accespoint_connected,
    e_msg_wifi_internet_connected,
    e_msg_wifi_portal_opened
} controllerQWiFiMesgType_t;

typedef struct 
{
  controllerQWiFiMesgType_t wifiStatus;
  int16_t rssi;
  bool valid;
} controllerQWiFiMesg_t;

// ====================================
// Time
// ====================================

// typedef enum 
// {
//     e_msg_time_unknown,
//     e_msg_time_data,
// } controllerQTimeMesgType_t;

// typedef struct 
// {
//   controllerQTimeMesgType_t mesgId;
//   int16_t data;
// } controllerQTimeMesg_t;


// ====================================
// HydroBrick
// ====================================

typedef enum 
{
    e_cmsg_hydro_unknown,
    e_cmsg_hydro_reading,
    e_cmsg_hydri_scanned_bricks
} controllerQHydroMesgType_t;

typedef struct
{
  uint8_t number;
  uint8_t addresses[CFG_MAX_NR_HYDROBRICKS][6];
} hydrometerQScannedBricks_t;


typedef struct 
{
  uint8_t  status;
  uint16_t angle_x100;
  uint16_t temperature_x10;
  uint16_t batteryVoltage_x1000;
  uint16_t SG_x1000;
  int16_t  RSSI;
} hydrometerQData_t;


typedef union
{
  hydrometerQData_t reading;
  hydrometerQScannedBricks_t scannedBricks;
} controllerHydroQData_t;


typedef struct 
{
  controllerQHydroMesgType_t mesgId;
  controllerHydroQData_t data;
} controllerQHydroMesg_t;

// ====================================
// Controller-Q message
// ====================================

typedef enum 
{
    e_mtype_controller,
    e_mtype_button,
    e_mtype_sensor,
    e_mtype_backend,
    e_mtype_wifi,
    e_mtype_hydro
} controllerQMesgType_t;

typedef union 
{
  controllerQTimerMesg_t     controlMesg;
  // controllerQButtonMesg_t   buttonMesg;
  controllerQSensorMesg_t   sensorMesg;
  controllerQBackendMesg_t  backendMesg;
  controllerQWiFiMesg_t     WiFiMesg;
  // controllerQTimeMesg_t     timeMesg;
  controllerQHydroMesg_t    hydroMesg;
} controllerQMesgData_t;


typedef struct 
{
  controllerQMesgType_t type;
  controllerQMesgData_t mesg;
  bool valid;
} controllerQItem_t;

// ====================================
// EXTERNALS
// ====================================

extern bool globalWifiConfigMode;
extern int controllerQueueSend(controllerQItem_t *, TickType_t);
extern void initController(void);

#endif

