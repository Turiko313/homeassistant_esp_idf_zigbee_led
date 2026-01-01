#ifndef MAIN_H
#define MAIN_H

#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"  // Ajout pour la file d'attente utilisée dans le code
#include "driver/gpio.h"

/* Configuration Zigbee */
#define INSTALLCODE_POLICY_ENABLE       false
#ifdef CONFIG_ZB_ZED
#define ESP_ZB_SLEEP_MAXIMUM_THRESHOLD_MS 86400000U
#endif         
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000

/* Endpoint pour la lumi�re */
#define HA_ESP_LIGHT_ENDPOINT           10

/* Masque des canaux primaires Zigbee */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK

/* Configuration Zigbee pour un End Device (ZED) */
#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

/* Configuration par d�faut de la radio Zigbee */
#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

/* Configuration par d�faut de l'h�te */
#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }

#endif /* MAIN_H */
