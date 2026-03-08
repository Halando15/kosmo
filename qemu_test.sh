#!/usr/bin/env bash
# =============================================================================
# KOSMO OS — Tests Automatizados con QEMU
# Archivo : scripts/debug/qemu_test.sh
# Función : Arranca Kosmo OS en QEMU headless, captura la salida por el
#           puerto serie, y verifica que cada subsistema arrancó correctamente.
# Uso     : ./scripts/debug/qemu_test.sh [--timeout 30]
# =============================================================================

set -e

# ── Colores ───────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

ok()   { echo -e "  ${GREEN}[PASS]${NC} $*"; PASSED=$((PASSED+1)); }
fail() { echo -e "  ${RED}[FAIL]${NC} $*"; FAILED=$((FAILED+1)); }
info() { echo -e "  ${CYAN}[INFO]${NC} $*"; }
hdr()  { echo -e "\n${BOLD}  ── $* ──────────────────────────────${NC}"; }

PASSED=0; FAILED=0
TIMEOUT="${2:-25}"   # segundos
ISO="build/kosmo-os.iso"
LOG="build/boot_serial.log"

# ── Verificar dependencias ────────────────────────────────────────────────────
hdr "Pre-flight checks"

if [ ! -f "$ISO" ]; then
    echo -e "  ${RED}✗${NC} ISO not found: $ISO"
    echo "  Run: make all"
    exit 1
fi
info "ISO: $ISO ($(du -sh "$ISO" | cut -f1))"

if ! which qemu-system-i386 >/dev/null 2>&1; then
    echo -e "  ${RED}✗${NC} qemu-system-i386 not found"
    exit 1
fi
info "QEMU: $(qemu-system-i386 --version | head -1)"

# ── Arrancar QEMU en background ───────────────────────────────────────────────
hdr "Booting Kosmo OS (headless, ${TIMEOUT}s timeout)"

mkdir -p build
rm -f "$LOG"

# Añadimos un serial que escribe al log Y a stdout
qemu-system-i386 \
    -m 128M \
    -no-reboot \
    -display none \
    -serial file:"$LOG" \
    -cdrom "$ISO" \
    -name "Kosmo OS Test" &

QEMU_PID=$!
info "QEMU PID: $QEMU_PID"

# Esperar a que el log contenga cierto texto o se agote el timeout
wait_for_log() {
    local pattern="$1"
    local deadline=$((SECONDS + TIMEOUT))
    while [ $SECONDS -lt $deadline ]; do
        [ -f "$LOG" ] && grep -q "$pattern" "$LOG" 2>/dev/null && return 0
        sleep 0.5
    done
    return 1
}

# ── Esperar arranque completo ─────────────────────────────────────────────────
info "Waiting for boot sequence..."
if wait_for_log "KosmoFS mounted\|All subsystems"; then
    info "Boot detected in serial output"
else
    echo -e "  ${YELLOW}⚠${NC}  Timeout waiting for boot. Proceeding with partial log."
fi

# Dar un segundo extra para que se estabilice
sleep 2

# Matar QEMU
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true
info "QEMU stopped"

# ── Analizar el log de arranque ───────────────────────────────────────────────
hdr "Analyzing boot log"

if [ ! -f "$LOG" ] || [ ! -s "$LOG" ]; then
    fail "No serial output captured"
    echo "  Check that kernel has serial output (kprintf should output to serial)"
    echo ""
    echo "  NOTE: If QEMU shows the GUI but no serial log, the kernel may"
    echo "  not have serial output enabled. Add this to kernel_main():"
    echo "    outb(0x3F8, c);  // COM1 for each kprintf character"
    echo ""
    # Verificar al menos que el kernel ELF existe y tiene el tamaño correcto
    hdr "Verifying kernel binary"
    if [ -f "build/kernel.elf" ]; then
        ok "kernel.elf exists ($(du -sh build/kernel.elf | cut -f1))"
        SIZE=$(stat -c%s build/kernel.elf 2>/dev/null || stat -f%z build/kernel.elf)
        [ "$SIZE" -gt 10000 ] && ok "Kernel size looks reasonable ($SIZE bytes)" \
                               || fail "Kernel seems too small ($SIZE bytes)"
    else
        fail "build/kernel.elf not found"
    fi
else
    info "Serial log: $LOG ($(wc -l < "$LOG") lines)"

    # ── Verificar cada subsistema por texto en el log ─────────────────────────
    hdr "Subsystem checks"

    check_log() {
        local name="$1"
        shift
        # Buscar cualquiera de los patrones
        for pat in "$@"; do
            grep -qi "$pat" "$LOG" 2>/dev/null && { ok "$name"; return; }
        done
        fail "$name (pattern not found in log)"
    }

    check_log "VGA init"        "vga\|text mode\|\[ OK \].*VGA"
    check_log "GDT init"        "GDT\|gdt_init\|\[ OK \].*GDT"
    check_log "IDT/PIC init"    "IDT\|PIC\|\[ OK \].*IDT"
    check_log "PIT 100Hz"       "PIT\|100 Hz\|\[ OK \].*PIT"
    check_log "PS/2 Keyboard"   "keyboard\|PS/2\|IRQ1"
    check_log "KosmoFS mount"   "KosmoFS\|mounted\|RAM disk"
    check_log "VESA/Framebuffer" "VESA\|framebuffer\|800x600\|GUI"
    check_log "No kernel panic" "PANIC\|Triple fault" 2>/dev/null \
        && fail "Kernel panic detected in log!" \
        || ok  "No kernel panic"

    # Imprimir el log para diagnóstico
    hdr "Boot log"
    cat "$LOG" | head -60 | sed 's/^/  /'
    LINES=$(wc -l < "$LOG")
    [ "$LINES" -gt 60 ] && echo "  ... ($((LINES-60)) more lines)"
fi

# ── Tests de la imagen ────────────────────────────────────────────────────────
hdr "Binary validation"

# Verificar firma Multiboot en el kernel
if [ -f "build/kernel.elf" ]; then
    if readelf -a build/kernel.elf 2>/dev/null | grep -q "multiboot\|0x1badb002"; then
        ok "Multiboot magic found in kernel ELF"
    else
        # Buscar en el binario crudo
        if grep -c $'\x02\xb0\xad\x1b' build/kosmo-os.iso >/dev/null 2>&1; then
            ok "Multiboot magic found in ISO"
        else
            fail "Multiboot magic not detected (kernel may not boot with GRUB)"
        fi
    fi
    ok "Kernel ELF exists"

    # Verificar secciones críticas
    readelf -S build/kernel.elf 2>/dev/null | grep -q "\.text"  && ok ".text section present"
    readelf -S build/kernel.elf 2>/dev/null | grep -q "\.bss"   && ok ".bss section present"
    readelf -S build/kernel.elf 2>/dev/null | grep -q "\.rodata" && ok ".rodata section present"

    # Tamaño del kernel
    TEXT_SIZE=$(readelf -S build/kernel.elf 2>/dev/null | awk '/.text/{print $7}' | head -1)
    [ -n "$TEXT_SIZE" ] && info "  .text size: 0x$TEXT_SIZE bytes"
fi

# Verificar ISO
if [ -f "$ISO" ]; then
    ISO_SIZE=$(stat -c%s "$ISO" 2>/dev/null || stat -f%z "$ISO")
    [ "$ISO_SIZE" -gt 1048576 ] && ok "ISO size OK (${ISO_SIZE} bytes)" \
                                 || fail "ISO too small (${ISO_SIZE} bytes) — grub-mkrescue may have failed"
fi

# ── Resumen final ─────────────────────────────────────────────────────────────
hdr "Test Summary"

echo -e "  ${GREEN}Passed: $PASSED${NC}"
[ "$FAILED" -gt 0 ] && echo -e "  ${RED}Failed: $FAILED${NC}" \
                     || echo -e "  Failed: $FAILED"
echo "  Total : $((PASSED + FAILED))"
echo ""

if [ "$FAILED" -eq 0 ]; then
    echo -e "  ${GREEN}${BOLD}✓  All tests passed! Kosmo OS is ready.${NC}"
    echo ""
    echo "  Next steps:"
    echo "    make run              # Interactive QEMU"
    echo "    ./scripts/run.sh usb  # Flash instructions"
    exit 0
else
    echo -e "  ${RED}${BOLD}✗  $FAILED test(s) failed.${NC}"
    echo ""
    echo "  Troubleshooting:"
    echo "    make run-debug + make gdb   # Debug with GDB"
    echo "    cat $LOG                    # Full boot log"
    exit 1
fi
