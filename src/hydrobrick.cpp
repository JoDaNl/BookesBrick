

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "hydrobrick.h"







static scanMode_t scanMode;
static scannedBricks_t scannedBricks;

// Queues
static xQueueHandle hydroQueue = NULL;

// Task handle
static TaskHandle_t hydroTaskHandle = NULL;

static NimBLEUUID HDhydrometerService(HDhydrometerServiceUUID);
static NimBLEUUID HDhydrometerCharacteristic(HDhydrometerCharacteristicID);

static NimBLEScan *pScan = NimBLEDevice::getScan();
static NimBLERemoteCharacteristic *pRemoteCharacteristic = NULL;
static NimBLEAdvertisedDevice *pHydroBrickDevice = NULL;
static NimBLEClient *pHydroBrickClient = NULL;

static hydrometerDataBytes_t hydrometerDataBytes;

static uint8_t hydroBrickAddressArray[] = {0xA2, 0x4B, 0xED, 0x2B, 0xCC, 0x4B};

static NimBLEAddress hydroBrickAddress(hydroBrickAddressArray, true);

// ========================================================================
// BLE init & de-init functions

static void initBLE(void)
{
  printf("[HYDRO] init BLE\n");
  NimBLEDevice::init("");
  NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9db
}

static void deInitBLE(void)
{
  printf("[HYDRO] de-init BLE\n");

  NimBLEDevice::deinit();
}

static void cleanUp(void)
{
  pScan->stop();

  //  if (pHydroBrickClient != NULL)
  {
    pHydroBrickClient->disconnect();
    //    delete pHydroBrickClient;
    //    pHydroBrickClient = NULL;
  }

  //  if (hydroBrickDevice != NULL)
  {

    //    delete hydroBrickDevice;
    //    hydroBrickDevice = NULL;
  }
}

// ========================================================================
// call-back functions

class clientConnectCallbacks : public NimBLEClientCallbacks
{
  void onConnect(NimBLEClient *pClient)
  {
    hydroQMesg qmesg;

    Serial.println("Connected");
    /** After connection we should change the parameters if we don't need fast response times.
     *  These settings are 150ms interval, 0 latency, 450ms timout.
     *  Timeout should be a multiple of the interval, minimum is 100ms.
     *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
     *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
     */
    //     pClient->updateConnParams(120, 120, 0, 60);

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
    hydroQMesg_t qmesg;

    printf("[HYDRO] Advertised Device found : %s , \"%s\"\n", advertisedDevice->getAddress().toString().c_str(), advertisedDevice->getName().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(HDhydrometerService))
    {
      switch (scanMode)
      {
      case e_scan_for_all_bricks:
      {
        printf("[HYDRO] Registered HydroBrick found!\n");
        if (scannedBricks.number < CFG_MAX_NR_HYDROBRICKS-1)
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
          if (pHydroBrickDevice == NULL)
          {
            pScan->stop();

            pHydroBrickDevice = advertisedDevice;

            qmesg.mesgId = e_msg_hydro_evt_device_discovered;
            qmesg.data = 0;
            hydroQueueSend(&qmesg, 0);

            printf("[HYDRO] Registered HydroBrick found!\n");
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
  printf("[HYDRO] Scan ended\n");
}

static advertisedDeviceCallbacks advertisedDeviceCallback;
static clientConnectCallbacks clientConnectCallBack;

// ========================================================================
//

void scanForHydroBrick(void)
{
  printf("[HYDRO] start scanning...\n");

  pScan->setAdvertisedDeviceCallbacks(&advertisedDeviceCallback, false);
  pScan->setInterval(1349);
  pScan->setWindow(449);
  pScan->setActiveScan(true);

  // Set device to NULL as indicator that no device has been discovered yet
  // Callback funtion will set the device-pointer
  pHydroBrickDevice = NULL;

  pScan->start(10);
  //  pScan->start(0, scanEndCallback);

  printf("[HYDRO] ...scanning done\n");
}

void connectToHydroBrick(void)
{
  pHydroBrickClient = NULL;

  if (pHydroBrickDevice != NULL)
  {
    printf("[HYDRO] HydroBrink connected\n");

    pHydroBrickClient = BLEDevice::createClient();
    pHydroBrickClient->setClientCallbacks(&clientConnectCallBack, false);

    // Connect to the remote BLE Server.
    pHydroBrickClient->connect(pHydroBrickDevice);
  }
}

void readService(void)
{
  BLERemoteService *pRemoteService;

  if (pHydroBrickClient != NULL)
  {
    printf("[HYDRO] read service\n");

    pRemoteService = pHydroBrickClient->getService(HDhydrometerService);

    if (pRemoteService == nullptr)
    {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(HDhydrometerCharacteristic.toString().c_str());
      pHydroBrickClient->disconnect();
      return;
    }

    printf("[HYDRO] Found service %s\n", HDhydrometerService.toString().c_str());

    pRemoteCharacteristic = pRemoteService->getCharacteristic(HDhydrometerCharacteristic);

    if (pRemoteCharacteristic == nullptr)
    {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(HDhydrometerService.toString().c_str());

      pHydroBrickClient->disconnect();
      return;
    }

    printf("[HYDRO] Found characteristic\n");

    // Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead())
    {
      std::string value;

      printf("[HYDRO] reading the characteristic...\n");

      value = pRemoteCharacteristic->readValue();

      printf("[HYDRO] number of bytes received=%d, payload=", value.length());
      for (int i = 0; i < value.length(); i++)
      {
        printf("0x%02X,", value[i]);

        // copy payload into union to form correct data-structure
        hydrometerDataBytes.bytes[i] = value[i];
      }
    }
    else
    {
      printf("[HYDRO] cannot read the characteristic !\n");
    }

    pHydroBrickClient->disconnect();
  }
}

static void timeOutTimerCallback(TimerHandle_t xTimer)
{
  hydroQMesg_t qmesg;

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
  hydroQMesg_t qMesgRecv;
  TimerHandle_t timeOutTimer;
  states_t state = state_idle;
  states_t next_state = state_idle;

  // SETUP TIMEOUT - TIMER
  timeOutTimer = xTimerCreate("timeout", 15000 / portTICK_RATE_MS, pdFALSE, 0, timeOutTimerCallback);

  initBLE();

  while (true)
  {
    // printf(".\n");

    // next_state = state;

    // RECEIVE QUEUE MESSAGE
    r = xQueueReceive(hydroQueue, &qMesgRecv, 100 / portTICK_RATE_MS);

    if (r == pdTRUE)
    {
      switch (qMesgRecv.mesgId)
      {
      case e_msg_hydro_cmd_get_reading:
      {
        printf("[HYDRO] qmesg = e_msg_hydro_cmd_get_reading\n");
        scanMode = e_scan_for_registered_brick;
        next_state = state_start_scan;
      }
      break;

      case e_msg_hydro_cmd_scan_bricks:
      {
        printf("[HYDRO] qmesg = e_msg_hydro_cmd_scan_bricks\n");
        scanMode = e_scan_for_all_bricks;
        scannedBricks.number = 0;
        next_state = state_start_scan;
      }
      break;

      case e_msg_hydro_evt_device_discovered:
      {
        printf("[HYDRO] qmesg = e_msg_hydro_device_discovered\n");
        next_state = state_discovered;
      }
      break;

      case e_msg_hydro_evt_device_connected:
      {
        printf("[HYDRO] qmesg = e_msg_hydro_device_connected\n");
        next_state = state_connected;
      }
      break;

      case e_msg_hydro_evt_timeout:
      {
        printf("[HYDRO] qmesg = e_msg_hydro_timeout\n");
        next_state = state_timeout;
      }
      break;
      }
    }

    if (next_state != state)
    {
      printf("[HYDRO] state change into : %d\n", next_state);
      state = next_state;
    }

    // STATEMACHINE
    switch (state)
    {
    case state_idle:
      //        next_state = state_idle;
      break;

    case state_start_scan:
    {
      //      if (hydroBrickDevice == NULL)
      {
        printf("[HYDRO] FSM start scanning\n");
        xTimerStart(timeOutTimer, 0);

        scanForHydroBrick();
        next_state = state_scanning;
      }
      //        next_state = state_scanning
    }
    break;

    case state_scanning:
    {
    }
    break;

    case state_discovered:
    {
      printf("[HYDRO] qmesg = e_msg_hydro_device_discovered\n");
      printf("[HYDRO] address = %s\n", pHydroBrickDevice->getAddress().toString().c_str());
      printf("[HYDRO] name    = %s\n", pHydroBrickDevice->getName().c_str());
      printf("[HYDRO] RSSI    = %d\n", pHydroBrickDevice->getRSSI());

      printf("[HYDRO] FSM connect to HydroBrick\n");
      connectToHydroBrick();
    }
    break;

    case state_connected:
    {
      readService();
      xTimerStop(timeOutTimer, 0);

      next_state = state_result;
    }
    break;

    case state_timeout:
    {
      printf("[HYDRO] FSM timeout\n");
      next_state = state_result;
    }
    break;

    case state_result:
    {
      switch (scanMode)
      {
        case e_scan_for_registered_brick:
        {
          printf("\n");
          printf("[HYDRO]    angle        = %2.2f\n", hydrometerDataBytes.data.angle_x100 / 100.0);
          printf("[HYDRO]    temperature  = %2.1f\n", hydrometerDataBytes.data.temperature_x10 / 10.0);
          printf("[HYDRO]    bat. voltage = %1.3f\n", hydrometerDataBytes.data.batteryVoltage_x1000 / 1000.0);
          printf("[HYDRO]    status       = %d\n", hydrometerDataBytes.data.status);
        }
        break;
        case e_scan_for_all_bricks:
        {
          printf("\n");
          printf("[HYDRO] Number of HydroBricks discovered : %d\n", scannedBricks.number);
          for(int i=0; i<scannedBricks.number; i++)
          {
            printf("        addres[%d] = %s\n", i, scannedBricks.addresses[i].toString().c_str());
          }        
        }
        break;
        default:
          printf("[HYDRO] error: unknown scan-mode!\n");
          break;        
      }

      // cleanUp();
      // deInitBLE();
      next_state = state_idle;
    }
    break;

    default:
    {
      printf("[HYDRO] UNKOWN STATE!\n");
      next_state = state_idle;
    }
    }

  } // while (true)
}

// wrapper for sendQueue
int hydroQueueSend(hydroQMesg_t *hydroQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (hydroQMesg != NULL)
  {
    r = xQueueSend(hydroQueue, hydroQMesg, xTicksToWait);
  }

  return r;
}

void initHydroBrick(void)
{
  printf("[CTRL] init\n");

  // Create queue
  hydroQueue = xQueueCreate(16, sizeof(hydroQMesg_t));

  if (hydroQueue == 0)
  {
    printf("[HYDRO] Cannot create controllerQueue. This is FATAL\n");
  }

  // Create task (must be on CORE 0 for NimBLE to function properly)
  xTaskCreatePinnedToCore(hydrometerTask, "hydrometerTask", 4 * 1024, NULL, 16, &hydroTaskHandle, 0);
}

// end of file
