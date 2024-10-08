//
// comms.cpp
//

// Module uses HTTPClient 

#include "config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#if (CFG_DISPLAY_TIME == true)
#include <ESP32Time.h>
#endif
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
static DynamicJsonDocument jsonResponseDoc(1024);
static DynamicJsonDocument PROAPIFilterDoc(80);
static JsonArray devices;
const int16_t urlBufSize = 300;
static char URL[urlBufSize];

// NETWORK
static HTTPClient http;
static WiFiClient client;

static char usedForDevicesValue[32];
static bool usedForDevicesValid;

// ACTUATORS
static uint8_t actuators = 0;
static bool actuatorsValid;

// TIME / RTC
#if (CFG_DISPLAY_TIME == true)
static ESP32Time rtc;
static bool rtcValid;
#endif

// ============================================================================
// CALL IOT API: SEND TEMPERATURE TO BACKEND AND GET NEW ACTUATOR VALUES
// - return value is next temperature request (in sec)
// ============================================================================
static uint16_t callBierBotIOTAPI(uint16_t temperature)
{
  // static HTTPClient http;
  static uint8_t updatedActuator;
  static uint64_t chipId;
  static int getStatus;
  static DeserializationError deserialisationError;
  static uint32_t nextTempReqMs;
  static uint16_t nextTempRequestSecValue;
  static controllerQItem_t controllerMesg;
  static bool validResponse;
  static bool status;

  const char *key_epower_0_state = "epower_0_state";
  const char *key_epower_1_state = "epower_1_state";
  const char *key_next_request_ms = "next_request_ms";
  const char *key_used_for_devices = "used_for_devices";

  // Wait-time is ZERO in case we get no valid response from the backend
  nextTempRequestSecValue = 0;

  // Default: assume we cannot receive actuator information
  // Note that when a brick in not part of a device we will also receive no actuator information
  actuatorsValid = false;

  //printf("[JOS] callBierBotIOTAPI  temp=%f\n", temperature/10.0);

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
           0, // NOTE: sensor-id is harcoded to 0, update in case of extending to multiple sensors
           (actuators & 1),
           ((actuators >> 1) & 1));

  ESP_LOGI(LOG_TAG, "---------------------------");
  ESP_LOGI(LOG_TAG, "API-url=%s", URL);

  validResponse = false;

  // Use http 1.0 for streaming. See : https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/
  printf("[JOS] calling callBierBotIOTAPI - 2 - URL=\n%s\n",URL);
  http.useHTTP10(true);

  // http.setReuse(true);
  // http.setTimeout(10);
  printf("[JOS] calling callBierBotIOTAPI - 3\n");
  status = http.begin(URL);

  printf("[JOS] status of http.begin() is:%d\n",status);

// See : https://www.arduino.cc/reference/en/libraries/wifi/client.connect/

  // http.begin(client, "brewbricks.com", 80, "/api/iot/v1?apikey=2vOfOwLP4szFrQTo1qLU&type=bookesbrick&brand=bierbot&version=0.2&chipid=1cf20ab865e4&s_number_temp_0=0.0&s_number_temp_id_0=0&a_bool_epower_0=0&a_bool_epower_1=0", false);
  // http.begin(client, "http://brewbricks.com/api/iot/v1?apikey=2vOfOwLP4szFrQTo1qLU&type=bookesbrick&brand=bierbot&version=0.2&chipid=1cf20ab865e4&s_number_temp_0=0.0&s_number_temp_id_0=0&a_bool_epower_0=0&a_bool_epower_1=0");
  // http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  printf("[JOS] calling callBierBotIOTAPI - 4\n");
  getStatus = http.GET();
  printf("[JOS] calling callBierBotIOTAPI - 5\n");

  printf("HTTP GET Heap Size: %d, free: %d\n",ESP.getHeapSize(),ESP.getFreeHeap());

  if (getStatus > 0)
  {
    // Parse JSON object
    deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());

    if (deserialisationError)
    {
      printf("[COMM] API deserializeJson() failed: %s\n", deserialisationError.f_str());
    }
    else
    {
      validResponse = true;
    }
  }
  else
  {
    printf("[COMM] No valid response from back-end. getResponse=%d (%s)\n", getStatus, http.errorToString(getStatus).c_str());
  }

  http.end();

  printf("[COMM] validResponse=%d\n", validResponse);

  if (validResponse)
  {
    // Extract values from server response
    // printf("[COMM] Response      : %ld\n", responseCount++);
    // printf("[COMM] error         : %d - %s\n", jsonResponseDoc["error"].as<int>(), jsonResponseDoc["error_text"].as<const char *>());
    // printf("[COMM] warning       : %d - %s\n", jsonResponseDoc["warning"].as<int>(), jsonResponseDoc["warning_text"].as<const char *>());
    // printf("[COMM] nextTempReqMs : %ld\n", jsonResponseDoc["next_request_ms"].as<long>());

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
      printf("[COMM] set relay 0 to  : %d\n", updatedActuator);

      updatedActuator = jsonResponseDoc[key_epower_1_state].as<uint8_t>();
      actuators = actuators | (updatedActuator & 1) << 1;
      printf("[COMM] set relay 1 to  : %d\n", updatedActuator);

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

  ESP_LOGI(LOG_TAG, "---------------------------");

  return nextTempRequestSecValue;
}

static void initPROAPIFilterDocs(void)
{
  // Use a JSON filter document, see : https://arduinojson.org/v6/how-to/deserialize-a-very-large-document/

  // IMPORTANT : in case the PROAPIFilterDoc gets extended please check the size of the object (in static declaration above in file)
  // veryfy by uncommentinfg the print of the jsondoc below !

  PROAPIFilterDoc["name"] = true;
  PROAPIFilterDoc["type"] = true;
  PROAPIFilterDoc["targetState"]["tempCelsius"] = true;
  PROAPIFilterDoc["active"] = true;
  // PROAPIFilterDoc["targetState"]["tempFahrenheid"] = true; // Not tested

  // printf("PROAPIFilterDoc=\m");
  // serializeJsonPretty(PROAPIFilterDoc, Serial);
  // printf("\n");
}

// ============================================================================
// QUERY PRO-API TO GET THE TEMPERATURE SET_POINT OF A ACTIVE BREW
// - return value is next API-request (in sec)
// ============================================================================

#if (CFG_DISPLAY_NONE == false)
static uint16_t callBierBotPROAPI(void)
{
  // static HTTPClient http; // TEST (as it was static declared in the module)
  static int getResponse;
  static DeserializationError deserialisationError;
  static uint16_t setPointValue;
  static bool setPointValid;
  static String deviceName;
  static String *deviceNameQPtr;
  static controllerQItem_t controllerMesg;
  static bool validResponse;

  setPointValid = false;
  deviceNameQPtr = NULL; // We do not have an explicit valid flag for this pointer, as we can set it to NULL

  if (usedForDevicesValid)
  {
    snprintf(URL, urlBufSize, "%s%s?apikey=%s&proapikey=%s&deviceid=%s", CFG_COMM_BBURL_API_BASE, CFG_COMM_BBURL_PRO_API_DEVICE, config.apiKey.c_str(), config.proApiKey.c_str(), usedForDevicesValue);
    ESP_LOGI(LOG_TAG, "PRO API-URL=%s\n", URL);

    validResponse = false;

    printf("[JOS] calling callBierBotPROAPI - 2 /  URL=\n%s\n",URL);

    // Use http 1.0 for streaming. See : https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/

    http.useHTTP10(true);
    http.begin(URL);
    getResponse = http.GET();

    if (getResponse > 0)
    {
      deserialisationError = deserializeJson(jsonResponseDoc, http.getStream(), DeserializationOption::Filter(PROAPIFilterDoc));

      if (deserialisationError)
      {
        ESP_LOGE(LOG_TAG, "PROAPI deserializeJson() failed: %s", deserialisationError.f_str());
        printf("COMMS] PROAPI deserializeJson() failed: %s\n", deserialisationError.f_str());
      }
      else
      {
        validResponse = true;
      }
    }
    else
    {
      ESP_LOGE(LOG_TAG, "PROAPI Received NO VALID RESPONSE: %d", getResponse);
      printf("[COMMS] PROAPI Received NO VALID RESPONSE: %d", getResponse);
    }

    http.end();

    printf("[JOS] calling callBierBotPROAPI - 3 - validResponse=%d\n",validResponse);

    if (validResponse)
    {
      // Extract values

      setPointValue = jsonResponseDoc["targetState"]["tempCelsius"].as<float>() * 10.0;   
      setPointValid = jsonResponseDoc["active"].as<bool>();
      deviceName = jsonResponseDoc["name"].as<String>();

      printf("[COMMS] setPointValue=%0.1f\n",setPointValue/10.0);
      printf("[COMMS] setPointValid=%d\n",setPointValid);
      printf("[COMMS] deviceName=%d\n",deviceName.c_str());

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
#endif

#if (CFG_DISPLAY_TIME == true)
static uint16_t getNetworkTime(void)
{
  // HTTPClient http; // TEST (as it was static declared in the module)

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
          printf("[COMM] EXTERNAL IP=%s\n", externalIPAdress);
        }
      }
    }

    http.end();
  }

  if (externalIPAdress != NULL)
  {
    snprintf(URL, urlBufSize, "%s%s", getTimeAPIURLBase, externalIPAdress);

    printf("[COMM] timeapu URL=%s\n", URL);

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
        hour = jsonResponseDoc[keyHour].as<int>();
        min = jsonResponseDoc[keyMinute].as<int>();
        sec = jsonResponseDoc[keySeconds].as<int>();
        day = jsonResponseDoc[keyDay].as<int>();
        month = jsonResponseDoc[keyMonth].as<int>();
        year = jsonResponseDoc[keyYear].as<int>();

        printf("[COMM]  TIME=%02d:%02d:%02d\n", hour, min, sec);
        printf("[COMM]  DATE=%4d:%02d:%02d\n", year, month, day);

        rtc.setTime(sec, min, hour, day, month, year, 0);
        rtcValid = true;
      }
    }

    http.end();
  }

  interval = rtcValid ? CFG_COMM_TIMEREQUEST_INTERVAL : 60;

  return (interval);
}
#endif

// ============================================================================
// COMMUNICATION TASK
// ============================================================================

static void communicationTask(void *arg)
{
  static commsQueueItem_t message;
  static uint16_t nextAPIRequestSecMax;
  static uint16_t nextAPIRequestSec;
  static bool nextAPIRequestValid;
  static uint16_t nextPROAPIRequestSec;
  static uint16_t nextTimeRequestSec;
  static bool temperatureValid = false;
  static int16_t temperature; // TODO : support for multiple temperatures

  // CONTROLLER MESSAGE
  static controllerQItem controllerMesg;

  // JSON SETUP FILTER DOC
  initPROAPIFilterDocs();

  // API-request times in seconds
  nextAPIRequestSecMax = 1;
  nextAPIRequestSec = nextAPIRequestSecMax;
  nextAPIRequestValid = false;
  nextPROAPIRequestSec = 0; // will call the API as soon as 'usedForDevices' is known
  nextTimeRequestSec = 0;

  // TASK LOOP

  while (true)
  {
    // Print memmory usage
    // printf("[COMM] Free memory: %d bytes, watermark: %d bytes\n", esp_get_free_heap_size(), uxTaskGetStackHighWaterMark(NULL));

    // Receive message from queue /  don't wait for message
    if (xQueueReceive(communicationQueue, &message, 0) == pdTRUE)
    {
      temperatureValid = message.valid;
      temperature = message.temperature;
    }

    if (temperatureValid)
    {
      // CALL BIERBOT IOT-API
      if (nextAPIRequestSec > 0)
      {
        nextAPIRequestSec--;
      }
      else
      {
        printf("[JOS] calling callBierBotIOTAPI - 1\n");
        nextAPIRequestSecMax = callBierBotIOTAPI(temperature);
        // just limit max to 255 (cannot send larger nr's to display)
        if (nextAPIRequestSecMax > 255)
        {
          nextAPIRequestSecMax = 255;
        }

        nextAPIRequestValid = true;

        if (nextAPIRequestSecMax == 0)
        {
          nextAPIRequestSecMax = 60;
          nextAPIRequestValid = false;
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

#if (CFG_DISPLAY_NONE == false)
    //  only call PRO API if 'usedForDevices' is valid
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
        printf("[JOS] calling callBierBotPROAPI - 1\n");
        nextPROAPIRequestSec = callBierBotPROAPI();
      }
    }

#if (CFG_DISPLAY_TIME == true)
    if (WiFi.status() == WL_CONNECTED)  // FIXME : why ths check?
    {
      if (nextTimeRequestSec > 0)
      {
        // count down to 0
        nextTimeRequestSec--;
      }
      else
      {
        // get time from the internet
        //        nextTimeRequestSec = getNetworkTime();   NU EVEN NIET !!!
        nextTimeRequestSec = 100;
      }
    }

    const bool hours24Mode = true;

    controllerMesg.type = e_mtype_time;
    controllerMesg.mesg.timeMesg.mesgId = e_msg_time_data;
    controllerMesg.valid = rtcValid;
    controllerMesg.mesg.timeMesg.data = ((rtc.getHour(hours24Mode) & 0xFF) << 8) | (rtc.getMinute() & 0xFF);
    controllerQueueSend(&controllerMesg, 0);
#endif // CFG_DISPLAY_TIME
#endif // CFG_DISPLAY_NONE

    // TODO : improve heartbeat...i.e. if backend silences the controller should know...maybe 2 or 3 'invalid' heartbeat messages would work
    controllerMesg.type = e_mtype_backend;
    controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_heartbeat;
    controllerMesg.mesg.backendMesg.data = (nextAPIRequestSecMax << 8) | nextAPIRequestSec;
    controllerMesg.mesg.backendMesg.valid = nextAPIRequestValid;
    controllerQueueSend(&controllerMesg, 0);

    // loop delay
    vTaskDelay(DELAY_1S / portTICK_RATE_MS);
  }
};

int communicationQueueSend(commsQueueItem_t *queueItem, TickType_t xTicksToWait)
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

  printf("Initial Heap Size: %d, free: %d\n",ESP.getHeapSize(),ESP.getFreeHeap());

  communicationQueue = xQueueCreate(4, sizeof(commsQueueItem_t));
  if (communicationQueue == 0)
  {
    ESP_LOGE(LOG_TAG, "Cannot create communicationQueue. This is FATAL\n");
  }

  // create task
  xTaskCreatePinnedToCore(communicationTask, "communicationTask", 10 * 1024, NULL, 10, &communicationTaskHandle, 1);

  printf("After create task Heap Size: %d, free: %d\n",ESP.getHeapSize(),ESP.getFreeHeap());

}

// end of file
