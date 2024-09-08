//
// config.h
//

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>
#include <WiFiGeneric.h>

//=============================================

// BAUDRATE SERIAL PORT
#define CFG_BAUDRATE (115200)

// SENSOR TYPE
#define CFG_TEMP_SENSOR_TYPE_SIMULATION_ENABLED true 
#define CFG_TEMP_SENSOR_TYPE_DS18B20_ENABLED    false
#define CFG_TEMP_SENSOR_TYPE_SHT3X_ENABLED      false
#define CFG_TEMP_SENSOR_TYPE_SHT4X_ENABLED      false

// SENSOR SPECIFIC OPTIONS
#define CFG_TEMP_SENSOR_SCAN_I2C                true
#define CFG_TEMP_SENSOR_TYPE_DS18B20_CHECK_COUNTERFEIT   true

// TEMPERATURE MEASUREMENT OPTIONS
#define CFG_TEMP_PIN                    GPIO_NUM_10  // 16 = Green led on CYD
#define CFG_TEMP_IN_CELCIUS             true
//#define CFG_TEMP_IN_FARENHEID          true
#define CFG_TEMP_SMOOTH_NUM_SAMPLES     7             // must be odd number
#define CFG_TEMP_SMOOTH_MAX_DEVIATION   10            // 1 degree * 10


//=============================================

 // #define CFG_LED_PIN             LED_BUILTIN

//=============================================

#define CFG_RELAY_TYPE_GPIO             false
#define CFG_RELAY_TYPE_IOEXP            true

#if (CFG_RELAY_TYPE_GPIO == true)
//#define CFG_RELAY0_PIN                  GPIO_NUM_17
#define CFG_RELAY0_PIN                  0     // 0 = no relay
#define CFG_RELAY0_ON_LEVEL             0
#define CFG_RELAY0_OUTPUT_TYPE          OUTPUT_OPEN_DRAIN     // or OUTPUT
#define CFG_RELAY0_ON_DELAY             200                   // compressor-delay in seconds
#define CFG_RELAY0_OFF_DELAY            0
#define CFG_RELAY0_LABEL                "Cool"

// #define CFG_RELAY1_PIN                  GPIO_NUM_4
#define CFG_RELAY1_PIN                  0      // 0 = no relay
#define CFG_RELAY1_ON_LEVEL             0
#define CFG_RELAY1_OUTPUT_TYPE          OUTPUT_OPEN_DRAIN
#define CFG_RELAY1_ON_DELAY             0
#define CFG_RELAY1_OFF_DELAY            0
#define CFG_RELAY1_LABEL                "Heat"
#endif

#if (CFG_RELAY_TYPE_IOEXP == true)
#define CFG_RELAY0_PIN                  0
#define CFG_RELAY0_ON_LEVEL             1
#define CFG_RELAY0_ON_DELAY             200                   // compressor-delay in seconds
#define CFG_RELAY0_OFF_DELAY            0
#define CFG_RELAY0_LABEL                "Cool"

#define CFG_RELAY1_PIN                  1
#define CFG_RELAY1_ON_LEVEL             1
#define CFG_RELAY1_ON_DELAY             0
#define CFG_RELAY1_OFF_DELAY            0
#define CFG_RELAY1_LABEL                "Heat"
#endif

//=============================================

#define CFG_COMM_USE_FIXEDCREDS         false
#define CFG_COMM_CREDS_SSID             "bookesbeer"
#define CFM_COMM_CREDS_PASSWD           "password"

#define CFG_COMM_SSID_PORTAL            "BOOKESBRICK"
#define CFG_COMM_ONLINE_TIMEOUT         120

#define CFG_COMM_BBURL_API_SERVER       "brewbricks.com"
#define CFG_COMM_BBURL_API_BASE         "https://brewbricks.com/api"

#define CFG_COMM_BBURL_API_IOT          "/iot/v1"
#define CFG_COMM_BBURL_PRO_API_DEVICE   "/device"
#define CFG_COMM_BBURL_PRO_API_DEVICES  "/devices"

#define CFG_COMM_DEVICE_TYPE            "bookesbrick"  // do not change, impacts API result message
#define CFG_COMM_DEVICE_BRAND           "bierbot" 
#define CFG_COMM_DEVICE_VERSION         "0.2" 
#define CFG_COMM_PROAPI_INTERVAL        60  // query PRO-API every n seconds

#define CFG_COMM_TIMEREQUEST_INTERVAL   60

#define CFG_COMM_WM_DEBUG               true
#define cfg_COMM_WM_RESET_SETTINGS      false // TODO: CHECK
#define CFG_COMM_WM_USE_DRD             false
#define CFG_COMM_WM_USE_PIN             false
#define CFG_COMM_WM_PORTAL_PIN          GPIO_NUM_4
#define CFG_COMM_HOSTNAME               "bookesbrick"

#define CFG_ENABLE_SCREENSHOT           true

//=============================================

#define BBPREFS                         "bbPrefs"
#define BBPREFS_APIKEY                  "bbPrApiKey"
#define BBPREFS_PROAPIKEY               "bbPrProApiKey"
#define BBPREFS_SSID                    "bbPrSSID"
#define BBPREFS_PASSWD                  "bbPrPasswd"
#define BBPREFS_HOSTNAME                "bbPrHostname"

#define BBDRDTIMEOUT                    10
// #define BBPINGURL                    CFG_COMM_BBURL_API_SERVER
#define BBPINGURL                       (IPAddress(8,8,8,8))  // google.com

//=============================================

typedef enum WiFiQueueMesgType
{
    e_cmd_start_wifi,
    e_cmd_start_portal,
    e_cmd_stop,
    e_event,
    e_unknown
} WiFiQueueMesgType_t;


typedef struct WiFiQueueItem
{
  WiFiQueueMesgType_t mesgId;
  WiFiEvent_t event;
} WiFiQueueItem_t;


typedef struct configValues
{
  bool inConfigMode;
  String SSID;
  String passwd;
  String apiKey; 
  String proApiKey; 
  String hostname;
} configValues_t;

extern configValues_t config;

extern void initWiFi(bool);
extern bool checkBootConfigMode(void);
extern void readConfig(void);
extern int WiFiQueueSend(WiFiQueueItem_t *, TickType_t);

#endif
