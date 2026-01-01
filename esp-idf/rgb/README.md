# RGB PWM Zigbee Light Controller (ESP32-H2/C6)

Contrôle de ruban LED RGB analogique (12V/24V) via PWM et Zigbee.

## GPIO (modifiable dans app_main.c)

- **Red** : GPIO4
- **Green** : GPIO5
- **Blue** : GPIO6

Utilise 3 MOSFETs canal N ou driver PWM.

## Build et flash

```bash
idf.py set-target esp32h2
idf.py build
idf.py -p COM3 flash monitor
```
