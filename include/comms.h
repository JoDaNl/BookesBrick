
#ifndef __COMMS_H__
#define __COMMS_H__

typedef struct commsQueueItem
{
  bool valid;
  uint16_t temperature;
} commsQueueItem_t;


extern int communicationQueueSend(commsQueueItem_t * queueItem, TickType_t xTicksToWait);
extern void initCommmunication(void);

#endif
