// display.c

#ifndef CFG_DISPLAY_NONE

#include <Arduino.h>
#include <ui.h>
#include "config.h"
#include "display.h"

#define LOG_TAG "DISP"

static QueueHandle_t displayQueue = NULL;
static TaskHandle_t displayTaskHandle = NULL;


// color defintions for chart series
static const lv_color_t tempGraphColor = lv_color_make(0x10, 0x80, 0x10);
static const lv_color_t setpGraphColor = lv_color_make(0x10, 0x10, 0x10);
static const lv_color_t coolGraphColor = lv_color_make(0x00, 0x00, 0xFF);
static const lv_color_t heatGraphColor = lv_color_make(0xFF, 0x00, 0x00);

// temperature chart series
static lv_chart_series_t *ui_temp_serie;
static lv_chart_series_t *ui_setp_serie;
static lv_chart_series_t *ui_cool_serie;
static lv_chart_series_t *ui_heat_serie;


#if (CFG_ENABLE_HYDROBRICK == true)
const lv_color_t tempHBGraphColor = lv_color_make(0xFF, 0x80, 0x00);
static lv_chart_series_t *ui_tempHB_serie;
#endif



// See : https://forum.lvgl.io/t/how-do-i-scroll-the-contents-of-a-lv-win-up-when-the-keyboard-pops-up-so-that-the-active-textarea-remains-in-view-using-lvgl8-1-0/6158
// See : https://docs.lvgl.io/master/examples.html#faded-area-line-chart-with-custom-division-lines

// UI callbacks

static void cb_chart_draw_event(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  lv_chart_series_t *serie;
  lv_area_t area;
  lv_obj_draw_part_dsc_t *dsc;

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
    lv_snprintf(dsc->text, sizeof(dsc->text), "%d", dsc->value / 10);
  }
}

void cb_clicked(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = lv_event_get_target(e);
}

static void displayTemperature(float value, bool valid, lv_obj_t *label, lv_obj_t *labelSmall, float lowRange, float highRange)
{
  String valueStr;
  String leftStr;
  String rightStr;
  int dotPos;

  valueStr = String(round(value * 10) / 10, 1);
  dotPos = valueStr.indexOf(".");

  leftStr = valueStr.substring(0, dotPos);
  rightStr = valueStr.substring(dotPos + 1);

  if (valid)
  {
    if (value > highRange)
    {
      // weight to high
      lv_label_set_text(label, "HI");
      if (labelSmall != NULL)
      {
        lv_label_set_text(labelSmall, "-");
      }
    }
    else if (value < lowRange)
    {
      // weight to low
      lv_label_set_text(label, "LO");
      if (labelSmall != NULL)
      {
        lv_label_set_text(labelSmall, "-");
      }
    }
    else
    {
      // show weight
      lv_label_set_text(label, leftStr.c_str());
      if (labelSmall != NULL)
      {
        lv_label_set_text(labelSmall, rightStr.c_str());
      }
    }
  }
  else
  {
    // no valid temperature
    lv_label_set_text(label, "--");
    if (labelSmall != NULL)
    {
      lv_label_set_text(labelSmall, "-");
    }
  }
}


// ============================================================================
// DISPLAY TASK
// ============================================================================

void displayTask(void *arg)
{
  displayQueueItem_t qMesg;

  uint16_t setPointx10 = 0;
  bool setPointx10Valid = false;

#if (CFG_ENABLE_HYDROBRICK == true)
  uint16_t tempHBx10 = 0;
  bool tempHBvalid = false;
#endif  

  uint32_t compDelayPerc = 0;

  uint8_t actuator0 = 0;
  uint8_t actuator1 = 0;

  bool heartbeatToggle = false;

  bool timeToggle = false;
  uint32_t tickCount = 0;
  uint32_t prevTickCount = 0;
  uint8_t timeHours;
  uint8_t timeMinutes;

  bool timeValid = false;
  bool setpValid = false;
  lv_coord_t temp_value;
  lv_coord_t setp_value;

  String displayText;
  uint8_t displayTextTime = 0;
  bool displayTextPersistent = false;
  
  // lv_label_set_text(ui_testLabel, LV_SYMBOL_SETTINGS LV_SYMBOL_OK);
  ui_temp_serie = lv_chart_add_series(ui_temperatureChart, tempGraphColor, LV_CHART_AXIS_PRIMARY_Y);
  ui_setp_serie = lv_chart_add_series(ui_temperatureChart, setpGraphColor, LV_CHART_AXIS_PRIMARY_Y);
  ui_cool_serie = lv_chart_add_series(ui_temperatureChart, coolGraphColor, LV_CHART_AXIS_SECONDARY_Y);
  ui_heat_serie = lv_chart_add_series(ui_temperatureChart, heatGraphColor, LV_CHART_AXIS_SECONDARY_Y);

#if (CFG_ENABLE_HYDROBRICK == true)
  ui_tempHB_serie = lv_chart_add_series(ui_temperatureChart, tempHBGraphColor, LV_CHART_AXIS_PRIMARY_Y);
#endif


  // set range for heal/cool background (on secondary axis) from 0 to 1
  lv_chart_set_range(ui_temperatureChart, LV_CHART_AXIS_SECONDARY_Y, 0, 1);

  lv_obj_set_style_size(ui_temperatureChart, 0, LV_PART_INDICATOR); // set dot-size to 0
  lv_obj_add_event_cb(ui_temperatureChart, cb_chart_draw_event, LV_EVENT_DRAW_PART_BEGIN, NULL);

  while (true)
  {
    if (xQueueReceive(displayQueue, &qMesg, 100 / portTICK_PERIOD_MS) == pdTRUE)
    {
      // printf("[DISP] received qMesg.type=%d\n", qMesg.type);

      switch (qMesg.type)
      {
      case e_temperature:
        // printf("[DISP] %2.1f\n", qMesg.data.temperature / 10.0);
        displayTemperature(qMesg.data.temperature / 10.0, qMesg.valid, ui_tempLabel, ui_tempLabelSmall, 0, 100);

        // Update graph
        if (qMesg.valid)
        {
          // both temp & set-point are x10
          temp_value = qMesg.data.temperature;
          setp_value = setPointx10Valid ? setPointx10 : LV_CHART_POINT_NONE;

          if (actuator0 == 1)
          {
            // Y-value is >1 so serie-line will be outside of visible area of graph !
            lv_chart_set_next_value(ui_temperatureChart, ui_cool_serie, 2 * 10);
          }
          else
          {
            lv_chart_set_next_value(ui_temperatureChart, ui_cool_serie, LV_CHART_POINT_NONE);
          }

          if (actuator1 == 1)
          {
            // Y-value is >1 so serie-line will be outside of visible area of graph !
            lv_chart_set_next_value(ui_temperatureChart, ui_heat_serie, 2 * 10);
          }
          else
          {
            lv_chart_set_next_value(ui_temperatureChart, ui_heat_serie, LV_CHART_POINT_NONE);
          }

          lv_chart_set_next_value(ui_temperatureChart, ui_setp_serie, setp_value);
          lv_chart_set_next_value(ui_temperatureChart, ui_temp_serie, temp_value);

          // re-use temp_value for HydroBrick
#if (CFG_ENABLE_HYDROBRICK == true)
          temp_value = tempHBx10;
          if (tempHBvalid)
          {
            lv_chart_set_next_value(ui_temperatureChart, ui_tempHB_serie, temp_value);
          }
          else
          {
            lv_chart_set_next_value(ui_temperatureChart, ui_tempHB_serie, LV_CHART_POINT_NONE);
          }
#endif

          // Get min/max values from temperature in graph in order to auto-scale graph

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

#if (CFG_ENABLE_HYDROBRICK == true)
          y_count = lv_chart_get_point_count(ui_temperatureChart);
          y_array = lv_chart_get_y_array(ui_temperatureChart, ui_tempHB_serie);

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

#endif
          // If y_min & y_max are value-wise close to each other the graph's y-axis may display
          // the same label 2 or 3 times.
          // assure the veritical axis covers a minumum of 3 degrees
          // note values are * 10
          if ((y_max - y_min) < 30)
          {
            y_max = y_max + 15;
            y_min = y_min - 15;
          }

          // printf("[DISP] Graph scale: min=%d, max=%d\n", y_min, y_max);
          lv_chart_set_range(ui_temperatureChart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
        }
        break;

      case e_setpoint:
        ESP_LOGI(LOG_TAG, "setpoint=%2.1f", qMesg.data.temperature / 10.0);
        displayTemperature(qMesg.data.temperature / 10.0, qMesg.valid, ui_setPointLabel, ui_setPointSmallLabel, 0, 100);
        setPointx10 = qMesg.data.temperature;
        setPointx10Valid = qMesg.valid;
        break;

      case e_error:
        break;

      case e_actuator:
        ESP_LOGI(LOG_TAG, "e_actuator=%d", qMesg.data.actuators);

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
          lv_obj_clear_flag(ui_offLabel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_coolBar, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_heatBar, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {

          if (actuator0 == 1)
          {
            lv_obj_clear_flag(ui_coolLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_heatLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_offLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_coolBar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_heatBar, LV_OBJ_FLAG_HIDDEN);

            // cool starts at compressor-delay value
            lv_bar_set_value(ui_coolBar, compDelayPerc, LV_ANIM_OFF);
          }

          if (actuator1 == 1)
          {
            lv_obj_add_flag(ui_coolLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_heatLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_offLabel, LV_OBJ_FLAG_HIDDEN);
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
          // printf("[DISP] compressor-delay=%d sec, %d\%\n",qMesg.data.compDelay,compDelayPerc);
          lv_bar_set_value(ui_coolBar, compDelayPerc, LV_ANIM_OFF);
        }
        break;

      case e_wifi:
        // RSSI  Value       Range WiFi Signal Strength
        // RSSI  > -30 dBm   Amazing
        // RSSI  < –67 dBm	 Fairly Good
        // RSSI  < –70 dBm	 Okay
        // RSSI  < –80 dBm	 Not good
        // RSSI  < –90 dBm	 Extremely weak signal
        static uint16_t rssi;
        static char label[2];

        if (qMesg.valid)
        {
          rssi = qMesg.data.wifiData.rssi; // rssi is a signed value
          label[0] = qMesg.data.wifiData.status; // set first character of string

          // printf("[DISP] WIFI-STATUS=%c\n", qMesg.data.wifiData.status);
          ESP_LOGI(LOG_TAG, "RSSI=%d", rssi);

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
        }
        else
        {
          lv_obj_clear_flag(ui_rssiBar0Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar1Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar2Panel, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_flag(ui_rssiBar3Panel, LV_OBJ_FLAG_HIDDEN);

          label[0] = '*';
        }

        label[1] = 0; // terminate string
        lv_label_set_text(ui_internetLabel, label);
        break;

      case e_time:
        timeHours = qMesg.data.time >> 8;    // hours in MS byte
        timeMinutes = qMesg.data.time & 255; // minutes in LS byte
        timeValid = qMesg.valid;
        break;

      case e_heartbeat:
      {
        uint8_t hearbeatValue;
        uint8_t hearbeatMax;

        const lv_color_t green_bar = lv_color_make(0x00, 0x77, 0x0F);
        const lv_color_t red_bar = lv_color_make(0x77, 0x00, 0x0F);

        lv_color_t color;

        hearbeatValue = qMesg.data.heartBeat & 255;
        hearbeatMax = qMesg.data.heartBeat >> 8;

        if (qMesg.valid)
        {
          color = green_bar;
        }
        else
        {
          color = red_bar;
        }

        lv_obj_set_style_bg_color(ui_backendBar, color, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        lv_bar_set_range(ui_backendBar, 0, hearbeatMax + 1);
        lv_bar_set_value(ui_backendBar, hearbeatMax - hearbeatValue + 1, LV_ANIM_ON);
      }
      break;

      case e_display_text:
        static String *stringPtr;

        stringPtr = qMesg.data.textData.stringPtr;

        if (stringPtr == NULL)
        {
          ESP_LOGI(LOG_TAG, "Display-text = NULL");
        }
        else
        {
          displayText = *stringPtr;
          switch (qMesg.data.textData.messageType)
          {
          case e_device_name:
            lv_label_set_text(ui_titleLabel, displayText.c_str());
            break;
          case e_status_bar:
            displayTextTime = qMesg.data.textData.timeInSec;
            displayTextPersistent = (displayTextTime == 0);
            lv_label_set_text(ui_messageLabel, displayText.c_str());
            ESP_LOGI(LOG_TAG, "status message=%s", displayText);
            break;
          }

          // Important: string has been used...time to delete it !!!
          delete stringPtr; 
        }
        break;


#if (CFG_ENABLE_HYDROBRICK == true)
        case e_specific_gravity:
          ESP_LOGI(LOG_TAG, " e_specific_gravity=%2.1f", qMesg.data.specificGravity / 1000.0);
          displayTemperature(qMesg.data.specificGravity, qMesg.valid, ui_gravityLabel, NULL, 1000, 9999);       
        break;

        case e_hb_temperature:
          ESP_LOGI(LOG_TAG, " e_hb_temperature=%2.1f", qMesg.data.temperature / 10.0);
          displayTemperature(qMesg.data.temperature / 10.0, qMesg.valid, ui_hbTempLabel, ui_hbTempLabelSmall, 0, 100);

          tempHBvalid = qMesg.data.temperature;
          tempHBx10 = qMesg.data.temperature;
        break;

        case e_voltage:
          ESP_LOGI(LOG_TAG, " e_voltage=%1.3f", qMesg.data.voltage / 1000.0);
          displayTemperature(qMesg.data.voltage / 10.0, qMesg.valid, ui_voltLabel, NULL, 0, 5000); 
        break;
#endif
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

      // handle status text
      if (~displayTextPersistent)
      {
        if (displayTextTime == 0)
        {
          lv_label_set_text(ui_messageLabel, "");
        }
        else
        {
          if (displayTextTime > 0)
          {
            displayTextTime--;
          }
        }
      }
    }

    // Update LVGL-GUI
    // ESP_LOGE(LOG_TAG, "BEFORE LV_TIMER_HANDLER");    
    // lv_timer_handler();
    // ESP_LOGE(LOG_TAG, "AFTER LV_TIMER_HANDLER");    
    updateLVGL();

#if (CFG_ENABLE_SCREENSHOT == true)
    static int prevButton = 1;
    extern lv_disp_drv_t disp_drv;
    uint8_t *ptr;

    // GPIO_NUM_0 == BOOT button
    if (digitalRead(GPIO_NUM_0) == 0 && prevButton == 1)
    {
      ESP_LOGI(LOG_TAG, "SCREENSHOT - TESTCODE");
    }
    prevButton = digitalRead(GPIO_NUM_0);
#endif

  }
}




//helper function : send text-message to display
void displayText(String * message, displayMessageType_t messageType, uint8_t timeInSec)
{
  displayQueueItem_t   displayQMesg;

  displayQMesg.type  = e_display_text;
  displayQMesg.data.textData.stringPtr = message; 
  displayQMesg.data.textData.timeInSec = timeInSec;
  displayQMesg.data.textData.messageType = messageType;
  displayQMesg.valid = true;

  displayQueueSend(&displayQMesg , 0);
}


// wrapper for sendQueue 
int displayQueueSend(displayQueueItem_t * displayQMesg, TickType_t xTicksToWait)
{
  int r;
  r = pdTRUE;
#ifndef CFG_DISPLAY_NONE
  if (displayQueue != NULL)
  {
    r =  xQueueSend(displayQueue, displayQMesg , xTicksToWait);
  }
#endif
  return r;
}


#endif // CFG_DISPLAY_NONE


void initDisplay(void)
{
  int r;

#ifdef CFG_DISPLAY_NONE
  ESP_LOGI(LOG_TAG, "No display enabled");
#else

#ifdef CFG_DISPLAY_CYD_320x240
  initLVGL();
#elif CFG_DISPLAY_CYD_480x320
  initLVGL();
#elif CFG_DISPLAY_LILYGO_TDISPLAY_S3
  initLVGL();
#elif CFG_DISPLAY_JC3248W535
  initLVGL();
#endif


  displayQueue = xQueueCreate(5, sizeof(displayQueueItem_t));
  if (displayQueue == 0)
  {
    ESP_LOGE(LOG_TAG, "Cannot create displayQueue");
  }

  // create task
  r = xTaskCreatePinnedToCore(displayTask, "displayTask", 8 * 1024, NULL, 10, &displayTaskHandle, 1); 

  if (r != pdPASS)
  {
    ESP_LOGE(LOG_TAG, "Could not create task, error-code=%d", r);
  }

#endif // CFG_DISPLAY_NONE
}



// end of file
