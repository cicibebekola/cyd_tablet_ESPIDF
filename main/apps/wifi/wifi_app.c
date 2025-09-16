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
static lv_obj_t* status_label = NULL;
static lv_obj_t* connection_status_label = NULL;
static lv_obj_t* keyboard = NULL;
static const char* TAG = "WIFI_APP";
static bool wifi_initialized = false;
static bool wifi_scanning = false;

// Network list
static wifi_ap_record_t* networks = NULL;
static int network_count = 0;

// =================== SD CARD FILE ===================
#define WIFI_CRED_FILE SD_PATH("wifi_credentials.txt")

typedef struct {
    char ssid[32];
    char password[64];
} wifi_cred_t;

// =================== FORWARD DECLARATIONS ===================
static void back_button_event_cb(lv_event_t* e);
static void scan_button_event_cb(lv_event_t* e);
static void wifi_connect_btn_cb(lv_event_t* e);
static void wifi_cancel_btn_cb(lv_event_t* e);
static void wifi_item_event_cb(lv_event_t* e);
static void textarea_event_cb(lv_event_t* e);
static void create_wifi_list(void);
static void load_saved_credentials(wifi_cred_t* cred);
static void save_credentials(const char* ssid, const char* password);
static void wifi_connect(const char* ssid, const char* password);
static bool wifi_auto_connect(void);
static void update_connection_status(void);
static void wifi_scan_networks(void);
static const char* get_signal_strength_symbol(int rssi);
static const char* get_auth_mode_text(wifi_auth_mode_t auth_mode);

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
    ESP_LOGI(TAG, "WiFi driver initialized");
}

// =================== KEYBOARD MANAGEMENT ===================
static void textarea_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        // Create keyboard when textarea gets focus
        if (!keyboard) {
            keyboard = lv_keyboard_create(lv_scr_act());
            lv_obj_set_size(keyboard, lv_pct(100), lv_pct(50));
            lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_color(keyboard, lv_color_hex(UI_COLOR_BG_DARK), 0);
        }
        lv_keyboard_set_textarea(keyboard, ta);
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Virtual keyboard shown");
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        // Hide keyboard when textarea loses focus
        if (keyboard) {
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Virtual keyboard hidden");
        }
    }
    else if (code == LV_EVENT_READY) {
        // Hide keyboard when user presses Enter/OK
        if (keyboard) {
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Virtual keyboard hidden (Enter pressed)");
        }
    }
}

// =================== STATUS UPDATES ===================
static void update_connection_status(void) {
    if (!connection_status_label) return;

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        char status_text[64];
        snprintf(status_text, sizeof(status_text), "Connected: %.20s", (char*)ap_info.ssid);
        lv_label_set_text(connection_status_label, status_text);
        lv_obj_set_style_text_color(connection_status_label, lv_color_hex(0x00FF00), 0); // Green
    } else {
        lv_label_set_text(connection_status_label, "Not Connected");
        lv_obj_set_style_text_color(connection_status_label, lv_color_hex(0xFF4444), 0); // Red
    }
}

// =================== SIGNAL STRENGTH ===================
static const char* get_signal_strength_symbol(int rssi) {
    if (rssi > -50) return LV_SYMBOL_WIFI;
    else if (rssi > -60) return LV_SYMBOL_WIFI;
    else if (rssi > -70) return LV_SYMBOL_WIFI;
    else return LV_SYMBOL_WIFI;
}

static const char* get_auth_mode_text(wifi_auth_mode_t auth_mode) {
    switch (auth_mode) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "Unknown";
    }
}

// =================== NETWORK SCANNING ===================
static void wifi_scan_networks(void) {
    if (wifi_scanning) {
        ESP_LOGW(TAG, "WiFi scan already in progress");
        return;
    }

    ESP_LOGI(TAG, "Starting WiFi scan");
    wifi_scanning = true;

    // Update status
    if (status_label) {
        lv_label_set_text(status_label, "Scanning...");
    }

    // Free previous networks
    if (networks) {
        free(networks);
        networks = NULL;
    }
    network_count = 0;

    // Initialize WiFi if needed
    wifi_driver_init();

    // Start scan
    esp_err_t scan_result = esp_wifi_scan_start(NULL, true);
    if (scan_result != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(scan_result));
        wifi_scanning = false;
        if (status_label) {
            lv_label_set_text(status_label, "Scan Failed");
        }
        return;
    }

    // Get scan results
    uint16_t ap_num = 20;
    wifi_ap_record_t ap_records[20];
    esp_err_t get_result = esp_wifi_scan_get_ap_records(&ap_num, ap_records);
    
    if (get_result == ESP_OK) {
        // Allocate memory for networks
        if (ap_num > 0) {
            networks = (wifi_ap_record_t*)malloc(ap_num * sizeof(wifi_ap_record_t));
            if (networks) {
                memcpy(networks, ap_records, ap_num * sizeof(wifi_ap_record_t));
                network_count = ap_num;
                ESP_LOGI(TAG, "Found %d networks", network_count);
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory for networks");
            }
        }

        // Update UI
        create_wifi_list();
        
        // Update status
        if (status_label) {
            char status_text[64];
            snprintf(status_text, sizeof(status_text), "%d networks", network_count);
            lv_label_set_text(status_label, status_text);
        }
    } else {
        ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(get_result));
        if (status_label) {
            lv_label_set_text(status_label, "Failed");
        }
    }

    wifi_scanning = false;
}

// =================== BACK BUTTON ===================
static void back_button_event_cb(lv_event_t* e) {
    app_manager_switch_to(APP_HOME);
}

// =================== SCAN BUTTON ===================
static void scan_button_event_cb(lv_event_t* e) {
    ESP_LOGI(TAG, "Scan button pressed");
    wifi_scan_networks();
    update_connection_status();
}

// =================== WIFI CONNECT ===================
static void wifi_connect(const char* ssid, const char* password) {
    if (!ssid || !password) return;

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);

    wifi_config_t cfg = {0};
    strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char*)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);

    esp_err_t result = esp_wifi_disconnect();
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "WiFi disconnect failed: %s", esp_err_to_name(result));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());

    save_credentials(ssid, password);
    
    // Update status after a delay to show connection attempt
    vTaskDelay(pdMS_TO_TICKS(2000));
    update_connection_status();
}

// =================== SAVE / LOAD CREDENTIALS ===================
static void save_credentials(const char* ssid, const char* password) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s,%s", ssid, password);
    esp_err_t result = sd_write_file(WIFI_CRED_FILE, buffer);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials saved");
    } else {
        ESP_LOGW(TAG, "Failed to save WiFi credentials");
    }
}

static void load_saved_credentials(wifi_cred_t* cred) {
    char buffer[128] = {0};
    if (sd_read_file(WIFI_CRED_FILE, buffer, sizeof(buffer)) == ESP_OK) {
        char* sep = strchr(buffer, ',');
        if (sep) {
            *sep = 0;
            strncpy(cred->ssid, buffer, sizeof(cred->ssid) - 1);
            strncpy(cred->password, sep + 1, sizeof(cred->password) - 1);
            ESP_LOGI(TAG, "Loaded saved credentials for SSID: %s", cred->ssid);
        }
    }
}

// =================== AUTO RECONNECT ===================
static bool wifi_auto_connect(void) {
    wifi_cred_t cred = {0};
    load_saved_credentials(&cred);
    if (strlen(cred.ssid) > 0 && strlen(cred.password) > 0) {
        ESP_LOGI(TAG, "Auto-connecting to saved network: %s", cred.ssid);
        wifi_connect(cred.ssid, cred.password);
        return true;
    }
    return false;
}

// =================== PASSWORD POPUP CALLBACK ===================
static void wifi_connect_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* popup = lv_obj_get_parent(btn);
    
    // Find the textarea in the popup (it should be the first child after labels)
    lv_obj_t* ta = NULL;
    uint32_t child_count = lv_obj_get_child_cnt(popup);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(popup, i);
        if (lv_obj_check_type(child, &lv_textarea_class)) {
            ta = child;
            break;
        }
    }
    
    if (ta) {
        const char* password = lv_textarea_get_text(ta);
        const char* ssid = (const char*)lv_event_get_user_data(e);
        wifi_connect(ssid, password);
    }
    
    // Hide keyboard before closing popup
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_del(popup);
}

// =================== CANCEL POPUP CALLBACK ===================
static void wifi_cancel_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* popup = lv_obj_get_parent(btn);
    
    // Hide keyboard before closing popup
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_del(popup);
}

// =================== WIFI ITEM CALLBACK ===================
static void wifi_item_event_cb(lv_event_t* e) {
    int network_index = (int)(uintptr_t)lv_event_get_user_data(e);
    
    if (network_index < 0 || network_index >= network_count) return;
    
    const char* ssid = (const char*)networks[network_index].ssid;
    ESP_LOGI(TAG, "Selected network: %s", ssid);

    // Check if network is open (no password required)
    if (networks[network_index].authmode == WIFI_AUTH_OPEN) {
        wifi_connect(ssid, "");
        return;
    }

    // Create password input popup
    lv_obj_t* popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(popup, 300, 200);
    lv_obj_center(popup);
    lv_obj_set_style_bg_color(popup, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_border_width(popup, 2, 0);

    // Network name label
    lv_obj_t* ssid_label = lv_label_create(popup);
    char title_text[64];
    snprintf(title_text, sizeof(title_text), "Connect to: %.30s", ssid);
    lv_label_set_text(ssid_label, title_text);
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_MID, 0, 10);

    // Security info
    lv_obj_t* auth_label = lv_label_create(popup);
    char auth_text[32];
    snprintf(auth_text, sizeof(auth_text), "Security: %s", get_auth_mode_text(networks[network_index].authmode));
    lv_label_set_text(auth_label, auth_text);
    lv_obj_set_style_text_color(auth_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(auth_label, &lv_font_montserrat_10, 0);
    lv_obj_align(auth_label, LV_ALIGN_TOP_MID, 0, 35);

    // Password input with keyboard support
    lv_obj_t* ta = lv_textarea_create(popup);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_placeholder_text(ta, "Enter password");
    lv_obj_set_size(ta, 260, 40);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x333333), 0);
    
    // Add event callback for keyboard management
    lv_obj_add_event_cb(ta, textarea_event_cb, LV_EVENT_ALL, NULL);

    // Connect button
    lv_obj_t* ok_btn = lv_btn_create(popup);
    lv_obj_set_size(ok_btn, 100, 40);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_LEFT, 40, -20);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(UI_COLOR_WIFI), 0);

    lv_obj_t* ok_label = lv_label_create(ok_btn);
    lv_label_set_text(ok_label, "Connect");
    lv_obj_center(ok_label);

    // Cancel button
    lv_obj_t* cancel_btn = lv_btn_create(popup);
    lv_obj_set_size(cancel_btn, 100, 40);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -40, -20);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(UI_COLOR_ACCENT), 0);

    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    // Add event callbacks - need to copy SSID string for callback
    char* ssid_copy = malloc(strlen(ssid) + 1);
    strcpy(ssid_copy, ssid);
    lv_obj_add_event_cb(ok_btn, wifi_connect_btn_cb, LV_EVENT_CLICKED, (void*)ssid_copy);
    lv_obj_add_event_cb(cancel_btn, wifi_cancel_btn_cb, LV_EVENT_CLICKED, NULL);
}

// =================== CREATE WIFI LIST ===================
static void create_wifi_list(void) {
    // Clear existing list
    if (wifi_list_cont) {
        lv_obj_del(wifi_list_cont);
        wifi_list_cont = NULL;
    }

    // Get screen dimensions
    lv_coord_t screen_height = lv_obj_get_height(wifi_screen);
    lv_coord_t title_bar_height = 35;

    // Create scrollable container
    wifi_list_cont = lv_obj_create(wifi_screen);
    lv_obj_set_size(wifi_list_cont, lv_obj_get_width(wifi_screen), screen_height - title_bar_height);
    lv_obj_set_pos(wifi_list_cont, 0, title_bar_height);
    lv_obj_set_style_bg_color(wifi_list_cont, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_radius(wifi_list_cont, 0, 0);
    lv_obj_set_style_pad_all(wifi_list_cont, 8, 0);
    lv_obj_set_style_border_width(wifi_list_cont, 0, 0);
    lv_obj_set_scroll_dir(wifi_list_cont, LV_DIR_VER);

    // Show networks or empty message
    if (network_count == 0) {
        lv_obj_t* empty_label = lv_label_create(wifi_list_cont);
        lv_label_set_text(empty_label, 
            "No WiFi networks found\n\n"
            "Press 'Scan' to search for networks\n"
            "Make sure your router is on and\n"
            "broadcasting its SSID");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
        lv_obj_center(empty_label);
        lv_label_set_long_mode(empty_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(empty_label, lv_pct(90));
        return;
    }

    // Add network items
    lv_coord_t y_pos = 0;
    for (int i = 0; i < network_count; i++) {
        lv_obj_t* item_btn = lv_btn_create(wifi_list_cont);
        lv_obj_set_size(item_btn, lv_pct(95), 60);
        lv_obj_set_pos(item_btn, 0, y_pos);
        
        // Color based on security and signal strength
        if (networks[i].authmode == WIFI_AUTH_OPEN) {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(0x4CAF50), 0); // Green for open
        } else if (networks[i].rssi > -50) {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(UI_COLOR_WIFI), 0); // Strong signal
        } else if (networks[i].rssi > -70) {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(UI_COLOR_SECONDARY), 0); // Medium signal
        } else {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(UI_COLOR_ACCENT), 0); // Weak signal
        }
        
        lv_obj_set_style_radius(item_btn, 5, 0);
        lv_obj_add_event_cb(item_btn, wifi_item_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        // SSID with WiFi symbol
        lv_obj_t* ssid_label = lv_label_create(item_btn);
        char ssid_text[64];
        snprintf(ssid_text, sizeof(ssid_text), "%s %.30s", 
                get_signal_strength_symbol(networks[i].rssi),
                (char*)networks[i].ssid);
        lv_label_set_text(ssid_label, ssid_text);
        lv_obj_set_style_text_color(ssid_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
        lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 10, 5);

        // Signal strength and security info
        lv_obj_t* info_label = lv_label_create(item_btn);
        char info_text[64];
        snprintf(info_text, sizeof(info_text), "Signal: %d dBm | %s", 
                networks[i].rssi, get_auth_mode_text(networks[i].authmode));
        lv_label_set_text(info_label, info_text);
        lv_obj_set_style_text_color(info_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
        lv_obj_set_style_text_font(info_label, &lv_font_montserrat_10, 0);
        lv_obj_align(info_label, LV_ALIGN_BOTTOM_LEFT, 10, -5);

        // Channel info
        lv_obj_t* channel_label = lv_label_create(item_btn);
        char channel_text[16];
        snprintf(channel_text, sizeof(channel_text), "Ch %d", networks[i].primary);
        lv_label_set_text(channel_label, channel_text);
        lv_obj_set_style_text_color(channel_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
        lv_obj_set_style_text_font(channel_label, &lv_font_montserrat_8, 0);
        lv_obj_align(channel_label, LV_ALIGN_TOP_RIGHT, -10, 5);

        y_pos += 65; // Increased spacing for better readability
    }
}

// =================== CREATE WIFI APP ===================
void create_wifi_app(void) {
    if (wifi_screen) return; // Already created

    ESP_LOGI(TAG, "Creating WiFi app");
    wifi_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_screen, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_pad_all(wifi_screen, 0, 0);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(wifi_screen);
    lv_obj_set_size(title_bar, lv_pct(100), 35);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button
    lv_obj_t* back_btn = lv_btn_create(title_bar);
    lv_obj_set_size(back_btn, 45, 25);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COLOR_ACCENT), 0);
    lv_obj_set_style_radius(back_btn, 3, 0);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_color(back_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_10, 0);
    lv_obj_center(back_label);

    // Scan button
    lv_obj_t* scan_btn = lv_btn_create(title_bar);
    lv_obj_set_size(scan_btn, 45, 25);
    lv_obj_align(scan_btn, LV_ALIGN_LEFT_MID, 55, 0);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(UI_COLOR_WIFI), 0);
    lv_obj_set_style_radius(scan_btn, 3, 0);
    lv_obj_add_event_cb(scan_btn, scan_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_set_style_text_color(scan_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_10, 0);
    lv_obj_center(scan_label);

    // Title
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "WiFi Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -5);

    // Connection status in title bar
    connection_status_label = lv_label_create(title_bar);
    lv_obj_set_style_text_color(connection_status_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(connection_status_label, &lv_font_montserrat_8, 0);
    lv_obj_align(connection_status_label, LV_ALIGN_CENTER, 0, 8);

    // Scan status
    status_label = lv_label_create(title_bar);
    lv_obj_set_style_text_color(status_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
    lv_obj_align(status_label, LV_ALIGN_RIGHT_MID, -5, 0);

    // Initialize WiFi driver
    wifi_driver_init();

    // Create empty list initially (user needs to press scan)
    create_wifi_list();
    
    // Update connection status
    update_connection_status();
    
    // Set initial status
    if (status_label) {
        lv_label_set_text(status_label, "Press Scan");
    }

    // Auto-connect to saved network if available
    if (wifi_auto_connect()) {
        // Small delay then update status
        vTaskDelay(pdMS_TO_TICKS(1000));
        update_connection_status();
    }

    // Link to app manager
    app_info_t* app_info = app_manager_get_app_info(APP_WIFI);
    if (app_info) {
        app_info->screen = wifi_screen;
        ESP_LOGI(TAG, "WiFi screen linked to app manager");
    }

    ESP_LOGI(TAG, "WiFi app created successfully");
}

// =================== DESTROY WIFI APP ===================
void destroy_wifi_app(void) {
    if (wifi_screen) {
        ESP_LOGI(TAG, "WiFi app destroyed");
        
        // Clean up keyboard
        if (keyboard) {
            lv_obj_del(keyboard);
            keyboard = NULL;
        }
        
        // Free allocated memory
        if (networks) {
            free(networks);
            networks = NULL;
        }
        network_count = 0;

        lv_obj_del(wifi_screen);
        wifi_screen = NULL;
        wifi_list_cont = NULL;
        status_label = NULL;
        connection_status_label = NULL;
    }
}

// =================== PUBLIC FUNCTIONS ===================
lv_obj_t* wifi_app_get_screen(void) {
    return wifi_screen;
}

void wifi_app_refresh(void) {
    if (wifi_screen && wifi_list_cont) {
        wifi_scan_networks();
        update_connection_status();
        ESP_LOGI(TAG, "WiFi list refreshed");
    }
}