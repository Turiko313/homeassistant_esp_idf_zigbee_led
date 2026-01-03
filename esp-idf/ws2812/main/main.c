/*
 * ESP32-H2 Zigbee WS2812 LED Strip Controller
 * Version avec support des effets
 */

#include "main.h"
#include "effects.h"
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
    .level = 0,
    .color_x = 0x616B,
    .color_y = 0x607D,
    .effect_id = 0,
    .speed_rainbow = 128,
    .speed_strobe = 128,
    .speed_twinkle = 128
};

// Dernier niveau non nul pour eviter le blocage a 0% au premier ON
static uint8_t last_level_non_zero = 200;

// Stockage persistant des attributs manufacturer-specific
static uint8_t attr_effect_value = 0;
static uint8_t attr_speed_rainbow = 128;
static uint8_t attr_speed_strobe  = 128;
static uint8_t attr_speed_twinkle = 128;

// Helper pour mettre à jour un attribut ZCL U8 avec log d'erreur
static void set_zcl_attr_u8(uint8_t endpoint, uint16_t cluster_id, uint16_t attr_id, uint8_t value)
{
    esp_err_t err = esp_zb_zcl_set_attribute_val(endpoint,
        cluster_id,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        attr_id,
        &value,
        false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_attr 0x%04X/0x%04X failed: %s", cluster_id, attr_id, esp_err_to_name(err));
    }
}

// Fonction helper pour remettre l'effet sur none et notifier Z2M
static void reset_effect_to_none(void)
{
    if (light_state.effect_id != EFFECT_NONE) {
        light_state.effect_id = EFFECT_NONE;
        effects_stop();
        attr_effect_value = 0;
        set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
            ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
            0xF000,
            attr_effect_value);
        ESP_LOGI(TAG, "Effet remis sur none");
    }
}

// Fonction helper pour obtenir la vitesse de l'effet actuel
static uint8_t get_current_effect_speed(void)
{
    switch (light_state.effect_id) {
        case EFFECT_RAINBOW: return light_state.speed_rainbow;
        case EFFECT_STROBE:  return light_state.speed_strobe;
        case EFFECT_TWINKLE: return light_state.speed_twinkle;
        default: return 128;
    }
}

// Conversion XY (CIE 1931) vers RGB
static void xy_to_rgb(uint16_t x, uint16_t y, uint8_t brightness, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float fx = (float)x / 65535.0f;
    float fy = (float)y / 65535.0f;
    
    if (fy < 0.0001f) fy = 0.0001f;
    
    float z = 1.0f - fx - fy;
    float Y = (float)brightness / 254.0f;
    float X = (Y / fy) * fx;
    float Z = (Y / fy) * z;
    
    float fr = X * 3.2406f + Y * -1.5372f + Z * -0.4986f;
    float fg = X * -0.9689f + Y * 1.8758f + Z * 0.0415f;
    float fb = X * 0.0557f + Y * -0.2040f + Z * 1.0570f;
    
    // Correction gamma (sRGB)
    fr = (fr > 0.0031308f) ? (1.055f * powf(fr, 1.0f/2.4f) - 0.055f) : (12.92f * fr);
    fg = (fg > 0.0031308f) ? (1.055f * powf(fg, 1.0f/2.4f) - 0.055f) : (12.92f * fg);
    fb = (fb > 0.0031308f) ? (1.055f * powf(fb, 1.0f/2.4f) - 0.055f) : (12.92f * fb);
    
    // Clamping
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
        effects_stop();
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
        return;
    }
    
    // Si un effet est actif, le système d'effets gère l'affichage
    if (light_state.effect_id != EFFECT_NONE && effects_is_active()) {
        ESP_LOGI(TAG, "Effet actif: %d", light_state.effect_id);
        return;
    }
    
    // Mode couleur fixe (pas d'effet)
    uint8_t r, g, b;
    xy_to_rgb(light_state.color_x, light_state.color_y, light_state.level, &r, &g, &b);
    
    ESP_LOGI(TAG, "LED ON - Level=%d, XY=(0x%04X,0x%04X) -> RGB(%d,%d,%d)", 
             light_state.level, light_state.color_x, light_state.color_y, r, g, b);
    
    effects_set_base_color(r, g, b);
    
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
                bool new_on = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state.on_off;
                ESP_LOGI(TAG, "ON/OFF -> %s", new_on ? "ON" : "OFF");
                
                if (new_on && !light_state.on_off) {
                    // Passage de OFF a ON
                    light_state.on_off = true;
                    
                    // Forcer un niveau minimum de 80% si level=0
                    if (light_state.level == 0) {
                        light_state.level = 200;  // ~80%
                        last_level_non_zero = light_state.level;
                        set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                            ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
                            ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID,
                            light_state.level);
                        effects_set_brightness(light_state.level);
                        ESP_LOGI(TAG, "Auto level = 200 (80%%) au premier ON");
                    }
                } else if (!new_on && light_state.on_off) {
                    // Passage de ON a OFF
                    light_state.on_off = false;
                    reset_effect_to_none();
                } else {
                    light_state.on_off = new_on;
                }
                
                light_changed = true;
            }
        }
        else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                light_state.level = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : light_state.level;
                if (light_state.level > 0) {
                    last_level_non_zero = light_state.level;
                }
                ESP_LOGI(TAG, "LEVEL -> %d", light_state.level);
                
                // Mettre a jour la luminosite des effets
                effects_set_brightness(light_state.level);
                
                // Si on change la luminosite a une valeur > 0, allumer automatiquement
                if (light_state.level > 0 && !light_state.on_off) {
                    light_state.on_off = true;
                    set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                        ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                        ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                        1);
                    ESP_LOGI(TAG, "Auto ON (level > 0)");
                }
                // Ne pas forcer OFF quand level=0, on laisse la lampe allumee mais noire
                else if (light_state.level == 0 && light_state.on_off) {
                    //light_state.on_off = false;
                    //set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                    //    ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                    //    ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                    //    0);
                    ESP_LOGI(TAG, "Level = 0 (lamp will stay ON but black)");
                    //reset_effect_to_none();
                }
                
                light_changed = true;
            }
        }
        else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
                light_state.color_x = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : light_state.color_x;
                ESP_LOGI(TAG, "COLOR_X -> 0x%04X", light_state.color_x);
                
                uint8_t r, g, b;
                xy_to_rgb(light_state.color_x, light_state.color_y, light_state.level, &r, &g, &b);
                effects_set_base_color(r, g, b);
                
                if (light_state.on_off && light_state.effect_id == EFFECT_NONE) {
                    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
                        led_strip_set_pixel(led_strip, i, r, g, b);
                    }
                    led_strip_refresh(led_strip);
                }
            }
            else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
                light_state.color_y = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : light_state.color_y;
                ESP_LOGI(TAG, "COLOR_Y -> 0x%04X", light_state.color_y);
                
                uint8_t r, g, b;
                xy_to_rgb(light_state.color_x, light_state.color_y, light_state.level, &r, &g, &b);
                effects_set_base_color(r, g, b);
                
                if (light_state.on_off && light_state.effect_id == EFFECT_NONE) {
                    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
                        led_strip_set_pixel(led_strip, i, r, g, b);
                    }
                    led_strip_refresh(led_strip);
                }
            }
            // Attribut personnalisé pour l'effet (ID 0xF000)
            else if (message->attribute.id == 0xF000 &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                uint8_t new_effect = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : 0;
                
                ESP_LOGI(TAG, "Effet recu via attribut 0xF000: %d", new_effect);
                
                if (new_effect < EFFECT_MAX) {
                    light_state.effect_id = new_effect;
                    attr_effect_value = light_state.effect_id;
                    set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                        ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                        0xF000,
                        attr_effect_value);
                    
                    if (light_state.effect_id == EFFECT_NONE) {
                        effects_stop();
                        ESP_LOGI(TAG, "Effet arrete");
                        light_changed = true;
                    } else {
                        if (!light_state.on_off) {
                            light_state.on_off = true;
                            set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                                ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                                ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                                1);
                            ESP_LOGI(TAG, "Auto ON (effet active)");
                        }
                        if (light_state.level == 0) {
                            light_state.level = 200;
                            last_level_non_zero = light_state.level;
                            set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                                ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
                                ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID,
                                light_state.level);
                            effects_set_brightness(light_state.level);
                            ESP_LOGI(TAG, "Auto level = 200 (effet active)");
                        }
                        uint8_t speed = get_current_effect_speed();
                        effects_start((effect_type_t)light_state.effect_id, speed);
                        ESP_LOGI(TAG, "Effet demarre: %d, vitesse: %d", light_state.effect_id, speed);
                    }
                }
            }
            // Vitesse Rainbow (0xF001)
            else if (message->attribute.id == 0xF001 &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                light_state.speed_rainbow = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : 128;
                attr_speed_rainbow = light_state.speed_rainbow;
                set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                    ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                    0xF001,
                    attr_speed_rainbow);
                ESP_LOGI(TAG, "Vitesse Rainbow: %d", light_state.speed_rainbow);
                if (light_state.effect_id == EFFECT_RAINBOW && light_state.on_off) {
                    effects_set_speed(light_state.speed_rainbow);
                }
            }
            // Vitesse Strobe (0xF002)
            else if (message->attribute.id == 0xF002 &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                light_state.speed_strobe = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : 128;
                attr_speed_strobe = light_state.speed_strobe;
                set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                    ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                    0xF002,
                    attr_speed_strobe);
                ESP_LOGI(TAG, "Vitesse Strobe: %d", light_state.speed_strobe);
                if (light_state.effect_id == EFFECT_STROBE && light_state.on_off) {
                    effects_set_speed(light_state.speed_strobe);
                }
            }
            // Vitesse Twinkle (0xF003)
            else if (message->attribute.id == 0xF003 &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                light_state.speed_twinkle = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : 128;
                attr_speed_twinkle = light_state.speed_twinkle;
                set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                    ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                    0xF003,
                    attr_speed_twinkle);
                ESP_LOGI(TAG, "Vitesse Twinkle: %d", light_state.speed_twinkle);
                if (light_state.effect_id == EFFECT_TWINKLE && light_state.on_off) {
                    effects_set_speed(light_state.speed_twinkle);
                }
            }
        }
        // Gestion du cluster Identify
        else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
                uint16_t identify_time = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : 0;
                ESP_LOGI(TAG, "Identify: %d secondes", identify_time);
                
                if (identify_time > 0) {
                    // Demarrer effet clignotement pour identification
                    effects_identify(identify_time);
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
        ESP_LOGW(TAG, "Callback Zigbee non gere (0x%x)", callback_id);
        break;
    }
    return ret;
}

// Gestionnaire des signaux Zigbee
static void bdb_start_network_steering_cb(uint8_t param)
{
    esp_err_t ret = esp_zb_bdb_start_top_level_commissioning(param);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Echec relance network steering : %s", esp_err_to_name(ret));
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
                ESP_LOGI(TAG, "Demarrage reseau (appairage)");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Redemarrage appareil");
            }
        } else {
            ESP_LOGW(TAG, "Echec redemarrage: %s", esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Connecte au reseau Zigbee - PAN:0x%04hx, Canal:%d, Addr:0x%04hx",
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Echec connexion reseau: %s", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_network_steering_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "Signal ZDO: %s (0x%x)", esp_zb_zdo_signal_to_string(sig_type), sig_type);
        break;
    }
}

// Tache Zigbee principale
static void esp_zb_task(void *pvParameters)
{
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();

    // Configuration : SEULEMENT XY (pas de HS, pas de Color Temperature)
    light_cfg.color_cfg.color_mode = 1;
    light_cfg.color_cfg.enhanced_color_mode = 1;
    light_cfg.color_cfg.color_capabilities = 0x0008;
    light_cfg.color_cfg.current_x = light_state.color_x;
    light_cfg.color_cfg.current_y = light_state.color_y;
    
    // Forcer ON/OFF a OFF et luminosite a 0 au demarrage
    light_cfg.on_off_cfg.on_off = false;
    light_cfg.level_cfg.current_level = 0;

    // ===== Endpoint 10 : LIGHT =====
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&light_cfg.basic_cfg);
    
    char manufacturer[] = {11, 'E', 'S', 'P', '3', '2', '-', 'Z', 'i', 'g', 'b', 'e'};
    char model[] = {12, 'W', 'S', '2', '8', '1', '2', '_', 'L', 'i', 'g', 'h', 't'};
    
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);
    
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(&light_cfg.identify_cfg);
    esp_zb_attribute_list_t *groups_cluster = esp_zb_groups_cluster_create(&light_cfg.groups_cfg);
    esp_zb_attribute_list_t *scenes_cluster = esp_zb_scenes_cluster_create(&light_cfg.scenes_cfg);
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&light_cfg.on_off_cfg);
    esp_zb_attribute_list_t *level_cluster = esp_zb_level_cluster_create(&light_cfg.level_cfg);
    esp_zb_attribute_list_t *color_cluster = esp_zb_color_control_cluster_create(&light_cfg.color_cfg);
    
    // Attribut personnalisé pour l'effet (ID 0xF000)
    esp_zb_cluster_add_manufacturer_attr(color_cluster, 
                                        ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                        0xF000,
                                        0x1234,
                                        ESP_ZB_ZCL_ATTR_TYPE_U8,
                                        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
                                        &attr_effect_value);

    // Attributs personnalises pour la vitesse de chaque effet
    esp_zb_cluster_add_manufacturer_attr(color_cluster, 
                                        ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                        0xF001,
                                        0x1234,
                                        ESP_ZB_ZCL_ATTR_TYPE_U8,
                                        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
                                        &attr_speed_rainbow);
    esp_zb_cluster_add_manufacturer_attr(color_cluster, 
                                        ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                        0xF002,
                                        0x1234,
                                        ESP_ZB_ZCL_ATTR_TYPE_U8,
                                        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
                                        &attr_speed_strobe);
    esp_zb_cluster_add_manufacturer_attr(color_cluster, 
                                        ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                        0xF003,
                                        0x1234,
                                        ESP_ZB_ZCL_ATTR_TYPE_U8,
                                        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
                                        &attr_speed_twinkle);

    esp_zb_cluster_list_t *cluster_list_light = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list_light, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list_light, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(cluster_list_light, groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(cluster_list_light, scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list_light, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(cluster_list_light, level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(cluster_list_light, color_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    
    esp_zb_endpoint_config_t endpoint_light_config = {
        .endpoint = HA_ESP_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list_light, endpoint_light_config);
    
    esp_zb_device_register(ep_list);

    ESP_LOGI(TAG, "Appareil enregistre: Color Dimmable Light (XY only)");

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
    
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
    
    // Initialiser le systeme d'effets
    effects_init(led_strip, LED_STRIP_LENGTH);

    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "  Zigbee WS2812 LED Strip Controller");
    ESP_LOGI(TAG, "  GPIO: %d | LEDs: %d", LED_STRIP_GPIO, LED_STRIP_LENGTH);
    ESP_LOGI(TAG, "  Demarrage: OFF (0%%)");
    ESP_LOGI(TAG, "===================================");

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 6, NULL);
}
