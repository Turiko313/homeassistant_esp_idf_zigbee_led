#ifndef MAIN_H
#define MAIN_H

#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "freertos/queue.h"        // ← À supprimer si tu ne l'utilises PAS dans main.c
// #include "driver/gpio.h"           // ← Pas nécessaire ici (tu n'utilises pas de GPIO dans ce header)

/* ====================== Configuration Zigbee ====================== */

// Politique d'install code (sécurité pour l'appairage)
#define INSTALLCODE_POLICY_ENABLE       false   // false = appairage classique (plus simple pour HA/Z2M)

// Timeout pour End Device (ZED) - évite d'être exclu du réseau si endormi trop longtemps
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    // ms - poll rate vers parent

// Endpoint Zigbee pour la lumière (doit être unique si plusieurs endpoints)
#define HA_ESP_LIGHT_ENDPOINT           10

// Canaux autorisés pour le réseau Zigbee (11-26)
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK

/* ================= Configuration rôle ZED (End Device) ================ */
#define ESP_ZB_ZED_CONFIG()                                             \
    {                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                           \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,               \
        .nwk_cfg.zed_cfg = {                                            \
            .ed_timeout = ED_AGING_TIMEOUT,                             \
            .keep_alive = ED_KEEP_ALIVE,                                \
        },                                                              \
    }

/* ================= Configuration radio et hôte par défaut ============ */
#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }
    
#endif /* MAIN_H */