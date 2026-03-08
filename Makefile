# =============================================================================
# KOSMO OS — Makefile
# =============================================================================

# ── Herramientas ──────────────────────────────────────────────────────────────
CC      := gcc
LD      := ld
OBJCOPY := objcopy

ifneq ($(shell which i686-elf-gcc 2>/dev/null),)
    CC      := i686-elf-gcc
    LD      := i686-elf-ld
    OBJCOPY := i686-elf-objcopy
    CROSS   := 1
else
    CROSS   := 0
endif

# ── Directorios ───────────────────────────────────────────────────────────────
BUILD_DIR  := build
ISO_DIR    := $(BUILD_DIR)/iso
BOOT_DIR   := $(ISO_DIR)/boot
GRUB_DIR   := $(BOOT_DIR)/grub
OBJ_DIR    := $(BUILD_DIR)/obj

ISO_FILE   := $(BUILD_DIR)/kosmo-os.iso
IMG_FILE   := $(BUILD_DIR)/kosmo-os.img
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BOOT_DIR)/kernel.bin

# ── Flags C ───────────────────────────────────────────────────────────────────
CFLAGS := \
    -m32 -std=c99 -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-pic -fno-omit-frame-pointer -Wall -Wextra -O2 \
    -Werror=implicit-function-declaration \
    -I include -I kernel -I kernel/arch/x86 -I kernel/core \
    -I drivers -I drivers/video -I drivers/input -I drivers/timer \
    -I libc -I shell -I shell/commands -I fs \
    -I gui -I gui/font -I tests

ifeq ($(CROSS),1)
    CFLAGS := $(filter-out -m32,$(CFLAGS))
endif

NASM_ELF_FLAGS := -f elf32
LDFLAGS        := -m elf_i386 -T kernel/linker.ld --nostdlib

QEMU       := qemu-system-i386
QEMU_FLAGS := -m 128M -vga std -serial stdio -no-reboot -name "Kosmo OS"

# ── Fuentes C ─────────────────────────────────────────────────────────────────
KERNEL_C_SRCS := \
    kernel/arch/x86/gdt.c       \
    kernel/arch/x86/idt.c       \
    kernel/core/kernel.c        \
    kernel/core/panic.c         \
    drivers/video/vga.c         \
    drivers/video/vesa.c        \
    drivers/input/keyboard.c    \
    drivers/input/mouse.c       \
    drivers/timer/pit.c         \
    fs/kosmofs.c                \
    shell/shell.c               \
    shell/commands/commands.c   \
    shell/commands/cmd_fs.c     \
    gui/font/font8x8.c          \
    gui/wm.c                    \
    gui/desktop.c               \
    libc/string.c               \
    libc/stdio.c                \
    tests/ktest.c

KERNEL_C_OBJS := $(patsubst %.c, $(OBJ_DIR)/%.o, $(KERNEL_C_SRCS))

# ── Objetos ASM del kernel — reglas explícitas ────────────────────────────────
ENTRY_OBJ := $(OBJ_DIR)/kernel/arch/x86/entry.o
ISR_OBJ   := $(OBJ_DIR)/kernel/arch/x86/isr.o
KERNEL_ASM_OBJS := $(ENTRY_OBJ) $(ISR_OBJ)

KERNEL_OBJS := $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS)

# =============================================================================
# TARGETS PRINCIPALES
# =============================================================================
.PHONY: all iso clean clean-obj run run-raw run-debug run-headless \
        gdb disk-image check-tools check-syntax kernel bootloader \
        dump-elf disasm symbols size info help test

# ─────────────────────────────────────────────────────────────────────────────
# "all" solo construye la ISO via GRUB.
# NO depende del bootloader propio (stage1/stage2).
# El bootloader solo se necesita para "make disk-image".
# ─────────────────────────────────────────────────────────────────────────────
all: check-tools $(ISO_FILE)
	@echo ""
	@echo "  +------------------------------------------+"
	@echo "  |   Kosmo OS built successfully!           |"
	@echo "  +------------------------------------------+"
	@ls -lh $(ISO_FILE)

iso: all

# =============================================================================
# VERIFICACIÓN
# =============================================================================
check-tools:
	@which nasm  >/dev/null 2>&1 \
	    || (echo "ERROR: nasm missing.  Run: sudo apt install nasm" && exit 1)
	@which $(CC) >/dev/null 2>&1 \
	    || (echo "ERROR: $(CC) missing. Run: sudo apt install gcc gcc-multilib" && exit 1)
	@which $(LD) >/dev/null 2>&1 \
	    || (echo "ERROR: ld missing.    Run: sudo apt install binutils" && exit 1)
	@echo "  Tools OK  (CC=$(CC)  CROSS=$(CROSS))"

check-syntax: check-tools
	@errors=0; \
	for f in $(KERNEL_C_SRCS); do \
	    $(CC) $(CFLAGS) -fsyntax-only "$$f" 2>&1 \
	    && printf "  + $$f\n" \
	    || { printf "  x $$f\n"; errors=$$((errors+1)); }; \
	done; \
	[ $$errors -eq 0 ] && echo "  All OK" || exit 1
	@test -f kernel/arch/x86/entry.asm \
	    || (echo "ERROR: kernel/arch/x86/entry.asm missing from repo!" && exit 1)
	@test -f kernel/arch/x86/isr.asm \
	    || (echo "ERROR: kernel/arch/x86/isr.asm missing from repo!" && exit 1)
	@echo "  ASM files OK"

# =============================================================================
# KERNEL (C + ASM del kernel — NO incluye el bootloader)
# =============================================================================
kernel: $(KERNEL_BIN)

$(BUILD_DIR):
	@mkdir -p $@

$(GRUB_DIR):
	@mkdir -p $(GRUB_DIR)

# Regla genérica para archivos C
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Reglas explícitas para los dos .asm del kernel
$(ENTRY_OBJ): kernel/arch/x86/entry.asm
	@mkdir -p $(dir $@)
	@echo "  [AS] kernel/arch/x86/entry.asm"
	@nasm $(NASM_ELF_FLAGS) $< -o $@

$(ISR_OBJ): kernel/arch/x86/isr.asm
	@mkdir -p $(dir $@)
	@echo "  [AS] kernel/arch/x86/isr.asm"
	@nasm $(NASM_ELF_FLAGS) $< -o $@

# Linkeo
$(KERNEL_ELF): $(KERNEL_OBJS) | $(BUILD_DIR)
	@echo "  [LD] Linking kernel.elf..."
	@$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)
	@size $@

# Extraer binario plano
$(KERNEL_BIN): $(KERNEL_ELF) | $(GRUB_DIR)
	@echo "  [OBJCOPY] kernel.elf -> kernel.bin"
	@$(OBJCOPY) -O binary $< $@
	@echo "  Kernel: $$(stat -c%s $@) bytes"

# =============================================================================
# ISO BOOTEABLE (GRUB2 + Multiboot — no necesita stage1/stage2)
# =============================================================================
$(GRUB_DIR)/grub.cfg: iso/grub/grub.cfg | $(GRUB_DIR)
	@cp $< $@

$(ISO_FILE): $(KERNEL_BIN) $(GRUB_DIR)/grub.cfg
	@echo "  [ISO] Running grub-mkrescue..."
	@if which grub-mkrescue >/dev/null 2>&1; then \
	    grub-mkrescue -o $@ $(ISO_DIR) 2>/dev/null \
	    && echo "  ISO ready: $@ ($$(du -sh $@ | cut -f1))"; \
	else \
	    echo "ERROR: grub-mkrescue not found."; \
	    echo "Run: sudo apt install grub-pc-bin grub-common xorriso mtools"; \
	    exit 1; \
	fi

# =============================================================================
# BOOTLOADER PROPIO (stage1/stage2) — solo para imagen raw de disco
# Solo se compila si los archivos .asm del bootloader existen en el repo.
# =============================================================================
STAGE1_SRC := boot/stage1/boot.asm
STAGE2_SRC := boot/stage2/stage2.asm
STAGE1_BIN := $(BUILD_DIR)/stage1.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin

bootloader:
	@test -f $(STAGE1_SRC) \
	    || (echo "ERROR: $(STAGE1_SRC) not found in repository" && exit 1)
	@test -f $(STAGE2_SRC) \
	    || (echo "ERROR: $(STAGE2_SRC) not found in repository" && exit 1)
	@$(MAKE) $(STAGE1_BIN) $(STAGE2_BIN)

$(STAGE1_BIN): $(STAGE1_SRC) | $(BUILD_DIR)
	@echo "  [AS] $(STAGE1_SRC)"
	@nasm -f bin $< -o $@
	@test $$(stat -c%s $@) -eq 512 \
	    || (echo "ERROR: Stage1 must be exactly 512 bytes" && exit 1)

$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILD_DIR)
	@echo "  [AS] $(STAGE2_SRC)"
	@nasm -f bin $< -o $@

# Imagen raw: SÍ necesita el bootloader propio
disk-image: $(KERNEL_BIN)
	@test -f $(STAGE1_SRC) \
	    || (echo "ERROR: $(STAGE1_SRC) needed for disk-image" && exit 1)
	@$(MAKE) $(STAGE1_BIN) $(STAGE2_BIN)
	@echo "  [IMG] Building 32 MB raw disk image..."
	@dd if=/dev/zero      bs=512 count=65536 of=$(IMG_FILE)          2>/dev/null
	@dd if=$(STAGE1_BIN)  bs=512 seek=0  conv=notrunc of=$(IMG_FILE) 2>/dev/null
	@dd if=$(STAGE2_BIN)  bs=512 seek=2  conv=notrunc of=$(IMG_FILE) 2>/dev/null
	@dd if=$(KERNEL_BIN)  bs=512 seek=16 conv=notrunc of=$(IMG_FILE) 2>/dev/null
	@echo "  Raw image: $(IMG_FILE)"

# =============================================================================
# QEMU
# =============================================================================
run: $(ISO_FILE)
	@$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_FILE)

run-raw: disk-image
	@$(QEMU) $(QEMU_FLAGS) -drive format=raw,file=$(IMG_FILE)

run-headless: $(ISO_FILE)
	@$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_FILE) -display none

run-debug: $(ISO_FILE)
	@echo "  GDB server on :1234"
	@$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_FILE) \
	    -s -S -d int,cpu_reset -D $(BUILD_DIR)/qemu_debug.log

gdb: $(KERNEL_ELF)
	@gdb $(KERNEL_ELF) \
	    -ex "target remote :1234" \
	    -ex "set architecture i386" \
	    -ex "break kernel_main" \
	    -ex "continue"

# =============================================================================
# TESTS
# =============================================================================
test:
	@$(MAKE) all CFLAGS="$(CFLAGS) -DKOSMO_TEST=1"

# =============================================================================
# ANÁLISIS
# =============================================================================
dump-elf: $(KERNEL_ELF)
	@readelf -a $< | head -80

disasm: $(KERNEL_ELF)
	@objdump -d -M intel $<

symbols: $(KERNEL_ELF)
	@nm $< | sort | grep -v " U "

size: $(KERNEL_ELF)
	@size $<

# =============================================================================
# LIMPIEZA E INFO
# =============================================================================
clean:
	@rm -rf $(BUILD_DIR)
	@echo "  Cleaned."

clean-obj:
	@rm -rf $(OBJ_DIR)

info:
	@echo "  CC=$(CC)  LD=$(LD)  CROSS=$(CROSS)"
	@echo "  C sources : $(words $(KERNEL_C_SRCS))"
	@echo "  ISO target: $(ISO_FILE)"

help:
	@echo ""
	@echo "  make all         Build ISO (does NOT need stage1/stage2)"
	@echo "  make kernel      Build kernel only"
	@echo "  make bootloader  Build stage1/stage2 (needs boot/ ASM files)"
	@echo "  make disk-image  Build raw .img (needs bootloader)"
	@echo "  make run         Run ISO in QEMU"
	@echo "  make run-debug   Run + GDB server on :1234"
	@echo "  make clean       Remove build/"
	@echo ""
