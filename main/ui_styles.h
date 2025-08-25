#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Modern color palette
#define UI_COLOR_PRIMARY        0x2196F3    // Material Blue
#define UI_COLOR_PRIMARY_DARK   0x1976D2    // Darker Blue
#define UI_COLOR_SECONDARY      0xFF9800    // Material Orange
#define UI_COLOR_ACCENT         0x4CAF50    // Material Green
#define UI_COLOR_SUCCESS        0x4CAF50    // Green
#define UI_COLOR_WARNING        0xFF9800    // Orange
#define UI_COLOR_ERROR          0xF44336    // Red
#define UI_COLOR_INFO           0x2196F3    // Blue

// Background colors
#define UI_COLOR_BG_DARK        0x121212    // Very dark background
#define UI_COLOR_BG_CARD        0x1E1E1E    // Card background
#define UI_COLOR_BG_SURFACE     0x2D2D2D    // Surface background
#define UI_COLOR_BG_ELEVATED    0x383838    // Elevated surface

// Text colors
#define UI_COLOR_TEXT_PRIMARY   0xFFFFFF    // Primary text (white)
#define UI_COLOR_TEXT_SECONDARY 0xB3B3B3    // Secondary text (light gray)
#define UI_COLOR_TEXT_DISABLED  0x666666    // Disabled text (dark gray)

// Border and divider colors
#define UI_COLOR_BORDER         0x333333    // Border color
#define UI_COLOR_DIVIDER        0x404040    // Divider color

// Shadow colors
#define UI_COLOR_SHADOW         0x000000    // Shadow color

// App-specific colors
#define UI_COLOR_CALCULATOR     0xFF6B35    // Orange-red
#define UI_COLOR_SETTINGS       0x9E9E9E    // Gray
#define UI_COLOR_WIFI           0x4CAF50    // Green
#define UI_COLOR_FILES          0xFFC107    // Amber
#define UI_COLOR_SYSTEM         0x9C27B0    // Purple
#define UI_COLOR_HOME           UI_COLOR_PRIMARY

// Modern styling functions
void ui_init_styles(void);
void ui_apply_card_style(lv_obj_t* obj);
void ui_apply_button_style(lv_obj_t* obj, uint32_t bg_color);
void ui_apply_title_style(lv_obj_t* obj);
void ui_apply_subtitle_style(lv_obj_t* obj);
void ui_apply_body_style(lv_obj_t* obj);
void ui_apply_caption_style(lv_obj_t* obj);

// Layout helpers
lv_obj_t* ui_create_card(lv_obj_t* parent);
lv_obj_t* ui_create_title_bar(lv_obj_t* parent, const char* title, const char* subtitle);
lv_obj_t* ui_create_modern_button(lv_obj_t* parent, const char* text, uint32_t color);
lv_obj_t* ui_create_icon_button(lv_obj_t* parent, const char* icon, const char* label, uint32_t color);

// Animation helpers
void ui_animate_button_press(lv_obj_t* obj);
void ui_animate_slide_in(lv_obj_t* obj);
void ui_animate_fade_in(lv_obj_t* obj);

#ifdef __cplusplus
}
#endif