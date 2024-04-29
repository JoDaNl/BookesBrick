
#ifndef __COMMS_H__
#define __COMMS_H__

extern int communicationQueueSend(int16_t * queueItem, TickType_t xTicksToWait);
extern void initCommmunication(void);

#endif
