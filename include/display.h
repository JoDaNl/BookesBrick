
#ifndef __DISPLAY__
#define __DISPLAY__

#include <Arduino.h>

typedef enum displayQueueDataType
{
    e_temperature,
    e_setpoint,
    e_actuator,
    e_delay,
    e_rssi,
    e_time,
    e_heartbeat,
    e_error,
    e_device_name
} displayQueueDataType_t;


typedef union displayQueueData
{
  int16_t temperature;
  int16_t setPoint;
  uint8_t actuators;
  int16_t rssi;
  uint16_t compDelay;
  uint16_t time;
  uint16_t heartBeat;
  String * stringPtr;
} displayQueueData_t;

typedef struct displayQueueItem
{
  displayQueueDataType_t type;
  bool valid;
  uint8_t number;
  displayQueueData_t data;
} displayQueueItem_t;

extern xQueueHandle displayQueue;
extern void displayTask(void *);
extern int displayQueueSend(displayQueueItem_t *, TickType_t);
extern void initDisplay(void);

#endif
