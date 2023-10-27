//
// BOOKES BEER AUTOMATION
//

// Includes
#include <Arduino.h>
#include "config.h"
#include "controller.h"
#include "blinkled.h"
#include "wifiman.h"
#include "comms.h"
#include "sensors.h"
#include "monitor.h"
#include "actuators.h"
//#include "i2c_lcd_16x2.h"



// ============================================================================
// SETUP
// ============================================================================

void setup()
{

  Serial.begin(115200);

  while (!Serial)
    ;   // wait for Serial to become available
  delay(100);

  // Welcome message
  printf("\n");
  printf("========================\n");
  printf("[SETUP] Started !!!\n");
  printf("========================\n");


#if (CFG_COMM_WM_USE_PIN == true)
  //check config button state at power-up
  config.inConfigMode = checkBootConfigMode();
#endif

  // Start all tasks
  
  initBlinkLed(CFG_LED_PIN);
  delay(10);
//  initDisplay(); 
  delay(10);

  initController();
  delay(10);

  if (config.inConfigMode)
  {
    initConfigPortal();
  }
  else
  {
    initWiFi();
    delay(10);
    initSensors();
    delay(10);
//  initMonitor();
//  delay(10);
//  initCommmunication();
//  delay(10);
//  initActuators();
//  delay(10);

  }

  printf("========================\n");
  printf("[SETUP] Done !!!\n");
  printf("========================\n");
  
};


void loop()
{
  sleep(10000); // TODO : does this make sense?
};

// end of file
