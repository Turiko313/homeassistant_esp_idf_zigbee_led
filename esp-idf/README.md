# ESP-IDF Zigbee LED Controller Examples (ESP32-H2)

Exemples ESP-IDF pour contrôler des rubans LED via Zigbee avec ESP32-H2.

**Optimisés pour l'économie d'énergie** : Light Sleep, fréquence CPU réduite, End Device Zigbee.

## Structure

```
esp-idf/
??? ws2812/          # Ruban adressable WS2812 (recommandé)
??? rgb/             # Ruban RGB analogique (PWM)
```

## Pré-requis

1. **ESP-IDF v5.1+** installé
2. **ESP32-H2** (pas de support C6 dans ces exemples)

## Installation ESP-IDF (Windows - méthode recommandée)

**Téléchargez l'installeur offline :** https://dl.espressif.com/dl/esp-idf/

1. Lancez `esp-idf-tools-setup-offline-x.x.x.exe`
2. Cochez **ESP32-H2**
3. Après installation, ouvrez **"ESP-IDF PowerShell"**

## Compiler et flasher

### WS2812 (RGB adressable)

```powershell
cd ws2812
idf.py set-target esp32h2
idf.py menuconfig   # optionnel : activer Light Sleep
idf.py build
idf.py -p COM3 flash monitor
```

### RGB PWM (analogique 12V/24V)

```powershell
cd rgb
idf.py set-target esp32h2
idf.py build
idf.py -p COM3 flash monitor
```

## Configuration GPIO

| Type | GPIO par défaut (modifiable dans app_main.c) |
|------|----------------------------------------------|
| WS2812 Data | GPIO8 |
| RGB R/G/B | GPIO4/5/6 |

## Optimisations d'économie d'énergie incluses

? **End Device Zigbee (ZED)** : consomme moins qu'un router  
? **PWM 5kHz** : équilibre entre qualité et consommation  
? **Logs minimaux** : réduit l'activité UART  

### Activer Light Sleep (optionnel)

Dans `menuconfig` :
```
Component config ? Power Management ? Enable dynamic frequency scaling
```

Ou ajoutez dans le code :
```c
#include "esp_pm.h"

esp_pm_config_t pm_config = {
    .max_freq_mhz = 96,
    .min_freq_mhz = 32,
    .light_sleep_enable = true
};
esp_pm_configure(&pm_config);
```

**Consommation ESP32-H2 estimée :**
- LEDs allumées : ~20-25 mA
- LEDs éteintes (idle) : ~5 mA
- Light Sleep : ~100 µA

## Intégration Home Assistant

1. Coordinateur Zigbee en mode appairage
2. Alimentez l'ESP32-H2
3. L'appareil apparaît automatiquement comme ampoule Zigbee
4. Contrôlez On/Off/Dim/Couleur

## Personnalisation

- **Nombre de LEDs WS2812** : `LED_STRIP_LENGTH` dans `ws2812/main/app_main.c`
- **GPIO** : Modifiez les `#define` en haut de `app_main.c`
- **Fréquence PWM** : `PWM_FREQ_HZ` dans `rgb/main/app_main.c`

## Documentation

- **ESP-Zigbee-SDK** : https://docs.espressif.com/projects/esp-zigbee-sdk/
- **ESP32-H2 Power** : https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/system/power_management.html
