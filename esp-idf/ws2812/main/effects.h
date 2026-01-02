#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdint.h>
#include <stdbool.h>
#include "led_strip.h"

/* Types d'effets disponibles */
typedef enum {
    EFFECT_NONE = 0,        // Couleur fixe (pas d'animation)
    EFFECT_RAINBOW,         // Arc-en-ciel qui défile
    EFFECT_STROBE,          // Clignotement rapide
    EFFECT_TWINKLE,         // Scintillement aléatoire (étoiles)
    EFFECT_MAX              // Nombre total d'effets
} effect_type_t;

/* Configuration d'un effet */
typedef struct {
    effect_type_t type;
    uint8_t speed;          // 1-255 (vitesse de l'animation)
    bool active;            // true si l'effet est en cours
} effect_config_t;

/* Couleur RGB */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

/**
 * @brief Initialise le système d'effets
 * 
 * @param strip Handle du ruban LED
 * @param num_leds Nombre de LEDs sur le ruban
 */
void effects_init(led_strip_handle_t strip, uint16_t num_leds);

/**
 * @brief Démarre un effet
 * 
 * @param type Type d'effet à démarrer
 * @param speed Vitesse de l'effet (1-255)
 */
void effects_start(effect_type_t type, uint8_t speed);

/**
 * @brief Arrête l'effet en cours
 */
void effects_stop(void);

/**
 * @brief Définit la couleur de base (pour EFFECT_NONE ou comme base pour certains effets)
 * 
 * @param r Rouge (0-255)
 * @param g Vert (0-255)
 * @param b Bleu (0-255)
 */
void effects_set_base_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Récupère la configuration actuelle de l'effet
 * 
 * @return Pointeur vers la configuration (lecture seule)
 */
const effect_config_t* effects_get_config(void);

/**
 * @brief Vérifie si un effet est actif
 * 
 * @return true si un effet (autre que NONE) est en cours
 */
bool effects_is_active(void);

#endif /* EFFECTS_H */
