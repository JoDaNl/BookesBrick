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

#define DELAY (100)                   // Loop delay in milliseconds
#define RSSI_COUNT (1 * 1000 / DELAY) // Send WiFi info to controlller every n loop-interations
#define PING_COUNT (30)               // Do a ping ever n WiFi checks

// Queues
static xQueueHandle WiFiQueue = NULL;

// module scope
static bool wmSaveConfig = false;
// static TaskHandle_t wifiCheckTaskHandle = NULL;
static TaskHandle_t WiFiTaskHandle = NULL;
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
  xTaskCreateStaticPinnedToCore(drdTask, "drdTask", 1 * 1024, NULL, 10, &drdTaskHandle, 1);
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

void readConfig(void)
{
  // portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  // taskENTER_CRITICAL(&myMutex);

  // critical section
  prefs.begin(BBPREFS, true); // read only mode
  config.apiKey = prefs.getString(BBPREFS_APIKEY, "");
  config.proApiKey = prefs.getString(BBPREFS_PROAPIKEY, "");
  config.SSID = prefs.getString(BBPREFS_SSID, "");
  config.passwd = prefs.getString(BBPREFS_PASSWD, "");
  config.hostname = prefs.getString(BBPREFS_HOSTNAME, CFG_COMM_HOSTNAME);
  prefs.end();

  // taskEXIT_CRITICAL(&myMutex);

  printf("[CONF] readConfig\n");
  printConfig();
}

// ===========================================================
// WRITE CONFIG SETTINGS TO EEPROM
// ===========================================================

static void writeConfig(void)
{
  printf("[CONF] writeConfig()\n");
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
    printf("[CONF] Handlerequest url=%s\n", request->url().c_str());
    request->redirect("/config.html");
  }
};

String htmlProcessor(const String &str)
{
  printf("[CONF] html_processor: %s\n", str);

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
                   printf("[CONF] submit pressed\n");
                   for (int i = 0; i < numParams; i++)
                   {
                     AsyncWebParameter *p = request->getParam(i);
                     if (p->isPost())
                     {
                       String pName = p->name();
                       String pValue = p->value();

                       pValue.trim(); // remove any leading & trailing spaces

                       printf("[CONF] config-setting %s set to: %s\n", pName.c_str(), pValue.c_str());

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

                   printf("[CONF] RESTARTING...\n");
                   vTaskDelay(3000 / portTICK_RATE_MS);
                   ESP.restart();
                 }

                 // check if cancel button has been pressed
                 if (request->hasArg("cancel"))
                 {
                   request->send(200, "text/plain", "Cancelled. Bookus Brick will now restart");

                   printf("[CONF] cancel pressed\n");
                   printf("[CONF] RESTARTING...\n");
                   vTaskDelay(3000 / portTICK_RATE_MS);
                   ESP.restart();
                 }

                 request->send(200, "text/plain", "Unknown"); });

  webServer.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/config.html", String(), false, htmlProcessor); });

  webServer.serveStatic("/", LittleFS, "/");

  webServer.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP

  webServer.onNotFound([](AsyncWebServerRequest *request)
                       {
      printf("[CONF] webpage not found: %s\n", request->url().c_str());
      request->send(404, "text/plain", "404: Not found"); });

  // start DNS server.
  // This will redirect all DNS-queries towards the AP
  dnsServer.setTTL(300);
  dnsServer.start(DNS_PORT, "*", IP);

  // start web server
  webServer.begin();
}

void stopConfigPortal()
{
  webServer.end();
  dnsServer.stop();
  WiFi.disconnect();
}

static void startWiFi()
{
  bool status;

  printf("\n");

  if (config.SSID.length() > 1 && config.passwd.length() > 1)
  {
    printf("[CONF] Connecting to WIFI...\n");
    printf("[CONF] hostname=<%s>\n", config.hostname.c_str());
    printf("[CONF]     SSID=<%s>\n", config.SSID.c_str());
    printf("[CONF]   passwd=<%s>\n", config.passwd.c_str());

    WiFi.setHostname(config.hostname.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.begin(config.SSID, config.passwd);
  }
  else
  {
    printf("[CONF] NO VALID WIFI CONFIGURATION !!!\n");
  }
}

static void stopWiFi(void)
{
}

static void WiFiEventHandler(WiFiEvent_t event)
{
  static WiFiQueueItem_t queueItem;

  queueItem.mesgId = e_event;
  queueItem.event = event;

  //  WiFiQueueSend(&queueItem, 0);

  if (WiFiQueue != NULL)
  {
    xQueueSendFromISR(WiFiQueue, &queueItem, NULL);
  }
}

typedef enum WiFiModeRequest
{
  REQUEST_STOP,
  REQUEST_WIFI,
  REQUEST_PORTAL
} WiFiModeRequest_t;

typedef enum WiFiState
{
  STATE_STOP,
  STATE_WIFI_REQUESTED,
  STATE_WIFI_CONNECTED,
  STATE_PORTAL_REQUESTED,
  STATE_PORTAL_CONNECTED
} WiFiState_t;



static void WiFiTask(void *arg)
{
  static uint16_t status_count;
  static uint16_t ping_count;
  static controllerQItem_t WiFiMesg;
  static WiFiQueueItem_t queueItem;
  static WiFiModeRequest_t request;
  static WiFiState_t state;

  status_count = 2; // after boot check
  ping_count = 2;   // after boot & connection check

  request = REQUEST_STOP;
  state = STATE_STOP;

  // one-time WiFi configuration stuff
  WiFi.onEvent(WiFiEventHandler);
  WiFi.setHostname(config.hostname.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); 
  WiFi.begin(config.SSID, config.passwd);

  printf("[WiFi] Entering task loop...\n");

  // TASK LOOP
  while (true)
  {

    if (xQueueReceive(WiFiQueue, &queueItem, 100 / portTICK_RATE_MS) == pdTRUE)
    {
      printf("[WIFI] message received : %d\n", queueItem.mesgId);

      switch (queueItem.mesgId)
      {
      case e_cmd_start_wifi:
        request = REQUEST_WIFI;
        break;

      case e_cmd_start_portal:
        request = REQUEST_PORTAL;
        break;

      case e_cmd_stop:
        request = REQUEST_STOP;
        break;

      case e_event:
        printf("[WIFI] event reveived : %s\n", WiFi.eventName(queueItem.event));

        switch (queueItem.event)
        {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          printf("[WIFI] WIFI connected, IP-address: %s\n", WiFi.localIP().toString().c_str());
          state = STATE_WIFI_CONNECTED;
          break;

        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
          state = STATE_STOP;
          break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
          break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
          state = STATE_STOP;
          break;
        }
      }


// #ifdef BLABLA
    printf("[WIFI] state=");
    switch (state)
    {
    case STATE_STOP:
      printf("STATE_STOP");
      break;
    case STATE_WIFI_REQUESTED:
      printf("STATE_WIFI_REQUESTED");
      break;
    case STATE_WIFI_CONNECTED:
      printf("STATE_WIFI_CONNECTED");
      break;
    case STATE_PORTAL_REQUESTED:
      printf("STATE_PORTAL_REQUESTED");
      break;
    case STATE_PORTAL_CONNECTED:
      printf("STATE_PORTAL_CONNECTED");
      break;
    }
    printf(", request=");
    switch (request)
    {
    case REQUEST_STOP:
      printf("REQUEST_TOP");
      break;
    case REQUEST_WIFI:
      printf("REQUEST_WIFI");
      break;
    case REQUEST_PORTAL:
      printf("REQUEST_PORTAL");
      break;
    }
    printf(", wifi.status()=%d\n",WiFi.status());
// #endif

    } // if (xQueueReceive(WiFiQueue


    switch (state)
    {
    case STATE_STOP:
      if (request == REQUEST_WIFI)
      {
        state = STATE_WIFI_REQUESTED;

//         WiFi.disconnect();

        // TODO : add check for valid config

        // WiFi.mode(WIFI_STA);
        // WiFi.begin(config.SSID, config.passwd);
        vTaskDelay(1000 / portTICK_RATE_MS);
      }

      if (request == REQUEST_PORTAL)
      {

      }

      break;

    case STATE_WIFI_REQUESTED:
      break;

    case STATE_WIFI_CONNECTED:
      if (request == REQUEST_STOP)
      {
//        WiFi.disconnect();
//        vTaskDelay(1000 / portTICK_RATE_MS);
      }

      if (request == REQUEST_PORTAL)
      {

      }
      break;

    case STATE_PORTAL_REQUESTED:
      break;

    case STATE_PORTAL_CONNECTED:
      break;
    }

    // Send WiFi status info to controller
    if (status_count == 0)
    {
      // if (state == STATE_WIFI_CONNECTED)
      if (WiFi.status() == WL_CONNECTED)
      {
        // WiFi connected...so set RSSI
        WiFiMesg.mesg.WiFiMesg.rssi = WiFi.RSSI();

        // Only set state when not connected to the internet
        //     due to differenct 'frequencies' of checking WiFi vesus Internet availability
        if (WiFiMesg.mesg.WiFiMesg.wifiStatus != e_msg_wifi_internet_connected)
        {
          WiFiMesg.mesg.WiFiMesg.wifiStatus = e_msg_wifi_accespoint_connected;
        }

#ifdef BBPINGURL
        // Check if we can reach the internet/brewbricks-server
        if (ping_count == 0)
        {

          if (Ping.ping(BBPINGURL))
          {
            printf("[CONF] PING: We have internet connection !!!\n");
            // Internet conmection as ping to server was successful
            WiFiMesg.mesg.WiFiMesg.wifiStatus = e_msg_wifi_internet_connected;

            ping_count = PING_COUNT; // long re-test count
          }
          else
          {
            printf("[CONF] PING: We have NO internet connection !!!\n");
            // No internet, but WiFi is still connected
            WiFiMesg.mesg.WiFiMesg.wifiStatus = e_msg_wifi_accespoint_connected;

            ping_count = 3; // try again in 3 seconds
          }
        }
        else
        {
          ping_count--;
        }
#endif
      }
      else
      {
        // No WiFi connected
        WiFiMesg.mesg.WiFiMesg.rssi = -1024;
        WiFiMesg.mesg.WiFiMesg.wifiStatus = e_msg_wifi_unconnected;
      }

      WiFiMesg.type = e_mtype_wifi;
      WiFiMesg.valid = true;
      controllerQueueSend(&WiFiMesg, 0);

      status_count = RSSI_COUNT;
    }
    else
    {
      status_count--;
    }

    // printf("[CONF] status_count=%d, ping_count=%d \n", status_count, ping_count);
  }
}

// wrapper for sendQueue
int WiFiQueueSend(WiFiQueueItem_t *WiFiQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;

  if (WiFiQueue != NULL)
  {
    r = xQueueSend(WiFiQueue, WiFiQMesg, xTicksToWait);
  }

  return r;
}

// ============================================================================
// INIT WIFI
// ============================================================================
// See : https://microcontrollerslab.com/esp32-wi-fi-manager-asyncwebserver/
// See : https://iotespresso.com/create-captive-portal-using-esp32/
// See : https://gist.github.com/SBajonczak/737310ee2c2360439e0daa75cad805b2

void initWiFi(bool startCaptivePortal)
{
  printf("[CONF] initWifi\n");

  _startCaptivePortal = startCaptivePortal;

  if (!LittleFS.begin(true))
  {
    Serial.println("[CONF] Error mounting LittleFS");
  }

  Serial.println("[CONF] available files:");
  listDir(LittleFS, "/", 2);

  readConfig();

  // FOR DEBUG ONLY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#if (CFG_COMM_WM_USE_DRD == true)
  initDRD();
#endif

  WiFiQueue = xQueueCreate(16, sizeof(WiFiQueueItem_t));

  // create task
  xTaskCreatePinnedToCore(WiFiTask, "WiFiTask", 3 * 1024, NULL, 10, &WiFiTaskHandle, 1); // was 2 * 1024

  printf("[CONF] initWifi() DONE\n");
}

// end of file
