//
//  sensors.cpp
//

// public domain libs
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// common includes
#include "config.h"
#include "smooth.h"

// task includes
#include "sensors.h"
#include "controller.h"
#include "comms.h"

#define  SENSOR_SIMULATION 1

// Temperature sensor
#define SMOOTH_SAMPLES  7 // must be odd number

// Queues
xQueueHandle sensorsQueue = NULL;

// Sensors task-handle
static TaskHandle_t sensorsTaskHandle = NULL;

// ============================================================================
// SENSORS TASK
// ============================================================================

void sensorsTask(void *arg)
{
  static float temp_sens = 0.0;
  static int temp_smooth = 0;
  static bool temp_error = false;
  static int num_sensors = 0;
  static controllerQItem_t qControllerMesg;

  static OneWire oneWire(CFG_TEMP_PIN);
  static DallasTemperature sensors(&oneWire);
  static Smooth<SMOOTH_SAMPLES> smooth;

  smooth.setMaxDeviation(10); // 1 degree


#ifndef SENSOR_SIMULATION
  // set-up DS18B20 temperature sensor(s)
  sensors.begin();
  sensors.setResolution(12);

  // TODO : support for multiple sensors
  num_sensors = sensors.getDS18Count();
  printf("[SENSORS] Number of DS18B20 sensors found=%d\n", num_sensors);
#else
  num_sensors = 1;
#endif

  // inform controller about number of sensors discovered
  if (controllerQueue)
  {
    qControllerMesg.type = e_mtype_sensor;
    qControllerMesg.mesg.sensorMesg.mesgId = e_msg_sensor_numSensors;
    qControllerMesg.mesg.sensorMesg.data = num_sensors;
    xQueueSend(controllerQueue, &qControllerMesg , 0);
  }

  // TODO : support for NTC sensors

  while (true)
  {
    // printf("[SENSORS] PRINTING..\n");

#ifndef SENSOR_SIMULATION
    if (num_sensors > 0)
    {
      sensors.requestTemperatures();

      // TODO : get temperatures for all discovered devices

#ifdef CFG_TEMP_IN_CELCIUS
      temp_sens = sensors.getTempCByIndex(0);
      temp_error = (temp_sens == DEVICE_DISCONNECTED_C);
#elif CFG_TEMP_IN_FARENHEID
      temperature = sensors.getTempFByIndex(0);
      temp_error = (temp_sens == DEVICE_DISCONNECTED_F);
#else
      printf("[SENSORS] NO TEMPERATURE SENSOR DEFINED\n");
#endif

#else // SENSOR_SIMULATION
     {
      temp_sens = 20.0 + rand()*5.0/RAND_MAX;
      temp_error = false;
#endif
      if (temp_error)
      {
        printf("[SENSORS] TEMPERATURE SENSOR ERROR !!!\n");
        
        // TODO : add optional power cycle 

        // reset 1-wire bus
        vTaskDelay(500 / portTICK_RATE_MS);
        oneWire.reset();
        vTaskDelay(500 / portTICK_RATE_MS);
        sensors.begin();
        vTaskDelay(500 / portTICK_RATE_MS);
      }
      else
      {
        // smooth measured temp * 100 (2 digits accuracy)
        smooth.setValue(temp_sens * 10);

        if (smooth.isValid())
        {
          temp_smooth = smooth.getValue();

          // send temperature to controller-queue
          if (controllerQueue)
          {
            qControllerMesg.type = e_mtype_sensor;
            qControllerMesg.mesg.sensorMesg.mesgId = e_msg_sensor_temperature;
            qControllerMesg.mesg.sensorMesg.data = temp_smooth;
            xQueueSend(controllerQueue, &qControllerMesg , 0);
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
  xTaskCreate(sensorsTask, "sensorsTask", 4096, NULL, 10, &sensorsTaskHandle);
}

// end of file