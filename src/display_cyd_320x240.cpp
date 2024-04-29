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

  smartdisplay_lcd_set_backlight(0.0f);
  smartdisplay_init();

  auto disp = lv_disp_get_default();
  lv_disp_set_rotation(disp, LV_DISP_ROT_270);
  ui_init();
  lv_disp_load_scr(ui_mainScreen);
  smartdisplay_lcd_set_backlight(1.0f);
}

// ============================================================================
// DISPLAY TASK
// ============================================================================

void displayTask(void *arg)
{
  static displayQueueItem_t qMesg;

  static uint16_t setPointx10 = 0;
  static bool setPointx10Valid = false;

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

  lv_label_set_text(ui_testLabel, LV_SYMBOL_SETTINGS LV_SYMBOL_OK);

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
          temp_value = qMesg.data.temperature / 10.0;
          setp_value = setPointx10Valid ? setPointx10 / 10.0 : LV_CHART_POINT_NONE;

          if (actuator0 == 1)
          {
            // Y-value is >1 so serie-line will be outside of visible area of graph !
            lv_chart_set_next_value(ui_temperatureChart, ui_cool_serie, 2);
          }
          else
          {
            lv_chart_set_next_value(ui_temperatureChart, ui_cool_serie, LV_CHART_POINT_NONE);
          }

          if (actuator1 == 1)
          {
            // Y-value is >1 so serie-line will be outside of visible area of graph !
            lv_chart_set_next_value(ui_temperatureChart, ui_heat_serie, 2);
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
            y_min = setPointx10 / 10;
            y_max = setPointx10 / 10;
          }
          else
          {
            // init min-max with first data-point
            y_min = *y_array;
            y_max = *y_array;
          }

          // loop from FIRSt to last data-point. Also works if we have only 1 datapoint.
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

        printf("[DISPLAY] 2 e_actuator\n");

        if ((actuator0 == 0) && (actuator1 == 0))
        {
          printf("[DISPLAY] 3a e_actuator\n");

          // lv_label_set_text(ui_coolHeatLabel, "OFF");
          // lv_obj_add_style(ui_coolHeatLabel, &style_text_off, 0);
          // lv_obj_add_flag(ui_coolHeatBar, LV_OBJ_FLAG_HIDDEN);

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
            // printf("[DISPLAY] 3b e_actuator\n");

            lv_obj_clear_flag(ui_coolLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_heatLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_offLabel,LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_coolBar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_heatBar, LV_OBJ_FLAG_HIDDEN);

            // cool can start at 0%
            lv_bar_set_value(ui_coolBar, 0, LV_ANIM_OFF);
            // printf("[DISPLAY] 3c e_actuator\n");
          }

          if (actuator1 == 1)
          {
            // printf("[DISPLAY] 3d e_actuator\n");

            lv_obj_add_flag(ui_coolLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_heatLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_offLabel,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_coolBar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_heatBar, LV_OBJ_FLAG_HIDDEN);

            // Heat is always 100%
            lv_bar_set_value(ui_heatBar, 100, LV_ANIM_OFF);
            // printf("[DISPLAY] 3e e_actuator\n");
          }
        }

        // printf("[DISPLAY] 4a e_actuator\n");
        vTaskDelay(100 / portTICK_RATE_MS); 
        // printf("[DISPLAY] 4b e_actuator\n");
        break;

      case e_delay:
        static uint32_t perc;
        perc = 100 - (100 * qMesg.data.compDelay) / CFG_RELAY0_ON_DELAY;
        lv_bar_set_value(ui_coolBar, perc, LV_ANIM_OFF);
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
  }
};

#endif

// end of file