#!/usr/bin/env bash
set -e

# ==============================================================================
# Zugfish - Modern Chess.com-Style Game Reviewer & Analysis Suite
# Universal Installation Script for Linux & macOS
# ==============================================================================

BOLD="\033[1m"
GREEN="\033[0;32m"
YELLOW="\033[0;33m"
CYAN="\033[0;36m"
RED="\033[0;31m"
RESET="\033[0m"

log_info()  { echo -e "${CYAN}[zugfish]${RESET} $*"; }
log_ok()    { echo -e "${GREEN}[zugfish]${RESET} $*"; }
log_warn()  { echo -e "${YELLOW}[zugfish:warn]${RESET} $*"; }
log_error() { echo -e "${RED}[zugfish:error]${RESET} $*"; }

INSTALL_DEPS=true
BUILD_GUI=true
BUILD_CLI=true
PREFIX="${HOME}/.local"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-deps)
            INSTALL_DEPS=false
            shift
            ;;
        --cli-only)
            BUILD_GUI=false
            shift
            ;;
        --gui-only)
            BUILD_CLI=false
            shift
            ;;
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        --system)
            PREFIX="/usr/local"
            shift
            ;;
        -h|--help)
            echo "Zugfish Installation Script"
            echo "Usage: ./install.sh [options]"
            echo ""
            echo "Options:"
            echo "  --prefix <dir>   Installation directory prefix (default: ~/.local)"
            echo "  --system         Install system-wide to /usr/local (requires sudo/root)"
            echo "  --no-deps        Skip automatic package manager dependency installation"
            echo "  --cli-only       Build and install only the zugfish-cli binary"
            echo "  --gui-only       Build and install only the zugfish-qt GUI binary"
            echo "  -h, --help       Show this help message"
            exit 0
            ;;
        *)
            log_error "Unknown argument: $1"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

log_info "${BOLD}Starting Zugfish installation...${RESET}"
log_info "Target install prefix: ${PREFIX}"

# 1. Dependency Resolution
install_packages() {
    if [[ "${INSTALL_DEPS}" != "true" ]]; then
        log_info "Skipping automatic dependency check (--no-deps specified)."
        return 0
    fi

    log_info "Detecting operating system and package manager..."

    if [[ "$OSTYPE" == "darwin"* ]]; then
        if command -v brew >/dev/null 2>&1; then
            log_info "Installing dependencies via Homebrew..."
            brew install cmake ninja stockfish curl nlohmann-json
            if [[ "${BUILD_GUI}" == "true" ]]; then
                brew install qt@6
            fi
        else
            log_warn "Homebrew not found. Please ensure CMake, Stockfish, libcurl, nlohmann-json (and Qt6 for GUI) are installed."
        fi
        return 0
    fi

    # Linux distro detection
    if [[ -f /etc/arch-release ]]; then
        log_info "Arch Linux detected. Installing dependencies via pacman..."
        local PKGS="cmake ninja gcc curl nlohmann-json"
        if [[ "${BUILD_GUI}" == "true" ]]; then
            PKGS="${PKGS} qt6-base qt6-declarative qt6-svg"
        fi
        if command -v sudo >/dev/null 2>&1 && [[ $EUID -ne 0 ]]; then
            sudo pacman -S --needed --noconfirm ${PKGS}
        else
            pacman -S --needed --noconfirm ${PKGS}
        fi

        # Stockfish is in AUR on Arch Linux
        if ! command -v stockfish >/dev/null 2>&1; then
            if command -v yay >/dev/null 2>&1; then
                log_info "Installing Stockfish via yay (AUR)..."
                yay -S --needed --noconfirm stockfish || true
            elif command -v paru >/dev/null 2>&1; then
                log_info "Installing Stockfish via paru (AUR)..."
                paru -S --needed --noconfirm stockfish || true
            else
                log_warn "Stockfish is not in official Arch repos (available in AUR as 'stockfish')."
                log_warn "Install it via AUR: 'yay -S stockfish' or 'paru -S stockfish', or download from https://stockfishchess.org"
            fi
        fi

    elif [[ -f /etc/debian_version ]]; then
        log_info "Debian/Ubuntu detected. Installing dependencies via apt..."
        local PKGS="build-essential cmake ninja-build libcurl4-openssl-dev nlohmann-json3-dev stockfish"
        if [[ "${BUILD_GUI}" == "true" ]]; then
            PKGS="${PKGS} qt6-base-dev qt6-declarative-dev libqt6svg6-dev qml6-module-qtquick qml6-module-qtquick-controls2 qml6-module-qtquick-layouts qml6-module-qtquick-templates2 qml6-module-qtquick-shapes"
        fi
        if command -v sudo >/dev/null 2>&1 && [[ $EUID -ne 0 ]]; then
            sudo apt-get update && sudo apt-get install -y ${PKGS}
        else
            apt-get update && apt-get install -y ${PKGS}
        fi

    elif [[ -f /etc/fedora-release ]] || [[ -f /etc/redhat-release ]]; then
        log_info "Fedora/RHEL detected. Installing dependencies via dnf..."
        local PKGS="cmake ninja-build gcc-c++ libcurl-devel nlohmann-json-devel stockfish"
        if [[ "${BUILD_GUI}" == "true" ]]; then
            PKGS="${PKGS} qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel"
        fi
        if command -v sudo >/dev/null 2>&1 && [[ $EUID -ne 0 ]]; then
            sudo dnf install -y ${PKGS}
        else
            dnf install -y ${PKGS}
        fi

    elif [[ -f /etc/alpine-release ]]; then
        log_info "Alpine Linux detected. Installing dependencies via apk..."
        local PKGS="build-base cmake ninja curl-dev nlohmann-json stockfish"
        if [[ "${BUILD_GUI}" == "true" ]]; then
            PKGS="${PKGS} qt6-qtbase-dev qt6-qtdeclarative-dev qt6-qtsvg-dev"
        fi
        if command -v sudo >/dev/null 2>&1 && [[ $EUID -ne 0 ]]; then
            sudo apk add ${PKGS}
        else
            apk add ${PKGS}
        fi

    else
        log_warn "Unknown Linux distribution. Skipping automatic package installation."
        log_warn "Please ensure CMake, C++20 compiler, libcurl, nlohmann-json, stockfish (and Qt6) are installed."
    fi
}

install_packages

# 2. Configure & Build
log_info "Configuring CMake project..."

CMAKE_GENERATOR="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR="Unix Makefiles"
fi

CMAKE_FLAGS=(
    -DCMAKE_BUILD_TYPE=Release
    -G "${CMAKE_GENERATOR}"
    -DBUILD_GUI=$([[ "${BUILD_GUI}" == "true" ]] && echo "ON" || echo "OFF")
    -DBUILD_CLI=$([[ "${BUILD_CLI}" == "true" ]] && echo "ON" || echo "OFF")
)

# On macOS with Homebrew Qt6, supply CMAKE_PREFIX_PATH if needed
if [[ "$OSTYPE" == "darwin"* ]]; then
    if [[ -d "/opt/homebrew/opt/qt@6" ]]; then
        CMAKE_FLAGS+=(-DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6")
    elif [[ -d "/usr/local/opt/qt@6" ]]; then
        CMAKE_FLAGS+=(-DCMAKE_PREFIX_PATH="/usr/local/opt/qt@6")
    fi
fi

cmake -B build "${CMAKE_FLAGS[@]}"

log_info "Compiling Zugfish..."
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build build --config Release -j"${NPROC}"

# 3. Installation to Destination
log_info "Installing binaries and desktop resources to ${PREFIX}..."

mkdir -p "${PREFIX}/bin"

if [[ "${BUILD_CLI}" == "true" && -f "build/zugfish-cli" ]]; then
    install -m 755 "build/zugfish-cli" "${PREFIX}/bin/zugfish-cli"
    log_ok "Installed CLI: ${PREFIX}/bin/zugfish-cli"
fi

if [[ "${BUILD_GUI}" == "true" && -f "build/gui/zugfish-qt" ]]; then
    install -m 755 "build/gui/zugfish-qt" "${PREFIX}/bin/zugfish-qt"
    ln -sf "${PREFIX}/bin/zugfish-qt" "${PREFIX}/bin/zugfish"
    log_ok "Installed GUI: ${PREFIX}/bin/zugfish-qt (symlinked as ${PREFIX}/bin/zugfish)"

    # Desktop entry & Icon (Linux only)
    if [[ "$OSTYPE" != "darwin"* ]]; then
        mkdir -p "${PREFIX}/share/applications"
        mkdir -p "${PREFIX}/share/icons/hicolor/scalable/apps"

        # Install icon
        if [[ -f "gui/assets/pieces/wK.svg" ]]; then
            cp "gui/assets/pieces/wK.svg" "${PREFIX}/share/icons/hicolor/scalable/apps/zugfish.svg"
        fi

        # Install .desktop launcher
        cat > "${PREFIX}/share/applications/zugfish.desktop" <<EOF
[Desktop Entry]
Name=Zugfish
GenericName=Chess Reviewer
Comment=Modern Chess.com-style Game Review & Analysis GUI
Exec=${PREFIX}/bin/zugfish
Icon=zugfish
Terminal=false
Type=Application
Categories=Game;BoardGame;
Keywords=chess;review;stockfish;analysis;lichess;
StartupWMClass=zugfish-qt
EOF
        chmod 644 "${PREFIX}/share/applications/zugfish.desktop"
        log_ok "Installed desktop launcher: ${PREFIX}/share/applications/zugfish.desktop"

        if command -v update-desktop-database >/dev/null 2>&1; then
            update-desktop-database "${PREFIX}/share/applications" 2>/dev/null || true
        fi
        if command -v gtk-update-icon-cache >/dev/null 2>&1; then
            gtk-update-icon-cache -f -t "${PREFIX}/share/icons/hicolor" 2>/dev/null || true
        fi
    fi
fi

# 4. Engine verification
echo ""
if command -v stockfish >/dev/null 2>&1; then
    SF_PATH=$(command -v stockfish)
    log_ok "Detected Stockfish at: ${SF_PATH}"
else
    log_warn "Stockfish was not found in your standard PATH."
    log_warn "Please install Stockfish or configure its path inside Zugfish settings."
fi

# 5. PATH check
if [[ ":$PATH:" != *":${PREFIX}/bin:"* ]]; then
    echo ""
    log_warn "${PREFIX}/bin is not in your current PATH."
    log_warn "Add it to your ~/.bashrc or ~/.zshrc:"
    echo -e "    ${BOLD}export PATH=\"${PREFIX}/bin:\$PATH\"${RESET}"
fi

echo ""
echo -e "${GREEN}${BOLD}======================================================${RESET}"
echo -e "${GREEN}${BOLD}     Zugfish has been successfully installed!         ${RESET}"
echo -e "${GREEN}${BOLD}======================================================${RESET}"
echo ""
echo "Run the application:"
if [[ "${BUILD_GUI}" == "true" ]]; then
    echo -e "  • GUI:  ${CYAN}zugfish${RESET} (or ${CYAN}zugfish-qt${RESET})"
fi
if [[ "${BUILD_CLI}" == "true" ]]; then
    echo -e "  • CLI:  ${CYAN}zugfish-cli --pgn path/to/game.pgn${RESET}"
fi
echo ""
