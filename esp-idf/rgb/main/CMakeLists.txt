idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp-zigbee-lib nvs_flash)
