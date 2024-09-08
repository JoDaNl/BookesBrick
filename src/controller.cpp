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

#define LOG_TAG "CONTROL"

// Queues
static xQueueHandle controllerQueue = NULL;

// controller task handle
static TaskHandle_t controllerTaskHandle = NULL;

// ============================================================================
// CONTROLLER TASK
// ============================================================================

void controllerTask(void *arg)
{
  static float temp_sens = 0.0;
  static bool temp_error = false;

  static uint16_t nextReceiveTime;
  static bool heartbeatTimeout = false;

  static controllerQItem_t    qMesgRecv;
  // static blinkLedQMesg_t      blinkLedQMesg;
  static actuatorQueueItem_t  actuatorsQMesg;
  static displayQueueItem_t   displayQMesg;
  static WiFiQueueItem_t      WiFIQMesg;
  static commsQueueItem_t     commsQMesg;

  static uint8_t actuators = 0;
  static uint16_t compDelay = 0;

  // Start WiFi
  WiFIQMesg.mesgId = e_cmd_start_wifi;
  WiFiQueueSend(&WiFIQMesg, 0);

  String * welcome = new String("Welcome !");
  displayText(welcome, e_status_bar, 10);

  // if (config.inConfigMode)
  // {
  //   blinkLedQMesg.mesgId = e_msg_blinkled_blink;
  //   blinkLedQMesg.onTimeMs = 10;
  //   blinkLedQMesg.offTimeMs = 200;
  // }
  // else
  // {
  //   blinkLedQMesg.mesgId = e_msg_blinkled_blink;
  //   blinkLedQMesg.onTimeMs = 500;
  //   blinkLedQMesg.offTimeMs = 1500;
  // }
  // blinkLedQueueSend(&blinkLedQMesg , 0);


#ifdef BOARD_HAS_RGB_LED
  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
#endif


  while (true)
  {

    if (xQueueReceive(controllerQueue, &qMesgRecv, 10 / portTICK_RATE_MS) == pdTRUE)
    {
      switch (qMesgRecv.type)
      {
        case e_mtype_button:
        {
          switch (qMesgRecv.mesg.buttonMesg.mesgId)
          {
            case e_msg_button_short:
              printf("[CTRL] received e_msg_button_short\n");
              break;

            case e_msg_button_long:
              printf("[CTRL] received e_type_button_long\n");
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
              printf("[CTRL] received e_msg_sensor_numSensors, data=%d\n", qMesgRecv.mesg.sensorMesg.data);
              break;

            case e_msg_sensor_temperature:
              // printf("[CTRL] received e_msg_sensor_temperature, data=%d\n", qMesgRecv.mesg.sensorMesg.data);

              // send temperature to communication task
              commsQMesg.valid = qMesgRecv.valid;
              commsQMesg.temperature = qMesgRecv.mesg.sensorMesg.data;
              communicationQueueSend(&commsQMesg, 0);
              
              // send temperature to display
              displayQMesg.type             = e_temperature;
              displayQMesg.data.temperature = qMesgRecv.mesg.sensorMesg.data;
              displayQMesg.valid            = qMesgRecv.valid;
              displayQueueSend(&displayQMesg , 0);
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
              printf("[CTRL] received e_msg_backend_actuators, data=%d, valid=%d\n", qMesgRecv.mesg.backendMesg.data, qMesgRecv.mesg.backendMesg.valid);
              
              // send actuators/relay state to actuators tasks
              if (qMesgRecv.mesg.backendMesg.data != actuators)
              {
                actuators = qMesgRecv.mesg.backendMesg.data;
                actuatorsQMesg.data = qMesgRecv.mesg.backendMesg.data;
                actuatorsQueueSend(&actuatorsQMesg, 0);
              }

              // send actuators to display task
              displayQMesg.type = e_actuator;
              displayQMesg.data.actuators = qMesgRecv.mesg.backendMesg.data;
              displayQMesg.valid          = qMesgRecv.mesg.backendMesg.valid;
              displayQueueSend(&displayQMesg , 0);
              break;
            
            case e_msg_backend_act_delay:
              // printf("[CTRL] received e_msg_backend_act_delay, nr=%d, data=%d\n",qMesgRecv.mesg.backendMesg.number, qMesgRecv.mesg.backendMesg.data);

              // send delay information to display
              displayQMesg.type           = e_delay;
              displayQMesg.number         = qMesgRecv.mesg.backendMesg.number;
              displayQMesg.data.compDelay = qMesgRecv.mesg.backendMesg.data;
              displayQMesg.valid          = true; 
              displayQueueSend(&displayQMesg , 0);

              compDelay = qMesgRecv.mesg.backendMesg.data;
              break;

            case e_msg_backend_heartbeat:
              // printf("[CTRL] received e_msg_backend_heartbeat, data=%d,%d\n", qMesgRecv.mesg.backendMesg.data>>8, qMesgRecv.mesg.backendMesg.data&255);
              // send heartBeat-detected information to display
              displayQMesg.type           = e_heartbeat;
              displayQMesg.data.heartBeat = qMesgRecv.mesg.backendMesg.data;
              displayQMesg.valid          = qMesgRecv.mesg.backendMesg.valid;
              displayQueueSend(&displayQMesg , 0);
              break;

            case e_msg_backend_temp_setpoint:
              printf("[CTRL] received e_msg_backend_temp_setpoint, data=%d, valid=%d\n", qMesgRecv.mesg.backendMesg.data, qMesgRecv.mesg.backendMesg.valid);
              // send temperature set-point information to display
              displayQMesg.type             = e_setpoint;
              displayQMesg.data.temperature = qMesgRecv.mesg.backendMesg.data;
              displayQMesg.valid            = qMesgRecv.mesg.backendMesg.valid;
              displayQueueSend(&displayQMesg , 0);
              break;

            case e_msg_backend_device_name:
              printf("[CTRL] received e_msg_backend_device_a\n");
              // send device name to display (using helper function)
              displayText(qMesgRecv.mesg.backendMesg.stringPtr, e_device_name, 0);
              break;
            default:
              break;
          }
        }
        break;

        case e_mtype_wifi:
        {
          static char label;
          // printf("[CTRL] received e_mtype_wifi (rssi=%d, status=%d)\n",qMesgRecv.mesg.WiFiMesg.rssi, qMesgRecv.mesg.WiFiMesg.wifiStatus);

          switch (qMesgRecv.mesg.WiFiMesg.wifiStatus)
          {
            case e_msg_wifi_unconnected:
              label = '-';
              break;
            case e_msg_wifi_accespoint_connected:
              label = 'A';
              break;
            case e_msg_wifi_internet_connected:
              label = 'I';
              break;
            case e_msg_wifi_portal_opened:
              label =  'p';
              break;
            case e_msg_wifi_unknown:
              label = '?';
              break;                                                
          }

          // send WiFi information to display
          displayQMesg.type                 = e_wifi;
          displayQMesg.data.wifiData.rssi   = qMesgRecv.mesg.WiFiMesg.rssi;
          displayQMesg.data.wifiData.status = label;        
          displayQMesg.valid                = qMesgRecv.valid;
          displayQueueSend(&displayQMesg , 0);
        }
        break;

        case e_mtype_time:
        {
          switch (qMesgRecv.mesg.timeMesg.mesgId)
          {
            case e_msg_time_data:
              // printf("[CTRL] received e_msg_time_data, data=%2d:%02d\n", qMesgRecv.mesg.timeMesg.data >> 8, qMesgRecv.mesg.timeMesg.data & 255);
              // send TIME to display
              displayQMesg.type             = e_time;
              displayQMesg.data.temperature = qMesgRecv.mesg.timeMesg.data;
              displayQMesg.valid            = qMesgRecv.valid; 
              displayQueueSend(&displayQMesg , 0);
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

    // if ( ((millis() - heartbeatTime)/1000 > nextReceiveTime) && heartbeatTimeout * 0.8 == false)
    // {
    //   heartbeatTimeout = true;
    //   // send heartbeat-timeout information to display
    //   displayQMesg.type         = e_heartbeat;
    //   displayQMesg.data.heartOn = false;
    //   displayQueueSend(&displayQMesg , 0);
    // }
    
    
#ifdef BOARD_HAS_RGB_LED
    switch (actuators)
    {
      case 1: // COOL -> BLUE
        digitalWrite(RGB_LED_R, 1);
        digitalWrite(RGB_LED_G, 1);
        digitalWrite(RGB_LED_B, compDelay & 1);
        break;
      case 2: // HEAT --> RED
        digitalWrite(RGB_LED_R, 0);
        digitalWrite(RGB_LED_G, 1);
        digitalWrite(RGB_LED_B, 1);
        break;
      default: // OTHERS --> OFF
        digitalWrite(RGB_LED_G, 1);
        digitalWrite(RGB_LED_G, 1);
        digitalWrite(RGB_LED_B, 1);
        break;
    }
#endif

  }

};

// wrapper for sendQueue 
int controllerQueueSend(controllerQItem_t * controllerQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (controllerQueue != NULL)
  {
    r =  xQueueSend(controllerQueue, controllerQMesg , xTicksToWait);
  }

  return r;
}


void initController(void)
{
  printf("[CTRL] init\n");

  controllerQueue = xQueueCreate(16, sizeof(controllerQItem_t));

  if (controllerQueue == 0)
  {
    printf("[CTRL] Cannot create controllerQueue. This is FATAL\n");
  }

  // create tasks
  xTaskCreatePinnedToCore(controllerTask, "controllerTask", 4 * 1024, NULL, 10, &controllerTaskHandle, 1);
}

// end of file