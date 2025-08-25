// text_viewer_app.h
#ifndef TEXT_VIEWER_APP_H
#define TEXT_VIEWER_APP_H

#include "lvgl.h"

void text_viewer_set_file_path(const char* file_path);
void create_text_viewer_app(void);
void destroy_text_viewer_app(void);
lv_obj_t* text_viewer_app_get_screen(void);

#endif // TEXT_VIEWER_APP_H

// text_viewer_app.c
#include "text_viewer_app.h"
#include "../../app_manager.h"
#include "../../ui_styles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"

static lv_obj_t *text_viewer_screen = NULL;
static lv_obj_t *text_content_area = NULL;
static char current_file_path[512];
static char pending_file_path[512];  // Store file path before creation

// Forward declarations
static void text_viewer_back_cb(lv_event_t *e);
static char* load_text_file(const char* file_path, size_t max_size);
static const char* format_file_size(size_t bytes);

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

// Load text file content with memory management
static char* load_text_file(const char* file_path, size_t max_size) {
    FILE* file = fopen(file_path, "r");
    if (!file) {
        ESP_LOGE("TEXT_VIEWER", "Failed to open text file: %s", file_path);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        ESP_LOGW("TEXT_VIEWER", "Empty or invalid file: %s", file_path);
        fclose(file);
        return NULL;
    }
    
    // Limit file size to prevent memory issues
    size_t read_size = (file_size > max_size) ? max_size : file_size;
    
    // Allocate buffer (+1 for null terminator)
    char* buffer = malloc(read_size + 1);
    if (!buffer) {
        ESP_LOGE("TEXT_VIEWER", "Failed to allocate memory for file content");
        fclose(file);
        return NULL;
    }
    
    // Read file content
    size_t bytes_read = fread(buffer, 1, read_size, file);
    buffer[bytes_read] = '\0';  // Null terminate
    
    fclose(file);
    
    ESP_LOGI("TEXT_VIEWER", "Loaded %zu bytes from %s", bytes_read, file_path);
    return buffer;
}

// Text viewer back button callback
static void text_viewer_back_cb(lv_event_t *e) {
    ESP_LOGI("TEXT_VIEWER", "Going back to folder app");
    app_manager_switch_to(APP_FOLDER);
}

void text_viewer_set_file_path(const char* file_path) {
    if (file_path) {
        strncpy(pending_file_path, file_path, sizeof(pending_file_path) - 1);
        pending_file_path[sizeof(pending_file_path) - 1] = '\0';
        ESP_LOGI("TEXT_VIEWER", "File path set to: %s", pending_file_path);
        
        // Force recreation of the text viewer if it already exists
        if (text_viewer_screen) {
            ESP_LOGI("TEXT_VIEWER", "Text viewer already exists, forcing recreation");
            destroy_text_viewer_app();
        }
    } else {
        pending_file_path[0] = '\0';
        ESP_LOGW("TEXT_VIEWER", "File path cleared");
    }
}

void create_text_viewer_app(void) {
    if (text_viewer_screen) {
        destroy_text_viewer_app();
    }
    
    // Check if file path was set
    if (pending_file_path[0] == '\0') {
        ESP_LOGE("TEXT_VIEWER", "No file path set! Call text_viewer_set_file_path() first");
        return;
    }
    
    // Copy pending path to current path
    strncpy(current_file_path, pending_file_path, sizeof(current_file_path) - 1);
    current_file_path[sizeof(current_file_path) - 1] = '\0';
    
    // Clear pending path to ensure fresh state
    pending_file_path[0] = '\0';
    
    ESP_LOGI("TEXT_VIEWER", "Creating text viewer for: %s", current_file_path);
    
    text_viewer_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(text_viewer_screen, lv_color_hex(UI_COLOR_BG_DARK), 0);
    lv_obj_set_style_pad_all(text_viewer_screen, 0, 0);
    
    // Title bar
    lv_obj_t *title_bar = lv_obj_create(text_viewer_screen);
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
    lv_obj_add_event_cb(back_btn, text_viewer_back_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_color(back_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_10, 0);
    lv_obj_center(back_label);
    
    // Title with filename
    lv_obj_t *title = lv_label_create(title_bar);
    const char* filename = strrchr(current_file_path, '/');
    if (filename) filename++; // Skip the slash
    else filename = current_file_path;
    
    char title_text[80];
    snprintf(title_text, sizeof(title_text), "%.60s", filename);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    
    // Content area
    text_content_area = lv_obj_create(text_viewer_screen);
    lv_obj_set_size(text_content_area, lv_pct(100), lv_obj_get_height(text_viewer_screen) - 35);
    lv_obj_set_pos(text_content_area, 0, 35);
    lv_obj_set_style_bg_color(text_content_area, lv_color_hex(0x1a1a1a), 0); // Darker background for text
    lv_obj_set_style_radius(text_content_area, 0, 0);
    lv_obj_set_style_pad_all(text_content_area, 10, 0);
    lv_obj_set_style_border_width(text_content_area, 0, 0);
    lv_obj_set_scroll_dir(text_content_area, LV_DIR_VER);
    
    // Clear any existing children in content area
    lv_obj_clean(text_content_area);
    
    // Load and display text content
    const size_t MAX_FILE_SIZE = 32768; // 32KB limit to prevent memory issues
    char* content = load_text_file(current_file_path, MAX_FILE_SIZE);
    
    if (content) {
        ESP_LOGI("TEXT_VIEWER", "Successfully loaded file content, creating text label");
        
        lv_obj_t *text_label = lv_label_create(text_content_area);
        lv_label_set_text(text_label, content);
        lv_obj_set_style_text_color(text_label, lv_color_hex(0xE0E0E0), 0); // Light gray text
        lv_obj_set_style_text_font(text_label, &lv_font_montserrat_10, 0);
        lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(text_label, lv_pct(98));
        lv_obj_align(text_label, LV_ALIGN_TOP_LEFT, 0, 0);
        
        // Force LVGL to update the display
        lv_obj_invalidate(text_label);
        lv_obj_invalidate(text_content_area);
        
        // Add file info
        struct stat file_stat;
        if (stat(current_file_path, &file_stat) == 0) {
            lv_obj_t *info_label = lv_label_create(title_bar);
            char info_text[64];
            snprintf(info_text, sizeof(info_text), "%s", format_file_size(file_stat.st_size));
            lv_label_set_text(info_label, info_text);
            lv_obj_set_style_text_color(info_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
            lv_obj_set_style_text_font(info_label, &lv_font_montserrat_8, 0);
            lv_obj_align(info_label, LV_ALIGN_RIGHT_MID, -5, 0);
        }
        
        free(content); // Free the loaded content after copying to label
        ESP_LOGI("TEXT_VIEWER", "Text file displayed successfully: %s", current_file_path);
    } else {
        // Show error message
        lv_obj_t *error_label = lv_label_create(text_content_area);
        lv_label_set_text(error_label, "Failed to load file content.\nFile may be too large or corrupted.");
        lv_obj_set_style_text_color(error_label, lv_color_hex(0xFF4444), 0);
        lv_obj_center(error_label);
        ESP_LOGE("TEXT_VIEWER", "Failed to load text file content: %s", current_file_path);
    }
    
    // Link to app manager (you'll need to add TEXT_VIEWER to your app enum)
    app_info_t* app_info = app_manager_get_app_info(APP_TEXT_VIEWER);
    if (app_info) {
        app_info->screen = text_viewer_screen;
        ESP_LOGI("TEXT_VIEWER", "Text viewer screen linked to app manager");
    }
    
    ESP_LOGI("TEXT_VIEWER", "Text viewer app created successfully");
}

void destroy_text_viewer_app(void) {
    if (text_viewer_screen) {
        ESP_LOGI("TEXT_VIEWER", "Text viewer app destroyed");
        
        lv_obj_del(text_viewer_screen);
        text_viewer_screen = NULL;
        text_content_area = NULL;
        
        // Clear file paths
        current_file_path[0] = '\0';
        pending_file_path[0] = '\0';
        
        // Clear the app info screen reference so app manager will call create next time
        app_info_t* app_info = app_manager_get_app_info(APP_TEXT_VIEWER);
        if (app_info) {
            app_info->screen = NULL;
            ESP_LOGI("TEXT_VIEWER", "Cleared app manager screen reference");
        }
    }
}

lv_obj_t* text_viewer_app_get_screen(void) {
    // If we have a pending file path but no screen, create it
    if (pending_file_path[0] != '\0' && !text_viewer_screen) {
        ESP_LOGI("TEXT_VIEWER", "Screen requested but doesn't exist, creating now");
        create_text_viewer_app();
    }
    
    return text_viewer_screen;
} 