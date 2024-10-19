
#ifdef CFG_DISPLAY_CYD_480x320

#include <Arduino.h>
#include <ui.h>
#include <esp32_smartdisplay.h>
#include "display.h"

#define LOG_TAG "DISP"

void initLVGL(void)
{
  // init TFT display & LVGL
  ESP_LOGI(LOG_TAG, "initLVGL for cyd_480x320");

  smartdisplay_init();
  smartdisplay_lcd_set_backlight(0.0f);

  auto disp = lv_disp_get_default();
  lv_disp_set_rotation(disp, LV_DISP_ROT_270);
  ui_init();
  lv_disp_load_scr(ui_mainScreen);

  smartdisplay_lcd_set_backlight(1.0f);

  ESP_LOGI(LOG_TAG, "initLVGL done");  
}

#endif // CFG_DISPLAY_CYD_480x320

// end of file