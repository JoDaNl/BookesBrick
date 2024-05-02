//
// comms.cpp
//

#include "config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Time.h>
#include <preferences.h>

#include "controller.h"
#include "comms.h"

#define LOG_TAG "COMMS"
#define DELAY_1S (1000)

// GLOBALS
static xQueueHandle communicationQueue = NULL;

// LOCALS
static TaskHandle_t communicationTaskHandle = NULL;

// JSON
static StaticJsonDocument<4096> jsonResponseDoc;
static StaticJsonDocument<256> PROAPIFilterDoc;
static JsonArray devices;
const int16_t urlBufSize = 300;
static char URL[urlBufSize];

// NETWORK
static HTTPClient http;

static char usedForDevicesValue[32];
static bool usedForDevicesValid;

// ACTUATORS
static uint8_t actuators = 0;
static bool actuatorsValid;

// TIME / RTC
static ESP32Time rtc; 
static bool rtcValid;

// ============================================================================
// CALL IOT API: SEND TEMPERATURE TO BACKEND AND GET NEW ACTUATOR VALUES
// - return value is next temperature request (in sec)
// ============================================================================
static uint16_t callBricksAPI(uint16_t temperature)
{
  static uint8_t updatedActuator;
  static uint64_t chipId;
  static int getStatus;
  static DeserializationError deserialisationError;
  static uint32_t nextTempReqMs;
  static uint16_t nextTempRequestSecValue;
  static bool nextTempRequestSecValid;
  static controllerQItem_t controllerMesg;

  const char *key_epower_0_state = "epower_0_state";
  const char *key_epower_1_state = "epower_1_state";
  const char *key_next_request_ms = "next_request_ms";
  const char *key_used_for_devices = "used_for_devices";

  // Default wait-time in case we get no valid response from the backend
  nextTempRequestSecValue = 60; // secs
  nextTempRequestSecValid = false;

  // Default: assume we cannot receive actuator information
  // Note that when a brick in not part of a device we will also receive no actuator information
  actuatorsValid = false;

  // Get MAC addres as unique ID for BB URL
  chipId = ESP.getEfuseMac();

  snprintf(URL, urlBufSize, "%s%s?apikey=%s&type=%s&brand=%s&version=%s&chipid=%6x%6x&s_number_temp_0=%2.1f&s_number_temp_id_0=%d&a_bool_epower_0=%d&a_bool_epower_1=%d",
           CFG_COMM_BBURL_API_BASE,
           CFG_COMM_BBURL_API_IOT,
           config.apiKey.c_str(),
           CFG_COMM_DEVICE_TYPE,
           CFG_COMM_DEVICE_BRAND,
           CFG_COMM_DEVICE_VERSION,
           (uint32_t)(chipId >> 24), // split in 2 parts as we cannot print a 64-bit integer
           (uint32_t)(chipId & 0x00FFFFFF),
           temperature / 10.0,
           0, // NOTE: sensor-id is harcoded to 0, update is case of extending to multiple sensors
           (actuators & 1),
           ((actuators >> 1) & 1));

  ESP_LOGI(LOG_TAG, "---------------------------");
  ESP_LOGI(LOG_TAG, "API-url=%s", URL);

  // Use http 1.0 for streaming. See : https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/
  http.useHTTP10(true);
  http.begin(URL);
  getStatus = http.GET();

  if (getStatus > 0)
  {
    // Parse JSON object
    deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());

    if (deserialisationError)
    {
      printf("[COMMS] API deserializeJson() failed: %s\n", deserialisationError.f_str());
    }
    else
    {
      // Extract values from server response

      // printf("[COMMS] Response      : %ld\n", responseCount++);
      // printf("[COMMS] error         : %d - %s\n", jsonResponseDoc["error"].as<int>(), jsonResponseDoc["error_text"].as<const char *>());
      // printf("[COMMS] warning       : %d - %s\n", jsonResponseDoc["warning"].as<int>(), jsonResponseDoc["warning_text"].as<const char *>());
      // printf("[COMMS] nextTempReqMs : %ld\n", jsonResponseDoc["next_request_ms"].as<long>());

      actuators = 0; // default, only set actuators when correct message has been received

      // if (jsonResponseDoc["error"])
      // {
      //   // check error string
      //   bbError = jsonResponseDoc["error_text"].as<String>();
      // }

      // if (jsonResponseDoc["warning"])
      // {
      //   // check warning string
      //   bbWarning = jsonResponseDoc["warning_text"].as<String>();
      // }

      if (jsonResponseDoc.containsKey(key_epower_0_state) && jsonResponseDoc.containsKey(key_epower_1_state))
      {
        updatedActuator = jsonResponseDoc[key_epower_0_state].as<uint8_t>();
        actuators = actuators | (updatedActuator & 1);
        printf("[COMMS] set relay 0 to  : %d\n", updatedActuator);

        updatedActuator = jsonResponseDoc[key_epower_1_state].as<uint8_t>();
        actuators = actuators | (updatedActuator & 1) << 1;
        printf("[COMMS] set relay 1 to  : %d\n", updatedActuator);

        actuatorsValid = true;
      }
      else
      {
        ESP_LOGE(LOG_TAG, "epower_N_states not received");
      }

      if (jsonResponseDoc.containsKey(key_next_request_ms))
      {
        nextTempReqMs = jsonResponseDoc[key_next_request_ms].as<long>();
        nextTempRequestSecValue = nextTempReqMs / 1000;
        nextTempRequestSecValid = true;
      }

      if (jsonResponseDoc.containsKey(key_used_for_devices))
      {
        // 'used_for_devices' is an array, but we assume here that the bookesbrick is
        //  only used within 1 device. Hence we only use the first element of the array
        devices = jsonResponseDoc[key_used_for_devices].as<JsonArray>();

        if (devices.size() > 0)
        {
          strncpy(usedForDevicesValue, devices[0], 32);
          ESP_LOGI(LOG_TAG, "usedForDevices=%s\n", usedForDevicesValue);
          usedForDevicesValid = true;
        }
      }
      else
      {
        usedForDevicesValid = false;
        ESP_LOGI(LOG_TAG, "used_for_devices not received");
      }

    } // extract values
  }
  else
  {
    printf("[COMMS] no valid response from back-end. getResponse=%d (%s)\n", getStatus, http.errorToString(getStatus).c_str());
  }

  http.end();

  ESP_LOGI(LOG_TAG, "---------------------------");

  return nextTempRequestSecValue;
}

static void initPROAPIFilterDocs(void)
{
  // Use a JSON filter document, see : https://arduinojson.org/v6/how-to/deserialize-a-very-large-document/

  PROAPIFilterDoc["name"] = true;
  PROAPIFilterDoc["type"] = true;
  PROAPIFilterDoc["targetState"]["tempCelsius"] = true;
  PROAPIFilterDoc["active"] = true;
  // PROAPIFilterDoc["targetState"]["tempFahrenheid"] = true; // Not tested
}

// ============================================================================
// QUERY PRO-API TO GET THE TEMPERATURE SET_POINT OF A ACTIVE BREW
// - return value is next API-request (in sec)
// ============================================================================

static uint16_t callBierBotPROAPI(void)
{
  static int getResponse;
  static DeserializationError deserialisationError;
  static uint16_t setPointValue;
  static bool setPointValid;
  static String deviceName;
  static String *deviceNameQPtr;
  static controllerQItem_t controllerMesg;

  setPointValid = false;
  deviceNameQPtr = NULL; // We do not have an explicit valid flag for this pointer, as we can set it to NULL

  if (usedForDevicesValid)
  {
    snprintf(URL, urlBufSize, "%s%s?apikey=%s&proapikey=%s&deviceid=%s", CFG_COMM_BBURL_API_BASE, CFG_COMM_BBURL_PRO_API_DEVICE, config.apiKey.c_str(), config.proApiKey.c_str(), usedForDevicesValue);
    ESP_LOGI(LOG_TAG, "PRO API-URL=%s\n", URL);

    // Use http 1.0 for streaming. See : https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/
    http.useHTTP10(true);
    http.begin(URL);
    getResponse = http.GET();

    if (getResponse > 0)
    {
      deserialisationError = deserializeJson(jsonResponseDoc, http.getStream(), DeserializationOption::Filter(PROAPIFilterDoc));

      if (deserialisationError)
      {
        ESP_LOGE(LOG_TAG, "PRO API deserializeJson() failed: %s", deserialisationError.f_str());
      }
      else
      {
        // Extract values
        setPointValue = jsonResponseDoc["targetState"]["tempCelsius"].as<float>() * 10.0;
        setPointValid = jsonResponseDoc["active"].as<bool>();
        deviceName = jsonResponseDoc["name"].as<String>();

        if (jsonResponseDoc.containsKey("name"))
        {
          // first time to receive name OR name has changed
          if ((deviceNameQPtr == NULL) || (deviceNameQPtr->equals(deviceName)))
          {
            deviceNameQPtr = new String(deviceName);

            controllerMesg.type = e_mtype_backend;
            controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_device_name;
            controllerMesg.mesg.backendMesg.stringPtr = deviceNameQPtr;
            controllerMesg.mesg.backendMesg.valid = true;
            controllerQueueSend(&controllerMesg, 0);
          }
        }
      }
    }
    else
    {
      ESP_LOGE(LOG_TAG, "PRO API Received NO VALID RESPONSE: %d", getResponse);
    }

    http.end();

    controllerMesg.type = e_mtype_backend;
    controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_temp_setpoint;
    controllerMesg.mesg.backendMesg.data = setPointValue;
    controllerMesg.mesg.backendMesg.valid = setPointValid;
    controllerQueueSend(&controllerMesg, 0);
  }
  else
  {
    ESP_LOGI(LOG_TAG, "Skipping PRO API call as we have no valid device-id yet");
  }

  return (CFG_COMM_PROAPI_INTERVAL);
}


static uint16_t getNetworkTime(void)
{
  const char *getExternIPURL = "https://api.ipify.org/?format=json";
  const char *keyExternalIP = "ip";
  const char *externalIPAdress = NULL; // NULL is unknown IP-address

  const char *getTimeAPIURLBase = "https://timeapi.io/api/Time/current/ip?ipAddress=";
  const char *keyHour = "hour";
  const char *keyMinute = "minute";
  const char *keyMilliSec = "milliSeconds";
  const char *keySeconds = "seconds";
  const char *keyDay = "day";
  const char *keyMonth = "month";
  const char *keyYear = "year";

  static int getStatus;
  static DeserializationError deserialisationError;

  static int hour;
  static int min;
  static int sec;
  static int day;
  static int month;
  static int year;

  static uint16_t interval;

  rtcValid = false;

  if (externalIPAdress == NULL)
  {
    http.useHTTP10(true);
    http.begin(getExternIPURL);
    getStatus = http.GET();

    if (getStatus > 0)
    {
      // Parse JSON object
      deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());

      if (deserialisationError)
      {
        ESP_LOGE(LOG_TAG, "API deserializeJson() failed: %s\n", deserialisationError.f_str());
      }
      else
      {
        // Extract values from server response
        if (jsonResponseDoc.containsKey(keyExternalIP))
        {
          externalIPAdress = jsonResponseDoc[keyExternalIP];
          printf("[COMMS] EXTERNAL IP=%s\n", externalIPAdress);
        }
      }
    }

    http.end();
  }


  if (externalIPAdress != NULL)
  {
    snprintf(URL, urlBufSize, "%s%s", getTimeAPIURLBase, externalIPAdress);

    printf("[COMMS] timeapu URL=%s\n", URL);

    http.useHTTP10(true);
    http.begin(URL);
    getStatus = http.GET();

    if (getStatus > 0)
    {
      // Parse JSON object
      deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());

      if (deserialisationError)
      {
        ESP_LOGE(LOG_TAG, "API deserializeJson() failed: %s\n", deserialisationError.f_str());
      }
      else
      {
        // Extract values from server response
        hour  = jsonResponseDoc[keyHour].as<int>();
        min   = jsonResponseDoc[keyMinute].as<int>();
        sec   = jsonResponseDoc[keySeconds].as<int>();
        day   = jsonResponseDoc[keyDay].as<int>();
        month = jsonResponseDoc[keyMonth].as<int>();
        year  = jsonResponseDoc[keyYear].as<int>();

        printf("[COMMS]  TIME=%02d:%02d:%02d\n", hour, min, sec);
        printf("[COMMS]  DATE=%4d:%02d:%02d\n", year, month, day);

        rtc.setTime(sec, min, hour, day, month, year, 0);
        rtcValid = true;
      }
    }

    http.end();
  }

  interval = rtcValid ? CFG_COMM_TIMEREQUEST_INTERVAL : 60;

  return (interval);
}

// ============================================================================
// COMMUNICATION TASK
// ============================================================================

static void communicationTask(void *arg)
{
  static bool status;
  static uint16_t nextAPIRequestSecMax;
  static uint16_t nextAPIRequestSec;
  static uint16_t nextPROAPIRequestSec;
  static uint16_t nextTimeRequestSec;
  static bool temperatureValid = false;
  static int16_t temperature; // TODO : support for multiple temperatures

  // CONTROLLER MESSAGE
  static controllerQItem controllerMesg;

  // JSON SETUP FILTER DOC
  initPROAPIFilterDocs();

  // API-request times in seconds
  nextAPIRequestSecMax  = 10;
  nextAPIRequestSec     = nextAPIRequestSecMax;
  nextPROAPIRequestSec  = 20;
  nextTimeRequestSec    = 5;

  // TASK LOOP

  while (true)
  {
    // Print memmory usage
    // printf("[COMMS] Free memory: %d bytes, watermark: %d bytes\n", esp_get_free_heap_size(), uxTaskGetStackHighWaterMark(NULL));

    // Receive message from queue
    if (xQueueReceive(communicationQueue, &temperature, 0) == pdTRUE)
    {     

      // CALL BIERBOT IOT-API
      if (nextAPIRequestSec > 0)
      {
        nextAPIRequestSec--;
      }
      else
      {
        nextAPIRequestSecMax = callBricksAPI(temperature);
        // just limit max to 255 (cannot send larger nr's to display)
        if (nextAPIRequestSecMax > 255)
        {
          nextAPIRequestSecMax = 255;
        }
        nextAPIRequestSec = nextAPIRequestSecMax;

        // Send actuator info to controller
        controllerMesg.type = e_mtype_backend;
        controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_actuators;
        controllerMesg.mesg.backendMesg.data = actuators;
        controllerMesg.mesg.backendMesg.valid = actuatorsValid;
        controllerQueueSend(&controllerMesg, 0);
      }
    }

    //  only call PRO API if 'udedForDevices' is valid
    if (usedForDevicesValid)
    {
      if (nextPROAPIRequestSec > 0)
      {
        // count down to 0
        nextPROAPIRequestSec--;
      }
      else
      {
        // Call to back-end
        nextPROAPIRequestSec = callBierBotPROAPI();
        // nextPROAPIRequestSec = 60;
      }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      if (nextTimeRequestSec > 0)
      {
        // count down to 0
        nextTimeRequestSec--;
      }
      else
      {
        // get time from the internet
        nextTimeRequestSec = getNetworkTime();
      }
    }

    const bool hours24Mode = true;

    controllerMesg.type = e_mtype_time;
    controllerMesg.mesg.timeMesg.mesgId = e_msg_time_data;
    controllerMesg.valid = rtcValid;
    controllerMesg.mesg.timeMesg.data = ((rtc.getHour(hours24Mode) & 0xFF) << 8) | (rtc.getMinute() & 0xFF);
    controllerQueueSend(&controllerMesg, 0);

    // printf("[COMMS] rtc.hour=%d, rtc.minute=%d\n", rtc.getHour(hours24Mode), rtc.getMinute());

    // TODO : improve heartbeat...i.e. if backend silences the controller should know
    controllerMesg.type = e_mtype_backend;
    controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_heartbeat;
    controllerMesg.mesg.backendMesg.data = (nextAPIRequestSecMax << 8) | nextAPIRequestSec;
    controllerMesg.mesg.backendMesg.valid = true;
    controllerQueueSend(&controllerMesg, 0);

    // loop delay
    vTaskDelay(DELAY_1S / portTICK_RATE_MS);
  }
};

int communicationQueueSend(int16_t *queueItem, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (communicationQueue != NULL)
  {
    r = xQueueSend(communicationQueue, queueItem, xTicksToWait);
  }

  return r;
}

// ============================================================================
// INIT COMMUNICATION
// ============================================================================

void initCommmunication(void)
{
  ESP_LOGI(LOG_TAG, "initCommunication");

  communicationQueue = xQueueCreate(4, sizeof(int16_t));
  if (communicationQueue == 0)
  {
    ESP_LOGE(LOG_TAG, "Cannot create communicationQueue. This is FATAL\n");
  }

  // create task
  xTaskCreate(communicationTask, "communicationTask", 10 * 1024, NULL, 10, &communicationTaskHandle);
}

// end of file
