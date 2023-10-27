//
// comms.cpp
//

#include "config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <preferences.h>

#include "controller.h"
#include "comms.h"

#define DELAY  (1000)

// GLOBALS
xQueueHandle communicationQueue = NULL;

// LOCALS
static TaskHandle_t communicationTaskHandle = NULL;
static HTTPClient http;
static StaticJsonDocument<2048> jsonResponseDoc;


// ============================================================================
// SEND TEMPERATURE TO BACKEND AND GET NEW ACTUATOR VALUES
// - return value is next temprature request
// ============================================================================

static uint16_t getActuatorsFromBackend(String &tempUrl, uint16_t sensorId, uint16_t temperature)
{
  static int getResponse;
  static uint8_t actuators = 0;
  static uint8_t actuatorsPrev = 0;
  static uint8_t updatedActuator;
  static String usedForDevices;

  static String response;
  static String fullURL;
  static String bbError;
  static String bbWarning;

  static uint32_t nextTempReqMs;
  static uint16_t nextTempRequestSec;
  static bool nextTempRequestSecValid;
  static uint32_t responseCount = 0;

  static controllerQItem_t controllerMesg;

  printf("[COMMS] Send temperature to BierBot\n");

  // Default wait-time in case we get no valid response from the backend
  nextTempRequestSec = 60; // secs
  nextTempRequestSecValid = false;

  fullURL = tempUrl +
              "&s_number_temp_0=" + temperature / 10.0 +
              "&s_number_temp_id_0=" + sensorId +
              "&a_bool_epower_0=" + (actuators & 1) +
              "&a_bool_epower_1=" + ((actuators >> 1) & 1);

  // spontaneous crashes on http.GET()

  printf("[COMMS]---------------------------\n");
  printf("[COMMS] TEMP API url=%s\n", fullURL.c_str());

  http.begin(fullURL);
  getResponse = http.GET();

  if (getResponse > 0)
  {
    response = http.getString();
    printf("[COMMS] response TEMP=%s\n", response.c_str());

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

    // Parse JSON object
    DeserializationError error = deserializeJson(jsonResponseDoc, response.c_str());

    if (error)
    {
      printf("[COMMS] deserializeJson() failed: %s\n", error.f_str());
      printf("[COMMS] server response is:\n%s\n", response.c_str());
    }
    else
    {
      // Extract values
      printf("[COMMS] Response        : %ld\n",     responseCount++);
      printf("[COMMS] error           : %d - %s\n", jsonResponseDoc["error"].as<int>(), jsonResponseDoc["error_text"].as<const char *>());
      printf("[COMMS] warning         : %d - %s\n", jsonResponseDoc["warning"].as<int>(), jsonResponseDoc["warning_text"].as<const char *>());
      printf("[COMMS] nextTempReqMs   : %ld\n",     jsonResponseDoc["next_request_ms"].as<long>());

      actuators = 0; // default, only set actuators when correct message has been received

      if (jsonResponseDoc["error"])
      {
        // check error string
        bbError = jsonResponseDoc["error_text"].as<String>();
      }

      if (jsonResponseDoc["warning"])
      {
        // check warning string
        bbWarning = jsonResponseDoc["warning_text"].as<String>();
      }

      if (jsonResponseDoc.containsKey("next_request_ms"))
      {
        nextTempReqMs = jsonResponseDoc["next_request_ms"].as<long>();
        nextTempRequestSec = nextTempReqMs / 1000;
        nextTempRequestSecValid = true;
      }

      if (jsonResponseDoc.containsKey("epower_0_state"))
      {
        updatedActuator = jsonResponseDoc["epower_0_state"].as<uint8_t>();
        printf("[COMMS] set relay 0 to  : %d\n", updatedActuator);

        actuators = actuators | (updatedActuator & 1);
      }
      else
      {
        printf("[COMMS] epower_0_state not received\n");
      }

      if (jsonResponseDoc.containsKey("epower_1_state"))
      {
        updatedActuator = jsonResponseDoc["epower_1_state"].as<uint8_t>();
        printf("[COMMS] set relay 1 to  : %d\n", updatedActuator);

        actuators = actuators | (updatedActuator & 1) << 1;
      }
      else
      {
        printf("[COMMS] epower_1_state not received\n");
      }

      if (jsonResponseDoc.containsKey("used_for_devices"))
      {
        JsonArray devices;
        JsonVariant device;

        devices = jsonResponseDoc["used_for_devices"];

        for(int i=0; i< devices.size(); i++)
        {
          device = devices[i];
          usedForDevices = device.as<String>();
          printf("[COMMS] usedForDevices[%d] : %s\n", i, usedForDevices);
        }

        actuators = actuators | (updatedActuator & 1) << 1;
      }
      else
      {
        printf("[COMMS] epower_1_state not received\n");
      }
    }

    // send actuators to controller
    if (actuators != actuatorsPrev)
    {
      actuatorsPrev = actuators;
      controllerMesg.type = e_mtype_backend;
      controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_actuators;
      controllerMesg.mesg.backendMesg.data = actuators;
      controllerMesg.mesg.backendMesg.valid = true;
      xQueueSend(controllerQueue, &controllerMesg , 0);
    }

    // Send heartbeat message 
    controllerMesg.type = e_mtype_backend;
    controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_heartbeat;
    controllerMesg.mesg.backendMesg.data = nextTempRequestSec;
    controllerMesg.mesg.backendMesg.valid = true;
    xQueueSend(controllerQueue, &controllerMesg , 0);
  }
  else
  {
    printf("[COMMS] no valid response from back-end. getResponse=%d\n", getResponse);

    // Send heartbeat message 
    controllerMesg.type = e_mtype_backend;
    controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_heartbeat;
    controllerMesg.mesg.backendMesg.data = nextTempRequestSec;
    controllerMesg.mesg.backendMesg.valid = false;
    xQueueSend(controllerQueue, &controllerMesg , 0);
  }
  printf("[COMMS]---------------------------\n");


  return nextTempRequestSec;
}


// ============================================================================
// GET UPDATED LCD INFO FROM BACKEND
// - See : https://forum.bierbot.com/viewtopic.php?t=68
// ============================================================================

static uint16_t getLCDInfoFromBackend(String &UrlLCD)
{
  static int getResponse;
  static uint32_t nextLCDReqMs;
  static uint16_t nextLCDRequestSec;
  static String response;
  // static StaticJsonDocument<128> filterDoc;
  static controllerQItem_t controllerMesg;

  static bool setPointValid;
  static int16_t setPointValue;

  nextLCDRequestSec = 60;
  setPointValid = false;
  setPointValue = 0;

  printf("[COMMS]---------------------------\n");
  printf("[COMMS] LCD API url=%s\n", UrlLCD.c_str());

  http.begin(UrlLCD);
  getResponse = http.GET();

  if (getResponse > 0)
  {
    response = http.getString();

    printf("[COMMS] Received response from LCD API\n");
    printf("[COMMS] response LCD=%s\n", response.c_str());

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

    // Use a JSON filter document, see : https://arduinojson.org/v6/how-to/deserialize-a-very-large-document/

    // {
    //     "result" : true,
    //     "brews" :
    //     [
    //         { "targetTemperatureC" : true }
    //     ],
    //  "next_request_ms" : true
    // }


    // filterDoc["result"] = true;
    // filterDoc["brews"][0]["targetTemperatureC"] = true;
    // filterDoc["next_request_ms"] = true;

    // Parse JSON object
    //DeserializationError error = deserializeJson(jsonResponseDoc, response.c_str(), DeserializationOption::Filter(filterDoc));
    DeserializationError error = deserializeJson(jsonResponseDoc, response.c_str());

    JsonArray brews;
    JsonObject brew;

    if (error)
    {
      printf("[COMMS] deserializeJson() failed: %s\n", error.f_str());
    }
    else
    {
      if (jsonResponseDoc["result"] == "success")
      {
        uint16_t targetTemp;
               
        if (jsonResponseDoc.containsKey("next_request_ms"))
        {
          printf("[COMMS]   next_request_ms=%ld\n", jsonResponseDoc["next_request_ms"].as<long>());
          nextLCDReqMs = jsonResponseDoc["next_request_ms"].as<long>();
          nextLCDRequestSec = nextLCDReqMs / 1000;
        }

        if (jsonResponseDoc.containsKey("brews"))
        {
          float bTemp;
          String bId;
          String bName;
          
          brews = jsonResponseDoc["brews"];
          printf("[COMMS]   brews num=%d\n", brews.size());
          for (int i=0; i< brews.size(); i++)
          {
            brew  = brews[i];
            bTemp = brew["targetTemperatureC"].as<float>();
            bName = brew["name"].as<String>();
            bId   = brew["id"].as<String>();
            printf("[COMMS]   targetTemp[%d]=%f\n", i, bTemp);
            printf("[COMMS]         name[%d]=%s\n", i, bName.c_str());
            printf("[COMMS]           id[%d]=%s\n", i, bId.c_str());

            // For non-ferminting brewing steps the backend may report a (valid) temperature of 0.
            // The brick sees this as an invalid temperature
            if (bTemp >0)
            {
              setPointValid = true;
              setPointValue = (int16_t)(bTemp*10);
            }
           }
        }
        else
        {
          printf("[COMMS]   no active brews\n");
        }
      }

    }

  }

// Send target temperature to controller task
  controllerMesg.type = e_mtype_backend;
  controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_temp_setpoint;
  controllerMesg.mesg.backendMesg.data = setPointValue;
  controllerMesg.mesg.backendMesg.valid = setPointValid;
  xQueueSend(controllerQueue, &controllerMesg , 0);

  printf("[COMMS]---------------------------\n");

  return nextLCDRequestSec;
}


// ============================================================================
// COMMUNICATION TASK
// ============================================================================

static void communicationTask(void *arg)
{
  static bool status;
  static uint16_t nextTempRequestSec;
  static uint16_t nextLCDRequestSec;

  // TEMPERATURE
  static bool temperature_valid = false;
  static int16_t temperature;  // TODO : support for multiple temperatures

  // NETWORK
  static int httpGETStatus;

  // use chip-id as unique ID required by BB api
  static uint64_t chipId;

  // BIERBOT urls
  static String bbUrlTemp;
  static String bbUrlLCD;
  static const String cfgCommBBApiUrlbase = CFG_COMM_BBAPI_URL_BASE;

  // ACTUATORS
  static uint8_t actuator0 = 0;
  static uint8_t actuator1 = 0;
  static uint8_t prevActuator0 = 0;
  static uint8_t prevActuator1 = 0;

  // JSON

  // Get MAC addres as unique ID for BB URL
  chipId = ESP.getEfuseMac();

  bbUrlLCD = cfgCommBBApiUrlbase +
             "?apikey=" + config.apiKey +
             "&type=display" +
             "&brand=oss" +
             "&version=0.1" +
             "&chipid=" + chipId +
             "&d_object_information_0=4x20";

  
  nextTempRequestSec = 10;
  nextLCDRequestSec = 15;

  // ===========================================================
  // TASK LOOP
  // ===========================================================

  printf("[COMMS] Entering task loop...\n");

  // TASK LOOP
  while (true)
  {
    // Print memmory usage
    // printf("[COMMS] Free memory: %d bytes\n", esp_get_free_heap_size());
    // printf("[COMMS] This Task watermark: %d bytes\n", uxTaskGetStackHighWaterMark(NULL));

    // Receive temperature from queue
    if (xQueueReceive(communicationQueue, &temperature, 0) == pdTRUE)
    {
      if (nextTempRequestSec > 0)
      {
        nextTempRequestSec--;
      }
      else
      {

        bbUrlTemp = cfgCommBBApiUrlbase +
                    "?apikey="              + config.apiKey +
                    "&type="                + CFG_COMM_DEVICE_TYPE +
                    "&brand="               + CFG_COMM_DEVICE_BRAND +
                    "&version="             + CFG_COMM_DEVICE_VERSION +
                    "&chipid="              + chipId;

        // Call to back-end
        // TODO : sensorId is hard-coded to 0
        nextTempRequestSec = getActuatorsFromBackend(bbUrlTemp, 0, temperature);
      }

      if (nextLCDRequestSec > 0)
      {
        // count down to 0
        nextLCDRequestSec--;
      }
      else
      {
          // Call to back-end
          nextLCDRequestSec = getLCDInfoFromBackend(bbUrlLCD);
       }
    }

    // loop delay
    vTaskDelay(DELAY / portTICK_RATE_MS);
  }
};


// ============================================================================
// INIT COMMUNICATION
// ============================================================================

void initCommmunication(void)
{

  printf("[COMMS] init\n");

  communicationQueue = xQueueCreate(4, sizeof(int16_t));
  if (communicationQueue == 0)
  {
    printf("[COMMS] Cannot create communicationQueue. This is FATAL\n");
  }

  // create task
  xTaskCreate(communicationTask, "communicationTask", 8192, NULL, 10, &communicationTaskHandle);
}

// end of file
