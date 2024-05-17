SET(SOURCES screens/ui_mainScreen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_icons_6asset_38_png.c
    images/ui_img_icons_1asset_33_png.c
    images/ui_img_1173369594.c
    images/ui_img_icons_2asset_34_png.c
    images/ui_img_517907122.c
    images/ui_img_icons_3asset_35_png.c
    images/ui_img_556913678.c
    images/ui_img_icons_4asset_36_png.c
    images/ui_img_1949788358.c
    images/ui_img_icons_5asset_37_png.c
    images/ui_img_1745259782.c
    images/ui_img_1659350234.c
    fonts/ui_font_weight100Font.c
    fonts/ui_font_weight25Font.c
    fonts/ui_font_weight50Font.c)

add_library(ui ${SOURCES})
