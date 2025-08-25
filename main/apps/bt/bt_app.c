#include "bt_app.h"
#include "app_manager.h"
#include "ui_styles.h"
#include "esp_log.h"

lv_obj_t* bt_screen = NULL;

static void back_button_event_cb(lv_event_t* e) {
    app_manager_go_home();
}

void create_bt_app(void) {
    if (bt_screen) return;

    // Create screen
    bt_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(bt_screen, lv_color_hex(UI_COLOR_BG_DARK), 0);

    // Create title
    lv_obj_t* title = lv_label_create(bt_screen);
    lv_label_set_text(title, "Bluetooth Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    // Create back button
    lv_obj_t* back_btn = lv_btn_create(bt_screen);
    lv_obj_set_size(back_btn, 120, 50);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COLOR_SECONDARY), 0);
    lv_obj_set_style_radius(back_btn, 10, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);

    // Button label
    lv_obj_t* btn_label = lv_label_create(back_btn);
    lv_label_set_text(btn_label, "Back");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(btn_label);

    ESP_LOGI("BT_APP", "Bluetooth app created");

        app_info_t* app_info = app_manager_get_app_info(APP_BLUETOOTH);
    if (app_info) {
        app_info->screen = bt_screen;
        ESP_LOGI("BT_APP", "Bluetooth screen linked to app manager");
    }
}

void destroy_bt_app(void) {
    if (bt_screen) {
        ESP_LOGI("BT_APP", "Bluetooth app destroyed");
        // Clear the app registry first
        app_info_t* app_info = app_manager_get_app_info(APP_BLUETOOTH);
        if (app_info) {
            app_info->screen = NULL;
        }
        bt_screen = NULL;
    }
}