#!/bin/bash
# check_setup.sh - Vérification de la configuration du projet ZB433

set -e

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Compteurs
TOTAL_CHECKS=0
PASSED_CHECKS=0

check() {
    local description="$1"
    local command="$2"
    local expected="$3"
    
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    
    echo -n "🔍 $description... "
    
    if eval "$command" >/dev/null 2>&1; then
        echo -e "${GREEN}✅ OK${NC}"
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        return 0
    else
        echo -e "${RED}❌ FAIL${NC}"
        return 1
    fi
}

echo "=========================================="
echo "  ZB433 - Vérification de la configuration"
echo "=========================================="
echo

# Vérifications de base
echo "📁 Structure du projet :"
check "CMakeLists.txt existe" "[ -f CMakeLists.txt ]"
check "idf_component.yml existe" "[ -f idf_component.yml ]"
check "partitions.csv existe" "[ -f partitions.csv ]"
check "sdkconfig.defaults existe" "[ -f sdkconfig.defaults ]"
check "main/CMakeLists.txt existe" "[ -f main/CMakeLists.txt ]"
check "main/main.c existe" "[ -f main/main.c ]"
check "README.md existe" "[ -f README.md ]"
check "build.sh existe" "[ -f build.sh ]"
check "build.sh exécutable" "[ -x build.sh ]"

echo
echo "🔧 Configuration ESP-IDF :"

# Vérifier ESP-IDF
if [ -n "$IDF_PATH" ]; then
    check "IDF_PATH défini" "true"
    check "ESP-IDF installé" "[ -d \"$IDF_PATH\" ]"
    check "export.sh existe" "[ -f \"$IDF_PATH/export.sh\" ]"
    
    # Charger ESP-IDF temporairement pour les tests
    if [ -f "$IDF_PATH/export.sh" ]; then
        source "$IDF_PATH/export.sh" >/dev/null 2>&1
        check "idf.py disponible" "command -v idf.py"
        check "Version ESP-IDF >= 5.2" "idf.py --version | grep -q 'v5\.[2-9]'"
    fi
else
    echo -e "⚠️  ${YELLOW}IDF_PATH non défini - chargez ESP-IDF avec: . ~/esp-idf/export.sh${NC}"
fi

echo
echo "📦 Dépendances du projet :"

# Vérifier les dépendances dans idf_component.yml
if [ -f "idf_component.yml" ]; then
    check "esp-zigbee-lib dans idf_component.yml" "grep -q 'esp-zigbee-lib' idf_component.yml"
    check "Version IDF >= 5.2.0" "grep -q 'idf.*>=5.2.0' idf_component.yml"
fi

# Vérifier la configuration CMake
if [ -f "main/CMakeLists.txt" ]; then
    check "esp-zigbee-lib dans CMakeLists.txt" "grep -q 'esp-zigbee-lib' main/CMakeLists.txt"
    check "nvs_flash requis" "grep -q 'nvs_flash' main/CMakeLists.txt"
    check "esp_wifi requis" "grep -q 'esp_wifi' main/CMakeLists.txt"
    check "esp_https_ota requis" "grep -q 'esp_https_ota' main/CMakeLists.txt"
fi

echo
echo "⚙️  Configuration du projet :"

# Vérifier sdkconfig.defaults
if [ -f "sdkconfig.defaults" ]; then
    check "Target ESP32-C6" "grep -q 'CONFIG_IDF_TARGET=\"esp32c6\"' sdkconfig.defaults"
    check "Zigbee activé" "grep -q 'CONFIG_ZB_ENABLED=y' sdkconfig.defaults"
    check "Mode Router" "grep -q 'CONFIG_ZB_ROLE_ROUTER=y' sdkconfig.defaults"
    check "Wi-Fi activé" "grep -q 'CONFIG_ESP_WIFI_ENABLED=y' sdkconfig.defaults"
    check "OTA HTTP autorisé" "grep -q 'CONFIG_OTA_ALLOW_HTTP=y' sdkconfig.defaults"
fi

# Vérifier partitions.csv
if [ -f "partitions.csv" ]; then
    check "Partition OTA configurée" "grep -q 'ota_0' partitions.csv"
    check "Partition NVS configurée" "grep -q 'nvs' partitions.csv"
fi

echo
echo "📝 Code source :"

# Vérifier le code source
if [ -f "main/main.c" ]; then
    check "Endpoints définis" "grep -q 'EP_OPEN.*10' main/main.c"
    check "Clés CAME-24 définies" "grep -q 'KEY_A.*0x03B29B' main/main.c"
    check "Fonction OTA implémentée" "grep -q 'do_ota_sequence' main/main.c"
    check "Factory Reset implémenté" "grep -q 'action_factory_reset' main/main.c"
    check "GPIO TX configuré" "grep -q 'TX433_GPIO' main/main.c"
fi

echo
echo "📚 Documentation et scripts :"

# Vérifier les fichiers de documentation
check "README.md non vide" "[ -s README.md ]"
check "Configuration Zigbee2MQTT" "[ -f zigbee2mqtt_config.yaml ]"
check "README simple" "[ -f README_SIMPLE.md ]"

echo
echo "=========================================="
echo "  Résumé de la vérification"
echo "=========================================="

if [ $PASSED_CHECKS -eq $TOTAL_CHECKS ]; then
    echo -e "${GREEN}🎉 Tous les tests sont passés ! ($PASSED_CHECKS/$TOTAL_CHECKS)${NC}"
    echo
    echo "✅ Le projet est prêt pour la compilation"
    echo "🚀 Utilisez: ./build.sh"
    exit 0
else
    echo -e "${RED}❌ $((TOTAL_CHECKS - PASSED_CHECKS)) test(s) ont échoué ($PASSED_CHECKS/$TOTAL_CHECKS)${NC}"
    echo
    echo "🔧 Corrigez les erreurs avant de continuer"
    exit 1
fi
