#include "ui_styles.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI_STYLES";

// Global style objects
static lv_style_t style_card;
static lv_style_t style_button;
static lv_style_t style_button_pressed;
static lv_style_t style_title;
static lv_style_t style_subtitle;
static lv_style_t style_body;
static lv_style_t style_caption;

void ui_init_styles(void) {
    ESP_LOGI(TAG, "Initializing UI styles");

    // Card style
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_hex(UI_COLOR_BG_CARD));
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_card, lv_color_hex(UI_COLOR_BORDER));
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_opa(&style_card, LV_OPA_30);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_shadow_width(&style_card, 10);
    lv_style_set_shadow_color(&style_card, lv_color_hex(UI_COLOR_SHADOW));
    lv_style_set_shadow_opa(&style_card, LV_OPA_20);
    lv_style_set_shadow_ofs_x(&style_card, 0);
    lv_style_set_shadow_ofs_y(&style_card, 4);
    lv_style_set_pad_all(&style_card, 16);

    // Button style
    lv_style_init(&style_button);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_radius(&style_button, 8);
    lv_style_set_shadow_width(&style_button, 6);
    lv_style_set_shadow_color(&style_button, lv_color_hex(UI_COLOR_SHADOW));
    lv_style_set_shadow_opa(&style_button,LV_OPA_20);
    lv_style_set_shadow_ofs_y(&style_button, 2);
    lv_style_set_border_width(&style_button, 0);
    lv_style_set_text_color(&style_button, lv_color_hex(UI_COLOR_TEXT_PRIMARY));
    lv_style_set_pad_hor(&style_button, 16);
    lv_style_set_pad_ver(&style_button, 12);

    // Button pressed style
    lv_style_init(&style_button_pressed);
    lv_style_set_shadow_ofs_y(&style_button_pressed, 1);
    lv_style_set_shadow_width(&style_button_pressed, 4);
    lv_style_set_transform_zoom(&style_button_pressed, 245); // 95% scale

    // Text styles
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_hex(UI_COLOR_TEXT_PRIMARY));
    lv_style_set_text_font(&style_title, &lv_font_montserrat_18);

    lv_style_init(&style_subtitle);
    lv_style_set_text_color(&style_subtitle, lv_color_hex(UI_COLOR_TEXT_SECONDARY));
    lv_style_set_text_font(&style_subtitle, &lv_font_montserrat_14);

    lv_style_init(&style_body);
    lv_style_set_text_color(&style_body, lv_color_hex(UI_COLOR_TEXT_PRIMARY));
    lv_style_set_text_font(&style_body, &lv_font_montserrat_14);

    lv_style_init(&style_caption);
    lv_style_set_text_color(&style_caption, lv_color_hex(UI_COLOR_TEXT_SECONDARY));
    lv_style_set_text_font(&style_caption, &lv_font_montserrat_12);

    ESP_LOGI(TAG, "UI styles initialized");
}

void ui_apply_card_style(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_card, LV_PART_MAIN);
}

void ui_apply_button_style(lv_obj_t* obj, uint32_t bg_color) {
    lv_obj_add_style(obj, &style_button, LV_PART_MAIN);
    lv_obj_add_style(obj, &style_button_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_lighten(lv_color_hex(bg_color), 20), LV_PART_MAIN | LV_STATE_PRESSED);
}

void ui_apply_title_style(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_title, LV_PART_MAIN);
}

void ui_apply_subtitle_style(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_subtitle, LV_PART_MAIN);
}

void ui_apply_body_style(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_body, LV_PART_MAIN);
}

void ui_apply_caption_style(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_caption, LV_PART_MAIN);
}

lv_obj_t* ui_create_card(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    ui_apply_card_style(card);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    return card;
}

lv_obj_t* ui_create_title_bar(lv_obj_t* parent, const char* title, const char* subtitle) {
    // Create title bar container
    lv_obj_t* title_bar = lv_obj_create(parent);
    lv_obj_set_size(title_bar, LV_PCT(100), 60);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(UI_COLOR_BG_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_bar, 16, LV_PART_MAIN);

    // Title text
    lv_obj_t* title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, title);
    ui_apply_title_style(title_label);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 0, -5);

    // Subtitle text (if provided)
    if (subtitle) {
        lv_obj_t* subtitle_label = lv_label_create(title_bar);
        lv_label_set_text(subtitle_label, subtitle);
        ui_apply_caption_style(subtitle_label);
        lv_obj_align(subtitle_label, LV_ALIGN_LEFT_MID, 0, 15);
    }

    return title_bar;
}

lv_obj_t* ui_create_modern_button(lv_obj_t* parent, const char* text, uint32_t color) {
    lv_obj_t* btn = lv_btn_create(parent);
    ui_apply_button_style(btn, color);
    
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    return btn;
}

lv_obj_t* ui_create_icon_button(lv_obj_t* parent, const char* icon, const char* label_text, uint32_t color) {
    lv_obj_t* btn = lv_btn_create(parent);
    ui_apply_button_style(btn, color);
    lv_obj_set_style_pad_all(btn, 12, LV_PART_MAIN);
    
    // Create container for icon and text
    lv_obj_t* content = lv_obj_create(btn);
    lv_obj_set_size(content, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);
    lv_obj_center(content);
    
    // Icon (large text)
    lv_obj_t* icon_label = lv_label_create(content);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_align(icon_label, LV_ALIGN_TOP_MID, 0, 0);
    
    // Label
    lv_obj_t* text_label = lv_label_create(content);
    lv_label_set_text(text_label, label_text);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(text_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_align_to(text_label, icon_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
    
    return btn;
}

void ui_animate_button_press(lv_obj_t* obj) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, 256, 245);
    lv_anim_set_time(&anim, 100);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_zoom);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
    
    // Return to normal
    lv_anim_set_values(&anim, 245, 256);
    lv_anim_set_delay(&anim, 100);
    lv_anim_start(&anim);
}

void ui_animate_slide_in(lv_obj_t* obj) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, lv_obj_get_x(obj) + 50, lv_obj_get_x(obj));
    lv_anim_set_time(&anim, 300);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
    
    // Fade in
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_start(&anim);
}

void ui_animate_fade_in(lv_obj_t* obj) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&anim, 250);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_start(&anim);
}