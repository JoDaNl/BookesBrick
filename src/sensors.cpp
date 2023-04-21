//
//  sensors.cpp
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
#define SMOOTH_SAMPLES  7 // must be odd number


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
  static bool temp_error = false;
  static int num_sensors;
  static displayQueueItem_t qMesg;

  static OneWire oneWire(CFG_TEMP_PIN);
  static DallasTemperature sensors(&oneWire);
  static Smooth<SMOOTH_SAMPLES> smooth;

  smooth.setMaxDeviation(10); // 1 degree

  qMesg.type = e_temperature;
  qMesg.index = 0;
  qMesg.duration = 0; // 0=display forever


  // set-up DS18B20 temperature sensor(s)
  sensors.begin();
  sensors.setResolution(12);

  // TODO : support for multiple sensors
  num_sensors = sensors.getDS18Count();
  printf("[SENSORS] Number of DS18B20 sensors found=%d\n", num_sensors);


  // TODO : support for NTC sensors

  while (true)
  {
    // printf("[SENSORS] PRINTING..\n");

    if (num_sensors > 0)
    {
      sensors.requestTemperatures();

      // TODO : get temperatures for all discovered devices
      // TODO : system should work (actuators) even if NO sensor is present ! There's a dependency right now

#ifdef CFG_TEMP_IN_CELCIUS
      temp_sens = sensors.getTempCByIndex(0);
      temp_error = (temp_sens == DEVICE_DISCONNECTED_C);
#elif CFG_TEMP_IN_FARENHEID
      temperature = sensors.getTempFByIndex(0);
      temp_error = (temp_sens == DEVICE_DISCONNECTED_F);
#else
      printf("[SENSORS] NO TEMPERATURE SENSOR DEFINED\n");
#endif

      if (temp_error)
      {
        printf("[SENSORS] TEMPERATURE SENSOR ERROR !!!\n");
        
        // reset 1-wire bus
        vTaskDelay(500 / portTICK_RATE_MS);
        oneWire.reset();
        vTaskDelay(500 / portTICK_RATE_MS);
        sensors.begin();
        vTaskDelay(500 / portTICK_RATE_MS);
      }
      else
      {

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