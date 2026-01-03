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
                ESP_LOGI(TAG, "ON/OFF -> %s (level=%d)", new_on ? "ON" : "OFF", light_state.level);
                
                if (new_on && !light_state.on_off) {
                    // Passage de OFF a ON
                    light_state.on_off = true;
                    // Si niveau a 0, appliquer un niveau par defaut 128 (50%)
                    if (light_state.level == 0) {
                        light_state.level = 128;
                        last_level_non_zero = light_state.level;
                        set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                            ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
                            ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID,
                            light_state.level);
                        effects_set_brightness(light_state.level);
                        ESP_LOGI(TAG, "Auto level = 128 (50%) au premier ON");
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
                uint8_t new_speed = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : 0;
                
                ESP_LOGI(TAG, "Vitesse Rainbow recu: %d", new_speed);
                
                if (new_speed > 0 && new_speed <= 255) {
                    light_state.speed_rainbow = new_speed;
                    attr_speed_rainbow = light_state.speed_rainbow;
                    set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                        ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                        0xF001,
                        attr_speed_rainbow);
                    
                    if (light_state.effect_id == EFFECT_RAINBOW) {
                        effects_set_brightness(light_state.level);
                        uint8_t speed = get_current_effect_speed();
                        effects_start(EFFECT_RAINBOW, speed);
                        ESP_LOGI(TAG, "Vitesse Rainbow ajuste: %d", light_state.speed_rainbow);
                    }
                }
            }
            // Vitesse Strobe (0xF002)
            else if (message->attribute.id == 0xF002 &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                uint8_t new_speed = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : 0;
                
                ESP_LOGI(TAG, "Vitesse Strobe recu: %d", new_speed);
                
                if (new_speed > 0 && new_speed <= 255) {
                    light_state.speed_strobe = new_speed;
                    attr_speed_strobe = light_state.speed_strobe;
                    set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                        ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                        0xF002,
                        attr_speed_strobe);
                    
                    if (light_state.effect_id == EFFECT_STROBE) {
                        effects_set_brightness(light_state.level);
                        uint8_t speed = get_current_effect_speed();
                        effects_start(EFFECT_STROBE, speed);
                        ESP_LOGI(TAG, "Vitesse Strobe ajuste: %d", light_state.speed_strobe);
                    }
                }
            }
            // Vitesse Twinkle (0xF003)
            else if (message->attribute.id == 0xF003 &&
                     message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                uint8_t new_speed = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : 0;
                
                ESP_LOGI(TAG, "Vitesse Twinkle recu: %d", new_speed);
                
                if (new_speed > 0 && new_speed <= 255) {
                    light_state.speed_twinkle = new_speed;
                    attr_speed_twinkle = light_state.speed_twinkle;
                    set_zcl_attr_u8(HA_ESP_LIGHT_ENDPOINT,
                        ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                        0xF003,
                        attr_speed_twinkle);
                    
                    if (light_state.effect_id == EFFECT_TWINKLE) {
                        effects_set_brightness(light_state.level);
                        uint8_t speed = get_current_effect_speed();
                        effects_start(EFFECT_TWINKLE, speed);
                        ESP_LOGI(TAG, "Vitesse Twinkle ajuste: %d", light_state.speed_twinkle);
                    }
                }
            }
        }
    }

    return ret;
}

// Boucle principale
void app_main(void)
{
    ESP_LOGI(TAG, "Démarrage de l'application");

    // Initialisation de la NVS (pour le stockage persistant)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGE || ret == ESP_ERR_NVS_PAGE_FULL) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialisation du Zigbee
    esp_zb_error_code_t ez_ret = esp_zb_init();
    ESP_ERROR_CHECK(ez_ret);
    
    // Création du serveur Zigbee Light
    ez_ret = esp_zb_service_create(HA_ESP_LIGHT_ENDPOINT, zb_attribute_handler);
    ESP_ERROR_CHECK(ez_ret);
    
    // Gestion des effets
    effects_init(LED_STRIP_LENGTH);
    
    // Initialisation du ruban LED
    led_strip = led_strip_create(LED_STRIP_GPIO, LED_STRIP_LENGTH, LED_STRIP_TYPE_WS2812);
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
    
    // Démarrage du réseau Zigbee
    ez_ret = esp_zb_start();
    ESP_ERROR_CHECK(ez_ret);
    
    ESP_LOGI(TAG, "Application prête");
}
