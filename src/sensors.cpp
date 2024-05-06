//
//  sensors.cpp
//

// public domain libs
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#if (CFG_TEMP_DS18B20_CHECK_COUNTERFEIT == true)
#include <CheckDS18B20.h>
#endif
// common includes
#include "config.h"
#include "smooth.h"

// task includes
#include "sensors.h"
#include "controller.h"
#include "comms.h"

// Loop delay
#define DELAY (200)

// Queues
static xQueueHandle sensorsQueue = NULL;

// Sensors task-handle
static TaskHandle_t sensorsTaskHandle = NULL;

// OneWire control
static OneWire oneWire(CFG_TEMP_PIN);

#if (CFG_TEMP_DS18B20_CHECK_COUNTERFEIT == true)
#include <CheckDS18B20.h>
using namespace CheckDS18B20;

static void checkDS18B20Conterfeit()
{
  DS18B20_family_enum result;
  
  result = ds18b20_family(&oneWire, 0);

  if (result == FAMILY_A1) 
  {
    Serial.println("[DS18B20] ORIGINAL DS18D20\n");
  } else {
    Serial.println("[DS18B20] CONTERFEIT DS18B20 - OR NO SENSOR CONNECTED !!!\n");
  }
}
#endif



// ============================================================================
// SENSORS TASK
// ============================================================================

static void sensorsTask(void *arg)
{
  static float tempSens = 0.0;
  static int tempSmooth = 0;
  static bool tempError = false;
  static int numSensors = 0;
  static bool tempValid = false;
  static controllerQItem_t qControllerMesg;


  static DallasTemperature sensors(&oneWire);
  static Smooth<CFG_TEMP_SMOOTH_SAMPLES> smooth;

  static uint16_t millisForConversion;

  millisForConversion = DELAY;

  smooth.setMaxDeviation(CFG_TEMP_MAX_DEVIATIONB); // 1 degree

  checkDS18B20Conterfeit();

  // TODO : support for NTC sensors

  // TASK LOOP
  while (true)
  {
    // printf("[SENSORS] LOOP..\n");

    // default
    tempValid = false;

    // If sensor(s) not initialised yet, or there is a sensor error --> Initialise sensor
    if (numSensors == 0 || tempError)
    {
      printf("[SENSORS] TEMPERATURE NOT OPENED OR SENSOR ERROR !!!\n");
#if (CFG_TEMP_SENSOR_SIMULATION == false)
      
      // oneWire.depower();
      // digitalWrite(CFG_TEMP_PIN, 0);
      // vTaskDelay(DELAY / portTICK_RATE_MS);    

      // set-up DS18B20 temperature sensor(s)   
      // TODO : support for multiple sensors
      sensors.begin();
      sensors.setResolution(12);    
      sensors.setWaitForConversion(false);
      millisForConversion = sensors.millisToWaitForConversion();
      numSensors = sensors.getDS18Count();

      printf("[SENSORS] Number of DS18B20 sensors found=%d\n", numSensors);
      printf("[SENSORS] isParasitePowerMode()=%d\n",sensors.isParasitePowerMode());
      printf("[SENSORS] millisToWaitForConversion()=%d\n",millisForConversion);
#else
      numSensors = 1;
#endif
      // inform controller about number of sensors discovered (currently information is not used)
      // qControllerMesg.type = e_mtype_sensor;
      // qControllerMesg.mesg.sensorMesg.mesgId = e_msg_sensor_numSensors;
      // qControllerMesg.mesg.sensorMesg.data = numSensors;
      // qControllerMesg.valid = true;
      // controllerQueueSend(&qControllerMesg, 0);
    }

    // If there is a sensor initialised --> Measure temperature
    if (numSensors > 0)
    {
#if (CFG_TEMP_SENSOR_SIMULATION == false)     
      sensors.requestTemperaturesByIndex(0);
      vTaskDelay(millisForConversion / portTICK_RATE_MS);            
#ifdef CFG_TEMP_IN_CELCIUS
      tempSens = sensors.getTempCByIndex(0);
      printf("[SENSORS] getTempCByIndex(0)=%f\n",tempSens);
      tempError = (tempSens == DEVICE_DISCONNECTED_C);
#elif CFG_TEMP_IN_FARENHEID
      temperature = sensors.getTempFByIndex(0);
      tempError = (tempSens == DEVICE_DISCONNECTED_F);
#else
#error "NO TEMPERATURE SENSOR DEFINED\n"
#endif
#else // CFG_TEMP_SENSOR_SIMULATION == true
      tempSens = 5.0 + rand() * 30.0 / RAND_MAX;
      tempError = false;
#endif

      if (tempError == false)
      {
        // smooth measured temp * 10 (1 digit accuracy)
        smooth.setValue(tempSens * 10);

        if (smooth.isValid())
        {
          tempSmooth = smooth.getValue();
          tempValid = true;
        }
      }
    }

    // TODO : add optional power cycle

    // reset 1-wire bus
    // vTaskDelay(500 / portTICK_RATE_MS);
    // oneWire.reset();
    // vTaskDelay(500 / portTICK_RATE_MS);
    // vTaskDelay(500 / portTICK_RATE_MS);
    // sensors.begin();
    // vTaskDelay(500 / portTICK_RATE_MS);

    // Always send temperature + valid-flag to controller-queue
    qControllerMesg.type = e_mtype_sensor;
    qControllerMesg.mesg.sensorMesg.mesgId = e_msg_sensor_temperature;
    qControllerMesg.mesg.sensorMesg.data = tempSmooth;
    qControllerMesg.valid = tempValid;
    controllerQueueSend(&qControllerMesg, 0);

    if (numSensors == 0)
    {
      vTaskDelay(DELAY / portTICK_RATE_MS);
    }
  }
}

// wrapper for sendQueue
int sensorsQueueSend(uint8_t *sensorsQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (sensorsQueue != NULL)
  {
    r = xQueueSend(sensorsQueue, sensorsQMesg, xTicksToWait);
  }

  return r;
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
  xTaskCreate(sensorsTask, "sensorsTask", 2 * 1024, NULL, 10, &sensorsTaskHandle);
}

// end of file