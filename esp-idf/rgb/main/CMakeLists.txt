idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver espressif__esp-zigbee-lib nvs_flash)
