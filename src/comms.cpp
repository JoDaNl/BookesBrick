//
// comms.cpp
//

// Module uses HTTPClient

#include "config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
//#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <WiFiClient.h>

#if (CFG_DISPLAY_TIME == true)
#include <ESP32Time.h>
#endif
#include <preferences.h>
#include "controller.h"
#include "comms.h"

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
  bool status;

  if (config.SSID.length() > 1 && config.passwd.length() > 1)
  {
    ESP_LOGI(LOG_TAG, "Connecting to WIFI");
    ESP_LOGI(LOG_TAG, "  hostname=%s", config.hostname.c_str());
    ESP_LOGI(LOG_TAG, "  SSID=%s", config.SSID.c_str());
    ESP_LOGI(LOG_TAG, "  passwd=%s", config.passwd.c_str());

    WiFi.setHostname(config.hostname.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.begin(config.SSID, config.passwd);
  }
  else
  {
    ESP_LOGE(LOG_TAG, "NO VALID WIFI CONFIGURATION");
  }
}

static void stopWiFi(void)
{
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
  bool status;

  const char key_epower_0_state[] = "epower_0_state";
  const char key_epower_1_state[] = "epower_1_state";
  const char key_next_request_ms[] = "next_request_ms";
  const char key_used_for_devices[] = "used_for_devices";

  // Default: assume we cannot receive actuator information
  // Note that when a brick is not part of a device we will also receive no actuator information
  actuatorsValid = false;

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
  http.useHTTP10(true);
  status = http.begin(URL);

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
      ESP_LOGI(LOG_TAG, "Set relay 1 to  : %dn", updatedActuator);

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
    else
    {
      nextTempReqMs = 60 * 1000; // one minute
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
  controllerMesg.mesg.backendMesg.data = actuators;
  controllerMesg.mesg.backendMesg.nextRequestInterval = nextTempReqMs;
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
  controllerMesg.mesg.backendMesg.data = setPointValue;
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
  externalIPAdress = "77.161.115.34";
  char URL2[] = "https://timeapi.io/api/Time/current/ip?ipAddress=77.161.115.34";

  if (externalIPAdress != NULL)
  {
    snprintf(URL, urlBufSize, "%s%s", getTimeAPIURLBase, externalIPAdress);

    ESP_LOGI(LOG_TAG, "heap before 2nd http.GET() : %d, free: %d", ESP.getHeapSize(), ESP.getFreeHeap());

    ESP_LOGI(LOG_TAG, "--> 1");
    http.useHTTP10(true);
    ESP_LOGI(LOG_TAG, "--> 2");
    http.setReuse(false);
    ESP_LOGI(LOG_TAG, "--> 3");
    //    client.setInsecure();
    ESP_LOGI(LOG_TAG, "--> 4");

    ESP_LOGI(LOG_TAG, "http.begin(), URL: %s", URL);

    if (http.begin(URL) == false)
    {
      ESP_LOGE(LOG_TAG, "http.begin() failed with URL: %s", getExternIPURL);
    }
    else
    {
      ESP_LOGI(LOG_TAG, "--> 5");
      getStatus = http.GET();
      ESP_LOGI(LOG_TAG, "--> 6");

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

#ifdef TWO

void readResponse(WiFiClientSecure *client)
{
  unsigned long timeout = millis();
  while (client->available() == 0)
  {
    if (millis() - timeout > 5000)
    {
      Serial.println(">>> Client Timeout !");
      client->stop();
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client->available())
  {
    String line = client->readStringUntil('\r');
    Serial.print(line);
  }

  Serial.printf("\nClosing connection\n\n");
}

static WiFiClientSecure client;

static void getNetworkTime2(void)
{
  const bool hours24Mode = true;
  // HTTPClient http; // TEST (as it was static declared in the module)

  const char *server = "api.ipify.org";
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

  int getStatus;
  DeserializationError deserialisationError;

  int hour;
  int min;
  int sec;
  int day;
  int month;
  int year;

  controllerQItem_t controllerMesg;

  // See : https://randomnerdtutorials.com/esp32-https-requests/
  // See : https://raw.githubusercontent.com/RuiSantosdotme/Random-Nerd-Tutorials/master/Projects/ESP32/ESP32_HTTPS/ESP32_WiFiClientSecure_No_Certificate/ESP32_WiFiClientSecure_No_Certificate.ino

  ESP_LOGV(LOG_TAG, "getNetworkTime2()");

  rtcValid = false;

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }

  ESP_LOGI(LOG_TAG, "heap before 1st http.GET() : %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

  client.setInsecure();

  if (!client.connect(server, 443))
  {
    ESP_LOGE(LOG_TAG, "client.connect() failed for server: %s", server);
  }
  else
  {
    ESP_LOGI(LOG_TAG, "client.connect() success for server : %s", server);

    // Make a HTTP request:
    client.println("GET /?format=json HTTP/1.0");
    client.println("Host: api.ipify.org");
    client.println("Connection: close");
    client.println();

    ESP_LOGI(LOG_TAG, "readResponse()");
    readResponse(&client);

    // while (client.connected())
    // {
    //   String line = client.readStringUntil('\n');
    //   if (line == "\r")
    //   {
    //     Serial.println("headers received");
    //     break;
    //   }
    // }

    ESP_LOGI(LOG_TAG, "parse stream");

    // Parse JSON object
    deserialisationError = deserializeJson(jsonResponseDoc, client);

    ESP_LOGI(LOG_TAG, "parsed stream");

    if (deserialisationError)
    {
      ESP_LOGE(LOG_TAG, "API deserializeJson() failed: %s", deserialisationError.f_str());
    }
    else
    {
      // Extract values from server response
      if (jsonResponseDoc.containsKey(keyExternalIP))
      {
        externalIPAdress = jsonResponseDoc[keyExternalIP];
        ESP_LOGI(LOG_TAG, "external IP=%s", externalIPAdress);
      }
    }
    // if there are incoming bytes available
    // from the server, read them and print them:
    // while (client.available())
    // {
    //   char c = client.read();
    //   Serial.write(c);
    // }
  }

  client.stop();

#ifdef blabla
  if (getStatus == HTTP_CODE_OK)
  {

    //    stream = http.getStreamPtr();

    // Parse JSON object
    deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());

    if (deserialisationError)
    {
      ESP_LOGE(LOG_TAG, "API deserializeJson() failed: %s", deserialisationError.f_str());
    }
    else
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

  if (externalIPAdress != NULL)
  {
    snprintf(URL, urlBufSize, "%s%s", getTimeAPIURLBase, externalIPAdress);

    ESP_LOGI(LOG_TAG, "heap before 2nd http.GET() : %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

    http.useHTTP10(true);
    http.begin(URL);
    getStatus = http.GET();
    ESP_LOGI(LOG_TAG, "GET 3");

    if (getStatus == HTTP_CODE_OK)
    {
      // Parse JSON object
      deserialisationError = deserializeJson(jsonResponseDoc, http.getStream());

      if (deserialisationError)
      {
        ESP_LOGE(LOG_TAG, "API deserializeJson() failed: %s", deserialisationError.f_str());
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

    http.end();
  }
#endif

  controllerMesg.type = e_mtype_backend;
  controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_time_updated;
  controllerMesg.valid = rtcValid;
  controllerMesg.mesg.controlMesg.data = ((rtc.getHour(hours24Mode) & 0xFF) << 8) | (rtc.getMinute() & 0xFF);
  controllerQueueSend(&controllerMesg, 0);

  return;
}

#endif

#endif

// ============================================================================
// COMMUNICATION TASK
// ============================================================================

static void communicationTask(void *arg)
{
  uint16_t r;
  commsQueueItem_t message;

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
        //        callBierBotPROAPI();
        break;
#if (CFG_ENABLE_HYDROBRICK == true)
      case e_type_comms_hydrobrick:
        break;
        // callHydroAPI(message.temperature_x10, message.SG_x1000, message.battteryLevel_x1000);
        break;
#endif
      case e_type_comms_ntp:
        // printf("[COMM] received e_type_comms_ntp\n");
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
