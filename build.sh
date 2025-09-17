#!/bin/bash
# build_and_flash.sh - Script pour compiler et flasher ZB433

set -e  # Arrêter en cas d'erreur

# Configuration
PROJECT_NAME="zb433"
DEFAULT_PORT="/dev/ttyUSB0"
BAUD_RATE="460800"

# Couleurs pour les logs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Fonction de log
log() {
    echo -e "${BLUE}[$(date +'%H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Vérifier si ESP-IDF est configuré
check_idf() {
    if [ -z "$IDF_PATH" ]; then
        error "ESP-IDF n'est pas configuré. Exécutez: . ~/esp-idf/export.sh"
        exit 1
    fi
    
    log "ESP-IDF détecté: $IDF_PATH"
    idf.py --version
}

# Détecter le port série automatiquement
detect_port() {
    local ports=()
    
    # Linux
    if [ -d "/dev" ]; then
        for port in /dev/ttyUSB* /dev/ttyACM*; do
            if [ -e "$port" ]; then
                ports+=("$port")
            fi
        done
    fi
    
    # macOS
    if [ -d "/dev" ] && command -v ls /dev/cu.* >/dev/null 2>&1; then
        for port in /dev/cu.usbserial* /dev/cu.usbmodem*; do
            if [ -e "$port" ]; then
                ports+=("$port")
            fi
        done
    fi
    
    if [ ${#ports[@]} -eq 0 ]; then
        warning "Aucun port série détecté. Utilisation du port par défaut: $DEFAULT_PORT"
        echo "$DEFAULT_PORT"
    elif [ ${#ports[@]} -eq 1 ]; then
        log "Port détecté automatiquement: ${ports[0]}"
        echo "${ports[0]}"
    else
        log "Plusieurs ports détectés:"
        for i in "${!ports[@]}"; do
            echo "  $((i+1)). ${ports[i]}"
        done
        
        read -p "Choisissez un port (1-${#ports[@]}) ou appuyez sur Entrée pour $DEFAULT_PORT: " choice
        
        if [ -z "$choice" ]; then
            echo "$DEFAULT_PORT"
        elif [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le ${#ports[@]} ]; then
            echo "${ports[$((choice-1))]}"
        else
            warning "Choix invalide. Utilisation du port par défaut: $DEFAULT_PORT"
            echo "$DEFAULT_PORT"
        fi
    fi
}

# Nettoyer le projet
clean_project() {
    log "Nettoyage du projet..."
    idf.py fullclean
    success "Nettoyage terminé"
}

# Configurer le target
set_target() {
    log "Configuration du target ESP32-C6..."
    idf.py set-target esp32c6
    success "Target configuré"
}

# Compiler le projet
build_project() {
    log "Compilation du projet..."
    
    # Vérifier les dépendances
    if [ ! -f "main/idf_component.yml" ]; then
        error "Fichier idf_component.yml manquant"
        exit 1
    fi
    
    # Compiler
    if idf.py build; then
        success "Compilation réussie"
    else
        error "Échec de la compilation"
        exit 1
    fi
}

# Flasher le projet
flash_project() {
    local port="$1"
    
    log "Flash du firmware sur $port..."
    
    if idf.py -p "$port" -b "$BAUD_RATE" flash; then
        success "Flash réussi"
    else
        error "Échec du flash"
        exit 1
    fi
}

# Monitorer les logs
monitor_project() {
    local port="$1"
    
    log "Démarrage du monitoring sur $port..."
    log "Appuyez sur Ctrl+C pour arrêter le monitoring"
    echo
    
    idf.py -p "$port" monitor
}

# Fonction principale
main() {
    echo "=========================================="
    echo "  ZB433 - Build & Flash Script"
    echo "  Routeur Zigbee ESP32-C6 CAME-24"
    echo "=========================================="
    echo
    
    # Vérifier ESP-IDF
    check_idf
    echo
    
    # Détecter le port
    PORT=$(detect_port)
    log "Port sélectionné: $PORT"
    echo
    
    # Menu interactif
    echo "Que voulez-vous faire ?"
    echo "1. Compiler seulement"
    echo "2. Compiler et flasher"
    echo "3. Compiler, flasher et monitorer"
    echo "4. Nettoyer et recompiler tout"
    echo "5. Monitorer seulement"
    echo
    
    read -p "Votre choix (1-5) [3]: " choice
    choice=${choice:-3}
    
    case $choice in
        1)
            # set_target
            build_project
            ;;
        2)
            set_target
            build_project
            flash_project "$PORT"
            ;;
        3)
            set_target
            build_project
            flash_project "$PORT"
            monitor_project "$PORT"
            ;;
        4)
            clean_project
            set_target
            build_project
            flash_project "$PORT"
            monitor_project "$PORT"
            ;;
        5)
            monitor_project "$PORT"
            ;;
        *)
            error "Choix invalide"
            exit 1
            ;;
    esac
    
    success "Script terminé"
}

# Gestion des arguments en ligne de commande
if [ $# -gt 0 ]; then
    case "$1" in
        --help|-h)
            echo "Usage: $0 [options]"
            echo
            echo "Options:"
            echo "  --help, -h     Afficher cette aide"
            echo "  --port PORT    Spécifier le port série"
            echo "  --clean        Nettoyer avant compilation"
            echo "  --build-only   Compiler seulement"
            echo "  --flash-only   Flasher seulement (nécessite un build préalable)"
            echo
            exit 0
            ;;
        --port)
            if [ -z "$2" ]; then
                error "Port manquant après --port"
                exit 1
            fi
            PORT="$2"
            shift 2
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --build-only)
            # check_idf
            # set_target
            # if [ "$CLEAN" = true ]; then
            #     clean_project
            # fi
            build_project
            exit 0
            ;;
        --flash-only)
            check_idf
            PORT=$(detect_port)
            flash_project "$PORT"
            exit 0
            ;;
        *)
            error "Option inconnue: $1"
            echo "Utilisez --help pour voir les options disponibles"
            exit 1
            ;;
    esac
fi

# Exécuter le script principal
main
