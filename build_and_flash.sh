#!/bin/bash

# Script de build et flash pour ZB433 Router
# Usage: ./build_and_flash.sh <port>
# Exemple: ./build_and_flash.sh /dev/ttyUSB0

set -e

# Vérifier que IDF_PATH est défini
if [ -z "$IDF_PATH" ]; then
    echo "❌ IDF_PATH n'est pas défini. Veuillez sourcer l'environnement ESP-IDF :"
    echo "   source ~/esp-idf/export.sh"
    exit 1
fi

# Vérifier qu'un port est fourni
if [ -z "$1" ]; then
    echo "❌ Usage: $0 <port>"
    echo "   Exemple: $0 /dev/ttyUSB0"
    exit 1
fi

PORT="$1"

echo "🔧 Configuration de la cible ESP32-C6..."
idf.py set-target esp32c6

echo "🔨 Compilation..."
idf.py build

echo "📤 Flash sur $PORT..."
idf.py -p "$PORT" flash monitor
