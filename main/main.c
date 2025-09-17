#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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

static TaskHandle_t ui_task_handle = NULL;
static TaskHandle_t app_manager_task_handle = NULL;

// --------------------------------------------------
// LVGL Tick Callback
// --------------------------------------------------
static void inc_lvgl_tick(void *arg) {
    lv_tick_inc(10);
}

// --------------------------------------------------
// SD Card Async Write Queue
// --------------------------------------------------
typedef struct {
    char path[128];
    char data[256];
    bool append;
} sd_write_t;

static QueueHandle_t sd_write_queue;

void sd_task(void *arg) {
    ESP_LOGI(TAG, "SD task started");

    if (sd_card_init(false) != ESP_OK) {
        ESP_LOGE(TAG, "SD card failed to initialize");
    }

    sd_write_t write_item;
    while (1) {
        if (xQueueReceive(sd_write_queue, &write_item, pdMS_TO_TICKS(100))) {
            esp_err_t ret;
            if (write_item.append) {
                ret = sd_append_file(write_item.path, write_item.data);
            } else {
                ret = sd_write_file(write_item.path, write_item.data);
            }

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "SD write failed: %s", esp_err_to_name(ret));
            }
        }
    }
}

void sd_queue_write(const char *path, const char *data, bool append) {
    sd_write_t item;
    strncpy(item.path, path, sizeof(item.path)-1);
    strncpy(item.data, data, sizeof(item.data)-1);
    item.append = append;
    xQueueSend(sd_write_queue, &item, 0);
}

// --------------------------------------------------
// UI Command Queue
// --------------------------------------------------
typedef enum {
    UI_CMD_CREATE_HOME_APP,
    UI_CMD_REFRESH_FOLDER
} ui_command_t;

static QueueHandle_t ui_cmd_queue;

// --------------------------------------------------
// UI / LVGL Task
// --------------------------------------------------
void ui_task(void *arg) {
    ESP_LOGI(TAG, "UI task started on Core 1");

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    ui_init_styles();

    if (app_manager_task_handle != NULL) {
        xTaskNotifyGive(app_manager_task_handle);
    }

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &inc_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 10 * 1000));
    
    ui_command_t cmd;
    while(1) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
        
        if (xQueueReceive(ui_cmd_queue, &cmd, 0)) {
            // Process commands here
        }
    }
}

// --------------------------------------------------
// App Manager Task
// --------------------------------------------------
void app_manager_task(void *arg) {
    ESP_LOGI(TAG, "App manager task started");
    
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "UI task is ready. Initializing app manager...");

    app_manager_init();

    while (1) {
        static bool sd_was_mounted = false;
        bool sd_currently_mounted = sd_is_mounted();
        if (sd_currently_mounted && !sd_was_mounted) {
            ui_command_t cmd = UI_CMD_REFRESH_FOLDER;
            xQueueSend(ui_cmd_queue, &cmd, 0);
        }
        sd_was_mounted = sd_currently_mounted;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// --------------------------------------------------
// System Logger Task
// --------------------------------------------------
void system_logger_task(void *arg) {
    ESP_LOGI(TAG, "System logger task started");

    uint32_t last_status_log = 0;

    while (1) {
        if (sd_is_mounted()) {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (current_time - last_status_log > 60000) {
                char buffer[256];
                snprintf(buffer, sizeof(buffer),
                         "[%lu] Free heap: %lu bytes, Min free: %lu bytes\n",
                         current_time / 1000,
                         (unsigned long)esp_get_free_heap_size(),
                         (unsigned long)esp_get_minimum_free_heap_size());
                sd_queue_write(SD_PATH("system_status.log"), buffer, true);
                last_status_log = current_time;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --------------------------------------------------
// System / Wi-Fi / BT Task
// --------------------------------------------------
void system_task(void *arg) {
    ESP_LOGI(TAG, "System task started on Core 0");

    esp_err_t reh = nvs_flash_init();
    if (reh == ESP_ERR_NVS_NO_FREE_PAGES || reh == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        reh = nvs_flash_init();
    }
    ESP_ERROR_CHECK(reh);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// --------------------------------------------------
// CPU Usage Monitoring Task
// --------------------------------------------------
void stats_task(void *arg) {
    printf("\nTask | CPU Time | Percentage\n");
    printf("-----------------------------------\n");
    while (1) {
        char stats_buffer[1024];
        vTaskDelay(pdMS_TO_TICKS(5000)); // Update every 5 seconds
        vTaskGetRunTimeStats(stats_buffer);
        printf("%s\n", stats_buffer);
    }
}

// --------------------------------------------------
// Main Entry Point
// --------------------------------------------------
void app_main(void) {
    ESP_LOGI(TAG, "Starting CYD Tablet Application");

    sd_write_queue = xQueueCreate(16, sizeof(sd_write_t));
    ui_cmd_queue = xQueueCreate(8, sizeof(ui_command_t));
    if (!sd_write_queue || !ui_cmd_queue) {
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }

    xTaskCreatePinnedToCore(system_task, "SystemTask", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(ui_task, "UITask", 8192, NULL, 10, &ui_task_handle, 1);
    xTaskCreatePinnedToCore(sd_task, "SDTask", 1024, NULL, 8, NULL, 1);
    xTaskCreatePinnedToCore(app_manager_task, "AppManagerTask", 512, NULL, 7, &app_manager_task_handle, 1);
    xTaskCreatePinnedToCore(system_logger_task, "LoggerTask", 512, NULL, 6, NULL, 1);

    // Create the task to monitor and display CPU usage
    xTaskCreate(stats_task, "StatsTask", 256, NULL, 4, NULL);

    sd_queue_write(SD_PATH("startup.log"), "CYD Tablet started successfully!\n", false);
    sd_queue_write(SD_PATH("readme.txt"), "Welcome to your CYD Tablet!\nThis file is stored on the SD card.\n", false);
    sd_queue_write(SD_PATH("system_status.log"), "logstart\n", false);
    sd_queue_write(SD_PATH("config.txt"), "# Configuration file\nbrightness=100\nvolume=50\n", false);
}