
// See: https://forum.arduino.cc/t/arduino-and-ble-on-esp32-as-server-and-client-combined-using-nimble/1151247

#if (CFG_ENABLE_HYDROBRICK == true)

#include <Arduino.h>
#include "config.h"
#include <NimBLEDevice.h>
#include "controller.h"
#include "hydrobrick.h"

#define LOG_TAG "HYDRO"

static scanMode_t scanMode;
static hydrometerScannedBricks_t scannedBricks;

// Queues
// static QueueHandle_t hydroQueue = NULL;
static QueueHandle_t hydroQueue = NULL;

// Task handle
static TaskHandle_t hydroTaskHandle = NULL;

static NimBLEUUID HDhydrometerService(HDhydrometerServiceUUID);
static NimBLEUUID HDhydrometerCharacteristic(HDhydrometerCharacteristicID);

static NimBLEScan *pScan = NimBLEDevice::getScan();
static NimBLERemoteCharacteristic *pRemoteCharacteristic = NULL;

// static NimBLEAdvertisedDevice *pHydroBrickDevice = NULL;

static bool hydroBrickDeviceValid;
static NimBLEAdvertisedDevice hydroBrickDevice;

static NimBLEClient *pHydroBrickClient = NULL;

static hydrometerDataBytes_t hydrometerDataBytes;

static uint8_t hydroBrickAddressArray[] = {0xA2, 0x4B, 0xED, 0x2B, 0xCC, 0x4B};

static NimBLEAddress hydroBrickAddress(hydroBrickAddressArray, true);

// ========================================================================
// Angle to SG conversion
static uint16_t angleToSG(uint16_t angle_x100)
{
  uint16_t SG;

  SG = 1000 + angle_x100;

  return SG;
}

// ========================================================================
// BLE init & de-init functions

static void initBLE(void)
{
  ESP_LOGI(LOG_TAG, "init BLE");

  printf("Heap Size 1: %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  NimBLEDevice::init("");
  printf("Heap Size 2: %d, free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9db
  printf("[HYDRO] init BLE done\n");

  pHydroBrickClient = BLEDevice::createClient();
}

static void deInitBLE(void)
{
  ESP_LOGI(LOG_TAG, "de-init BLE");

  pScan->stop();
  if (pHydroBrickClient != NULL)
  {
    pHydroBrickClient->disconnect();
  }

  NimBLEDevice::deinit(true);
}

static void cleanUp(void)
{
  pScan->stop();
  pHydroBrickClient->disconnect();
}

// ========================================================================
// call-back functions

class clientConnectCallbacks : public NimBLEClientCallbacks
{
  void onConnect(NimBLEClient *pClient)
  {
    hydroQMesg qmesg;

    /** After connection we should change the parameters if we don't need fast response times.
     *  These settings are 150ms interval, 0 latency, 450ms timout.
     *  Timeout should be a multiple of the interval, minimum is 100ms.
     *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
     *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
     */
    pClient->updateConnParams(120, 120, 0, 60);

    qmesg.mesgId = e_msg_hydro_evt_device_connected;
    qmesg.data = 0;
    hydroQueueSend(&qmesg, 0);
  }

  void onDisconnect(NimBLEClient *pClient)
  {
    hydroQMesg qmesg;

    qmesg.mesgId = e_msg_hydro_evt_device_disconnected;
    qmesg.data = 0;
    hydroQueueSend(&qmesg, 0);
  }

  /** Called when the peripheral requests a change to the connection parameters.
   *  Return true to accept and apply them or false to reject and keep
   *  the currently used parameters. Default will return true.
   */
#ifdef DO_USE
  bool onConnParamsUpdateRequest(NimBLEClient *pClient, const ble_gap_upd_params *params)
  {
    if (params->itvl_min < 24)
    { /** 1.25ms units */
      return false;
    }
    else if (params->itvl_max > 40)
    { /** 1.25ms units */
      return false;
    }
    else if (params->latency > 2)
    { /** Number of intervals allowed to skip */
      return false;
    }
    else if (params->supervision_timeout > 100)
    { /** 10ms units */
      return false;
    }

    return true;
  };
#endif
};

class advertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
  void onResult(NimBLEAdvertisedDevice *advertisedDevice)
  {
    hydroQueueItem_t qmesg;

    ESP_LOGI(LOG_TAG, "Advertised Device: %s, %s", advertisedDevice->getAddress().toString().c_str(), advertisedDevice->getName().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(HDhydrometerService))
    {
      switch (scanMode)
      {
      case e_scan_for_all_bricks:
      {
        ESP_LOGD(LOG_TAG, "HydroBrick found");
        if (scannedBricks.number < CFG_MAX_NR_HYDROBRICKS - 1)
        {
          scannedBricks.addresses[scannedBricks.number] = advertisedDevice->getAddress();
          scannedBricks.number++;
        }
      }
      break;

      case e_scan_for_registered_brick:
      {
        if (advertisedDevice->getAddress() == hydroBrickAddress)
        {
          // if (hydroBrickDeviceValid == false)
          {
            //  copy object
            hydroBrickDevice = *advertisedDevice;
            hydroBrickDeviceValid = true;

            //            pHydroBrickDevice = advertisedDevice;

            qmesg.mesgId = e_msg_hydro_evt_device_discovered;
            qmesg.data = 0;
            hydroQueueSend(&qmesg, 0);

            pScan->stop();

            ESP_LOGD(LOG_TAG, "Registered HydroBrick found!");
          }
        }
      }
      break;

      default:
        pScan->stop();
        break;
      }
    }
  }
};

void scanEndCallback(NimBLEScanResults results)
{
  ESP_LOGD(LOG_TAG, "Scan ended");
}

static advertisedDeviceCallbacks advertisedDeviceCallback;
static clientConnectCallbacks clientConnectCallBack;

// ========================================================================
//

void scanForHydroBrick(void)
{
  ESP_LOGI(LOG_TAG, "start scanning");

  pScan->setAdvertisedDeviceCallbacks(&advertisedDeviceCallback, false);
  pScan->setInterval(1349);
  pScan->setWindow(449);
  pScan->setActiveScan(true);

  // pScan->setFilterPolicy(BLE_HCI_SCAN_FILT_USE_WL);
  // NimBLEDevice::whiteListAdd(hydroBrickAddress);

  pScan->setMaxResults(0);

  // Set device to NULL as indicator that no device has been discovered yet
  // Callback funtion will set the device-pointer
  // pHydroBrickDevice = NULL;

  hydroBrickDeviceValid = false;

  pScan->start(10);
  //  pScan->start(0, scanEndCallback);

  ESP_LOGI(LOG_TAG, "scanning done");
}

void connectToHydroBrick(void)
{
  //  pHydroBrickClient = NULL;

  if (hydroBrickDeviceValid)
  {
    ESP_LOGI(LOG_TAG, "HydroBrink connected");

    pHydroBrickClient->setClientCallbacks(&clientConnectCallBack, false);

    // Connect to the remote BLE Server.
    pHydroBrickClient->connect(&hydroBrickDevice);
  }
}

void readService(void)
{
  std::string value;
  BLERemoteService *pRemoteService;

  if (pHydroBrickClient != NULL)
  {
    ESP_LOGD(LOG_TAG, "Read service");

    pRemoteService = pHydroBrickClient->getService(HDhydrometerService);

    if (pRemoteService != NULL)
    {
      ESP_LOGD(LOG_TAG, "Found service %s", HDhydrometerService.toString().c_str());

      pRemoteCharacteristic = pRemoteService->getCharacteristic(HDhydrometerCharacteristic);

      if (pRemoteCharacteristic != NULL && pRemoteCharacteristic->canRead())
      {
        value = pRemoteCharacteristic->readValue();
        // printf("[HYDRO] number of bytes received=%d, payload=", value.length());
        for (int i = 0; i < value.length(); i++)
        {
          // printf("0x%02X,", value[i]);
          // copy payload into union to form correct data-structure
          hydrometerDataBytes.bytes[i] = value[i];
        }
        // printf("\n");
      }
      else
      {
        ESP_LOGE(LOG_TAG, "Failed to find characteristic");
        pHydroBrickClient->disconnect();
        return;
      }
    }
    else
    {
      ESP_LOGE(LOG_TAG, "Failed to find service UUID");
      pHydroBrickClient->disconnect();
      return;
    }

    pHydroBrickClient->disconnect();
  }
  else
  {
    ESP_LOGE(LOG_TAG, "pHydroBrickClient == NULL");
  }
}

static void timeOutTimerCallback(TimerHandle_t xTimer)
{
  hydroQueueItem_t qmesg;

  qmesg.mesgId = e_msg_hydro_evt_timeout;
  qmesg.data = 0;
  hydroQueueSend(&qmesg, 0);
}

// ========================================================================
// Hydrometer task function

typedef enum
{
  state_idle,
  state_start_scan,
  state_scanning,
  state_discovered,
  state_connected,
  state_read,
  state_timeout,
  state_result
} states_t;

void hydrometerTask(void *arg)
{
  uint16_t r;
  hydroQueueItem_t qMesgRecv;
  TimerHandle_t timeOutTimer;
  states_t state = state_idle;
  states_t next_state = state_idle;
  bool timeOutOccurred;
  controllerQItem_t controllerQMesg;

  // SETUP TIMEOUT - TIMER
  timeOutTimer = xTimerCreate("timeout", 15000 / portTICK_PERIOD_MS, pdFALSE, 0, timeOutTimerCallback);

  while (true)
  {
    // RECEIVE QUEUE MESSAGE
    r = xQueueReceive(hydroQueue, &qMesgRecv, 100 / portTICK_PERIOD_MS);

    if (r == pdTRUE)
    {
      switch (qMesgRecv.mesgId)
      {
      case e_msg_hydro_cmd_get_reading:
        ESP_LOGI(LOG_TAG, "qmesg = e_msg_hydro_cmd_get_reading");
        scanMode = e_scan_for_registered_brick;
        next_state = state_start_scan;
        break;

      case e_msg_hydro_cmd_scan_bricks:
        ESP_LOGV(LOG_TAG, "qmesg = e_msg_hydro_cmd_scan_bricks");
        scanMode = e_scan_for_all_bricks;
        scannedBricks.number = 0;
        next_state = state_start_scan;
        break;

      case e_msg_hydro_evt_device_discovered:
        ESP_LOGV(LOG_TAG, "qmesg = e_msg_hydro_evt_device_discovered");
        next_state = state_discovered;
        break;

      case e_msg_hydro_evt_device_connected:
        ESP_LOGV(LOG_TAG, "qmesg = e_msg_hydro_evt_device_connected");
        next_state = state_connected;
        break;

      case e_msg_hydro_evt_timeout:
        ESP_LOGV(LOG_TAG, "qmesg = e_msg_hydro_evt_timeout");
        next_state = state_timeout;
        break;
      }
    }

    if (next_state != state)
    {
        ESP_LOGV(LOG_TAG, "state change into : %d", next_state);
      state = next_state;
    }

    // STATEMACHINE
    switch (state)
    {
    case state_idle:
      //        next_state = state_idle;
      break;

    case state_start_scan:
      ESP_LOGV(LOG_TAG, "FSM state_start_scan");

      timeOutOccurred = false;
      xTimerStart(timeOutTimer, 0);
      scanForHydroBrick();
      next_state = state_scanning;
      break;

    case state_scanning:
      break;

    case state_discovered:
      ESP_LOGV(LOG_TAG, "FSM state_discovered");
      ESP_LOGI(LOG_TAG,"address = %s", hydroBrickDevice.getAddress().toString().c_str());
      ESP_LOGI(LOG_TAG,"name    = %s", hydroBrickDevice.getName().c_str());
      ESP_LOGI(LOG_TAG,"RSSI    = %d", hydroBrickDevice.getRSSI());    
      connectToHydroBrick();
      break;

    case state_connected:
      readService();
      xTimerStop(timeOutTimer, 0);

      next_state = state_result;
      break;

    case state_timeout:
      ESP_LOGV(LOG_TAG, "FSM state_timeout");
      timeOutOccurred = true;
      next_state = state_result;
      break;

    case state_result:
      switch (scanMode)
      {
      case e_scan_for_registered_brick:
      {
        controllerQMesg.valid = !timeOutOccurred;
        controllerQMesg.type = e_mtype_hydro;
        controllerQMesg.mesg.hydroMesg.mesgId = e_cmsg_hydro_reading;

        if (timeOutOccurred)
        {
          controllerQMesg.mesg.hydroMesg.data.reading.angle_x100 = 0;
          controllerQMesg.mesg.hydroMesg.data.reading.temperature_x10 = 0;
          controllerQMesg.mesg.hydroMesg.data.reading.batteryVoltage_x1000 = 0;
          controllerQMesg.mesg.hydroMesg.data.reading.RSSI = 0;
        }
        else
        {
          controllerQMesg.mesg.hydroMesg.data.reading.status = hydrometerDataBytes.data.status;
          controllerQMesg.mesg.hydroMesg.data.reading.angle_x100 = hydrometerDataBytes.data.angle_x100;
          controllerQMesg.mesg.hydroMesg.data.reading.temperature_x10 = hydrometerDataBytes.data.temperature_x10;
          controllerQMesg.mesg.hydroMesg.data.reading.batteryVoltage_x1000 = hydrometerDataBytes.data.batteryVoltage_x1000;
          controllerQMesg.mesg.hydroMesg.data.reading.SG_x1000 = angleToSG(hydrometerDataBytes.data.angle_x100);
          controllerQMesg.mesg.hydroMesg.data.reading.RSSI = hydroBrickDevice.getRSSI();
        }

        controllerQueueSend(&controllerQMesg, 0);
      }
      break;
      case e_scan_for_all_bricks:
      {
        ESP_LOGI(LOG_TAG, "Number of HydroBricks discovered : %d", scannedBricks.number);
        for (int i = 0; i < scannedBricks.number; i++)
        {
          ESP_LOGI(LOG_TAG, " addres[%d] = %s", i, scannedBricks.addresses[i].toString().c_str());
        }
      }
      break;
      }

      next_state = state_idle;
      break;

    default:
      ESP_LOGE(LOG_TAG, "FSM unknown state");
      next_state = state_idle;
      break;
    }

  } // while (true)
}

// wrapper for sendQueue
int hydroQueueSend(hydroQueueItem_t *hydroQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (hydroQueue != NULL)
  {
    r = xQueueSend(hydroQueue, hydroQMesg, xTicksToWait);
  }

  return r;
}

void initHydroBrick(void)
{
  int r;

  ESP_LOGI(LOG_TAG, "init");

  initBLE();

  // Create queue
  hydroQueue = xQueueCreate(16, sizeof(hydroQueueItem_t));
  if (hydroQueue == 0)
  {
    ESP_LOGE(LOG_TAG, "Cannot create controllerQueue.");
  }

  // Create task (must be on CORE 0 for NimBLE to function properly)
  r = xTaskCreatePinnedToCore(hydrometerTask, "hydrometerTask", 10 * 1024, NULL, 16, &hydroTaskHandle, 0);

  if (r != pdPASS)
  {
    ESP_LOGE(LOG_TAG, "Could not create task, error-code=%d", r);
  }

}

#endif
// end of file
