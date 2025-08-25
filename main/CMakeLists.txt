file(GLOB_RECURSE SOURCES
    "main.c"
    "app_manager.c"
    "ui_styles.c"
    "sd_card_manager.c"
    "apps/*/*.c"  # This will include all .c files in subdirectories
)

idf_component_register(SRCS ${SOURCES}
                      INCLUDE_DIRS ".")