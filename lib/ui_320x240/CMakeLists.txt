SET(SOURCES ui_mainScreen.c
    ui_configurationScreen.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    fonts/ui_font_weight100Font.c
    fonts/ui_font_weight25Font.c
    fonts/ui_font_weight50Font.c)

add_library(ui ${SOURCES})
