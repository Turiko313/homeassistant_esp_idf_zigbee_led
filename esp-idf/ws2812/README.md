# WS2812 Zigbee Light Controller (ESP32-H2/C6)

Contrôle de ruban LED adressable WS2812/WS2812B via Zigbee.

## GPIO

- **Data WS2812** : GPIO8 (avec level shifter 5V recommandé)
- Nombre de LEDs : 60 (configurable dans `app_main.c`)

## Build et flash

```bash
idf.py set-target esp32h2
idf.py build
idf.py -p COM3 flash monitor
```

## Personnalisation

Dans `main/app_main.c` :
- `LED_STRIP_GPIO` : pin data
- `LED_STRIP_LENGTH` : nombre de LEDs
- Effets : ajoutez vos propres patterns dans `led_update_task()`
