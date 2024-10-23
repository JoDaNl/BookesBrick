
// See : https://github.com/moononournation

#ifdef CFG_DISPLAY_JC3248W535

#include <Arduino.h>
#include <ui.h>
#include <Arduino_GFX_Library.h>
#ifdef HAS_TOUCH
#include <touch.h>
#endif
#include "config.h"
#include "display.h"

#define LOG_TAG "LVGL"

#define BACKLIGHT_PIN 1

//#define DIRECT_MODE 1

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    45 /* CS */, 
    47 /* SCK */, 
    21 /* D0 */, 
    48 /* D1 */, 
    40 /* D2 */, 
    39 /* D3 */);

static Arduino_GFX *g = new Arduino_AXS15231B(bus, 
  GFX_NOT_DEFINED /* RST */, 
  0 /* rotation */, 
  false /* IPS */, 
  320 /* width */, 
  480 /* height */);

#define CANVAS

static Arduino_Canvas *gfx = new Arduino_Canvas(
    320 /* width */, 
    480 /* height */, 
    g, 
    0 /* output_x */, 
    0 /* output_y */, 
    3 /* 270 rotation, set to 1 for 90 */);

static uint32_t screenWidth;
static uint32_t screenHeight;
static uint32_t bufSize;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;


static void LVGLFlush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
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



void initLVGL()
{
  ESP_LOGI(LOG_TAG,"init LVGL");

#ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
#endif

  // Init Display
  if (!gfx->begin())
  {
    ESP_LOGE(LOG_TAG,"gfx->begin() failed!");
  }

  gfx->fillScreen(BLACK);


#ifdef HAS_TOUCH
  // Init touch device
  touch_init(gfx->width(), gfx->height(), gfx->getRotation());
#endif

  lv_init();

  screenWidth = gfx->width();
  screenHeight = gfx->height();

#ifdef DIRECT_MODE
  bufSize = screenWidth * screenHeight;
#else
  bufSize = screenWidth * 40;
#endif

#ifdef ESP32
#if defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL))
  disp_draw_buf = (lv_color_t *)gfx->getFramebuffer();
#else  // !(defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL)))
//  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!disp_draw_buf)
  {
    // remove MALLOC_CAP_INTERNAL flag try again
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
  }
#endif // !(defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL)))
#else  // !ESP32
  ESP_LOGE(LOG_TAG,"LVGL disp_draw_buf heap_caps_malloc failed! malloc again...");
  disp_draw_buf = (lv_color_t *)malloc(bufSize * 2);
#endif // !ESP32
  if (!disp_draw_buf)
  {
    ESP_LOGE(LOG_TAG,"LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, bufSize);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = LVGLFlush;
    disp_drv.draw_buf = &draw_buf;
#ifdef DIRECT_MODE
    disp_drv.direct_mode = true;
#endif
    lv_disp_drv_register(&disp_drv);



#ifdef HAS_TOUCH
    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
#endif


    ESP_LOGI(LOG_TAG,"ui init");

    ui_init();
    lv_disp_load_scr(ui_mainScreen);
    
    // update once to prevent random bufer data after power up being displayed
    updateLVGL(); 

    // now switch on backlight
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);
  }

  ESP_LOGI(LOG_TAG,"init LVGL done");
}




void updateLVGL(void)
{
    lv_timer_handler(); /* let the GUI do its work */

#ifdef DIRECT_MODE
#if defined(CANVAS) || defined(RGB_PANEL)
  gfx->flush();
#else // !(defined(CANVAS) || defined(RGB_PANEL))
#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
#else
  gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
#endif
#endif // !(defined(CANVAS) || defined(RGB_PANEL))
#else  // !DIRECT_MODE
#ifdef CANVAS
  gfx->flush();
#endif
#endif // !DIRECT_MODE

}

#endif

// end of file;