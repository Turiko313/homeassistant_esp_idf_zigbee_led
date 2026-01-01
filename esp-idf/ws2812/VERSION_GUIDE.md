# Choix de la version à compiler

## Versions disponibles

| Fichier | Description | GPIO |
|---------|-------------|------|
| `main.c` | **1 LED simple** - Couleur statique | GPIO5 |
| `main_dual.c` | **2 LEDs indépendantes** | GPIO8 + GPIO5 |
| `main_effects.c` | **1 LED avec effets ESPHome** | GPIO5 |

---

## ?? Version avec effets (RECOMMANDÉE)

**Effets disponibles :**
1. **Aucun** - Couleur statique
2. **Arc-en-ciel** (Rainbow) - Défilement arc-en-ciel
3. **Stroboscope** (Strobe) - Flash rapide
4. **Scintillement** (Flicker) - Effet bougie
5. **Pulsation** (Pulse) - Respiration douce
6. **Scanner** (Scan) - Effet K2000
7. **Étoiles** (Twinkle) - Scintillement aléatoire
8. **Feu d'artifice** (Fireworks) - Explosions de couleurs

---

## ?? Changer de version

**Dans `main/CMakeLists.txt`, modifie la ligne :**

```cmake
# Version simple (couleur statique uniquement)
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")

# Version avec effets (RECOMMANDÉ)
idf_component_register(SRCS "main_effects.c"
                    INCLUDE_DIRS ".")

# Version 2 LEDs indépendantes
idf_component_register(SRCS "main_dual.c"
                    INCLUDE_DIRS ".")
```

Puis recompile :
```bash
idf.py build
idf.py -p COM5 flash
```

---

## ?? Changer d'effet dans le code

**Dans `main_effects.c`, ligne ~470 :**

```c
// Démarrer avec effet Arc-en-ciel
light_state.current_effect = EFFECT_RAINBOW;  // Change ici !
```

**Options :**
- `EFFECT_NONE` - Aucun effet (couleur Zigbee)
- `EFFECT_RAINBOW` - Arc-en-ciel
- `EFFECT_STROBE` - Stroboscope
- `EFFECT_FLICKER` - Scintillement
- `EFFECT_PULSE` - Pulsation
- `EFFECT_SCAN` - Scanner
- `EFFECT_TWINKLE` - Étoiles
- `EFFECT_FIREWORKS` - Feu d'artifice

---

## ?? Contrôle depuis Home Assistant

Avec Zigbee, tu peux toujours contrôler :
- ? On/Off
- ? Luminosité (affecte tous les effets)
- ? Couleur (pour les effets qui l'utilisent)

L'effet tourne en continu quand la lumière est allumée.

---

## ?? TODO futur

Pour changer d'effet depuis Home Assistant, il faudrait :
1. Ajouter un custom cluster Zigbee
2. Ou utiliser MQTT en parallèle de Zigbee
3. Ou créer des scènes Zigbee pour chaque effet
