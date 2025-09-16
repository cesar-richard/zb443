# ZB433 - Routeur Zigbee ESP32-C6 avec CAME-24 et OTA

Routeur Zigbee ESP32-C6 qui expose 4 endpoints pour contrôler des portes CAME-24 via 433 MHz et gérer les mises à jour OTA.

## Fonctionnalités

- **EP10** : Switch On/Off → Envoie trame 433 MHz CAME-24 (clé `0x03B29B`) pour porte A
- **EP11** : Switch On/Off → Envoie trame 433 MHz CAME-24 (clé `0x03B29A`) pour porte B  
- **EP13** : Switch On/Off → Déclenche séquence OTA (firmware via Wi-Fi/HTTP)
- **EP14** : Switch On/Off → Factory Reset (efface NVS, reboot, repart en inclusion)

## Hardware

- **MCU** : ESP32-C6
- **TX 433 MHz** : GPIO4 (modifiable dans `main.c`)
- **Alimentation** : 3.3V (via USB ou alimentation externe)

## Installation ESP-IDF

### 1. Installer ESP-IDF v5.2.x

```bash
# Cloner ESP-IDF
git clone -b v5.2.2 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf

# Installer les outils
cd ~/esp-idf
./install.sh esp32c6

# Configurer l'environnement (à faire à chaque session)
. ~/esp-idf/export.sh
```

### 2. Vérifier l'installation

```bash
idf.py --version
# Devrait afficher : ESP-IDF v5.2.2
```

## Configuration du projet

### 1. Cloner le projet

```bash
git clone <votre-repo> zb433
cd zb433
```

### 2. Configurer le target

```bash
idf.py set-target esp32c6
```

### 3. Configurer Wi-Fi et OTA (optionnel)

```bash
idf.py menuconfig
```

Ou modifier directement `sdkconfig.defaults` :

```ini
# Wi-Fi credentials
CONFIG_ZB433_WIFI_SSID="VotreSSID"
CONFIG_ZB433_WIFI_PASS="VotreMotDePasse"

# Default OTA URL
CONFIG_ZB433_DEFAULT_OTA_URL="http://homeassistant.local:8123/local/zb433.bin"
```

## Compilation et Flash

### Méthode 1 : Commandes manuelles

```bash
# Compiler
idf.py build

# Flasher (remplacer /dev/ttyUSB0 par votre port)
idf.py -p /dev/ttyUSB0 flash monitor

# Ou en une seule commande
idf.py -p /dev/ttyUSB0 build flash monitor
```

### Méthode 2 : Script automatisé

```bash
# Rendre exécutable
chmod +x build_and_flash.sh

# Exécuter (modifier le port dans le script si nécessaire)
./build_and_flash.sh
```

## Utilisation

### 1. Inclusion Zigbee

1. Mettre l'appareil sous tension
2. L'appareil démarre automatiquement en mode inclusion
3. Dans Zigbee2MQTT, cliquer sur "Permit join" 
4. L'appareil apparaîtra avec 4 endpoints

### 2. Contrôle des portes

- **EP10** : Envoie `ON` → Porte A s'ouvre (clé CAME-24 `0x03B29B`)
- **EP11** : Envoie `ON` → Porte B s'ouvre (clé CAME-24 `0x03B29A`)

### 3. Mise à jour OTA (EP13)

#### a) Configurer l'URL OTA

Envoyer une commande ZCL Write Attribute sur EP13, cluster Basic :

```json
{
  "endpoint": 13,
  "cluster": "basic",
  "attribute": "0xF001",
  "manufacturerCode": 4660,
  "value": "http://votre-serveur.local:8080/zb433.bin"
}
```

#### b) Déclencher l'OTA

Envoyer `ON` sur EP13 :

```json
{
  "endpoint": 13,
  "cluster": "onOff",
  "command": "on"
}
```

L'appareil va :
1. Se connecter au Wi-Fi
2. Télécharger le firmware depuis l'URL configurée
3. Redémarrer avec le nouveau firmware

### 4. Factory Reset (EP14)

Envoyer `ON` sur EP14 :

```json
{
  "endpoint": 14,
  "cluster": "onOff", 
  "command": "on"
}
```

L'appareil va :
1. Effacer toute la NVS
2. Redémarrer
3. Repartir en mode inclusion Zigbee

## Configuration Zigbee2MQTT

### 1. Configuration de base

```yaml
# configuration.yaml
zigbee2mqtt:
  # ... autres configs ...
  devices:
    '0x1234567890abcdef':  # Remplacer par l'IEEE de votre appareil
      friendly_name: 'ZB433-Router'
      description: 'Routeur Zigbee ESP32-C6 CAME-24'
      retain: false
```

### 2. Automations pour OTA

```yaml
# automations.yaml
- alias: 'ZB433 - Set OTA URL'
  trigger:
    platform: mqtt
    topic: 'zigbee2mqtt/ZB433-Router/set'
  action:
    - service: mqtt.publish
      data:
        topic: 'zigbee2mqtt/ZB433-Router/13/basic/set'
        payload: >
          {
            "0xF001": "http://homeassistant.local:8123/local/zb433.bin",
            "manufacturerCode": 4660
          }

- alias: 'ZB433 - Trigger OTA'
  trigger:
    platform: state
    entity_id: input_boolean.zb433_ota_trigger
    to: 'on'
  action:
    - service: mqtt.publish
      data:
        topic: 'zigbee2mqtt/ZB433-Router/13/set'
        payload: '{"state": "ON"}'
    - service: input_boolean.turn_off
      entity_id: input_boolean.zb433_ota_trigger

- alias: 'ZB433 - Factory Reset'
  trigger:
    platform: state
    entity_id: input_boolean.zb433_factory_reset
    to: 'on'
  action:
    - service: mqtt.publish
      data:
        topic: 'zigbee2mqtt/ZB433-Router/14/set'
        payload: '{"state": "ON"}'
    - service: input_boolean.turn_off
      entity_id: input_boolean.zb433_factory_reset
```

### 3. Interface Lovelace

```yaml
# ui-lovelace.yaml
type: entities
title: ZB433 Router
entities:
  - entity: zigbee2mqtt.zb433_router_10
    name: Porte A
  - entity: zigbee2mqtt.zb433_router_11  
    name: Porte B
  - entity: input_boolean.zb433_ota_trigger
    name: Déclencher OTA
  - entity: input_boolean.zb433_factory_reset
    name: Factory Reset
```

## Dépannage

### Problème de compilation

Si `idf.py build` échoue avec "esp-zigbee-lib not found" :

```bash
# Méthode 1 : Nettoyer et reconstruire
idf.py fullclean
idf.py build

# Méthode 2 : Forcer le téléchargement des composants
rm -rf build/
idf.py reconfigure
idf.py build
```

### Problème de flash

```bash
# Vérifier le port série
ls /dev/ttyUSB* /dev/ttyACM*

# Flasher avec reset
idf.py -p /dev/ttyUSB0 -b 460800 flash monitor
```

### Problème Wi-Fi OTA

1. Vérifier les credentials dans `sdkconfig.defaults`
2. Vérifier que l'URL OTA est accessible
3. Consulter les logs : `idf.py monitor`

## Structure du projet

```
zb433/
├── CMakeLists.txt              # Configuration CMake principale
├── idf_component.yml           # Dépendances Component Manager
├── partitions.csv              # Table de partitions OTA
├── sdkconfig.defaults          # Configuration par défaut
├── build_and_flash.sh          # Script de build/flash
├── README.md                   # Ce fichier
└── main/
    ├── CMakeLists.txt          # Configuration composant main
    └── main.c                  # Code source principal
```

## Personnalisation

### Modifier les clés CAME-24

Dans `main.c`, lignes 50-51 :

```c
#define KEY_A  0x03B29B  // porte A (03 B2 9B)
#define KEY_B  0x03B29A  // porte B (03 B2 9A)
```

### Modifier le GPIO TX

Dans `main.c`, ligne 26 :

```c
#define TX433_GPIO GPIO_NUM_4   // DATA vers TX433
```

### Modifier les timings CAME-24

Dans `main.c`, lignes 54-56 :

```c
#define T_SHORT   350    // µs
#define T_LONG   1050    // µs  
#define GAP_US   8000    // µs entre répétitions
```

## Licence

MIT License - Voir le fichier LICENSE pour plus de détails.
