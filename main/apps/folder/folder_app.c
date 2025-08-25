#include "lvgl.h"
#include "folder_app.h"
#include "../../app_manager.h"
#include "../../ui_styles.h"
#include "../../sd_card_manager.h"
#include "./apps/text_view/text_viewer_app.h"  // Include the separate text viewer
#include "./apps/video_player/video_player_app.h"  // Include the video player
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "esp_log.h"

// Static variables
static lv_obj_t *folder_screen = NULL;
static lv_obj_t *file_list = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *sd_status_label = NULL;

// Dynamic file structure
typedef struct {
    char name[64];
    bool is_folder;
    size_t size;  // Add file size
} file_item_t;

// Current directory path
static char current_path[256] = "/sdcard";

// Dynamic array for files
static file_item_t *files = NULL;
static int file_count = 0;

// Forward declarations
static void create_file_list(void);
static void file_item_event_cb(lv_event_t *e);
static void back_button_event_cb(lv_event_t *e);
static void refresh_button_event_cb(lv_event_t *e);
static int load_directory_contents(const char *path);
static void update_sd_status(void);
static const char* format_file_size(size_t bytes);
static bool is_text_file(const char* filename);

// New: Get appropriate LVGL symbol based on file type
static const char* get_file_symbol(const char* filename, bool is_folder) {
    if (is_folder) {
        return LV_SYMBOL_DIRECTORY;
    }
    
    // Get file extension for different file type symbols
    const char* ext = strrchr(filename, '.');
    if (ext) {
        ext++; // Skip the dot
        if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "log") == 0) {
            return LV_SYMBOL_FILE;
        } else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "png") == 0 || 
                   strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "gif") == 0) {
            return LV_SYMBOL_IMAGE;
        } else if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "wav") == 0 || 
                   strcasecmp(ext, "aac") == 0) {
            return LV_SYMBOL_AUDIO;
        } else if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0 || 
                   strcasecmp(ext, "mov") == 0) {
            return LV_SYMBOL_VIDEO;
        } 
    }
    
    return LV_SYMBOL_FILE; // Default file symbol
}

static const char* format_file_size(size_t bytes) {
    static char size_str[32];
    
    if (bytes < 1024) {
        snprintf(size_str, sizeof(size_str), "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(size_str, sizeof(size_str), "%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(size_str, sizeof(size_str), "%.1f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(size_str, sizeof(size_str), "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
    
    return size_str;
}

// Check if file is a text file based on extension
static bool is_text_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    ext++; // Skip the dot
    return (strcasecmp(ext, "txt") == 0 || 
            strcasecmp(ext, "log") == 0 ||
            strcasecmp(ext, "cfg") == 0 ||
            strcasecmp(ext, "conf") == 0 ||
            strcasecmp(ext, "ini") == 0 ||
            strcasecmp(ext, "json") == 0 ||
            strcasecmp(ext, "xml") == 0 ||
            strcasecmp(ext, "csv") == 0);
}

// New: Update SD card status display
static void update_sd_status(void) {
    if (!sd_status_label) return;
    
    if (sd_is_mounted()) {
        uint64_t total_bytes, free_bytes;
        if (sd_get_space_info(&total_bytes, &free_bytes) == ESP_OK) {
            char status_text[128];
            snprintf(status_text, sizeof(status_text), 
                     "SD: %.1f GB total", 
                     total_bytes / (1024.0 * 1024.0 * 1024.0));
            lv_label_set_text(sd_status_label, status_text);
            lv_obj_set_style_text_color(sd_status_label, lv_color_hex(0x00FF00), 0); // Green
        } else {
            lv_label_set_text(sd_status_label, "SD: Mounted");
            lv_obj_set_style_text_color(sd_status_label, lv_color_hex(0x00FF00), 0); // Green
        }
    } else {
        lv_label_set_text(sd_status_label, "SD: Not Available");
        lv_obj_set_style_text_color(sd_status_label, lv_color_hex(0xFF4444), 0); // Red
    }
}

// Enhanced directory loading with SD card check
static int load_directory_contents(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[512];
    
    ESP_LOGI("FOLDER_APP", "Attempting to load directory: %s", path);
    
    // Check if SD card is mounted
    if (!sd_is_mounted()) {
        ESP_LOGW("FOLDER_APP", "SD card is not mounted yet");
        
        // Free previous file list
        if (files) {
            free(files);
            files = NULL;
        }
        file_count = 0;
        return 0;
    }
    
    // Free previous file list
    if (files) {
        free(files);
        files = NULL;
    }
    file_count = 0;
    
    // Open directory
    dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGE("FOLDER_APP", "Failed to open directory: %s (errno: %d)", path, errno);
        ESP_LOGE("FOLDER_APP", "Error description: %s", strerror(errno));
        return 0;
    }
    ESP_LOGI("FOLDER_APP", "Directory opened successfully");
    
    // Count files first
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGD("FOLDER_APP", "Found entry: %s", entry->d_name);
        // Skip hidden files and current/parent directory entries
        if (entry->d_name[0] == '.') {
            ESP_LOGD("FOLDER_APP", "Skipping hidden/system entry: %s", entry->d_name);
            continue;
        }
        count++;
    }
    
    ESP_LOGI("FOLDER_APP", "Found %d visible entries in directory", count);
    
    if (count == 0) {
        closedir(dir);
        ESP_LOGI("FOLDER_APP", "Directory is empty: %s", path);
        return 0;
    }
    
    // Allocate memory for files
    files = (file_item_t*)malloc(count * sizeof(file_item_t));
    if (!files) {
        ESP_LOGE("FOLDER_APP", "Failed to allocate memory for file list");
        closedir(dir);
        return 0;
    }
    ESP_LOGI("FOLDER_APP", "Allocated memory for %d files", count);
    
    // Reset directory reading
    rewinddir(dir);
    
    // Read files and determine types
    int index = 0;
    while ((entry = readdir(dir)) != NULL && index < count) {
        // Skip hidden files and current/parent directory entries
        if (entry->d_name[0] == '.') continue;
        
        // Copy name
        strncpy(files[index].name, entry->d_name, sizeof(files[index].name) - 1);
        files[index].name[sizeof(files[index].name) - 1] = '\0';
        
        // Get full path for stat
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        // Check if it's a directory and get file size
        if (stat(full_path, &file_stat) == 0) {
            files[index].is_folder = S_ISDIR(file_stat.st_mode);
            files[index].size = files[index].is_folder ? 0 : file_stat.st_size;
            ESP_LOGD("FOLDER_APP", "Entry %d: %s (%s, %zu bytes)", index, files[index].name, 
                    files[index].is_folder ? "DIR" : "FILE", files[index].size);
        } else {
            files[index].is_folder = false; // Default to file if stat fails
            files[index].size = 0;
            ESP_LOGW("FOLDER_APP", "stat() failed for %s, defaulting to file", full_path);
        }
        
        index++;
    }
    
    closedir(dir);
    file_count = index;
    
    ESP_LOGI("FOLDER_APP", "Successfully loaded %d items from %s", file_count, path);
    return file_count;
}

static void back_button_event_cb(lv_event_t *e) {
    // Check if we can go up one directory
    if (strcmp(current_path, "/sdcard") != 0) {
        // Go up one directory
        char *last_slash = strrchr(current_path, '/');
        if (last_slash && last_slash != current_path) {
            *last_slash = '\0'; // Remove last directory
        } else {
            strcpy(current_path, "/sdcard"); // Go back to root
        }
        
        // Reload directory contents
        load_directory_contents(current_path);
        create_file_list();
        update_sd_status();  // Update SD status
        
        // Update status
        if (status_label) {
            char status_text[128];
            snprintf(status_text, sizeof(status_text), "%.50s (%d items)", current_path, file_count);
            lv_label_set_text(status_label, status_text);
        }
    } else {
        // At root, go back to home
        app_manager_switch_to(APP_HOME);
    }
}

// New: Refresh button callback
static void refresh_button_event_cb(lv_event_t *e) {
    ESP_LOGI("FOLDER_APP", "Refreshing file list");
    
    // Reload directory contents
    load_directory_contents(current_path);
    create_file_list();
    update_sd_status();
    
    // Update status
    if (status_label) {
        char status_text[128];
        snprintf(status_text, sizeof(status_text), "%.50s (%d items)", current_path, file_count);
        lv_label_set_text(status_label, status_text);
    }
}

static void file_item_event_cb(lv_event_t *e) {
    int file_index = (int)(uintptr_t)lv_event_get_user_data(e);
    
    if (file_index < 0 || file_index >= file_count) return;
    
    ESP_LOGI("FOLDER_APP", "Selected: %s", files[file_index].name);
    
    if (files[file_index].is_folder) {
        // Navigate into folder
        char new_path[512];
        snprintf(new_path, sizeof(new_path), "%s/%s", current_path, files[file_index].name);
        strncpy(current_path, new_path, sizeof(current_path) - 1);
        current_path[sizeof(current_path) - 1] = '\0';
        
        // Reload directory contents
        load_directory_contents(current_path);
        create_file_list();
        update_sd_status();
        
        // Update status
        if (status_label) {
            char status_text[128];
            snprintf(status_text, sizeof(status_text), "%.50s (%d items)", current_path, file_count);
            lv_label_set_text(status_label, status_text);
        }
    } else {
        // Handle file opening
        char full_file_path[512];
        snprintf(full_file_path, sizeof(full_file_path), "%s/%s", current_path, files[file_index].name);
        
        if (is_text_file(files[file_index].name)) {
            // Open text file in separate text viewer app
            ESP_LOGI("FOLDER_APP", "Opening text file: %s", files[file_index].name);
            text_viewer_set_file_path(full_file_path);  // Set the file path first
            app_manager_switch_to(APP_TEXT_VIEWER);     // Switch to text viewer app
        } else if (video_player_is_supported_file(files[file_index].name)) {
            // Open video file in video player app
            ESP_LOGI("FOLDER_APP", "Opening video file: %s", files[file_index].name);
            video_player_set_file_path(full_file_path); // Set the file path first
            app_manager_switch_to(APP_VIDEO_PLAYER);    // Switch to video player app
        } else {
            // Show file info for non-text files
            ESP_LOGI("FOLDER_APP", "File info: %s (%s)", 
                     files[file_index].name, format_file_size(files[file_index].size));
            
            // TODO: Add viewers for other file types (images, etc.)
        }
    }
}

static void create_file_list(void) {
    if (file_list) {
        lv_obj_del(file_list);
    }
    
    // Get the actual screen height
    lv_coord_t screen_height = lv_obj_get_height(folder_screen);
    lv_coord_t title_bar_height = 35;
    
    // Create scrollable container for file list
    file_list = lv_obj_create(folder_screen);
    lv_obj_set_size(file_list, lv_obj_get_width(folder_screen), screen_height - title_bar_height);
    lv_obj_set_pos(file_list, 0, title_bar_height);
    lv_obj_set_style_bg_color(file_list, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_radius(file_list, 0, 0);
    lv_obj_set_style_pad_all(file_list, 8, 0);
    lv_obj_set_style_border_width(file_list, 0, 0);
    lv_obj_set_scroll_dir(file_list, LV_DIR_VER);
    
    // Check SD card status first
    if (!sd_is_mounted()) {
        // Show SD card not available message
        lv_obj_t *error_label = lv_label_create(file_list);
        lv_label_set_text(error_label, 
            "SD Card Not Available\n\n"
            "Please check:\n"
            "• SD card is inserted\n"
            "• SD card is formatted (FAT32)\n"
            "• Connections are secure\n\n"
            "Press Refresh to try again");
        lv_obj_set_style_text_color(error_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
        lv_obj_center(error_label);
        lv_label_set_long_mode(error_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(error_label, lv_pct(90));
        return;
    }
    
    // Add file items from SD card
    lv_coord_t y_pos = 0;
    for (int i = 0; i < file_count; i++) {
        lv_obj_t *item_btn = lv_btn_create(file_list);
        lv_obj_set_size(item_btn, lv_pct(95), 50);  // Slightly taller for file size
        lv_obj_set_pos(item_btn, 0, y_pos);
        
        // Different colors for folders vs files vs text files vs video files
        if (files[i].is_folder) {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(UI_COLOR_SECONDARY), 0);
        } else if (is_text_file(files[i].name)) {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(0x4CAF50), 0); // Green for text files
        } else if (video_player_is_supported_file(files[i].name)) {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(0xFF5722), 0); // Orange for video files
        } else {
            lv_obj_set_style_bg_color(item_btn, lv_color_hex(UI_COLOR_ACCENT), 0);
        }
        
        lv_obj_set_style_radius(item_btn, 5, 0);
        lv_obj_add_event_cb(item_btn, file_item_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        
        // Create main label with file/folder name using appropriate symbol
        lv_obj_t *item_label = lv_label_create(item_btn);
        char label_text[80];
        snprintf(label_text, sizeof(label_text), "%s %s", 
                get_file_symbol(files[i].name, files[i].is_folder),
                files[i].name);
        lv_label_set_text(item_label, label_text);
        lv_obj_set_style_text_color(item_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
        lv_obj_align(item_label, LV_ALIGN_TOP_LEFT, 10, 5);
        
        // Create size label for files
        if (!files[i].is_folder && files[i].size > 0) {
            lv_obj_t *size_label = lv_label_create(item_btn);
            lv_label_set_text(size_label, format_file_size(files[i].size));
            lv_obj_set_style_text_color(size_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
            lv_obj_set_style_text_font(size_label, &lv_font_montserrat_10, 0);
            lv_obj_align(size_label, LV_ALIGN_BOTTOM_LEFT, 10, -5);
        }
        
        // Add type indicators for special files
        if (!files[i].is_folder && is_text_file(files[i].name)) {
            lv_obj_t *type_label = lv_label_create(item_btn);
            lv_label_set_text(type_label, "TEXT");
            lv_obj_set_style_text_color(type_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(type_label, &lv_font_montserrat_8, 0);
            lv_obj_align(type_label, LV_ALIGN_TOP_RIGHT, -5, 5);
        } else if (!files[i].is_folder && video_player_is_supported_file(files[i].name)) {
            lv_obj_t *type_label = lv_label_create(item_btn);
            lv_label_set_text(type_label, "VIDEO");
            lv_obj_set_style_text_color(type_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(type_label, &lv_font_montserrat_8, 0);
            lv_obj_align(type_label, LV_ALIGN_TOP_RIGHT, -5, 5);
        }
        
        y_pos += 55; // Move to next position (increased spacing)
    }
    
    // Show message if no files
    if (file_count == 0) {
        lv_obj_t *empty_label = lv_label_create(file_list);
        lv_label_set_text(empty_label, "No files found in this directory");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
        lv_obj_center(empty_label);
    }
}

void create_folder_app(void) {
    if (folder_screen) return; // already created

    ESP_LOGI("FOLDER_APP", "Creating folder screen");
    folder_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(folder_screen, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_pad_all(folder_screen, 0, 0);

    // Title bar
    lv_obj_t *title_bar = lv_obj_create(folder_screen);
    lv_obj_set_size(title_bar, lv_pct(100), 35);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button
    lv_obj_t *back_btn = lv_btn_create(title_bar);
    lv_obj_set_size(back_btn, 45, 25);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COLOR_ACCENT), 0);
    lv_obj_set_style_radius(back_btn, 3, 0);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_color(back_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_10, 0);
    lv_obj_center(back_label);

    // Refresh button
    lv_obj_t *refresh_btn = lv_btn_create(title_bar);
    lv_obj_set_size(refresh_btn, 55, 25);
    lv_obj_align(refresh_btn, LV_ALIGN_LEFT_MID, 55, 0);
    lv_obj_set_style_bg_color(refresh_btn, lv_color_hex(0x4CAF50), 0); // Green
    lv_obj_set_style_radius(refresh_btn, 3, 0);
    lv_obj_add_event_cb(refresh_btn, refresh_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, "Refresh");
    lv_obj_set_style_text_color(refresh_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(refresh_label, &lv_font_montserrat_10, 0);
    lv_obj_center(refresh_label);

    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    lv_label_set_text(title, "Files");
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -5);

    // SD card status in title bar
    sd_status_label = lv_label_create(title_bar);
    lv_obj_set_style_text_color(sd_status_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(sd_status_label, &lv_font_montserrat_8, 0);
    lv_obj_align(sd_status_label, LV_ALIGN_CENTER, 0, 8);

    // Path status
    status_label = lv_label_create(title_bar);
    lv_obj_set_style_text_color(status_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
    lv_obj_align(status_label, LV_ALIGN_RIGHT_MID, -5, 0);

    // Load SD card contents and create the file list
    load_directory_contents(current_path);
    create_file_list();
    update_sd_status();
    
    // Update status
    if (status_label) {
        char status_text[128];
        snprintf(status_text, sizeof(status_text), "%.50s (%d items)", current_path, file_count);
        lv_label_set_text(status_label, status_text);
    }

    // Link to app manager
    app_info_t* app_info = app_manager_get_app_info(APP_FOLDER);
    if (app_info) {
        app_info->screen = folder_screen;
        ESP_LOGI("FOLDER_APP", "Folder screen linked to app manager");
    }
    
    ESP_LOGI("FOLDER_APP", "Folder app created successfully");
}

void destroy_folder_app(void) {
    if (folder_screen) {
        ESP_LOGI("FOLDER_APP", "Folder app destroyed");
        
        // Free allocated memory
        if (files) {
            free(files);
            files = NULL;
        }
        file_count = 0;
        
        lv_obj_del(folder_screen);
        folder_screen = NULL;
        file_list = NULL;
        status_label = NULL;
        sd_status_label = NULL;
    }
}

lv_obj_t* folder_app_get_screen(void) {
    return folder_screen;
}

void folder_app_refresh(void) {
    if (folder_screen && file_list) {
        load_directory_contents(current_path);
        create_file_list();
        update_sd_status();
        
        // Update status
        if (status_label) {
            char status_text[128];
            snprintf(status_text, sizeof(status_text), "%.50s (%d items)", current_path, file_count);
            lv_label_set_text(status_label, status_text);
        }
        
        ESP_LOGI("FOLDER_APP", "File list refreshed");
    }
}