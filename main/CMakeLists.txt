idf_component_register(SRCS
"main.c"
"app_manager.h"
"app_manager.c"
"ui_styles.h"
"ui_styles.c"
"app_manager.c"
"sd_card_manager.c"

    "apps/home/home_app.c"
    "apps/wifi/wifi_app.c" 
    "apps/bt/bt_app.c"
    "apps/folder/folder_app.c"

                    INCLUDE_DIRS ".")
