/*
 * ESP32-H2 Zigbee WS2812 LED Strip Controller with Effects
 * GPIO5 - 60 LEDs - Effets style ESPHome
 */

#include "main.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_random.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "led_strip.h"

// Configuration
#define LED_STRIP_GPIO      5
#define LED_STRIP_LENGTH    60
#define HA_ESP_LIGHT_ENDPOINT  10

static const char *TAG = "ZIGBEE_WS2812";
static led_strip_handle_t led_strip = NULL;

// Effets disponibles (comme ESPHome)
typedef enum {
    EFFECT_NONE = 0,
    EFFECT_RAINBOW,
    EFFECT_STROBE,
    EFFECT_FLICKER,
    EFFECT_PULSE,
    EFFECT_SCAN,
    EFFECT_TWINKLE,
    EFFECT_FIREWORKS,
    EFFECT_MAX
} led_effect_t;

static const char *effect_names[] = {
    "Aucun",
    "Arc-en-ciel",
    "Stroboscope",
    "Scintillement",
    "Pulsation",
    "Scanner",
    "Etoiles",
    "Feu d'artifice"
};

// État de la lumière
typedef struct {
    bool on_off;
    uint8_t level;
    uint8_t hue;
    uint8_t saturation;
    led_effect_t current_effect;
    uint32_t effect_counter;
} light_state_t;

static light_state_t light_state = {
    .on_off = false,
    .level = 128,
    .hue = 0,          // Hue à 0 (rouge) - sera défini par Home Assistant
    .saturation = 0,   // Saturation à 0 = BLANC au lieu de couleur
    .current_effect = EFFECT_NONE,
    .effect_counter = 0
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

// Effet: Arc-en-ciel (Rainbow)
static void effect_rainbow(uint32_t counter)
{
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        uint8_t hue = (counter + (i * 256 / LED_STRIP_LENGTH)) & 0xFF;
        uint8_t r, g, b;
        hsv_to_rgb(hue, 254, light_state.level, &r, &g, &b);
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
}

// Effet: Stroboscope
static void effect_strobe(uint32_t counter)
{
    uint8_t r, g, b;
    bool on = (counter % 10) < 5;
    
    if (on) {
        hsv_to_rgb(light_state.hue, light_state.saturation, light_state.level, &r, &g, &b);
    } else {
        r = g = b = 0;
    }
    
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
}

// Effet: Scintillement (Flicker)
static void effect_flicker(uint32_t counter)
{
    uint8_t r, g, b;
    uint8_t brightness = light_state.level - (esp_random() % 50);
    hsv_to_rgb(light_state.hue, light_state.saturation, brightness, &r, &g, &b);
    
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
}

// Effet: Pulsation (Pulse)
static void effect_pulse(uint32_t counter)
{
    uint8_t r, g, b;
    float brightness = (sinf(counter * 0.1f) + 1.0f) / 2.0f;
    uint8_t level = (uint8_t)(brightness * light_state.level);
    hsv_to_rgb(light_state.hue, light_state.saturation, level, &r, &g, &b);
    
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
}

// Effet: Scanner (Knight Rider)
static void effect_scan(uint32_t counter)
{
    uint8_t r, g, b;
    hsv_to_rgb(light_state.hue, light_state.saturation, light_state.level, &r, &g, &b);
    
    int pos = counter % (LED_STRIP_LENGTH * 2);
    if (pos >= LED_STRIP_LENGTH) {
        pos = LED_STRIP_LENGTH * 2 - pos - 1;
    }
    
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        if (abs(i - pos) < 3) {
            led_strip_set_pixel(led_strip, i, r, g, b);
        } else {
            led_strip_set_pixel(led_strip, i, 0, 0, 0);
        }
    }
}

// Effet: Étoiles scintillantes (Twinkle)
static void effect_twinkle(uint32_t counter)
{
    if (counter % 5 == 0) {
        for (int i = 0; i < LED_STRIP_LENGTH; i++) {
            if (esp_random() % 10 < 2) {
                uint8_t r, g, b;
                hsv_to_rgb(light_state.hue, light_state.saturation, light_state.level, &r, &g, &b);
                led_strip_set_pixel(led_strip, i, r, g, b);
            } else {
                led_strip_set_pixel(led_strip, i, 0, 0, 0);
            }
        }
    }
}

// Effet: Feu d'artifice
static void effect_fireworks(uint32_t counter)
{
    static int burst_pos = -1;
    static int burst_size = 0;
    
    if (counter % 30 == 0) {
        burst_pos = esp_random() % LED_STRIP_LENGTH;
        burst_size = 0;
    }
    
    if (burst_pos >= 0) {
        burst_size++;
        if (burst_size > 10) {
            burst_pos = -1;
            led_strip_clear(led_strip);
            return;
        }
        
        for (int i = 0; i < LED_STRIP_LENGTH; i++) {
            if (abs(i - burst_pos) <= burst_size) {
                uint8_t r, g, b;
                uint8_t brightness = light_state.level * (10 - burst_size) / 10;
                hsv_to_rgb((light_state.hue + i * 10) & 0xFF, light_state.saturation, brightness, &r, &g, &b);
                led_strip_set_pixel(led_strip, i, r, g, b);
            }
        }
    }
}

// Mise à jour du ruban LED avec effets
static void update_led_strip(void)
{
    if (!light_state.on_off) {
        ESP_LOGI(TAG, "update_led_strip: Lumière OFF");
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
        return;
    }
    
    ESP_LOGI(TAG, "update_led_strip: ON - Effet=%d, Hue=%d, Sat=%d, Level=%d", 
             light_state.current_effect, light_state.hue, light_state.saturation, light_state.level);
    
    switch (light_state.current_effect) {
        case EFFECT_RAINBOW:
            effect_rainbow(light_state.effect_counter);
            break;
        case EFFECT_STROBE:
            effect_strobe(light_state.effect_counter);
            break;
        case EFFECT_FLICKER:
            effect_flicker(light_state.effect_counter);
            break;
        case EFFECT_PULSE:
            effect_pulse(light_state.effect_counter);
            break;
        case EFFECT_SCAN:
            effect_scan(light_state.effect_counter);
            break;
        case EFFECT_TWINKLE:
            effect_twinkle(light_state.effect_counter);
            break;
        case EFFECT_FIREWORKS:
            effect_fireworks(light_state.effect_counter);
            break;
        default:
        case EFFECT_NONE: {
            // Couleur statique
            uint8_t r, g, b;
            hsv_to_rgb(light_state.hue, light_state.saturation, light_state.level, &r, &g, &b);
            for (int i = 0; i < LED_STRIP_LENGTH; i++) {
                led_strip_set_pixel(led_strip, i, r, g, b);
            }
            break;
        }
    }
    
    led_strip_refresh(led_strip);
    light_state.effect_counter++;
}

// Tâche d'animation des effets
static void effect_task(void *pvParameters)
{
    while (1) {
        if (light_state.on_off && light_state.current_effect != EFFECT_NONE) {
            update_led_strip();
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // 20 FPS
    }
}

// Gestionnaire des attributs Zigbee
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool light_changed = false;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Message vide");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                        "Message reçu : statut d'erreur (%d)", message->info.status);

    ESP_LOGI(TAG, "Message reçu : endpoint(%d), cluster(0x%x), attribut(0x%x)",
             message->info.dst_endpoint, message->info.cluster, message->attribute.id);

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

    // Mettre à jour les LEDs immédiatement pour tous les changements
    update_led_strip();

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

    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
    
    // Configurer le mode couleur pour support Hue/Saturation
    light_cfg.color_cfg.color_mode = 0;  // 0 = Hue/Saturation mode
    light_cfg.color_cfg.enhanced_color_mode = 0;
    light_cfg.color_cfg.color_capabilities = 0x0001;  // Bit 0 = Hue/Saturation supported
    
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&light_cfg.basic_cfg);
    
    char manufacturer[] = {11, 'E', 'S', 'P', '3', '2', '-', 'Z', 'i', 'g', 'b', 'e'};
    char model[] = {15, 'W', 'S', '2', '8', '1', '2', '_', 'E', 'f', 'f', 'e', 'c', 't', 's'};
    
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);
    
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(&light_cfg.identify_cfg);
    esp_zb_attribute_list_t *groups_cluster = esp_zb_groups_cluster_create(&light_cfg.groups_cfg);
    esp_zb_attribute_list_t *scenes_cluster = esp_zb_scenes_cluster_create(&light_cfg.scenes_cfg);
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&light_cfg.on_off_cfg);
    esp_zb_attribute_list_t *level_cluster = esp_zb_level_cluster_create(&light_cfg.level_cfg);
    esp_zb_attribute_list_t *color_cluster = esp_zb_color_control_cluster_create(&light_cfg.color_cfg);

    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(cluster_list, groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(cluster_list, scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(cluster_list, level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(cluster_list, color_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);
    
    // Forcer les valeurs initiales des attributs couleur pour que Home Assistant les détecte
    uint8_t color_mode = 0x00;  // Hue/Saturation mode
    uint16_t color_capabilities = 0x0001;  // Hue/Saturation capable
    uint8_t hue = 0;
    uint8_t saturation = 0;
    
    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT, 
                                  ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                  ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID,
                                  &color_mode,
                                  false);
    
    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT,
                                  ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                  ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_CAPABILITIES_ID,
                                  &color_capabilities,
                                  false);
    
    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT,
                                  ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                  ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID,
                                  &hue,
                                  false);
    
    esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT,
                                  ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                  ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID,
                                  &saturation,
                                  false);
    
    ESP_LOGI(TAG, "Attributs couleur ZCL initialisés: mode=%d, capabilities=0x%04x", color_mode, color_capabilities);

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

    // Configuration WS2812 sur GPIO5
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

    // TEST: Vérifier que le ruban LED fonctionne
    ESP_LOGI(TAG, "TEST LED STRIP - ROUGE (RGB) pendant 2 secondes");
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        led_strip_set_pixel(led_strip, i, 50, 0, 0);  // R, G, B
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "TEST LED STRIP - ROUGE (GRB) pendant 2 secondes");
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        led_strip_set_pixel(led_strip, i, 0, 50, 0);  // G, R, B (= ROUGE en GRB)
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "TEST LED STRIP - Vert (RGB) pendant 2 secondes");
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        led_strip_set_pixel(led_strip, i, 0, 50, 0);  // R, G, B
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "TEST LED STRIP - Bleu (RGB) pendant 2 secondes");
    for (int i = 0; i < LED_STRIP_LENGTH; i++) {
        led_strip_set_pixel(led_strip, i, 0, 0, 50);  // R, G, B
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(2000));

    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);

    ESP_LOGI(TAG, "Zigbee WS2812 Light avec effets - GPIO: %d, LEDs: %d", LED_STRIP_GPIO, LED_STRIP_LENGTH);
    ESP_LOGI(TAG, "Démarrage: Lumière OFF - Effet actif: %s", effect_names[light_state.current_effect]);
    ESP_LOGI(TAG, "Effets disponibles: Rainbow, Strobe, Flicker, Pulse, Scan, Twinkle, Fireworks");
    
    // TEST MANUEL: Simuler un changement de couleur depuis Zigbee
    ESP_LOGI(TAG, "TEST MANUEL - Allumage en ROUGE pendant 3 secondes");
    light_state.on_off = true;
    light_state.hue = 0;        // Rouge
    light_state.saturation = 254;  // Couleur saturée
    light_state.level = 128;
    update_led_strip();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "TEST MANUEL - Changement en VERT pendant 3 secondes");
    light_state.hue = 85;       // Vert (85/254 * 360 ? 120°)
    light_state.saturation = 254;
    update_led_strip();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "TEST MANUEL - Changement en BLEU pendant 3 secondes");
    light_state.hue = 170;      // Bleu (170/254 * 360 ? 240°)
    light_state.saturation = 254;
    update_led_strip();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "TEST MANUEL - Extinction");
    light_state.on_off = false;
    update_led_strip();
    
    // Démarrer avec lumière ÉTEINTE (contrôle via Home Assistant)
    // Pour changer l'effet par défaut, modifie: light_state.current_effect = EFFECT_RAINBOW;

    // Création de la tâche d'animation
    xTaskCreate(effect_task, "LED_Effects", 2048, NULL, 5, NULL);
    
    // Création de la tâche Zigbee
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 6, NULL);
}
