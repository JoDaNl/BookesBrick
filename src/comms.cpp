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

#define DELAY (1000)

// GLOBALS
xQueueHandle communicationQueue = NULL;

// LOCALS
static TaskHandle_t communicationTaskHandle = NULL;
static HTTPClient http;
static StaticJsonDocument<4096> jsonResponseDoc;
static StaticJsonDocument<256> proFilterDoc;

static String usedForDevicesValue;
static bool usedForDevicesValid;

// ============================================================================
// SEND TEMPERATURE TO BACKEND AND GET NEW ACTUATOR VALUES
// - return value is next temperature request (in sec)
// ============================================================================

static uint16_t getActuatorsFromBackend(String &tempUrl, uint16_t sensorId, uint16_t temperature)
{
  static int getResponse;
  static uint8_t actuators = 0;
  static uint8_t actuatorsPrev = 0;
  static uint8_t updatedActuator;

  static String response;
  static String fullURL;
  static String bbError;
  static String bbWarning;

  static uint32_t nextTempReqMs;
  static uint16_t nextTempRequestSecValue;
  static bool nextTempRequestSecValid;
  static uint32_t responseCount = 0;

  static controllerQItem_t controllerMesg;

  printf("[COMMS] Send temperature to BierBot\n");

  // Default wait-time in case we get no valid response from the backend
  nextTempRequestSecValue = 60; // secs
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

    //response = "{\"error\":0,\"error_text\":\"\",\"warning\":0,\"warning_text\":\"\",\"next_request_ms\":10000,\"epower_0_state\":1,\"epower_1_state\":0,\"used_for_devices\":[\"U5iFFLOgAG07AZ7wrql0\", \"U5iFFLOgAG07AZ7wrql1\"]}";
    // response = "{\"error\":0,\"error_text\":\"\",\"warning\":0,\"warning_text\":\"\",\"next_request_ms\":10000,\"epower_0_state\":1,\"epower_1_state\":0,\"used_for_devices\":[\"U5iFFLOgAG07AZ7wrql0\"]}";
    // printf("[COMMS] response TEMP=%s\n", response.c_str());

    // See: https://arduinojson.org/v6/issues/cannot-get-values/

    response = http.getString();
    printf("[COMMS] response TEMP (received)  =%s\n", response.c_str());
    response.replace("\"[\\", "[");
    response.replace("\\\"]\"", "\"]");
    printf("[COMMS] response TEMP (corrected) =%s\n", response.c_str());

    // Parse JSON object
    DeserializationError deserialisationError = deserializeJson(jsonResponseDoc, response);

    if (deserialisationError)
    {
      printf("[COMMS] deserializeJson() failed: %s\n", deserialisationError.f_str());
      printf("[COMMS] server response is:\n%s\n", response.c_str());
    }
    else
    {
      // Extract values
      printf("[COMMS] Response      : %ld\n", responseCount++);
      printf("[COMMS] error         : %d - %s\n", jsonResponseDoc["error"].as<int>(), jsonResponseDoc["error_text"].as<const char *>());
      printf("[COMMS] warning       : %d - %s\n", jsonResponseDoc["warning"].as<int>(), jsonResponseDoc["warning_text"].as<const char *>());
      printf("[COMMS] nextTempReqMs : %ld\n", jsonResponseDoc["next_request_ms"].as<long>());

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
        nextTempRequestSecValue = nextTempReqMs / 1000;
        nextTempRequestSecValid = true;
      }

      if (jsonResponseDoc.containsKey("epower_0_state"))
      {
        updatedActuator = jsonResponseDoc["epower_0_state"].as<uint8_t>();
        printf("[COMMS] set relay 0 to  : %d\n", updatedActuator);
      }
      else
      {
        printf("[COMMS] epower_0_state not received\n");
        updatedActuator = 0; // safety measure : disable actuators in case of communication error
      }

      actuators = actuators | (updatedActuator & 1);

      if (jsonResponseDoc.containsKey("epower_1_state"))
      {
        updatedActuator = jsonResponseDoc["epower_1_state"].as<uint8_t>();
        printf("[COMMS] set relay 1 to  : %d\n", updatedActuator);
      }
      else
      {
        printf("[COMMS] epower_1_state not received\n");
        updatedActuator = 0; // safety measure : disable actuators in case of communication error
      }

      actuators = actuators | (updatedActuator & 1) << 1;


      usedForDevicesValid = false; // default

      if (jsonResponseDoc.containsKey("used_for_devices"))
      {
        JsonArray devices;
        JsonVariant device;


        // StaticJsonDocument<1024> jsonResponseDoc2;
        // DeserializationError deserialisationError2 = deserializeJson(jsonResponseDoc2, jsonResponseDoc["used_for_devices"]);



        devices = jsonResponseDoc["used_for_devices"].as<JsonArray>();
        String d = jsonResponseDoc["used_for_devices"];
        printf("[COMMS] ==> d = %s\n", d.c_str());
        printf("[COMMS] ==> l = %d\n", d.length());


        printf("[COMMS] devices.size() = %d\n", devices.size());


        // if (dsvices.size()>0)
        {
          for (int i = 0; i < devices.size(); i++)
          {
            device = devices[i];
            usedForDevicesValue = device.as<String>();
            printf("[COMMS] usedForDevices[%d] : %s\n", i, usedForDevicesValue.c_str());
          }
          usedForDevicesValid = true;
        }
        // else
        // {
        //   printf("[COMMS] used_for_devices not received\n");
        // }
      }

       usedForDevicesValid = true;

      // send actuators to controller
      if (actuators != actuatorsPrev)
      {
        actuatorsPrev = actuators;
        controllerMesg.type = e_mtype_backend;
        controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_actuators;
        controllerMesg.mesg.backendMesg.data = actuators;
        controllerMesg.mesg.backendMesg.valid = true;
        xQueueSend(controllerQueue, &controllerMesg, 0);
      }

      // Send heartbeat message
      controllerMesg.type = e_mtype_backend;
      controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_heartbeat;
      controllerMesg.mesg.backendMesg.data = nextTempRequestSecValue;
      controllerMesg.mesg.backendMesg.valid = true;
      xQueueSend(controllerQueue, &controllerMesg, 0);

    } // extract values
  }
  else
  {
    printf("[COMMS] no valid response from back-end. getResponse=%d\n", getResponse);

    // Send heartbeat message
    controllerMesg.type = e_mtype_backend;
    controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_heartbeat;
    controllerMesg.mesg.backendMesg.data = nextTempRequestSecValue;
    controllerMesg.mesg.backendMesg.valid = false;
    xQueueSend(controllerQueue, &controllerMesg, 0);
  }
  printf("[COMMS]---------------------------\n");

  return nextTempRequestSecValue;
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
  static controllerQItem_t controllerMesg;

  static bool setPointValid;
  static int16_t setPointValue;

  nextLCDRequestSec = 60;
  setPointValid = false;
  setPointValue = 0;

  printf("[COMMS] LCD API url=%s\n", UrlLCD.c_str());

  http.begin(UrlLCD);
  getResponse = http.GET();

  if (getResponse > 0)
  {
    response = http.getString();

    printf("[COMMS] Received response from LCD API\n");
    printf("[COMMS] response LCD=%s\n", response.c_str());


    DeserializationError deserialisationError = deserializeJson(jsonResponseDoc, response.c_str());

    JsonArray brews;
    JsonObject brew;

    if (deserialisationError)
    {
      printf("[COMMS] deserializeJson() failed: %s\n", deserialisationError.f_str());
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
          for (int i = 0; i < brews.size(); i++)
          {
            brew = brews[i];
            bTemp = brew["targetTemperatureC"].as<float>();
            bName = brew["name"].as<String>();
            bId = brew["id"].as<String>();
            printf("[COMMS]   targetTemp[%d]=%f\n", i, bTemp);
            printf("[COMMS]         name[%d]=%s\n", i, bName.c_str());
            printf("[COMMS]           id[%d]=%s\n", i, bId.c_str());

            // For non-ferminting brewing steps the backend may report a (valid) temperature of 0.
            // The brick sees this as an invalid temperature
            if (bTemp > 0)
            {
              setPointValid = true;
              setPointValue = (int16_t)(bTemp * 10);
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
  xQueueSend(controllerQueue, &controllerMesg, 0);

  printf("[COMMS]---------------------------\n");

  return nextLCDRequestSec;
}


static void initFilterDocs(void)
{
  // Use a JSON filter document, see : https://arduinojson.org/v6/how-to/deserialize-a-very-large-document/

  proFilterDoc["name"] = true;
  proFilterDoc["type"] = true;
  proFilterDoc["active"] = true;
  proFilterDoc["targetState"]["tempCelsius"] = true;
}



static uint16_t getProAPIInfo()
{
  static String response;
  static String URL;
  static int getResponse;
  static uint16_t setPointValue;
  static bool setPointValid;
  static controllerQItem_t controllerMesg;

  setPointValid = false;

  if (usedForDevicesValid)
  {
    URL = "https://bricks.bierbot.com/api/device?apikey=" + config.apiKey + "&proapikey=" + config.proApiKey + "&deviceid=" + usedForDevicesValue;

    printf("[COMMS] PRO API url=%s\n", URL.c_str());

    http.begin(URL);
    getResponse = http.GET();

    if (getResponse > 0)
    {
      response = http.getString();

      printf("[COMMS] Received response from PRO API\n");
//      printf("[COMMS] response PRO=%s\n", response.c_str());

      DeserializationError error = deserializeJson(jsonResponseDoc, response.c_str(), DeserializationOption::Filter(proFilterDoc));

      if (error)
      {
        printf("[COMMS] PRO deserializeJson() failed: %s\n", error.f_str());
        printf("[COMMS] PRO server response is:\n%s\n", response.c_str());
      }
      else
      {
        // Extract values
        printf("[COMMS] PRO name            : %s\n", jsonResponseDoc["name"].as<String>().c_str());
        printf("[COMMS] PRO type            : %s\n", jsonResponseDoc["type"].as<String>().c_str());
        // printf("[COMMS] PRO valid           : %s\n", jsonResponseDoc["valid"].as<String>().c_str());
        // printf("[COMMS] PRO blocked         : %s\n", jsonResponseDoc["blocked"].as<String>().c_str());
        printf("[COMMS] PRO active          : %s\n", jsonResponseDoc["active"].as<String>().c_str());
        // printf("[COMMS] usedForProcessId    : %s\n", jsonResponseDoc["usedForProcessId"].as<String>().c_str());
        // printf("[COMMS] usedForProcessType  : %s\n", jsonResponseDoc["usedForProcessType"].as<String>().c_str());
        printf("[COMMS] Temperature         : %s\n", jsonResponseDoc["targetState"]["tempCelsius"].as<String>().c_str());

        setPointValue = jsonResponseDoc["targetState"]["tempCelsius"].as<float>() * 10;

      }
    }
    else
    {
      printf("[COMMS] Received NO VALID RESPONSE from PRO API: %d\n", getResponse);
    }

    controllerMesg.type = e_mtype_backend;
    controllerMesg.mesg.backendMesg.mesgId = e_msg_backend_temp_setpoint;
    controllerMesg.mesg.backendMesg.data = setPointValue;
    controllerMesg.mesg.backendMesg.valid = setPointValid;
    xQueueSend(controllerQueue, &controllerMesg, 0);
  }
  else
  {
    printf("[COMMS] Skipping pro-API call as we have no valid device-id yet\n");
  }

  return (15); // query every 15 seconds
}

// ============================================================================
// COMMUNICATION TASK
// ============================================================================

static void communicationTask(void *arg)
{
  static bool status;
  static uint16_t nextTempRequestSec;
  static uint16_t nextLCDRequestSec;
  static uint16_t nextPRORequestSec;

  // TEMPERATURE
  static bool temperature_valid = false;
  static int16_t temperature; // TODO : support for multiple temperatures

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
  initFilterDocs();

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
  nextPRORequestSec = 15;

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
                    "?apikey=" + config.apiKey +
                    "&type=" + CFG_COMM_DEVICE_TYPE +
                    "&brand=" + CFG_COMM_DEVICE_BRAND +
                    "&version=" + CFG_COMM_DEVICE_VERSION +
                    "&chipid=" + chipId;

        // Call to back-end
        // TODO : sensorId is hard-coded to 0
        nextTempRequestSec = getActuatorsFromBackend(bbUrlTemp, 0, temperature);
      }

      // if (nextLCDRequestSec > 0)
      // {
      //   // count down to 0
      //   nextLCDRequestSec--;
      // }
      // else
      // {
      //   // Call to back-end
      //   nextLCDRequestSec = getLCDInfoFromBackend(bbUrlLCD);
      // }

      if (nextPRORequestSec > 0)
      {
        // count down to 0
        nextPRORequestSec--;
      }
      else
      {
        // Call to back-end
        nextPRORequestSec = getProAPIInfo();
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
