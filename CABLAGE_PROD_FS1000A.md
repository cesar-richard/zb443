### Câblage production FS1000A (TX seul) avec ESP32‑C6 DevKit

Cette note décrit le montage « prod ready » pour émettre en 433,92 MHz avec un FS1000A alimenté en 5 V, isolation par 2N2222 (open‑collector), logique inversée déjà activée dans le firmware (`CAME_INVERT_OUTPUT=1`), TX sur `GPIO9`. La LED WS2812 reste sur `GPIO8`.

Firmware concerné:
- `main/came433.h`: `#define CAME_INVERT_OUTPUT 1`
- `main/came433.c`: inversion gérée via `OUT_LEVEL()`
- GPIO: TX = `GPIO9`, LED = `GPIO8`

---

### BOM (minimum)
- FS1000A (émetteur 433 MHz)
- ESP32‑C6 DevKit
- 2N2222 (NPN)
- Résistances: 1 kΩ (×1), 10 kΩ (×1)
- Condensateurs: 100 nF (×1), 470 µF 10 V (×1)
- Antenne: fil rigide ~17,3 cm (λ/4 à 433,92 MHz)
- Alim secteur 240→5 V (ou 5 V USB du DevKit en dev)

---

### Alimentation et masses

| Net | De | Vers | Notes |
|---|---|---|---|
| +5V | Alim 240→5V | FS1000A VCC | Alimente l’émetteur en 5 V (meilleure portée) |
| +5V | Alim 240→5V | DevKit 5V | Alimente le DevKit (ou utilisez l’USB en dev) |
| GND | Alim 240→5V GND | DevKit GND | Masse commune |
| GND | DevKit GND | FS1000A GND | Masse commune |
| 100 nF | VCC ↔ GND | Au plus près du FS1000A | Découplage local du module TX |
| 470 µF 10 V | +5V ↔ GND | Rail +5V | Réservoir pour pics de courant TX |

---

### Chaîne TX (open‑collector 5 V via 2N2222)

| Net | De | Vers | Valeur | Notes |
|---|---|---|---:|---|
| Base drive | ESP32 `GPIO9` | Base 2N2222 via R | 1 kΩ | Limitation courant base |
| Émetteur | 2N2222 | GND | — | Référence |
| Collecteur | 2N2222 | FS1000A DATA | — | Noeud open‑collector |
| Pull‑up DATA | +5V | FS1000A DATA | 10 kΩ | Ramène DATA à 5 V lorsque le NPN est OFF |
| Antenne | FS1000A ANT | Fil 17,3 cm | — | λ/4, fil droit dégagé |

Remarque: montage inverseur. Le firmware est déjà configuré (`CAME_INVERT_OUTPUT=1`).

---

### LED (déjà gérée par le firmware)

| Net | De | Vers | Notes |
|---|---|---|---|
| LED data | ESP32 `GPIO8` | LED RGB (WS2812 onboard si présente) | `led.c` gère signal et effets |

---

### Schéma ASCII (TX seul)

```
                +5V (alimentation)
                  |---------------------------> DevKit 5V
                  |-------[10k]------.
                  |                  |
               [470uF]            FS1000A
                  |                TX DATA o-----.------> ANT (fil 17.3 cm)
                 GND                  |          |
                  |                   '----o  Collecteur 2N2222
                  |                         |
DevKit GND o-------------------------------o- Émetteur
                                           |
DevKit GPIO9 o--[1k]--o Base 2N2222        |
                        |
                       GND (pull‑down base optionnel si dispo)
```

---

### Recommandations RF / EMI
- Fil d’antenne droit de 17,3 cm, vertical, éloigné des plans de masse et faisceaux d’alim.
- Masse commune en étoile, retour court vers la source 5 V.
- 100 nF au plus près du VCC du FS1000A, 470 µF sur le rail 5 V.
- Éviter les boucles de masse et les nappes non torsadées autour de l’antenne.

---

### Validation rapide
1. Brancher selon les tableaux ci‑dessus.
2. Compiler/flash (`came433_test()` ou déclenchement via endpoints Zigbee).
3. Vérifier à l’oscillo sur DATA FS1000A: impulsions niveau 5 V, actives « bas » (NPN ON) conformément à l’inversion.
4. Tester la portée en extérieur; ajuster et dégager l’antenne si nécessaire.
