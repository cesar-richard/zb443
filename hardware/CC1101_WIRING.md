# CC1101 — Câblage pour ESP32‑C6 DevKit (433,92 MHz FSK)

Objectif: remplacer le FS1000A (OOK) par un CC1101 (2‑FSK) pour reproduire les trames `FuriHalSubGhzPreset2FSKDev238Async`.

Alimentation
- VCC 3,3 V (strict, pas 5 V)
- GND commun
- 100 nF au plus près du VCC CC1101 + 10 µF proche du module
- Antenne λ/4 ≈ 17,3 cm sur le connecteur du module

SPI (mode 0)
- SCK  → GPIO6
- MOSI → GPIO7
- MISO → GPIO5
- CSN  → GPIO10 (actif bas)
- GDO0 → GPIO11 (IRQ)

Récap connexions
- 3V3 ↔ VCC CC1101
- GND ↔ GND CC1101
- GPIO6 ↔ SCK
- GPIO7 ↔ MOSI
- GPIO5 ↔ MISO
- GPIO10 ↔ CSN
- GPIO11 ↔ GDO0

Notes
- Retirer FS1000A + 2N2222 (inutiles en FSK)
- I/O ESP32‑C6 en 3,3 V compatibles CC1101
- Garder 100 nF et 470 µF sur le rail 3V3 global
