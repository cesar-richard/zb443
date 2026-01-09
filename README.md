# ZB433 Router - ESP32-C6 Zigbee + 433MHz CAME-24

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2.x-blue.svg)](https://github.com/espressif/esp-idf)
[![ESP32-C6](https://img.shields.io/badge/ESP32--C6-RISC--V-green.svg)](https://www.espressif.com/en/products/socs/esp32-c6)
[![Zigbee](https://img.shields.io/badge/Zigbee-3.0-orange.svg)](https://zigbeealliance.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Vue d'ensemble

Le **ZB433 Router** est un routeur Zigbee ESP32-C6 capable de contrôler des portes CAME-24 via 433MHz. Il agit comme un pont entre votre réseau Zigbee (Zigbee2MQTT/Home Assistant) et vos équipements CAME-24 existants.

### Fonctionnalités

- **Routeur Zigbee 3.0** : Compatible avec Zigbee2MQTT
- **Contrôle 433MHz CAME-24** : Deux boutons (Portail Principal et Portail Parking)
- **Identify** : Cluster 0x0003 avec effet LED (breathing)
- **Indicateur LED** : WS2812 avec couleurs distinctes par endpoint
- **2 Endpoints** : EP1 (Portail Principal), EP2 (Portail Parking)
- **Auto-reset** : L'attribut on_off se remet à OFF après 5 secondes (debounce)

## Installation

### Prérequis

- **ESP-IDF v5.2.x** installé et configuré
- **ESP32-C6 DevKit** (alimentation via USB)
- **Emetteur 433MHz FS1000A** (ASK/OOK)
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

## Utilisation

### Endpoints disponibles

| Endpoint | Fonction | Code 433MHz |
|----------|----------|-------------|
| **EP1** | Portail Principal | 0x03B29B |
| **EP2** | Portail Parking | 0x03B29A |

### Clusters Zigbee par endpoint

- **Basic (0x0000)** : Manufacturer "Cesar RICHARD EI", Model "ZB433-Router"
- **On/Off (0x0006)** : Controle du portail (ON = envoi 433MHz)
- **Identify (0x0003)** : Identification visuelle via LED

### Exemples MQTT

```bash
# Ouvrir Portail Principal (EP1)
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/1/set" -m '{"state":"ON"}'

# Ouvrir Portail Parking (EP2)
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/2/set" -m '{"state":"ON"}'

# Identify via attribut (EP1) - LED breathing pendant 5 secondes
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/1/set" -m '{"identify":5}'
```

### Feedback LED

| Etat | Couleur |
|------|---------|
| Demarrage | Rouge |
| Attente connexion | Orange |
| Connecte au reseau | LED eteinte |
| Commande EP1 | Bleu-cyan |
| Commande EP2 | Magenta |
| Identify | Breathing blanc-bleu |

## Schema de cablage

### GPIO Assignments

| GPIO | Fonction | Details |
|------|----------|---------|
| **GPIO4** | 433MHz TX | Driver high-side NPN+PNP |
| **GPIO8** | WS2812 LED | Indicateur RGB |

### Circuit 433MHz

Voir le guide detaille: `CABLAGE_PROD_FS1000A.md` (montage 5V prod-ready avec 2N2222 open-collector).

- **TX** : `GPIO4` → R 1 kOhm → Base 2N2222, Collecteur → DATA FS1000A, Emetteur → GND
- **Pull-up** : DATA 10 kOhm vers +5V
- **Alim** : +5V et GND communs, 100nF pres du FS1000A, 470uF sur le rail 5V
- **LED** : `GPIO8` (WS2812)
- **Antenne** : fil 17,3 cm (lambda/4)

## Configuration Zigbee

### Parametres du routeur

- **Type** : Router (ESP_ZB_DEVICE_TYPE_ROUTER)
- **Canaux** : Scan tous les canaux (ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK)
- **Profil** : Home Automation (0x0104)
- **Max children** : 10

### Inclusion au reseau

1. Mettre le coordinateur en mode "permit join"
2. L'appareil s'inclut automatiquement et expose 2 endpoints
3. Si l'inclusion echoue, le device reessaie automatiquement apres 1 seconde

## Depannage

### Device ne s'inclut pas

- Verifier que le coordinateur est en "permit join"
- Controler le canal Zigbee (le device scanne tous les canaux)
- Redemarrer Zigbee2MQTT
- Eteindre les autres routeurs Zigbee orphelins qui peuvent interferer

### Portes 433MHz ne repondent pas

- Verifier les cles CAME-24 dans le code (`came433.h`)
- Controler l'antenne (17,3 cm)
- Verifier le cablage du driver NPN (GPIO4 doit etre LOW au repos)
- Augmenter le nombre de repetitions (CAME_REPEATS dans `came433.h`)

### LED ne s'allume pas

- Verifier le cablage de la LED WS2812 sur GPIO8
- Controler l'alimentation 5V de la LED

## Architecture du code

```
main/
├── main.c        # Point d'entree, initialisation
├── zigbee.c/h    # Stack Zigbee, signal handler, action handlers
├── endpoints.c/h # Creation des endpoints, gestion des commandes
├── came433.c/h   # Protocole CAME-24 via RMT
└── led.c/h       # Controle LED WS2812
```

## Methode vendoring (si necessaire)

Si le Component Manager pose probleme :

```bash
# Cloner la librairie Zigbee
git clone --recursive https://github.com/espressif/esp-zigbee-lib.git components/esp-zigbee-lib

# Nettoyer et recompiler
idf.py fullclean
idf.py set-target esp32c6
idf.py build
```

## Licence

Ce projet est sous licence MIT. Voir le fichier [LICENSE](LICENSE) pour plus de details.
