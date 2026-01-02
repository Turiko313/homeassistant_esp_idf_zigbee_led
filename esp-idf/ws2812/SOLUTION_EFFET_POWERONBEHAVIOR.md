# ?? SOLUTION DÉFINITIVE - Retirer Effect et Power-on behavior

## Le problème

**Effect** et **Power-on behavior** sont **générés automatiquement par Zigbee2MQTT**, même si votre firmware ESP32 ne les implémente pas. C'est le comportement standard de Zigbee2MQTT pour toutes les lumières.

**Vous ne pouvez PAS les retirer depuis le firmware ESP32.**

---

## ? La SEULE solution qui fonctionne

### Méthode : Converter externe (fichier .mjs)

Créez un fichier qui **remplace** la définition auto-générée de Zigbee2MQTT.

**Format moderne recommandé :** `.mjs` (ES Module)

---

## ?? Installation (5 minutes) - VERSION MODERNE

### Méthode 1 : Via l'interface Zigbee2MQTT (RECOMMANDÉ)

**La plus simple et la plus rapide !**

#### Étape 1 : Accéder aux converters externes

1. Ouvrez **Zigbee2MQTT** dans votre navigateur
2. Cliquez sur **Settings** (??)
3. Allez dans l'onglet **External converters**

#### Étape 2 : Ajouter le converter

1. Cliquez sur **"Add new converter"**
2. **Copiez-collez** le contenu du fichier `WS2812_Light.mjs` (disponible dans le repo)
3. Cliquez sur **"Save"**

**Code à copier :**
```javascript
import {light} from 'zigbee-herdsman-converters/lib/modernExtend';

const definition = {
    zigbeeModel: ['WS2812_Light'],
    model: 'WS2812_Light',
    vendor: 'ESP32-Zigbee',
    description: 'WS2812 LED Strip Controller',
    extend: [
        light({
            colorTemp: false,
            color: {modes: ['xy'], enhancedHue: false},
            powerOnBehavior: false,
            effect: false,
        }),
    ],
    endpoint: (device) => ({default: 10}),
};

export default definition;
```

#### Étape 3 : Redémarrer Zigbee2MQTT

Redémarrez via l'interface ou via Home Assistant (Settings ? Add-ons ? Zigbee2MQTT ? Restart)

#### Étape 4 : Re-pairing

1. **Supprimez** l'appareil de Zigbee2MQTT
2. **Permit join**
3. **Reset** l'ESP32-H2 (débrancher/rebrancher)

---

### Méthode 2 : Via fichier configuration (alternative)

**Si vous préférez éditer des fichiers :**

#### Étape 1 : Créer le fichier

**Home Assistant (Addon) :**
```
/config/zigbee2mqtt/WS2812_Light.mjs
```

**Docker :**
```
/app/data/WS2812_Light.mjs
```

**Contenu :** Même code que la Méthode 1

#### Étape 2 : Modifier configuration.yaml

Ajoutez cette section dans `/config/zigbee2mqtt/configuration.yaml` :

```yaml
# External converters
external_converters:
  - WS2812_Light.mjs
```

**Exemple complet :**
```yaml
homeassistant: true
permit_join: false
mqtt:
  base_topic: zigbee2mqtt
  server: mqtt://localhost

# AJOUTEZ CETTE SECTION
external_converters:
  - WS2812_Light.mjs

frontend:
  port: 8080
```

#### Étape 3 : Redémarrer et re-pairer

Même procédure que Méthode 1.

---

### Étape 3 : Redémarrer Zigbee2MQTT

**Home Assistant :**
1. Paramètres ? Modules complémentaires
2. Zigbee2MQTT ? Redémarrer

**Docker :**
```bash
docker restart zigbee2mqtt
```

**Manuel :**
```bash
sudo systemctl restart zigbee2mqtt
```

---

### Étape 4 : Vérifier dans les logs

Après redémarrage, vérifiez les logs de Zigbee2MQTT :

```
info: Loaded external converter 'WS2812_Light.mjs'
```

Si vous voyez des erreurs, vérifiez la syntaxe du fichier .mjs

---

### Étape 5 : Supprimer et re-pairer l'appareil

**Important :** La nouvelle définition ne sera appliquée QUE si vous re-pairez.

1. **Supprimer** l'appareil de Zigbee2MQTT
2. **Permit join**
3. **Reset** l'ESP32-H2 (débrancher/rebrancher)
4. L'appareil est détecté avec la **nouvelle définition**

---

## ? Résultat attendu

Après re-pairing :

```json
{
  "exposes": [
    {
      "type": "light",
      "features": [
        { "name": "state" },
        { "name": "brightness" },
        { "name": "color_xy" }
      ]
    },
    { "name": "linkquality" }
  ]
}
```

**? Effect : RETIRÉ**  
**? Power-on behavior : RETIRÉ**  
**? Color Temperature : RETIRÉ**  
**? Color Temp Startup : RETIRÉ**  
**? Color HS : RETIRÉ (seulement XY)**

---

## ?? Fichiers fournis

| Fichier | Format | Description |
|---------|--------|-------------|
| **`WS2812_Light.mjs`** | ES Module (moderne) | ? **RECOMMANDÉ** |
| `WS2812_Light.js` | CommonJS (legacy) | Pour anciennes versions Z2M |

**Utilisez `.mjs` si votre Zigbee2MQTT est version 1.30.0+**

---

## ?? Dépannage

### Erreur : "Cannot use import statement outside a module"

**Cause :** Vous avez utilisé le format `.mjs` avec une vieille version de Z2M.

**Solution :** Utilisez `WS2812_Light.js` (format CommonJS) à la place.

### Le converter n'est pas chargé

**Vérifiez :**
1. Le fichier est bien dans `/config/zigbee2mqtt/`
2. `external_converters:` est bien dans `configuration.yaml`
3. Le nom du fichier correspond exactement : `WS2812_Light.mjs`
4. Zigbee2MQTT a redémarré
5. Vérifiez les logs : `info: Loaded external converter...`

### Les effets apparaissent toujours

**Cause :** L'ancienne définition est en cache.

**Solution :**
```bash
# Arrêtez Zigbee2MQTT
# Supprimez le cache
rm /config/zigbee2mqtt/database.db.backup
# Redémarrez Zigbee2MQTT
# Re-pairez l'appareil
```

### L'appareil n'est plus détecté

**Cause :** Erreur de syntaxe dans le fichier .mjs

**Solution :**
1. Vérifiez les logs : `/config/zigbee2mqtt/log/...`
2. Corrigez les erreurs
3. Redémarrez Z2M

---

## ?? Migration depuis l'ancien format (.js)

Si vous aviez déjà `WS2812_Light.js` :

1. **Supprimez** l'ancien fichier `.js`
2. **Créez** le nouveau fichier `.mjs`
3. **Ajoutez** `external_converters:` dans `configuration.yaml`
4. **Redémarrez** Zigbee2MQTT
5. **Re-pairez** l'appareil

---

## ?? Liens utiles

- **Zigbee2MQTT - External converters** : https://www.zigbee2mqtt.io/advanced/support-new-devices/01_support_new_devices.html
- **Exemple officiel (light.mjs)** : https://github.com/Nerivec/z2m-external-converter-dev/blob/main/examples/light.mjs
- **ModernExtend documentation** : https://www.zigbee2mqtt.io/advanced/support-new-devices/03_support_new_devices_advanced.html

---

## ?? Pourquoi le format .mjs est meilleur ?

| Avantage | .mjs (moderne) | .js (legacy) |
|----------|----------------|--------------|
| **Syntaxe** | ES6 modules (import/export) | CommonJS (require/module.exports) |
| **ModernExtend** | ? Supporté nativement | ?? Nécessite workarounds |
| **Simplicité** | ? Code plus court | ?? Plus verbeux |
| **Future-proof** | ? Standard moderne | ?? Déprécié progressivement |

**Recommandation :** Utilisez **`.mjs`** si Z2M >= 1.30.0

---

## ? Conclusion

**La SEULE façon de vraiment retirer "Effect" et "Power-on behavior" est de créer un converter externe.**

**Fichier recommandé :** `WS2812_Light.mjs` (format moderne)

**Configuration requise :** `external_converters:` dans `configuration.yaml`

**Après installation :** Effect et Power-on behavior disparaissent complètement ! ??
