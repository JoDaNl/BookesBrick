//
//  controller.cpp
//

#include <Arduino.h>

#include "blinkled.h"


#define DELAY (10)


// Queues
xQueueHandle blinkLedQueue = NULL;

static uint8_t _pin;

// task handle
static TaskHandle_t blinkLedTaskHandle = NULL;

// ============================================================================
// BLINK-LED TASK
// ============================================================================

void blinkLedTask(void *arg)
{
  static blinkLedQMesg_t newQMesgRecv;
  static blinkLedQMesg_t qMesgRecv;
  static uint16_t countDown = 0;
  static bool ledOn = 0;
  static bool prevLedOn = 0;

  // default values
  qMesgRecv.mesgId = e_msg_blinkled_off;
  qMesgRecv.onTimeMs = 1000;
  qMesgRecv.offTimeMs = 1000;

  while (true)
  {

    if (xQueueReceive(blinkLedQueue, &newQMesgRecv, DELAY / portTICK_RATE_MS) == pdTRUE)
    {
      qMesgRecv = newQMesgRecv;
    }

    switch (qMesgRecv.mesgId)
    {
    case e_msg_blinkled_off:
      ledOn = false;
      countDown = 0;
      break;

    case e_msg_blinkled_on:
      ledOn = true;
      countDown = 0;
      break;

    case e_msg_blinkled_blink:
      if (countDown <= 0)
      {
        if (ledOn)
        {
          ledOn = false;
          countDown = qMesgRecv.offTimeMs;
        }
        else
        {
          ledOn = true;
          countDown = qMesgRecv.onTimeMs;
        }
      }
      countDown = countDown - DELAY;
      break;

    default:
      break;
    }

    if (ledOn != prevLedOn)
    {
      digitalWrite(_pin, ledOn);
      prevLedOn = ledOn;
    }
  }
};


// ============================================================================
// INIT BLINK-LED
// ============================================================================

void initBlinkLed(uint8_t pin)
{
  printf("[BLINKLED] init\n");

  _pin = pin;
  pinMode(pin, OUTPUT);

  blinkLedQueue = xQueueCreate(10, sizeof(blinkLedQMesg_t));

  if (blinkLedQueue == 0)
  {
    printf("[BLINKLED] Cannot create controllerQueue. This is FATAL\n");
  }

  // create task
  xTaskCreate(blinkLedTask, "blinkLedTask", 4096, NULL, 10, &blinkLedTaskHandle);
}

// end of file