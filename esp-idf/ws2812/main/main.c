/*
 * ESP32-H2 Zigbee WS2812 LED Strip Controller
 * Version simple - Contrôle couleur RGB via Zigbee
 */

#include "main.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "led_strip.h"

// Configuration
#define LED_STRIP_GPIO      5
#define LED_STRIP_LENGTH    60

static const char *TAG = "ZIGBEE_WS2812";
static led_strip_handle_t led_strip = NULL;

static light_state_t light_state = {
    .on_off = false,
    .level = 0,        // Toujours 0% au démarrage
    .hue = 0,
    .saturation = 0,
    .color_x = 0x616B,
    .color_y = 0x607D
};

// Conversion HSV vers RGB (corrigée)
static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    // Cas spécial : saturation = 0 ? couleur blanche/grise
    if (s == 0) {
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    uint32_t hue = (uint32_t)h * 360 / 254;      // h (0-254) ? 0-360°
    uint8_t region = hue / 60;
    uint16_t rem = hue % 60;
    uint8_t remainder = (rem * 255) / 60;        // CORRECTION: 0 � 255
    
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

// Conversion XY (CIE 1931) vers RGB
static void xy_to_rgb(uint16_t x, uint16_t y, uint8_t brightness, uint8_t *r, uint8_t *g, uint8_t *b)
{
    // Convertir les coordonnées Zigbee (0-65535) en float (0.0-1.0)
    float fx = (float)x / 65535.0f;
    float fy = (float)y / 65535.0f;
    
    // Éviter division par zéro
    if (fy < 0.0001f) fy = 0.0001f;
    
    // Convertir XY vers XYZ (avec Y = brightness normalisé)
    float z = 1.0f - fx - fy;
    float Y = (float)brightness / 254.0f;
    float X = (Y / fy) * fx;
    float Z = (Y / fy) * z;
    
    // Matrice de conversion XYZ vers RGB (sRGB D65)
    float fr = X * 3.2406f + Y * -1.5372f + Z * -0.4986f;
    float fg = X * -0.9689f + Y * 1.8758f + Z * 0.0415f;
    float fb = X * 0.0557f + Y * -0.2040f + Z * 1.0570f;
    
    // Correction gamma (sRGB)
    if (fr > 0.0031308f) {
        fr = 1.055f * powf(fr, 1.0f/2.4f) - 0.055f;
    } else {
        fr = 12.92f * fr;
    }
    
    if (fg > 0.0031308f) {
        fg = 1.055f * powf(fg, 1.0f/2.4f) - 0.055f;
    } else {
        fg = 12.92f * fg;
    }
    
    if (fb > 0.0031308f) {
        fb = 1.055f * powf(fb, 1.0f/2.4f) - 0.055f;
    } else {
        fb = 12.92f * fb;
    }
    
    // Clamping et conversion en 8 bits
    if (fr < 0.0f) {
        fr = 0.0f;
    }
    if (fr > 1.0f) {
        fr = 1.0f;
    }
    
    if (fg < 0.0f) {
        fg = 0.0f;
    }
    if (fg > 1.0f) {
        fg = 1.0f;
    }
    
    if (fb < 0.0f) {
        fb = 0.0f;
    }
    if (fb > 1.0f) {
        fb = 1.0f;
    }
    
    *r = (uint8_t)(fr * 255.0f);
    *g = (uint8_t)(fg * 255.0f);
    *b = (uint8_t)(fb * 255.0f);
}

// Mise à jour du ruban LED
static void update_led_strip(void)
{
    if (!light_state.on_off) {
        ESP_LOGI(TAG, "LED OFF");
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
        return;
    }
    
    uint8_t r, g, b;
    xy_to_rgb(light_state.color_x, light_state.color_y, light_state.level, &r, &g, &b);
    
    ESP_LOGI(TAG, "LED ON - Level=%d, XY=(0x%04X,0x%04X) → RGB(%d,%d,%d)", 
             light_state.level, light_state.color_x, light_state.color_y, r, g, b);
    
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
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
                        "Statut d'erreur (%d)", message->info.status);

    if (message->info.dst_endpoint == HA_ESP_LIGHT_ENDPOINT) {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                light_state.on_off = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state.on_off;
                ESP_LOGI(TAG, "ON/OFF → %s", light_state.on_off ? "ON" : "OFF");
                light_changed = true;
            }
        }
        else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                light_state.level = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : light_state.level;
                ESP_LOGI(TAG, "LEVEL → %d", light_state.level);
                light_changed = true;
            }
        }
        else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
                light_state.color_x = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : light_state.color_x;
                ESP_LOGI(TAG, "COLOR_X → 0x%04X (%.4f)", light_state.color_x, (float)light_state.color_x / 65535.0f);
                
                uint8_t r, g, b;
                xy_to_rgb(light_state.color_x, light_state.color_y, light_state.level, &r, &g, &b);
                
                if (light_state.on_off) {
                    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
                        led_strip_set_pixel(led_strip, i, r, g, b);
                    }
                    led_strip_refresh(led_strip);
                }
            }
            else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
                light_state.color_y = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : light_state.color_y;
                ESP_LOGI(TAG, "COLOR_Y → 0x%04X (%.4f)", light_state.color_y, (float)light_state.color_y / 65535.0f);
                
                uint8_t r, g, b;
                xy_to_rgb(light_state.color_x, light_state.color_y, light_state.level, &r, &g, &b);
                
                if (light_state.on_off) {
                    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
                        led_strip_set_pixel(led_strip, i, r, g, b);
                    }
                    led_strip_refresh(led_strip);
                }
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
        ESP_LOGW(TAG, "Callback Zigbee non géré (0x%x)", callback_id);
        break;
    }
    return ret;
}

// Gestionnaire des signaux Zigbee
static void bdb_start_network_steering_cb(uint8_t param)
{
    esp_err_t ret = esp_zb_bdb_start_top_level_commissioning(param);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec relance network steering : %s", esp_err_to_name(ret));
    }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialisation Zigbee");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Démarrage réseau (appairage)");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Redémarrage appareil");
            }
        } else {
            ESP_LOGW(TAG, "Échec redémarrage: %s", esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Connecté au réseau Zigbee - PAN:0x%04hx, Canal:%d, Addr:0x%04hx",
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Échec connexion réseau: %s", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_network_steering_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "Signal ZDO: %s (0x%x)", esp_zb_zdo_signal_to_string(sig_type), sig_type);
        break;
    }
}

// Tâche Zigbee principale
static void esp_zb_task(void *pvParameters)
{
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();

    // Configuration : SEULEMENT XY (pas de HS, pas de Color Temperature)
    light_cfg.color_cfg.color_mode = 1;                         // 1 = XY
    light_cfg.color_cfg.enhanced_color_mode = 1;                // Enhanced mode
    light_cfg.color_cfg.color_capabilities = 0x0008;            // XY (bit 3) SEULEMENT
    light_cfg.color_cfg.current_x = light_state.color_x;
    light_cfg.color_cfg.current_y = light_state.color_y;
    
    // Note: start_up_on_off n'existe pas dans la structure ESP-IDF
    // Le power-on behavior est géré manuellement via NVS
    
    // ===== Endpoint 10 : LIGHT =====
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&light_cfg.basic_cfg);
    
    char manufacturer[] = {11, 'E', 'S', 'P', '3', '2', '-', 'Z', 'i', 'g', 'b', 'e'};
    char model[] = {12, 'W', 'S', '2', '8', '1', '2', '_', 'L', 'i', 'g', 'h', 't'};
    
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);
    
    // NE PAS ajouter Identify cluster pour éviter les effets
    esp_zb_attribute_list_t *groups_cluster = esp_zb_groups_cluster_create(&light_cfg.groups_cfg);
    esp_zb_attribute_list_t *scenes_cluster = esp_zb_scenes_cluster_create(&light_cfg.scenes_cfg);
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&light_cfg.on_off_cfg);
    esp_zb_attribute_list_t *level_cluster = esp_zb_level_cluster_create(&light_cfg.level_cfg);
    esp_zb_attribute_list_t *color_cluster = esp_zb_color_control_cluster_create(&light_cfg.color_cfg);

    esp_zb_cluster_list_t *cluster_list_light = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list_light, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    // IDENTIFY CLUSTER RETIRÉ VOLONTAIREMENT
    esp_zb_cluster_list_add_groups_cluster(cluster_list_light, groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(cluster_list_light, scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list_light, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(cluster_list_light, level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(cluster_list_light, color_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Créer la liste des endpoints
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    
    // Endpoint 10 : Light
    esp_zb_endpoint_config_t endpoint_light_config = {
        .endpoint = HA_ESP_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list_light, endpoint_light_config);
    
    esp_zb_device_register(ep_list);

    ESP_LOGI(TAG, "Appareil enregistré: Color Dimmable Light (XY only)");

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

    // Configuration LED Strip WS2812
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_LENGTH,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // Démarrage : LEDs éteintes (niveau 0%)
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);

    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "  Zigbee WS2812 LED Strip Controller");
    ESP_LOGI(TAG, "  GPIO: %d | LEDs: %d", LED_STRIP_GPIO, LED_STRIP_LENGTH);
    ESP_LOGI(TAG, "  Demarrage: OFF (0%%)");
    ESP_LOGI(TAG, "===================================");

    // Création de la tâche Zigbee
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 6, NULL);
}
