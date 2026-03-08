/* =============================================================================
 * KOSMO OS — Kernel Panic (Implementación)
 * Archivo : kernel/core/panic.c
 * Función : Muestra una pantalla de error estilo BSOD y detiene el sistema.
 * ============================================================================= */

#include "panic.h"
#include "vga.h"
#include "stdio.h"
#include "io.h"

void kernel_panic(const char* message, const char* file, uint32_t line) {
    cli();

    /* Pantalla azul con texto blanco */
    vga_clear_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);

    /* Borde superior */
    vga_set_cursor(0, 1);
    for (int i = 0; i < VGA_WIDTH; i++) vga_putchar('=');

    vga_set_cursor(2, 3);
    kprintf("  *** KOSMO OS KERNEL PANIC ***");

    vga_set_cursor(2, 5);
    kprintf("  A fatal error occurred and the system was halted.");
    kprintf("\n  to prevent damage to your computer.");

    vga_set_cursor(2, 8);
    kprintf("  Error: %s", message);

    vga_set_cursor(2, 10);
    kprintf("  File:  %s", file);
    kprintf("\n  Line:  %u", line);

    vga_set_cursor(2, 13);
    kprintf("  If this is the first time you see this screen,");
    kprintf("\n  restart your computer. If this screen appears");
    kprintf("\n  again, check your hardware configuration.");

    vga_set_cursor(2, 18);
    kprintf("  Press any key to... do nothing. System halted.");

    /* Borde inferior */
    vga_set_cursor(0, 23);
    for (int i = 0; i < VGA_WIDTH; i++) vga_putchar('=');

    /* Halt permanente */
    for (;;) {
        cli();
        hlt();
    }
}
