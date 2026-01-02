/*
 * Systeme d'effets pour ruban LED WS2812
 * Inspire des effets ESPHome
 */

#include "effects.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include <math.h>
#include <stdlib.h>

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
static uint8_t g_brightness = 255;  // Luminosite globale (0-255)
static TaskHandle_t g_effect_task_handle = NULL;

// Buffer pour stocker l'etat de chaque LED (pour twinkle)
static uint8_t *g_led_brightness = NULL;

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

/* Effet 1 : Arc-en-ciel (Rainbow) - Degrade sur tout le ruban */
static void effect_rainbow(uint32_t frame)
{
    for (int i = 0; i < g_num_leds; i++) {
        // Chaque LED a une teinte differente, le tout defile avec le temps
        uint16_t hue = ((frame * 3) + (i * 360 / g_num_leds)) % 360;
        uint8_t r, g, b;
        hsv_to_rgb(hue, 255, 255, &r, &g, &b);
        
        // Appliquer la luminosite globale
        r = (r * g_brightness) / 255;
        g = (g * g_brightness) / 255;
        b = (b * g_brightness) / 255;
        
        led_strip_set_pixel(g_led_strip, i, r, g, b);
    }
    led_strip_refresh(g_led_strip);
    
    // Log pour debug (seulement toutes les 100 frames)
    if (frame % 100 == 0) {
        ESP_LOGI(TAG, "Rainbow frame=%lu, brightness=%d", frame, g_brightness);
    }
}

/* Effet 2 : Strobe (Clignotement) */
static void effect_strobe(uint32_t frame)
{
    bool on = (frame % 2) == 0;
    
    for (int i = 0; i < g_num_leds; i++) {
        if (on) {
            // Appliquer la luminosite globale
            uint8_t r = (g_base_color.r * g_brightness) / 255;
            uint8_t g = (g_base_color.g * g_brightness) / 255;
            uint8_t b = (g_base_color.b * g_brightness) / 255;
            led_strip_set_pixel(g_led_strip, i, r, g, b);
        } else {
            led_strip_set_pixel(g_led_strip, i, 0, 0, 0);
        }
    }
    led_strip_refresh(g_led_strip);
}

/* Effet 3 : Twinkle (Scintillement etoiles) */
static void effect_twinkle(uint32_t frame)
{
    if (g_led_brightness == NULL) {
        return;
    }
    
    for (int i = 0; i < g_num_leds; i++) {
        uint32_t rand_val = esp_random();
        
        // Chaque LED a une chance de changer d'etat (ON ou OFF)
        if ((rand_val % 100) < 8) {
            // 8% de chance de changer d'etat
            if (g_led_brightness[i] == 0) {
                // Allumer avec luminosite aleatoire (180-255)
                g_led_brightness[i] = 180 + (rand_val % 76);
            } else {
                // Eteindre immediatement
                g_led_brightness[i] = 0;
            }
        }
        
        // Appliquer la luminosite de l'etoile ET la luminosite globale
        uint8_t star_brightness = g_led_brightness[i];
        uint8_t total_brightness = (star_brightness * g_brightness) / 255;
        
        uint8_t r = (g_base_color.r * total_brightness) / 255;
        uint8_t g = (g_base_color.g * total_brightness) / 255;
        uint8_t b = (g_base_color.b * total_brightness) / 255;
        
        led_strip_set_pixel(g_led_strip, i, r, g, b);
    }
    
    led_strip_refresh(g_led_strip);
}

/* Tache FreeRTOS pour gerer les effets */
static void effect_task(void *pvParameters)
{
    uint32_t frame = 0;
    
    ESP_LOGI(TAG, "Tache d'effets demarree");
    
    while (1) {
        if (g_effect_config.active && g_led_strip != NULL) {
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
                    break;
            }
            
            frame++;
            
            // Delai base sur la vitesse
            uint32_t delay_ms = 200 - ((g_effect_config.speed * 190) / 255);
            if (delay_ms < 20) delay_ms = 20;  // Minimum 20ms
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            frame = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/* ============== Fonctions publiques ============== */

void effects_init(led_strip_handle_t strip, uint16_t num_leds)
{
    g_led_strip = strip;
    g_num_leds = num_leds;
    
    // Allouer le buffer pour twinkle
    if (g_led_brightness != NULL) {
        free(g_led_brightness);
    }
    g_led_brightness = (uint8_t *)calloc(num_leds, sizeof(uint8_t));
    if (g_led_brightness == NULL) {
        ESP_LOGE(TAG, "Echec allocation buffer LED");
    }
    
    // Creer la tache d'effet
    BaseType_t ret = xTaskCreate(
        effect_task,
        "effect_task",
        2048,
        NULL,
        5,
        &g_effect_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Echec creation tache d'effet");
        g_effect_task_handle = NULL;
    } else {
        ESP_LOGI(TAG, "Systeme d'effets initialise (%d LEDs)", num_leds);
    }
}

void effects_start(effect_type_t type, uint8_t speed)
{
    if (type >= EFFECT_MAX) {
        ESP_LOGW(TAG, "Type d'effet invalide: %d", type);
        return;
    }
    
    // Reset le buffer twinkle quand on demarre un effet
    if (g_led_brightness != NULL) {
        memset(g_led_brightness, 0, g_num_leds * sizeof(uint8_t));
    }
    
    g_effect_config.type = type;
    g_effect_config.speed = (speed == 0) ? 50 : speed;
    g_effect_config.active = (type != EFFECT_NONE);
    
    const char *effect_names[] = {"None", "Rainbow", "Strobe", "Twinkle"};
    ESP_LOGI(TAG, "Effet demarre: %s (vitesse=%d)", effect_names[type], g_effect_config.speed);
}

void effects_stop(void)
{
    g_effect_config.active = false;
    g_effect_config.type = EFFECT_NONE;
    ESP_LOGI(TAG, "Effet arrete");
}

void effects_set_base_color(uint8_t r, uint8_t g, uint8_t b)
{
    g_base_color.r = r;
    g_base_color.g = g;
    g_base_color.b = b;
}

void effects_set_brightness(uint8_t brightness)
{
    g_brightness = brightness;
    ESP_LOGI(TAG, "Luminosite effet: %d", brightness);
}

void effects_set_speed(uint8_t speed)
{
    g_effect_config.speed = (speed == 0) ? 50 : speed;
    ESP_LOGI(TAG, "Vitesse effet: %d", g_effect_config.speed);
}

void effects_identify(uint16_t duration_sec)
{
    ESP_LOGI(TAG, "Identify: clignotement pendant %d secondes", duration_sec);
    
    // Sauvegarder l'etat actuel
    effect_type_t saved_type = g_effect_config.type;
    bool saved_active = g_effect_config.active;
    
    // Clignoter pendant la duree specifiee
    for (uint16_t i = 0; i < duration_sec * 4; i++) {  // 4 clignotements par seconde
        bool on = (i % 2) == 0;
        
        for (int j = 0; j < g_num_leds; j++) {
            if (on) {
                led_strip_set_pixel(g_led_strip, j, 255, 255, 255);  // Blanc
            } else {
                led_strip_set_pixel(g_led_strip, j, 0, 0, 0);
            }
        }
        led_strip_refresh(g_led_strip);
        vTaskDelay(pdMS_TO_TICKS(250));  // 250ms par etat
    }
    
    // Eteindre a la fin
    led_strip_clear(g_led_strip);
    led_strip_refresh(g_led_strip);
    
    // Restaurer l'effet precedent si actif
    if (saved_active && saved_type != EFFECT_NONE) {
        g_effect_config.type = saved_type;
        g_effect_config.active = true;
    }
    
    ESP_LOGI(TAG, "Identify termine");
}

const effect_config_t* effects_get_config(void)
{
    return &g_effect_config;
}

bool effects_is_active(void)
{
    return g_effect_config.active;
}
