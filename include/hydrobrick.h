
#ifndef __HYDROBRICK__
#define __HYDROBRICK__

#include <NimBLEDevice.h>
#include "config.h"

// DO NOT CHANGE - USE SAME UUIDS AS IN HYDROBRICK PROJECT
const char HDdeviceName                     [] = "HydroBrick";
const char HDhydrometerServiceUUID          [] = "63875899-6490-4d1b-9f0a-abee8653282c";
const char HDhydrometerCharacteristicID     [] = "35f14e74-c1ae-4153-9abb-938481aa24cf";

typedef enum
{
  e_scan_for_all_bricks,
  e_scan_for_registered_brick
} scanMode_t;

typedef struct
{
  NimBLEAddress addresses[CFG_HYDRO_MAX_NR_BRICKS];
  uint8_t number;
} hydrometerScannedBricks_t;


typedef enum
{
  battery_charging = 1,
  harwdware_error  = 128
} hydrometerStatus_t;

typedef struct 
{
  uint8_t  status;
  uint16_t angle_x100;
  uint16_t temperature_x10;
  uint16_t batteryVoltage_x1000;
} hydrometerData_t;


typedef union
{
  hydrometerData_t data;
  uint8_t bytes[sizeof(hydrometerData_t)];
} hydrometerDataBytes_t;

typedef enum hydroQMesgType
{
    e_msg_hydro_unknown,
    e_msg_hydro_cmd_get_reading,
    e_msg_hydro_cmd_scan_bricks,
    e_msg_hydro_evt_device_discovered,
    e_msg_hydro_evt_device_connected,
    e_msg_hydro_evt_device_disconnected,
    e_msg_hydro_evt_timeout,
    e_msg_hydro_evt_attribute_read
} hydroQMesgType_t;


typedef struct hydroQMesg
{
  hydroQMesgType_t mesgId;
  int16_t data;
} hydroQueueItem_t;


extern void initHydroBrick(void);
extern int hydroQueueSend(hydroQueueItem_t * , TickType_t );

#endif