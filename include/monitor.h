
#ifndef __MONITOR_H__
#define __MONITOR_H__

#include <Arduino.h>

extern int monitorQueueSend(uint8_t *, TickType_t);
extern void initMonitor(void);

#endif
