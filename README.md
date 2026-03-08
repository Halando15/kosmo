# Kosmo OS

**Un sistema operativo real x86 de 32 bits, escrito desde cero en C y ASM.**  
Diseñado para portátiles antiguos. Arranca desde USB. Sin dependencias de ningún SO host.

```
  ██╗  ██╗ ██████╗ ███████╗███╗   ███╗ ██████╗ 
  ██║ ██╔╝██╔═══██╗██╔════╝████╗ ████║██╔═══██╗
  █████╔╝ ██║   ██║███████╗██╔████╔██║██║   ██║
  ██╔═██╗ ██║   ██║╚════██║██║╚██╔╝██║██║   ██║
  ██║  ██╗╚██████╔╝███████║██║ ╚═╝ ██║╚██████╔╝
  ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝     ╚═╝ ╚═════╝ 
  v0.1.0 "Genesis"  —  x86 32-bit  —  MIT License
```

---

## Características

| Componente | Implementación |
|---|---|
| Bootloader | MBR Stage1 + Stage2 (A20, E820, GDT) |
| Kernel | Monolítico, modo protegido 32-bit, modelo plano 4GB |
| GDT/IDT | 6 descriptores, 48 vectores (32 excepciones + 16 IRQs) |
| PIT | 100 Hz, `sleep_ms`, uptime |
| Teclado | PS/2 IRQ1, Scancode Set 1, buffer circular, historial |
| Ratón | PS/2 IRQ12, paquetes 3 bytes, posición absoluta |
| Filesystem | **KosmoFS** — RAM disk, 256 inodos, 128 KB, árbol completo |
| Pantalla texto | VGA 80×25, 16 colores, cursor hardware |
| Pantalla gráfica | **VESA VBE 2.0** — 800×600×32bpp, framebuffer lineal |
| GUI | Window Manager, arrastre, taskbar, reloj en tiempo real |
| Terminal | Shell con historial, TAB, 24+ comandos |

---

## Compilar

### Requisitos

```bash
# Ubuntu / Debian / WSL2
sudo apt install nasm gcc binutils grub-pc-bin xorriso qemu-system-x86 mtools
```

### Build rápido

```bash
# Todo en un paso (instala dependencias + compila)
chmod +x scripts/install.sh
./scripts/install.sh
```

### Build manual

```bash
make all          # Compila todo + genera build/kosmo-os.iso
make run          # Arranca en QEMU
make disk-image   # Genera imagen raw .img (para USB/VirtualBox)
make clean        # Limpia archivos generados
```

---

## Ejecutar

### QEMU (recomendado para desarrollo)

```bash
make run                    # GUI 800x600
make run-debug              # + servidor GDB en :1234
make gdb                    # Conectar GDB (en otra terminal)
./scripts/run.sh            # Menú interactivo
./scripts/run.sh debug      # Directo al modo debug
```

### USB físico (Rufus — Windows)

1. Descarga [Rufus](https://rufus.ie)
2. Selecciona tu USB (≥ 32 MB)
3. Boot selection: **Disk or ISO image**
4. Selecciona `build/kosmo-os.iso`
5. Esquema: **MBR** · Sistema destino: **BIOS (o UEFI-CSM)**
6. START

### USB físico (Ventoy)

1. Instala [Ventoy](https://ventoy.net) en el USB
2. Copia `build/kosmo-os.iso` a la partición Ventoy
3. Arranca desde USB y selecciona **Kosmo OS**

### VirtualBox

```bash
make disk-image                          # Genera kosmo-os.img
make run-vbox                            # Convierte a VMDK
# o manualmente:
VBoxManage convertdd build/kosmo-os.img build/kosmo-os.vmdk --format VMDK
```

---

## Estructura del proyecto

```
kosmo-os/
├── Makefile                    ← Build system completo
├── README.md
├── scripts/
│   ├── install.sh              ← Instalador de dependencias
│   └── run.sh                  ← Launcher QEMU interactivo
├── boot/
│   ├── stage1/boot.asm         ← MBR 512 bytes (LBA + CHS)
│   └── stage2/
│       ├── stage2.asm          ← A20, E820, GDT, modo protegido
│       └── a20.asm             ← 3 métodos de activación A20
├── kernel/
│   ├── linker.ld               ← Kernel en 0x100000
│   ├── arch/x86/
│   │   ├── entry.asm           ← Entry Multiboot + stack 16KB
│   │   ├── isr.asm             ← 48 stubs de interrupción
│   │   ├── gdt.c / .h          ← Global Descriptor Table
│   │   └── idt.c / .h          ← Interrupt Descriptor Table + PIC
│   └── core/
│       ├── kernel.c / .h       ← kernel_main(), init secuencial
│       └── panic.c / .h        ← BSOD + halt
├── drivers/
│   ├── video/
│   │   ├── vga.c / .h          ← Texto 80×25, scroll, cursor
│   │   └── vesa.c / .h         ← Framebuffer 800×600×32bpp
│   ├── input/
│   │   ├── keyboard.c / .h     ← PS/2 IRQ1, buffer circular
│   │   └── mouse.c / .h        ← PS/2 IRQ12, posición absoluta
│   └── timer/
│       └── pit.c / .h          ← PIT 100Hz, sleep_ms, uptime
├── fs/
│   └── kosmofs.c / .h          ← RAM disk, 256 inodos, 128 KB
├── shell/
│   ├── shell.c / .h            ← Bucle, historial, TAB, parser
│   └── commands/
│       ├── commands.c / .h     ← help, clear, about, echo, mem…
│       └── cmd_fs.c            ← ls, cat, write, cd, find, df…
├── gui/
│   ├── wm.c / .h               ← Window Manager, drag, taskbar
│   ├── desktop.c / .h          ← 4 ventanas del escritorio
│   └── font/
│       └── font8x8.c / .h      ← Fuente bitmap 8×8, 128 glifos
├── libc/
│   ├── string.c / .h           ← memset/cpy, str*, atoi, itoa
│   └── stdio.c / .h            ← kprintf, ksprintf (%d%s%x…)
└── include/
    ├── types.h                  ← uint8_t…, bool, PACKED, NORETURN
    ├── io.h                     ← inb/outb, sti/cli/hlt
    └── multiboot.h              ← multiboot_info_t, VBE info
```

---

## Mapa de memoria

```
0x00000000 - 0x000003FF   IVT (BIOS)
0x00000400 - 0x000004FF   BDA (BIOS Data Area)
0x00007C00 - 0x00007DFF   Stage1 (MBR)
0x00008000 - 0x0000FFFF   Stage2
0x00090000 - 0x0009FFFF   Stack del kernel (64 KB)
0x000A0000 - 0x000BFFFF   Memoria de video
0x000B8000 - 0x000BFFFF   Buffer VGA (texto)
0x000C0000 - 0x000FFFFF   BIOS ROM / reservada
0x00100000 - 0x00?????? ← Kernel (cargado por GRUB)
0x00?????? - fin RAM      RAM disponible
[Dirección física VBE]    Framebuffer VESA (800×600×4 = ~1.8 MB)
```

---

## Comandos del shell

```
help    about   ver     clear   echo    uptime  mem
ls      cat     write   append  rm      mkdir   rmdir
cd      pwd     df      touch   find    color   history
reboot  halt
```

---

## Secuencia de arranque

```
BIOS POST → MBR (Stage1) → Stage2
  → A20 activada (3 métodos)
  → Mapa de memoria E820
  → GDT temporal → Modo protegido 32-bit
  → GRUB2 carga el kernel ELF (Multiboot)
    → kernel_main()
      → VGA init (log de arranque)
      → GDT completa (6 descriptores, ring 0/3)
      → IDT + PIC remapeado (IRQ0→32, IRQ8→40)
      → PIT 100Hz
      → PS/2 Keyboard (IRQ1)
      → KosmoFS init (128 KB RAM disk)
      → VESA VBE init (800×600×32bpp)
      → PS/2 Mouse (IRQ12)
      → Window Manager + Escritorio
      → [Bucle infinito con HLT]
```

---

## Licencia

MIT — Libre para usar, modificar y distribuir.

> *"Fast, light, and real."*
