/* =============================================================================
 * KOSMO OS — Kernel Principal
 * Archivo : kernel/core/kernel.c
 * Función : Punto de entrada en C del kernel. kernel_main() es llamado
 *           desde entry.asm después de configurar el stack.
 *           Inicializa todos los subsistemas en orden y arranca la interfaz.
 * ============================================================================= */

#include "kernel.h"
#include "panic.h"
#include "../arch/x86/gdt.h"
#include "../arch/x86/idt.h"
#include "vga.h"
#include "io.h"
#include "multiboot.h"
#include "types.h"
#include "string.h"
#include "stdio.h"
#include "pit.h"
#include "keyboard.h"
#include "shell.h"
#include "kosmofs.h"

/* =============================================================================
 * ESTADO GLOBAL DEL SISTEMA
 * ============================================================================= */

system_info_t sys_info;

/* =============================================================================
 * FUNCIONES PRIVADAS DE ARRANQUE
 * ============================================================================= */

/**
 * bss_clear — Zerear la sección .bss del kernel
 * Es responsabilidad del kernel hacerlo, no del bootloader.
 * Necesario para que las variables globales sin inicializar sean 0.
 */
static void bss_clear(void) {
    uint8_t* start = (uint8_t*)&_bss_start;
    uint8_t* end   = (uint8_t*)&_bss_end;
    while (start < end) {
        *start++ = 0;
    }
}

/**
 * parse_multiboot — Extraer información del struct multiboot_info
 * Recoge: RAM total, nombre del bootloader.
 */
static void parse_multiboot(multiboot_info_t* mbi) {
    if (!mbi) return;

    /* Memoria */
    if (multiboot_has_mem(mbi)) {
        sys_info.total_ram_kb = mbi->mem_lower + mbi->mem_upper;
    }

    /* Nombre del bootloader */
    if (mbi->flags & MULTIBOOT_FLAG_LOADER_NAME) {
        const char* name = (const char*)(uintptr_t)mbi->boot_loader_name;
        strncpy(sys_info.bootloader_name, name, 63);
        sys_info.bootloader_name[63] = '\0';
    } else {
        strcpy(sys_info.bootloader_name, "Unknown");
    }
}

/**
 * print_separator — Imprime una línea decorativa
 */
static void print_separator(vga_color_t color) {
    vga_set_color(color, VGA_COLOR_BLACK);
    for (int i = 0; i < VGA_WIDTH; i++) vga_putchar('-');
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
}

/**
 * print_boot_logo — Muestra el logo de Kosmo OS en ASCII art
 */
static void print_boot_logo(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n");
    kprintf("  ##   ## ##  ##     ##   ####  ##   ##   ##  ##  ####\n");
    kprintf("  ##  ##  ##  ##    ##  ##  ## ###  ## ## ###  ##\n");
    kprintf("  ####   ##  ##   ##  ##  ## ## ## ## ## ## ##\n");
    kprintf("  ## ##   ##  ## ##  ##  ## ## ## ## ##  ##   ##\n");
    kprintf("  ##  ##   ####   ####  ####  ## ## ##  #### ####\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("\n");
}

/**
 * print_boot_header — Muestra la cabecera de arranque con info del sistema
 */
static void print_boot_header(void) {
    vga_clear_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);

    /* Logo */
    print_boot_logo();

    /* Línea de título */
    print_separator(VGA_COLOR_DARK_GREY);

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("  %s v%s \"%s\"",
            KOSMO_NAME, KOSMO_VERSION_STRING, KOSMO_CODENAME);
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("    |  x86 32-bit Protected Mode\n");

    print_separator(VGA_COLOR_DARK_GREY);
    kprintf("\n");
}

/**
 * print_sysinfo — Muestra tabla de información del sistema
 */
static void print_sysinfo(void) {
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprintf("  System Information\n");
    print_separator(VGA_COLOR_DARK_GREY);

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    kprintf("  Architecture  : x86 (i686) 32-bit\n");

    if (sys_info.total_ram_kb > 0) {
        uint32_t ram_mb = sys_info.total_ram_kb / 1024;
        kprintf("  Total RAM     : %u KB", sys_info.total_ram_kb);
        if (ram_mb > 0) kprintf(" (%u MB)", ram_mb);
        kprintf("\n");
    } else {
        kprintf("  Total RAM     : Unknown\n");
    }

    kprintf("  Bootloader    : %s\n", sys_info.bootloader_name);

    uint32_t kernel_end = (uint32_t)(uintptr_t)&_kernel_end;
    uint32_t kernel_size = kernel_end - 0x100000;
    kprintf("  Kernel base   : 0x%08X\n", (uint32_t)0x100000);
    kprintf("  Kernel end    : 0x%08X  (%u KB)\n",
            kernel_end, kernel_size / 1024);

    kprintf("  VGA Buffer    : 0x%08X  (Text 80x25)\n", (uint32_t)VGA_MEMORY);
    kprintf("\n");
}

/**
 * print_init_step — Imprimir resultado de un paso de inicialización
 */
static void print_ok(const char* name) {
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("  [ ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("OK");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf(" ]  %s\n", name);
}

static void print_fail(const char* name) {
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("  [");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    kprintf("FAIL");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("]  %s\n", name);
}

/* Suprimir "defined but not used" para print_fail, la usaremos en fases futuras */

/* =============================================================================
 * KERNEL MAIN — PUNTO DE ENTRADA EN C
 * ============================================================================= */

/**
 * kernel_main — Función principal del kernel
 * @magic : debe ser MULTIBOOT_BOOTLOADER_MAGIC (0x2BADB002) si arrancó GRUB
 * @mbi   : puntero a la estructura multiboot_info pasada por GRUB
 *
 * ORDEN DE INICIALIZACIÓN (importa: no cambiar el orden):
 *  1. BSS clear        — sin esto, variables globales no son 0
 *  2. VGA init         — a partir de aquí podemos mostrar texto
 *  3. Multiboot parse  — leer info de RAM y bootloader
 *  4. GDT init         — configurar segmentos de memoria
 *  5. IDT + PIC init   — registrar interrupciones
 *  6. STI              — habilitar interrupciones
 *  7. Subsistemas      — timer, teclado, fs...
 *  8. Shell / GUI      — interfaz de usuario
 */
void kernel_main(uint32_t magic, multiboot_info_t* mbi) {

    /* ── Paso 1: Limpiar BSS ─────────────────────────────────────────────── */
    bss_clear();

    /* ── Paso 2: Inicializar VGA (primera salida a pantalla) ─────────────── */
    vga_init();
    print_boot_header();

    /* ── Paso 3: Validar Multiboot ───────────────────────────────────────── */
    memset(&sys_info, 0, sizeof(sys_info));
    sys_info.boot_magic       = magic;
    sys_info.multiboot_valid  = multiboot_is_valid(magic);

    if (sys_info.multiboot_valid) {
        parse_multiboot(mbi);
    } else {
        strcpy(sys_info.bootloader_name, "Kosmo Stage2 Bootloader");
    }

    /* Mostrar info del sistema */
    print_sysinfo();

    /* ── Paso 4: Inicializar GDT ─────────────────────────────────────────── */
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprintf("  Initializing subsystems...\n");
    print_separator(VGA_COLOR_DARK_GREY);

    gdt_init();
    print_ok("GDT (Global Descriptor Table)");

    /* ── Paso 5: Inicializar PIC + IDT ───────────────────────────────────── */
    pic_init();
    print_ok("PIC 8259 (Interrupt Controller remapped)");

    idt_init();
    print_ok("IDT (Interrupt Descriptor Table, 48 vectors)");

    /* ── Paso 6: Habilitar interrupciones ────────────────────────────────── */
    sti();
    print_ok("Interrupts enabled (STI)");

    /* ── Paso 7: Los demás subsistemas se añadirán en las siguientes fases ─ */
    /* ── Paso 7: Driver Timer ───────────────────────────────────────────────── */
    pit_init();

    /* ── Paso 8: Driver Teclado ──────────────────────────────────────────────── */
    keyboard_init();
    /* Fase 5: shell_start()                */
    /* Fase 6: fs_init()                    */
    /* Fase 7: gui_start()                  */

    kprintf("\n");
    print_separator(VGA_COLOR_DARK_GREY);

    /* ── Pantalla de bienvenida final ────────────────────────────────────── */
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("\n  Kosmo OS is running!\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("  All core subsystems initialized successfully.\n\n");

    /* ── Paso 9: Filesystem ─────────────────────────────────────────────────── */
    kfs_init();

    /* ── Paso 10: Lanzar el shell (Fase 5) ─────────────────────────────────── */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n  All subsystems loaded. Starting shell...\n");
    sleep_ms(400);

    /* shell_start() contiene el bucle principal — no retorna */
    shell_start();

    /* ── Bucle principal del kernel ──────────────────────────────────────── */
    /* Por ahora, un bucle de espera de interrupciones (halt loop).
     * En Fase 5 aquí irá el dispatcher del shell.
     * En Fase 7 aquí irá el event loop de la GUI. */
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);

    for (;;) {
        hlt();  /* Pausar CPU hasta la próxima interrupción → ahorro de energía */
    }

    /* No se llega aquí nunca, pero por si acaso: */
    PANIC("kernel_main() returned unexpectedly!");
}
