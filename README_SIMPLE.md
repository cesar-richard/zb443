# ZB433 - Routeur Zigbee ESP32-C6

Routeur Zigbee qui contrôle des portes CAME-24 via 433 MHz.

## 🚀 Démarrage rapide

### 1. Installer ESP-IDF
```bash
git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf
cd ~/esp-idf && ./install.sh esp32c6
. ~/esp-idf/export.sh
```

### 2. Compiler et flasher
```bash
cd zb433
./build.sh
```

## 📋 Endpoints

| Endpoint | Action |
|----------|--------|
| **EP10** | Porte A (CAME-24 `0x03B29B`) |
| **EP11** | Porte B (CAME-24 `0x03B29A`) |
| **EP13** | OTA + config URL |
| **EP14** | Factory Reset |

## 🔧 Configuration

- **GPIO TX** : GPIO4 (modifiable dans `main.c`)
- **Clés CAME-24** : Modifiables dans `main.c` lignes 50-51
- **Wi-Fi** : Configuré dans `sdkconfig.defaults`

## 📁 Fichiers importants

- `main/main.c` - Code source
- `build.sh` - Script de compilation
- `check.sh` - Vérification du projet
- `zigbee2mqtt_config.yaml` - Config Zigbee2MQTT

C'est tout ! 🎉
