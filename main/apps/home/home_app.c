#include "home_app.h"
#include "./app_manager.h"
#include "ui_styles.h"
#include "sd_card_manager.h"
#include "esp_wifi.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include <stdio.h>
#include "apps/bt/bt_app.h"
#include "apps/wifi/wifi_app.h"
#include "./apps/video_player/video_player_app.h"
#include "esp_log.h"

static lv_obj_t *home_screen = NULL;
static const char* TAG = "HOME_APP";

// Simple ADC for battery reading (no complex initialization)
static esp_adc_cal_characteristics_t adc_chars;
static bool adc_simple_init = false;

// Forward declarations
static void app_button_event_cb(lv_event_t *e);
static float read_battery_voltage_simple(void);
static void create_status_bar_simple(lv_obj_t *parent);

// =================== SIMPLE ADC READING ===================
static float read_battery_voltage_simple(void) {
    if (!adc_simple_init) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);
        adc_simple_init = true;
    }
    
    int adc_reading = adc1_get_raw(ADC1_CHANNEL_6);
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
    return voltage_mv / 1000.0f;
}

// =================== SIMPLE STATUS BAR ===================
static void create_status_bar_simple(lv_obj_t *parent) {
    // Simple status bar - no complex nested objects
    lv_obj_t *status_bar = lv_obj_create(parent);
    lv_obj_set_size(status_bar, lv_pct(100), 25);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 2, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi status (left) - read once, no updates
    lv_obj_t *wifi_label = lv_label_create(status_bar);
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI " WiFi");
        lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI " No WiFi");
        lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xFF4444), 0);
    }
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_10, 0);
    lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, 5, 0);

    // Storage status (center) - read once, no updates
    lv_obj_t *storage_label = lv_label_create(status_bar);
    if (sd_is_mounted()) {
        uint64_t total_bytes, free_bytes;
        if (sd_get_space_info(&total_bytes, &free_bytes) == ESP_OK) {
            char storage_text[32];
            float free_gb = free_bytes / (1024.0f * 1024.0f * 1024.0f);
            snprintf(storage_text, sizeof(storage_text), "%s %.1fGB", LV_SYMBOL_SD_CARD, free_gb);
            lv_label_set_text(storage_label, storage_text);
        } else {
            lv_label_set_text(storage_label, LV_SYMBOL_SD_CARD " SD OK");
        }
        lv_obj_set_style_text_color(storage_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(storage_label, LV_SYMBOL_SD_CARD " No SD");
        lv_obj_set_style_text_color(storage_label, lv_color_hex(0xFF4444), 0);
    }
    lv_obj_set_style_text_font(storage_label, &lv_font_montserrat_10, 0);
    lv_obj_align(storage_label, LV_ALIGN_CENTER, 0, 0);

    // Battery status (right) - read once, no updates
    lv_obj_t *battery_label = lv_label_create(status_bar);
    float voltage = read_battery_voltage_simple();
    char battery_text[32];
    
    const char* battery_symbol;
    lv_color_t battery_color;
    
    if (voltage >= 4.0f) {
        battery_symbol = LV_SYMBOL_BATTERY_FULL;
        battery_color = lv_color_hex(0x00FF00);
    } else if (voltage >= 3.7f) {
        battery_symbol = LV_SYMBOL_BATTERY_3;
        battery_color = lv_color_hex(0x8BC34A);
    } else if (voltage >= 3.4f) {
        battery_symbol = LV_SYMBOL_BATTERY_2;
        battery_color = lv_color_hex(0xFFC107);
    } else if (voltage >= 3.0f) {
        battery_symbol = LV_SYMBOL_BATTERY_1;
        battery_color = lv_color_hex(0xFF9800);
    } else {
        battery_symbol = LV_SYMBOL_BATTERY_EMPTY;
        battery_color = lv_color_hex(0xFF4444);
    }

    snprintf(battery_text, sizeof(battery_text), "%s %.2fV", battery_symbol, voltage);
    lv_label_set_text(battery_label, battery_text);
    lv_obj_set_style_text_color(battery_label, battery_color, 0);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_10, 0);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -5, 0);
}

// =================== APP BUTTON CALLBACK ===================
static void app_button_event_cb(lv_event_t *e) {
    app_id_t target = (app_id_t)(uintptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Switching to app: %d", target);
    app_manager_switch_to(target);
}

// =================== CREATE HOME APP ===================
void create_home_app(void) {
    if (home_screen) return; // already created
    
    ESP_LOGI(TAG, "Creating home screen");
    home_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(home_screen, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_pad_all(home_screen, 0, 0);

    // Create status bar (no timers, no complex objects)
    create_status_bar_simple(home_screen);

    // App buttons in 2x2 grid layout
    const int button_width = 140;
    const int button_height = 80;
    const int spacing_x = 160;
    const int spacing_y = 100;
    lv_coord_t start_x = -spacing_x / 2;
    lv_coord_t start_y = -spacing_y / 2 + 20; // Offset for status bar

    // WiFi button (top left)
    lv_obj_t *wifi_btn = lv_btn_create(home_screen);
    lv_obj_set_size(wifi_btn, button_width, button_height);
    lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(UI_COLOR_WIFI), 0);
    lv_obj_set_style_radius(wifi_btn, 15, 0);
    lv_obj_align(wifi_btn, LV_ALIGN_CENTER, start_x, start_y);
    lv_obj_add_event_cb(wifi_btn, app_button_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)APP_WIFI);
    
    lv_obj_t *wifi_icon = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(wifi_icon, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t *wifi_label = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_label, "WiFi");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_12, 0);
    lv_obj_align(wifi_label, LV_ALIGN_CENTER, 0, 15);

    // Folder button (top right)
    lv_obj_t *folder_btn = lv_btn_create(home_screen);
    lv_obj_set_size(folder_btn, button_width, button_height);
    lv_obj_set_style_bg_color(folder_btn, lv_color_hex(UI_COLOR_SECONDARY), 0);
    lv_obj_set_style_radius(folder_btn, 15, 0);
    lv_obj_align(folder_btn, LV_ALIGN_CENTER, start_x + spacing_x, start_y);
    lv_obj_add_event_cb(folder_btn, app_button_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)APP_FOLDER);
    
    lv_obj_t *folder_icon = lv_label_create(folder_btn);
    lv_label_set_text(folder_icon, LV_SYMBOL_DIRECTORY);
    lv_obj_set_style_text_color(folder_icon, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(folder_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(folder_icon, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t *folder_label = lv_label_create(folder_btn);
    lv_label_set_text(folder_label, "Files");
    lv_obj_set_style_text_color(folder_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(folder_label, &lv_font_montserrat_12, 0);
    lv_obj_align(folder_label, LV_ALIGN_CENTER, 0, 15);

    // Video Player button (bottom left)
    lv_obj_t *video_btn = lv_btn_create(home_screen);
    lv_obj_set_size(video_btn, button_width, button_height);
    lv_obj_set_style_bg_color(video_btn, lv_color_hex(0xFF5722), 0);
    lv_obj_set_style_radius(video_btn, 15, 0);
    lv_obj_align(video_btn, LV_ALIGN_CENTER, start_x, start_y + spacing_y);
    lv_obj_add_event_cb(video_btn, app_button_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)APP_VIDEO_PLAYER);
    
    lv_obj_t *video_icon = lv_label_create(video_btn);
    lv_label_set_text(video_icon, LV_SYMBOL_VIDEO);
    lv_obj_set_style_text_color(video_icon, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(video_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(video_icon, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t *video_label = lv_label_create(video_btn);
    lv_label_set_text(video_label, "Video");
    lv_obj_set_style_text_color(video_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(video_label, &lv_font_montserrat_12, 0);
    lv_obj_align(video_label, LV_ALIGN_CENTER, 0, 15);

    // Music button (bottom right)
    lv_obj_t *music_btn = lv_btn_create(home_screen);
    lv_obj_set_size(music_btn, button_width, button_height);
    lv_obj_set_style_bg_color(music_btn, lv_color_hex(0x9C27B0), 0);
    lv_obj_set_style_radius(music_btn, 15, 0);
    lv_obj_align(music_btn, LV_ALIGN_CENTER, start_x + spacing_x, start_y + spacing_y);
    // Note: Music app not implemented yet
    
    lv_obj_t *music_icon = lv_label_create(music_btn);
    lv_label_set_text(music_icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(music_icon, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(music_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(music_icon, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t *music_label = lv_label_create(music_btn);
    lv_label_set_text(music_label, "Music");
    lv_obj_set_style_text_color(music_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(music_label, &lv_font_montserrat_12, 0);
    lv_obj_align(music_label, LV_ALIGN_CENTER, 0, 15);

    // Link to app manager
    app_info_t* app_info = app_manager_get_app_info(APP_HOME);
    if (app_info) {
        app_info->screen = home_screen;
        ESP_LOGI(TAG, "Home screen linked to app manager");
    }
    
    ESP_LOGI(TAG, "Home app created successfully");
}

// =================== DESTROY HOME APP (FOLLOWS WORKING PATTERN) ===================
void destroy_home_app(void) {
    if (home_screen) {
        ESP_LOGI(TAG, "Home app destroyed");
        // Follow the working pattern - just clear pointer, no complex cleanup
        home_screen = NULL;
    }
}

// =================== PUBLIC FUNCTIONS ===================
lv_obj_t* home_app_get_screen(void) {
    return home_screen;
}