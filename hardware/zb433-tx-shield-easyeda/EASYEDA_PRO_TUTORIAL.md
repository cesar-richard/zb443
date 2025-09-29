# ZB433 — EasyEDA Pro (from scratch, simplifié)

Cible: ESP32‑C6‑DEV‑KIT‑N8 + Micro‑USB power‑only + FS1000A (5V) avec driver NPN. Pas d’USB data, pas de LDO, pas de boutons.

## 1) Nouveau schéma
- New → Schematic (Pro)
- Grid 2.54 mm. Unités mm.

## 2) Composants à placer (recherche System Lib)
- DevKit: `ESP32-C6-DEV-KIT-N8` (symbole + footprint officiels)
- Micro‑USB: `USB Micro-B Receptacle` (5 broches + shield, SMD horizontal)
- FS1000A: soit `PinHeader 1x3 P2.54` (VCC/DATA/GND), soit ta footprint directe
- NPN: `MMBT2222A SOT-23` (ou `2N2222A TO-92` si THT)
- Résistances 0603: `R 0603` → 1k (`RB`), 10k (`RPU`), option 100k (`RBE` base→GND)
- Condos 0603: `C 0603` → 100nF (découplage FS1000A)
- Bulk: `Electrolytic 470uF/10V` (radial D6.3/D8) → `CBULK`

## 3) Labels de nets (crée-les)
- `+5V`, `GND`, `IO4`, `DATA_TX5V`

## 4) Câblage minimal par labels
- Micro‑USB (power only):
  - Pin1 → `+5V`
  - Pin5 + Shield → `GND`
  - Pins 2/3/4 (D−/D+/ID) → NC
- Alimentation DevKit:
  - Pin 5V du `ESP32-C6-DEV-KIT-N8` → `+5V`
  - Toute pin GND du DevKit → `GND`
- FS1000A (ou header 1×3):
  - Pin1 (VCC) → `+5V`
  - Pin3 (GND) → `GND`
  - Pin2 (DATA) → `DATA_TX5V`
- Driver NPN:
  - `IO4` (sur le DevKit) → `RB` 1k → base `Q1`
  - `Q1` émetteur → `GND`
  - `Q1` collecteur → `DATA_TX5V`
  - `RPU` 10k: `+5V` ↔ `DATA_TX5V`
  - (Option) `RBE` 100k: base `Q1` ↔ `GND` (anti flottement)
- Découplage ligne 5V FS1000A:
  - `Cdec` 100nF: `+5V` ↔ `GND` près du module FS1000A
  - `CBULK` 470uF/10V: `+5V` ↔ `GND` à l’entrée du module (ou près du header)

## 5) Convert to PCB (footprints)
- DevKit: footprint officiel `ESP32-C6-DEV-KIT-N8`
- Micro‑USB: réceptacle horizontal 5P correspondant à ton stock (shield à GND)
- NPN: `SOT-23` (ou `TO-92`)
- R/C: `0603`
- Header FS1000A: `1x3 P2.54` (ou footprint FS1000A directe)
- Bulk: radial D6.3/D8 selon dispo

## 6) Placement / Routage
- Micro‑USB en bord; plan `GND` continu.
- `+5V` court et large vers le DevKit, puis vers FS1000A.
- `CBULK` près du FS1000A; `Cdec 100nF` au plus près de VCC FS1000A.
- Tracé `IO4→RB→Q1.B` court; `Q1.E` à un via GND proche; `Q1.C` direct sur `DATA_TX5V`.

## 7) Mini‑BOM (indicatif)
- ESP32‑C6‑DEV‑KIT‑N8
- Micro‑USB réceptacle 5P (SMD horizontal)
- FS1000A TX 433 MHz
- MMBT2222A (SOT‑23) ou 2N2222A (TO‑92)
- R 0603: 1k (RB), 10k (RPU), [100k option RBE]
- C 0603: 100nF (découplage)
- Électrolytique 470uF/10V (bulk)
- Header 1×3 P2.54 (si pas de soudure directe)

## 8) Remarques
- Le DevKit intègre sa régulation 5V→3V3 et l’USB de programmation: ton Micro‑USB sur le shield ne fait qu’alimenter en 5V.
- Aucun D+/D− routé; aucun bouton nécessaire.
- Avec ce driver NPN+PNP, `DATA_TX5V` est maintenu à 0 V (via `RPD`) quand `IO4` est bas; quand `IO4` passe haut, `Q1` tire la base de `Q2` bas et `Q2` force `DATA_TX5V` à +5 V.

## 9) Firmware (logique / init)
- Logique OOK: actif‑haut. `GPIO4=1` → `DATA_TX5V=+5V` (émission). `GPIO4=0` → `DATA_TX5V=0V` (repos, aucune émission).
- Init GPIO: sortie push‑pull, sans pull‑up/down; niveau au repos **bas (0)**.
- Dans le code (`main/came433.c`):
  - Démarrage et repos: `gpio_set_level(CAME_GPIO, 0);` (idle bas)
  - Avant/Après transmission: forcer l’IDLE bas.
  - L’encodeur RMT génère des impulsions hautes pour les « HIGH » du protocole CAME (LOW puis HIGH par bit, sync: long LOW puis court HIGH).
- Si un jour tu inverses la topologie matérielle, il faudra inverser la polarité (niveau idle et niveaux dans l’encodeur).

Done. Reprends le schéma en suivant ces étapes; tu devrais passer au PCB en <5 min.
