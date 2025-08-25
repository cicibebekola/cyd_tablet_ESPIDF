// video_player_app.h
#ifndef VIDEO_PLAYER_APP_H
#define VIDEO_PLAYER_APP_H

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum JPEG frame size (adjust based on your video quality)
 */
#define VIDEO_MAX_FRAME_SIZE    (50 * 1024)  // 50KB per frame

/**
 * @brief Video player states
 */
typedef enum {
    VIDEO_STATE_STOPPED,
    VIDEO_STATE_PLAYING,
    VIDEO_STATE_PAUSED,
    VIDEO_STATE_ERROR
} video_state_t;

/**
 * @brief Set the video file path for the next video player creation
 */
void video_player_set_file_path(const char* file_path);

/**
 * @brief Create and initialize the video player application
 */
void create_video_player_app(void);

/**
 * @brief Destroy the video player application and free resources
 */
void destroy_video_player_app(void);

/**
 * @brief Get the main screen object of the video player
 */
lv_obj_t* video_player_app_get_screen(void);

/**
 * @brief Check if a file is a supported video file type
 */
bool video_player_is_supported_file(const char* filename);

#ifdef __cplusplus
}
#endif

#endif // VIDEO_PLAYER_APP_H

// video_player_app.c
#include "video_player_app.h"
#include "../../app_manager.h"
#include "../../ui_styles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>        // Add this for sin() function
#include "esp_log.h"
#include "esp_timer.h"

// MJPEG file header structure (simplified)
typedef struct {
    uint32_t frame_count;
    uint32_t fps;
    uint32_t width;
    uint32_t height;
} mjpeg_header_t;

static lv_obj_t *video_screen = NULL;
static lv_obj_t *video_image = NULL;
static lv_obj_t *control_panel = NULL;
static lv_obj_t *play_btn = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *time_label = NULL;

static char current_file_path[512];
static char pending_file_path[512];

static FILE *video_file = NULL;
static uint32_t total_frames = 0;
static uint32_t current_frame = 0;
static uint32_t fps = 30;
static video_state_t video_state = VIDEO_STATE_STOPPED;
static esp_timer_handle_t frame_timer = NULL;

// Forward declarations
static void video_player_back_cb(lv_event_t *e);
static void play_pause_btn_cb(lv_event_t *e);
static void create_test_video_cb(lv_event_t *e);
static void frame_timer_cb(void *arg);
static bool load_video_info(const char* file_path);
static bool load_next_frame(void);
static void update_controls(void);
static const char* format_time(uint32_t seconds);
static void create_test_video(void);
static uint8_t* create_solid_color_jpeg(uint8_t r, uint8_t g, uint8_t b, uint32_t* size_out);

// Create a simple solid color JPEG frame for testing
static uint8_t* create_solid_color_jpeg(uint8_t r, uint8_t g, uint8_t b, uint32_t* size_out) {
    // This is a minimal JPEG with solid color (simplified for testing)
    // In a real implementation, you'd use a JPEG encoder library
    static uint8_t simple_jpeg[] = {
        // JPEG header and minimal data for a solid color image
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
        0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43,
        0x00, 0x08, 0x06, 0x06, 0x07, 0x06, 0x05, 0x08, 0x07, 0x07, 0x07, 0x09,
        0x09, 0x08, 0x0A, 0x0C, 0x14, 0x0D, 0x0C, 0x0B, 0x0B, 0x0C, 0x19, 0x12,
        0x13, 0x0F, 0x14, 0x1D, 0x1A, 0x1F, 0x1E, 0x1D, 0x1A, 0x1C, 0x1C, 0x20,
        0x24, 0x2E, 0x27, 0x20, 0x22, 0x2C, 0x23, 0x1C, 0x1C, 0x28, 0x37, 0x29,
        0x2C, 0x30, 0x31, 0x34, 0x34, 0x34, 0x1F, 0x27, 0x39, 0x3D, 0x38, 0x32,
        0x3C, 0x2E, 0x33, 0x34, 0x32, 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x01, 0x40,
        0x00, 0xF0, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01,
        0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xDA, 0x00,
        0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00
    };
    
    // Copy to dynamic memory and modify color values (simplified)
    uint8_t* jpeg_data = malloc(sizeof(simple_jpeg) + 100); // Extra space for color data
    if (!jpeg_data) {
        *size_out = 0;
        return NULL;
    }
    
    memcpy(jpeg_data, simple_jpeg, sizeof(simple_jpeg));
    
    // Add some color data and JPEG end marker
    uint32_t size = sizeof(simple_jpeg);
    
    // Add minimal scan data (this is very simplified)
    for (int i = 0; i < 20; i++) {
        jpeg_data[size++] = (r + g + b + i) & 0xFF; // Simple color-based pattern
    }
    
    // Add JPEG end marker
    jpeg_data[size++] = 0xFF;
    jpeg_data[size++] = 0xD9;
    
    *size_out = size;
    return jpeg_data;
}

// Create test video file for development/testing
static void create_test_video(void) {
    ESP_LOGI("VIDEO_PLAYER", "Creating test video file...");
    
    const char* test_file = "/sdcard/test_video.mjpeg";
    FILE* file = fopen(test_file, "wb");
    if (!file) {
        ESP_LOGE("VIDEO_PLAYER", "Failed to create test video file");
        return;
    }
    
    // Write MJPEG header
    mjpeg_header_t header = {
        .frame_count = 60,      // 2 seconds at 30 fps
        .fps = 30,
        .width = 240,
        .height = 320
    };
    fwrite(&header, sizeof(header), 1, file);
    
    // Create frames with different colors (rainbow effect)
    for (uint32_t frame = 0; frame < header.frame_count; frame++) {
        // Generate colors that change over time for visual effect
        uint8_t r = (uint8_t)(128 + 127 * sin((double)frame * 0.1));
        uint8_t g = (uint8_t)(128 + 127 * sin((double)frame * 0.1 + 2.0));
        uint8_t b = (uint8_t)(128 + 127 * sin((double)frame * 0.1 + 4.0));
        
        uint32_t jpeg_size;
        uint8_t* jpeg_data = create_solid_color_jpeg(r, g, b, &jpeg_size);
        
        if (jpeg_data && jpeg_size > 0) {
            // Write frame size
            fwrite(&jpeg_size, sizeof(uint32_t), 1, file);
            
            // Write frame data
            fwrite(jpeg_data, 1, jpeg_size, file);
            
            free(jpeg_data);
            
            ESP_LOGI("VIDEO_PLAYER", "Created test frame %lu/%lu", 
                     (unsigned long)(frame + 1), (unsigned long)header.frame_count);
        } else {
            ESP_LOGE("VIDEO_PLAYER", "Failed to create test frame %lu", (unsigned long)frame);
            break;
        }
    }
    
    fclose(file);
    ESP_LOGI("VIDEO_PLAYER", "Test video created: %s", test_file);
    ESP_LOGI("VIDEO_PLAYER", "Video: %lu frames, %lu fps, %lux%lu", 
             (unsigned long)header.frame_count, (unsigned long)header.fps,
             (unsigned long)header.width, (unsigned long)header.height);
}

/**
 * @brief Create a test video file on the SD card for development/testing
 */
void video_player_create_test_file(void) {
    create_test_video();
}


bool video_player_is_supported_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    ext++; // Skip the dot
    return (strcasecmp(ext, "mjpeg") == 0 || 
            strcasecmp(ext, "mjpg") == 0);
}

static const char* format_time(uint32_t seconds) {
    static char time_str[16];
    uint32_t minutes = seconds / 60;
    seconds = seconds % 60;
    snprintf(time_str, sizeof(time_str), "%02lu:%02lu", (unsigned long)minutes, (unsigned long)seconds);
    return time_str;
}

static void update_controls(void) {
    if (!progress_bar || !time_label) return;
    
    // Update progress bar
    if (total_frames > 0) {
        uint32_t progress = (current_frame * 100) / total_frames;
        lv_bar_set_value(progress_bar, (int32_t)progress, LV_ANIM_OFF);
    }
    
    // Update time display
    uint32_t current_seconds = (fps > 0) ? current_frame / fps : 0;
    uint32_t total_seconds = (fps > 0) ? total_frames / fps : 0;
    
    char time_text[32];
    snprintf(time_text, sizeof(time_text), "%s / %s", 
             format_time(current_seconds), format_time(total_seconds));
    lv_label_set_text(time_label, time_text);
    
    // Update play button
    if (play_btn) {
        if (video_state == VIDEO_STATE_PLAYING) {
            lv_label_set_text(lv_obj_get_child(play_btn, 0), "Pause");
        } else {
            lv_label_set_text(lv_obj_get_child(play_btn, 0), "Play");
        }
    }
}

static bool load_video_info(const char* file_path) {
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        ESP_LOGE("VIDEO_PLAYER", "Failed to open video file: %s", file_path);
        return false;
    }
    
    // Read simple header (you'll need to adapt this to your MJPEG format)
    mjpeg_header_t header;
    size_t read_size = fread(&header, 1, sizeof(header), file);
    
    if (read_size != sizeof(header)) {
        // If no header, assume defaults for now
        ESP_LOGW("VIDEO_PLAYER", "No header found, using defaults");
        total_frames = 300;  // Assume 10 seconds at 30fps
        fps = 30;
    } else {
        total_frames = header.frame_count;
        fps = header.fps;
        ESP_LOGI("VIDEO_PLAYER", "Video info: %lu frames, %lu fps", (unsigned long)total_frames, (unsigned long)fps);
    }
    
    fclose(file);
    return true;
}

static bool load_next_frame(void) {
    if (!video_file) return false;
    
    // Read JPEG frame size (4 bytes)
    uint32_t frame_size;
    if (fread(&frame_size, 1, sizeof(frame_size), video_file) != sizeof(frame_size)) {
        ESP_LOGI("VIDEO_PLAYER", "End of video reached");
        video_state = VIDEO_STATE_STOPPED;
        current_frame = 0;
        fseek(video_file, sizeof(mjpeg_header_t), SEEK_SET); // Reset to start
        return false;
    }
    
    if (frame_size > VIDEO_MAX_FRAME_SIZE) {
        ESP_LOGE("VIDEO_PLAYER", "Frame too large: %lu bytes", (unsigned long)frame_size);
        return false;
    }
    
    // Read JPEG frame data
    uint8_t* frame_data = malloc(frame_size);
    if (!frame_data) {
        ESP_LOGE("VIDEO_PLAYER", "Failed to allocate frame buffer");
        return false;
    }
    
    size_t bytes_read = fread(frame_data, 1, frame_size, video_file);
    if (bytes_read != frame_size) {
        ESP_LOGE("VIDEO_PLAYER", "Failed to read complete frame");
        free(frame_data);
        return false;
    }
    
    // Create LVGL image descriptor
    lv_img_dsc_t img_dsc = {
        .header.always_zero = 0,
        .header.w = 240,  // Your video width
        .header.h = 320,  // Your video height
        .data_size = frame_size,
        .header.cf = LV_IMG_CF_RAW,
        .data = frame_data
    };
    
    // Update the image widget
    if (video_image) {
        lv_img_set_src(video_image, &img_dsc);
    }
    
    current_frame++;
    update_controls();
    
    free(frame_data);
    return true;
}

static void frame_timer_cb(void *arg) {
    if (video_state == VIDEO_STATE_PLAYING) {
        if (!load_next_frame()) {
            // End of video or error
            video_state = VIDEO_STATE_STOPPED;
            update_controls();
            
            // Stop timer
            if (frame_timer) {
                esp_timer_stop(frame_timer);
            }
        }
    }
}

static void play_pause_btn_cb(lv_event_t *e) {
    if (video_state == VIDEO_STATE_PLAYING) {
        // Pause
        video_state = VIDEO_STATE_PAUSED;
        if (frame_timer) {
            esp_timer_stop(frame_timer);
        }
        ESP_LOGI("VIDEO_PLAYER", "Video paused");
    } else {
        // Play
        video_state = VIDEO_STATE_PLAYING;
        if (frame_timer) {
            uint32_t frame_interval = 1000000 / fps; // microseconds
            esp_timer_start_periodic(frame_timer, frame_interval);
        }
        ESP_LOGI("VIDEO_PLAYER", "Video playing");
    }
    
    update_controls();
}

static void create_test_video_cb(lv_event_t *e) {
    ESP_LOGI("VIDEO_PLAYER", "Creating test video...");
    create_test_video();
    
    // Show completion message (removed unused parent variable)
    lv_obj_t* mbox = lv_msgbox_create(lv_scr_act(), "Test Video", 
                                      "Test video created:\n/sdcard/test_video.mjpeg\n\n"
                                      "60 frames, 30 FPS\nColorful animation", 
                                      NULL, true);
    lv_obj_center(mbox);
}

static void video_player_back_cb(lv_event_t *e) {
    ESP_LOGI("VIDEO_PLAYER", "Going back to folder app");
    app_manager_switch_to(APP_FOLDER);
}

void video_player_set_file_path(const char* file_path) {
    if (file_path) {
        strncpy(pending_file_path, file_path, sizeof(pending_file_path) - 1);
        pending_file_path[sizeof(pending_file_path) - 1] = '\0';
        ESP_LOGI("VIDEO_PLAYER", "File path set to: %s", pending_file_path);
        
        // Force recreation if already exists
        if (video_screen) {
            ESP_LOGI("VIDEO_PLAYER", "Video player already exists, forcing recreation");
            destroy_video_player_app();
        }
    } else {
        pending_file_path[0] = '\0';
        ESP_LOGW("VIDEO_PLAYER", "File path cleared");
    }
}

void create_video_player_app(void) {
    if (video_screen) {
        destroy_video_player_app();
    }
    
    // Check if file path was set
    if (pending_file_path[0] == '\0') {
        ESP_LOGE("VIDEO_PLAYER", "No file path set! Call video_player_set_file_path() first");
        return;
    }
    
    // Copy pending path to current path
    strncpy(current_file_path, pending_file_path, sizeof(current_file_path) - 1);
    current_file_path[sizeof(current_file_path) - 1] = '\0';
    pending_file_path[0] = '\0';
    
    ESP_LOGI("VIDEO_PLAYER", "Creating video player for: %s", current_file_path);
    
    // Load video information
    if (!load_video_info(current_file_path)) {
        ESP_LOGE("VIDEO_PLAYER", "Failed to load video info");
        return;
    }
    
    // Open video file
    video_file = fopen(current_file_path, "rb");
    if (!video_file) {
        ESP_LOGE("VIDEO_PLAYER", "Failed to open video file");
        return;
    }
    
    // Skip header
    fseek(video_file, sizeof(mjpeg_header_t), SEEK_SET);
    current_frame = 0;
    video_state = VIDEO_STATE_STOPPED;
    
    // Create main screen
    video_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(video_screen, lv_color_hex(0x000000), 0); // Black background
    lv_obj_set_style_pad_all(video_screen, 0, 0);
    
    // Title bar
    lv_obj_t *title_bar = lv_obj_create(video_screen);
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
    lv_obj_add_event_cb(back_btn, video_player_back_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_color(back_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_10, 0);
    lv_obj_center(back_label);
    
    // Create test button
    lv_obj_t *test_btn = lv_btn_create(title_bar);
    lv_obj_set_size(test_btn, 55, 25);
    lv_obj_align(test_btn, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(test_btn, lv_color_hex(0xFF9800), 0); // Orange
    lv_obj_set_style_radius(test_btn, 3, 0);
    lv_obj_add_event_cb(test_btn, create_test_video_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *test_label = lv_label_create(test_btn);
    lv_label_set_text(test_label, "Test");
    lv_obj_set_style_text_color(test_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(test_label, &lv_font_montserrat_10, 0);
    lv_obj_center(test_label);
    
    // Title
    lv_obj_t *title = lv_label_create(title_bar);
    const char* filename = strrchr(current_file_path, '/');
    if (filename) filename++; // Skip the slash
    else filename = current_file_path;
    
    char title_text[80];
    snprintf(title_text, sizeof(title_text), "%.50s", filename);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    
    // Video display area
    video_image = lv_img_create(video_screen);
    lv_obj_set_size(video_image, 240, 240); // Square area for 240x320 rotated content
    lv_obj_set_pos(video_image, (320-240)/2, 35); // Center horizontally
    lv_obj_set_style_bg_color(video_image, lv_color_hex(0x333333), 0);
    
    // Control panel
    control_panel = lv_obj_create(video_screen);
    lv_obj_set_size(control_panel, lv_pct(100), 45);
    lv_obj_set_pos(control_panel, 0, 240+35);
    lv_obj_set_style_bg_color(control_panel, lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(control_panel, 0, 0);
    lv_obj_clear_flag(control_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    // Play/Pause button
    play_btn = lv_btn_create(control_panel);
    lv_obj_set_size(play_btn, 60, 30);
    lv_obj_align(play_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_add_event_cb(play_btn, play_pause_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, "Play");
    lv_obj_set_style_text_color(play_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(play_label, &lv_font_montserrat_10, 0);
    lv_obj_center(play_label);
    
    // Progress bar
    progress_bar = lv_bar_create(control_panel);
    lv_obj_set_size(progress_bar, 150, 10);
    lv_obj_align(progress_bar, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_bar_set_range(progress_bar, 0, 100);
    
    // Time label
    time_label = lv_label_create(control_panel);
    lv_label_set_text(time_label, "00:00 / 00:00");
    lv_obj_set_style_text_color(time_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_8, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 8);
    
    // Create frame timer
    esp_timer_create_args_t timer_args = {
        .callback = frame_timer_cb,
        .arg = NULL,
        .name = "video_frame_timer"
    };
    esp_timer_create(&timer_args, &frame_timer);
    
    // Update initial display
    update_controls();
    
    // Link to app manager
    app_info_t* app_info = app_manager_get_app_info(APP_VIDEO_PLAYER);
    if (app_info) {
        app_info->screen = video_screen;
        ESP_LOGI("VIDEO_PLAYER", "Video player screen linked to app manager");
    }
    
    ESP_LOGI("VIDEO_PLAYER", "Video player app created successfully");
}

void destroy_video_player_app(void) {
    if (video_screen) {
        ESP_LOGI("VIDEO_PLAYER", "Video player app destroyed");
        
        // Stop and delete timer
        if (frame_timer) {
            esp_timer_stop(frame_timer);
            esp_timer_delete(frame_timer);
            frame_timer = NULL;
        }
        
        // Close video file
        if (video_file) {
            fclose(video_file);
            video_file = NULL;
        }
        
        video_state = VIDEO_STATE_STOPPED;
        current_frame = 0;
        total_frames = 0;
        
        lv_obj_del(video_screen);
        video_screen = NULL;
        video_image = NULL;
        control_panel = NULL;
        play_btn = NULL;
        progress_bar = NULL;
        time_label = NULL;
        
        // Clear file paths
        current_file_path[0] = '\0';
        pending_file_path[0] = '\0';
        
        // Clear app manager reference
        app_info_t* app_info = app_manager_get_app_info(APP_VIDEO_PLAYER);
        if (app_info) {
            app_info->screen = NULL;
            ESP_LOGI("VIDEO_PLAYER", "Cleared app manager screen reference");
        }
    }
}

lv_obj_t* video_player_app_get_screen(void) {
    if (pending_file_path[0] != '\0' && !video_screen) {
        ESP_LOGI("VIDEO_PLAYER", "Screen requested but doesn't exist, creating now");
        create_video_player_app();
    }
    
    return video_screen;
}