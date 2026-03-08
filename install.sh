#!/usr/bin/env bash
# =============================================================================
# KOSMO OS — Script de Instalación y Build
# Archivo : scripts/install.sh
# Uso     : chmod +x scripts/install.sh && ./scripts/install.sh
#
# Instala todas las dependencias y compila Kosmo OS en:
#   - WSL2 (Windows 10/11)
#   - Ubuntu 20.04 / 22.04 / 24.04
#   - Debian 11/12
# =============================================================================

set -e   # Salir si cualquier comando falla

# ── Colores ───────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

ok()   { echo -e "  ${GREEN}✓${NC}  $*"; }
warn() { echo -e "  ${YELLOW}⚠${NC}  $*"; }
err()  { echo -e "  ${RED}✗${NC}  $*" >&2; }
info() { echo -e "  ${CYAN}→${NC}  $*"; }
hdr()  { echo -e "\n${BOLD}  ── $* ──────────────────────────${NC}"; }

# ── Banner ────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}"
echo "  ██╗  ██╗ ██████╗ ███████╗███╗   ███╗ ██████╗ "
echo "  ██║ ██╔╝██╔═══██╗██╔════╝████╗ ████║██╔═══██╗"
echo "  █████╔╝ ██║   ██║███████╗██╔████╔██║██║   ██║"
echo "  ██╔═██╗ ██║   ██║╚════██║██║╚██╔╝██║██║   ██║"
echo "  ██║  ██╗╚██████╔╝███████║██║ ╚═╝ ██║╚██████╔╝"
echo "  ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝     ╚═╝ ╚═════╝ "
echo ""
echo -e "${NC}${BOLD}  OS Build System — installer v1.0${NC}"
echo ""

# ── Detectar entorno ──────────────────────────────────────────────────────────
hdr "Detecting environment"

IS_WSL=false
IS_MACOS=false
DISTRO="unknown"

if [ -f /proc/version ] && grep -qi microsoft /proc/version; then
    IS_WSL=true
    ok "WSL2 detected"
elif [ "$(uname)" = "Darwin" ]; then
    IS_MACOS=true
    warn "macOS detected (experimental — use Homebrew)"
fi

if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO="$ID"
    ok "Distro: $PRETTY_NAME"
fi

# ── Instalar dependencias ─────────────────────────────────────────────────────
hdr "Installing build dependencies"

if [ "$IS_MACOS" = "true" ]; then
    if ! which brew >/dev/null 2>&1; then
        err "Homebrew not found. Install from https://brew.sh"
        exit 1
    fi
    brew install nasm i686-elf-gcc i686-elf-binutils xorriso \
                 grub qemu 2>/dev/null || true
    ok "macOS packages installed"
elif which apt-get >/dev/null 2>&1; then
    info "Using apt-get..."
    sudo apt-get update -qq

    PKGS="nasm gcc binutils grub-pc-bin grub-common xorriso qemu-system-x86 mtools"

    # En algunos sistemas el paquete de QEMU tiene distinto nombre
    if apt-cache show qemu-system-x86 >/dev/null 2>&1; then
        QEMU_PKG="qemu-system-x86"
    else
        QEMU_PKG="qemu-system-x86_64"
    fi

    sudo apt-get install -y $PKGS 2>/dev/null || {
        warn "Some packages failed. Trying individually..."
        for pkg in $PKGS; do
            sudo apt-get install -y "$pkg" 2>/dev/null \
                && ok "$pkg" \
                || warn "$pkg not found (skipping)"
        done
    }
    ok "APT packages installed"
elif which dnf >/dev/null 2>&1; then
    sudo dnf install -y nasm gcc binutils xorriso qemu-system-x86 grub2-tools
    ok "DNF packages installed"
else
    warn "Unknown package manager. Install manually:"
    warn "  nasm gcc binutils grub-pc-bin xorriso qemu-system-x86"
fi

# ── Verificar herramientas esenciales ─────────────────────────────────────────
hdr "Verifying tools"

check_tool() {
    if which "$1" >/dev/null 2>&1; then
        ok "$1: $(which $1)"
    else
        err "$1: NOT FOUND"
        MISSING_TOOLS="$MISSING_TOOLS $1"
    fi
}

MISSING_TOOLS=""
check_tool nasm
check_tool gcc
check_tool ld
check_tool grub-mkrescue
check_tool qemu-system-i386

if [ -n "$MISSING_TOOLS" ]; then
    warn "Missing tools:$MISSING_TOOLS"
    warn "Build may fail. Install missing tools and try again."
fi

# ── Compilar el sistema operativo ─────────────────────────────────────────────
hdr "Building Kosmo OS"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

info "Project directory: $PROJECT_DIR"

# Verificar que estamos en el directorio correcto
if [ ! -f "Makefile" ] || [ ! -d "kernel" ]; then
    err "Run this script from the kosmo-os project root."
    exit 1
fi

info "Running: make check-syntax"
make check-syntax && ok "Syntax check passed"

info "Running: make all"
time make all -j$(nproc 2>/dev/null || echo 1)

if [ -f "build/kosmo-os.iso" ]; then
    ok "ISO built: build/kosmo-os.iso ($(du -sh build/kosmo-os.iso | cut -f1))"
else
    err "ISO not found after build!"
    exit 1
fi

# ── Instrucciones finales ─────────────────────────────────────────────────────
hdr "Build complete!"

echo ""
echo -e "${BOLD}  Next steps:${NC}"
echo ""
echo -e "  ${GREEN}Run in QEMU:${NC}"
echo "    make run"
echo ""
echo -e "  ${GREEN}Debug with GDB:${NC}"
echo "    make run-debug    # Terminal 1"
echo "    make gdb          # Terminal 2"
echo ""
echo -e "  ${GREEN}Flash to USB (Rufus/Ventoy):${NC}"
echo "    Use: build/kosmo-os.iso"
echo "    Rufus: GPT + UEFI (or MBR + BIOS/Legacy)"
echo "    Ventoy: Copy the .iso to the Ventoy USB drive"
echo ""

if [ "$IS_WSL" = "true" ]; then
    WIN_PATH=$(wslpath -w "$PROJECT_DIR/build/kosmo-os.iso" 2>/dev/null || true)
    if [ -n "$WIN_PATH" ]; then
        echo -e "  ${CYAN}WSL2 path for Windows:${NC}"
        echo "    $WIN_PATH"
    fi
fi

echo ""
