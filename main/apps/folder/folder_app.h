#pragma once

// Forward declare the LVGL object type to avoid include issues
typedef struct _lv_obj_t lv_obj_t;

// Creation and destruction functions
void create_folder_app(void);
void destroy_folder_app(void);

// Helper to expose the screen pointer
lv_obj_t* folder_app_get_screen(void);

// Additional functionality specific to folder app
void folder_app_refresh(void);