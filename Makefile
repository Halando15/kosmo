# =============================================================================
# KOSMO OS — Makefile Principal  (Fase 8 — Build System Completo)
# Uso: make all | make iso | make run | make run-debug | make clean | make help
# =============================================================================

# ── Herramientas ──────────────────────────────────────────────────────────────
CC      := gcc
AS      := nasm
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
    -fno-pic -fno-omit-frame-pointer -Wall -Wextra -O2              \
    -Werror=implicit-function-declaration                            \
    -I include -I kernel -I kernel/arch/x86 -I kernel/core          \
    -I drivers -I drivers/video -I drivers/input -I drivers/timer   \
    -I libc -I shell -I shell/commands -I fs                         \
    -I gui -I gui/font                 \
    -I tests

ifeq ($(CROSS),1)
    CFLAGS := $(filter-out -m32,$(CFLAGS))
endif

NASM_BIN_FLAGS := -f bin
NASM_ELF_FLAGS := -f elf32

LDFLAGS := -m elf_i386 -T kernel/linker.ld --nostdlib

# ── Fuentes ───────────────────────────────────────────────────────────────────
KERNEL_C_SRCS := \
    kernel/arch/x86/gdt.c       kernel/arch/x86/idt.c     \
    kernel/core/kernel.c        kernel/core/panic.c        \
    drivers/video/vga.c         drivers/video/vesa.c       \
    drivers/input/keyboard.c    drivers/input/mouse.c      \
    drivers/timer/pit.c                                    \
    fs/kosmofs.c                                           \
    shell/shell.c               shell/commands/commands.c  \
    shell/commands/cmd_fs.c                                \
    gui/font/font8x8.c          gui/wm.c  gui/desktop.c   \
    libc/string.c               libc/stdio.c \
    tests/ktest.c

KERNEL_ASM_SRCS := kernel/arch/x86/entry.asm kernel/arch/x86/isr.asm

KERNEL_C_OBJS   := $(patsubst %.c,  $(OBJ_DIR)/%.o, $(KERNEL_C_SRCS))
KERNEL_ASM_OBJS := $(patsubst %.asm,$(OBJ_DIR)/%.o, $(KERNEL_ASM_SRCS))
KERNEL_OBJS     := $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS)

STAGE1_BIN := $(BUILD_DIR)/stage1.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin

QEMU       := qemu-system-i386
QEMU_FLAGS := -m 128M -vga std -serial stdio -no-reboot -name "Kosmo OS"

# =============================================================================
# TARGETS
# =============================================================================
.PHONY: all iso clean clean-obj run run-raw run-debug run-headless \
        gdb disk-image check-tools check-syntax dump-elf disasm symbols \
        size map info help bootloader kernel

## all — Compile everything and generate ISO
all: check-tools $(ISO_FILE)
	@echo ""
	@echo "  ╔══════════════════════════════════════════╗"
	@echo "  ║   Kosmo OS built successfully!           ║"
	@echo "  ╚══════════════════════════════════════════╝"
	@ls -lh $(ISO_FILE)

iso: all

check-tools:
	@which nasm >/dev/null 2>&1 || (echo "ERROR: nasm missing. sudo apt install nasm" && exit 1)
	@which $(CC) >/dev/null 2>&1 || (echo "ERROR: $(CC) missing" && exit 1)
	@which $(LD) >/dev/null 2>&1 || (echo "ERROR: ld missing" && exit 1)
	@echo "  Tools OK (CC=$(CC), CROSS=$(CROSS))"

check-syntax: check-tools
	@errors=0; for f in $(KERNEL_C_SRCS); do \
	    $(CC) $(CFLAGS) -fsyntax-only "$$f" 2>&1 \
	    && printf "  \033[32m✓\033[0m $$f\n" \
	    || { printf "  \033[31m✗\033[0m $$f\n"; errors=$$((errors+1)); }; \
	done; [ $$errors -eq 0 ] && echo "  All OK" || exit 1

# ── Bootloader ────────────────────────────────────────────────────────────────
bootloader: $(STAGE1_BIN) $(STAGE2_BIN)

$(BUILD_DIR):
	@mkdir -p $@

$(STAGE1_BIN): boot/stage1/boot.asm | $(BUILD_DIR)
	@echo "  [AS] stage1.asm → stage1.bin"
	@nasm $(NASM_BIN_FLAGS) $< -o $@
	@test $$(stat -c%s $@) -eq 512 || (echo "ERROR: Stage1 must be 512 bytes" && exit 1)

$(STAGE2_BIN): boot/stage2/stage2.asm | $(BUILD_DIR)
	@echo "  [AS] stage2.asm → stage2.bin"
	@nasm $(NASM_BIN_FLAGS) $< -o $@

# ── Kernel ────────────────────────────────────────────────────────────────────
kernel: $(KERNEL_BIN)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  [AS] $<"
	@nasm $(NASM_ELF_FLAGS) $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) | $(BUILD_DIR)
	@echo "  [LD] Linking kernel..."
	@$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)
	@size $@

$(GRUB_DIR):
	@mkdir -p $(GRUB_DIR)

$(KERNEL_BIN): $(KERNEL_ELF) | $(GRUB_DIR)
	@echo "  [OBJCOPY] ELF → raw BIN"
	@$(OBJCOPY) -O binary $< $@

# ── ISO (GRUB2 + Multiboot) ───────────────────────────────────────────────────
$(GRUB_DIR)/grub.cfg: iso/grub/grub.cfg | $(GRUB_DIR)
	@cp $< $@

$(ISO_FILE): $(KERNEL_BIN) $(GRUB_DIR)/grub.cfg
	@echo "  [ISO] Building bootable ISO..."
	@if which grub-mkrescue >/dev/null 2>&1; then \
	    grub-mkrescue -o $@ $(ISO_DIR) 2>/dev/null; \
	    echo "  ISO ready: $@ ($$(du -sh $@ | cut -f1))"; \
	else \
	    echo "  WARN: grub-mkrescue not found."; \
	    echo "  Install: sudo apt install grub-pc-bin xorriso mtools"; \
	    dd if=/dev/zero bs=1024 count=1440 of=$@ 2>/dev/null; \
	fi

# ── Raw disk image ────────────────────────────────────────────────────────────
## disk-image — Build raw 32 MB disk image (for VirtualBox/Rufus)
disk-image: $(IMG_FILE)

$(IMG_FILE): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN)
	@echo "  [IMG] Building raw disk image (32 MB)..."
	@dd if=/dev/zero  bs=512 count=65536 of=$@             2>/dev/null
	@dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0  conv=notrunc 2>/dev/null
	@dd if=$(STAGE2_BIN) of=$@ bs=512 seek=2  conv=notrunc 2>/dev/null
	@dd if=$(KERNEL_BIN) of=$@ bs=512 seek=16 conv=notrunc 2>/dev/null
	@echo "  Raw image: $@ (Kernel at sector 16)"

# ── QEMU ──────────────────────────────────────────────────────────────────────
## run — Boot ISO in QEMU
run: $(ISO_FILE)
	@$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_FILE)

## run-raw — Boot raw image in QEMU
run-raw: $(IMG_FILE)
	@$(QEMU) $(QEMU_FLAGS) -drive format=raw,file=$(IMG_FILE)

## run-headless — Run without display (serial only)
run-headless: $(ISO_FILE)
	@$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_FILE) -display none

## run-debug — QEMU + GDB server on :1234
run-debug: $(ISO_FILE)
	@echo "  GDB server on :1234 — run 'make gdb' in another terminal"
	@$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_FILE) \
	    -s -S -d int,cpu_reset -D $(BUILD_DIR)/qemu_debug.log

## gdb — Attach GDB to running QEMU
gdb: $(KERNEL_ELF)
	@gdb $(KERNEL_ELF) \
	    -ex "target remote :1234" \
	    -ex "set architecture i386" \
	    -ex "break kernel_main" \
	    -ex "continue"

## run-vbox — Convert to VMDK for VirtualBox
run-vbox: $(IMG_FILE)
	@VBoxManage convertdd $(IMG_FILE) $(BUILD_DIR)/kosmo-os.vmdk --format VMDK \
	    && echo "  VMDK ready: $(BUILD_DIR)/kosmo-os.vmdk" \
	    || echo "  VBoxManage not found"

# ── Análisis ──────────────────────────────────────────────────────────────────
## dump-elf — Show ELF headers
dump-elf: $(KERNEL_ELF)
	@readelf -a $< | head -80

## disasm — Disassemble kernel (Intel syntax)
disasm: $(KERNEL_ELF)
	@objdump -d -M intel $< | less

## symbols — Symbol table
symbols: $(KERNEL_ELF)
	@nm $< | sort | grep -v " U "

## size — Section sizes
size: $(KERNEL_ELF)
	@size $<

## map — Linker memory map
map: $(KERNEL_OBJS)
	@$(LD) $(LDFLAGS) -Map=$(BUILD_DIR)/kernel.map -o $(KERNEL_ELF) $(KERNEL_OBJS)
	@grep -A2 "^\.text\|^\.data\|^\.bss\|^\.rodata" $(BUILD_DIR)/kernel.map

# ── Limpieza ──────────────────────────────────────────────────────────────────
## clean — Remove all build artifacts
clean:
	@rm -rf $(BUILD_DIR)
	@echo "  Cleaned."

## clean-obj — Remove object files only
clean-obj:
	@rm -rf $(OBJ_DIR)

# ── Info ──────────────────────────────────────────────────────────────────────
## info — Show build configuration
info:
	@echo "  CC=$(CC)  LD=$(LD)  CROSS=$(CROSS)"
	@echo "  C sources : $(words $(KERNEL_C_SRCS))"
	@echo "  ASM sources: $(words $(KERNEL_ASM_SRCS))"
	@echo "  ISO: $(ISO_FILE)   IMG: $(IMG_FILE)"


## test — Build with KOSMO_TEST=1 and run tests in QEMU
test:
	$(MAKE) all CFLAGS="$(CFLAGS) -DKOSMO_TEST=1"
	$(QEMU) $(QEMU_FLAGS) -display none -cdrom $(ISO_FILE) || true

## test-verbose — Run automated QEMU test suite
test-verbose: all
	chmod +x scripts/debug/qemu_test.sh
	./scripts/debug/qemu_test.sh

## help — List all targets
help:
	@echo ""; grep -E '^## ' Makefile | sed 's/## /  make /'; echo ""

-include $(KERNEL_C_OBJS:.o=.d)
