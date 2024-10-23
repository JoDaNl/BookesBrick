//
// comms.cpp
//

// Module uses HTTPClient

#include "config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#if (CFG_DISPLAY_TIME == true)
#include <ESP32Time.h>
#endif
#include "controller.h"
#include "comms.h"
#if (CFG_HYDRO_ENABLE == true)
#include "hydrobrick.h"
#endif

#define LOG_TAG "COMMS"

// GLOBALS
static QueueHandle_t communicationQueue = NULL;

// LOCALS
static TaskHandle_t communicationTaskHandle = NULL;

// JSON
static DynamicJsonDocument jsonResponseDoc(1024);
static DynamicJsonDocument PROAPIFilterDoc(80);
static JsonArray devices;

static const int16_t urlBufSize = 300;
static char URL[urlBufSize];

// NETWORK
static HTTPClient http;
// static WiFiClientSecure client;
static WiFiClient client;

// Get MAC addres as unique ID for BB URL
static const uint64_t chipId = ESP.getEfuseMac();

//
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
// WIFI functions
// ============================================================================

static void startWiFi()
{
  if (config.SSID.length() > 1 && config.passwd.length() > 1)
  {
    ESP_LOGI(LOG_TAG, "Connecting to WIFI");
    ESP_LOGI(LOG_TAG, "  hostname=%s", config.hostname.c_str());
    ESP_LOGI(LOG_TAG, "  SSID=%s", config.SSID.c_str());
    ESP_LOGI(LOG_TAG, "  passwd=%s", config.passwd.c_str());

    // Start WIFI
    // Important : set modem sleep mode ON. This allows concurrent use of BLE
    //             If not than the system crashes !
    WiFi.setHostname(config.hostname.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(true);
    WiFi.begin(config.SSID, config.passwd);
  }
  else
  {
    ESP_LOGE(LOG_TAG, "NO VALID WIFI CONFIGURATION");
  }

}

// ============================================================================
// CALL IOT API: SEND TEMPERATURE TO BACKEND AND GET NEW ACTUATOR VALUES
// send actuator & next poll interval to controller queue
// ============================================================================
static void callBierBotIOTAPI(uint16_t temperature)
{
  uint8_t updatedActuator;
  int getStatus;
  DeserializationError deserialisationError;
  uint32_t nextTempReqMs;
  controllerQItem_t controllerMesg;
  bool validResponse;

  const char key_epower_0_state[] = "epower_0_state";
  const char key_epower_1_state[] = "epower_1_state";
  const char key_next_request_ms[] = "next_request_ms";
  const char key_used_for_devices[] = "used_for_devices";

  // Default: assume we cannot receive actuator information
  // Note that when a brick is not part of a device we will also receive no actuator information
  actuatorsValid = false;
  nextTempReqMs = 0;
  
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

  ESP_LOGI(LOG_TAG, "API-url=%s", URL);

  validResponse = false;

  // Use http 1.0 for streaming. See : https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/
  http.useHTTP10(true);
  http.begin(URL);

  // See : https://www.arduino.cc/reference/en/libraries/wifi/client.connect/
  // http.begin(client, "brewbricks.com", 80, "/api/iot/v1?apikey=2vOfOwLP4szFrQTo1qLU&type=bookesbrick&brand=bierbot&version=0.2&chipid=1cf20ab865e4&s_number_temp_0=0.0&s_number_temp_id_0=0&a_bool_epower_0=0&a_bool_epower_1=0", false);
  // http.begin(client, "http://brewbricks.com/api/iot/v1?apikey=2vOfOwLP4szFrQTo1qLU&type=bookesbrick&brand=bierbot&version=0.2&chipid=1cf20ab865e4&s_number_temp_0=0.0&s_number_temp_id_0=0&a_bool_epower_0=0&a_bool_epower_1=0");
  // http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  getStatus = http.GET();

  if (getStatus > 0)
  {
    // Parse JSON object
    deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());

    if (deserialisationError)
    {
      ESP_LOGE(LOG_TAG, "API deserializeJson() failed: %s", deserialisationError.f_str());
      ESP_LOGE(LOG_TAG, "URL: %s", URL);
    }
    else
    {
      validResponse = true;
    }
  }
  else
  {
    ESP_LOGE(LOG_TAG, "No valid response from back-end. getResponse=%d (%s)", getStatus, http.errorToString(getStatus).c_str());
  }

  http.end();

  ESP_LOGI(LOG_TAG, "validResponse=%d", validResponse);

  if (validResponse)
  {
    // default, only set actuators when correct message has been received
    actuators = 0;

    // get actuator values
    if (jsonResponseDoc.containsKey(key_epower_0_state) && jsonResponseDoc.containsKey(key_epower_1_state))
    {
      updatedActuator = jsonResponseDoc[key_epower_0_state].as<uint8_t>();
      actuators = actuators | (updatedActuator & 1);
      ESP_LOGI(LOG_TAG, "Set relay 0 to : %d", updatedActuator);

      updatedActuator = jsonResponseDoc[key_epower_1_state].as<uint8_t>();
      actuators = actuators | (updatedActuator & 1) << 1;
      ESP_LOGI(LOG_TAG, "Set relay 1 to  : %d", updatedActuator);

      actuatorsValid = true;
    }
    else
    {
      ESP_LOGE(LOG_TAG, "epower_N_states not received");
    }

    // get next poll interval
    if (jsonResponseDoc.containsKey(key_next_request_ms))
    {
      nextTempReqMs = jsonResponseDoc[key_next_request_ms].as<long>();
    }

    // get 'usedfordevices' which is needed in te PROAPI call
    if (jsonResponseDoc.containsKey(key_used_for_devices))
    {
      // 'used_for_devices' is an array, but we assume here that the bookesbrick is
      //  only used within 1 device. Hence we only use the first element of the array
      devices = jsonResponseDoc[key_used_for_devices].as<JsonArray>();

      if (devices.size() > 0)
      {
        strncpy(usedForDevicesValue, devices[0], 32);
        ESP_LOGI(LOG_TAG, "usedForDevices=%s", usedForDevicesValue);
        usedForDevicesValid = true;
      }
    }
    else
    {
      usedForDevicesValid = false;
      ESP_LOGI(LOG_TAG, "used_for_devices not received");
    }

  } // extract values

  // Send actuator info to controller
  controllerMesg.type = e_mtype_backend;
  controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_actuators;
  controllerMesg.mesg.backendMesg.data16 = actuators;
  controllerMesg.mesg.backendMesg.valid = actuatorsValid;
  controllerQueueSend(&controllerMesg, 0);

  // Send next request time to controller
  controllerMesg.type = e_mtype_backend;
  controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_next_IOTAPIcall_ms;
  controllerMesg.mesg.backendMesg.data32 = nextTempReqMs;
  controllerMesg.mesg.backendMesg.valid = actuatorsValid;
  controllerQueueSend(&controllerMesg, 0);
}

static void initPROAPIFilterDocs(void)
{
  // Use a JSON filter document, see : https://arduinojson.org/v6/how-to/deserialize-a-very-large-document/

  // IMPORTANT : in case the PROAPIFilterDoc gets extended please check the size of the object (in static declaration above in file)
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
#if (CFG_DISPLAY_NONE == false)
static void callBierBotPROAPI(void)
{
  // static HTTPClient http; // TEST (as it was static declared in the module)
  int getResponse;
  DeserializationError deserialisationError;
  uint16_t setPointValue;
  bool setPointValid;
  String deviceName;
  String *deviceNameQPtr;
  controllerQItem_t controllerMesg;
  bool validResponse;

  setPointValue = 0;
  setPointValid = false;
  deviceNameQPtr = NULL; // We do not have an explicit valid flag for this pointer, as we can set it to NULL

  if (usedForDevicesValid)
  {
    snprintf(URL, urlBufSize, "%s%s?apikey=%s&proapikey=%s&deviceid=%s", CFG_COMM_BBURL_API_BASE, CFG_COMM_BBURL_PROAPI_DEVICE, config.apiKey.c_str(), config.proApiKey.c_str(), usedForDevicesValue);
    ESP_LOGI(LOG_TAG, "PRO API-URL=%s", URL);

    validResponse = false;

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
      }
      else
      {
        validResponse = true;
      }
    }
    else
    {
      ESP_LOGE(LOG_TAG, "PROAPI Received NO VALID RESPONSE: %d", getResponse);
    }

    http.end();

    if (validResponse)
    {
      // Extract values

      setPointValue = jsonResponseDoc["targetState"]["tempCelsius"].as<float>() * 10.0;
      setPointValid = jsonResponseDoc["active"].as<bool>();
      deviceName = jsonResponseDoc["name"].as<String>();

      ESP_LOGI(LOG_TAG, "setPointValue=%0.1f", setPointValue / 10.0);
      ESP_LOGI(LOG_TAG, "setPointValid=%d", setPointValid);
      ESP_LOGI(LOG_TAG, "deviceName=%d", deviceName.c_str());

      if (jsonResponseDoc.containsKey("name"))
      {
        // first time to receive name OR name has changed
        if ((deviceNameQPtr == NULL) || (deviceNameQPtr->equals(deviceName)))
        {
          deviceNameQPtr = new String(deviceName);
        }
      }
    }
  }
  else
  {
    ESP_LOGI(LOG_TAG, "Skipping PRO API call as we have no valid device-id yet");
  }

  controllerMesg.type = e_mtype_backend;
  controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_temp_setpoint;
  controllerMesg.mesg.backendMesg.data16 = setPointValue;
  controllerMesg.mesg.backendMesg.valid = setPointValid;
  controllerQueueSend(&controllerMesg, 0);

  if (deviceNameQPtr != NULL)
  {
    controllerMesg.type = e_mtype_backend;
    controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_device_name;
    controllerMesg.mesg.backendMesg.stringPtr = deviceNameQPtr;
    controllerMesg.mesg.backendMesg.valid = true;
    controllerQueueSend(&controllerMesg, 0);
  }

  // Send next request time to controller
  controllerMesg.type = e_mtype_backend;
  controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_next_PROAPIcall_ms;
  controllerMesg.mesg.backendMesg.data32 = 60 * 1000;
  controllerMesg.mesg.backendMesg.valid = true;
  controllerQueueSend(&controllerMesg, 0);  
}
#endif

#if (CFG_DISPLAY_TIME == true)

const char getExternIPURL[] = "https://api.ipify.org/?format=json";
const char keyExternalIP[] = "ip";

const char getTimeAPIURLBase[] = "https://timeapi.io/api/Time/current/ip?ipAddress=";
const char keyHour[] = "hour";
const char keyMinute[] = "minute";
const char keyMilliSec[] = "milliSeconds";
const char keySeconds[] = "seconds";
const char keyDay[] = "day";
const char keyMonth[] = "month";
const char keyYear[] = "year";

static void getNetworkTime(void)
{
  const bool hours24Mode = true;
  // HTTPClient http; // TEST (as it was static declared in the module)

  int getStatus;
  // DeserializationError deserialisationError;

  int hour;
  int min;
  int sec;
  int day;
  int month;
  int year;

  const char *externalIPAdress = NULL; // NULL is unknown IP-address

  controllerQItem_t controllerMesg;

  ESP_LOGV(LOG_TAG, "getNetworkTime()");

  rtcValid = false;

#ifdef TEST123
  ESP_LOGI(LOG_TAG, "heap before 1st http.GET() : %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

  http.useHTTP10(true);
  http.setReuse(false);
  client.setInsecure();

  if (http.begin(client, getExternIPURL) == false)
  {
    ESP_LOGE(LOG_TAG, "http.begin() failed with URL: %s", getExternIPURL);
  }

  // ESP_LOGI(LOG_TAG, "http.connected()=%d", http.connected());

  getStatus = http.GET();

  if (getStatus == HTTP_CODE_OK)
  {

    //    stream = http.getStreamPtr();

    // Parse JSON object
    // deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());
    deserializeJson(jsonResponseDoc, http.getStream());

    // if (deserialisationError)
    // {
    //   ESP_LOGE(LOG_TAG, "API deserializeJson() failed: %s", deserialisationError.f_str());
    // }
    // else
    {
      // Extract values from server response
      if (jsonResponseDoc.containsKey(keyExternalIP))
      {
        externalIPAdress = jsonResponseDoc[keyExternalIP];
        ESP_LOGI(LOG_TAG, "external IP=%s", externalIPAdress);
      }
    }
  }
  else
  {
    ESP_LOGE(LOG_TAG, "HTTP.get(getExternIPURL) failed");
  }

  ESP_LOGI(LOG_TAG, "heap after 1st http.GET() : %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

  http.end();
  client.stop();
#endif

  // TEST
  externalIPAdress = "77.161.115.34"; // FIXME !!!

  if (externalIPAdress != NULL)
  {
    snprintf(URL, urlBufSize, "%s%s", getTimeAPIURLBase, externalIPAdress);

    ESP_LOGI(LOG_TAG, "heap before 2nd http.GET() : %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());

    http.useHTTP10(true);
    http.setReuse(false);
    // client.setInsecure();

    if (http.begin(URL) == false)
    {
      ESP_LOGE(LOG_TAG, "http.begin() failed with URL: %s", getExternIPURL);
    }
    else
    {
      getStatus = http.GET();

      if (getStatus == HTTP_CODE_OK)
      {
        // Parse JSON object
        // deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());
        deserializeJson(jsonResponseDoc, http.getStream());

        // if (deserialisationError)
        // {
        //   ESP_LOGE(LOG_TAG, "API deserializeJson() failed: %s", deserialisationError.f_str());
        // }
        // else
        {
          // Extract values from server response
          hour = jsonResponseDoc[keyHour].as<int>();
          min = jsonResponseDoc[keyMinute].as<int>();
          sec = jsonResponseDoc[keySeconds].as<int>();
          day = jsonResponseDoc[keyDay].as<int>();
          month = jsonResponseDoc[keyMonth].as<int>();
          year = jsonResponseDoc[keyYear].as<int>();

          ESP_LOGD(LOG_TAG, "TIME=%02d:%02d:%02d", hour, min, sec);
          ESP_LOGD(LOG_TAG, "DATE=%4d:%02d:%02d", year, month, day);

          rtc.setTime(sec, min, hour, day, month, year, 0);
          rtcValid = true;
        }
      }
      else
      {
        ESP_LOGE(LOG_TAG, "HTTP.get(getTimeAPIURLBase) failed: %d", getStatus);
      }
    }

    http.end();
    client.stop();
  }

  controllerMesg.type = e_mtype_backend;
  controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_time_update;
  controllerMesg.valid = rtcValid;

  if (rtcValid)
  {
    controllerMesg.mesg.controlMesg.data = ((rtc.getHour(hours24Mode) & 0xFF) << 8) | (rtc.getMinute() & 0xFF);
  }
  else
  {
    controllerMesg.mesg.controlMesg.data = 0;
  }
  controllerQueueSend(&controllerMesg, 0);

  return;
}

#endif

// ============================================================================
// COMMUNICATION TASK
// ============================================================================

static void communicationTask(void *arg)
{
  static uint16_t r;
  static commsQueueItem_t message;
  static hydroQueueItem_t hydroMessage;

  printf("Heap Size (initWiFi 4): %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());

  // TASK LOOP
  while (true)
  {
    // Receive message from queue
    r = xQueueReceive(communicationQueue, &message, 100 / portTICK_PERIOD_MS);

    if (r == pdTRUE)
    {
      switch (message.type)
      {
      case e_type_comms_iotapi:
        callBierBotIOTAPI(message.temperature_x10);
        break;
      case e_type_comms_proapi:
        callBierBotPROAPI();
        break;
#if (CFG_HYDRO_ENABLE == true)
      case e_type_comms_hydrobrick:
        break;
        hydroMessage.mesgId = e_msg_hydro_cmd_get_reading;
        hydroQueueSend(&hydroMessage, 0);
        break;
#endif
      case e_type_comms_ntp:
        getNetworkTime();
        break;
      }

#if (CFG_DISPLAY_NONE == false)
      //  only call PRO API if 'usedForDevices' is valid

#if (CFG_DISPLAY_TIME == true)
//     nextTimeRequestSec = getNetworkTime();
#endif // CFG_DISPLAY_TIME
#endif // CFG_DISPLAY_NONE
    }
  }
}

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
  int r;

  ESP_LOGI(LOG_TAG, "initCommunication");
  printf("Heap Size (initCommunication 1): %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

  // SETUP JSON FILTER DOC
  initPROAPIFilterDocs();

#if (CFG_HYDRO_ENABLE == true)
  ESP_LOGI(LOG_TAG, "init HydroBrick");
  initHydroBrick();
#endif 

  communicationQueue = xQueueCreate(4, sizeof(commsQueueItem_t));
  if (communicationQueue == 0)
  {
    ESP_LOGE(LOG_TAG, "Cannot create communicationQueue. This is FATAL");
  }

  printf("Heap Size (initCommunication 2): %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

  // create task
  r = xTaskCreatePinnedToCore(communicationTask, "communicationTask", /* 10 */ 10 * 1024, NULL, 16, &communicationTaskHandle, /* 1 */ 0); // was 10 * 1024

  if (r != pdPASS)
  {
    ESP_LOGE(LOG_TAG, "could not create task, error-code=%d", r);
  }

  startWiFi();

  printf("Heap Size (initCommunication 3): %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
}

// end of file
