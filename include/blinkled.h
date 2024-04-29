
#ifndef __BLINKLED_H__
#define __BLINKLED_H__

#include <Arduino.h>

typedef enum blinkLedMesgType
{
    e_msg_blinkled_unknown,
    e_msg_blinkled_off,
    e_msg_blinkled_on,
    e_msg_blinkled_blink
} blinkLedMesgType_t;

typedef struct blinkLedQMesg
{
  blinkLedMesgType_t mesgId;
  uint16_t onTimeMs;
  uint16_t offTimeMs;
} blinkLedQMesg_t;

extern int blinkLedQueueSend(blinkLedQMesg_t *, TickType_t xTicksToWait);
extern void initBlinkLed(uint8_t pin);

#endif
