/**
 * @file sd_card_manager.c
 * @brief SD Card implementation using VSPI (SPI3_HOST) without DMA
 * Fixed for ESP32/ESP-IDF compatibility with enhanced free space calculation
 */

#include "sd_card_manager.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ff.h"

/* ==========================================================================
 * PRIVATE VARIABLES
 * ========================================================================== */

static const char *TAG = "sd_card_manager";
static sdmmc_card_t *s_card = NULL;
static bool s_card_mounted = false;

/* ==========================================================================
 * PRIVATE FUNCTION DECLARATIONS
 * ========================================================================== */

static esp_err_t sd_check_mounted(void);

/* ==========================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * ========================================================================== */

esp_err_t sd_card_init(bool format_if_failed)
{
    esp_err_t ret;

    if (s_card_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    // Configure mount options
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = format_if_failed,
        .max_files = SD_MAX_OPEN_FILES,
        .allocation_unit_size = SD_ALLOCATION_UNIT_SIZE
    };

    ESP_LOGI(TAG, "Initializing SD card on VSPI (SPI3_HOST) with DMA");
    
    // Ensure SPI3_HOST is completely free before we use it
    spi_bus_free(SD_SPI_HOST);  // Free any existing instance (ignore errors)
    
    // Small delay to let the system settle
    vTaskDelay(pdMS_TO_TICKS(200));

    // Configure SD host to use VSPI
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    host.max_freq_khz = SD_SPI_FREQ_KHZ;

    // Configure SPI bus for VSPI with DMA (matching LCD's successful approach)
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SD_MAX_TRANSFER_SIZE,  // Increased for DMA compatibility
        .flags = 0,
        .intr_flags = 0,
    };

    // Initialize SPI bus WITH DMA (matching LCD's successful approach)
    ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI3_HOST already initialized, trying to free and retry...");
        spi_bus_free(SD_SPI_HOST);
        vTaskDelay(pdMS_TO_TICKS(100));
        ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize VSPI bus: %s", esp_err_to_name(ret));
        return ESP_ERR_SD_MOUNT_FAILED;
    }

    // Configure SD device with additional compatibility settings
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = SD_SPI_HOST;
    slot_config.gpio_cd = -1;    // No card detect pin
    slot_config.gpio_wp = -1;    // No write protect pin
    slot_config.gpio_int = -1;   // No interrupt pin

    // Mount filesystem
    ESP_LOGI(TAG, "Mounting filesystem with %d kHz clock", SD_SPI_FREQ_KHZ);
    ESP_LOGI(TAG, "SD pins - CS:%d, MOSI:%d, MISO:%d, CLK:%d", 
             SD_PIN_CS, SD_PIN_MOSI, SD_PIN_MISO, SD_PIN_CLK);
    
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card filesystem");
        
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Filesystem mount failed. Try setting format_if_failed=true");
        } else if (ret == ESP_ERR_INVALID_ARG) {
            ESP_LOGE(TAG, "Invalid arguments - check SPI configuration");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "SD card not found - check connections and card insertion");
        } else if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGE(TAG, "Out of memory");
        } else if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Invalid state - SPI bus may already be in use");
        } else {
            ESP_LOGE(TAG, "SD card error code: 0x%x (%s)", ret, esp_err_to_name(ret));
        }
        
        ESP_LOGE(TAG, "Troubleshooting tips:");
        ESP_LOGE(TAG, "1. Check SD card is properly inserted");
        ESP_LOGE(TAG, "2. Verify wiring: CS=%d, MOSI=%d, MISO=%d, CLK=%d", 
                 SD_PIN_CS, SD_PIN_MOSI, SD_PIN_MISO, SD_PIN_CLK);
        ESP_LOGE(TAG, "3. Add 10kΩ pull-up resistors to all SD card pins");
        ESP_LOGE(TAG, "4. Try a different/known-good SD card");
        ESP_LOGE(TAG, "5. Check for conflicts with SPI2_HOST (LCD/Touch)");
        
        spi_bus_free(SD_SPI_HOST);
        return ESP_ERR_SD_MOUNT_FAILED;
    }

    s_card_mounted = true;
    ESP_LOGI(TAG, "SD card mounted successfully");
    
    // Print card information
    sdmmc_card_print_info(stdout, s_card);
    
    return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
    if (!s_card_mounted) {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_OK;
    }

    // Unmount filesystem
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount filesystem: %s", esp_err_to_name(ret));
    }

    // Free SPI bus
    spi_bus_free(SD_SPI_HOST);
    
    s_card = NULL;
    s_card_mounted = false;
    
    ESP_LOGI(TAG, "SD card unmounted and resources freed");
    return ret;
}

esp_err_t sd_write_file(const char *path, const char *data)
{
    if (sd_check_mounted() != ESP_OK) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_SD_NOT_MOUNTED;
    }
    
    ESP_LOGI(TAG, "Writing to file: %s", path);
    
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_ERR_SD_FILE_FAILED;
    }
    
    if (fprintf(f, "%s", data) < 0) {
        ESP_LOGE(TAG, "Failed to write data to file");
        fclose(f);
        return ESP_ERR_SD_FILE_FAILED;
    }
    
    fclose(f);
    ESP_LOGI(TAG, "File written successfully");
    return ESP_OK;
}

esp_err_t sd_read_file(const char *path, char *buffer, size_t buffer_size)
{
    if (sd_check_mounted() != ESP_OK) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_SD_NOT_MOUNTED;
    }
    
    if (buffer == NULL || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid buffer parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Reading from file: %s", path);
    
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        return ESP_ERR_SD_FILE_FAILED;
    }
    
    // Read file content
    size_t bytes_read = fread(buffer, 1, buffer_size - 1, f);
    buffer[bytes_read] = '\0';  // Null terminate
    
    fclose(f);
    ESP_LOGI(TAG, "Read %zu bytes from file", bytes_read);
    return ESP_OK;
}

esp_err_t sd_append_file(const char *path, const char *data)
{
    if (sd_check_mounted() != ESP_OK) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_SD_NOT_MOUNTED;
    }
    
    ESP_LOGI(TAG, "Appending to file: %s", path);
    
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", path);
        return ESP_ERR_SD_FILE_FAILED;
    }
    
    if (fprintf(f, "%s", data) < 0) {
        ESP_LOGE(TAG, "Failed to append data to file");
        fclose(f);
        return ESP_ERR_SD_FILE_FAILED;
    }
    
    fclose(f);
    ESP_LOGI(TAG, "Data appended successfully");
    return ESP_OK;
}

bool sd_is_mounted(void)
{
    return s_card_mounted;
}

esp_err_t sd_get_card_info(sdmmc_card_t **card_info)
{
    if (sd_check_mounted() != ESP_OK) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_SD_NOT_MOUNTED;
    }
    
    if (card_info != NULL) {
        *card_info = s_card;
    }
    
    return ESP_OK;
}

esp_err_t sd_format_card(void)
{
    if (sd_check_mounted() != ESP_OK) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_SD_NOT_MOUNTED;
    }
    
    ESP_LOGW(TAG, "Formatting SD card - ALL DATA WILL BE LOST!");
    
    esp_err_t ret = esp_vfs_fat_sdcard_format(SD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format SD card: %s", esp_err_to_name(ret));
        return ESP_ERR_SD_FORMAT_FAILED;
    }
    
    ESP_LOGI(TAG, "SD card formatted successfully");
    return ESP_OK;
}

esp_err_t sd_get_space_info(uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (sd_check_mounted() != ESP_OK) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_SD_NOT_MOUNTED;
    }
    
    // Method 1: Try FATFS direct API first
    FATFS *fs;
    DWORD free_clusters;
    
    FRESULT res = f_getfree("0:", &free_clusters, &fs);
    if (res == FR_OK) {
        uint32_t sector_size = 512; // Standard sector size
        uint32_t cluster_size = fs->csize * sector_size;
        
        if (total_bytes != NULL) {
            *total_bytes = (uint64_t)(fs->n_fatent - 2) * cluster_size;
        }
        if (free_bytes != NULL) {
            *free_bytes = (uint64_t)free_clusters * cluster_size;
        }
        
        ESP_LOGI(TAG, "Total capacity: %llu bytes (%.2f MB)", 
                 total_bytes ? *total_bytes : 0,
                 total_bytes ? (double)*total_bytes / (1024.0 * 1024.0) : 0.0);
        ESP_LOGI(TAG, "Free space: %llu bytes (%.2f MB)", 
                 free_bytes ? *free_bytes : 0,
                 free_bytes ? (double)*free_bytes / (1024.0 * 1024.0) : 0.0);
        
        return ESP_OK;
    }
    
    // Method 2: Fallback - get total capacity from card info only
    ESP_LOGW(TAG, "FATFS f_getfree failed (error: %d), using card info only", res);
    
    if (total_bytes != NULL && s_card != NULL) {
        *total_bytes = ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size;
        ESP_LOGI(TAG, "Total capacity: %llu bytes (%.2f MB)", 
                 *total_bytes, (double)*total_bytes / (1024.0 * 1024.0));
    }
    
    if (free_bytes != NULL) {
        *free_bytes = 0;  // Unknown
        ESP_LOGW(TAG, "Free space calculation not available");
    }
    
    return ESP_ERR_NOT_SUPPORTED;
}

/* ==========================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * ========================================================================== */

static esp_err_t sd_check_mounted(void)
{
    if (!s_card_mounted) {
        return ESP_ERR_SD_NOT_MOUNTED;
    }
    return ESP_OK;
}