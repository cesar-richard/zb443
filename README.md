# ZB433 Router - ESP32-C6 Zigbee + 433MHz CAME-24

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2.x-blue.svg)](https://github.com/espressif/esp-idf)
[![ESP32-C6](https://img.shields.io/badge/ESP32--C6-RISC--V-green.svg)](https://www.espressif.com/en/products/socs/esp32-c6)
[![Zigbee](https://img.shields.io/badge/Zigbee-3.0-orange.svg)](https://zigbeealliance.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Vue d'ensemble

Le **ZB433 Router** est un routeur Zigbee ESP32-C6 capable de contrôler des portes CAME-24 via 433MHz et de recevoir des mises à jour OTA. Il agit comme un pont entre votre réseau Zigbee (Zigbee2MQTT) et vos équipements CAME-24 existants.

### Fonctionnalités

- **Routeur Zigbee 3.0** : Compatible avec Zigbee2MQTT
- **Contrôle 433MHz CAME-24** : Deux boutons (Portail Principal et Portail Parking)
- **Identify**: cluster 0x0003 (IdentifyTime + TriggerEffect) avec effet LED
- **Indicateur LED**: couleurs distinctes lors d’une commande
- **2 Endpoints** : EP1 (Portail Principal), EP2 (Portail Parking)

## Installation

### Prérequis

- **ESP-IDF v5.2.x** installé et configuré
- **ESP32-C6 DevKit** (alimentation via USB)
- **Émetteur 433MHz FS1000A** (ASK/OOK)
- **Zigbee2MQTT** configuré avec Home Assistant

### Compilation et flash

```bash
# Configurer l'environnement ESP-IDF
source ~/esp-idf/export.sh

# Configurer la cible
idf.py set-target esp32c6

# Compiler
idf.py build

# Flasher et monitorer
idf.py -p /dev/ttyUSB0 flash monitor
```

### Configuration Wi-Fi

Éditer `sdkconfig.defaults` :

```ini
CONFIG_ZB433_WIFI_SSID="VOTRE_SSID"
CONFIG_ZB433_WIFI_PASSWORD="VOTRE_MOT_DE_PASSE"
CONFIG_ZB433_DEFAULT_OTA_URL="http://homeassistant.local:8123/local/zb433.bin"
```

## Configuration

### Zigbee2MQTT

1. Mettre le coordinateur en mode "permit join"
2. L'appareil s'inclut automatiquement et expose 2 endpoints :
   - **EP1** : Portail Principal (code 0x03B29B)
   - **EP2** : Portail Parking (code 0x03B29A)
3. Identify (device): utiliser le cluster `genIdentify` sur EP1 (IdentifyTime/TriggerEffect)

### Exemple MQTT

```bash
# Ouvrir Portail Principal (EP1)
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/1/set" -m '{"state":"ON"}'

# Ouvrir Portail Parking (EP2)
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/2/set" -m '{"state":"ON"}'

# Identify via attribut (EP1)
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/1/set" -m '{"write":{"cluster":"genIdentify","payload":{"identify_time":5}}}'
```

## Utilisation

### Endpoints disponibles

| Endpoint | Fonction | Action |
|----------|----------|--------|
| **EP1** | Portail Principal | `state: ON` → TX 433MHz (0x03B29B) |
| **EP2** | Portail Parking   | `state: ON` → TX 433MHz (0x03B29A) |

Identify (device): via EP1 (`genIdentify.identify_time`, TriggerEffect).
LED: EP1 bleu-cyan, EP2 magenta lors d’une commande.

### Procédure OTA

1. **Déposer le binaire** dans `config/www/` de Home Assistant
2. **Accéder** à `http://homeassistant.local:8123/local/zb433.bin`
3. **Déclencher** via EP13 ou modifier l'URL via attribut 0xF001

### Procédure Factory Reset

- Envoyer `ON` sur EP14 → efface NVS + reboot automatique

## Schéma de câblage

Voir le guide détaillé: `CABLAGE_PROD_FS1000A.md` (montage 5V prod‑ready avec 2N2222 open‑collector, inversion déjà gérée par le firmware).

- TX: `GPIO9` → R 1 kΩ → Base 2N2222, Collecteur → DATA FS1000A, Émetteur → GND, Pull‑up DATA 10 kΩ vers +5 V
- Alim: +5 V et GND communs, 100 nF près du FS1000A, 470 µF sur le rail 5 V
- LED: `GPIO8` (WS2812)
- Antenne: fil 17,3 cm (λ/4)

## Dépannage

### Device ne s'inclut pas
- Vérifier que le coordinateur est en "permit join"
- Contrôler le canal Zigbee (11, 15, 20, 25)
- Redémarrer Zigbee2MQTT

### Portes 433MHz ne répondent pas
- Vérifier les clés CAME-24 dans le code
- Ajuster les timings T_SHORT/T_LONG si nécessaire
- Augmenter le nombre de répétitions
- Contrôler l'antenne (17,3 cm)

### OTA ne fonctionne pas
- Vérifier SSID/mot de passe Wi-Fi
- Contrôler que l'URL est joignable
- Consulter les logs série pour plus de détails

## Méthode vendoring (si nécessaire)

Si le Component Manager pose problème :

```bash
# Cloner la librairie Zigbee
git clone --recursive https://github.com/espressif/esp-zigbee-lib.git components/esp-zigbee-lib

# Nettoyer et recompiler
idf.py fullclean
idf.py set-target esp32c6
idf.py build
```

## Licence

Ce projet est sous licence MIT. Voir le fichier [LICENSE](LICENSE) pour plus de détails.