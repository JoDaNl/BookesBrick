//
// wifiman.cpp
//

#include "config.h"
#include <Arduino.h>
#include <preferences.h>

//#include <WifiManager>
#include <ESPAsync_WiFiManager_lite.h>
#include "wifiman.h"


// module scope
static WiFiManager wm;
static bool wmSaveConfig = false;
static uint64_t chipId;
static bool initWifiDone = false;


// ============================================================================
// WIFIMAN TASK
// ============================================================================

static void wifimanTask(void *arg)
{
  static bool status;
  // static displayQueueItem_t displayQMesg;

  // Message for display task
  // displayQMesg.type = e_wifiInfo;
  // displayQMesg.index = 0;
  // displayQMesg.duration = 10; // seconds


  // ===========================================================
  // TASK LOOP
  // ===========================================================


  printf("[WIFIMAN] Entering task loop...\n");

  // TASK LOOP
  while (true)
  {
    if (initWifiDone)
    {
      // Check WiFi status
      if (WiFi.status() != WL_CONNECTED)
      {
        printf("[WIFIMAN] Reconnecting to WiFi...\n");
        WiFi.reconnect();
      }

      // loop delay
      vTaskDelay(1000 / portTICK_RATE_MS);
#if (CFG_COMM_WM_USE_DRD == true)      
      printf("[WIFIMAN] drd->loop()\n");
//      drd->loop();
#endif      
    }
    else
    {
      // wifi is being initisalised, blink LED faster
      // loop delay
      vTaskDelay(100 / portTICK_RATE_MS);
    }
  }

};


// ============================================================================
// INIT WIFIMAN
// ============================================================================

void initWifiMan(void)
{
  static TaskHandle_t wifimanTaskHandle = NULL;

  printf("[WIFIMAN] init\n");

  // create task first...in order to have LED blinking
  xTaskCreate(wifimanTask, "wifimanTask", 4096, NULL, 10, &wifimanTaskHandle);

}

// end of file
 