//
//  monitor.cpp
//

#include "config.h"

#include <Arduino.h>
#include <preferences.h>

// #include "monitor.h"

// Queue
static xQueueHandle monitorQueue = NULL;

// Actuators task
static TaskHandle_t monitorTaskHandle = NULL;

static int bootCounter=0;
static Preferences prefs;

// ============================================================================
// MONITOR TASK
// ============================================================================

static void monitorTask(void *arg)
{
  static uint8_t qReceiveMesg;
  static uint16_t onlineTimeoutCount = CFG_COMM_ONLINE_TIMEOUT;
  static uint8_t blink = 0;

  while (true)
  {
    if (xQueueReceive(monitorQueue, &qReceiveMesg, 1000 / portTICK_RATE_MS) == pdTRUE)
    {
      printf("[MONITOR] received qMesg=%d\n", qReceiveMesg);

      // 0 = heartbeat message
      if (qReceiveMesg == 0) 
      {
        // We are on-line , re-trigger time-out counter
        onlineTimeoutCount = CFG_COMM_ONLINE_TIMEOUT;
      }

      // qDisplayMesg.type = e_error;
      // qDisplayMesg.data.error = qReceiveMesg; // when 0 --> online
      // qDisplayMesg.index = 0;
      // qDisplayMesg.duration = 0;
      // xQueueSend(displayQueue, &qDisplayMesg, 0);
    }

    if (onlineTimeoutCount > 0)
    {
      onlineTimeoutCount--;
    }

    if (onlineTimeoutCount == 0)
    {
      // printf("[MONITOR] OFFLINE detected\n");
      // qDisplayMesg.type = e_error;
      // qDisplayMesg.data.error = 1; // TODO : replace by constants/defines...  1=offline
      // qDisplayMesg.index = 0;
      // qDisplayMesg.duration = 0;
      // xQueueSend(displayQueue, &qDisplayMesg, 0);      
    }

    // printf("[MONITOR] Time-out counter=%d\n",onlineTimeoutCount);

    // send heartbeat message to display
    // qDisplayMesg.type = e_heartbeat;
    // qDisplayMesg.index = 0;
    // qDisplayMesg.duration = 0;
    // qDisplayMesg.data.heartbeat = blink;
    // xQueueSend(displayQueue, &qDisplayMesg, 0);

//     printf("[MONITOR] bootCounter=%d\n", bootCounter);
    blink = !blink;
  }


};

// wrapper for sendQueue 
int monitorQueueSend(uint8_t * monitorQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (monitorQueue != NULL)
  {
    r =  xQueueSend(monitorQueue, monitorQMesg , xTicksToWait);
  }

  return r;
}




void initMonitor(void)
{
  printf("[MONITOR] init\n");

//#define BB_BOOTCOUNT 1

#ifdef BB_BOOTCOUNT
  prefs.begin("BOOTCOUNT", false);  // read only mode
  bootCounter = prefs.getInt("bootcounter", 0);
  bootCounter++;
  prefs.putInt("bootcounter", bootCounter);
  prefs.end();
#endif

  monitorQueue = xQueueCreate(5, sizeof(uint8_t));

  if (monitorQueue == 0)
  {
    printf("[MONITOR] Cannot create monitorQueue. This is FATAL\n");
  }

  // create task
  xTaskCreatePinnedToCore(monitorTask, "monitorTask", 1 * 1024, NULL, 10, &monitorTaskHandle, 1);
}

// end of file