//
//  sensors.cpp
//

#include <Arduino.h>
#include "config.h"
#include "smooth.h"
#include "sensors.h"
#include "controller.h"

#define LOG_TAG "SENS"

// SENSOR SPECIFIC INCLUDES

#if (CFG_TEMP_SENSOR_TYPE_DS18B20_ENABLED == true)
#include <OneWire.h>
#include <DallasTemperature.h>
#if (CFG_TEMP_SENSOR_TYPE_DS18B20_ENABLED == true)
#if (CFG_TEMP_SENSOR_TYPE_DS18B20_CHECK_COUNTERFEIT == true)
#include <CheckDS18B20.h>
using namespace CheckDS18B20;
#endif
#endif
#endif

#if (CFG_TEMP_SENSOR_TYPE_SHT3X_ENABLED == true)
#include <Wire.h>
#include <SensirionI2cSht3x.h>
#endif

#if (CFG_TEMP_SENSOR_TYPE_SHT4X_ENABLED == true)
#include <Wire.h>
#include <SensirionI2cSht4x.h>
#endif

// Loop delay
#define DELAY (1000)

// Queues
//static QueueHandle_t sensorsQueue = NULL;
static QueueHandle_t sensorsQueue = NULL;

// Sensors task-handle
static TaskHandle_t sensorsTaskHandle = NULL;

#ifdef BLABLA
#if (CFG_TEMP_SENSOR_SCAN_I2C == true)
void scanI2Cbus(void)
{
  byte error, address;
  int nDevices;

  ESP_LOGI(LOG_TAG, "I2C scanning");

  nDevices = 0;
  for (address = 1; address < 127; address++)
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      ESP_LOGI(LOG_TAG, "I2C device found at address 0x%02X", address);
      nDevices++;
    }
    else if (error == 4)
    {
      ESP_LOGI(LOG_TAG, "I2CUnknown error at address 0x%02X", address);
    }
  }

  if (nDevices == 0)
  {
    ESP_LOGI(LOG_TAG, "No I2C devices found");
  }
  else
  {
    ESP_LOGI(LOG_TAG, "I2C scanning done");
  }
}
#endif // CFG_TEMP_SENSOR_SCAN_I2C
#endif

class temperatureSensorBase
{
public:
  bool init(void)
  {
    return false;
  };

  void setCelcius(bool)
  {
    tempInCelcius = true;
  };

  void setFahrenheid(bool)
  {
    tempInCelcius = false;
  };

  bool getTemperature(float &temperature)
  {
    temperature = -127;
    return true;
  }

private:
  bool tempInCelcius = true;
};

#if (CFG_TEMP_SENSOR_TYPE_SIMULATION_ENABLED == true)
class temperatureSensorSimulator : public temperatureSensorBase
{
public:
  bool init(void)
  {
    return true;
  }

  bool getTemperature(float &temperature)
  {
    temperature = 5.0 + rand() * 30.0 / RAND_MAX;
    return true;
  }
};
#endif

#if (CFG_TEMP_SENSOR_TYPE_DS18B20_ENABLED == true)
class temperatureSensorDS18B20 : public temperatureSensorBase
{
private:
  OneWire oneWire;
  DallasTemperature sensors;
  uint8_t numSensors;

#if (CFG_TEMP_SENSOR_TYPE_DS18B20_CHECK_COUNTERFEIT == true)
  void checkDS18B20Counterfeit()
  {
    CheckDS18B20::DS18B20_family_enum result;

    result = CheckDS18B20::ds18b20_family(&oneWire, 0);

    if (result == CheckDS18B20::FAMILY_A1)
    {
      ESP_LOGI(LOG_TAG, "DS18B20 Original device");
    }
    else
    {
      ESP_LOGE(LOG_TAG, "DS18B20 Counterfeit device");

    }
  }
#endif

public:
  bool init(void)
  {
    bool status;
    uint16_t millisForConversion;

    status = false;
    oneWire.begin(CFG_TEMP_PIN);
    sensors.setOneWire(&oneWire);

    sensors.begin();
    sensors.setResolution(12);
    sensors.setWaitForConversion(true);
    millisForConversion = sensors.millisToWaitForConversion();
    numSensors = sensors.getDS18Count();

    ESP_LOGI(LOG_TAG, "Number of DS18B20 sensors found=%d", numSensors);

    if (numSensors > 0)
    {
      status = true;

      ESP_LOGI(LOG_TAG, "isParasitePowerMode()=%d\n", sensors.isParasitePowerMode());
      ESP_LOGI(LOG_TAG, "millisToWaitForConversion()=%d\n", millisForConversion);

#if (CFG_TEMP_SENSOR_TYPE_DS18B20_CHECK_COUNTERFEIT == true)
      checkDS18B20Counterfeit();
#endif
    }

    return status;
  };

  bool getTemperature(float &temperature)
  {
    bool status;

    status = false;
    temperature = DEVICE_DISCONNECTED_C;

    if (numSensors > 0)
    {
      sensors.requestTemperaturesByIndex(0);
      vTaskDelay(sensors.millisToWaitForConversion() / portTICK_PERIOD_MS);

#ifdef CFG_TEMP_IN_CELCIUS
      temperature = sensors.getTempCByIndex(0);
      status = (temperature != DEVICE_DISCONNECTED_C);
#elif CFG_TEMP_IN_FARENHEID
      temperature = sensors.getTempFByIndex(0);
      tempError = (tempSens == DEVICE_DISCONNECTED_F);
#endif
    }
    return status;
  }
};
#endif

#if (CFG_TEMP_SENSOR_TYPE_SHT3X_ENABLED == true)
class temperatureSensorSHT3x : public temperatureSensorBase
{
private:
  SensirionI2cSht3x sensor;
  char errorMessage[64];

public:
  bool init(void)
  {
    bool status;

#if (defined CFG_I2C_SDA && defined CFG_I2C_SCL)
    Wire.begin(CFG_I2C_SDA, CFG_I2C_SCL);
#else
    Wire.begin();
#endif
    sensor.begin(Wire, SHT30_I2C_ADDR_44);
    sensor.stopMeasurement();
    vTaskDelay(1 / portTICK_PERIOD_MS);
    status = (sensor.softReset() == 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    return status;
  }

  bool getTemperature(float &temperature)
  {
    static bool status;
    static int16_t error;
    static float humidity;

    status = false;

    error = sensor.measureSingleShot(REPEATABILITY_MEDIUM, false, temperature, humidity);
    if (error != NO_ERROR)
    {
      errorToString(error, errorMessage, sizeof errorMessage);
      ESP_LOGE(LOG_TAG, "Sensor error: %s", errorMessage);
    }
    else
    {
      status = true;
    }

    return status;
  }
};
#endif

#if (CFG_TEMP_SENSOR_TYPE_SHT4X_ENABLED == true)
class temperatureSensorSHT4x : public temperatureSensorBase
{
private:
  SensirionI2cSht4x sensor;
  int16_t error;
  char errorMessage[64];

public:
  bool init(void)
  {
    bool status;

#if (defined CFG_I2C_SDA && defined CFG_I2C_SCL)
    Wire.begin(CFG_I2C_SDA, CFG_I2C_SCL);
#else
    Wire.begin();
#endif

    sensor.begin(Wire, SHT30_I2C_ADDR_44);
    status = (sensor.softReset() == 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    return status;
  }

  bool getTemperature(float &temperature)
  {
    static bool status;
    static float humidity;

    status = false;

    error = sensor.measureHighPrecision(temperature, humidity);
    if (error != NO_ERROR)
    {
      errorToString(error, errorMessage, sizeof errorMessage);
      ESP_LOGE(LOG_TAG, "Sensor error: %s", errorMessage);
    }
    else
    {
      status = true;
    }

    return status;
  }
};
#endif

// ============================================================================
// SENSORS TASK
// ============================================================================

static void sensorsTask(void *arg)
{
  static float tempSens = 0.0;
  static int tempSmooth = 0;
  static bool tempInitValid = false;
  static bool tempSensValid = false;
  static bool tempSmoothValid = false;
  static controllerQItem_t qControllerMesg;
  static Smooth<CFG_TEMP_SMOOTH_NUM_SAMPLES> smooth;

#if (CFG_TEMP_SENSOR_TYPE_SIMULATION_ENABLED == true)
  static temperatureSensorSimulator sensor;
#elif (CFG_TEMP_SENSOR_TYPE_DS18B20_ENABLED == true)
  static temperatureSensorDS18B20 sensor;
#elif (CFG_TEMP_SENSOR_TYPE_SHT3X_ENABLED == true)
  static temperatureSensorSHT3x sensor;
#elif (CFG_TEMP_SENSOR_TYPE_SHT4X_ENABLED == true)
  static temperatureSensorSHT4x sensor;
#else

#endif

  smooth.setMaxDeviation(CFG_TEMP_SMOOTH_MAX_DEVIATION); // 1 degree

  tempInitValid = false;
  tempSensValid = false;
  tempSmoothValid = false;

  // TASK LOOP
  while (true)
  {
    // If sensor(s) not initialised yet, or there is a sensor error --> Initialise sensor
    if ((tempInitValid == false) || (tempSensValid == false))
    {
      tempInitValid = sensor.init();
      ESP_LOGI(LOG_TAG,"Temp sensor init: %d", tempInitValid);
    }

    // If there is a sensor initialised --> Measure temperature
    if (tempInitValid)
    {
      tempSensValid = sensor.getTemperature(tempSens);

      if (tempSensValid)
      {
        // smooth measured temp * 10 (1 digit accuracy)
        smooth.setValue(tempSens * 10);

        if (smooth.isValid())
        {
          tempSmooth = smooth.getValue();
          tempSmoothValid = true;
        }
      }
      else
      {
        ESP_LOGE(LOG_TAG,"Invbalid temperature measurement");
      }
    }

    // Always send temperature + valid-flag to controller-queue
    qControllerMesg.type = e_mtype_sensor;
    qControllerMesg.mesg.sensorMesg.mesgId = e_msg_sensor_temperature;
    qControllerMesg.mesg.sensorMesg.data = tempSmooth;
    qControllerMesg.valid = (tempSensValid & tempSmoothValid); // both valids must be true !
    controllerQueueSend(&qControllerMesg, 0);

    // vTaskDelay(DELAY / portTICK_PERIOD_MS);
    vTaskDelay(DELAY / portTICK_PERIOD_MS);
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
  int r;

  ESP_LOGI(LOG_TAG, "init");

  // scanI2Cbus();

  sensorsQueue = xQueueCreate(5, sizeof(uint8_t));

  if (sensorsQueue == 0)
  {
    ESP_LOGE(LOG_TAG, "Cannot create sensorsQueue");
  }

  // create task
  r = xTaskCreatePinnedToCore(sensorsTask, "sensorsTask", 2 * 1024, NULL, 10, &sensorsTaskHandle, 1);

  if (r != pdPASS)
  {
    ESP_LOGE(LOG_TAG, "Could not create task, error-code=%d", r);
  }
}

// end of file