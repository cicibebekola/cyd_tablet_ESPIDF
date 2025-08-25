#include "app_manager.h"
#include "ui_styles.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"


// App includes
#include "./apps/home/home_app.h"
#include "./apps/wifi/wifi_app.h"
#include "./apps/bt/bt_app.h"
#include "./apps/folder/folder_app.h"

// App registry with memory management
static app_info_t apps[APP_MAX_COUNT] = {
    {
        .id = APP_HOME,
        .name = "Home",
        .color = UI_COLOR_HOME,
        .create = create_home_app,
        .destroy = destroy_home_app,
        .screen = NULL
    },
    {
        .id = APP_WIFI,
        .name = "WiFi",
        .color = UI_COLOR_WIFI,
        .create = create_wifi_app,
        .destroy = destroy_wifi_app,
        .screen = NULL
    },
    {
        .id = APP_BLUETOOTH,
        .name = "Bluetooth",
        .color = UI_COLOR_SECONDARY,
        .create = create_bt_app,
        .destroy = destroy_bt_app,
        .screen = NULL
    },
    {
        .id = APP_FOLDER,
        .name = "Folder",
        .color = UI_COLOR_SECONDARY,
        .create = create_folder_app,
        .destroy = destroy_folder_app,
        .screen = NULL
    }

};

static app_id_t current_app = APP_HOME;

void app_manager_init(void) {
    ui_init_styles();
    
    ESP_LOGI("APP_MGR", "Creating home app");
    create_home_app();
    
    // Load the home screen as the active screen
    app_info_t* home_app = &apps[APP_HOME];
    if (home_app->screen) {
        lv_scr_load(home_app->screen);
        ESP_LOGI("APP_MGR", "Home screen loaded and displayed");
    } else {
        ESP_LOGE("APP_MGR", "Home screen is NULL!");
    }
    
    current_app = APP_HOME;
    ESP_LOGI("APP_MGR", "App Manager initialized");
}

void ui_switch_to_screen(lv_obj_t* new_screen) {
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

void app_manager_switch_to(app_id_t target) {


    ESP_LOGI("APP_MGR", "Switching to app %d", target);

      size_t free_before = esp_get_free_heap_size();
    ESP_LOGI("MEMORY", "=== SWITCHING TO APP %d ===", target);
    ESP_LOGI("MEMORY", "Free heap BEFORE: %zu bytes", free_before);
    
    if (target >= APP_MAX_COUNT) {
        ESP_LOGE("APP_MGR", "Invalid app ID: %d", target);
        return;
    }
    
    if (target == current_app) {
        ESP_LOGI("APP_MGR", "Already on app %d", target);
        return;
    }

    ESP_LOGI("APP_MGR", "Current app: %d, Target app: %d", current_app, target);

    // Create target app FIRST (before destroying current)
    app_info_t* next = &apps[target];
    if (!next->screen && next->create) {
        ESP_LOGI("APP_MGR", "Creating app %d", target);
        next->create();
    }
    
    // Switch to new screen BEFORE destroying old one
    if (next->screen) {
        ESP_LOGI("APP_MGR", "Loading screen for app %d", target);
        ui_switch_to_screen(next->screen);
        current_app = target;
        ESP_LOGI("APP_MGR", "Successfully switched to %s app", next->name);
        
        // NOW clean up the previous app
        app_info_t* prev = &apps[current_app != target ? current_app : APP_HOME];
        if (prev->destroy && prev->screen && prev != next) {
            ESP_LOGI("APP_MGR", "Destroying previous app");
            prev->destroy();
            // Don't use lv_obj_del_async during screen transition
            prev->screen = NULL;
        }
    } else {
        ESP_LOGE("APP_MGR", "Failed to create screen for app %d", target);
    
    }
    
size_t free_after = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    
    ESP_LOGI("MEMORY", "Free heap AFTER: %zu bytes", free_after);
    ESP_LOGI("MEMORY", "Memory change: %+d bytes", (int)(free_after - free_before));
    ESP_LOGI("MEMORY", "Minimum free ever: %zu bytes", min_free);
    ESP_LOGI("MEMORY", "=== SWITCH COMPLETE ===");
}

void app_manager_go_home(void) {
    app_manager_switch_to(APP_HOME);
}

app_id_t app_manager_get_current_app(void) {
    return current_app;
}

app_info_t* app_manager_get_app_info(app_id_t id) {
    if (id < APP_MAX_COUNT) {
        return &apps[id];
    }
    ESP_LOGW("APP_MGR", "Invalid app ID requested: %d", id);
    return NULL;
}