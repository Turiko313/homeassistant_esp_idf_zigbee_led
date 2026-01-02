/*
 * Système d'effets pour ruban LED WS2812
 * Inspiré des effets ESPHome
 */

#include "effects.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "EFFECTS";

/* Variables globales */
static led_strip_handle_t g_led_strip = NULL;
static uint16_t g_num_leds = 0;
static effect_config_t g_effect_config = {
    .type = EFFECT_NONE,
    .speed = 50,
    .active = false
};
static rgb_color_t g_base_color = {255, 255, 255};
static TaskHandle_t g_effect_task_handle = NULL;

/* Conversion HSV vers RGB (pour effet rainbow) */
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }

    uint8_t region = h / 60;
    uint8_t remainder = (h % 60) * 255 / 60;
    
    uint8_t p = (v * (255 - s)) / 255;
    uint8_t q = (v * (255 - ((s * remainder) / 255))) / 255;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) / 255))) / 255;

    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

/* Effet 1 : Arc-en-ciel (Rainbow) */
static void effect_rainbow(uint32_t frame)
{
    for (int i = 0; i < g_num_leds; i++) {
        uint16_t hue = ((frame * 2 + i * 360 / g_num_leds) % 360);
        uint8_t r, g, b;
        hsv_to_rgb(hue, 255, 255, &r, &g, &b);
        led_strip_set_pixel(g_led_strip, i, r, g, b);
        
        // Log pour debug (seulement toutes les 50 frames)
        if (frame % 50 == 0 && i == 0) {
            ESP_LOGI(TAG, "Rainbow frame=%lu: LED0 hue=%d RGB(%d,%d,%d)", 
                     frame, hue, r, g, b);
        }
    }
    led_strip_refresh(g_led_strip);
}

/* Effet 2 : Strobe (Clignotement) */
static void effect_strobe(uint32_t frame)
{
    bool on = (frame % 2) == 0;
    
    // Log pour debug
    if (frame % 20 == 0) {
        ESP_LOGI(TAG, "Strobe frame=%lu: %s, color=(%d,%d,%d)", 
                 frame, on ? "ON" : "OFF", 
                 g_base_color.r, g_base_color.g, g_base_color.b);
    }
    
    for (int i = 0; i < g_num_leds; i++) {
        if (on) {
            led_strip_set_pixel(g_led_strip, i, g_base_color.r, g_base_color.g, g_base_color.b);
        } else {
            led_strip_set_pixel(g_led_strip, i, 0, 0, 0);
        }
    }
    led_strip_refresh(g_led_strip);
}

/* Effet 3 : Twinkle (Scintillement aléatoire) */
static void effect_twinkle(uint32_t frame)
{
    // Chaque LED a une chance aléatoire de scintiller
    for (int i = 0; i < g_num_leds; i++) {
        // Générer un nombre pseudo-aléatoire basé sur frame et LED index
        uint32_t random = (frame * 997 + i * 1009) % 100;
        
        if (random < 15) {  // 15% de chance de scintiller
            // LED brillante (couleur de base)
            led_strip_set_pixel(g_led_strip, i, g_base_color.r, g_base_color.g, g_base_color.b);
        } else if (random < 30) {
            // LED à demi-luminosité
            led_strip_set_pixel(g_led_strip, i, 
                g_base_color.r / 2, 
                g_base_color.g / 2, 
                g_base_color.b / 2);
        } else {
            // LED éteinte
            led_strip_set_pixel(g_led_strip, i, 0, 0, 0);
        }
    }
    
    // Log pour debug
    if (frame % 20 == 0) {
        ESP_LOGI(TAG, "Twinkle frame=%lu, base_color=(%d,%d,%d)", 
                 frame, g_base_color.r, g_base_color.g, g_base_color.b);
    }
    
    led_strip_refresh(g_led_strip);
}

/* Tâche FreeRTOS pour gérer les effets */
static void effect_task(void *pvParameters)
{
    uint32_t frame = 0;
    
    ESP_LOGI(TAG, "Tâche d'effets démarrée");
    
    while (1) {
        if (g_effect_config.active && g_led_strip != NULL) {
            // Log toutes les 50 frames pour debug
            if (frame % 50 == 0) {
                ESP_LOGI(TAG, "Effect running: type=%d, frame=%lu, active=%d", 
                         g_effect_config.type, frame, g_effect_config.active);
            }
            
            switch (g_effect_config.type) {
                case EFFECT_RAINBOW:
                    effect_rainbow(frame);
                    break;
                    
                case EFFECT_STROBE:
                    effect_strobe(frame);
                    break;
                    
                case EFFECT_TWINKLE:
                    effect_twinkle(frame);
                    break;
                    
                case EFFECT_NONE:
                default:
                    // Pas d'effet, rien à faire
                    break;
            }
            
            frame++;
            
            // Délai basé sur la vitesse (1-255 -> 200ms à 10ms)
            // Vitesse haute = délai court = animation rapide
            uint32_t delay_ms = 200 - ((g_effect_config.speed * 190) / 255);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            // Effet inactif, attendre un peu avant de revérifier
            frame = 0;  // Reset frame counter
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/* ============== Fonctions publiques ============== */

void effects_init(led_strip_handle_t strip, uint16_t num_leds)
{
    g_led_strip = strip;
    g_num_leds = num_leds;
    
    // Créer la tâche d'effet
    BaseType_t ret = xTaskCreate(
        effect_task,
        "effect_task",
        2048,
        NULL,
        5,  // Priorité moyenne
        &g_effect_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Échec création tâche d'effet");
        g_effect_task_handle = NULL;
    } else {
        ESP_LOGI(TAG, "Système d'effets initialisé (%d LEDs)", num_leds);
    }
}

void effects_start(effect_type_t type, uint8_t speed)
{
    if (type >= EFFECT_MAX) {
        ESP_LOGW(TAG, "Type d'effet invalide: %d", type);
        return;
    }
    
    g_effect_config.type = type;
    g_effect_config.speed = (speed == 0) ? 50 : speed; // Défaut si speed = 0
    g_effect_config.active = (type != EFFECT_NONE);
    
    const char *effect_names[] = {"None", "Rainbow", "Strobe", "Twinkle"};
    ESP_LOGI(TAG, "Effet démarré: %s (vitesse=%d, active=%d)", 
             effect_names[type], g_effect_config.speed, g_effect_config.active);
}

void effects_stop(void)
{
    g_effect_config.active = false;
    g_effect_config.type = EFFECT_NONE;
    ESP_LOGI(TAG, "Effet arrêté (active=%d)", g_effect_config.active);
}

void effects_set_base_color(uint8_t r, uint8_t g, uint8_t b)
{
    g_base_color.r = r;
    g_base_color.g = g;
    g_base_color.b = b;
    ESP_LOGI(TAG, "Couleur de base mise à jour: RGB(%d,%d,%d)", r, g, b);
}

const effect_config_t* effects_get_config(void)
{
    return &g_effect_config;
}

bool effects_is_active(void)
{
    return g_effect_config.active;
}
