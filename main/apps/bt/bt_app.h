#ifndef BT_APP_H
#define BT_APP_H

#include "lvgl.h"

void create_bt_app(void);
void destroy_bt_app(void);
lv_obj_t* bt_app_get_screen(void);

#endif // BT_APP_H