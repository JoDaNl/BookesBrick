
#ifndef __SENSORS_H__
#define __SENSORS_H__

#include <Arduino.h>


extern xQueueHandle sensorsQueue;

extern void initSensors(void);

#endif
