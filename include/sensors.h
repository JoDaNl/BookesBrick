
#ifndef __SENSORS_H__
#define __SENSORS_H__

#include <Arduino.h>


extern int sensorsQueueSend(uint8_t *, TickType_t);
extern void initSensors(void);

#endif
