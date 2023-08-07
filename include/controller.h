
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__


// button 

typedef enum controllerQButtonMesgType
{
    e_msg_button_unknown,
    e_msg_button_short,
    e_msg_button_long
} controllerQButtonMesgType_t;

typedef struct controllerQButtonMesg
{
  controllerQButtonMesgType_t mesgId;
  uint8_t data;
} controllerQButtonMesg_t;


// Sensor

typedef enum controllerQSensorMesgType
{
    e_msg_sensor_unknown,
    e_msg_sensor_numSensors,
    e_msg_sensor_temperature
} controllerQSensorMesgType_t;

typedef struct controllerQSensorMesg
{
  controllerQSensorMesgType_t mesgId;
  uint16_t data;                 // temperature multiplied by 10 | number of sensors
} controllerQSensorMesg_t;


// Controller-Q message

typedef enum controllerQMesgType
{
    e_mtype_button,
    e_mtype_sensor
} controllerQMesgType_t;

typedef union controllerQMesgData
{
  controllerQButtonMesg_t buttonMesg;
  controllerQSensorMesg_t sensorMesg;
} controllerQMesgData_t;


typedef struct controllerQItem
{
  controllerQMesgType_t type;
  controllerQMesgData_t mesg;
} controllerQItem_t;


extern bool globalWifiConfigMode;
extern xQueueHandle controllerQueue;
extern void initController(void);

#endif
