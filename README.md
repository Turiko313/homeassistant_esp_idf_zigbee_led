# Zigbee LED Strip with ESP-IDF (ESP32-C6/H2) for Home Assistant

Projet de base pour piloter un ruban LED (mono, CCT, RGB, RGBW ou WS2812) avec un SoC Zigbee ESP32-C6 / ESP32-H2 via ESP-IDF (ESPHome), intégrable dans Home Assistant comme ampoule Zigbee.

## Matériel
- ESP32-C6 ou ESP32-H2 (radio 802.15.4 nécessaire pour Zigbee)
- Ruban LED 5 V adressable (WS2812) ou analogique 12/24 V (mono, CCT, RGB/RGBW) — les configs analogiques fonctionnent aussi en 5 V si MOSFETs logique niveau
- 3/4 MOSFETs canal N (ou driver dédié) pour rubans analogiques
- Level shifter 5 V conseillé pour données WS2812
- Alimentation adaptée au ruban, masse commune avec l'ESP
- Câblage : sorties PWM (LEDC) vers MOSFETs pour analogique ; 1 pin data pour WS2812

## Exemples fournis (`examples/`)
Choisissez le YAML selon le type de ruban :
- `zigbee_mono_12-24v.yaml` : 1 canal (blanc) analogique 5/12/24 V
- `zigbee_cct_12-24v.yaml` : 2 canaux blanc chaud/froid (CCT) analogique 5/12/24 V
- `zigbee_rgb_12-24v.yaml` : 3 canaux RGB analogique 5/12/24 V
- `zigbee_rgbw_12-24v.yaml` : 4 canaux RGBW analogique 5/12/24 V
- `zigbee_ws2812_5v.yaml` : ruban adressable 5 V (WS2812/WS2812B), piloté comme une seule lumière

Points communs dans chaque YAML :
- Zigbee configuré (canal, PAN ID, clé réseau) à ajuster selon votre réseau
- Pins GPIO par défaut pour ESP32-C6 DevKitC (LED analogique : 4/5/6/7 ; WS2812 data : 8) — modifiez selon votre carte
- Bouton virtuel de reboot exposé (entité `button`), retour au firmware via Zigbee
- Effets de lumière préconfigurés (random/strobe/flicker pour analogique, rainbow/twinkle/scan pour WS2812)
- **Wi-Fi optionnel** : sections `wifi`, `api`, `ota` activées par défaut pour logs/OTA, commentez-les pour mode Zigbee pur

## Pré-requis
- ESP-IDF ? 5.1 installé (`idf.py --version`)
- ESPHome ? 2023.12 avec support Zigbee

## Installation ESPHome
```powershell
pip install esphome
```

## Configuration Wi-Fi (optionnel)
Créez `secrets.yaml` dans le même dossier que vos YAMLs :
```yaml
wifi_ssid: "VotreSSID"
wifi_password: "VotreMotDePasse"
```

**Mode Zigbee pur (sans Wi-Fi)** : commentez les blocs `wifi:`, `api:`, `ota:` dans le YAML.  
**Mode dual (Zigbee + Wi-Fi)** : gardez les blocs pour logs en temps réel et OTA sans câble.

## Construction et flash (ESPHome CLI)
```powershell
# Compile
esphome compile examples/zigbee_rgbw_12-24v.yaml

# Flash via USB
esphome upload examples/zigbee_rgbw_12-24v.yaml --device COM3

# Logs en temps réel (si Wi-Fi activé)
esphome logs examples/zigbee_rgbw_12-24v.yaml
```

## Intégration Home Assistant (Zigbee)
1. Mettre le coordinateur en mode appairage (ZHA ou zigbee2mqtt).
2. Alimentez l'ESP : il se joint en tant qu'ampoule Zigbee (type selon YAML).
3. Dans Home Assistant, renommez l'entité et testez On/Off, dimming, couleurs/CT/effets.
4. Si Wi-Fi activé : l'appareil apparaît aussi dans ESPHome Dashboard pour logs/OTA.

## Personnalisation
- Ajustez les GPIO pour vos cartes (C6/H2, modules compacts).
- Changez le nombre de LEDs pour WS2812 (`led_count`).
- Adaptez les effets ou la durée de transition.
- Sélectionnez un canal Zigbee peu encombré (11/15/20/25).
- Désactivez Wi-Fi si vous voulez économiser l'énergie (mode Zigbee pur).

## Améliorations possibles
- Ajouter un bouton physique (reset/appairage).
- Ajouter un capteur de température interne pour surveiller l'ESP.
- Exposer diagnostics Zigbee (LQI/RSSI) si supporté par la stack.
- Fournir des presets de broches par carte (`sdkconfig.defaults`).

## État du dépôt
Le code applicatif ESP-IDF/ESPHome est à ajouter selon vos besoins. Les YAML d'exemple sont prêts à l'emploi.