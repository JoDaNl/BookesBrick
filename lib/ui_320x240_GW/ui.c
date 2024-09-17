// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.2
// LVGL version: 8.3.6
// Project name: ui_320x240_GW

#include "ui.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////


// SCREEN: ui_mainScreen
void ui_mainScreen_screen_init(void);
lv_obj_t * ui_mainScreen;
lv_obj_t * ui_TitleContainer;
lv_obj_t * ui_titleLabel;
lv_obj_t * ui_tempPanel;
lv_obj_t * ui_tempLabel;
lv_obj_t * ui_tempLabelDot;
lv_obj_t * ui_tempLabelSmall;
lv_obj_t * ui_unitLabel;
lv_obj_t * ui_setpointPanel;
lv_obj_t * ui_setPointLabel;
lv_obj_t * ui_weightLabelDot11;
lv_obj_t * ui_setPointSmallLabel;
lv_obj_t * ui_SETLabel;
lv_obj_t * ui_coolHeatPanel;
lv_obj_t * ui_offLabel;
lv_obj_t * ui_coolLabel;
lv_obj_t * ui_heatLabel;
lv_obj_t * ui_coolBar;
lv_obj_t * ui_heatBar;
void ui_event_infoPanel(lv_event_t * e);
lv_obj_t * ui_infoPanel;
lv_obj_t * ui_temperatureChart;
lv_obj_t * ui_statusContainer;
lv_obj_t * ui_timeContainer;
lv_obj_t * ui_timeLabel;
lv_obj_t * ui_backendContainer;
lv_obj_t * ui_backendBar;
lv_obj_t * ui_networkContainer;
lv_obj_t * ui_rssiContainer;
lv_obj_t * ui_rssiBar0Panel;
lv_obj_t * ui_rssiBar1Panel;
lv_obj_t * ui_rssiBar2Panel;
lv_obj_t * ui_rssiBar3Panel;
lv_obj_t * ui_rssiLine0Panel;
lv_obj_t * ui_rssiLine1Panel;
lv_obj_t * ui_rssiLine2Panel;
lv_obj_t * ui_rssiLine3Panel;
lv_obj_t * ui_internetLabel;
lv_obj_t * ui_messageContainer;
lv_obj_t * ui_messageLabel;
lv_obj_t * ui_confirmContainer;


// SCREEN: ui_configurationScreen
void ui_configurationScreen_screen_init(void);
lv_obj_t * ui_configurationScreen;
lv_obj_t * ui_TitleContainer1;
lv_obj_t * ui_titleLabel1;
lv_obj_t * ui_tempPanel1;
lv_obj_t * ui_statusContainer1;
lv_obj_t * ui_confirmContainer1;
lv_obj_t * ui_confirmPanel1;
lv_obj_t * ui_confirmTextLabel1;
lv_obj_t * ui_CANCELButton1;
lv_obj_t * ui_CANCELLabel1;
lv_obj_t * ui_Switch2;
lv_obj_t * ui____initial_actions0;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=1
    #error "LV_COLOR_16_SWAP should be 1 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void ui_event_infoPanel(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        cb_clicked(e);
    }
}

///////////////////// SCREENS ////////////////////

void ui_init(void)
{
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                               false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    ui_mainScreen_screen_init();
    ui_configurationScreen_screen_init();
    ui____initial_actions0 = lv_obj_create(NULL);
    lv_disp_load_scr(ui_mainScreen);
}
