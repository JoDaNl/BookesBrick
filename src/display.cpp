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



//helper function : send text-message to display
void displayText(String * message, displayMessageType_t messageType, uint8_t timeInSec)
{
  displayQueueItem_t   displayQMesg;

  displayQMesg.type  = e_display_text;
  displayQMesg.data.textData.stringPtr = message; 
  displayQMesg.data.textData.timeInSec = timeInSec;
  displayQMesg.data.textData.messageType = messageType;
  displayQMesg.valid = true;

  displayQueueSend(&displayQMesg , 0);
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
  printf("[DISP] no display enabled\n");
#else
  printf("[DISP] init\n");
  
#ifdef CFG_DISPLAY_CYD_320x240
  initDisplay_cyd_320x240();
#endif
#ifdef CFG_DISPLAY_CYD_480x320
  initDisplay_cyd_480x320();
#endif

  displayQueue = xQueueCreate(5, sizeof(displayQueueItem_t));
  if (displayQueue == 0)
  {
    printf("[DISP] Cannot create displayQueue. This is FATAL");
  }

  // create task
  xTaskCreatePinnedToCore(displayTask, "displayTask", 10 * 1024, NULL, 10, &displayTaskHandle, 1);
#endif
}

// end of file
