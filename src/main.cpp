//
// BOOKUS BEER AUTOMATION
//

// Includes
#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "monitor.h"
#include "comms.h"
#include "actuators.h"
#include "i2c_lcd_16x2.h"


// ============================================================================
// SETUP
// ============================================================================

void setup()
{
 
  Serial.begin(115200);

  while (!Serial)
    ;   // wait for Serial to become available
  delay(200);

  // Welcome message
  printf("\n");
  printf("========================\n");
  printf("[SETUP] Started !!!\n");
  printf("========================\n");

  // Start all tasks
  delay(100);
  initMonitor();
  delay(100);
  initDisplay(); 
  delay(100);
  initCommmunication();
  delay(100);
  initActuators();
  delay(100);
  initSensors();
  printf("========================\n");
  printf("\n");
  
};


void loop()
{
  sleep(10000); // TODO : does this make sense?
};
