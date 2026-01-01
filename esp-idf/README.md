# ESP-IDF Zigbee LED Controller Examples (ESP32-H2)

Exemples ESP-IDF pour contrôler des rubans LED via Zigbee avec ESP32-H2.

**?? Important : ESP-IDF 5.3.1 et support Zigbee**

ESP-IDF 5.3.1 nécessite le composant externe `esp-zigbee-lib` du gestionnaire de composants Espressif. Les dépendances sont automatiquement téléchargées lors de la première compilation.

## Pré-requis

1. **ESP-IDF 5.3.1** installé (installeur offline recommandé)
2. **ESP32-H2** (support Zigbee natif)
3. **Connexion Internet** pour télécharger `esp-zigbee-lib` lors de la première compilation

## Installation ESP-IDF (Windows - méthode recommandée)

**Téléchargez l'installeur offline :** https://dl.espressif.com/dl/esp-idf/

1. Lancez `esp-idf-tools-setup-offline-5.3.1.exe`
2. Cochez **ESP32-H2**
3. Après installation, ouvrez **"ESP-IDF 5.3 PowerShell"**

## Compiler et flasher

### WS2812 (RGB adressable)

```powershell
cd ws2812
idf.py set-target esp32h2
idf.py build                    # Télécharge esp-zigbee-lib automatiquement
idf.py -p COM5 flash monitor
```

### RGB PWM (analogique 12V/24V)

```powershell
cd rgb
idf.py set-target esp32h2
idf.py build
idf.py -p COM5 flash monitor
```

## Configuration GPIO

| Type | GPIO par défaut (modifiable dans app_main.c) |
|------|----------------------------------------------|
| WS2812 Data | GPIO8 |
| RGB R/G/B | GPIO4/5/6 |

## Dépendances automatiques

Les composants suivants sont téléchargés automatiquement via le gestionnaire de composants :
- `espressif/led_strip` (pour WS2812)
- `espressif/esp-zigbee-lib` (bibliothèque Zigbee pour ESP-IDF 5.3)

**Première compilation :** Peut prendre 5-10 minutes (téléchargement + compilation).

**Compilations suivantes :** 1-2 minutes.

## Optimisations d'économie d'énergie incluses

? **End Device Zigbee (ZED)** : consomme moins qu'un router  
? **PWM 5kHz** : équilibre entre qualité et consommation  
? **Logs minimaux** : réduit l'activité UART  

**Consommation ESP32-H2 estimée :**
- LEDs allumées : ~20-25 mA
- LEDs éteintes (idle) : ~5 mA
- Light Sleep : ~100 µA (si configuré)

## Intégration Home Assistant

1. Coordinateur Zigbee en mode appairage
2. Alimentez l'ESP32-H2
3. L'appareil apparaît automatiquement comme ampoule Zigbee
4. Contrôlez On/Off/Dim/Couleur

## Dépannage

### Erreur "esp-zigbee-lib could not be found"
? Assurez-vous d'avoir une connexion Internet active lors de la première compilation. Le composant sera téléchargé automatiquement.

### Erreur de compilation API Zigbee
? Le code utilise l'API `esp-zigbee-lib` 1.x. Si vous avez une version différente, vérifiez la documentation : https://components.espressif.com/components/espressif/esp-zigbee-lib

## Documentation

- **esp-zigbee-lib** : https://components.espressif.com/components/espressif/esp-zigbee-lib
- **ESP32-H2 Zigbee** : https://docs.espressif.com/projects/esp-zigbee-sdk/
- **ESP32-H2 Power** : https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32h2/api-reference/system/power_management.html
