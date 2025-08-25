#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include "lvgl.h"

// App IDs
typedef enum {
    APP_HOME = 0,
    APP_WIFI,
    APP_BLUETOOTH,
    APP_FOLDER,
    APP_MAX_COUNT
} app_id_t;

// App info structure
typedef struct {
    app_id_t id;
    const char* name;
    uint32_t color;
    void (*create)(void);
    void (*destroy)(void);
    lv_obj_t* screen;
} app_info_t;

// App manager functions
void app_manager_init(void);
void app_manager_switch_to(app_id_t target);
void app_manager_go_home(void);
app_id_t app_manager_get_current_app(void);
app_info_t* app_manager_get_app_info(app_id_t id);

// UI helper function
void ui_switch_to_screen(lv_obj_t* new_screen);

#endif // APP_MANAGER_H