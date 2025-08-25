/**
 * @file text_viewer_app.h
 * @brief Text Viewer Application for ESP32 LVGL Tablet
 * 
 * Provides functionality to view text files (.txt, .log, .cfg, .conf, .ini, 
 * .json, .xml, .csv) in a dedicated viewer with scrolling and memory management.
 * 
 * Features:
 * - Support for multiple text file formats
 * - Memory-safe file loading (32KB limit)
 * - Scrollable text display
 * - File size information
 * - Clean navigation back to folder app
 * 
 * @author Your Name
 * @date 2025
 */

#ifndef TEXT_VIEWER_APP_H
#define TEXT_VIEWER_APP_H

#include "lvgl.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum file size that can be loaded (32KB)
 * 
 * This limit prevents memory issues on ESP32 without PSRAM.
 * Files larger than this will be truncated.
 */
#define TEXT_VIEWER_MAX_FILE_SIZE   32768

/**
 * @brief Maximum path length for file paths
 */
#define TEXT_VIEWER_MAX_PATH_LEN    512

/**
 * @brief Set the file path for the next text viewer creation
 * 
 * This function must be called before create_text_viewer_app() to specify
 * which file should be displayed. This allows the text viewer to work with
 * the app manager's parameterless create function pattern.
 * 
 * @param file_path Full path to the text file to display
 * @return void
 */
void text_viewer_set_file_path(const char* file_path);

/**
 * @brief Create and initialize the text viewer application
 * 
 * Creates the text viewer screen and loads the file specified by the previous
 * call to text_viewer_set_file_path(). If no file path was set, displays
 * an error message.
 * 
 * @return void
 * 
 * @note Must call text_viewer_set_file_path() first to set the file to display
 */
void create_text_viewer_app(void);

/**
 * @brief Destroy the text viewer application and free resources
 * 
 * Cleans up all LVGL objects, frees allocated memory, and resets
 * the text viewer state. This function is safe to call multiple times.
 * 
 * @return void
 */
void destroy_text_viewer_app(void);

/**
 * @brief Get the main screen object of the text viewer
 * 
 * Returns the LVGL screen object for the text viewer application.
 * This is used by the app manager for screen management.
 * 
 * @return lv_obj_t* Pointer to the text viewer screen object, 
 *                   or NULL if not created
 */
lv_obj_t* text_viewer_app_get_screen(void);

/**
 * @brief Check if a file is a supported text file type
 * 
 * Determines if a file can be opened by the text viewer based on
 * its file extension. Supports common text file formats.
 * 
 * @param filename Name of the file (including extension)
 * @return true if the file is a supported text format
 * @return false if the file is not supported
 * 
 * Supported extensions:
 * - .txt (Plain text)
 * - .log (Log files)  
 * - .cfg/.conf (Configuration files)
 * - .ini (INI files)
 * - .json (JSON files)
 * - .xml (XML files)
 * - .csv (CSV files)
 */
bool text_viewer_is_supported_file(const char* filename);

/**
 * @brief Get the currently loaded file path
 * 
 * Returns the path of the currently displayed file, or NULL if
 * no file is loaded.
 * 
 * @return const char* Path to current file, or NULL
 */
const char* text_viewer_get_current_file(void);

/**
 * @brief Refresh the current file display
 * 
 * Reloads and redisplays the current file. Useful if the file
 * has been modified externally.
 * 
 * @return true if refresh was successful
 * @return false if refresh failed (file not found, memory error, etc.)
 */
bool text_viewer_refresh(void);

#ifdef __cplusplus
}
#endif

#endif // TEXT_VIEWER_APP_H