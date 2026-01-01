idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES led_strip espressif__esp-zigbee-lib nvs_flash)
