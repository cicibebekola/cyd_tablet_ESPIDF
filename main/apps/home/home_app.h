#pragma once
#include "lvgl.h"

// Creation and destruction functions
void create_home_app(void);
void destroy_home_app(void);

// Optional helper to expose the screen pointer
lv_obj_t* home_app_get_screen(void);
