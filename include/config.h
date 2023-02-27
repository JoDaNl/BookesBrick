//
// config.h
//

#ifndef __CONFIG_H__
#define __CONFIG_H__

//=============================================

#define CFG_TEMP_ENABLED        true
#define CFG_TEMP_PIN            GPIO_NUM_27
#define CFG_TEMP_IN_CELCIUS     true
//#define CFG_TEMP_IN_FARENHEID   true

//=============================================

#define CFG_LCD_16x2_I2C        true

//=============================================

// Relay outputs
#define CFG_RELAY0_PIN          GPIO_NUM_16
#define CFG_RELAY0_ON_LEVEL     1
#define CFG_RELAY0_OUTPUT_TYPE  OUTPUT      // or OUTPUT_OPENDRAIN
#define CFG_RELAY0_ON_DELAY     200         // compressor-delay
#define CFG_RELAY0_OFF_DELAY    0
#define CFG_RELAY0_LABEL        "Cool"

#define CFG_RELAY1_PIN          GPIO_NUM_17
#define CFG_RELAY1_ON_LEVEL     1
#define CFG_RELAY1_OUTPUT_TYPE  OUTPUT
#define CFG_RELAY1_ON_DELAY     0
#define CFG_RELAY1_OFF_DELAY    0
#define CFG_RELAY1_LABEL        "Heat"

//=============================================

#define CFG_COMM_USE_WIFIMANAGER    true
#define CFG_COMM_USE_FIXEDCREDS     false
#define CFG_COMM_CREDS_SSID         "bookesbeer"
#define CFM_COMM_CREDS_PASSWD       "password"

#define CFG_COMM_SSID_PORTAL        "BOOKESBRICK"
#define CFG_COMM_ONLINE_TIMEOUT     120

#define CFG_COMM_BBAPI_URL_BASE     "https://bricks.bierbot.com/api/iot/v1"
#define CFG_COMM_DEVICE_TYPE        "bookusbrick" 
#define CFG_COMM_DEVICE_BRAND       "bookusbeer" 
#define CFG_COMM_DEVICE_VERSION     "0.1" 

//=============================================

#endif
