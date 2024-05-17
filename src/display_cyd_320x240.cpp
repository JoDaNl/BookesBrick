//
// i2c_lcd_16x2.cpp
//

#ifdef CFG_DISPLAY_CYD_320x240

#include <Arduino.h>
#include <ui.h>
#include "esp32_smartdisplay.h"
#include "config.h"
#include "display.h"

#define LOG_TAG "display"


typedef enum
{
  TEMP_SERIE,
  COOL_SERIE,
  HEAT_SERIE
} series_indentifier_t;

// temperature chart series
static lv_chart_series_t *ui_temp_serie;
static lv_chart_series_t *ui_setp_serie;
static lv_chart_series_t *ui_cool_serie;
static lv_chart_series_t *ui_heat_serie;

// See : https://forum.lvgl.io/t/how-do-i-scroll-the-contents-of-a-lv-win-up-when-the-keyboard-pops-up-so-that-the-active-textarea-remains-in-view-using-lvgl8-1-0/6158
// See : https://docs.lvgl.io/master/examples.html#faded-area-line-chart-with-custom-division-lines

// UI callbacks

static void cb_chart_draw_event(lv_event_t *e)
{
  static lv_obj_t *obj = lv_event_get_target(e);
  static lv_chart_series_t *serie;
  static lv_area_t area;
  static lv_obj_draw_part_dsc_t *dsc;

  dsc = lv_event_get_draw_part_dsc(e);

  // Customize chart series
  if ((dsc->part == LV_PART_ITEMS) && (dsc->p1 != NULL) && (dsc->p2 != NULL))
  {
    // get current serie
    serie = (lv_chart_series_t *)(dsc->sub_part_ptr);

    // only draw the colored background for cool & heat serie
    if ((serie == ui_cool_serie) || (serie == ui_heat_serie))
    {
      // Add a line mask that keeps the area below the line
      lv_draw_mask_line_param_t line_mask_param;
      lv_draw_mask_line_points_init(&line_mask_param, dsc->p1->x, dsc->p1->y, dsc->p2->x, dsc->p2->y, LV_DRAW_MASK_LINE_SIDE_BOTTOM);
      int16_t line_mask_id = lv_draw_mask_add(&line_mask_param, NULL);

      /*Draw a rectangle that will be affected by the mask*/
      lv_draw_rect_dsc_t draw_rect_dsc;
      lv_draw_rect_dsc_init(&draw_rect_dsc);
      draw_rect_dsc.bg_opa = LV_OPA_10;
      draw_rect_dsc.bg_color = dsc->line_dsc->color;

      area.x1 = dsc->p1->x;
      area.x2 = dsc->p2->x - 1;     // prevent 1 pixel overlap with neighbour...causes visible vertical lines otherwise
      area.y1 = obj->coords.y1 + 2; // add offset at top of graph to prevent overlap with graph-border
      area.y2 = obj->coords.y2 - 2; // subtract offset at bottom of graph  to prevent overlap with graph-border
      lv_draw_rect(dsc->draw_ctx, &draw_rect_dsc, &area);

      // Remove the mask
      lv_draw_mask_free_param(&line_mask_param);
      lv_draw_mask_remove_id(line_mask_id);
    }
  }

  // Customize labels of Y-axes...i.e. divide graph values by 10 (graph does not handle floats)
  if (dsc->part == LV_PART_TICKS)
  {
    lv_snprintf(dsc->text, sizeof(dsc->text), "%d", dsc->value/10);
  }

}

void cb_clicked(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = lv_event_get_target(e);
  printf("*\n");
}


static void displayTemperature(float temperature, bool valid, lv_obj_t *temperatureLabel, lv_obj_t *temperatureLabelSmall, float lowRange, float highRange)
{
  static String temperatureStr;
  static String leftStr;
  static String rightStr;
  static int dotPos;

  temperatureStr = String(round(temperature * 10) / 10, 1);
  dotPos = temperatureStr.indexOf(".");

  leftStr = temperatureStr.substring(0, dotPos);
  rightStr = temperatureStr.substring(dotPos + 1);

  if (valid)
  {
    if (temperature > highRange)
    {
      // weight to high
      lv_label_set_text(temperatureLabel, "HI");
      lv_label_set_text(temperatureLabelSmall, "-");
    }
    else if (temperature < lowRange)
    {
      // weight to low
      lv_label_set_text(temperatureLabel, "LO");
      lv_label_set_text(temperatureLabelSmall, "-");
    }
    else
    {
      // show weight
      lv_label_set_text(temperatureLabel, leftStr.c_str());
      lv_label_set_text(temperatureLabelSmall, rightStr.c_str());
    }
  }
  else
  {
    // no valid weight
    lv_label_set_text(temperatureLabel, "--");
    lv_label_set_text(temperatureLabelSmall, "-");
  }
}

void initDisplay_cyd_320x240(void)
{
  // init TFT display & LVGL
  ESP_LOGI(LOG_TAG, "void initDisplay_cyd_320x240");

  smartdisplay_init();
  smartdisplay_lcd_set_backlight(0.0f);

  auto disp = lv_disp_get_default();
  lv_disp_set_rotation(disp, LV_DISP_ROT_270);
  ui_init();
  lv_disp_load_scr(ui_mainScreen);
}

// ============================================================================
// DISPLAY TASK
// ============================================================================

void displayTask(void *arg)
{
  static displayQueueItem_t qMesg;

  static uint16_t setPointx10 = 0;
  static bool setPointx10Valid = false;

  static uint32_t compDelayPerc = 0;

  static uint8_t actuator0 = 0;
  static uint8_t actuator1 = 0;

  static bool heartbeatToggle = false;

  static bool timeToggle = false;
  static uint32_t tickCount = 0;
  static uint32_t prevTickCount = 0;
  static uint8_t timeHours;
  static uint8_t timeMinutes;

  static bool timeValid = false;
  static bool setpValid = false;
  static lv_coord_t temp_value;
  static lv_coord_t setp_value;

  static String deviceName;

  static float lcdBacklight = 0.0;


  // lv_label_set_text(ui_testLabel, LV_SYMBOL_SETTINGS LV_SYMBOL_OK);

  ui_setp_serie = lv_chart_add_series(ui_temperatureChart, lv_color_hex(0x808080), LV_CHART_AXIS_PRIMARY_Y);
  ui_temp_serie = lv_chart_add_series(ui_temperatureChart, lv_color_hex(0x101010), LV_CHART_AXIS_PRIMARY_Y);
  ui_cool_serie = lv_chart_add_series(ui_temperatureChart, lv_color_hex(0x0000FF), LV_CHART_AXIS_SECONDARY_Y);
  ui_heat_serie = lv_chart_add_series(ui_temperatureChart, lv_color_hex(0xFF0000), LV_CHART_AXIS_SECONDARY_Y);

  // set range for heal/cool background (on secondary axis) from 0 to 1
  lv_chart_set_range(ui_temperatureChart, LV_CHART_AXIS_SECONDARY_Y, 0, 1);

  lv_obj_set_style_size(ui_temperatureChart, 0, LV_PART_INDICATOR); // set dot-size to 0
  lv_obj_add_event_cb(ui_temperatureChart, cb_chart_draw_event, LV_EVENT_DRAW_PART_BEGIN, NULL);

  while (true)
  {
    if (xQueueReceive(displayQueue, &qMesg, 100 / portTICK_RATE_MS) == pdTRUE)
    {
      // printf("[DISPLAY] received qMesg.type=%d\n", qMesg.type);

      switch (qMesg.type)
      {
      case e_temperature:
        // printf("[DISPLAY] %2.1f\n", qMesg.data.temperature / 10.0);
        displayTemperature(qMesg.data.temperature / 10.0, qMesg.valid, ui_tempLabel, ui_tempLabelSmall, 0, 100);

        // Update graph
        if (qMesg.valid)
        {
          // both temp & set-point are  * 10
          temp_value = qMesg.data.temperature;
          setp_value = setPointx10Valid ? setPointx10 : LV_CHART_POINT_NONE;
          // temp_value = qMesg.data.temperature / 10.0;
          // setp_value = setPointx10Valid ? setPointx10 / 10.0 : LV_CHART_POINT_NONE;

          if (actuator0 == 1)
          {
            // Y-value is >1 so serie-line will be outside of visible area of graph !
            lv_chart_set_next_value(ui_temperatureChart, ui_cool_serie, 2*10);
          }
          else
          {
            lv_chart_set_next_value(ui_temperatureChart, ui_cool_serie, LV_CHART_POINT_NONE);
          }

          if (actuator1 == 1)
          {
            // Y-value is >1 so serie-line will be outside of visible area of graph !
            lv_chart_set_next_value(ui_temperatureChart, ui_heat_serie, 2*10);
          }
          else
          {
            lv_chart_set_next_value(ui_temperatureChart, ui_heat_serie, LV_CHART_POINT_NONE);
          }

          lv_chart_set_next_value(ui_temperatureChart, ui_setp_serie, setp_value);
          lv_chart_set_next_value(ui_temperatureChart, ui_temp_serie, temp_value);

          static lv_coord_t *y_array;
          static uint16_t y_count;
          static lv_coord_t y_min;
          static lv_coord_t y_max;

          y_count = lv_chart_get_point_count(ui_temperatureChart);
          y_array = lv_chart_get_y_array(ui_temperatureChart, ui_temp_serie);

          if (setPointx10Valid)
          {
            // init min/max with setPoint, so target-temp will always be in view of graph
            y_min = setPointx10;
            y_max = setPointx10;
          }
          else
          {
            // init min-max with first data-point
            y_min = *y_array;
            y_max = *y_array;
          }

          // loop from FIRST to last data-point. Also works if we have only 1 datapoint.
          for (uint16_t i = 0; i < y_count; i++)
          {
            if (*y_array != LV_CHART_POINT_NONE)
            {
              if (*y_array < y_min)
              {
                y_min = *y_array;
              }

              if (*y_array > y_max)
              {
                y_max = *y_array;
              }
              y_array++;
            }
          }

          // If y_min & y_max are value-wise close to each other the graph's y-axis may display
          // the same label 2 or 3 times.
          // assure the veritical axis covers a minumum of 3 degrees
          // note values are * 10
          if ( (y_max - y_min) < 30)
          {
            y_max = y_max + 15;
            y_min = y_min - 15;
          }
          
          // printf("[DISPLAY] Graph scale: min=%d, max=%d\n", y_min, y_max);
          lv_chart_set_range(ui_temperatureChart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
        }
        break;

      case e_setpoint:
        printf("[DISPLAY] %2.1f\n", qMesg.data.temperature / 10.0);
        ESP_LOGI(LOG_TAG, " setpoint=%2.1f", qMesg.data.temperature / 10.0);
        displayTemperature(qMesg.data.temperature / 10.0, qMesg.valid, ui_setPointLabel, ui_setPointSmallLabel, 0, 100);
        setPointx10 = qMesg.data.temperature;
        setPointx10Valid = qMesg.valid;
        break;

      case e_error:
        break;

      case e_actuator:
        printf("[DISPLAY] e_actuator=%d\n", qMesg.data.actuators);

        if (qMesg.valid)
        {
          actuator0 = (qMesg.data.actuators & 1);      // COOL
          actuator1 = (qMesg.data.actuators & 2) >> 1; // HEAT
        }
        else
        {
          actuator0 = 0;
          actuator1 = 0;
        }

        if ((actuator0 == 0) && (actuator1 == 0))
        {
          lv_obj_add_flag(ui_coolLabel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_heatLabel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_offLabel,LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_coolBar, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_heatBar, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {

          if (actuator0 == 1)
          {
            lv_obj_clear_flag(ui_coolLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_heatLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_offLabel,LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_coolBar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_heatBar, LV_OBJ_FLAG_HIDDEN);

            // cool starts at compressor-delay value
            lv_bar_set_value(ui_coolBar, compDelayPerc, LV_ANIM_OFF);
          }

          if (actuator1 == 1)
          {
            lv_obj_add_flag(ui_coolLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_heatLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_offLabel,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_coolBar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_heatBar, LV_OBJ_FLAG_HIDDEN);

            // Heat is always 100%
            lv_bar_set_value(ui_heatBar, 100, LV_ANIM_OFF);
          }
        }
        break;

      case e_delay:
        // only use compressor delay for actuator '0' 
        if (qMesg.number == 0)
        {
          compDelayPerc = 100 - (100 * qMesg.data.compDelay) / CFG_RELAY0_ON_DELAY;
          // printf("[DISPLAY] compressor-delay=%d sec, %d\%\n",qMesg.data.compDelay,compDelayPerc);
          lv_bar_set_value(ui_coolBar, compDelayPerc, LV_ANIM_OFF);
        }
        break;

      case e_rssi:
        // RSSI  Value       Range WiFi Signal Strength
        // RSSI  > -30 dBm   Amazing
        // RSSI  < –67 dBm	 Fairly Good
        // RSSI  < –70 dBm	 Okay
        // RSSI  < –80 dBm	 Not good
        // RSSI  < –90 dBm	 Extremely weak signal (unusable
        static int rssi;
        rssi = (int)qMesg.data.rssi; // rssi is a negative value, so cast to signed int

        if (qMesg.valid)
        {
          ESP_LOGI(LOG_TAG, "RSSI=%d", qMesg.data.rssi);
        }

        if (rssi < -90)
        {
          lv_obj_add_flag(ui_rssiBar0Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar1Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar2Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar3Panel, LV_OBJ_FLAG_HIDDEN);
        }
        else if (rssi < -80)
        {
          lv_obj_clear_flag(ui_rssiBar0Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar1Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar2Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar3Panel, LV_OBJ_FLAG_HIDDEN);
        }
        else if (rssi < -70)
        {
          lv_obj_clear_flag(ui_rssiBar0Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar1Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar2Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar3Panel, LV_OBJ_FLAG_HIDDEN);
        }
        else if (rssi < -67)
        {
          lv_obj_clear_flag(ui_rssiBar0Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar1Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar2Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_rssiBar3Panel, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
          lv_obj_clear_flag(ui_rssiBar0Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar1Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar2Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar3Panel, LV_OBJ_FLAG_HIDDEN);
        }
        break;

      case e_time:
        timeHours = qMesg.data.time >> 8;    // hours in MS byte
        timeMinutes = qMesg.data.time & 255; // minutes in LS byte
        timeValid = qMesg.valid;
        break;

      case e_heartbeat:
        uint8_t hearbeatValue;
        uint8_t hearbeatMax;

        hearbeatValue = qMesg.data.heartBeat & 255;
        hearbeatMax = qMesg.data.heartBeat >> 8;

        lv_bar_set_range(ui_backendBar, 0, hearbeatMax + 1);
        lv_bar_set_value(ui_backendBar, hearbeatValue + 1, LV_ANIM_ON);
        break;

      case e_device_name:
        static String *stringPtr;

        stringPtr = qMesg.data.stringPtr;

        if (stringPtr == NULL)
        {
          ESP_LOGI(LOG_TAG, "Device Name = NULL");
        }
        else
        {
          deviceName = *stringPtr;
          ESP_LOGI(LOG_TAG, "Device Name=%s", deviceName);
          delete stringPtr; // important string has been used...time to delete it !!!
        }

        lv_label_set_text(ui_titleLabel, deviceName.c_str());
        break;
      }
    }

    // Show/update time in lower status-bar once every second
    tickCount = xTaskGetTickCount();
    if ((tickCount - prevTickCount) > 1000)
    {
      static char separator;

      prevTickCount = tickCount;
      timeToggle = !timeToggle;

      separator = timeToggle ? ':' : ' '; // Blink colon between hours & minutes

      if (timeValid)
      {
        lv_label_set_text_fmt(ui_timeLabel, "%2d%c%02d", timeHours, separator, timeMinutes);
      }
      else
      {
        lv_label_set_text_fmt(ui_timeLabel, "--%c--", separator);
      }
    }

    // Update LVGL-GUI
    lv_timer_handler();
    
#if (CFG_ENABLE_SCREENSHOT == true)
    static int prevButton = 1;
    extern lv_disp_drv_t disp_drv;
    uint8_t * ptr;

    // GPIO_NUM_0 == BOOT button 
    if (digitalRead(GPIO_NUM_0) == 0 && prevButton == 1)
    {
      printf("[DISPLAY] SCREENSHOT\n");   

      // ptr = (uint8_t *)disp_drv.draw_buf->buf1;
      // for(int x=0; x<disp_drv.hor_res-1; x++)
      // {
      //   printf("[SHOT] x=%03d: ",x);
      //   for(int y=0; y<disp_drv.ver_res; y++)
      //   {
      //     printf("%02X,",*ptr++);
      //   }
      //   printf("%02X\n",*ptr++);
      // }
    // }

    // static int prevButton = 1;
    // if (digitalRead(GPIO_NUM_0) == 0 && prevButton == 1)
    // {

    //   printf("[DISPLAY] SCREENSHOT\n");   
      
    //   lv_obj_t * root = lv_scr_act();
    //   //lv_obj_t * snapshot_obj = lv_img_create(root);
    //   lv_img_dsc_t * snapshot; //  = (lv_img_dsc_t *)lv_img_get_src(snapshot_obj);
    //   lv_color_t pixel_color;
    //   uint8_t red;
    //   uint8_t green;
    //   uint8_t blue;

    //   lv_coord_t w, h;
      
    //   snapshot = lv_snapshot_take_to_buf(root, LV_IMG_CF_RGB565);
    //   if (snapshot == NULL)
    //   {
    //     printf("[DISPLAY] Cannot allocate memeory for snapshot\n");
    //   }
    //   else
    //   {


      // w = lv_obj_get_width(snapshot);
      // h = lv_obj_get_height(snapshot);

      // w = snapshot->header.w;
      // h = snapshot->header.h;

      // printf("[DISPLAY] SCREENSHOT w=%d\n",w);   
      // printf("[DISPLAY] SCREENSHOT h=%d\n",h);   
      // }


      int w, h;

      lv_obj_t * root = lv_scr_act();
      w = lv_obj_get_width(root);
      h = lv_obj_get_height(root);
      printf("[DISPLAY] SCREENSHOT w=%d\n",w);   
      printf("[DISPLAY] SCREENSHOT h=%d\n",h);   

      extern lv_disp_drv_t disp_drv;
      uint16_t * ptr;

      uint16_t pixel;
      uint8_t red, green ,blue;

      ptr = (uint16_t *)disp_drv.draw_buf->buf1;

      for(int x=0; x<w; x++)
      {
        printf("[SHOT] x=%03d: ",x);
        for(int y=0; y<1; y++)
        {
          pixel = (uint16_t)*ptr++;
          red   = pixel & 0x1F;
          blue  = (pixel >> 11) & 0x1F;
          printf("R:%d, G:%d, B:%d \n", red, green, blue);
        }
        printf("\n");
      }
    }
    prevButton = digitalRead(GPIO_NUM_0);

#endif

    // increment backlight to full brightness
    // this is done after (1st) invocation of lv_imer_handler() to make
    // sure display buffer has valid content and no random pixels (after power-up)
    if (lcdBacklight < 1.0)
    {     
      lcdBacklight += 0.02;
      smartdisplay_lcd_set_backlight(lcdBacklight);
    }

  }
};

#endif

// end of file;