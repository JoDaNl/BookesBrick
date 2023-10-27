//
//  controller.cpp
//

#include <Arduino.h>

#include "config.h"
#include "blinkled.h"
#include "controller.h"

// Queues
xQueueHandle controllerQueue = NULL;

// controller task handle
static TaskHandle_t controllerTaskHandle = NULL;

// ============================================================================
// CONTROLLER TASK
// ============================================================================

void controllerTask(void *arg)
{
  static float temp_sens = 0.0;
  static bool temp_error = false;

  static controllerQItem_t qMesgRecv;

  static blinkLedQMesg_t blinkLedQMesg;


  if (config.inConfigMode)
  {
    blinkLedQMesg.mesgId = e_msg_blinkled_blink;
    blinkLedQMesg.onTimeMs = 10;
    blinkLedQMesg.offTimeMs = 200;
  }
  else
  {
    blinkLedQMesg.mesgId = e_msg_blinkled_blink;
    blinkLedQMesg.onTimeMs = 500;
    blinkLedQMesg.offTimeMs = 1500;
  }
  xQueueSend(blinkLedQueue, &blinkLedQMesg , 0);


  while (true)
  {

    if (xQueueReceive(controllerQueue, &qMesgRecv, 10 / portTICK_RATE_MS) == pdTRUE)
    {
      // printf("[CONTROLLER] message received, type=%d\n", qMesgRecv.type);
      // printf("[CONTROLLER] message sensorMsg, type=%d\n", qMesgRecv.mesg.sensorMesg.mesgId);
      // printf("[CONTROLLER] message sensorMsg, data=%d\n", qMesgRecv.mesg.sensorMesg.data);

      switch (qMesgRecv.type)
      {
      case e_mtype_button:
        switch (qMesgRecv.mesg.buttonMesg.mesgId)
        {
        case e_msg_button_short:
          printf("[CONTROL] received e_msg_button_short\n");
          break;
          break;

        case e_msg_button_long:
          printf("[CONTROL] received e_type_button_long\n");
          break;

        default:
          break;
        }
        break;

      case e_mtype_sensor:
        switch (qMesgRecv.mesg.sensorMesg.mesgId)
        {
        case e_msg_sensor_numSensors:
          printf("[CONTROL] received e_msg_sensor_numSensors, data=%d\n", qMesgRecv.mesg.sensorMesg.data);
          break;
        case e_msg_sensor_temperature:
          printf("[CONTROL] received e_msg_sensor_temperature, data=%d\n", qMesgRecv.mesg.sensorMesg.data);
          break;
        default:
          break;
        }
        break;

      default:
        break;
      }
    }
  }
};

void initController(void)
{
  printf("[CONTROLLER] init\n");

  controllerQueue = xQueueCreate(10, sizeof(controllerQItem_t));

  if (controllerQueue == 0)
  {
    printf("[CONTROLLER] Cannot create controllerQueue. This is FATAL\n");
  }

  // create tasks
  xTaskCreate(controllerTask, "controllerTask", 4096, NULL, 10, &controllerTaskHandle);
}

// end of file