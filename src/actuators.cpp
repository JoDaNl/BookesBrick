//
//  actuators.cpp
//

#include "config.h"
#include <Arduino.h>
#include "actuators.h"
#include "controller.h"

#define LOG_TAG "ACTUATORS"

// GLOBALS
static xQueueHandle actuatorsQueue = NULL;

static void setActuator(uint8_t number, uint8_t onOff)
{
  static bool pinModeNotSet = true;
  uint8_t pin;
  uint8_t onLevel;
  bool valid;

  if (pinModeNotSet)
  {
    pinModeNotSet = false;
    pinMode(CFG_RELAY0_PIN, CFG_RELAY0_OUTPUT_TYPE);
    pinMode(CFG_RELAY1_PIN, CFG_RELAY1_OUTPUT_TYPE);
  }

  valid = false;

  switch (number)
  {
    case 0:
      pin     = CFG_RELAY0_PIN;
      onLevel = CFG_RELAY0_ON_LEVEL;
      valid   = true;
      break;
    case 1:
      pin     = CFG_RELAY1_PIN;
      onLevel = CFG_RELAY1_ON_LEVEL;
      valid   = true;
      break;  
  }

  if (onOff == 0)
  {
    // invert level
    onLevel = !onLevel;
  }

  if (valid)
  {
    digitalWrite(pin, onLevel);
    printf("[ACTUATORS] digitalWrite(%d, %d)\n",pin, onLevel);    
    ESP_LOGI(LOG_TAG,"digitalWrite(%d, %d)",pin, onLevel);
  }
}


void powerUpActuators(void)
{
  setActuator(0, 0);
  setActuator(1, 0);
}


// ============================================================================
// ACTUATORS TASK
// ============================================================================

static void actuatorsTask(void *arg)
{
  static actuatorQueueItem_t actuatorMesg;
  static controllerQItem_t controllerMsg;

  // TODO : use arrays...you know how to..!
  static uint8_t actuatorReqeuest0;
  static uint8_t actuatorReqeuest1;
  static uint8_t actuatorActual0;
  static uint8_t actuatorActual1;
  static uint16_t onDelaySec0;
  static uint16_t onDelaySec1;
  static uint16_t offDelaySec0;
  static uint16_t offDelaySec1;

  // INIT DELAYS
  // we also could start the task loop with all delays initially zero,
  // however presetting them ensures a initial delay after boot.
  // This prevent violation the delays in case of accidental boot-
  // loops during code development or possible power-outages
  onDelaySec0   = CFG_RELAY0_ON_DELAY;
  onDelaySec1   = CFG_RELAY1_ON_DELAY;
  offDelaySec0  = CFG_RELAY0_OFF_DELAY;
  offDelaySec1  = CFG_RELAY0_OFF_DELAY;

  actuatorActual0 = 0;
  actuatorActual1 = 0;

  // Task loop
  while (true)
  {
    // get message from queue
    if (xQueueReceive(actuatorsQueue, &actuatorMesg, 1000 / portTICK_RATE_MS) == pdTRUE)
    {
      // printf("[ACTUATORS] received qMesg.data=%d\n", actuatorMesg.data);
      ESP_LOGI(LOG_TAG,"received qMesg.data=%d", actuatorMesg.data);
      actuatorReqeuest0 = actuatorMesg.data & 1;
      actuatorReqeuest1 = (actuatorMesg.data >> 1) & 1;
    }

    // ON-OFF DELAY 
    // Actuators may have a pre-defined (see config.h) on and/or off delay
    // when an actuator goes into off-state it immediately can be triggered to go into on-state 
    // but then CFG_RELAY0_ON_DELAY time must  have been passed before the actuator is 
    // actually switched on...the task will take care for the delayed-switch-on
    // Similar is true for switching off.
    // All delays are in seconds/loop-iterations


    // ACTUATOR 0 - Switch ON request

    if (actuatorReqeuest0 && !actuatorActual0)
    {
      controllerMsg.type                    = e_mtype_backend;
      controllerMsg.mesg.backendMesg.mesgId = e_msg_backend_act_delay;
      controllerMsg.mesg.backendMesg.number = 0;
      controllerMsg.mesg.backendMesg.data   = onDelaySec0;
      controllerQueueSend(&controllerMsg, 0);
    }

    // switch on if onDelay is not counting
    if (actuatorReqeuest0 && !actuatorActual0 && (onDelaySec0 == 0))
    {
      // printf("[ACTUATORS] RELAY0 ON !!!\n");
      ESP_LOGI(LOG_TAG,"RELAY0 ON !!!");
      setActuator(0, 1);
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
      controllerMsg.type                    = e_mtype_backend;
      controllerMsg.mesg.backendMesg.mesgId = e_msg_backend_act_delay;
      controllerMsg.mesg.backendMesg.number = 0;
      controllerMsg.mesg.backendMesg.data   = offDelaySec0;
      controllerQueueSend(&controllerMsg, 0);
    }

    // switch off if offDelay is not counting
    if (!actuatorReqeuest0 && actuatorActual0 && (offDelaySec0 == 0))
    {
      // printf("[ACTUATORS] RELAY0 OFF !!!\n");
      ESP_LOGI(LOG_TAG,"RELAY0 OFF !!!");
      setActuator(0, 0);
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

    // if (( onDelaySec0 > 0) || (offDelaySec0 > 0) )
    // {
    //    printf("[ACTUATORS] onDelaySec0=%d - offDelaySec0=%d\n", onDelaySec0, offDelaySec0);
    // }



    // ACTUATOR 1 - Switch ON request

    if (actuatorReqeuest1 && !actuatorActual1)
    {
      controllerMsg.type                    = e_mtype_backend;
      controllerMsg.mesg.backendMesg.mesgId = e_msg_backend_act_delay;
      controllerMsg.mesg.backendMesg.number = 1;
      controllerMsg.mesg.backendMesg.data   = onDelaySec1;
      controllerQueueSend(&controllerMsg, 0);
    }

    // switch on if onDelay is not counting
    if (actuatorReqeuest1 && !actuatorActual1 && (onDelaySec1 == 0))
    {
      // printf("[ACTUATORS] RELAY1 ON !!!\n");
      ESP_LOGI(LOG_TAG,"RELAY1 ON !!!");
      setActuator(1, 1);
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
      controllerMsg.type                      = e_mtype_backend;
      controllerMsg.mesg.backendMesg.mesgId = e_msg_backend_act_delay;
      controllerMsg.mesg.backendMesg.number = 1;
      controllerMsg.mesg.backendMesg.data   = offDelaySec1;
      controllerQueueSend(&controllerMsg, 0);   
    }

    // switch off if offDelay is not counting
    if (!actuatorReqeuest1 && actuatorActual1 && (offDelaySec1 == 0))
    {
      // printf("[ACTUATORS] RELAY1 OFF !!!\n");
      ESP_LOGI(LOG_TAG,"RELAY1 OFF !!!");
      setActuator(1, 0);
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

    // if (( onDelaySec1 > 0) || (offDelaySec1 > 0) )
    // {
    //   printf("[ACTUATORS] onDelaySec1=%d - offDelaySec1=%d\n", onDelaySec1, offDelaySec1);
    // }

  }
};



// wrapper for sendQueue 
int actuatorsQueueSend(actuatorQueueItem_t * actuatorQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (actuatorsQueue != NULL)
  {
    r =  xQueueSend(actuatorsQueue, actuatorQMesg , xTicksToWait);
  }

  return r;
}



void initActuators(void)
{
  static TaskHandle_t actuatorsTaskHandle = NULL;

  // printf("[ACTUATORS] init\n");
  ESP_LOGI(LOG_TAG,"initActuators()");

  actuatorsQueue = xQueueCreate(5, sizeof(actuatorQueueItem_t));

  if (actuatorsQueue == 0)
  {
    // printf("[ACTUATORS] Cannot create actuatorsQueue. This is FATAL\n");
    ESP_LOGE(LOG_TAG,"Cannot create actuatorsQueue. This is FATAL");
  }

  // create task
  xTaskCreate(actuatorsTask, "actuatorsTask", 4 * 1024, NULL, 10, &actuatorsTaskHandle);
}

// end of file