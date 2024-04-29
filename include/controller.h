
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

// ====================================
// BUTTON 
// ====================================

typedef enum controllerQButtonMesgType
{
    e_msg_button_unknown,
    e_msg_button_short,
    e_msg_button_long
} controllerQButtonMesgType_t;

typedef struct controllerQButtonMesg
{
  controllerQButtonMesgType_t mesgId;
  uint8_t data;
} controllerQButtonMesg_t;


// ====================================
// SENSOR
// ====================================

typedef enum controllerQSensorMesgType
{
    e_msg_sensor_unknown,
    e_msg_sensor_numSensors,
    e_msg_sensor_temperature
} controllerQSensorMesgType_t;

typedef struct controllerQSensorMesg
{
  controllerQSensorMesgType_t mesgId;
  int16_t data;                // temperature multiplied by 10 | number of sensors
} controllerQSensorMesg_t;


// ====================================
// BACKEND : actuators / Relays / diplay
// ====================================

typedef enum controllerQBackendMesgType
{
    e_msg_backend_unknown,
    e_msg_backend_actuators,
    e_msg_backend_act_delay,   // MUST CHANGE!!!!!!
    e_msg_backend_heartbeat,
    e_msg_backend_temp_setpoint,
    e_msg_backend_device_name
} controllerQBackendMesgType_t;

typedef struct controllerQBackendMesg
{
  controllerQBackendMesgType_t mesgId;
  bool valid;
  uint8_t number;
  int16_t data;
  String * stringPtr;
} controllerQBackendMesg_t;

// ====================================
// WiFI
// ====================================

typedef enum controllerQWiFiMesgType
{
    e_msg_WiFi_unknown,
    e_msg_WiFi_rssi,
} controllerQWiFiMesgType_t;

typedef struct controllerQWiFiMesg
{
  controllerQWiFiMesgType_t mesgId;
  int16_t data;
} controllerQWiFiMesg_t;

// ====================================
// Time
// ====================================

typedef enum controllerQTimeMesgType
{
    e_msg_time_unknown,
    e_msg_time_data,
} controllerQTimeMesgType_t;

typedef struct controllerQTimeMesg
{
  controllerQTimeMesgType_t mesgId;
  int16_t data;
} controllerQTimeMesg_t;


// ====================================
// Controller-Q message
// ====================================

typedef enum controllerQMesgType
{
    e_mtype_button,
    e_mtype_sensor,
    e_mtype_backend,
    e_mtype_wifi,
    e_mtype_time
} controllerQMesgType_t;

typedef union controllerQMesgData
{
  controllerQButtonMesg_t   buttonMesg;
  controllerQSensorMesg_t   sensorMesg;
  controllerQBackendMesg_t  backendMesg;
  controllerQWiFiMesg_t     WiFiMesg;
  controllerQTimeMesg_t     timeMesg;
} controllerQMesgData_t;


typedef struct controllerQItem
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

