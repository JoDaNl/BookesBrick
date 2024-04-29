
#ifndef __ACTUATORS_H__
#define __ACTUATORS_H__

#include <Arduino.h>


typedef struct actuatorQueueItem
{
  uint8_t data;        // every bit corresponds with an actuator
} actuatorQueueItem_t;

extern int actuatorsQueueSend(actuatorQueueItem_t *, TickType_t);
extern void powerUpActuators(void);
extern void initActuators(void);

#endif
