/*
 * ESP32-H2/C6 Zigbee RGB PWM LED Controller
 * 
 * Contrôle un ruban RGB analogique via PWM et Zigbee
 */

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_zigbee_core.h"

#define PWM_RED_GPIO        4
#define PWM_GREEN_GPIO      5
#define PWM_BLUE_GPIO       6

#define PWM_FREQ_HZ         5000
#define PWM_RESOLUTION      LEDC_TIMER_8_BIT

static const char *TAG = "ZIGBEE_RGB";

static struct {
    bool on_off;
    uint8_t level;
    uint8_t hue;
    uint8_t saturation;
} light_state = {
    .on_off = false,
    .level = 128,
    .hue = 0,
    .saturation = 254
};

static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint16_t h_scaled = h * 360 / 254;
    uint8_t s_scaled = s * 100 / 254;
    
    uint8_t region = h_scaled / 60;
    uint8_t remainder = (h_scaled - (region * 60)) * 6;
    
    uint8_t p = (v * (255 - s_scaled)) >> 8;
    uint8_t q = (v * (255 - ((s_scaled * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s_scaled * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

static void update_pwm(void) {
    uint8_t r, g, b;
    
    if (!light_state.on_off) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
    } else {
        hsv_to_rgb(light_state.hue, light_state.saturation, light_state.level, &r, &g, &b);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, r);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, g);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, b);
    }
    
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
            light_state.on_off = *(bool *)message->attribute.data.value;
            ESP_LOGI(TAG, "Light %s", light_state.on_off ? "ON" : "OFF");
            update_pwm();
        }
    } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) {
            light_state.level = *(uint8_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "Level: %d", light_state.level);
            update_pwm();
        }
    } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID) {
            light_state.hue = *(uint8_t *)message->attribute.data.value;
            update_pwm();
        } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID) {
            light_state.saturation = *(uint8_t *)message->attribute.data.value;
            update_pwm();
        }
    }
    return ESP_OK;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    if (callback_id == ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID) {
        return zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
    }
    return ESP_OK;
}

static void init_pwm(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);
    
    ledc_channel_config_t channels[] = {
        {LEDC_CHANNEL_0, PWM_RED_GPIO, LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, 0, 0},
        {LEDC_CHANNEL_1, PWM_GREEN_GPIO, LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, 0, 0},
        {LEDC_CHANNEL_2, PWM_BLUE_GPIO, LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, 0, 0}
    };
    
    for (int i = 0; i < 3; i++) {
        ledc_channel_config(&channels[i]);
    }
}

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
    };
    
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    
    init_pwm();
    
    ESP_LOGI(TAG, "Zigbee RGB Light - GPIO R:%d G:%d B:%d", PWM_RED_GPIO, PWM_GREEN_GPIO, PWM_BLUE_GPIO);
    
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
