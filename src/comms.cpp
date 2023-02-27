//
// comms.cpp
//

#include "config.h"
#include <Arduino.h>
// the following defines MUST be defined BEFORE the DRD include file!
#define USE_SPIFFS false
#define USE_LITTEFS false
#define ESP_DRD_USE_EEPROM true
#include <ESP_DoubleResetDetector.h>
#include <WifiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <preferences.h>
#include <ESP32ping.h>
#include "monitor.h"
#include "comms.h"
#include "i2c_lcd_16x2.h"
#include "actuators.h"

#define COMMS_WM_RESET false
#define COMMS_WM_DEBUG true

#define BBPREFS "bbbrick"
#define BBAPIKEYID "bbApiKey"
#define BBAPIKEYLABEL "BB API-key"
#define BBPORTALTIMEOUT 90
#define BBDRDTIMEOUT 5
#define BBPINGURL "8.8.8.8"

// GLOBALS
xQueueHandle communicationQueue = NULL;

// module scope
static WiFiManager wm;
static bool wmSaveConfig = false;
static DoubleResetDetector *drd;
static uint64_t chipId;
static String apiKey;

// ============================================================================
// WIFIMANAGER CALLBACK
// ============================================================================

static void saveWMConfigCallback()
{
  printf("[COMMS] WiFiManager callback : save config\n");
  wmSaveConfig = true;
}

// ============================================================================
// INIT WIFI
// ============================================================================

static void initWiFi(void)
{
  static bool status;
  static displayQueueItem_t displayQMesg;

  // PREFERENCES
  static Preferences prefs;

  // Message for display task
  displayQMesg.type = e_wifiInfo;
  displayQMesg.index = 0;
  displayQMesg.duration = 10; // seconds

  // Get MAC addres as unique ID for BB URL
  chipId = ESP.getEfuseMac();

  // ===========================================================
  // WIFIMANAGER
  // ===========================================================

  // Optionally reset WifiManager settings
#if (COMM_WM_RESET == true)
  printf("[COMMS] RESETTING ALL WIFIMANAGER SETTINGS!!!\n");
  wm.resetSettings();
#endif

  // Optionally silence WiFiManager...to chatty at startup
  wm.setDebugOutput(COMMS_WM_DEBUG);
#if (COMMS_WM_DEBUG == true)
  printf("[COMMS] WIFIMANAGER DEBUG INFO ON\n");
  wm.debugPlatformInfo();
  wm.debugSoftAPConfig();
#endif

  // read 'preferences'
  prefs.begin(BBPREFS, true);               // read only mode
  apiKey = prefs.getString(BBAPIKEYID, ""); // leave default empty
  printf("[COMMS] apiKey read from preferences=%s\n", apiKey.c_str());
  prefs.end();

  // ===========================================================
  // START WIFI
  // ===========================================================

  // Add custom "BierBot Bricks API Key" parameter to WiFiManager, max length = 32
  static WiFiManagerParameter bbApiKey(BBAPIKEYID, BBAPIKEYLABEL, apiKey.c_str(), 32);
  wm.addParameter(&bbApiKey);
  wm.setSaveConfigCallback(saveWMConfigCallback);

  // Double Reset detection
  drd = new DoubleResetDetector(BBDRDTIMEOUT, 0); // TODO : static declaraion (not pointer..so no NEW required)

  if (drd->detectDoubleReset())
  {
    // Start configuration portal
    printf("\n\n[COMMS] Double Reset Detected...\n");
    printf("[COMMS] ...starting config portal\n\n");

    // Send info to display-task
    strcpy(displayQMesg.data.wifiInfo, (char *)"Config Mode");
    xQueueSend(displayQueue, &displayQMesg, 0);

    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(BBPORTALTIMEOUT);

    // Start the config portal
    status = wm.startConfigPortal(CFG_COMM_SSID_PORTAL);
    printf("[COMMS] Portal closed. Status=%d\n\n", status);
  }
  else
  {
    printf("[COMMS] WiFiManager Autoconnect..\n");
    status = wm.autoConnect(CFG_COMM_SSID_PORTAL);
    printf("[COMMS] WiFimanager autoconnect status=%d\n", status);
  }

  if (wmSaveConfig)
  {
    // Save custom parameter to 'preferences'
    apiKey = bbApiKey.getValue();
    printf("[COMMS] saving custom parameter(s)\n");
    printf("[COMMS] apiKey=%s\n", apiKey.c_str());
    prefs.begin(BBPREFS, false);
    prefs.putString(BBAPIKEYID, apiKey);
    prefs.end();
  }

  // ===========================================================
  // CHECK FOR INTERNET CONNECTION, PING GOOGLE
  // ===========================================================

  // At this point in the coe we should have WiFi connection...check if we can reach the internet
  status = Ping.ping(BBPINGURL, 3);

  if (status)
  {
    printf("[COMMS] We have internet connection !!!\n");
  }
  else
  {
    printf("[COMMS] We have NO internet connection !!!\n");
  }
}

// ============================================================================
// COMMUNICATION TASK
// ============================================================================

static void communicationTask(void *arg)
{
  static bool status;
  static displayQueueItem_t displayQMesg;
  static actuatorQueueItem_t actuatorQMesg;
  static uint32_t nextTempReqMs;
  static uint16_t nextTempRequestSec;
  static uint32_t nextLCDReqMs;
  static uint16_t nextLCDRequestSec;
  static uint8_t qMesg;

  // TEMPERATURE
  static bool temperature_valid = false;
  static uint16_t temperature; // TODO : support for multiple temperatures

  // NETWORK
  static HTTPClient http;
  static int httpGETStatus;

  // PREFERENCES
  static Preferences prefs;

  // BIERBOT
  static String bbUrlPart;
  static String bbUrlTemp;
  static String bbUrlLCD;
  static const String cfgCommBBApiUrlbase = CFG_COMM_BBAPI_URL_BASE;

  // ACTUATORS
  static uint8_t actuator0 = 0;
  static uint8_t actuator1 = 0;
  static uint8_t prevActuator0 = 0;
  static uint8_t prevActuator1 = 0;

  // JSON
  static StaticJsonDocument<2048> jsonResponseDoc; // TODO : use dynamic allocation !

  // Message for display task
  displayQMesg.type = e_wifiInfo;
  displayQMesg.index = 0;
  displayQMesg.duration = 10; // seconds

  bbUrlPart = cfgCommBBApiUrlbase +
              "?apikey=" + apiKey +
              "&type=" + CFG_COMM_DEVICE_TYPE +
              "&brand=" + CFG_COMM_DEVICE_BRAND +
              "&version=" + CFG_COMM_DEVICE_VERSION +
              "&chipid=" + chipId;

  bbUrlLCD = cfgCommBBApiUrlbase +
             "?apikey=" + apiKey +
             "&type=display" +
             "&brand=oss" +
             "&version=0.1" +
             "&chipid=" + chipId +
             "&d_object_information_0=4x20";

  // ===========================================================
  // TASK LOOP
  // ===========================================================

  // TODO : simplify code, re strucuture & put TEMp & LCD stuff into helper functions

  // initial start-up delay...it's fancy to be late :-)
  nextTempRequestSec = 15;
  nextLCDRequestSec = 20;

  printf("[COMMS] Entering task loop...\n");

  // TASK LOOP
  while (true)
  {
    // Receive temperature from queue
    if (xQueueReceive(communicationQueue, &temperature, 0) == pdTRUE)
    {
      temperature_valid = true;
      printf("[COMMS] temperature received : %2.1f\n", temperature / 10.0);
    }

    // Check WiFi status
    if (WiFi.status() != WL_CONNECTED)
    {
      printf("[COMMS] Reconnecting to WiFi...\n");
      if (WiFi.reconnect())
      {
        printf("[COMMS] Reconnecting succeeded\n");
      }
      else
      {
        printf("[COMMS] Reconnecting FAILED!\n");
      }
    }
    else
    {
      // We are (again) connected to WiFi !
      // printf("[COMMS] next_request_sec=%d\n",nextTempRequestSec);

      // Send temperature to BB backend...
      if (nextTempRequestSec == 0)
      {
        printf("[COMMS] contacting BB to send temperature\n");

        // Default wait-time in case we get no valid response from the backend
        nextTempRequestSec = 60; // secs

        if (temperature_valid)
        {
          // TODO : for testing purposes only...must be configurable/parameterised
          int sensor_id = 0;

          bbUrlTemp = bbUrlPart +
                      "&s_number_temp_0=" + temperature / 10.0 +
                      "&s_number_temp_id_0=" + sensor_id +
                      "&a_bool_epower_0=" + actuator0 +
                      "&a_bool_epower_1=" + actuator1;

          printf("[COMMS] url=%s\n", bbUrlTemp.c_str());

          http.begin(bbUrlTemp);
          httpGETStatus = http.GET();

          if (httpGETStatus > 0)
          {
            String response;

            response = http.getString();

            // example response:
            // {
            //  "error":0,
            //  "error_text":"",
            //  "warning":0,
            //  "warning_text":"",
            //  "next_request_ms":15000,
            //  "epower_0_state":0,
            //  "epower_1_state":0
            // }

            // printf("[COMMS] response TEMP=%s\n", response.c_str());

            // Parse JSON object
            DeserializationError error = deserializeJson(jsonResponseDoc, response.c_str());

            if (error)
            {
              printf("[COMMS] deserializeJson() failed: %s\n", error.f_str());
              // TODO : send errors to monitor-task
            }
            else
            {
              // Extract values
              printf("[COMMS] Response:\n");
              printf("[COMMS] error           : %d\n", jsonResponseDoc["error"].as<int>());
              printf("[COMMS] error_text      : %s\n", jsonResponseDoc["error_text"].as<const char *>());
              printf("[COMMS] warning         : %d\n", jsonResponseDoc["warning"].as<int>());
              printf("[COMMS] warning_text    : %s\n", jsonResponseDoc["warning_text"].as<const char *>());
              printf("[COMMS] nextTempReqMs   : %ld\n", jsonResponseDoc["next_request_ms"].as<long>());

              String bbError;
              String bbWarning;

              if (jsonResponseDoc["error"])
              {
                // check error string
                bbError = jsonResponseDoc["error_text"].as<String>();
                // TODO : send errors to monitor-task
              }
              else
              {
                // No error...so we trigger the monitor task
                qMesg = 0;
                xQueueSend(monitorQueue, &qMesg, 0);
              }

              if (jsonResponseDoc["warning"])
              {
                // check warning string
                bbWarning = jsonResponseDoc["warning_text"].as<String>();
                // TODO : send errors to monitor-task
              }

              if (jsonResponseDoc.containsKey("next_request_ms"))
              {
                nextTempReqMs = jsonResponseDoc["next_request_ms"].as<long>();
                nextTempRequestSec = nextTempReqMs / 1000;
              }

              if (jsonResponseDoc.containsKey("epower_0_state"))
              {
                actuator0 = jsonResponseDoc["epower_0_state"].as<uint8_t>();
                printf("[COMMS] set relay 0 to: %d\n", actuator0);
              }
              else
              {
                printf("[COMMS] epower_0_state not received\n");
              }

              if (jsonResponseDoc.containsKey("epower_1_state"))
              {
                actuator1 = jsonResponseDoc["epower_1_state"].as<uint8_t>();
                printf("[COMMS] set relay 1 to: %d\n", actuator1);
              }
              else
              {
                printf("[COMMS] epower_1_state not received\n");
              }

              // TODO : add error checking before sending status to actuators;

              // Send actuator changes to actuator task
              if (actuator0 != prevActuator0)
              {
                actuatorQMesg.number = 0;
                actuatorQMesg.onOff = actuator0;
                xQueueSend(actuatorsQueue, &actuatorQMesg, 0);
                prevActuator0 = actuator0;
              }

              if (actuator1 != prevActuator1)
              {
                actuatorQMesg.number = 1;
                actuatorQMesg.onOff = actuator1;
                xQueueSend(actuatorsQueue, &actuatorQMesg, 0);
                prevActuator1 = actuator1;
              }
            }
          }
          else
          {
            printf("[COMMS] Error on HTTP GET request\n");
            // TODO : send to display
          }

          http.end(); // Free the resources
        }

      }

      if (nextTempRequestSec > 0)
      {
        // count down to 0
        nextTempRequestSec--;
      }

    }
#define NO_LCD_TEST_CODE
#ifdef LCD_TEST_CODE
 
    // THIS IS ALPHA CODE....PAINT STILL WET !!!

    if (nextLCDRequestSec == 0)
    {
      nextLCDRequestSec = 60;

      http.begin(bbUrlLCD);
      httpGETStatus = http.GET();

      if (httpGETStatus > 0)
      {
        String response;

        response = http.getString();

        // example response:
        // {
        //    "result" : "success",
        //    "error" : 0,
        //    "error_text" : "",
        //    "warning" : 0,
        //    "warning_text" : "",
        //    "settings" :
        //    {
        //        "temperatureUnit" : "celsius",
        //        "displayBrewEveryS" : 5,
        //        "showLocalIP" : false
        //    },
        //    "brews" :
        //    [
        //       {
        //           "currentTemperatureC" :
        //           {
        //               "na" : -273,
        //               "primary" : 16.375,
        //               "secondary" : -273,
        //               "hlt" : -273,
        //               "mlt" : -273
        //           },
        //           "id" : "iWY3XYUVGtR81oudBy7p",
        //           "name" : "Fermentation only",
        //           "targetTemperatureC" : 17,
        //           "nextEvents" :
        //           [
        //           ]
        //        }
        //    ],
        //    "next_request_ms" : 45000
        // }

        printf("[COMMS] Received response from LCD API\n");
        printf("[COMMS] response LCD=%s\n", response.c_str());

        // Parse JSON object
        DeserializationError error = deserializeJson(jsonResponseDoc, response.c_str());

        if (error)
        {
          printf("[COMMS] deserializeJson() failed: %s\n", error.f_str());
        }
        else
        {
          if (jsonResponseDoc["error"] == 0)
          {
            if (jsonResponseDoc.containsKey("next_request_ms"))
            {
              nextLCDReqMs = jsonResponseDoc["next_request_ms"].as<long>();
              nextLCDRequestSec = nextLCDReqMs / 1000;
            }
          }
        }
      }
    }

    if (nextLCDRequestSec > 0)
    {
      // count down to 0
      nextLCDRequestSec--;
    }

#endif

    // Double Reset Detector & WiFiManager must be called regularly
    drd->loop();
    wm.process();


    // printf("[COMMS]     nextTempRequestSec=%3d   nextLCDRequestSec=%3d\n", nextTempRequestSec, nextLCDRequestSec);

    // loop delay
    vTaskDelay(1000 / portTICK_RATE_MS);
  }
};

// ============================================================================
// INIT COMMUNICATION
// ============================================================================

void initCommmunication(void)
{
  static TaskHandle_t communicationTaskHandle = NULL;

  printf("[COMMS] init\n");

  initWiFi();

  // queue is only 1 deep...
  // sender is probably to fast and cannot transmit all measured temperatures
  // but we don't care....just take 1 temperature from queue...
  communicationQueue = xQueueCreate(1, sizeof(uint16_t));
  if (communicationQueue == 0)
  {
    printf("[DISPLAY] Cannot create communicationQueue. This is FATAL");
  }

  // create task
  xTaskCreatePinnedToCore(communicationTask, "communicationTask", 4096, NULL, 10, &communicationTaskHandle, 0);
}

// end of file
