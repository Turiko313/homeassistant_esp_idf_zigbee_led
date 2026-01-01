# Zigbee LED Strip with ESP-IDF (ESP32-C6/H2) for Home Assistant

Projet pour piloter des rubans LED (mono, CCT, RGB, RGBW ou WS2812) avec ESP32-C6 / ESP32-H2 via Zigbee, intégrable dans Home Assistant.

## ?? État du support

### ESPHome (exemples YAML)
?? **Support Zigbee ESPHome NON disponible actuellement.**  
Les exemples dans `examples/` sont prêts pour utilisation future quand ESPHome ajoutera le composant `zigbee`.

### ESP-IDF (exemples C/C++) ? FONCTIONNEL
**Utilisez les exemples ESP-IDF dans le dossier `esp-idf/` pour une solution Zigbee fonctionnelle maintenant.**

---

## ?? Démarrage rapide (ESP-IDF)

### 1. Installation ESP-IDF

**Windows :**
```powershell
# Télécharger l'installeur offline : https://dl.espressif.com/dl/esp-idf/
# Après installation, ouvrir "ESP-IDF PowerShell" ou :
cd esp-idf
.\install.bat esp32h2,esp32c6
.\export.bat
```

**Linux / macOS :**
```bash
git clone -b v5.3.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32h2,esp32c6
. ./export.sh
```

### 2. Compiler et flasher un exemple

```bash
# Exemple WS2812 (recommandé pour débuter)
cd esp-idf/ws2812
idf.py set-target esp32h2      # ou esp32c6
idf.py build
idf.py -p COM3 flash monitor   # remplacez COM3 par votre port
```

**Exemples disponibles :**
- `esp-idf/ws2812/` : Ruban adressable WS2812/WS2812B (RGB full color)
- `esp-idf/rgb/` : Ruban RGB analogique PWM (12V/24V)
- `esp-idf/rgbw/` : Ruban RGBW analogique PWM (à venir)
- `esp-idf/cct/` : Ruban bi-température blanc chaud/froid (à venir)
- `esp-idf/mono/` : Ruban mono-canal blanc (à venir)

### 3. Intégration Home Assistant

1. Dans Home Assistant : **Paramètres ? Appareils et services ? Zigbee (ZHA ou zigbee2mqtt) ? Ajouter un appareil**
2. Alimentez votre ESP32, il rejoint automatiquement le réseau Zigbee
3. L'ampoule apparaît, renommez-la et contrôlez On/Off/Dim/Couleur

---

## Matériel

- ESP32-C6 ou ESP32-H2 (radio 802.15.4 nécessaire pour Zigbee)
- Ruban LED 5 V adressable (WS2812) ou analogique 12/24 V (RGB/RGBW/CCT/mono)
- 3/4 MOSFETs canal N (ou driver) pour rubans analogiques
- Level shifter 5 V conseillé pour données WS2812
- Alimentation adaptée, masse commune avec l'ESP

## GPIO par défaut (modifiables dans le code)

| Type | GPIO ESP32-H2/C6 |
|------|------------------|
| WS2812 Data | GPIO8 |
| RGB R/G/B | GPIO4/5/6 |
| RGBW R/G/B/W | GPIO4/5/6/7 |
| CCT CW/WW | GPIO4/5 |
| Mono W | GPIO4 |

---

## Exemples ESPHome (pour utilisation future)

Les YAMLs dans `examples/` et `examples/h2/` sont prêts pour quand ESPHome supportera Zigbee officiellement.

### ESP32-C6 (Wi-Fi + Zigbee)
- `zigbee_mono_12-24v.yaml`
- `zigbee_cct_12-24v.yaml`
- `zigbee_rgb_12-24v.yaml`
- `zigbee_rgbw_12-24v.yaml`
- `zigbee_ws2812_5v.yaml`

### ESP32-H2 (Zigbee pur, pas de Wi-Fi)
- `examples/h2/zigbee_mono_12-24v_h2.yaml`
- `examples/h2/zigbee_cct_12-24v_h2.yaml`
- `examples/h2/zigbee_rgb_12-24v_h2.yaml`
- `examples/h2/zigbee_rgbw_12-24v_h2.yaml`
- `examples/h2/zigbee_ws2812_5v_h2.yaml`

---

## Comparaison C6 vs H2

| Feature | ESP32-C6 | ESP32-H2 |
|---------|----------|----------|
| Wi-Fi 2.4 GHz | ? | ? |
| Zigbee (802.15.4) | ? | ? |
| Bluetooth LE | ? | ? |
| Prix | + cher | - cher |
| Utilisation | Dev/debug | Production |

**Recommandation : ESP32-C6 pour flexibilité, ESP32-H2 pour production Zigbee pure.**

---

## Dépannage

### Port COM non détecté
Installez les pilotes USB-to-Serial :
- **CP210x** : https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- **CH340** : http://www.wch-ic.com/downloads/CH341SER_EXE.html

### Erreur "idf.py: command not found"
Lancez `export.bat` (Windows) ou `. ./export.sh` (Linux/macOS) dans le terminal avant de compiler.

### L'appareil Zigbee n'apparaît pas dans Home Assistant
- Vérifiez que le coordinateur est en mode appairage
- Vérifiez les logs série : `idf.py monitor`
- Réinitialisez l'ESP32 (bouton RESET)

---

## Documentation

- **ESP-IDF Zigbee SDK** : https://docs.espressif.com/projects/esp-zigbee-sdk/
- **ESP-IDF Getting Started** : https://docs.espressif.com/projects/esp-idf/
- **Home Assistant ZHA** : https://www.home-assistant.io/integrations/zha/

---

## Licence

Projet open-source pour la communauté. Contributions bienvenues !