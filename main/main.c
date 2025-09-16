#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "app_manager.h"
#include "ui_styles.h"
#include "sd_card_manager.h"

static const char *TAG = "CYD_TABLET";

static void inc_lvgl_tick(void *arg)
{
    lv_tick_inc(10);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting CYD Tablet Application");
    
    // Add stabilization delay for cold boot
    ESP_LOGI(TAG, "System stabilization delay...");
    vTaskDelay(pdMS_TO_TICKS(200));

    // Initialize NVS (required for Wi-Fi)
    ESP_LOGI(TAG, "Initializing NVS...");
    esp_err_t reh = nvs_flash_init();
    if (reh == ESP_ERR_NVS_NO_FREE_PAGES || reh == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    reh = nvs_flash_init();
    }
    ESP_ERROR_CHECK(reh);
    ESP_LOGI(TAG, "NVS initialized successfully");
    
    // Initialize LVGL first
    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();
    
    // Initialize display with proper reset sequence
    ESP_LOGI(TAG, "Initializing display...");
    lv_port_disp_init();
    
    // Important: Add delay for display to fully initialize
    ESP_LOGI(TAG, "Display stabilizing...");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Initialize touch screen
    ESP_LOGI(TAG, "Initializing touch screen...");
    vTaskDelay(pdMS_TO_TICKS(300));
    
    lv_port_indev_init();
    ESP_LOGI(TAG, "Touch screen controller ready");
    
    // Initialize SD card
    ESP_LOGI(TAG, "Initializing SD card...");
    vTaskDelay(pdMS_TO_TICKS(300));
    
    esp_err_t ret = sd_card_init(false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card initialization failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SD card initialized successfully");
        
        // Create sample files
        const char *test_msg = "CYD Tablet started successfully!\n";
        if (sd_write_file(SD_PATH("startup.log"), test_msg) == ESP_OK) {
            ESP_LOGI(TAG, "SD card test write successful");
        }
        
        sd_write_file(SD_PATH("readme.txt"), "Welcome to your CYD Tablet!\nThis file is stored on the SD card.\n");
        sd_write_file(SD_PATH("config.txt"), "# Configuration file\nbrightness=100\nvolume=50\n");
        sd_append_file(SD_PATH("system.log"), "System initialized successfully\n");
        ESP_LOGI(TAG, "Sample files created for testing");
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Initialize applications
    ESP_LOGI(TAG, "Loading applications...");
    vTaskDelay(pdMS_TO_TICKS(300));
    
    app_manager_init();
    ESP_LOGI(TAG, "Application manager ready");
    
    // Start with home screen
    app_manager_go_home();
    
    // Setup LVGL timer
    ESP_LOGI(TAG, "Starting LVGL timer...");
    
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &inc_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 10 * 1000));
    
    ESP_LOGI(TAG, "LVGL timer started (10ms tick)");
    
    ESP_LOGI(TAG, "CYD Tablet initialization complete, starting main loop");

    // Main loop
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
        
        // Auto-refresh folder app when SD card becomes available
        static bool sd_was_mounted = false;
        bool sd_currently_mounted = sd_is_mounted();
        
        if (sd_currently_mounted && !sd_was_mounted) {
            ESP_LOGI(TAG, "SD card became available - refreshing folder app");
            extern void folder_app_refresh(void);
            folder_app_refresh();
        }
        sd_was_mounted = sd_currently_mounted;
        
        // System logging (only if SD card is mounted)
        static uint32_t last_status_log = 0;
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (sd_is_mounted() && (current_time - last_status_log > 60000)) {
            char status_buffer[128];
            snprintf(status_buffer, sizeof(status_buffer),
                     "[%lu] Free heap: %lu bytes, Min free: %lu bytes\n",
                     current_time / 1000,
                     (unsigned long)esp_get_free_heap_size(),
                     (unsigned long)esp_get_minimum_free_heap_size());
            
            sd_append_file(SD_PATH("system_status.log"), status_buffer);
            last_status_log = current_time;
        }
    }
}