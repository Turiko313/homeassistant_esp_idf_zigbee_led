# Zigbee LED Strip Controller - ESP32-H2 + Home Assistant

Contrôleur Zigbee minimaliste pour rubans LED WS2812/WS2812B avec ESP32-H2, compatible Home Assistant et Zigbee2MQTT.

**?? ESP32-H2 uniquement** : Zigbee natif, consommation minimale, idéal pour la production.

---

## ?? Démarrage rapide

### 1. Matériel requis

- **ESP32-H2** (module avec antenne Zigbee)
- **Ruban LED WS2812/WS2812B** (5V, adressable)
- **Alimentation 5V** adaptée au nombre de LEDs
- **Câbles de connexion**

**Connexions :**
- ESP32-H2 GPIO 5 ? WS2812 DIN
- ESP32-H2 GND ? WS2812 GND
- Alimentation 5V ? WS2812 5V

### 2. Installation ESP-IDF

**Windows (recommandé : installeur offline) :**
```powershell
# Télécharger : https://dl.espressif.com/dl/esp-idf/
# Lancer l'installeur, cocher ESP32-H2
# Ouvrir "ESP-IDF PowerShell"
```

**Linux / macOS :**
```bash
git clone -b v5.3.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32h2
. ./export.sh
```

### 3. Compiler et flasher

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

## ?? Fonctionnalités

? **ON/OFF** - Allumer/Éteindre  
? **Brightness** - Luminosité 0-254  
? **Color XY** - Couleur CIE 1931 (picker de couleur)

**Au redémarrage :** Toujours OFF à 0% (pas de mémoire)

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

---

## ?? Converter Zigbee2MQTT (optionnel)

Par défaut, Zigbee2MQTT expose des fonctionnalités inutiles (Effect, Power-on behavior, Color Temperature).

**Pour une interface propre :**

1. **Dans Zigbee2MQTT** : Settings ? External converters ? Add new converter
2. **Copiez le contenu** de `esp-idf/ws2812/WS2812_Light.mjs`
3. **Sauvegardez et redémarrez** Zigbee2MQTT
4. **Supprimez et re-pairez** l'appareil

**Résultat :** Interface minimaliste avec seulement State, Brightness et Color XY.

**Documentation complète :** [esp-idf/ws2812/](esp-idf/ws2812/)

---

## ?? Structure du projet

```
homeassistant_esp_idf_zigbee_led/
??? esp-idf/ws2812/               # Projet WS2812
?   ??? main/                     # Code source
?   ??? README.md                 # Documentation complète
?   ??? WS2812_Light.mjs          # Converter Zigbee2MQTT
?   ??? SOLUTION_EFFET_POWERONBEHAVIOR.md  # Guide converter détaillé
??? README.md                     # Ce fichier
```

---

## ?? Dépannage

### Port COM non détecté
Installez les pilotes USB-to-Serial :
- **CP210x** : https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- **CH340** : http://www.wch-ic.com/downloads/CH341SER_EXE.html

### Erreur "idf.py: command not found"
- **Windows** : Ouvrez "ESP-IDF PowerShell"
- **Linux/macOS** : Lancez `. ./export.sh`

### L'appareil Zigbee n'apparaît pas
1. Vérifiez le mode pairing du coordinateur
2. Vérifiez les logs : `idf.py monitor`
3. Reset l'ESP32-H2

### Les couleurs sont incorrectes
Test rouge pur :
```bash
mosquitto_pub -t "zigbee2mqtt/WS2812_Light/set" \
  -m '{"state":"ON","color":{"x":0.7006,"y":0.2993}}'
```

---

## ?? Documentation

- **[Documentation complète WS2812](esp-idf/ws2812/README.md)**
- **ESP-IDF Zigbee SDK** : https://docs.espressif.com/projects/esp-zigbee-sdk/
- **Zigbee2MQTT** : https://www.zigbee2mqtt.io/
- **Home Assistant ZHA** : https://www.home-assistant.io/integrations/zha/

---

## ?? Caractéristiques ESP32-H2

| Feature | ESP32-H2 |
|---------|----------|
| Wi-Fi | ? Non supporté |
| Zigbee (802.15.4) | ? Natif |
| Bluetooth LE | ? BLE 5.2 |
| Consommation idle | ~5 mA |
| Consommation sleep | ~100 µA |
| Idéal pour | Production Zigbee, batterie |

---

## ? Checklist

- [ ] ESP-IDF installé (v5.3+)
- [ ] ESP32-H2 détecté (COM port)
- [ ] Projet compile sans erreur
- [ ] Appareil s'appaire dans Z2M/ZHA
- [ ] Lumière fonctionne (ON/OFF/Brightness/Color)
- [ ] (Optionnel) Converter externe installé

---

**Version :** Minimale et simplifiée  
**Hardware :** ESP32-H2 + WS2812/WS2812B  
**Firmware :** ESP-IDF v5.3+

?? **Projet prêt pour la production !**?? **Projet prêt pour la production !**?? **Projet prêt pour la production !**?? **Projet prêt pour la production !**