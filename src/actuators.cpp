//
//  actuators.cpp
//

#include "config.h"

#include <Arduino.h>
#include "actuators.h"
#include "i2c_lcd_16x2.h"

// Queues
xQueueHandle actuatorsQueue = NULL;

// Actuators task
static TaskHandle_t actuatorsTaskHandle = NULL;

// ============================================================================
// ACTUATORS TASK
// ============================================================================

static void actuatorsTask(void *arg)
{
  static actuatorQueueItem_t qMesg;

  // TODO : use arrays...you know how to..!
  static uint8_t actuatorReqeuest0;
  static uint8_t actuatorReqeuest1;
  static uint8_t actuatorActual0;
  static uint8_t actuatorActual1;
  static uint16_t onDelaySec0;
  static uint16_t onDelaySec1;
  static uint16_t offDelaySec0;
  static uint16_t offDelaySec1;

  static displayQueueItem_t displayQMesg;

  displayQMesg.type = e_actuator;

  // TODO : make configurable
  pinMode(CFG_RELAY0_PIN, OUTPUT);
  pinMode(CFG_RELAY1_PIN, OUTPUT);

  // Set all actuator off
  digitalWrite(CFG_RELAY0_PIN, !CFG_RELAY0_ON_LEVEL);
  digitalWrite(CFG_RELAY1_PIN, !CFG_RELAY1_ON_LEVEL);

  while (true)
  {
    if (xQueueReceive(actuatorsQueue, &qMesg, 1000 / portTICK_RATE_MS) == pdTRUE)
    {
      printf("[ACTUATORS] received qMesg.number=%d - qMesg.onOff=%d\n", qMesg.number, qMesg.onOff);

      switch (qMesg.number)
      {
      case 0:
        actuatorReqeuest0 = qMesg.onOff;
        break;
      case 1:
        actuatorReqeuest1 = qMesg.onOff;
        break;
      default:
        break;
      }
    }

    // ON-OFF DELAY 

    // ACTUATOR 0
    if (actuatorReqeuest0 && !actuatorActual0)
    {
      displayQMesg.index = 0; 
      displayQMesg.data.actuator = 1;
      displayQMesg.duration = onDelaySec0;
      xQueueSend(displayQueue, &displayQMesg, 0);
    }

    if (actuatorReqeuest0 && !actuatorActual0 && (onDelaySec0 == 0))
    {
      printf("[ACTUATORS] RELAY0 ON !!!\n");
      digitalWrite(CFG_RELAY0_PIN, CFG_RELAY0_ON_LEVEL);
      actuatorActual0 = 1;
      offDelaySec0 = CFG_RELAY0_OFF_DELAY;
    }
    else
    {
      if (onDelaySec0 > 0)
      {
        // count-down
        onDelaySec0--;
      }
    }


    if (!actuatorReqeuest0 && actuatorActual0)
    {
      displayQMesg.index = 0; 
      displayQMesg.data.actuator = 0;
      displayQMesg.duration = offDelaySec0;
      xQueueSend(displayQueue, &displayQMesg, 0);
    }

    // switch off if offDelay is not counting
    if (!actuatorReqeuest0 && actuatorActual0 && (offDelaySec0 == 0))
    {
      printf("[ACTUATORS] RELAY0 OFF !!!\n");
      digitalWrite(CFG_RELAY0_PIN, !CFG_RELAY0_ON_LEVEL);
      actuatorActual0 = 0;
      onDelaySec0 = CFG_RELAY0_ON_DELAY;
    }
    else
    {
      // count-down
      if (offDelaySec0 > 0)
      {
        offDelaySec0--;
      }
    }

    if (( onDelaySec0 > 0) || (offDelaySec0 > 0) )
    {
      printf("[ACTUATORS] onDelaySec0=%d - offDelaySec0=%d\n", onDelaySec0, offDelaySec0);
    }


    // ACTUATOR 1
    if (actuatorReqeuest1 && !actuatorActual1)
    {
      displayQMesg.index = 1; 
      displayQMesg.data.actuator = 1;
      displayQMesg.duration = onDelaySec1;
      xQueueSend(displayQueue, &displayQMesg, 0);
    }

    if (actuatorReqeuest1 && !actuatorActual1 && (onDelaySec1 == 0))
    {
      printf("[ACTUATORS] RELAY1 ON !!!\n");
      digitalWrite(CFG_RELAY1_PIN, CFG_RELAY1_ON_LEVEL);
      actuatorActual1 = 1;
      offDelaySec1 = CFG_RELAY1_OFF_DELAY;
    }
    else
    {
      if (onDelaySec1 > 0)
      {
        // count-down
        onDelaySec1--;
      }
    }


    if (!actuatorReqeuest1 && actuatorActual1)
    {
      displayQMesg.index = 1; 
      displayQMesg.data.actuator = 0;
      displayQMesg.duration = offDelaySec1;
      xQueueSend(displayQueue, &displayQMesg, 0);
    }

    if (!actuatorReqeuest1 && actuatorActual1 && (offDelaySec1 == 0))
    {
      printf("[ACTUATORS] RELAY1 OFF !!!\n");
      digitalWrite(CFG_RELAY1_PIN, !CFG_RELAY1_ON_LEVEL);
      actuatorActual1 = 0;
      onDelaySec1 = CFG_RELAY1_ON_DELAY;
    }
    else
    {
      if (offDelaySec1 > 0)
      {
      // count-down
        offDelaySec1--;
      }
    }

    if (( onDelaySec1 > 0) || (offDelaySec1 > 0) )
    {
      printf("[ACTUATORS] onDelaySec1=%d - offDelaySec1=%d\n", onDelaySec1, offDelaySec1);
    }

  }
};


void initActuators(void)
{
  printf("[ACTUATORS] init\n");

  actuatorsQueue = xQueueCreate(5, sizeof(actuatorQueueItem_t));

  if (actuatorsQueue == 0)
  {
    printf("[ACTUATORS] Cannot create actuatorsQueue. This is FATAL\n");
  }

  // create task
  xTaskCreatePinnedToCore(actuatorsTask, "actuatorsTask", 4096, NULL, 10, &actuatorsTaskHandle, 0);
}

// end of file