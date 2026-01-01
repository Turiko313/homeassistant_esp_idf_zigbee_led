idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES led_strip esp_zb_lib nvs_flash)
