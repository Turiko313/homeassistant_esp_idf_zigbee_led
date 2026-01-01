idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES led_strip esp_zigbee_core esp_zigbee_cluster nvs_flash)
