//
//  actuators.cpp
//

#include "config.h"
#include <Arduino.h>
#include "actuators.h"
#include "i2c_lcd_16x2.h"

// GLOBALS
xQueueHandle actuatorsQueue = NULL;

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

  // Set all actuators off
  digitalWrite(CFG_RELAY0_PIN, !CFG_RELAY0_ON_LEVEL);
  digitalWrite(CFG_RELAY1_PIN, !CFG_RELAY1_ON_LEVEL);


  // INIT DELAYS
  // we also could start the task loop with all delays initially zero,
  // however presetting them ensures a initial delay after boot.
  // This prevent violation the delays in case of accidental boot-
  // loops during code development or possible power-outages
  onDelaySec0 = CFG_RELAY0_ON_DELAY;
  onDelaySec1 = CFG_RELAY1_ON_DELAY;
  offDelaySec0 = CFG_RELAY0_OFF_DELAY;
  offDelaySec1 = CFG_RELAY0_OFF_DELAY;

  // Task loop
  while (true)
  {
    // get message from queue
    if (xQueueReceive(actuatorsQueue, &qMesg, 1000 / portTICK_RATE_MS) == pdTRUE)
    {
      printf("[ACTUATORS] received qMesg.number=%d - qMesg.onOff=%d\n", qMesg.number, qMesg.onOff);

      // qMesg.number is the actuator=number
      // qMesg.onOff is the deribed/new state of the actuator
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
    // Actuators may have a pre-defined (see config.h) on and/or off delay
    // when an actuator goes into off-state it immediately can be triggered to go into on-state 
    // but then CFG_RELAY0_ON_DELAY time must must have been passed before the actuator is 
    // actually switched on...the task will take care for the delayed-switch-on
    // Similar is true for switching off.
    // All delays are in seconds/loop-iterations


    // ACTUATOR 0 - Switch ON request

    if (actuatorReqeuest0 && !actuatorActual0)
    {
      displayQMesg.index = 0; 
      displayQMesg.data.actuator = 1;
      displayQMesg.duration = onDelaySec0;
      xQueueSend(displayQueue, &displayQMesg, 0);
    }

    // switch on if onDelay is not counting
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
        // Count down
        onDelaySec0--;
      }
    }



    // ACTUATOR 0 - Switch OFF request

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
      // Count down
      if (offDelaySec0 > 0)
      {
        offDelaySec0--;
      }
    }

    if (( onDelaySec0 > 0) || (offDelaySec0 > 0) )
    {
      // printf("[ACTUATORS] onDelaySec0=%d - offDelaySec0=%d\n", onDelaySec0, offDelaySec0);
    }



    // ACTUATOR 1 - Switch ON request

    if (actuatorReqeuest1 && !actuatorActual1)
    {
      displayQMesg.index = 1; 
      displayQMesg.data.actuator = 1;
      displayQMesg.duration = onDelaySec1;
      xQueueSend(displayQueue, &displayQMesg, 0);
    }

    // switch on if onDelay is not counting
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



    // ACTUATOR 1 - Switch OFF request

    if (!actuatorReqeuest1 && actuatorActual1)
    {
      displayQMesg.index = 1; 
      displayQMesg.data.actuator = 0;
      displayQMesg.duration = offDelaySec1;
      xQueueSend(displayQueue, &displayQMesg, 0);
    }

    // switch off if offDelay is not counting
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
      // printf("[ACTUATORS] onDelaySec1=%d - offDelaySec1=%d\n", onDelaySec1, offDelaySec1);
    }

  }
};


void initActuators(void)
{
  static TaskHandle_t actuatorsTaskHandle = NULL;

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