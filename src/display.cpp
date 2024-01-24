// display.c

#include <Arduino.h>
#include "display.h"

#ifndef CFG_DISPLAY_NONE
// Queues
xQueueHandle displayQueue = NULL;
// display task
static TaskHandle_t displayTaskHandle = NULL;
#endif

// wrapper for sendQueue 
int displayQueueSend(const void * const displayQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;
#ifndef CFG_DISPLAY_NONE
  if (displayQueue != NULL)
  {
    r =  xQueueSend(displayQueue, &displayQMesg , xTicksToWait);
  }
#endif
  return r;
}

void initDisplay(void)
{
#ifdef CFG_DISPLAY_NONE
  printf("[DISPLAY] no display enabled\n");
#else
  printf("[DISPLAY] init\n");
  
  displayQueue = xQueueCreate(5, sizeof(displayQueueItem_t));
  if (displayQueue == 0)
  {
    printf("[DISPLAY] Cannot create displayQueue. This is FATAL");
  }

  // create task
  xTaskCreate(displayTask, "displayTask", 4096, NULL, 10, &displayTaskHandle);
#endif
}

// end of file
