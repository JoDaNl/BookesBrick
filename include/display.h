
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
    e_heartbeat,
    e_error
} displayQueueDataType_t;


typedef union displayQueueData
{
  int16_t temperature;
  int16_t setPoint;
  uint8_t actuators;
  int16_t rssi;
//  uint8_t error;
  uint16_t compDelay;
  bool heartOn;
} displayQueueData_t;

typedef struct displayQueueItem
{
  displayQueueDataType_t type;
  bool valid;
  uint8_t number;
  // uint16_t duration;
  displayQueueData_t data;
} displayQueueItem_t;

extern xQueueHandle displayQueue;
extern void displayTask(void *arg);
extern int displayQueueSend(const void * const pvItemToQueue, TickType_t xTicksToWait);
extern void initDisplay(void);

#endif
