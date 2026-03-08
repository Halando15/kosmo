#!/usr/bin/env bash
# =============================================================================
# KOSMO OS — Launcher QEMU Interactivo
# Archivo : scripts/run.sh
# Uso     : ./scripts/run.sh [modo]
#   Modos: gui | text | debug | headless | vbox | usb
# =============================================================================

set -e

CYAN='\033[0;36m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
RED='\033[0;31m'; BOLD='\033[1m'; NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

ISO="build/kosmo-os.iso"
IMG="build/kosmo-os.img"

QEMU="qemu-system-i386"
BASE_FLAGS="-m 128M -no-reboot -name 'Kosmo OS' -serial stdio"

# ── Verificar que existe la ISO ───────────────────────────────────────────────
if [ ! -f "$ISO" ]; then
    echo -e "  ${RED}✗${NC}  ISO not found: $ISO"
    echo "  Run: make all"
    exit 1
fi

# ── Selector de modo ──────────────────────────────────────────────────────────
MODE="${1:-menu}"

if [ "$MODE" = "menu" ]; then
    echo ""
    echo -e "${BOLD}${CYAN}  Kosmo OS — QEMU Launcher${NC}"
    echo "  ─────────────────────────────────"
    echo "  1) GUI mode    (800x600, VGA std)"
    echo "  2) Text mode   (Terminal 80x25)"
    echo "  3) Debug mode  (GDB on :1234)"
    echo "  4) Headless    (serial only)"
    echo "  5) Raw image   (disk boot)"
    echo "  6) VirtualBox  (convert to VMDK)"
    echo "  0) Exit"
    echo ""
    printf "  Select [1-6]: "
    read -r choice
    case "$choice" in
        1) MODE="gui";;
        2) MODE="text";;
        3) MODE="debug";;
        4) MODE="headless";;
        5) MODE="raw";;
        6) MODE="vbox";;
        0) exit 0;;
        *) MODE="gui";;
    esac
fi

echo -e "  ${CYAN}→${NC}  Starting Kosmo OS in ${BOLD}$MODE${NC} mode..."
echo ""

case "$MODE" in

    gui)
        # Modo gráfico estándar con VGA
        $QEMU $BASE_FLAGS \
            -vga std \
            -cdrom "$ISO"
        ;;

    text)
        # Forzar modo texto apagando VESA
        $QEMU $BASE_FLAGS \
            -vga cirrus \
            -cdrom "$ISO"
        ;;

    debug)
        echo -e "  ${YELLOW}⚠${NC}  GDB server on localhost:1234"
        echo "  Open another terminal and run: make gdb"
        echo "  Or: gdb build/kernel.elf -ex 'target remote :1234'"
        echo ""
        $QEMU $BASE_FLAGS \
            -vga std \
            -cdrom "$ISO" \
            -s -S \
            -d int,cpu_reset \
            -D build/qemu_debug.log
        ;;

    headless)
        echo "  Output on serial (stdio). Ctrl+C to quit."
        $QEMU $BASE_FLAGS \
            -display none \
            -cdrom "$ISO"
        ;;

    raw)
        if [ ! -f "$IMG" ]; then
            echo -e "  ${YELLOW}⚠${NC}  Raw image not found. Building..."
            make disk-image
        fi
        $QEMU $BASE_FLAGS \
            -vga std \
            -drive format=raw,file="$IMG"
        ;;

    vbox)
        if ! which VBoxManage >/dev/null 2>&1; then
            echo -e "  ${RED}✗${NC}  VBoxManage not found."
            echo "  Install VirtualBox or use the ISO directly."
            exit 1
        fi
        VMDK="build/kosmo-os.vmdk"
        [ ! -f "$IMG" ] && make disk-image
        VBoxManage convertdd "$IMG" "$VMDK" --format VMDK 2>/dev/null || true
        echo -e "  ${GREEN}✓${NC}  VMDK: $VMDK"
        echo "  Import in VirtualBox: New VM → Other/Unknown → Use existing VMDK"
        ;;

    usb)
        echo ""
        echo -e "${BOLD}  Flash to USB instructions:${NC}"
        echo ""
        echo "  ISO file: $(realpath $ISO)"
        echo ""
        echo "  Windows — Rufus:"
        echo "    1. Download Rufus from rufus.ie"
        echo "    2. Select your USB drive"
        echo "    3. Boot selection: Disk or ISO image"
        echo "    4. Select kosmo-os.iso"
        echo "    5. Partition: MBR   Target: BIOS or UEFI"
        echo "    6. Click START"
        echo ""
        echo "  Windows — Ventoy:"
        echo "    1. Install Ventoy on USB"
        echo "    2. Copy kosmo-os.iso to the Ventoy partition"
        echo "    3. Boot from USB and select Kosmo OS"
        echo ""
        echo "  Linux — dd (CAREFUL — overwrites the entire device):"
        echo "    sudo dd if=$ISO of=/dev/sdX bs=4M status=progress"
        echo "    Replace sdX with your USB device (check with: lsblk)"
        echo ""
        ;;

    *)
        echo "Unknown mode: $MODE"
        echo "Usage: $0 [gui|text|debug|headless|raw|vbox|usb]"
        exit 1
        ;;
esac
