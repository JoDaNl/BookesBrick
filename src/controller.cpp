//
//  controller.cpp
//

#include <Arduino.h>

#include "config.h"
#include "blinkled.h"
#include "actuators.h"
#include "display.h"
#include "controller.h"
#include "comms.h"

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

  static uint32_t heartbeatTime = 0;
  static uint16_t nextReceiveTime;
  static bool heartbeatTimeout = false;

  static controllerQItem_t    qMesgRecv;
  static blinkLedQMesg_t      blinkLedQMesg;
  static actuatorQueueItem_t  actuatorsQMesg; // TODO : rename type/align with others
  static displayQueueItem_t   displayQMesg;

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
        {
          switch (qMesgRecv.mesg.buttonMesg.mesgId)
          {
            case e_msg_button_short:
              printf("[CONTROL] received e_msg_button_short\n");
              break;

            case e_msg_button_long:
              printf("[CONTROL] received e_type_button_long\n");
              break;

            default:
              break;
          }
        }
        break;

        case e_mtype_sensor:
        {
          switch (qMesgRecv.mesg.sensorMesg.mesgId)
          {
            case e_msg_sensor_numSensors:
              printf("[CONTROL] received e_msg_sensor_numSensors, data=%d\n", qMesgRecv.mesg.sensorMesg.data);
              break;
            case e_msg_sensor_temperature:
              printf("[CONTROL] received e_msg_sensor_temperature, data=%d\n", qMesgRecv.mesg.sensorMesg.data);
              // send temperature to communication task
              if (qMesgRecv.valid)
              {
                if (communicationQueue != NULL) // due to late init of COMMS / TODO: Wrapper macro around xQueueSend to check if Queue is not NULL
                {
                  xQueueSend(communicationQueue, &qMesgRecv.mesg.sensorMesg.data , 0);
                } 
              }
              
              // send temperature to display
              displayQMesg.type             = e_temperature;
              displayQMesg.data.temperature = qMesgRecv.mesg.sensorMesg.data;
              displayQMesg.valid            = qMesgRecv.valid;
              displayQueueSend(&displayQMesg , 0);
              // xQueueSend(displayQueue, &displayQMesg , 0);
              break;
            default:
              break;
          }
        }
        break;

        case e_mtype_backend:
        {
          switch (qMesgRecv.mesg.backendMesg.mesgId)
          {
            case e_msg_backend_actuators:
              printf("[CONTROL] received e_msg_backend_actuators, data=%d\n", qMesgRecv.mesg.backendMesg.data);
              // send actuators/relay state to actuators tasks
              actuatorsQMesg.data = qMesgRecv.mesg.backendMesg.data;
              xQueueSend(actuatorsQueue, &actuatorsQMesg, 0);

              // send actuators to display
              displayQMesg.type = e_actuator;
              displayQMesg.data.actuators = qMesgRecv.mesg.backendMesg.data;
              displayQMesg.valid          = qMesgRecv.mesg.backendMesg.valid;
              displayQueueSend(&displayQMesg , 0);
              // xQueueSend(displayQueue, &displayQMesg , 0);

              break;
            case e_msg_backend_act_delay:
              printf("[CONTROL] received e_msg_backend_act_delay, nr=%d, data=%d\n",qMesgRecv.mesg.backendMesg.number, qMesgRecv.mesg.backendMesg.data);

              // send delay information to display
              displayQMesg.type       = e_delay;
              displayQMesg.number     = qMesgRecv.mesg.backendMesg.number;
              displayQMesg.data.compDelay = qMesgRecv.mesg.backendMesg.data;
              displayQMesg.valid       = true; 
              displayQueueSend(&displayQMesg , 0);
              // xQueueSend(displayQueue, &displayQMesg , 0);
              break;
            case e_msg_backend_heartbeat:
              printf("[CONTROL] received e_msg_backend_heartbeat, data=%d\n", qMesgRecv.mesg.backendMesg.data);
              heartbeatTime = millis();
              nextReceiveTime = qMesgRecv.mesg.backendMesg.data;
              heartbeatTimeout = false;

              // send heartBeat-detected information to display
              displayQMesg.type       = e_heartbeat;
              displayQMesg.data.heartOn = true;
              displayQMesg.valid      = qMesgRecv.mesg.backendMesg.valid;
              displayQueueSend(&displayQMesg , 0);
              // xQueueSend(displayQueue, &displayQMesg , 0);
              break;

            case e_msg_backend_temp_setpoint:
              printf("[CONTROL] received e_msg_backend_temp_setpoint, data=%d\n", qMesgRecv.mesg.backendMesg.data);

              // send temperature set-point information to display
              displayQMesg.type          = e_setpoint;
              displayQMesg.data.setPoint = qMesgRecv.mesg.backendMesg.data;
              displayQMesg.valid         = qMesgRecv.mesg.backendMesg.valid;
              displayQueueSend(&displayQMesg , 0);
              // xQueueSend(displayQueue, &displayQMesg , 0);
              break;

            default:
              break;
          }
        }
        break;

        case e_mtype_wifi:
        {
          switch (qMesgRecv.mesg.WiFiMesg.mesgId)
          {
            case e_msg_WiFi_rssi:
              printf("[CONTROL] received e_msg_WiFi_rssi, data=%d\n", qMesgRecv.mesg.WiFiMesg.data);

              // send RSSI to display
              displayQMesg.type             = e_rssi;
              displayQMesg.data.temperature = qMesgRecv.mesg.WiFiMesg.data;
              displayQMesg.valid            = true; 
              displayQueueSend(&displayQMesg , 0);
              // xQueueSend(displayQueue, &displayQMesg , 0);
              break;
            default:
              break;
          }
        }
        break;

        default:
          break;
      }
    }

    // check for expiration of heartbeat time (at 80% of nextReceive...so heart blinks briefly)

    if ( ((millis() - heartbeatTime)/1000 > nextReceiveTime) && heartbeatTimeout * 0.8 == false)
    {
      heartbeatTimeout = true;
      // send heartbeat-timeout information to display
      displayQMesg.type         = e_heartbeat;
      displayQMesg.data.heartOn = false;
      displayQueueSend(&displayQMesg , 0);
      // xQueueSend(displayQueue, &displayQMesg , 0);
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