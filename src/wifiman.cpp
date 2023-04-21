//
// wifiman.cpp
//

#include "config.h"
#include <Arduino.h>
#include <preferences.h>

// the following defines MUST be defined BEFORE the DRD include file!
#if (CFG_COMM_WM_USE_DRD == true)
#define ESP_DRD_USE_LITTLEFS false
#define ESP_DRD_USE_SPIFFS true
#define ESP_DRD_USE_EEPROM false
#define DOUBLERESETDETECTOR_DEBUG true
#include <ESP_DoubleResetDetector.h>
#endif

#include <WifiManager.h>
#include <ESP32ping.h>
#include "wifiman.h"
#include "i2c_lcd_16x2.h"


// module scope
static WiFiManager wm;
static bool wmSaveConfig = false;
static uint64_t chipId;
static bool initWifiDone = false;

#if (CFG_COMM_WM_USE_DRD == true)
static DoubleResetDetector *drd;
#endif

// PREFERENCES
String apiKey;
static Preferences prefs;

// ============================================================================
// WIFIMANAGER CALLBACK
// ============================================================================

static void saveWMConfigCallback()
{
  printf("[WIFIMAN] WiFiManager callback : save config\n");
  wmSaveConfig = true;
}

// ============================================================================
// INIT WIFI
// ============================================================================

static void initWiFi(void)
{
  static bool status;
  static displayQueueItem_t displayQMesg;
  
#if (CFG_COMM_WM_USE_PIN == true)
  pinMode(CFG_COMM_WM_PORTAL_PIN, INPUT_PULLDOWN); // "open config portal" request-pin
#endif

  // Message for display task
  displayQMesg.type = e_wifiInfo;
  displayQMesg.index = 0;
  displayQMesg.duration = 5; // seconds
  
  // read 'preferences'
  prefs.begin(BBPREFS, true);               // read only mode
  apiKey = prefs.getString(BBAPIKEYID, ""); // leave default empty
  printf("[WIFIMAN] apiKey read from preferences=%s\n", apiKey.c_str());
  prefs.end();


  // ===========================================================
  // WIFIMANAGER
  // ===========================================================

  // Optionally reset WifiManager settings
#if (CFG_COMM_WM_RESET_SETTINGS == true)
  printf("[WIFIMAN] RESETTING ALL WIFIMANAGER SETTINGS!!!\n");
  wm.resetSettings();
#endif

  // Optionally silence WiFiManager...to chatty at startup
  wm.setDebugOutput(CFG_COMM_WM_DEBUG);
#if (CFG_COMM_WM_DEBUG == true)
  printf("[WIFIMAN] WIFIMANAGER DEBUG INFO ON\n");
  wm.debugPlatformInfo();
  wm.debugSoftAPConfig();
#endif


  // ===========================================================
  // START WIFI
  // ===========================================================

  // Add custom "BierBot Bricks API Key" parameter to WiFiManager, max length = 32
  static WiFiManagerParameter bbApiKey(BBAPIKEYID, BBAPIKEYLABEL, apiKey.c_str(), 32);
  
#if (CFG_COMM_WM_USE_DRD == true)
  // Double Reset detection
  drd = new DoubleResetDetector(BBDRDTIMEOUT, 0); // TODO : static declaraion (not pointer..so no NEW required)

  if (drd->detectDoubleReset())
  {
    // Start configuration portal
    printf("\n\n[WIFIMAN] Double Reset Detected...\n");
#endif

#if (CFG_COMM_WM_USE_PIN == true)

  if ( digitalRead(CFG_COMM_WM_PORTAL_PIN) )
  {
    // Start configuration portal
    printf("\n\n[WIFIMAN] Wifi Configuration button pressed...\n");
#endif

    printf("[WIFIMAN] ...starting config portal\n\n");


    // Send info to display-task
    // if (displayQueue)
    // {
    //   strcpy(displayQMesg.data.wifiInfo, (char *)"Config Mode");
    //   xQueueSend(displayQueue, &displayQMesg, 0);
    // }

    // WiFimanager settings
    wm.addParameter(&bbApiKey);
    wm.setSaveConfigCallback(saveWMConfigCallback);
    wm.setConfigPortalBlocking(true); // make portal blocking
    wm.setConfigPortalTimeout(BBPORTALTIMEOUT);
    wm.setCaptivePortalEnable(true);
    wm.setEnableConfigPortal(false);
    wm.setHostname(CFG_COMM_HOSTNAME);

    

    // Start the config portal
    status = wm.startConfigPortal(CFG_COMM_SSID_PORTAL);
    printf("[WIFIMAN] Portal closed. Status=%d\n\n", status);

    if (wmSaveConfig)
    {
      // Save custom parameter to 'preferences'
      apiKey = bbApiKey.getValue();
      printf("[WIFIMAN] saving custom parameter(s)\n");
      printf("[WIFIMAN] apiKey=%s\n", apiKey.c_str());
      prefs.begin(BBPREFS, false);
      prefs.putString(BBAPIKEYID, apiKey);
      prefs.end();
    }

    vTaskDelay(1000 / portTICK_RATE_MS);
    ESP.restart();
    
  }
  else
  {
    printf("[WIFIMAN] no DRD, WiFiManager Autoconnect started..\n");
    status = wm.autoConnect(CFG_COMM_SSID_PORTAL);
    printf("[WIFIMAN] WiFimanager autoconnect status=%d\n", status);
    if (displayQueue)
    {
      strcpy(displayQMesg.data.wifiSSID, WiFi.SSID().c_str());
      strcpy(displayQMesg.data.wifiIP, WiFi.localIP().toString().c_str());
      xQueueSend(displayQueue, &displayQMesg, 0);
    }

  }


#ifdef BBPINGURL
  // ===========================================================
  // CHECK FOR INTERNET CONNECTION, PING GOOGLE
  // ===========================================================

  // At this point in the code we should have WiFi connection...check if we can reach the internet
  status = Ping.ping(BBPINGURL, 3);

  if (status)
  {
    printf("[WIFIMAN] We have internet connection !!!\n");
  }
  else
  {
    printf("[WIFIMAN] We have NO internet connection !!!\n");
  }
#endif

  // set flag for task (is atomic write)
  initWifiDone = true;
  printf("[WIFIMAN] initWifi() DONE\n");
}

// ============================================================================
// WIFIMAN TASK
// ============================================================================


static void wifimanTask(void *arg)
{
  static bool status;
  static displayQueueItem_t displayQMesg;
  static uint8_t ledOn = 0;

  // Message for display task
  displayQMesg.type = e_wifiInfo;
  displayQMesg.index = 0;
  displayQMesg.duration = 10; // seconds

  // set LED as ouput
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, ledOn);

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
      drd->loop();
#endif      
    }
    else
    {
      // wifi is being initisalised, blink LED faster
      // loop delay
      vTaskDelay(100 / portTICK_RATE_MS);
    }

    ledOn = !ledOn;
    digitalWrite(LED_BUILTIN, ledOn);

    //printf("[WIFIMAN] PORTAL-PIN=%d\n",digitalRead(PORTAL_PIN));
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
  xTaskCreatePinnedToCore(wifimanTask, "wifimanTask", 4096, NULL, 10, &wifimanTaskHandle, 0);

  // start Wifi
  initWiFi();
}

// end of file
 