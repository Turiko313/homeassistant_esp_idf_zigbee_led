idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES led_strip espressif__esp-zigbee-lib nvs_flash)

# Add ha/ subdirectory from esp-zigbee-lib to include paths
target_include_directories(${COMPONENT_LIB} PRIVATE 
    "${CMAKE_CURRENT_LIST_DIR}/../../managed_components/espressif__esp-zigbee-lib/include/ha")
