#include "home_app.h"
#include "./app_manager.h"
#include "ui_styles.h"
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include <stdio.h>
#include "apps/bt/bt_app.h"
#include "apps/wifi/wifi_app.h"
#include "./apps/video_player/video_player_app.h"  // Keep this path
#include "esp_log.h"

static lv_obj_t *home_screen = NULL;

static void app_button_event_cb(lv_event_t *e) {
    app_id_t target = (app_id_t)(uintptr_t)lv_event_get_user_data(e);
    app_manager_switch_to(target);
}

static void test_video_button_event_cb(lv_event_t *e) {
    ESP_LOGI("HOME_APP", "Creating test video...");
    
    // Create test video file
    video_player_create_test_file();
    
    // Show completion message
    lv_obj_t* mbox = lv_msgbox_create(lv_scr_act(), "Test Video Created", 
                                      "Test video saved to:\n/sdcard/test_video.mjpeg\n\n"
                                      "60 frames, 30 FPS\nGo to Folder app to play it!", 
                                      NULL, true);
    lv_obj_center(mbox);
    
    ESP_LOGI("HOME_APP", "Test video creation complete");
}

void create_home_app(void) {
    if (home_screen) return; // already created
    
    ESP_LOGI("HOME_APP", "Creating home screen");
    home_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(home_screen, lv_color_hex(UI_COLOR_BG_DARK), 0);

    // Title
    lv_obj_t *title = lv_label_create(home_screen);
    lv_label_set_text(title, "Home Screen");
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    // WiFi button (top left)
    lv_obj_t *wifi_btn = lv_btn_create(home_screen);
    lv_obj_set_size(wifi_btn, 120, 60);
    lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(UI_COLOR_WIFI), 0);
    lv_obj_set_style_radius(wifi_btn, 10, 0);
    lv_obj_align(wifi_btn, LV_ALIGN_CENTER, -80, -40);
    lv_obj_add_event_cb(wifi_btn, app_button_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)APP_WIFI);
    
    lv_obj_t *wifi_label = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_label, "WiFi");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(wifi_label);

    // Bluetooth button (top right)
    lv_obj_t *bt_btn = lv_btn_create(home_screen);
    lv_obj_set_size(bt_btn, 120, 60);
    lv_obj_set_style_bg_color(bt_btn, lv_color_hex(UI_COLOR_SECONDARY), 0);
    lv_obj_set_style_radius(bt_btn, 10, 0);
    lv_obj_align(bt_btn, LV_ALIGN_CENTER, 80, -40);
    lv_obj_add_event_cb(bt_btn, app_button_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)APP_BLUETOOTH);
    
    lv_obj_t *bt_label = lv_label_create(bt_btn);
    lv_label_set_text(bt_label, "Bluetooth");
    lv_obj_set_style_text_color(bt_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(bt_label);

    // Folder button (center)
    lv_obj_t *folder_btn = lv_btn_create(home_screen);
    lv_obj_set_size(folder_btn, 120, 60);
    lv_obj_set_style_bg_color(folder_btn, lv_color_hex(UI_COLOR_SECONDARY), 0);
    lv_obj_set_style_radius(folder_btn, 10, 0);
    lv_obj_align(folder_btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_event_cb(folder_btn, app_button_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)APP_FOLDER);
    
    lv_obj_t *folder_label = lv_label_create(folder_btn);
    lv_label_set_text(folder_label, "Folder");
    lv_obj_set_style_text_color(folder_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(folder_label);

    // Test Video button (bottom)
    lv_obj_t *test_btn = lv_btn_create(home_screen);
    lv_obj_set_size(test_btn, 160, 50);
    lv_obj_set_style_bg_color(test_btn, lv_color_hex(0xFF5722), 0); // Orange color
    lv_obj_set_style_radius(test_btn, 10, 0);
    lv_obj_align(test_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(test_btn, test_video_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *test_label = lv_label_create(test_btn);
    lv_label_set_text(test_label, "Create Test Video");
    lv_obj_set_style_text_color(test_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(test_label, &lv_font_montserrat_12, 0);
    lv_obj_center(test_label);
    
    app_info_t* app_info = app_manager_get_app_info(APP_HOME);
    if (app_info) {
        app_info->screen = home_screen;
        ESP_LOGI("HOME_APP", "Home screen linked to app manager");
    }
    
    ESP_LOGI("HOME_APP", "Home app created successfully");
}

void destroy_home_app(void) {
    if (home_screen) {
        // Optional: Add printf for debugging if needed
        // printf("Home app destroyed\n");
        home_screen = NULL;
    }
}

lv_obj_t* home_app_get_screen(void) {
    return home_screen;
}