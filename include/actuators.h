
#ifndef __ACTUATORS_H__
#define __ACTUATORS_H__

#include <Arduino.h>



typedef struct actuatorQueueItem
{
  u_int8_t number;
  u_int8_t onOff;
} actuatorQueueItem_t;


extern xQueueHandle actuatorsQueue;

extern void initActuators(void);

#endif
