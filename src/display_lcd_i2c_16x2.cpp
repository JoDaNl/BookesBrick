//
// i2c_lcd_16x2.cpp
//

#ifdef CFG_DISPLAY_LCD_I2C_16X2

#include "config.h"

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "display.h"


// LCD Display
static LiquidCrystal_I2C lcd(0x27, 16, 2);  // TODO : address into config.h

void lcdHelper(void)
{
  lcd.clear();

  // Temperature
  lcd.setCursor(0, 0);
  lcd.print("Temp");
  lcd.setCursor(0, 1);
  lcd.printf(" -- ");

  // Set-point
  lcd.setCursor(6, 0);
  lcd.print("SetP");
  lcd.setCursor(6, 1);
  lcd.print(" -- ");

  // Actuator(s)
  lcd.setCursor(12, 1);
  lcd.print(" -- ");

  // Print cool/heat/status
  lcd.setCursor(12, 0);
  lcd.print(" -");
}

// ============================================================================
// DISPLAY TASK
// ============================================================================

void displayTask(void *arg)
{
  static displayQueueItem_t qMesg;
  static char buf[16];
  static const int msToWait = 500 / portTICK_PERIOD_MS;

  lcd.init();

  // start-up message part 1
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Bookes Beer");
  lcd.setCursor(3, 1);
  lcd.print("Automation");
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Supports");
  lcd.setCursor(3, 1);
  lcd.print("Bierbot (c)");
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  lcdHelper();

  while (true)
  {
    if (xQueueReceive(displayQueue, &qMesg, msToWait) == pdTRUE)
    {
      // printf("[DISP] received qMesg.type=%d - ",qMesg.type);

      switch (qMesg.type)
      {
      case e_temperature:
        // printf("[DISP] %2.1f\n",qMesg.data.temperature / 10.0);
        lcd.setCursor(0, 1);
        sprintf(buf, "%2.1f", qMesg.data.temperature / 10.0);
        lcd.printf(buf);
        break;
      case e_setpoint:
        // printf("[DISP] setPoint=%d\n",qMesg.data.temperature);
        lcd.setCursor(6, 1);
        sprintf(buf, "%2.1f", qMesg.data.temperature / 10.0);
        lcd.printf(buf);
        break;
        
      // case e_rssi:
      //   lcd.clear();
      //   lcd.setCursor(0, 0);
      //   lcd.printf("SSID:%s", qMesg.data.wifiSSID);
      //   lcd.printf("IP  :%s", qMesg.data.wifiIP);
      //   vTaskDelay(qMesg.duration * 1000 / portTICK_PERIOD_MS);
      //   lcdHelper();
      //   break;

      // case e_heartbeat:
      //   // printf("[DISP] heartbeat %d\n",qMesg.data.heartbeat);
      //   lcd.setCursor(15, 0);
      //   if (qMesg.data.heartbeat)
      //   {
      //     lcd.print("*");
      //   }
      //   else
      //   {
      //     lcd.print(" ");
      //   }
      //   break;

      case e_error:
        // printf("[DISP] error=%d\n",qMesg.data.error);
        lcd.setCursor(12, 0);

        // if (qMesg.data.error == 0)
        // {
        //   // Nnline
        //   lcd.print("On ");
        // }
        // else if (qMesg.data.error == 1)
        // {
        //   // Offline
        //   lcd.print("Off");
        // }
        // else
        // {
        //   // Error code
        //   lcd.print(qMesg.data.error);
        // }
        break;

      case e_actuator:
        // TODO : make a list of actuators
        static uint8_t actuator0 = 0;
        static uint8_t actuator1 = 0;
        const char *label;
        printf("[DISP] actuator=%d value=%d\n", qMesg.number, qMesg.data.actuators);
        lcd.setCursor(12, 1);

        // Assuming COOL & HEAT are mutual exclusive in BierBot !

        switch (qMesg.number)
        {
        case 0:
          actuator0 = qMesg.data.actuators;
          break;
        case 1:
          actuator1 = qMesg.data.actuators;
          break;
        default:
          break;
        }

        if ((actuator0 == 0) && (actuator1 == 0))
        {
          lcd.print("    ");
        }
        else
        {
          if (qMesg.data.actuators)
          {
//             if (qMesg.duration == 0)  : FIXME, removed after adding LCD 12864
            {
              switch (qMesg.number)
              {
              case 0:
                label = CFG_RELAY0_LABEL;
                break;
              case 1:
                label = CFG_RELAY1_LABEL;
                break;
              default:
                break;
              }

              lcd.print(label);
            }
//            else
            {
//              lcd.printf("%4d", qMesg.duration);  : FIXME, removed after adding LCD 12864
            }
          }
        }
        break;
      }
    };

    // printf("\n");
  }
};

#endif 

// end of file