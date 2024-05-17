//
// lcd_128x64.cpp
//


#ifdef CFG_DISPLAY_LCD_12864

#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "display.h"

// 128 x 64  LCD DISPLAY
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/ 18, /* data=*/ 23, /* CS=*/ 5, /* reset=*/ 22); // ESP32

#define DELAY                   (100)

static bool temperatureValid;
static uint16_t temperature;
static bool  setPointValid;
static uint16_t setPoint;
static bool heart;
static bool  rssiValid;
static int16_t rssi;
static uint8_t actuators;
static uint16_t cdelay;

// fonts : https://github.com/olikraus/u8g2/wiki/fntlistallplain#42-pixel-height

static void displaySetup(void)
{
  u8g2.begin();

  // prepare
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);

  u8g2.firstPage();
}


static void tempToStr(bool temperatureValid, int16_t temperature, char * tempStr1, char * tempStr2)
{
  uint8_t tempInt;
  uint8_t tempFrac;

  if (temperatureValid)
  {
      // we cannot handle negative temperatures
    if (temperature <= 0)
    {
      strcpy(tempStr1,"<0");
      strcpy(tempStr2,"  ");
    }
    else
    {
      // we cannot display more than 2 digits (1000 is 100 degrees)
      if (temperature >= 1000)
      {
        strcpy(tempStr1,"HI");
        strcpy(tempStr2,"  ");
      }
      else
      {
        tempInt  = temperature / 10;
        tempFrac = temperature - tempInt*10;
        sprintf(tempStr1,"%2d", tempInt);
        sprintf(tempStr2,".%1d", tempFrac);
        // printf("tempStr1=*%s* tempStr2=*%s*\n", tempStr1, tempStr2);
      }
    }
  }
  else
  {
    // no valid temperature yet
      strcpy(tempStr1,"--");
      strcpy(tempStr2,".-");
  }
}

// RENDER DISPLAY

static void displayRender(void)
{
  char tempStr1[8];
  char tempStr2[8];

  u8g2.clearBuffer();

  // TEMPERATURE 
  tempToStr(temperatureValid, temperature, tempStr1, tempStr2);

  // draw temperature
  u8g2.setFont(u8g2_font_logisoso42_tf);  // LARGE font
  u8g2.drawStr(0, 0, tempStr1);

  // degree symbol
  u8g2.setFont(u8g2_font_logisoso18_tf);  // small font
  u8g2.drawStr(50, 24, tempStr2);
  u8g2.drawGlyph(52,3,0xB0);

  #ifdef CFG_TEMP_IN_CELCIUS
  u8g2.drawStr(58, 0, "C");
  #endif
  #ifdef CFG_TEMP_IN_FARENHEID
  u8g2.drawStr(58, 0, "F");
  #endif
  
  // cool or heat
  switch (actuators)
  {
    case 1: 
      u8g2.drawStr(80, 0, "Cool");
      break;
    case 2:
      u8g2.drawStr(80, 0, "Heat");
      break;
    default:
      u8g2.drawStr(80, 0, " -- ");
      break;
  }
  
  // setpoint
  tempToStr(setPointValid, setPoint, tempStr1, tempStr2);
  strcat(tempStr1, tempStr2);
  u8g2.drawStr(80, 24, tempStr1);

  // heart
  u8g2.setFont(u8g2_font_unifont_t_78_79);
  if (heart)
  {
    u8g2.drawGlyph(110,48,0x2764); // heart symbol
  }
  else
  {
    u8g2.drawGlyph(110,48,0x2715); // diagonal cross
  }
  
  // RSSI  > -30 dBm   Amazing
  // RSSI  <– 55 dBm	 Very good signal
  // RSSI  <– 67 dBm	 Fairly Good
  // RSSI  <– 70 dBm	 Okay
  // RSSI  <– 80 dBm	 Not good
  // RSSI  <– 90 dBm	 Extremely weak signal (unusable
  int rssi_x   = 95;
  int rssi_y   = 63; // bottom of graph

  if (rssiValid)
  {
    int bars     = 0;
    int levels[] = { -90, -80, -70, -67, -55, -30};

    while ((rssi > levels[bars]) && (bars < 6))
    {
      bars++;
    }
    // printf("[DISPLAY] rssi=%d bars=%d\n",rssi, bars);

    for(int i=0; i<7; i++)
    {
      u8g2.drawPixel(rssi_x+i*2,rssi_y);
      if (i<=bars)
      {
        u8g2.drawVLine(rssi_x+i*2,rssi_y-2-i*2,i*2+1);
      }
    }
  }
  else
  {
    u8g2.drawGlyph(rssi_x, 48, 0x2715); // diagonal cross
  }

  // compressor delay
  // Only show if an actuator is enabled. When the backend is at the end of a 
  // recipy/manual-mode it will disable the actuators. The actuator task will stop de delay-count
  // and cdelay will not reach zero. If we do not do this the delay value will remain on the LCD
  if ( (cdelay > 0)  && (actuators != 0) )
  {
    char strDelay[16];
    sprintf(strDelay,"comp delay %d", cdelay);
    // printf("[DISPLAY] delay=%s\n",strDelay);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(5, 52, strDelay);
  }

  // show all on the LCD display
  u8g2.nextPage();
}

// ============================================================================
// DISPLAY TASK
// ============================================================================

void displayTask(void *arg)
{
  static displayQueueItem_t qMesg;
  static uint16_t doRenderCount;
  static bool doRender;

  temperatureValid = false;
  temperature = 0;
  setPointValid = false;
  setPoint = 0;
  actuators = 0;

  displaySetup();

  doRenderCount = 0;;
  doRender = false;

  while (true)
  {
   if (xQueueReceive(displayQueue, &qMesg, DELAY / portTICK_RATE_MS) == pdTRUE)
    {
      // printf("[DISPLAY] received qMesg.type=%d\n", qMesg.type);

      doRender          = true;

      switch (qMesg.type)
      {
      case e_temperature:
        temperature       = qMesg.data.temperature;
        temperatureValid  = qMesg.valid;
        break;
      case e_setpoint:
        setPoint          = qMesg.data.setPoint;
        setPointValid     = qMesg.valid;
        break;
      case e_rssi:
        rssi              = qMesg.data.rssi; 
        rssiValid         = qMesg.valid;
        break;
      case e_error:
        break;
      case e_actuator:
        actuators         = qMesg.data.actuators;
        break;
      case e_heartbeat:
        heart             = qMesg.data.heartBeat;
        break;
      case e_delay:
        cdelay            = qMesg.data.compDelay;
        break;
      default:
        doRender          = false;
        break;
      }
    };

    if (doRenderCount == 0)
    {
      displayRender();
      doRenderCount = 3;
      doRender = false;
    }
    else
    {
      if (doRender)
      {
        doRenderCount--;
      }
    }

  }
}

#endif

// end of file
