/*
 * ESP32-H2 Zigbee WS2812 LED Strip Controller
 * 
 * Controle un ruban WS2812 via Zigbee comme une ampoule RGB
 * Compatible Home Assistant (ZHA / zigbee2mqtt)
 */

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee_ha_standard.h"
#include "led_strip.h"

#define LED_STRIP_GPIO      8
#define LED_STRIP_LENGTH    60

static const char *TAG = "ZIGBEE_WS2812";
static led_strip_handle_t led_strip = NULL;

// État de la lumière
static struct {
    bool on_off;
    uint8_t level;      // 0-255
    uint8_t hue;        // 0-254
    uint8_t saturation; // 0-254
} light_state = {
    .on_off = false,
    .level = 128,
    .hue = 0,
    .saturation = 254
};

// Conversion HSV vers RGB
static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint16_t h_scaled = h * 360 / 254;
    uint8_t region = h_scaled / 60;
    uint8_t remainder = (h_scaled - (region * 60)) * 6;
    
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

// Mise à jour du ruban LED
static void update_led_strip(void) {
    uint8_t r, g, b;
    
    if (!light_state.on_off) {
        // Éteint
        led_strip_clear(led_strip);
    } else {
        // Calcul RGB depuis HSV
        hsv_to_rgb(light_state.hue, light_state.saturation, light_state.level, &r, &g, &b);
        
        // Applique la couleur à toutes les LEDs
        for (int i = 0; i < LED_STRIP_LENGTH; i++) {
            led_strip_set_pixel(led_strip, i, r, g, b);
        }
    }
    led_strip_refresh(led_strip);
}

// Callback attributs Zigbee
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
    esp_err_t ret = ESP_OK;
    
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
            light_state.on_off = *(bool *)message->attribute.data.value;
            ESP_LOGI(TAG, "Light %s", light_state.on_off ? "ON" : "OFF");
            update_led_strip();
        }
    } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) {
            light_state.level = *(uint8_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "Level: %d", light_state.level);
            update_led_strip();
        }
    } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID) {
            light_state.hue = *(uint8_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "Hue: %d", light_state.hue);
            update_led_strip();
        } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID) {
            light_state.saturation = *(uint8_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "Saturation: %d", light_state.saturation);
            update_led_strip();
        }
    }
    
    return ret;
}

// Callback actions Zigbee
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    esp_err_t ret = ESP_OK;
    
    switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
            ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
            break;
        default:
            ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
            break;
    }
    
    return ret;
}

// Task Zigbee
static void esp_zb_task(void *pvParameters) {
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    
    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    
    esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(&light_cfg.basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&light_cfg.identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, esp_zb_on_off_cluster_create(&light_cfg.on_off_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(cluster_list, esp_zb_level_cluster_create(&light_cfg.level_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(cluster_list, esp_zb_color_control_cluster_create(&light_cfg.color_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,
        .app_device_version = 0
    };
    
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Configuration WS2812
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_LENGTH,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    
    ESP_LOGI(TAG, "Starting Zigbee WS2812 Light");
    ESP_LOGI(TAG, "GPIO: %d, LEDs: %d", LED_STRIP_GPIO, LED_STRIP_LENGTH);
    
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
