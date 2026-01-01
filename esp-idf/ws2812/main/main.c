/*
 * ESP32-H2 Zigbee WS2812 LED Strip Controller
 * Based on ESP_Zigbee_quad_switch example
 */

#include "main.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "led_strip.h"

// Configuration
#define LED_STRIP_GPIO      5      // Changé de 8 à 5
#define LED_STRIP_LENGTH    60
#define HA_ESP_LIGHT_ENDPOINT  10

static const char *TAG = "ZIGBEE_WS2812";
static led_strip_handle_t led_strip = NULL;

// État de la lumière
typedef struct {
    bool on_off;
    uint8_t level;
    uint8_t hue;
    uint8_t saturation;
} light_state_t;

static light_state_t light_state = {
    .on_off = false,
    .level = 128,
    .hue = 0,
    .saturation = 254
};

// Conversion HSV vers RGB
static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
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
static void update_led_strip(void)
{
    uint8_t r, g, b;
    
    if (!light_state.on_off) {
        led_strip_clear(led_strip);
    } else {
        hsv_to_rgb(light_state.hue, light_state.saturation, light_state.level, &r, &g, &b);
        for (int i = 0; i < LED_STRIP_LENGTH; i++) {
            led_strip_set_pixel(led_strip, i, r, g, b);
        }
    }
    led_strip_refresh(led_strip);
}

// Gestionnaire des attributs Zigbee
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool light_changed = false;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Message vide");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                        "Message reçu : statut d'erreur (%d)", message->info.status);

    ESP_LOGI(TAG, "Message reçu : endpoint(%d), cluster(0x%x), attribut(0x%x), taille données(%d)",
             message->info.dst_endpoint, message->info.cluster, message->attribute.id, message->attribute.data.size);

    if (message->info.dst_endpoint == HA_ESP_LIGHT_ENDPOINT) {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                light_state.on_off = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state.on_off;
                ESP_LOGI(TAG, "Light set to %s", light_state.on_off ? "ON" : "OFF");
                light_changed = true;
            }
        }
        else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                light_state.level = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : light_state.level;
                ESP_LOGI(TAG, "Light level set to %d", light_state.level);
                light_changed = true;
            }
        }
        else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                light_state.hue = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : light_state.hue;
                ESP_LOGI(TAG, "Light hue set to %d", light_state.hue);
                light_changed = true;
            }
            else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                light_state.saturation = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : light_state.saturation;
                ESP_LOGI(TAG, "Light saturation set to %d", light_state.saturation);
                light_changed = true;
            }
        }
    }

    if (light_changed) {
        update_led_strip();
    }

    return ret;
}

// Gestionnaire des actions Zigbee
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Callback d'action Zigbee reçu (0x%x)", callback_id);
        break;
    }
    return ret;
}

// Gestionnaire des signaux Zigbee
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialisation de la pile Zigbee");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Démarrage de l'appareil en mode %s factory-reset", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Démarrage de la direction réseau");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Redémarrage de l'appareil");
            }
        } else {
            ESP_LOGW(TAG, "Échec du redémarrage de l'appareil (statut : %s)", esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Rejoint le réseau avec succès (Extended PAN ID : %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID : 0x%04hx, Canal : %d, Adresse courte : 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Échec de la direction réseau après 30 secondes (statut : %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "Signal de la pile ZDO : %s (0x%x), statut : %s",
                 esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        break;
    }
}

// Tâche Zigbee principale
static void esp_zb_task(void *pvParameters)
{
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    // Configuration des clusters pour lumière couleur dimmable
    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
    
    // Initialiser les valeurs de couleur
    light_cfg.color_cfg.current_hue = 0;
    light_cfg.color_cfg.current_saturation = 254;
    light_cfg.color_cfg.color_mode = 0;  // 0 = Hue/Saturation mode
    light_cfg.color_cfg.enhanced_color_mode = 0;
    light_cfg.color_cfg.color_capabilities = 0x0001;  // Bit 0 = Hue/Saturation supported
    
    // Création des clusters
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&light_cfg.basic_cfg);
    
    // Ajout des attributs Manufacturer et Model pour identification
    char manufacturer[] = {11, 'E', 'S', 'P', '3', '2', '-', 'Z', 'i', 'g', 'b', 'e'};  // "ESP32-Zigbee"
    char model[] = {12, 'W', 'S', '2', '8', '1', '2', '_', 'L', 'i', 'g', 'h', 't'};     // "WS2812_Light"
    
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);
    
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(&light_cfg.identify_cfg);
    esp_zb_attribute_list_t *groups_cluster = esp_zb_groups_cluster_create(&light_cfg.groups_cfg);
    esp_zb_attribute_list_t *scenes_cluster = esp_zb_scenes_cluster_create(&light_cfg.scenes_cfg);
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&light_cfg.on_off_cfg);
    esp_zb_attribute_list_t *level_cluster = esp_zb_level_cluster_create(&light_cfg.level_cfg);
    esp_zb_attribute_list_t *color_cluster = esp_zb_color_control_cluster_create(&light_cfg.color_cfg);

    // Création de la liste de clusters
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(cluster_list, groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(cluster_list, scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(cluster_list, level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(cluster_list, color_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Création de l'endpoint
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
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
    esp_zb_stack_main_loop();
}

// Fonction principale
void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

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

    ESP_LOGI(TAG, "Zigbee WS2812 Light - GPIO: %d, LEDs: %d", LED_STRIP_GPIO, LED_STRIP_LENGTH);

    // Création de la tâche Zigbee
    if (xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 6, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Échec de la création de la tâche Zigbee");
    }
}
