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

### ESP32-C6 (Wi-Fi + Zigbee)
Choisissez le YAML selon le type de ruban :
- `zigbee_mono_12-24v.yaml` : 1 canal (blanc) analogique 5/12/24 V
- `zigbee_cct_12-24v.yaml` : 2 canaux blanc chaud/froid (CCT) analogique 5/12/24 V
- `zigbee_rgb_12-24v.yaml` : 3 canaux RGB analogique 5/12/24 V
- `zigbee_rgbw_12-24v.yaml` : 4 canaux RGBW analogique 5/12/24 V
- `zigbee_ws2812_5v.yaml` : ruban adressable 5 V (WS2812/WS2812B), piloté comme une seule lumière

**ESP32-C6 supporte Wi-Fi + Zigbee simultanément** ? logs OTA et mises à jour sans câble.

### ESP32-H2 (Zigbee pur uniquement) - `examples/h2/`
?? **ESP32-H2 n'a pas de Wi-Fi natif** (seulement Bluetooth LE + Zigbee). Utilisez les versions sans Wi-Fi dans `examples/h2/` :
- `zigbee_mono_12-24v_h2.yaml`
- `zigbee_cct_12-24v_h2.yaml`
- `zigbee_rgb_12-24v_h2.yaml`
- `zigbee_rgbw_12-24v_h2.yaml`
- `zigbee_ws2812_5v_h2.yaml`

Mode Zigbee pur ? pas de logs temps réel, flash via USB uniquement.

### Points communs
- Zigbee configuré (canal, PAN ID, clé réseau) à ajuster selon votre réseau
- Pins GPIO par défaut pour ESP32-C6 DevKitC (LED analogique : 4/5/6/7 ; WS2812 data : 8) — modifiez selon votre carte
- Bouton virtuel de reboot exposé (entité `button`)
- Effets de lumière préconfigurés (random/strobe/flicker pour analogique, rainbow/twinkle/scan pour WS2812)

## Pré-requis
- ESP-IDF ? 5.1 installé (`idf.py --version`)
- ESPHome ? 2023.12 avec support Zigbee

## Installation ESPHome
```powershell
pip install esphome
```

## Configuration Wi-Fi (ESP32-C6 uniquement)
Créez `secrets.yaml` dans le même dossier que vos YAMLs :
```yaml
wifi_ssid: "VotreSSID"
wifi_password: "VotreMotDePasse"
```

**ESP32-H2** : ignore `secrets.yaml`, pas de Wi-Fi supporté.

## Construction et flash (ESPHome CLI)
```powershell
# Pour ESP32-C6 (avec Wi-Fi)
esphome run examples/zigbee_rgbw_12-24v.yaml

# Pour ESP32-H2 (Zigbee pur)
esphome run examples/h2/zigbee_rgbw_12-24v_h2.yaml
```

## Intégration Home Assistant (Zigbee)
1. Mettre le coordinateur en mode appairage (ZHA ou zigbee2mqtt).
2. Alimentez l'ESP : il se joint en tant qu'ampoule Zigbee (type selon YAML).
3. Dans Home Assistant, renommez l'entité et testez On/Off, dimming, couleurs/CT/effets.
4. Si Wi-Fi activé (C6) : l'appareil apparaît aussi dans ESPHome Dashboard pour logs/OTA.

## Personnalisation
- Ajustez les GPIO pour vos cartes (C6/H2, modules compacts).
- Changez le nombre de LEDs pour WS2812 (`led_count`).
- Adaptez les effets ou la durée de transition.
- Sélectionnez un canal Zigbee peu encombré (11/15/20/25).

## Comparaison C6 vs H2

| Feature | ESP32-C6 | ESP32-H2 |
|---------|----------|----------|
| Wi-Fi 2.4 GHz | ? | ? |
| Zigbee (802.15.4) | ? | ? |
| Bluetooth LE | ? | ? |
| Logs OTA ESPHome | ? | ? |
| Économie énergie | Moyen | Meilleur |
| Prix | + cher | - cher |

**Recommandation : ESP32-C6 pour développement/debug, ESP32-H2 pour production si Wi-Fi inutile.**

## Améliorations possibles
- Ajouter un bouton physique (reset/appairage).
- Ajouter un capteur de température interne pour surveiller l'ESP.
- Exposer diagnostics Zigbee (LQI/RSSI) si supporté par la stack.
- Fournir des presets de broches par carte (`sdkconfig.defaults`).

## État du dépôt
Le code applicatif ESP-IDF/ESPHome est à ajouter selon vos besoins. Les YAML d'exemple sont prêts à l'emploi.