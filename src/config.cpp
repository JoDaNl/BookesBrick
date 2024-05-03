//
// config.cpp
//

#include <Arduino.h>
#include <preferences.h>
#include <ESP32ping.h>
#include <LittleFS.h>
#include <ESPAsyncDNSServer.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "controller.h"

// the following defines MUST be defined BEFORE the DRD include file!
#if (CFG_COMM_WM_USE_DRD == true)
#define ESP_DRD_USE_LITTLEFS true
#define ESP_DRD_USE_SPIFFS false
#define ESP_DRD_USE_EEPROM false
#define DOUBLERESETDETECTOR_DEBUG true
#include <ESP_DoubleResetDetector.h>
#endif

#include "config.h"

#define LOG_TAG "config"

// module scope
static bool wmSaveConfig = false;
static TaskHandle_t wifiCheckTaskHandle = NULL;
static bool _startCaptivePortal;

#if (CFG_COMM_WM_USE_DRD == true)
static TaskHandle_t drdTaskHandle = NULL;
#endif

#if (CFG_COMM_WM_USE_DRD == true)
static DoubleResetDetector *drd;
#endif

// Config struct
configValues_t config;

static Preferences prefs;

AsyncWebServer webServer(80);

// DNS server
const byte DNS_PORT = 53;
AsyncDNSServer dnsServer;

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
    ESP_LOGI(LOG_TAG, "\nDouble Reset Detected...\n");
    configMode = true;
  }
#endif

#if (CFG_COMM_WM_USE_PIN == true)
  // set pin to input with pull-up
  pinMode(CFG_COMM_WM_PORTAL_PIN, INPUT_PULLUP);
  vTaskDelay(100 / portTICK_RATE_MS);

  // check if button was pressed during boot
  if (!digitalRead(CFG_COMM_WM_PORTAL_PIN))
  {
    // wait 1 second & check button again
    vTaskDelay(1000 / portTICK_RATE_MS);
    if (!digitalRead(CFG_COMM_WM_PORTAL_PIN))
    {
      ESP_LOGI(LOG_TAG,("\nWifi Configuration button pressed...\n");
      configMode = true;
    }
  }
#endif

  if (configMode)
  {
    ESP_LOGI(LOG_TAG, "\nSTARTING IN CONFIG MODE\n");
  }
  else
  {
    ESP_LOGI(LOG_TAG, "\nSTARTING IN NORMAL MODE\n");
  }

  return configMode;
}

// ============================================================================
// DRD TASK
// ============================================================================
#if (CFG_COMM_WM_USE_DRD == true)
static void drdTask(void *arg)
{

  ESP_LOGI(LOG_TAG, "Entering DRD loop...\n");

  // TASK LOOP
  while (true)
  {
    drd->loop();

    // loop delay
    vTaskDelay(2000 / portTICK_RATE_MS);
  }
};

static void initDRD(void)
{
  bool status;

  ESP_LOGI(LOG_TAG, "initDRD..");

  // create task first...in order to have LED blinking
  xTaskCreate(drdTask, "drdTask", 1 * 1024, NULL, 10, &drdTaskHandle);
}
#endif

// ===========================================================
// READ CONFIG SETTINGS FROM EEPROM
// ===========================================================

static void printConfig(void)
{
  ESP_LOGI(LOG_TAG, "   apiKey=%s", config.apiKey.c_str());
  ESP_LOGI(LOG_TAG, "   proApiKey=%s", config.proApiKey.c_str());
  ESP_LOGI(LOG_TAG, "   SSID=%s", config.SSID.c_str());
  ESP_LOGI(LOG_TAG, "   passwd=%s", config.passwd.c_str());
  ESP_LOGI(LOG_TAG, "   hostname=%s", config.hostname.c_str());
}

static void readConfig(void)
{
  prefs.begin(BBPREFS, true); // read only mode
  config.apiKey = prefs.getString(BBPREFS_APIKEY, "");
  config.proApiKey = prefs.getString(BBPREFS_PROAPIKEY, "");
  config.SSID = prefs.getString(BBPREFS_SSID, "");
  config.passwd = prefs.getString(BBPREFS_PASSWD, "");
  config.hostname = prefs.getString(BBPREFS_HOSTNAME, CFG_COMM_HOSTNAME);
  prefs.end();

  printf("[CONFIG] readConfig\n");
  printConfig();
}

// ===========================================================
// WRITE CONFIG SETTINGS TO EEPROM
// ===========================================================

static void writeConfig(void)
{
  printf("[CONFIG] writeConfig()\n");
  printConfig();

  prefs.begin(BBPREFS, false); // write mode
  prefs.putString(BBPREFS_APIKEY, config.apiKey.c_str());
  prefs.putString(BBPREFS_PROAPIKEY, config.proApiKey.c_str());
  prefs.putString(BBPREFS_SSID, config.SSID.c_str());
  prefs.putString(BBPREFS_PASSWD, config.passwd.c_str());
  prefs.putString(BBPREFS_HOSTNAME, config.hostname.c_str());
  prefs.end();
}

// ===========================================================
// LIST DIRECTORY (LITTLEFS)
// ===========================================================

static void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  File root;
  File file;

  ESP_LOGI(LOG_TAG, "Listing directory: %s\r", dirname);
  root = fs.open(dirname);

  if (!root)
  {
    ESP_LOGE(LOG_TAG, "- failed to open directory");
    return;
  }

  if (!root.isDirectory())
  {
    ESP_LOGE(LOG_TAG, " - not a directory");
    return;
  }

  file = root.openNextFile();

  while (file)
  {
    if (file.isDirectory())
    {
      ESP_LOGI(LOG_TAG, "  DIR : %s" file.name());
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
      ESP_LOGI(LOG_TAG, "  FILE: %s SIZE: %d", file.name(), file.size());
    }
    file = root.openNextFile();
  }
}

static void printParams(AsyncWebServerRequest *request)
{
  AsyncWebParameter *param;
  int num;

  num = request->params();

  for (int i = 0; i < num; i++)
  {
    param = request->getParam(i);
    printf("param %s = %s", param->name().c_str(), param->value().c_str());
  }
}

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    printf("[CONFIG] Handlerequest url=%s\n", request->url().c_str());
    request->redirect("/config.html");
  }
};

String htmlProcessor(const String &str)
{
  printf("[CONFIG] html_processor: %s\n", str);

  if (str == "apikey")
  {
    return config.apiKey;
  }
  if (str == "proapikey")
  {
    return config.proApiKey;
  }
  if (str == "SSID")
  {
    return config.SSID;
  }
  if (str == "passwd")
  {
    return config.passwd;
  }
  if (str == "hostname")
  {
    return config.hostname;
  }

  return String(); // return emty String
}

void startConfigPortal()
{

  printf("Setting Access Point\n");
  WiFi.softAP(CFG_COMM_SSID_PORTAL, NULL);

  IPAddress IP = WiFi.softAPIP();
  printf("AP IP address: %s\n", IP.toString().c_str());

  // handle form POST
  webServer.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
               {
                 int numParams = request->params();

                 // check if submit button has been pressed
                 if (request->hasArg("submit"))
                 {
                   printf("[CONFIG] submit pressed\n");
                   for (int i = 0; i < numParams; i++)
                   {
                     AsyncWebParameter *p = request->getParam(i);
                     if (p->isPost())
                     {
                       String pName = p->name();
                       String pValue = p->value();

                       pValue.trim(); // remove any leading & trailing spaces

                       printf("[CONFIG] config-setting %s set to: %s\n", pName.c_str(), pValue.c_str());

                       if (pName.equals("apikey"))
                       {
                         config.apiKey = pValue;
                       }

                       if (pName.equals("proapikey"))
                       {
                         config.proApiKey = pValue;
                       }

                       if (pName.equals("SSID"))
                       {
                         config.SSID = pValue;
                       }

                       if (pName.equals("passwd"))
                       {
                         config.passwd = pValue;
                       }
                       if (pName.equals("hostname"))
                       {
                         config.hostname = pValue;
                       }
                     }
                   }

                   writeConfig();
                   request->send(200, "text/plain", "Values saved. Bookus Brick will now restart");

                   printf("[CONFIG] RESTARTING...\n");
                   vTaskDelay(3000 / portTICK_RATE_MS);
                   ESP.restart();
                 }

                 // check if cancel button has been pressed
                 if (request->hasArg("cancel"))
                 {
                   request->send(200, "text/plain", "Cancelled. Bookus Brick will now restart");

                   printf("[CONFIG] cancel pressed\n");
                   printf("[CONFIG] RESTARTING...\n");
                   vTaskDelay(3000 / portTICK_RATE_MS);
                   ESP.restart();
                 }

                 request->send(200, "text/plain", "Unknown");
               });

  webServer.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/config.html", String(), false, htmlProcessor); });

  webServer.serveStatic("/", LittleFS, "/");

  webServer.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP

  webServer.onNotFound([](AsyncWebServerRequest *request)
                       {
      printf("[CONFIG] webpage not found: %s\n", request->url().c_str());
      request->send(404, "text/plain", "404: Not found"); });

  // start DNS server.
  // This will redirect all DNS-queries towards the AP
  dnsServer.setTTL(300);
  dnsServer.start(DNS_PORT, "*", IP);

  // start web server
  webServer.begin();
}

static void startWifi()
{
  bool status;

  printf("\n");
  printf("[CONFIG] Connecting to WIFI...\n");
  printf("[CONFIG] hostname=<%s>\n", config.hostname.c_str());
  printf("[CONFIG]     SSID=<%s>\n", config.SSID.c_str());
  printf("[CONFIG]   passwd=<%s>\n", config.passwd.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config.hostname.c_str());
  WiFi.begin(config.SSID, config.passwd);

  vTaskDelay(1000 / portTICK_RATE_MS);

  while (WiFi.status() != WL_CONNECTED)
  {
    printf(".");
    WiFi.reconnect();
    vTaskDelay(100 / portTICK_RATE_MS);
  }
  printf("\n\n");

  printf("[CONFIG] WIFI connected, IP-address: %s\n", WiFi.localIP().toString().c_str());

#ifdef BBPINGURL
  // We should have WiFi connection...check if we can reach the internet

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
}

// ===========================================================
// WIFI CHECK TASK
// ===========================================================

static void wifiCheckTask(void *arg)
{
  static bool status;
  static int16_t rssi;
  static controllerQItem_t WiFiMesg;

  printf("[CONFIG] Entering task loop...\n");

  if (_startCaptivePortal)
  {
    startConfigPortal();
  }
  else
  {
    startWifi();
  }

  // TASK LOOP
  while (true)
  {
    // Check WiFi status..only in normal operation  mode...do not check Wifi when portal is open
    if (!config.inConfigMode)
    {
      // printf("[CONFIG] Check WIFI...\n");

      if (WiFi.status() != WL_CONNECTED)
      {
        printf("[CONFIG] Reconnecting to WiFi...\n");
        WiFi.reconnect();
      }

      rssi = WiFi.RSSI();
      // send RSSI to controller
      WiFiMesg.type = e_mtype_wifi;
      WiFiMesg.mesg.WiFiMesg.mesgId = e_msg_WiFi_rssi;
      WiFiMesg.mesg.WiFiMesg.data = rssi;
      controllerQueueSend(&WiFiMesg, 0);      
    }

    // loop delay
    vTaskDelay(5000 / portTICK_RATE_MS);
  }
};

// ============================================================================
// INIT WIFI
// ============================================================================
// See : https://microcontrollerslab.com/esp32-wi-fi-manager-asyncwebserver/
// See : https://iotespresso.com/create-captive-portal-using-esp32/
// See : https://gist.github.com/SBajonczak/737310ee2c2360439e0daa75cad805b2

void initWiFi(bool startCaptivePortal)
{
  printf("[CONFIG] initWifi\n");

  _startCaptivePortal = startCaptivePortal;

  if (!LittleFS.begin(true))
  {
    Serial.println("[CONFIG] Error mounting LittleFS");
  }

  Serial.println("[CONFIG] available files:");
  listDir(LittleFS, "/", 2);

  readConfig();

#if (CFG_COMM_WM_USE_DRD == true)
  initDRD();
#endif

  // create task first...in order to have LED blinking
  xTaskCreate(wifiCheckTask, "wifiCheckTask", 2 * 1024, NULL, 10, &wifiCheckTaskHandle);

  printf("[CONFIG] initWifi() DONE\n");
}

// end of file
