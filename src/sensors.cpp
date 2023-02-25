//
//  actuators.cpp
//

#include "config.h"

#include <Arduino.h>
#include "sensors.h"
#include "comms.h"
#include "i2c_lcd_16x2.h"

#include <OneWire.h>
#include <DallasTemperature.h>
#include "smooth.h"

// Temperature sensor


// Queues
xQueueHandle sensorsQueue = NULL;

// Actuators task
static TaskHandle_t sensorsTaskHandle = NULL;

// ============================================================================
// SENSORS TASK
// ============================================================================

void sensorsTask(void *arg)
{
  static float temp_sens = 0.0;
  static int temp_smooth = 0;
  static int temp_sprev = 0;
  static int num_sensors;
  static displayQueueItem_t qMesg;

  static OneWire oneWire(CFG_TEMP_PIN);
  static DallasTemperature sensors(&oneWire);
  static Smooth smooth;


  qMesg.type     = e_temperature;
  qMesg.index    = 0; 
  qMesg.duration = 0; // 0=display forever


  // set-up DS18B20 temperature sensor(s)
  sensors.begin();
  sensors.setResolution(12);

  // TODO : support for multiple sensors
  num_sensors = sensors.getDS18Count();


  while (true)
  {
    // printf("[SENSORS] PRINTING..\n");

    sensors.requestTemperatures();

    // TODO : get temperatures for all discovered devices
    // TODO : system should work (actuators) even if NO sensor is present !
#ifdef CFG_TEMP_IN_CELCIUS
    temp_sens = sensors.getTempCByIndex(0);
#elif CFG_TEMP_IN_FARENHEID
    temperature = sensors.getTempFByIndex(0);
#else
     printf("[SENSORS] NO TEMPERATURE SENSOR DEFINED\n");
#endif

    // printf("[SENSORS] Temperature sensor %d is: %f\n",0, sensors.getTempCByIndex(0) );

    // smooth measured temp * 100 (2 digits accuracy)
    smooth.setValue(temp_sens * 10);

    if (smooth.isValid())
    {
      temp_smooth = smooth.getValue();
    
     // send temperature to display-queue
     // only send when temperature has changed.
      if ((temp_smooth != temp_sprev) && displayQueue != NULL)
      {
        qMesg.data.temperature = temp_smooth;

        // not checking result (must be pdPASS)...
        xQueueSend(displayQueue, &qMesg, 0);
        xQueueSend(communicationQueue, &temp_smooth, 0);


        temp_sprev = temp_smooth;
      }
    }


    vTaskDelay(2000 / portTICK_RATE_MS);
  }
};


void initSensors(void)
{
  printf("[SENSORS] init\n");

  sensorsQueue = xQueueCreate(5, sizeof(uint8_t));

  if (sensorsQueue == 0)
  {
    printf("[SENSORS] Cannot create sensorsQueue. This is FATAL\n");
  }

  // create task
  xTaskCreatePinnedToCore(sensorsTask, "sensorsTask", 4096, NULL, 10, &sensorsTaskHandle, 0);
}

// end of file