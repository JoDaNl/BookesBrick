
#ifndef __COMMS_H__
#define __COMMS_H__


typedef enum
{
  e_type_comms_iotapi,
  e_type_comms_proapi,
  e_type_comms_hydrobrick,
  e_type_comms_ntp
} commsyQueueDataType_t;

typedef struct commsQueueItem
{
  bool valid;
  commsyQueueDataType_t type;
  uint16_t temperature_x10;
  uint16_t SG_x1000;
  uint16_t battteryLevel_x1000;
} commsQueueItem_t;


extern int communicationQueueSend(commsQueueItem_t * queueItem, TickType_t xTicksToWait);
extern void initCommmunication(void);

#endif
