# WS2812 Zigbee Controller

Contrôleur Zigbee minimaliste pour ruban LED WS2812/WS2812B avec ESP32-H2.

---

## ?? Fonctionnalités

? **ON/OFF** - Allumer/Éteindre  
? **Brightness** - Luminosité 0-254  
? **Color XY** - Couleur CIE 1931 (picker de couleur)

**Au redémarrage :** Toujours **OFF à 0%** (pas de mémoire)

---

## ?? Installation

### 1. Matériel requis

- **ESP32-H2** (avec support Zigbee)
- **Ruban LED WS2812/WS2812B**
- **Alimentation 5V** adaptée au nombre de LEDs

### 2. Connexions

| ESP32-H2 | WS2812 |
|----------|--------|
| GPIO 5   | DIN    |
| GND      | GND    |
| 5V       | 5V     |

### 3. Compilation et flash

```bash
cd esp-idf/ws2812
idf.py set-target esp32h2
idf.py build
idf.py -p COM3 flash monitor
```

### 4. Pairing Zigbee

**Zigbee2MQTT :**
1. Cliquez sur **"Permit join"**
2. Alimentez l'ESP32-H2
3. L'appareil est détecté automatiquement

**Home Assistant ZHA :**
1. Configuration ? Intégrations ? Zigbee Home Automation
2. Ajouter un appareil
3. Alimentez l'ESP32-H2

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

Après modification, recompilez avec `idf.py build`.

---

## ?? Utilisation

### Home Assistant

L'entité `light.ws2812_led_strip` est créée automatiquement.

**Exemple automation :**
```yaml
automation:
  - alias: "Allumer au coucher du soleil"
    trigger:
      - platform: sun
        event: sunset
    action:
      - service: light.turn_on
        target:
          entity_id: light.ws2812_led_strip
        data:
          brightness: 200
          xy_color: [0.5, 0.4]
```

### MQTT (Zigbee2MQTT)

**Allumer en rouge :**
```bash
mosquitto_pub -t "zigbee2mqtt/WS2812_Light/set" \
  -m '{"state":"ON","brightness":254,"color":{"x":0.7,"y":0.3}}'
```

**Régler la luminosité :**
```bash
mosquitto_pub -t "zigbee2mqtt/WS2812_Light/set" -m '{"brightness":150}'
```

**Éteindre :**
```bash
mosquitto_pub -t "zigbee2mqtt/WS2812_Light/set" -m '{"state":"OFF"}'
```

---

## ?? Converter Zigbee2MQTT (optionnel mais recommandé)

Par défaut, Zigbee2MQTT expose des fonctionnalités inutiles comme **Effect**, **Power-on behavior**, et **Color Temperature**.

### Installation via l'interface Zigbee2MQTT

**Méthode simple (recommandée) :**

1. **Ouvrez Zigbee2MQTT** ? Settings ? External converters
2. **Cliquez sur** "Add new converter"
3. **Copiez-collez** le contenu du fichier `WS2812_Light.mjs` (dans ce dossier)
4. **Sauvegardez**
5. **Redémarrez** Zigbee2MQTT
6. **Supprimez et re-pairez** l'appareil WS2812

### Résultat

**Avant converter :**
- State, Brightness, Color XY
- ? Effect (inutile)
- ? Power-on behavior (non implémenté)
- ? Color Temperature (non applicable)
- ? Color Temp Startup

**Après converter :**
- State, Brightness, Color XY
- ? Interface propre sans fonctionnalités inutiles

**Guide complet :** [SOLUTION_EFFET_POWERONBEHAVIOR.md](SOLUTION_EFFET_POWERONBEHAVIOR.md)

---

## ?? Couleurs (format XY)

Le contrôleur utilise le format **CIE 1931 XY** pour les couleurs.

**Exemples de couleurs :**

| Couleur | X | Y |
|---------|---|---|
| Rouge | 0.7006 | 0.2993 |
| Vert | 0.1724 | 0.7468 |
| Bleu | 0.1355 | 0.0399 |
| Blanc | 0.3127 | 0.3290 |
| Jaune | 0.4432 | 0.5154 |
| Cyan | 0.1547 | 0.2369 |
| Magenta | 0.3209 | 0.1542 |

**Conversion RGB ? XY :** Utilisez le color picker dans Home Assistant ou Zigbee2MQTT.

---

## ?? Dépannage

### L'appareil ne s'appaire pas

1. Vérifiez que le coordinateur est en mode pairing
2. Reset l'ESP32-H2 (débrancher/rebrancher)
3. Vérifiez les logs : `idf.py monitor`

### Les couleurs sont incorrectes

**Test rouge pur :**
```json
{"state":"ON","color":{"x":0.7006,"y":0.2993}}
```

Si le rouge apparaît vert ou vice-versa, vérifiez `LED_PIXEL_FORMAT_GRB` dans le code (normalement déjà correct).

### "Effect" et "Power-on behavior" apparaissent

C'est normal si vous n'avez pas installé le converter externe.  
Voir section **"Converter Zigbee2MQTT"** ci-dessus.

### Les LEDs ne s'allument pas

1. Vérifiez l'alimentation 5V
2. Vérifiez les connexions (GPIO 5 ? DIN)
3. Vérifiez `LED_STRIP_LENGTH` dans le code
4. Testez avec : `{"state":"ON","brightness":254}`

---

## ?? Structure du projet

```
homeassistant_esp_idf_zigbee_led/
??? esp-idf/
?   ??? ws2812/
?       ??? main/
?       ?   ??? main.c                    # Code principal
?       ?   ??? main.h                    # Définitions
?       ?   ??? CMakeLists.txt
?       ?   ??? idf_component.yml
?       ??? CMakeLists.txt
?       ??? README.md                     # Ce fichier
?       ??? INSTALLATION_CONVERTER.md     # Guide converter (express)
?       ??? SOLUTION_EFFET_POWERONBEHAVIOR.md  # Guide converter (complet)
?
??? WS2812_Light.mjs                      # Converter Zigbee2MQTT
??? README.md                             # README principal
```

---

## ?? Documentation

| Fichier | Description |
|---------|-------------|
| **[README.md](esp-idf/ws2812/README.md)** | Ce fichier - Guide complet |
| **[INSTALLATION_CONVERTER.md](esp-idf/ws2812/INSTALLATION_CONVERTER.md)** | Installation express du converter |
| **[SOLUTION_EFFET_POWERONBEHAVIOR.md](esp-idf/ws2812/SOLUTION_EFFET_POWERONBEHAVIOR.md)** | Guide complet + troubleshooting converter |

---

## ?? Liens utiles

- **ESP-IDF Zigbee SDK** : https://docs.espressif.com/projects/esp-zigbee-sdk/
- **Zigbee2MQTT** : https://www.zigbee2mqtt.io/
- **Home Assistant ZHA** : https://www.home-assistant.io/integrations/zha/
- **WS2812B Datasheet** : https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf
- **External converters Z2M** : https://www.zigbee2mqtt.io/advanced/support-new-devices/01_support_new_devices.html

---

## ? Checklist de validation

Après installation :

- [ ] L'ESP32-H2 compile et flashe sans erreur
- [ ] L'appareil s'appaire dans Zigbee2MQTT/ZHA
- [ ] La lumière apparaît dans Home Assistant
- [ ] ON/OFF fonctionne
- [ ] Brightness fonctionne
- [ ] Color picker XY fonctionne
- [ ] Les couleurs sont correctes (rouge = rouge, etc.)
- [ ] (Optionnel) Converter externe installé et fonctionnel

---

**Version :** Minimale et simplifiée  
**Hardware :** ESP32-H2 + WS2812/WS2812B  
**Firmware :** ESP-IDF v5.3+  
**Zigbee :** End Device (ZED)

?? **Projet prêt pour la production !**
