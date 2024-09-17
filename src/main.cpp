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

#include "hydrobrick.h"

#define LOG_TAG "main"

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
  // Immediately set outputs/actuators to non-active
  // this to prevent unwatend heating/cooling after a reboot
  powerUpActuators();

  Serial.begin(CFG_BAUDRATE);

  while (!Serial) // wait for Serial to become available
  {
    delay(100);
  }

  // Welcome message
  ESP_LOGI(LOG_TAG, "");
  ESP_LOGI(LOG_TAG, "========================");
  ESP_LOGI(LOG_TAG, "[MAIN] Started !!!\n");
  ESP_LOGI(LOG_TAG, "========================");

  // pinMode(GPIO_NUM_7, OUTPUT);

#if (CFG_COMM_WM_USE_PIN == true)
  // check config button state at power-up
  config.inConfigMode = checkBootConfigMode();
#endif

  config.inConfigMode = false;

  // TEST CODE FOR HYDROBRICK GATEWAY
  initHydroBrick();

  // Start all tasks

  //  initBlinkLed(CFG_LED_PIN);
  delay(10);
  initDisplay();
  delay(10);
  initWiFi(config.inConfigMode);
  delay(10);
  initController();
  delay(10);

  if (~config.inConfigMode)
  {
    delay(10);
    initSensors();
    delay(10);
    initCommmunication();
    delay(10);
    initActuators();
    delay(10);
    initMonitor();
    delay(10);
  }


  static hydroQMesg_t qmesg;

//  qmesg.mesgId = e_msg_hydro_cmd_get_reading;
  qmesg.mesgId = e_msg_hydro_cmd_scan_bricks;
  qmesg.data = 0;

  delay(1000);
  
  hydroQueueSend(&qmesg,0);




  // terminate standard 'Arduino' task
  vTaskDelete(NULL);
};

void loop() {
  // empty task loop
};

// end of file
