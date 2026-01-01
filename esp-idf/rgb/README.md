# RGB PWM Zigbee Light Controller (ESP32-H2)

Contrôle de ruban LED RGB analogique (12V/24V) via PWM et Zigbee.

## GPIO (modifiables dans app_main.c)

- **Red** : GPIO4
- **Green** : GPIO5
- **Blue** : GPIO6

Utilisez 3 MOSFETs canal N (IRLZ44N recommandé) ou un driver PWM dédié.

## Build et flash

```bash
idf.py set-target esp32h2
idf.py build
idf.py -p COM3 flash monitor
```

## Économie d'énergie

- **Fréquence PWM** : 5 kHz par défaut (réduisez à 1 kHz si pas de scintillement visible)
- **End Device Zigbee** : consomme moins qu'un router

Dans `app_main.c`, modifiez :
```c
#define PWM_FREQ_HZ  1000  // au lieu de 5000
```

**Consommation ESP32-H2 :**
- RGB allumé : ~20 mA (ESP) + LEDs via MOSFETs
- RGB éteint : ~5 mA
