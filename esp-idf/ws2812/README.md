# WS2812 Zigbee Light Controller (ESP32-H2)

Contrôle de ruban LED adressable WS2812/WS2812B via Zigbee.

**Optimisé pour l'économie d'énergie** : End Device Zigbee, logs minimaux.

## GPIO

- **Data WS2812** : GPIO8 (utilisez un level shifter 5V)
- **Nombre de LEDs** : 60 (modifiable dans `app_main.c`)

## Build et flash

```bash
idf.py set-target esp32h2
idf.py build
idf.py -p COM3 flash monitor
```

## Personnalisation

Dans `main/app_main.c` :
- `LED_STRIP_GPIO` : pin data WS2812
- `LED_STRIP_LENGTH` : nombre de LEDs (60 par défaut)
- Ajoutez vos propres effets dans la fonction de mise à jour

## Activer Light Sleep (économie d'énergie)

Décommentez dans `app_main()` :
```c
// esp_pm_configure(&pm_config);
```

**Consommation :**
- 60 LEDs allumées : ~25 mA (ESP) + LED (variable)
- LEDs éteintes : ~5 mA
- Light Sleep : ~100 µA (quand inactif)

## Home Assistant

L'appareil apparaît comme "Color Dimmable Light" avec support HSV complet.
