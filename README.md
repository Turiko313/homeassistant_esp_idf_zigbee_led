# Zigbee LED Strip Controller - ESP32-H2

Contrôleur Zigbee minimaliste pour rubans LED WS2812/WS2812B avec ESP32-H2, compatible Home Assistant et Zigbee2MQTT.

---

## ?? Démarrage rapide

### 1. Matériel requis

- **ESP32-H2** (module avec antenne Zigbee)
- **Ruban LED WS2812/WS2812B** (5V, adressable)
- **Alimentation 5V** adaptée au nombre de LEDs

**Connexions :**
- ESP32-H2 GPIO 5 ? WS2812 DIN
- ESP32-H2 GND ? WS2812 GND
- Alimentation 5V ? WS2812 5V

### 2. Versions requises

| Composant | Version |
|-----------|---------|
| **ESP-IDF** | v5.3.1 |
| **Zigbee SDK** | espressif/esp-zigbee-sdk v1.1 |
| **LED Strip** | espressif/led_strip v1.2.0 |
| **FreeRTOS** | v10.6 |

### 3. Compiler et flasher

```bash
cd esp-idf/ws2812
idf.py set-target esp32h2
idf.py -p COM5 erase-flash && idf.py clean && idf.py build && idf.py -p COM5 flash
```

?? Remplacez `COM5` par votre port COM (COM3, COM4, etc.)

---

## ?? Fonctionnalités

? **ON/OFF** - Allumer/Éteindre  
? **Brightness** - Luminosité 0-254  
? **Color XY** - Couleur CIE 1931 (picker de couleur)

---

## ?? Configuration

**Nombre de LEDs :**
```c
// esp-idf/ws2812/main/main.c - ligne 18
#define LED_STRIP_LENGTH    60  // Changez ici
```

**GPIO Data :**
```c
// esp-idf/ws2812/main/main.c - ligne 17
#define LED_STRIP_GPIO      5   // Changez ici
```

Recompilez après modification.

---

## ?? Converter Zigbee2MQTT (optionnel)

Par défaut, Zigbee2MQTT expose des fonctionnalités inutiles (Effect, Power-on behavior, Color Temperature).

**Pour une interface propre :**

1. **Dans Zigbee2MQTT** ? Settings ? External converters ? Add new converter
2. **Copiez le contenu** de `esp-idf/ws2812/WS2812_Light.mjs`
3. **Sauvegardez et redémarrez** Zigbee2MQTT
4. **Supprimez et re-pairez** l'appareil

---