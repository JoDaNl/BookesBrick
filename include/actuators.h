
#ifndef __ACTUATORS_H__
#define __ACTUATORS_H__

#include <Arduino.h>



typedef struct actuatorQueueItem
{
  uint8_t number;
  uint8_t onOff;
} actuatorQueueItem_t;


extern xQueueHandle actuatorsQueue;

extern void initActuators(void);

#endif
