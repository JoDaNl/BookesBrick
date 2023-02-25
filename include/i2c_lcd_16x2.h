
#ifndef __I2C_LCD_16x2_H__
#define __I2C_LCD_16x2_H__

#include <Arduino.h>

typedef enum displayQueueDataType
{
    e_temperature,
    e_actuator,
    e_wifiInfo,
    e_heartbeat,
    e_error
} displayQueueDataType_t;


typedef union displayQueueData
{
  u_int16_t temperature;
  u_int8_t actuator;
  u_int8_t heartbeat;
  u_int8_t error;
  char wifiInfo[32];
} displayQueueData_t;

typedef struct displayQueueItem
{
  displayQueueDataType_t type;
  u_int8_t index;
  u_int16_t duration;
  displayQueueData_t data;
} displayQueueItem_t;

extern xQueueHandle displayQueue;


extern void initDisplay(void);


#endif
