#include "wifi_app.h"
#include "app_manager.h"
#include "ui_styles.h"
#include "esp_log.h"
#include "sd_card_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include <string.h>
#include <stdlib.h>

lv_obj_t* wifi_screen = NULL;
static lv_obj_t* wifi_list_cont = NULL;
static const char* TAG = "WIFI_APP";
static bool wifi_initialized = false;

// =================== SD CARD FILE ===================
#define WIFI_CRED_FILE SD_PATH("wifi_credentials.txt")

typedef struct {
    char ssid[32];
    char password[64];
} wifi_cred_t;

// =================== FORWARD DECLARATIONS ===================
static void back_button_event_cb(lv_event_t* e);
static void wifi_connect_btn_cb(lv_event_t* e);
static void wifi_item_event_cb(lv_event_t* e);
static void create_wifi_list(void);
static void load_saved_credentials(wifi_cred_t* cred);
static void save_credentials(const char* ssid, const char* password);
static void wifi_connect(const char* ssid, const char* password);
static bool wifi_auto_connect(void);

static void wifi_driver_init(void) {
    if (wifi_initialized) return;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_initialized = true;
    ESP_LOGI("WIFI_APP", "WiFi driver initialized");
}

// =================== BACK BUTTON ===================
static void back_button_event_cb(lv_event_t* e) {
    app_manager_go_home();
}

// =================== WIFI CONNECT ===================
static void wifi_connect(const char* ssid, const char* password) {
    if (!ssid || !password) return;

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);

    wifi_config_t cfg = {0};
    strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char*)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());

    save_credentials(ssid, password);
}

// =================== SAVE / LOAD CREDENTIALS ===================
static void save_credentials(const char* ssid, const char* password) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s,%s", ssid, password);
    sd_write_file(WIFI_CRED_FILE, buffer);
}

static void load_saved_credentials(wifi_cred_t* cred) {
    char buffer[128] = {0};
    if (sd_read_file(WIFI_CRED_FILE, buffer, sizeof(buffer)) == ESP_OK) {
        char* sep = strchr(buffer, ',');
        if (sep) {
            *sep = 0;
            strncpy(cred->ssid, buffer, sizeof(cred->ssid) - 1);
            strncpy(cred->password, sep + 1, sizeof(cred->password) - 1);
        }
    }
}

// =================== AUTO RECONNECT ===================
static bool wifi_auto_connect(void) {
    wifi_cred_t cred = {0};
    load_saved_credentials(&cred);
    if (strlen(cred.ssid) > 0 && strlen(cred.password) > 0) {
        wifi_connect(cred.ssid, cred.password);
        return true;
    }
    return false;
}

// =================== PASSWORD POPUP CALLBACK ===================
static void wifi_connect_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* popup = lv_obj_get_parent(btn);
    lv_obj_t* ta = lv_obj_get_child(popup, 0); // textarea
    const char* password = lv_textarea_get_text(ta);
    const char* ssid = (const char*)lv_event_get_user_data(e);

    wifi_connect(ssid, password);
    lv_obj_del(popup);
}

// =================== WIFI ITEM CALLBACK ===================
static void wifi_item_event_cb(lv_event_t* e) {
    const char* ssid = (const char*)lv_event_get_user_data(e);

    lv_obj_t* popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(popup, 300, 150);
    lv_obj_center(popup);
    lv_obj_set_style_bg_color(popup, lv_color_hex(UI_COLOR_BG_DARK), 0);

    lv_obj_t* ta = lv_textarea_create(popup);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_placeholder_text(ta, "Enter password");
    lv_obj_set_size(ta, 260, 40);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t* ok_btn = lv_btn_create(popup);
    lv_obj_set_size(ok_btn, 100, 40);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_obj_t* ok_label = lv_label_create(ok_btn);
    lv_label_set_text(ok_label, "Connect");
    lv_obj_center(ok_label);

    lv_obj_add_event_cb(ok_btn, wifi_connect_btn_cb, LV_EVENT_CLICKED, (void*)ssid);
}

// =================== CREATE WIFI LIST ===================
static void create_wifi_list(void) {
    
    wifi_driver_init(); 

    if (wifi_list_cont) lv_obj_del(wifi_list_cont);

    wifi_list_cont = lv_obj_create(wifi_screen);
    lv_obj_set_size(wifi_list_cont, lv_pct(100), lv_pct(60));
    lv_obj_align(wifi_list_cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scroll_dir(wifi_list_cont, LV_DIR_VER);
    lv_obj_set_style_bg_color(wifi_list_cont, lv_color_hex(UI_COLOR_BG_DARK), 0);

    // Scan WiFi networks
    uint16_t ap_num = 20;
    wifi_ap_record_t ap_records[20];
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    for (int i = 0; i < ap_num; i++) {
        lv_obj_t* btn = lv_btn_create(wifi_list_cont);
        lv_obj_set_size(btn, lv_pct(90), 50);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10, i * 55);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, (const char*)ap_records[i].ssid);
        lv_obj_center(label);

        lv_obj_add_event_cb(btn, wifi_item_event_cb, LV_EVENT_CLICKED, (void*)ap_records[i].ssid);
    }
}

// =================== CREATE WIFI APP ===================
void create_wifi_app(void) {
    if (wifi_screen) return;

    wifi_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_screen, lv_color_hex(UI_COLOR_BG_DARK), 0);

    lv_obj_t* title = lv_label_create(wifi_screen);
    lv_label_set_text(title, "WiFi Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t* back_btn = lv_btn_create(wifi_screen);
    lv_obj_set_size(back_btn, 120, 50);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COLOR_WIFI), 0);
    lv_obj_set_style_radius(back_btn, 10, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_label = lv_label_create(back_btn);
    lv_label_set_text(btn_label, "Back");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(btn_label);

    create_wifi_list();
    wifi_auto_connect();

    app_info_t* app_info = app_manager_get_app_info(APP_WIFI);
    if (app_info) app_info->screen = wifi_screen;

    ESP_LOGI(TAG, "WiFi app created");
}

// =================== DESTROY WIFI APP ===================
void destroy_wifi_app(void) {
    if (wifi_screen) {
        lv_obj_del(wifi_screen);
        wifi_screen = NULL;
        wifi_list_cont = NULL;
        ESP_LOGI(TAG, "WiFi app destroyed");
    }
}
