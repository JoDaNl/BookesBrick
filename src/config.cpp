//
// config.cpp
//

#include <Arduino.h>
#include <preferences.h>
#include <ESP32ping.h>
#include <WifiManager.h>

#include "config.h"

// the following defines MUST be defined BEFORE the DRD include file!
#if (CFG_COMM_WM_USE_DRD == true)
#define ESP_DRD_USE_LITTLEFS false
#define ESP_DRD_USE_SPIFFS false
#define ESP_DRD_USE_EEPROM true
#define DOUBLERESETDETECTOR_DEBUG true
#include <ESP_DoubleResetDetector.h>
#endif

#include "config.h"



// module scope
static WiFiManager wm;
static bool wmSaveConfig = false;
static TaskHandle_t wifiCheckTaskHandle = NULL;

#if (CFG_COMM_WM_USE_DRD == true)
static TaskHandle_t drdTaskHandle = NULL;
#endif

#if (CFG_COMM_WM_USE_DRD == true)
static DoubleResetDetector *drd;
#endif

// Config struct
configValues_t config;

static Preferences prefs;



// ============================================================================
// CHECK BOOT CONFIG MODE
// ============================================================================

bool checkBootConfigMode(void)
{
  bool configMode = false;

#if (CFG_COMM_WM_USE_DRD == true)
  // Double Reset detection
  drd = new DoubleResetDetector(BBDRDTIMEOUT, 0); // TODO : static declaraion (not pointer..so no NEW required)

  if (drd->detectDoubleReset())
  {
    printf("\n\n[CONFIG] Double Reset Detected...\n");
    configMode = true;
  }
#endif

#if (CFG_COMM_WM_USE_PIN == true)
  // set pin to input with pull-up
  pinMode(CFG_COMM_WM_PORTAL_PIN, INPUT_PULLUP); 
  vTaskDelay(100 / portTICK_RATE_MS); 

  // check if button was pressed during boot
  if ( !digitalRead(CFG_COMM_WM_PORTAL_PIN) )
  {
    // wait 1 second & check button again
    vTaskDelay(1000 / portTICK_RATE_MS); 
    if ( !digitalRead(CFG_COMM_WM_PORTAL_PIN) )
    {
      printf("\n\n[CONFIG] Wifi Configuration button pressed...\n");
      configMode = true;
    }
  }
#endif

  if (configMode)
  {
    printf("\n\n[CONFIG] STARTING IN CONFIG MODE\n");
  }
  else
  {
    printf("\n\n[CONFIG] STARTING IN NORMAL MODE\n");
  }


  return configMode;
}


// ============================================================================
// DRD TASK
// ============================================================================
#if (CFG_COMM_WM_USE_DRD == true)      
static void drdTask(void *arg)
{

  printf("[CONFIG] Entering DRD loop...\n");

  // TASK LOOP
  while (true)
  {
//    printf("[CONFIG] drd.loop \n");
    drd->loop();

    // loop delay
    vTaskDelay(2000 / portTICK_RATE_MS);
  }

};

static void initDRD(void)
{
  bool status;

  printf("[CONFIG] initDRD..\n");

  // create task first...in order to have LED blinking
  xTaskCreate(drdTask, "drdTask", 4096, NULL, 10, &drdTaskHandle);

}
#endif      


// ===========================================================
// READ CONFIG SETTINGS FROM EEPROM
// ===========================================================

static void readConfigValues(void)
{
  prefs.begin(BBPREFS, true);               // read only mode
  config.apiKey = prefs.getString(BBAPIKEYID, ""); // leave default empty
  printf("[CONFIG] apiKey read from preferences=%s\n", config.apiKey.c_str());
  prefs.end();
}

// ===========================================================
// WRITE CONFIG SETTINGS TO EEPROM
// ===========================================================

static void writeConfigValues(void)
{
  printf("[CONFIG] saving configuration values\n");
  printf("[CONFIG]  - apiKey=%s\n", config.apiKey.c_str());

  prefs.begin(BBPREFS, false); // write mode
  prefs.putString(BBAPIKEYID, config.apiKey.c_str());
  prefs.end();
}

// ============================================================================
// WIFIMANAGER CALLBACK
// ============================================================================

static void saveConfigCallback()
{
  printf("[CONFIG] WiFiManager callback : save config\n");
  wmSaveConfig = true;
}


// ==========================================================
// START WIFIMANAGER CONFIG PORTAL
// ==========================================================

void initConfigPortal(void)
{
  static bool status;
  
  printf("[CONFIG] Starting config portal\n\n");

  initDRD();
//  readConfigValues();

  // Optionally reset WifiManager settings
#if (CFG_COMM_WM_RESET_SETTINGS == true)
  printf("[CONFIG] RESETTING ALL WIFIMANAGER SETTINGS!!!\n");
  wm.resetSettings();
#endif

  // Optionally silence WiFiManager...to chatty at startup
  wm.setDebugOutput(CFG_COMM_WM_DEBUG);
#if (CFG_COMM_WM_DEBUG == true)
  printf("[CONFIG] WIFIMANAGER DEBUG INFO ON\n");
  wm.debugPlatformInfo();
  wm.debugSoftAPConfig();
#endif

  // Add custom "BierBot Bricks API Key" parameter to WiFiManager, max length = 32
  static WiFiManagerParameter bbApiKey(BBAPIKEYID, BBAPIKEYLABEL, config.apiKey.c_str(), 32);

  // WiFimanager settings
  wm.addParameter(&bbApiKey);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalBlocking(true); // make portal blocking
  wm.setConfigPortalTimeout(BBPORTALTIMEOUT);
  wm.setCaptivePortalEnable(true);
  wm.setEnableConfigPortal(false);
  wm.setHostname(CFG_COMM_HOSTNAME);

  // Start the config portal / blocking call
  status = wm.startConfigPortal(CFG_COMM_SSID_PORTAL);
  printf("[CONFIG] Portal closed. Status=%d\n\n", status);
  vTaskDelay(2000 / portTICK_RATE_MS);

  if (wmSaveConfig)
  {
    // Save custom parameter to 'preferences'   
    config.apiKey = bbApiKey.getValue();
    printf("[CONFIG got api-key from portal: %s\n",config.apiKey.c_str());
    writeConfigValues();   
  }

  vTaskDelay(2000 / portTICK_RATE_MS);
  ESP.restart();
    
}




// ============================================================================
// WIFI CHECK TASK
// ============================================================================

static void wifiCheckTask(void *arg)
{
  static bool status;

  printf("[CONFIG] Entering task loop...\n");

  // TASK LOOP
  while (true)
  {
    // Check WiFi status..only in normal operation  mode...do not check Wifi when portal is open
    if (!config.inConfigMode)
    {
//      printf("[CONFIG] Check WIFI...\n");

      if (WiFi.status() != WL_CONNECTED)
      {
        printf("[CONFIG] Reconnecting to WiFi...\n");
        WiFi.reconnect();
      }
    }

    // loop delay
    vTaskDelay(2000 / portTICK_RATE_MS);
  }

};





// ============================================================================
// INIT WIFI
// ============================================================================

void initWiFi(void)
{
  bool status;

  printf("[CONFIG] WiFiManager Autoconnect started..\n");
  status = wm.autoConnect(CFG_COMM_SSID_PORTAL);
  printf("[CONFIG] WiFimanager autoconnect status=%d\n", status);

  initDRD();

  // config.apiKey = "123456AB";
  // vTaskDelay(2000 / portTICK_RATE_MS);
  // writeConfigValues();
  // vTaskDelay(2000 / portTICK_RATE_MS);
  readConfigValues();
  // vTaskDelay(2000 / portTICK_RATE_MS);


#ifdef BBPINGURL
  // ===========================================================
  // CHECK FOR INTERNET CONNECTION, PING GOOGLE
  // ===========================================================

  // At this point in the code we should have WiFi connection...check if we can reach the internet
  status = Ping.ping(BBPINGURL, 3);

  if (status)
  {
    printf("[CONFIG] We have internet connection !!!\n");
  }
  else
  {
    printf("[CONFIG] We have NO internet connection !!!\n");
  }
#endif


  // create task first...in order to have LED blinking
  xTaskCreate(wifiCheckTask, "wifiCheckTask", 4096, NULL, 10, &wifiCheckTaskHandle);


  printf("[CONFIG] initWifi() DONE\n");
}











// end of file
 