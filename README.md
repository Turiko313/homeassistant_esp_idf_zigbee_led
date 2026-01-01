# Zigbee LED Strip Controller for ESP32-H2 with Home Assistant

Contrôlez vos rubans LED (mono, CCT, RGB, RGBW ou WS2812) avec un ESP32-H2 via Zigbee, directement intégrable dans Home Assistant.

**? ESP32-H2 uniquement** : optimisé pour Zigbee pur, consommation minimale, idéal pour production.

---

## ?? Démarrage rapide (ESP-IDF)

### 1. Installation ESP-IDF

**Windows (recommandé : installeur offline) :**
```powershell
# Télécharger : https://dl.espressif.com/dl/esp-idf/
# Lancer l'installeur, cocher ESP32-H2
# Après installation, ouvrir "ESP-IDF PowerShell"
```

**Linux / macOS :**
```bash
git clone -b v5.3.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32h2
. ./export.sh
```

### 2. Compiler et flasher

```bash
# Exemple WS2812 (recommandé pour débuter)
cd esp-idf/ws2812
idf.py set-target esp32h2
idf.py build
idf.py -p COM3 flash monitor
```

**Exemples disponibles :**
- `esp-idf/ws2812/` : Ruban adressable WS2812/WS2812B (RGB full color)
- `esp-idf/rgb/` : Ruban RGB analogique PWM (12V/24V)

### 3. Intégration Home Assistant

1. **Paramètres ? Appareils et services ? Zigbee (ZHA ou zigbee2mqtt) ? Ajouter un appareil**
2. Alimentez l'ESP32-H2, il rejoint automatiquement le réseau Zigbee
3. Contrôlez On/Off/Dim/Couleur depuis Home Assistant

---

## ?? Matériel

- **ESP32-H2** (radio Zigbee 802.15.4 + Bluetooth LE, **pas de Wi-Fi**)
- Ruban LED 5V adressable (WS2812) ou analogique 12V/24V (RGB/RGBW/CCT/mono)
- 3/4 MOSFETs canal N (ou driver) pour rubans analogiques
- Level shifter 5V conseillé pour données WS2812
- Alimentation adaptée, masse commune avec l'ESP32

## ?? GPIO par défaut (modifiables dans le code)

| Type LED | GPIO ESP32-H2 |
|----------|---------------|
| WS2812 Data | GPIO8 |
| RGB R/G/B | GPIO4/5/6 |
| RGBW R/G/B/W | GPIO4/5/6/7 |
| CCT CW/WW | GPIO4/5 |
| Mono W | GPIO4 |

---

## ? Optimisations d'économie d'énergie (ESP32-H2)

L'ESP32-H2 est conçu pour la faible consommation. Activez ces optimisations dans vos projets :

### 1. Mode Light Sleep (recommandé)
Lorsque les LEDs sont éteintes, l'ESP32-H2 peut entrer en Light Sleep (~100 µA) :

```c
// Dans app_main.c, après initialisation Zigbee
#include "esp_pm.h"

esp_pm_config_t pm_config = {
    .max_freq_mhz = 96,      // Fréquence max CPU
    .min_freq_mhz = 32,      // Fréquence min CPU (économie)
    .light_sleep_enable = true
};
esp_pm_configure(&pm_config);
```

### 2. Réduction fréquence PWM
Pour rubans analogiques, réduisez la fréquence PWM si pas de scintillement visible :

```c
// Dans rgb/main/app_main.c
#define PWM_FREQ_HZ  1000  // au lieu de 5000 (consomme moins)
```

### 3. Désactiver les logs UART en production
```c
// Dans menuconfig : Component config ? Log output ? Default log verbosity ? Error
// Ou dans code :
esp_log_level_set("*", ESP_LOG_ERROR);
```

### 4. Optimiser le reporting Zigbee
Réduisez la fréquence de reporting pour économiser la radio :

```c
// Configure des attributs Zigbee avec reporting interval plus long
// (exemple à ajouter dans la config des clusters)
```

### 5. Utiliser un End Device (ZED) plutôt qu'un Router
Les exemples utilisent déjà `ESP_ZB_ZED_CONFIG()` (End Device), qui consomme moins qu'un router Zigbee.

**Consommation estimée ESP32-H2 :**
- Actif (TX Zigbee) : ~20 mA
- Idle (radio ON) : ~5 mA
- Light Sleep : ~100 µA
- Deep Sleep : ~5 µA (non compatible avec Zigbee actif)

---

## ?? Exemples ESPHome (pour utilisation future)

?? **Support Zigbee ESPHome NON disponible actuellement.**  
Les YAMLs dans `examples/` sont prêts pour quand ESPHome ajoutera le composant `zigbee` officiellement.

- `examples/zigbee_mono_12-24v.yaml` : Mono-canal blanc
- `examples/zigbee_cct_12-24v.yaml` : Bi-température (CCT)
- `examples/zigbee_rgb_12-24v.yaml` : RGB
- `examples/zigbee_rgbw_12-24v.yaml` : RGBW
- `examples/zigbee_ws2812_5v.yaml` : WS2812 adressable

---

## ?? Dépannage

### Port COM non détecté
Installez les pilotes USB-to-Serial :
- **CP210x** (Silicon Labs) : https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- **CH340** (WCH) : http://www.wch-ic.com/downloads/CH341SER_EXE.html

### Erreur "idf.py: command not found"
- **Windows** : Ouvrez "ESP-IDF PowerShell" depuis le menu Démarrer
- **Linux/macOS** : Lancez `. ./export.sh` dans le terminal

### L'appareil Zigbee n'apparaît pas dans Home Assistant
1. Vérifiez que le coordinateur est en mode appairage
2. Vérifiez les logs : `idf.py monitor`
3. Réinitialisez l'ESP32-H2 (bouton RESET)
4. Vérifiez que le canal Zigbee correspond à votre réseau

### Consommation élevée
- Activez Light Sleep (voir section optimisations)
- Vérifiez qu'il n'y a pas de boucle while(1) sans delay dans le code
- Réduisez la fréquence CPU dans menuconfig

---

## ?? Documentation

- **ESP-IDF Zigbee SDK** : https://docs.espressif.com/projects/esp-zigbee-sdk/
- **ESP32-H2 Datasheet** : https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf
- **Home Assistant ZHA** : https://www.home-assistant.io/integrations/zha/
- **ESP32-H2 Power Management** : https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/system/power_management.html

---

## ?? Caractéristiques ESP32-H2

| Feature | ESP32-H2 |
|---------|----------|
| Wi-Fi | ? Non supporté |
| Zigbee (802.15.4) | ? Natif |
| Bluetooth LE | ? BLE 5.2 |
| Fréquence CPU | 96 MHz |
| RAM | 320 KB |
| Flash | Externe (selon module) |
| Consommation idle | ~5 mA |
| Consommation sleep | ~100 µA |
| Prix | € (économique) |
| Idéal pour | Production Zigbee, batterie |

---

## ?? Licence

Projet open-source pour la communauté. Contributions bienvenues !

## ?? Contribuer

- Fork le projet
- Créez une branche (`git checkout -b feature/amelioration`)
- Commit (`git commit -m 'Ajout feature X'`)
- Push (`git push origin feature/amelioration`)
- Ouvrez une Pull Request