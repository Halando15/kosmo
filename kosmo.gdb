################################################################################
# KOSMO OS — GDB Helper Script
# Archivo : scripts/debug/kosmo.gdb
# Uso     :
#   Terminal 1:  make run-debug
#   Terminal 2:  gdb -x scripts/debug/kosmo.gdb build/kernel.elf
################################################################################

# ── Configuración básica ──────────────────────────────────────────────────────
set architecture i386
set disassembly-flavor intel
set print pretty on
set print array on
set print array-indexes on
set pagination off

# Cargar el ELF del kernel con símbolos de debug
file build/kernel.elf

# Conectar al QEMU corriendo en modo debug
target remote :1234

echo \n[Kosmo OS GDB] Connected to QEMU. Symbols loaded.\n

# ── Breakpoints iniciales ─────────────────────────────────────────────────────
break kernel_main
break panic_handler
break kfs_init

# Breakpoints opcionales (comentados para mayor velocidad):
# break gdt_init
# break idt_init
# break vesa_init
# break wm_run
# break shell_start

# ── Comandos personalizados ───────────────────────────────────────────────────

# Mostrar información del estado del sistema en el punto actual
define kosmo-info
    echo \n=== Kosmo OS Kernel State ===\n
    echo CPU Registers:\n
    info registers
    echo \nStack Backtrace:\n
    backtrace 10
    echo \nCurrent instruction:\n
    x/5i $eip
end
document kosmo-info
  Show CPU state, registers, and backtrace at current position.
end

# Inspeccionar la GDT
define show-gdt
    echo \nGDT entries (base from GDTR):\n
    set $gdtr_limit = 0
    set $gdtr_base  = 0
    # Leer GDTR directamente
    p/x (unsigned int)&gdt_entries
    x/48bx &gdt_entries
end
document show-gdt
  Dump GDT entries as raw bytes.
end

# Inspeccionar la IDT
define show-idt
    echo \nIDT (first 16 entries):\n
    x/128bx &idt_entries
end
document show-idt
  Dump first 16 IDT entries.
end

# Inspeccionar el framebuffer VGA
define show-vga
    echo \nVGA text buffer (first 5 rows, 80 cols each):\n
    x/80hx 0xB8000
    x/80hx 0xB80A0
    x/80hx 0xB8140
    x/80hx 0xB81E0
    x/80hx 0xB8280
end
document show-vga
  Dump VGA text buffer (first 5 rows).
end

# Ver tabla de inodos de KosmoFS
define show-kfs
    echo \nKosmoFS inode 0 (root):\n
    p kfs.inodes[0]
    echo \nKosmoFS statistics:\n
    p kfs.total_files
    p kfs.total_dirs
    p kfs.mounted
end
document show-kfs
  Show KosmoFS internal state and root inode.
end

# Monitorizar interrupciones
define watch-irq
    echo \nWatching IRQ handler dispatch...\n
    break isr_dispatch
    commands
        silent
        printf "IRQ/EXC %d at EIP=0x%08x\n", $edi, $eip
        continue
    end
end
document watch-irq
  Set a silent breakpoint on isr_dispatch to trace all interrupts.
end

# Inspeccionar el stack del kernel
define show-stack
    echo \nKernel stack (ESP → ESP+128):\n
    x/32wx $esp
end
document show-stack
  Dump the top of the kernel stack.
end

# Breakpoint en panic con volcado automático
define hook-panic
    break panic_handler
    commands
        echo \n!!! KERNEL PANIC CAUGHT BY GDB !!!\n
        kosmo-info
        show-stack
        echo Press 'c' to continue or 'q' to quit.\n
    end
end
document hook-panic
  Install a breakpoint on panic_handler with automatic state dump.
end

# Trazar ejecución instrucción a instrucción con log
define trace-from
    set logging on
    set logging file build/trace.log
    echo Tracing to build/trace.log...\n
    while 1
        x/1i $eip
        stepi
    end
end
document trace-from
  Trace instruction by instruction, logging to build/trace.log.
  Interrupt with Ctrl+C.
end

# ── Acciones al conectar ──────────────────────────────────────────────────────
echo \n[Kosmo OS GDB] Breakpoints set:\n
info breakpoints

echo \n[Kosmo OS GDB] Custom commands available:\n
echo   kosmo-info  — CPU state + backtrace\n
echo   show-gdt    — GDT entries\n
echo   show-idt    — IDT entries\n
echo   show-vga    — VGA text buffer\n
echo   show-kfs    — KosmoFS state\n
echo   show-stack  — Kernel stack\n
echo   watch-irq   — Trace all interrupts\n
echo   hook-panic  — Auto-dump on kernel panic\n
echo \nType 'continue' (or 'c') to start execution.\n

# Instalamos el hook de panic automáticamente
hook-panic

# ── Continuar la ejecución ────────────────────────────────────────────────────
continue
