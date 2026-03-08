# =============================================================================
# KOSMO OS — Makefile Principal
# Archivo : Makefile
# Función : Compila el bootloader, kernel y genera la imagen ISO.
#           Soporte para QEMU y VirtualBox.
# =============================================================================

# -----------------------------------------------------------------------------
# CONFIGURACIÓN DEL TOOLCHAIN
# Preferimos i686-elf-gcc (cross-compiler puro).
# Si no está disponible, usamos gcc con flags de cross-compilación.
# -----------------------------------------------------------------------------

# Detectar si tenemos cross-compiler
CROSS_GCC := $(shell which i686-elf-gcc 2>/dev/null)
CROSS_LD  := $(shell which i686-elf-ld 2>/dev/null)
CROSS_AS  := $(shell which i686-elf-as 2>/dev/null)

ifdef CROSS_GCC
    CC  := i686-elf-gcc
    LD  := i686-elf-ld
    AS  := i686-elf-as
    $(info [INFO] Using cross-compiler: i686-elf-gcc)
else
    CC  := gcc
    LD  := ld
    AS  := as
    $(info [WARN] Cross-compiler not found. Using system gcc with -m32)
    EXTRA_CFLAGS := -m32
    EXTRA_LDFLAGS := -m elf_i386
endif

NASM    := nasm
AR      := ar
OBJCOPY := objcopy

# -----------------------------------------------------------------------------
# DIRECTORIOS
# -----------------------------------------------------------------------------
BUILD_DIR   := build
ISO_DIR     := $(BUILD_DIR)/iso
GRUB_DIR    := $(ISO_DIR)/boot/grub

# -----------------------------------------------------------------------------
# FLAGS DE COMPILACIÓN
# -----------------------------------------------------------------------------

# Flags para el kernel C
CFLAGS := \
    $(EXTRA_CFLAGS)         \
    -std=c99                \
    -ffreestanding          \
    -fno-builtin            \
    -fno-stack-protector    \
    -fno-pic                \
    -Wall                   \
    -Wextra                 \
    -O2                     \
    -I include              \
    -I kernel/arch/x86      \
    -I drivers/video        \
    -I drivers/input        \
    -I drivers/timer        \
    -I shell                \
    -I shell/commands       \
    -I kernel/core          \
    -I fs                   \
    -I kernel               \
    -I drivers              \
    -I libc

# Flags para el linker
LDFLAGS := \
    $(EXTRA_LDFLAGS)        \
    -nostdlib               \
    -T kernel/linker.ld

# Flags para NASM
NASMFLAGS_BIN   := -f bin
NASMFLAGS_ELF   := -f elf32

# -----------------------------------------------------------------------------
# ARCHIVOS FUENTE
# -----------------------------------------------------------------------------

# Stage 1 y Stage 2 (binarios puros)
BOOT1_SRC   := boot/stage1/boot.asm
BOOT2_SRC   := boot/stage2/stage2.asm

# Entry point del kernel (ASM)
KERNEL_ASM_SRCS := \
    kernel/arch/x86/entry.asm \
    kernel/arch/x86/isr.asm

# Fuentes C del kernel
KERNEL_C_SRCS := \
    kernel/core/kernel.c        \
    kernel/core/panic.c         \
    kernel/arch/x86/gdt.c       \
    kernel/arch/x86/idt.c       \
    kernel/arch/x86/pic.c       \
    kernel/mm/pmm.c             \
    kernel/mm/heap.c            \
    drivers/video/vga.c         \
    drivers/input/keyboard.c    \
    drivers/timer/pit.c         \
    shell/shell.c               \
    shell/commands/commands.c   \
    shell/commands/cmd_fs.c     \
    fs/kosmofs.c                \
    libc/string.c               \
    libc/stdio.c

# Objetos generados
KERNEL_ASM_OBJS := $(patsubst %.asm, $(BUILD_DIR)/%.o, $(KERNEL_ASM_SRCS))
KERNEL_C_OBJS   := $(patsubst %.c,   $(BUILD_DIR)/%.o, $(KERNEL_C_SRCS))
KERNEL_OBJS     := $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS)

# -----------------------------------------------------------------------------
# OBJETIVOS PRINCIPALES
# -----------------------------------------------------------------------------

.PHONY: all clean iso run run-vga run-debug setup-dirs

# Por defecto: compilar todo
all: setup-dirs bootloader kernel iso
	@echo ""
	@echo "============================================"
	@echo "  KOSMO OS build complete!"
	@echo "  ISO: $(BUILD_DIR)/kosmo-os.iso"
	@echo "============================================"

# -----------------------------------------------------------------------------
# CREAR DIRECTORIOS DE BUILD
# -----------------------------------------------------------------------------
setup-dirs:
	@mkdir -p $(BUILD_DIR)/boot/stage1
	@mkdir -p $(BUILD_DIR)/boot/stage2
	@mkdir -p $(BUILD_DIR)/kernel/arch/x86
	@mkdir -p $(BUILD_DIR)/kernel/core
	@mkdir -p $(BUILD_DIR)/kernel/mm
	@mkdir -p $(BUILD_DIR)/drivers/video
	@mkdir -p $(BUILD_DIR)/drivers/input
	@mkdir -p $(BUILD_DIR)/drivers/timer
	@mkdir -p $(BUILD_DIR)/libc
	@mkdir -p $(GRUB_DIR)
	@echo "[DIRS] Build directories ready."

# -----------------------------------------------------------------------------
# BOOTLOADER
# -----------------------------------------------------------------------------
bootloader: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/stage2.bin
	@echo "[BOOT] Bootloader ready."

# Stage 1: MBR (512 bytes exactos)
$(BUILD_DIR)/boot.bin: $(BOOT1_SRC)
	@echo "[NASM] $< -> $@"
	$(NASM) $(NASMFLAGS_BIN) $< -o $@
	@# Verificar que el tamaño es exactamente 512 bytes
	@SIZE=$$(stat -c%s $@); \
	if [ "$$SIZE" -ne 512 ]; then \
	    echo "[ERROR] boot.bin must be exactly 512 bytes! Got $$SIZE"; \
	    exit 1; \
	fi
	@echo "[OK] boot.bin is 512 bytes."

# Stage 2
$(BUILD_DIR)/stage2.bin: $(BOOT2_SRC) boot/stage2/a20.asm
	@echo "[NASM] $< -> $@"
	$(NASM) $(NASMFLAGS_BIN) -I boot/stage2/ $< -o $@
	@echo "[OK] stage2.bin: $$(stat -c%s $@) bytes"

# -----------------------------------------------------------------------------
# KERNEL
# -----------------------------------------------------------------------------
kernel: $(BUILD_DIR)/kernel.bin
	@echo "[KERNEL] Kernel ready: $$(stat -c%s $<) bytes"

# Compilar ASM del kernel
$(BUILD_DIR)/%.o: %.asm
	@echo "[NASM] $< -> $@"
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS_ELF) $< -o $@

# Compilar C del kernel
$(BUILD_DIR)/%.o: %.c
	@echo "[CC]   $< -> $@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Enlazar el kernel en un ELF y convertir a binario plano
$(BUILD_DIR)/kernel.elf: $(KERNEL_OBJS)
	@echo "[LD]   Linking kernel..."
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	@echo "[BIN]  Extracting raw binary..."
	$(OBJCOPY) -O binary $< $@

# -----------------------------------------------------------------------------
# IMAGEN ISO (booteable con GRUB2)
# Compatible con Rufus, Ventoy, QEMU, VirtualBox
# -----------------------------------------------------------------------------
iso: $(BUILD_DIR)/kosmo-os.iso

$(BUILD_DIR)/kosmo-os.iso: $(BUILD_DIR)/kernel.bin iso/grub/grub.cfg
	@echo "[ISO]  Building ISO image..."

	# Preparar estructura del directorio ISO
	@cp $(BUILD_DIR)/kernel.bin $(ISO_DIR)/boot/kernel.bin
	@cp iso/grub/grub.cfg $(GRUB_DIR)/grub.cfg

	# Generar ISO con GRUB2 (xorriso + grub-mkrescue)
	grub-mkrescue -o $(BUILD_DIR)/kosmo-os.iso $(ISO_DIR) \
	    --compress=no \
	    2>/dev/null || \
	grub2-mkrescue -o $(BUILD_DIR)/kosmo-os.iso $(ISO_DIR) \
	    --compress=no

	@echo "[OK]   ISO generated: $(BUILD_DIR)/kosmo-os.iso"
	@echo "       Size: $$(du -h $(BUILD_DIR)/kosmo-os.iso | cut -f1)"

# -----------------------------------------------------------------------------
# IMAGEN DE DISCO RAW (para pruebas con bootloader propio)
# Genera una imagen .img con MBR + Stage2 + Kernel
# -----------------------------------------------------------------------------
disk-image: bootloader kernel
	@echo "[IMG]  Creating raw disk image..."

	# Crear imagen vacía de 10MB
	dd if=/dev/zero of=$(BUILD_DIR)/kosmo.img bs=512 count=20480 2>/dev/null

	# Escribir Stage 1 en el sector 0 (MBR)
	dd if=$(BUILD_DIR)/boot.bin of=$(BUILD_DIR)/kosmo.img \
	    bs=512 count=1 conv=notrunc 2>/dev/null

	# Escribir Stage 2 desde el sector 2
	dd if=$(BUILD_DIR)/stage2.bin of=$(BUILD_DIR)/kosmo.img \
	    bs=512 seek=1 conv=notrunc 2>/dev/null

	# Escribir Kernel desde el sector 18
	dd if=$(BUILD_DIR)/kernel.bin of=$(BUILD_DIR)/kosmo.img \
	    bs=512 seek=17 conv=notrunc 2>/dev/null

	@echo "[OK]   Disk image: $(BUILD_DIR)/kosmo.img"

# -----------------------------------------------------------------------------
# EJECUCIÓN CON QEMU
# -----------------------------------------------------------------------------

# Arranque desde ISO con GRUB (modo recomendado)
run: iso
	@echo "[QEMU] Starting Kosmo OS from ISO..."
	qemu-system-i386 \
	    -cdrom $(BUILD_DIR)/kosmo-os.iso \
	    -m 64M \
	    -vga std \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown

# Modo pantalla gráfica QEMU sin terminal
run-vga: iso
	qemu-system-i386 \
	    -cdrom $(BUILD_DIR)/kosmo-os.iso \
	    -m 64M \
	    -vga std \
	    -no-reboot

# Arranque desde imagen raw (bootloader propio)
run-raw: disk-image
	@echo "[QEMU] Starting Kosmo OS from raw image..."
	qemu-system-i386 \
	    -drive file=$(BUILD_DIR)/kosmo.img,format=raw \
	    -m 64M \
	    -vga std \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown

# Modo debug con GDB
run-debug: iso
	@echo "[QEMU] Starting Kosmo OS in debug mode (GDB on :1234)..."
	qemu-system-i386 \
	    -cdrom $(BUILD_DIR)/kosmo-os.iso \
	    -m 64M \
	    -vga std \
	    -serial stdio \
	    -s -S \
	    -no-reboot \
	    -no-shutdown

# Conectar GDB al QEMU en debug
gdb:
	gdb \
	    -ex "target remote :1234" \
	    -ex "symbol-file $(BUILD_DIR)/kernel.elf" \
	    -ex "break kernel_main" \
	    -ex "continue"

# -----------------------------------------------------------------------------
# LIMPIEZA
# -----------------------------------------------------------------------------
clean:
	@echo "[CLEAN] Removing build directory..."
	@rm -rf $(BUILD_DIR)
	@echo "[CLEAN] Done."

# Limpiar solo objetos (mantener ISO)
clean-objs:
	@find $(BUILD_DIR) -name "*.o" -delete
	@find $(BUILD_DIR) -name "*.elf" -delete

# -----------------------------------------------------------------------------
# INFORMACIÓN DE BUILD
# -----------------------------------------------------------------------------
info:
	@echo "========================================"
	@echo "  KOSMO OS Build Information"
	@echo "========================================"
	@echo "  CC:      $(CC)"
	@echo "  LD:      $(LD)"
	@echo "  NASM:    $(shell $(NASM) --version | head -1)"
	@echo "  CFLAGS:  $(CFLAGS)"
	@echo "========================================"
