# ESP-IDF Zigbee LED Controller Examples

Ce dossier contient des exemples ESP-IDF pour contrôler des rubans LED via Zigbee avec ESP32-C6/H2.

## Structure

```
esp-idf/
??? ws2812/          # Ruban adressable WS2812
??? rgb/             # Ruban RGB analogique (PWM)
??? rgbw/            # Ruban RGBW analogique (PWM)
??? cct/             # Ruban bi-température (blanc chaud/froid)
??? mono/            # Ruban mono-canal (blanc)
```

## Pré-requis

1. **ESP-IDF installé** (v5.1 ou supérieur)
2. **ESP-Zigbee-SDK** (inclus dans ESP-IDF v5.1+)

## Installation rapide ESP-IDF (Windows)

```powershell
# Télécharger l'installeur offline
# https://dl.espressif.com/dl/esp-idf/

# Ou installer manuellement
git clone -b v5.3.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
.\install.bat esp32h2,esp32c6
.\export.bat
```

## Compiler et flasher un exemple

### WS2812 (recommandé pour débuter)

```powershell
cd esp-idf/ws2812
idf.py set-target esp32h2
idf.py menuconfig   # optionnel : configurer les GPIO
idf.py build
idf.py -p COM3 flash monitor
```

### RGB / RGBW / CCT / Mono

Même procédure, changez simplement le dossier.

## Configuration GPIO

Chaque exemple a des GPIO par défaut configurables dans `main/idf_component.yml` ou directement dans le code :

| Type | GPIO par défaut (H2/C6) |
|------|-------------------------|
| WS2812 Data | GPIO8 |
| RGB R/G/B | GPIO4/5/6 |
| RGBW R/G/B/W | GPIO4/5/6/7 |
| CCT CW/WW | GPIO4/5 |
| Mono W | GPIO4 |

Modifiez dans `main/app_main.c` si votre carte utilise d'autres pins.

## Intégration Home Assistant

Après flash :
1. Mettez votre coordinateur Zigbee (ZHA/zigbee2mqtt) en mode appairage
2. Alimentez l'ESP32, il rejoint automatiquement le réseau
3. L'appareil apparaît comme une ampoule Zigbee contrôlable

## Personnalisation

Chaque exemple inclut :
- Configuration Zigbee (canal, PAN ID, clé réseau)
- Contrôle On/Off/Dim via Zigbee
- Couleurs (RGB/RGBW/CCT) si applicable
- Code commenté pour faciliter les modifications

## Support

Documentation officielle ESP-Zigbee-SDK :  
https://docs.espressif.com/projects/esp-zigbee-sdk/
