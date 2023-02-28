
#ifndef __I2C_LCD_16x2_H__
#define __I2C_LCD_16x2_H__

#include <Arduino.h>

typedef enum displayQueueDataType
{
    e_temperature,
    e_setpoint,
    e_actuator,
    e_wifiInfo,
    e_heartbeat,
    e_error
} displayQueueDataType_t;


typedef union displayQueueData
{
  uint16_t temperature;
  uint8_t actuator;
  uint8_t heartbeat;
  uint8_t error;
  char wifiInfo[32];
} displayQueueData_t;

typedef struct displayQueueItem
{
  displayQueueDataType_t type;
  uint8_t index;
  uint16_t duration;
  displayQueueData_t data;
} displayQueueItem_t;

extern xQueueHandle displayQueue;


extern void initDisplay(void);


#endif
