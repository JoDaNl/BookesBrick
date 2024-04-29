//
// BOOKES BRICK
//

// Includes
#include <Arduino.h>
#include "config.h"
#include "controller.h"
// #include "blinkled.h"
#include "wifiman.h"
#include "comms.h"
#include "sensors.h"
#include "monitor.h"
#include "actuators.h"
#include "display.h"

#define LOG_TAG "main"


// Taken from arduino-esp main.cpp
// #ifndef ARDUINO_LOOP_STACK_SIZE
// #ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
// #define ARDUINO_LOOP_STACK_SIZE 8192
// #else
// #define ARDUINO_LOOP_STACK_SIZE CONFIG_ARDUINO_LOOP_STACK_SIZE
// #endif
// #endif


// ============================================================================
// SETUP
// ============================================================================

void setup()
{
  // Immediately set outputs/actuators to non-active
  powerUpActuators();

  Serial.begin(115200);

  // while (!Serial) ;   // wait for Serial to become available
  delay(100);

  // Welcome message
  ESP_LOGI(LOG_TAG,"");
  ESP_LOGI(LOG_TAG,"========================");
  ESP_LOGI(LOG_TAG,"[MAIN] Started !!!\n");
  ESP_LOGI(LOG_TAG,"========================");


#if (CFG_COMM_WM_USE_PIN == true)
  // check config button state at power-up
  config.inConfigMode = checkBootConfigMode();
#endif

  // config.inConfigMode = true;

  // Start all tasks

  //  initBlinkLed(CFG_LED_PIN);
  delay(10);
  initDisplay();
  delay(10);

  initController();
  delay(100);

  initWiFi(config.inConfigMode);

  if (~config.inConfigMode)
  {
    delay(10);
    initSensors();
    delay(10);
    initCommmunication();
    delay(10);
    initActuators();
    delay(10);
    //   initMonitor();
    //   delay(10);
  }

};

void loop()
{
  // just a very long wait-time, as Arduino-style loop is not used
  vTaskDelay(3600000 / portTICK_RATE_MS);
};

// end of file
