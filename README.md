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
| **ESP-IDF** | >= 5.0 |
| **esp-zboss-lib** | 1.6.4 |
| **esp-zigbee-lib** | 1.6.4 |
| **led_strip** | ^2.5.0 |

### 3. Compiler et flasher

```bash
cd esp-idf/ws2812
idf.py set-target esp32h2
idf.py -p COM5 erase-flash && idf.py clean && idf.py build && idf.py -p COM5 flash
```

?? Remplacez `COM5` par votre port COM (COM3, COM4, etc.)

---

## ? Fonctionnalités

| Fonctionnalité | Description |
|----------------|-------------|
| ? **ON/OFF** | Allumer/éteindre |
| ? **Brightness** | Luminosité 0-254 |
| ? **Color XY** | Couleur CIE 1931 (picker de couleur) |
| ? **Effets** | Rainbow, Strobe, Twinkle |

### Effets disponibles

| ID | Effet | Description |
|----|-------|-------------|
| 0 | None | Couleur fixe |
| 1 | Rainbow | Arc-en-ciel défilant |
| 2 | Strobe | Clignotement rapide |
| 3 | Twinkle | Scintillement aléatoire (étoiles) |

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

## ?? Converter Zigbee2MQTT

Pour activer les effets dans Zigbee2MQTT :

1. **Dans Zigbee2MQTT** ? Settings ? External converters ? Add new converter
2. **Copiez le contenu** de `WS2812_Light.mjs` (à la racine du projet)
3. **Sauvegardez et redémarrez** Zigbee2MQTT
4. **Supprimez et re-pairez** l'appareil

Le converter expose :
- Lumière avec luminosité et couleur XY
- Sélecteur d'effet (none, rainbow, strobe, twinkle)

---

## ?? Structure du projet

```
??? WS2812_Light.mjs          # Converter Zigbee2MQTT
??? esp-idf/ws2812/
?   ??? main/
?   ?   ??? main.c            # Code principal
?   ?   ??? main.h            # Configuration Zigbee
?   ?   ??? effects.c         # Système d'effets LED
?   ?   ??? effects.h         # Définitions des effets
?   ??? CMakeLists.txt
??? README.md
```

---

## ?? Licence

MIT License