//
// BOOKES BRICK
//

// Includes
#include <Arduino.h>
#include "esp_log.h"
#include "config.h"
#include "controller.h"
// #include "blinkled.h"
#include "wifiman.h"
#include "comms.h"
#include "sensors.h"
// #include "monitor.h"
#include "actuators.h"
#include "display.h"

#if (CFG_ENABLE_HYDROBRICK == true)
#include "hydrobrick.h"
#endif

#define LOG_TAG "MAIN"

// Config struct
configValues_t config;

// General TODO-list
// - go into condig mode when there is no SSID/PWD stored in filesystem
// - when file-system cannot be openened show message/go in to FATAL-mode
// - Add mode into GUI status bar (mode=ONLINE/LOCAL/ERROR/FATAL/CONFIG)
// - Add method (button/GUI/DRD) to enter config mode
// - Add support for NTC sensors, using (external) ADC
// - FIX: When tempsensor reads no value the graph will contine to draw last valid temperature...it should dislay not temperature for those measurements
// - Add way to set the update frequency of the temperature graph

// ============================================================================
// SETUP
// ============================================================================

void setup()
{
  Serial.begin(CFG_BAUDRATE);

  while (!Serial) // wait for Serial to become available
  {
    delay(100);
  }

  // Welcome message
  ESP_LOGI(LOG_TAG, "");
  ESP_LOGI(LOG_TAG, "==========================");
  ESP_LOGI(LOG_TAG, "    BookesBrick Started");
  ESP_LOGI(LOG_TAG, "==========================");
  ESP_LOGI(LOG_TAG, "");

  ESP_LOGI(LOG_TAG, "start setup: %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  ESP_LOGI(LOG_TAG, "ESP.getSdkVersion()=%s", ESP.getSdkVersion());

  // Immediately set outputs/actuators to non-active
  // this to prevent unwatend heating/cooling after a reboot
  powerUpActuators();

#ifdef BOARD_HAS_RGB_LED
  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
#endif


  // FOR DEBUG ONLY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#include "../../../_config/myconfig.h" 
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  // pinMode(GPIO_NUM_7, OUTPUT);

#if (CFG_COMM_WM_USE_PIN == true)
  // check config button state at power-up
  config.inConfigMode = checkBootConfigMode();
#endif

  //  config.inConfigMode = false;

  // Start all tasks

  initDisplay();
  ESP_LOGI(LOG_TAG, "initDisplay done: %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());

  delay(100);
  initCommmunication();
  ESP_LOGI(LOG_TAG, "initCommunication done: %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());

  delay(10);
  initController();
  ESP_LOGI(LOG_TAG, "initController done: %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());

  delay(10);
  initSensors();
  ESP_LOGI(LOG_TAG, "initSensors done: %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());

  delay(10);
  initActuators();
  ESP_LOGI(LOG_TAG, "initActuators done: %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());

#if (CFG_ENABLE_HYDROBRICK == true)
  delay(10);
  initHydroBrick();
  ESP_LOGI(LOG_TAG, "initHydroBrick done: %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());
#endif

  delay(10);
  // terminate standard 'Arduino' task
  vTaskDelete(NULL);
};

void loop() 
{
  delay(100);
};

// end of file
