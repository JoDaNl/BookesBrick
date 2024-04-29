// display.c

#include <Arduino.h>
#include "display.h"

#ifndef CFG_DISPLAY_NONE

// Queue - declared globally, as it it used in display driver modules
xQueueHandle displayQueue = NULL;

// display task
static TaskHandle_t displayTaskHandle = NULL;
#endif

// wrapper for sendQueue 
int displayQueueSend(displayQueueItem_t * displayQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;
#ifndef CFG_DISPLAY_NONE
  if (displayQueue != NULL)
  {
    r =  xQueueSend(displayQueue, displayQMesg , xTicksToWait);
  }
#endif
  return r;
}

#ifdef CFG_DISPLAY_CYD_320x240
extern void  initDisplay_cyd_320x240(void);
#endif
#ifdef CFG_DISPLAY_CYD_480x320
extern void  initDisplay_cyd_480x320(void);
#endif

void initDisplay(void)
{
#ifdef CFG_DISPLAY_NONE
  printf("[DISPLAY] no display enabled\n");
#else
  printf("[DISPLAY] init\n");
  
#ifdef CFG_DISPLAY_CYD_320x240
  initDisplay_cyd_320x240();
#endif
#ifdef CFG_DISPLAY_CYD_480x320
  initDisplay_cyd_480x320();
#endif

  displayQueue = xQueueCreate(5, sizeof(displayQueueItem_t));
  if (displayQueue == 0)
  {
    printf("[DISPLAY] Cannot create displayQueue. This is FATAL");
  }

  // create task
  xTaskCreate(displayTask, "displayTask", 10 * 1024, NULL, 10, &displayTaskHandle);
#endif
}

// end of file
