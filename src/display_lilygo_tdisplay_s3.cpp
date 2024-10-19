
// See : https://github.com/moononournation

#ifdef CFG_DISPLAY_LILYGO_TDISPLAY_S3

#include <Arduino.h>
#include <ui.h>
#include "config.h"
#include "display.h"

#define LOG_TAG "DISP"

/*******************************************************************************
 * Start of Arduino_GFX setting
 ******************************************************************************/
#include <Arduino_GFX_Library.h>
#include "GFX\Arduino_GFX_dev_device.h"

//===========================================================================

static uint32_t screenWidth;
static uint32_t screenHeight;
static uint32_t bufSize;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

/* Display flushing */
static void displayFlush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{

#ifndef DIRECT_MODE
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
#endif // #ifndef DIRECT_MODE

  lv_disp_flush_ready(disp_drv);
}




void initLVGL(void)
{
  // init TFT display & LVGL
  ESP_LOGI(LOG_TAG, "init LVGL");

#ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
#endif

  // Init Display
  if (!gfx->begin())
  {
    ESP_LOGE(LOG_TAG, "gfx->begin() failed");
  }
  gfx->fillScreen(BLACK);
  gfx->setRotation(1);
  
#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  // Init touch device
  //  touch_init(gfx->width(), gfx->height(), gfx->getRotation());

  lv_init();

// #if LV_USE_LOG != 0
//   lv_log_register_print_cb(Serial.prin); /* register print function for debugging */
// #endif

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  ESP_LOGI(LOG_TAG, "screenWidth=%d, screenHeight=%d",screenWidth, screenHeight);

#ifdef DIRECT_MODE
  bufSize = screenWidth * screenHeight;
#else
  bufSize = screenWidth * 40;
#endif

#ifdef ESP32
#if defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL))
  disp_draw_buf = (lv_color_t *)gfx->getFramebuffer();
#else  // !(defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL)))

//  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); // MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT   
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);  
  
  if (!disp_draw_buf)
  {
    ESP_LOGE(LOG_TAG, "1st disp_draw_buf allocate failed");
    // remove MALLOC_CAP_INTERNAL flag try again
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
  }
#endif // !(defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL)))
#else  // !ESP32
  ESP_LOGE(LOG_TAG, "disp_draw_buf heap_caps_malloc failed");
  disp_draw_buf = (lv_color_t *)malloc(bufSize * 2);
#endif // !ESP32
  if (!disp_draw_buf)
  {
    ESP_LOGE(LOG_TAG, "final disp_draw_buf allocate failed");
  }
  else
  {
    ESP_LOGI(LOG_TAG, "disp_draw_buf allocate ok");
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, bufSize);

    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = displayFlush;
    disp_drv.draw_buf = &draw_buf;

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);

#ifdef DIRECT_MODE
    disp_drv.direct_mode = true;
#endif

    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    // static lv_indev_drv_t indev_drv;
    // lv_indev_drv_init(&indev_drv);
    // indev_drv.type = LV_INDEV_TYPE_POINTER;
    // indev_drv.read_cb = my_touchpad_read;
    // lv_indev_drv_register(&indev_drv);

    ESP_LOGI(LOG_TAG, "initLVGL init UI");

    auto disp = lv_disp_get_default();
    lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);
    ui_init();
    lv_disp_load_scr(ui_mainScreen);

    ESP_LOGI(LOG_TAG, "initLVGL done");
  }
}


#endif

// end of file;