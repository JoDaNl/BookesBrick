
#ifndef __DISPLAY__
#define __DISPLAY__

#include <Arduino.h>

typedef enum displayQueueDataType
{
    e_temperature,
    e_setpoint,
    e_actuator,
    e_delay,
    e_wifi,
    e_time,
    e_heartbeat,
    e_error,
    e_display_text,
    e_specific_gravity,
    e_hb_temperature,
    e_progress_tick,
    e_voltage
} displayQueueDataType_t;

typedef enum displayMessageType
{
  e_device_name,
  e_status_bar
} displayMessageType_t;

typedef struct displayTextData
{
  String * stringPtr;
  uint8_t timeInSec;
  displayMessageType_t messageType;
} displayTextData_t;

typedef struct displayWiFiData
{
  int16_t rssi; // signed
  char status;  
} displayWiFiData_t;

typedef union displayQueueData
{
  int16_t temperature;
  int16_t setPoint;
  uint8_t actuators;
  uint16_t compDelay;
  uint16_t time;
  uint16_t heartBeat;
  uint16_t specificGravity;
  uint16_t voltage;
  displayTextData_t textData;
  displayWiFiData_t wifiData;
} displayQueueData_t;

typedef struct displayQueueItem
{
  displayQueueDataType_t type;
  displayQueueData_t data;
  bool valid;
  uint8_t number;
} displayQueueItem_t;


extern int displayQueueSend(displayQueueItem_t *, TickType_t);
extern void initDisplay(void);
extern void initLVGL(void);
extern void updateLVGL(void);
extern void displayText(String *, displayMessageType_t, uint8_t);

#endif
