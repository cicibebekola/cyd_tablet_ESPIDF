/**
 * @file sd_card_manager.h
 * @brief SD Card interface using VSPI (SPI3_HOST) without DMA
 * 
 * This header provides SD card functionality that works alongside
 * LCD/Touch controllers on SPI2_HOST with DMA.
 * 
 * Pin Configuration:
 * - SD Card (VSPI/SPI3_HOST): CS=5, MOSI=23, CLK=18, MISO=19
 * - LCD/Touch (HSPI/SPI2_HOST): Your existing setup on GPIOs 12,13,14,15,2,27,33,36
 */

#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * CONFIGURATION DEFINES
 * ========================================================================== */

/** SD Card mount point in filesystem */
#define SD_MOUNT_POINT "/sdcard"

/** Maximum character size for file operations */
#define SD_MAX_CHAR_SIZE 64

/** Maximum number of simultaneously open files */
#define SD_MAX_OPEN_FILES 5

/** Allocation unit size for FAT filesystem (16KB) */
#define SD_ALLOCATION_UNIT_SIZE (16 * 1024)

/* ==========================================================================
 * PIN DEFINITIONS (VSPI/SPI3_HOST)
 * ========================================================================== */

/** SD Card Chip Select pin (active low) */
#define SD_PIN_CS    5

/** SD Card MOSI (Master Out Slave In) pin */
#define SD_PIN_MOSI  23

/** SD Card MISO (Master In Slave Out) pin */  
#define SD_PIN_MISO  19

/** SD Card Clock pin */
#define SD_PIN_CLK   18

/* ==========================================================================
 * SPI CONFIGURATION
 * ========================================================================== */

/** SPI Host to use for SD card (back to VSPI with better settings) */
#define SD_SPI_HOST  SPI3_HOST

/** Maximum SPI transfer size in bytes (increased for compatibility) */
#define SD_MAX_TRANSFER_SIZE 2048

/** SPI frequency in kHz (reduced for stability without DMA) */
#define SD_SPI_FREQ_KHZ 20000

/* ==========================================================================
 * PUBLIC FUNCTION DECLARATIONS
 * ========================================================================== */

/**
 * @brief Initialize SD card and mount FAT filesystem
 * 
 * This function initializes the VSPI bus, configures the SD card,
 * and mounts the FAT filesystem. Call this once during startup.
 * 
 * @param format_if_failed If true, format card if mount fails
 * @return ESP_OK on success, error code on failure
 */
esp_err_t sd_card_init(bool format_if_failed);

/**
 * @brief Unmount SD card and free resources
 * 
 * Call this to properly unmount the filesystem and free SPI resources.
 * Should be called before system shutdown or SD card removal.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t sd_card_deinit(void);

/**
 * @brief Write data to a file on SD card
 * 
 * @param path Full path to file (e.g., "/sdcard/data.txt")
 * @param data Null-terminated string to write
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sd_write_file(const char *path, const char *data);

/**
 * @brief Read data from a file on SD card
 * 
 * @param path Full path to file (e.g., "/sdcard/data.txt")  
 * @param buffer Buffer to store read data
 * @param buffer_size Size of buffer
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sd_read_file(const char *path, char *buffer, size_t buffer_size);

/**
 * @brief Append data to a file on SD card
 * 
 * @param path Full path to file (e.g., "/sdcard/log.txt")
 * @param data Null-terminated string to append
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sd_append_file(const char *path, const char *data);

/**
 * @brief Check if SD card is mounted and ready
 * 
 * @return true if SD card is mounted, false otherwise
 */
bool sd_is_mounted(void);

/**
 * @brief Get SD card information
 * 
 * @param card_info Pointer to store card information (can be NULL)
 * @return ESP_OK if card is available, ESP_FAIL otherwise
 */
esp_err_t sd_get_card_info(sdmmc_card_t **card_info);

/**
 * @brief Format SD card with FAT filesystem
 * 
 * WARNING: This will erase all data on the SD card!
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t sd_format_card(void);

/**
 * @brief Get space info on SD card (total capacity only)
 * 
 * Note: Free space calculation is not available on ESP32
 * 
 * @param total_bytes Pointer to store total space (can be NULL)
 * @param free_bytes Pointer to store free space - will be set to 0 (can be NULL)  
 * @return ESP_OK on success, error code on failure
 */
esp_err_t sd_get_space_info(uint64_t *total_bytes, uint64_t *free_bytes);

/* ==========================================================================
 * CONVENIENCE MACROS
 * ========================================================================== */

/** Create full path to file on SD card */
#define SD_PATH(filename) (SD_MOUNT_POINT "/" filename)

/** Check if file exists on SD card */
#define SD_FILE_EXISTS(path) (access(path, F_OK) == 0)

/** Delete file from SD card */
#define SD_DELETE_FILE(path) (unlink(path) == 0)

/* ==========================================================================
 * ERROR HANDLING
 * ========================================================================== */

/** SD Card specific error base */
#define ESP_ERR_SD_BASE 0x6000

/** SD card not mounted error */
#define ESP_ERR_SD_NOT_MOUNTED (ESP_ERR_SD_BASE + 1)

/** SD card mount failed error */  
#define ESP_ERR_SD_MOUNT_FAILED (ESP_ERR_SD_BASE + 2)

/** SD card format failed error */
#define ESP_ERR_SD_FORMAT_FAILED (ESP_ERR_SD_BASE + 3)

/** SD card file operation failed error */
#define ESP_ERR_SD_FILE_FAILED (ESP_ERR_SD_BASE + 4)

#ifdef __cplusplus
}
#endif

#endif /* SD_CARD_MANAGER_H */