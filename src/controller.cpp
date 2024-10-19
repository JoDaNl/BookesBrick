//
//  controller.cpp
//

#include <Arduino.h>

#include "config.h"
// #include "blinkled.h"
#include "actuators.h"
#include "display.h"
#include "controller.h"
#include "comms.h"

#if (CFG_ENABLE_HYDROBRICK == true)

// EXPERIMENTAL include
#include <esp_wifi.h>

#include "hydrobrick.h"
#endif

#if (CFG_DISPLAY_TIME == true)
#include <ESP32Time.h>
#endif

#define LOG_TAG "CTRL"

// Queues
static QueueHandle_t controllerQueue = NULL;

// controller task handle
static TaskHandle_t controllerTaskHandle = NULL;

// communication time variables (all in milliseconds)
static uint32_t NTPCallTimeMS;
static uint32_t displayTimeTimeMS;
static uint32_t IOTAPICallTimeMS;
static uint32_t PROAPICallTimeMS;

#if (CFG_ENABLE_HYDROBRICK == true)
static uint32_t hydroCallTimeMS;
static TimerHandle_t hydroTimer;
#endif

static TimerHandle_t NTPTimer;
static TimerHandle_t displayTimeTimer;
static TimerHandle_t IOTAPITimer;
static TimerHandle_t PROAPITimer;

// TIME / RTC
#if (CFG_DISPLAY_TIME == true)
static ESP32Time rtc;
static bool rtcValid = false;
#endif

// TEMPERATURE
static uint16_t temperature_x10;
static bool temperatureValid = false;

static void NTPTimerCallback(TimerHandle_t timer)
{
  controllerQItem_t qmesg;

  qmesg.type = e_mtype_controller;
  qmesg.valid = true;
  qmesg.mesg.controlMesg.mesgId = e_msg_timer_ntp;
  qmesg.mesg.controlMesg.data = 0;
  controllerQueueSend(&qmesg, 0);
}

static void displayTimeCallback(TimerHandle_t timer)
{
  controllerQItem_t qmesg;

  qmesg.type = e_mtype_controller;
  qmesg.valid = true;
  qmesg.mesg.controlMesg.mesgId = e_msg_timer_time;
  qmesg.mesg.controlMesg.data = 0;
  controllerQueueSend(&qmesg, 0);
}

static void IOTAPITimerCallback(TimerHandle_t timer)
{
  controllerQItem_t qmesg;

  qmesg.type = e_mtype_controller;
  qmesg.valid = true;
  qmesg.mesg.controlMesg.mesgId = e_msg_timer_iotapi;
  qmesg.mesg.controlMesg.data = 0;
  controllerQueueSend(&qmesg, 0);
}

static void PROAPITimerCallback(TimerHandle_t timer)
{
  controllerQItem_t qmesg;

  qmesg.type = e_mtype_controller;
  qmesg.valid = true;
  qmesg.mesg.controlMesg.mesgId = e_msg_timer_proapi;
  qmesg.mesg.controlMesg.data = 0;
  controllerQueueSend(&qmesg, 0);
}

#if (CFG_ENABLE_HYDROBRICK == true)
static void hydroTimerCallback(TimerHandle_t timer)
{
  controllerQItem_t qmesg;

  qmesg.type = e_mtype_controller;
  qmesg.valid = true;
  qmesg.mesg.controlMesg.mesgId = e_msg_timer_hydro;
  qmesg.mesg.controlMesg.data = 0;
  controllerQueueSend(&qmesg, 0);
}
#endif

// ============================================================================
// CONTROLLER TASK
// ============================================================================

void controllerTask(void *arg)
{
  float temp_sens = 0.0;
  bool temp_error = false;

  uint16_t nextReceiveTime;
  bool heartbeatTimeout = false;

  controllerQItem_t qMesgRecv;
  // static blinkLedQMesg_t      blinkLedQMesg;
  actuatorQueueItem_t actuatorsQMesg;
  displayQueueItem_t displayQMesg;
//  static WiFiQueueItem_t WiFIQMesg;
  commsQueueItem_t commsQMesg;

#if (CFG_ENABLE_HYDROBRICK == true)
  hydroQueueItem_t hydroQmesg;
#endif

  uint8_t actuators = 0;
  uint16_t compDelay = 0;

  printf("Heap Size (initController 4): %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

  // Start WiFi
//  WiFIQMesg.mesgId = e_cmd_start_wifi;
//  WiFiQueueSend(&WiFIQMesg, 0);

  String *welcome = new String("Welcome !");
  displayText(welcome, e_status_bar, 10);

  // Timer stuff

  NTPCallTimeMS = 3000;
  NTPTimer = xTimerCreate("ntp", NTPCallTimeMS / portTICK_PERIOD_MS, pdFALSE, 0, NTPTimerCallback); // One-shot
  xTimerStart(NTPTimer, 0);

  displayTimeTimeMS = 5000;
  displayTimeTimer = xTimerCreate("ntp", NTPCallTimeMS / portTICK_PERIOD_MS, pdTRUE, 0, displayTimeCallback); // Autoreload
  xTimerStart(displayTimeTimer, 0);

  IOTAPICallTimeMS = 200000;
  IOTAPITimer = xTimerCreate("iotapi", IOTAPICallTimeMS / portTICK_PERIOD_MS, pdFAIL, 0, IOTAPITimerCallback); // One-shot
  xTimerStart(IOTAPITimer, 0);

#if (CFG_ENABLE_HYDROBRICK == true)
  hydroCallTimeMS = 4000; // initial time
  hydroTimer = xTimerCreate("hydro", hydroCallTimeMS / portTICK_PERIOD_MS, pdFALSE, 0, hydroTimerCallback);
  xTimerStart(hydroTimer, 0);
#endif

  while (true)
  {

    if (xQueueReceive(controllerQueue, &qMesgRecv, 10 / portTICK_PERIOD_MS) == pdTRUE)
    {
      switch (qMesgRecv.type)
      {
      case e_mtype_controller:
        switch (qMesgRecv.mesg.controlMesg.mesgId)
        {
#if (CFG_ENABLE_HYDROBRICK == true)
        case e_msg_timer_hydro:
          // timer has triggered, we must now read the hydrometer
          ESP_LOGI(LOG_TAG,"e_msg_timer_hydro");

          // EXPERIMENTAL : switch off wifi
          // printf("Deinitialised Wifi 1\n");
          // printf("Heap Size : %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
          // esp_wifi_stop();
          // esp_wifi_deinit();
          // printf("Deinitialised Wifi 2\n");
          // printf("Heap Size : %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

          hydroQmesg.mesgId = e_msg_hydro_cmd_get_reading;
          hydroQmesg.data = 0;
          hydroQueueSend(&hydroQmesg, 0);

          // delay(5000);
          // esp_wifi_start();
          break;
#endif
        case e_msg_timer_iotapi:
          ESP_LOGI(LOG_TAG,"e_msg_timer_iotapi");
          if (temperatureValid)
          {
            // send temperature to communication task
            commsQMesg.type = e_type_comms_iotapi;
            commsQMesg.temperature_x10 = temperature_x10;
            commsQMesg.valid = true;
            communicationQueueSend(&commsQMesg, 0);
          }
          else
          {
            // restart timer / retry in 1 second
            xTimerChangePeriod(IOTAPITimer, 1000 / portTICK_PERIOD_MS, 0);
            xTimerStart(IOTAPITimer, 0);
          }
          break;

        case e_msg_timer_proapi:
          break;

        case e_msg_timer_ntp:
          // timer has triggered, we must now get accurate time using NTP
          ESP_LOGI(LOG_TAG,"e_msg_timer_ntp");
          commsQMesg.type = e_type_comms_ntp;
          commsQMesg.valid = true;
          communicationQueueSend(&commsQMesg, 0);
          break;

#if (CFG_DISPLAY_TIME == true)
        case e_msg_timer_time:
          ESP_LOGV(LOG_TAG, "e_msg_timer_time");
          // send TIME to display
          if (rtcValid)
          {
            displayQMesg.type = e_time;
            displayQMesg.data.time = (uint8_t)rtc.getHour(true) << 8 | (uint8_t)rtc.getMinute();
            displayQMesg.valid = true;
            displayQueueSend(&displayQMesg, 0);
          }
          break;
#endif
        }
        break; // e_mtype_controller

      case e_mtype_sensor:
        switch (qMesgRecv.mesg.sensorMesg.mesgId)
        {
        case e_msg_sensor_numSensors:
          ESP_LOGI(LOG_TAG, "received e_msg_sensor_numSensors, data=%d", qMesgRecv.mesg.sensorMesg.data);
          break;

        case e_msg_sensor_temperature:
          ESP_LOGI(LOG_TAG, "received e_msg_sensor_temperature, data=%d", qMesgRecv.mesg.sensorMesg.data);

          // send temperature to communication task
          temperature_x10 = qMesgRecv.mesg.sensorMesg.data;
          temperatureValid = qMesgRecv.valid;

          // send temperature to display
          displayQMesg.type = e_temperature;
          displayQMesg.data.temperature = qMesgRecv.mesg.sensorMesg.data;
          displayQMesg.valid = qMesgRecv.valid;
          displayQueueSend(&displayQMesg, 0);
          break;
        }
        break; // e_mtype_sensor

      case e_mtype_backend:
        switch (qMesgRecv.mesg.backendMesg.mesgId)
        {
        case e_msg_backend_actuators:
          ESP_LOGI(LOG_TAG, "received e_msg_backend_actuators, data=%d, valid=%d", qMesgRecv.mesg.backendMesg.data, qMesgRecv.mesg.backendMesg.valid);

          // send actuators/relay state to actuators tasks
          if (qMesgRecv.mesg.backendMesg.data != actuators)
          {
            actuators = qMesgRecv.mesg.backendMesg.data;
            actuatorsQMesg.data = qMesgRecv.mesg.backendMesg.data;
            actuatorsQueueSend(&actuatorsQMesg, 0);
          }

          // send actuators to display task
          displayQMesg.type = e_actuator;
          displayQMesg.data.actuators = qMesgRecv.mesg.backendMesg.data;
          displayQMesg.valid = qMesgRecv.mesg.backendMesg.valid;
          displayQueueSend(&displayQMesg, 0);

          // Restart timer
          xTimerChangePeriod(IOTAPITimer, qMesgRecv.mesg.backendMesg.nextRequestInterval / portTICK_PERIOD_MS, 0);
          xTimerStart(IOTAPITimer, 0);
          break; // e_msg_backend_actuators

        case e_msg_backend_act_delay:
          ESP_LOGI(LOG_TAG, "received e_msg_backend_act_delay, nr=%d, data=%d",qMesgRecv.mesg.backendMesg.number, qMesgRecv.mesg.backendMesg.data);

          // send delay information to display
          displayQMesg.type = e_delay;
          displayQMesg.number = qMesgRecv.mesg.backendMesg.number;
          displayQMesg.data.compDelay = qMesgRecv.mesg.backendMesg.data;
          displayQMesg.valid = true;
          displayQueueSend(&displayQMesg, 0);

          compDelay = qMesgRecv.mesg.backendMesg.data;
          break; // e_msg_backend_act_delay

        case e_msg_backend_heartbeat:
          ESP_LOGI(LOG_TAG, "received e_msg_backend_heartbeat, data=%d,%d", qMesgRecv.mesg.backendMesg.data>>8, qMesgRecv.mesg.backendMesg.data&255);
          // send heartBeat-detected information to display
          displayQMesg.type = e_heartbeat;
          displayQMesg.data.heartBeat = qMesgRecv.mesg.backendMesg.data;
          displayQMesg.valid = qMesgRecv.mesg.backendMesg.valid;
          displayQueueSend(&displayQMesg, 0);
          break; // e_msg_backend_heartbeat

        case e_msg_backend_temp_setpoint:
          ESP_LOGI(LOG_TAG, "received e_msg_backend_temp_setpoint, data=%d, valid=%d", qMesgRecv.mesg.backendMesg.data, qMesgRecv.mesg.backendMesg.valid);
          // send temperature set-point information to display
          displayQMesg.type = e_setpoint;
          displayQMesg.data.temperature = qMesgRecv.mesg.backendMesg.data;
          displayQMesg.valid = qMesgRecv.mesg.backendMesg.valid;
          displayQueueSend(&displayQMesg, 0);
          break; // e_msg_backend_temp_setpoint

        case e_msg_backend_device_name:
          ESP_LOGI(LOG_TAG, "received e_msg_backend_device_a");
          // send device name to display (using helper function)
          displayText(qMesgRecv.mesg.backendMesg.stringPtr, e_device_name, 0);
          break; // e_msg_backend_device_name

        case e_msg_backend_time_update:
          ESP_LOGI(LOG_TAG, "received e_msg_time_data, data=%2d:%02d", qMesgRecv.mesg.controlMesg.data >> 8, qMesgRecv.mesg.controlMesg.data & 255);

          // re-start NTP timer with appropriate interval
          if (qMesgRecv.valid)
          {
            xTimerChangePeriod(NTPTimer, CFG_COMM_TIMEREQUEST_INTERVAL_S * 1000 / portTICK_PERIOD_MS, 0);
            rtcValid = true;
          }
          else
          {
            xTimerChangePeriod(NTPTimer, 10 * 1000 / portTICK_PERIOD_MS, 0);
          }
          xTimerStart(NTPTimer, 0);

          break; // e_msg_backend_time_updated
        }
        break; // e_mtype_backend

      case e_mtype_wifi:
        char label;
        ESP_LOGI(LOG_TAG, "received e_mtype_wifi (rssi=%d, status=%d)",qMesgRecv.mesg.WiFiMesg.rssi, qMesgRecv.mesg.WiFiMesg.wifiStatus);

        switch (qMesgRecv.mesg.WiFiMesg.wifiStatus)
        {
        case e_msg_wifi_unconnected:
          label = '-';
          break;
        case e_msg_wifi_accespoint_connected:
          label = 'A';
          break;
        case e_msg_wifi_internet_connected:
          label = 'I';
          break;
        case e_msg_wifi_portal_opened:
          label = 'p';
          break;
        case e_msg_wifi_unknown:
          label = '?';
          break;
        }

        // send WiFi information to display
        displayQMesg.type = e_wifi;
        displayQMesg.data.wifiData.rssi = qMesgRecv.mesg.WiFiMesg.rssi;
        displayQMesg.data.wifiData.status = label;
        displayQMesg.valid = qMesgRecv.valid;
        displayQueueSend(&displayQMesg, 0);
        break; // e_mtype_wifi

#if (CFG_ENABLE_HYDROBRICK == true)
      case e_mtype_hydro:
        switch (qMesgRecv.mesg.hydroMesg.mesgId)
        {
        case e_cmsg_hydro_reading:
          if (qMesgRecv.valid)
          {
            ESP_LOGI(LOG_TAG, "hydrobrick reading:");
            ESP_LOGI(LOG_TAG, " SG           = %1.3f", qMesgRecv.mesg.hydroMesg.data.reading.SG_x1000 / 1000.0);
            ESP_LOGI(LOG_TAG, " angle        = %2.2f", qMesgRecv.mesg.hydroMesg.data.reading.angle_x100 / 100.0);
            ESP_LOGI(LOG_TAG, " temperature  = %2.1f", qMesgRecv.mesg.hydroMesg.data.reading.temperature_x10 / 10.0);
            ESP_LOGI(LOG_TAG, " bat. voltage = %1.3f", qMesgRecv.mesg.hydroMesg.data.reading.batteryVoltage_x1000 / 1000.0);
            ESP_LOGI(LOG_TAG, " status       = %d", qMesgRecv.mesg.hydroMesg.data.reading.status);
            ESP_LOGI(LOG_TAG, " RSSI         = %d", qMesgRecv.mesg.hydroMesg.data.reading.RSSI);

            hydroCallTimeMS = 30 * 1000;

            if (qMesgRecv.mesg.hydroMesg.data.reading.status & battery_charging)
            {
              String *charge = new String("Charging");
              displayText(charge, e_status_bar, 25);
            }
            else
            {
              String *angle = new String(qMesgRecv.mesg.hydroMesg.data.reading.batteryVoltage_x1000 / 1000.0);
              displayText(angle, e_status_bar, 25);
            }
          }
          else
          {
            ESP_LOGE(LOG_TAG, "hydrobrick reading failed");
            hydroCallTimeMS = 5 * 1000;
          }

          ESP_LOGI(LOG_TAG, "hydrobrick next reading after %d (ms)", hydroCallTimeMS);
          xTimerChangePeriod(hydroTimer, hydroCallTimeMS / portTICK_PERIOD_MS, 0);
          xTimerStart(hydroTimer, 0);

          // send specific gravity to display
          displayQMesg.type = e_specific_gravity;
          displayQMesg.data.specificGravity = qMesgRecv.mesg.hydroMesg.data.reading.SG_x1000;
          displayQMesg.valid = qMesgRecv.valid;
          displayQueueSend(&displayQMesg, 0);

          // send HB temperature to display
          displayQMesg.type = e_hb_temperature;
          displayQMesg.data.temperature = qMesgRecv.mesg.hydroMesg.data.reading.temperature_x10;
          displayQMesg.valid = qMesgRecv.valid;
          displayQueueSend(&displayQMesg, 0);

          // send voltage to display
          displayQMesg.type = e_voltage;
          displayQMesg.data.voltage = qMesgRecv.mesg.hydroMesg.data.reading.batteryVoltage_x1000;
          displayQMesg.valid = qMesgRecv.valid;
          displayQueueSend(&displayQMesg, 0);

          break; // e_cmsg_hydro_reading
        }
        break; // e_mtype_hydro
#endif
      }
    }
  }

#ifdef BOARD_HAS_RGB_LED
  switch (actuators)
  {
  case 1: // COOL -> BLUE
    digitalWrite(RGB_LED_R, 1);
    digitalWrite(RGB_LED_G, 1);
    digitalWrite(RGB_LED_B, compDelay & 1);
    break;
  case 2: // HEAT --> RED
    digitalWrite(RGB_LED_R, 0);
    digitalWrite(RGB_LED_G, 1);
    digitalWrite(RGB_LED_B, 1);
    break;
  default: // OTHERS --> OFF
    digitalWrite(RGB_LED_G, 1);
    digitalWrite(RGB_LED_G, 1);
    digitalWrite(RGB_LED_B, 1);
    break;
  }
#endif
}

// wrapper for sendQueue
int controllerQueueSend(controllerQItem_t *controllerQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (controllerQueue != NULL)
  {
    r = xQueueSend(controllerQueue, controllerQMesg, xTicksToWait);
  }

  return r;
}

void initController(void)
{
  int r;

  ESP_LOGI(LOG_TAG, "init");

  printf("Heap Size (initController 1): %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

  controllerQueue = xQueueCreate(8, sizeof(controllerQItem_t));

  printf("Heap Size (initController 2): %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());

  if (controllerQueue == 0)
  {
    ESP_LOGE(LOG_TAG, "Cannot create controllerQueue");
  }

  // create task
  r = xTaskCreatePinnedToCore(controllerTask, "controllerTask", 5 * 1024, NULL, 10, &controllerTaskHandle, 1);

  if (r != pdPASS)
  {
    ESP_LOGE(LOG_TAG, "Could not create task, error-code=%d", r);
  }

  printf("Heap Size (initController 3): %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
}

// end of file