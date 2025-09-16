# ZB433 - Quick Start Guide

## 🚀 Démarrage rapide

### 1. Installer ESP-IDF v5.2.x
```bash
git clone -b v5.2.2 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf
cd ~/esp-idf && ./install.sh esp32c6
. ~/esp-idf/export.sh
```

### 2. Cloner et configurer le projet
```bash
git clone <votre-repo> zb433
cd zb433
idf.py set-target esp32c6
```

### 3. Compiler et flasher
```bash
# Méthode simple
./build_and_flash.sh

# Ou manuellement
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## 📋 Endpoints Zigbee

| Endpoint | Fonction | Action |
|----------|----------|--------|
| **EP10** | Porte A | Envoie CAME-24 `0x03B29B` |
| **EP11** | Porte B | Envoie CAME-24 `0x03B29A` |
| **EP13** | OTA | Déclenche mise à jour + config URL |
| **EP14** | Reset | Factory Reset (efface NVS) |

## 🔧 Configuration OTA

### Via Zigbee2MQTT
```json
// Configurer URL OTA
{
  "endpoint": 13,
  "cluster": "basic",
  "attribute": "0xF001",
  "manufacturerCode": 4660,
  "value": "http://votre-serveur.local:8080/zb433.bin"
}

// Déclencher OTA
{
  "endpoint": 13,
  "cluster": "onOff",
  "command": "on"
}
```

## 🛠️ Dépannage

### Compilation échoue
```bash
# Nettoyer et reconstruire
idf.py fullclean
idf.py build

# Ou utiliser le vendoring
./setup_vendoring.sh
./test_vendoring.sh
```

### Flash échoue
```bash
# Vérifier le port
ls /dev/ttyUSB* /dev/ttyACM*

# Flasher avec reset
idf.py -p /dev/ttyUSB0 -b 460800 flash monitor
```

## 📁 Fichiers importants

- `main/main.c` - Code source principal
- `sdkconfig.defaults` - Configuration par défaut
- `partitions.csv` - Table de partitions OTA
- `build_and_flash.sh` - Script de build/flash
- `zigbee2mqtt_config.yaml` - Config Zigbee2MQTT
- `node_red_flows.json` - Flows Node-RED

## ✅ Vérification

```bash
./check_setup.sh
```

## 🎯 Prochaines étapes

1. **Flasher** le firmware sur ton ESP32-C6
2. **Inclure** l'appareil dans Zigbee2MQTT
3. **Tester** les endpoints 10 et 11 (portes)
4. **Configurer** l'URL OTA (endpoint 13)
5. **Tester** l'OTA et le Factory Reset

---
**Note** : Ajuste les clés CAME-24 dans `main.c` selon tes portes !
