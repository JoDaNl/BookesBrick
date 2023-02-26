//
//  monitor.cpp
//

#include "config.h"

#include <Arduino.h>
#include "monitor.h"
#include "i2c_lcd_16x2.h"

// Queue
xQueueHandle monitorQueue = NULL;

// Actuators task
static TaskHandle_t monitorTaskHandle = NULL;

// ============================================================================
// MONITOR TASK
// ============================================================================

static void monitorTask(void *arg)
{
  static uint8_t qReceiveMesg;
  static uint8_t ledOn = 0;
  static displayQueueItem_t qDisplayMesg;
  static uint16_t onlineTimeoutCount = CFG_COMM_ONLINE_TIMEOUT;

  // set LED as ouput, as it will toggle every second
  pinMode(LED_BUILTIN, OUTPUT);

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

      qDisplayMesg.type = e_error;
      qDisplayMesg.data.error = qReceiveMesg; // when 0 --> online
      qDisplayMesg.index = 0;
      qDisplayMesg.duration = 0;
      xQueueSend(displayQueue, &qDisplayMesg, 0);
    }

    if (onlineTimeoutCount > 0)
    {
      onlineTimeoutCount--;
    }

    if (onlineTimeoutCount == 0)
    {
      // printf("[MONITOR] OFFLINE detected\n");
      qDisplayMesg.type = e_error;
      qDisplayMesg.data.error = 1; // TODO : replace by constants/defines...  1=offline
      qDisplayMesg.index = 0;
      qDisplayMesg.duration = 0;
      xQueueSend(displayQueue, &qDisplayMesg, 0);      
    }

    // printf("[MONITOR] Time-out counter=%d\n",onlineTimeoutCount);

    // toggle builtin led
    digitalWrite(LED_BUILTIN, ledOn);

    // send heartbeat message to display

    qDisplayMesg.type = e_heartbeat;
    qDisplayMesg.index = 0;
    qDisplayMesg.duration = 0;
    qDisplayMesg.data.heartbeat = ledOn;
    xQueueSend(displayQueue, &qDisplayMesg, 0);

    ledOn = !ledOn;
  }
};

void initMonitor(void)
{
  printf("[MONITOR] init\n");

  monitorQueue = xQueueCreate(5, sizeof(uint8_t));

  if (monitorQueue == 0)
  {
    printf("[MONITOR] Cannot create monitorQueue. This is FATAL\n");
  }

  // create task
  xTaskCreatePinnedToCore(monitorTask, "monitorTask", 4096, NULL, 10, &monitorTaskHandle, 0);
}

// end of file