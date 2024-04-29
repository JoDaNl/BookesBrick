// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.0
// LVGL version: 8.3.6
// Project name: ui_320x240

#ifndef _UI_320X240_UI_H
#define _UI_320X240_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined __has_include
#if __has_include("lvgl.h")
#include "lvgl.h"
#elif __has_include("lvgl/lvgl.h")
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif
#else
#include "lvgl.h"
#endif

#include "ui_helpers.h"
#include "ui_events.h"

// SCREEN: ui_mainScreen
void ui_mainScreen_screen_init(void);
extern lv_obj_t * ui_mainScreen;
extern lv_obj_t * ui_TitleContainer;
extern lv_obj_t * ui_titleLabel;
extern lv_obj_t * ui_tempPanel;
extern lv_obj_t * ui_tempLabel;
extern lv_obj_t * ui_tempLabelDot;
extern lv_obj_t * ui_tempLabelSmall;
extern lv_obj_t * ui_unitLabel;
extern lv_obj_t * ui_setpointPanel;
extern lv_obj_t * ui_setPointLabel;
extern lv_obj_t * ui_weightLabelDot11;
extern lv_obj_t * ui_setPointSmallLabel;
extern lv_obj_t * ui_SETLabel;
extern lv_obj_t * ui_coolHeatPanel;
extern lv_obj_t * ui_offLabel;
extern lv_obj_t * ui_coolLabel;
extern lv_obj_t * ui_heatLabel;
extern lv_obj_t * ui_coolBar;
extern lv_obj_t * ui_heatBar;
extern lv_obj_t * ui_infoPanel;
extern lv_obj_t * ui_temperatureChart;
extern lv_obj_t * ui_statusContainer;
extern lv_obj_t * ui_timeContainer;
extern lv_obj_t * ui_timeLabel;
extern lv_obj_t * ui_backendContainer;
extern lv_obj_t * ui_backendBar;
extern lv_obj_t * ui_networkContainer;
extern lv_obj_t * ui_rssiContainer;
extern lv_obj_t * ui_rssiBar0Panel;
extern lv_obj_t * ui_rssiBar1Panel;
extern lv_obj_t * ui_rssiBar2Panel;
extern lv_obj_t * ui_rssiBar3Panel;
extern lv_obj_t * ui_rssiLine0Panel;
extern lv_obj_t * ui_rssiLine1Panel;
extern lv_obj_t * ui_rssiLine2Panel;
extern lv_obj_t * ui_rssiLine3Panel;
extern lv_obj_t * ui_internetLabel;
extern lv_obj_t * ui_setPanel;
extern lv_obj_t * ui_testLabel;
extern lv_obj_t * ui_setLabel;
extern lv_obj_t * ui_modePanel;
extern lv_obj_t * ui_modeLabel;
extern lv_obj_t * ui_confirmContainer;
extern lv_obj_t * ui_confirmPanel;
extern lv_obj_t * ui_confirmTextLabel;
void ui_event_OKButton(lv_event_t * e);
extern lv_obj_t * ui_OKButton;
extern lv_obj_t * ui_OKLabel;
extern lv_obj_t * ui_CANCELButton;
extern lv_obj_t * ui_CANCELLabel;
extern lv_obj_t * ui_Switch1;
extern lv_obj_t * ui____initial_actions0;





LV_FONT_DECLARE(ui_font_weight100Font);
LV_FONT_DECLARE(ui_font_weight25Font);
LV_FONT_DECLARE(ui_font_weight50Font);



void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif