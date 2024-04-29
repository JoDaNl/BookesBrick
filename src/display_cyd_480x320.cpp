//
// i2c_lcd_16x2.cpp
//

#ifdef CFG_DISPLAY_CYD_480x320

#include <Arduino.h>
#include <ui.h>
#include "esp32_smartdisplay.h"
#include "config.h"
#include "display.h"


void initDisplay_cyd_480x320(void)
{
  // init TFT display & LVGL
  printf("[DISPLAY] CYD - INIT\n");
  smartdisplay_init();
  smartdisplay_lcd_set_backlight(1.0f);
  auto disp = lv_disp_get_default();
  lv_disp_set_rotation(disp, LV_DISP_ROT_90);
  ui_init();
  lv_disp_load_scr(ui_mainScreen);
}

// ============================================================================
// DISPLAY TASK
// ============================================================================

void displayTask(void *arg)
{
  static displayQueueItem_t qMesg;
  static char buf[16];


  printf("[DISPLAY] CYD - ENTERING TASK-LOOP\n");

  while (true)
  {
    if (xQueueReceive(displayQueue, &qMesg, 40 / portTICK_RATE_MS) == pdTRUE)
    {
      // printf("[DISPLAY] received qMesg.type=%d - ",qMesg.type);

      switch (qMesg.type)
      {
      case e_temperature:
        // printf("[DISPLAY] %2.1f\n",qMesg.data.temperature / 10.0);
        break;
      case e_setpoint:
        break;
        

      case e_error:
        // printf("[DISPLAY] error=%d\n",qMesg.data.error);
        break;

      case e_actuator:
        static uint8_t actuator0 = 0;
        static uint8_t actuator1 = 0;
        const char *label;

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

        }
        else
        {
          if (qMesg.data.actuators)
          {
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
            }
          }
        }
        break;
      }
    }

    // printf("[DISPLAY] lv_timer_handle()\n");
    lv_timer_handler();

  }
};

#endif 

// end of file