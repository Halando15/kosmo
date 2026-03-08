/* =============================================================================
 * KOSMO OS — Driver VGA Modo Texto (Implementación)
 * Archivo : drivers/video/vga.c
 * Función : Implementa escritura en el buffer de memoria VGA (0xB8000).
 *           Soporta: colores 16, cursor HW, scroll automático, posición x/y.
 *           Compatible con modo texto 80x25 estándar de cualquier PC x86.
 * ============================================================================= */

#include "vga.h"
#include "io.h"
#include "types.h"

/* =============================================================================
 * ESTADO INTERNO DEL DRIVER
 * ============================================================================= */

/* Puntero al buffer VGA en memoria (volátil: el compilador no puede cachear) */
static volatile uint16_t* const vga_buffer = (volatile uint16_t*)VGA_MEMORY;

/* Posición actual del cursor de escritura */
static uint8_t cursor_col = 0;
static uint8_t cursor_row = 0;

/* Atributo de color actual (byte alto de cada celda VGA) */
static uint8_t current_attr = VGA_DEFAULT_ATTR;

/* =============================================================================
 * FUNCIONES PRIVADAS (INTERNAS)
 * ============================================================================= */

/**
 * vga_entry — Construir una celda VGA de 16 bits
 * @c   : carácter ASCII
 * @attr: byte de atributo (color)
 */
static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)((uint8_t)c) | ((uint16_t)attr << 8);
}

/**
 * vga_index — Calcular el índice en el buffer para columna/fila
 */
static inline uint16_t vga_index(uint8_t col, uint8_t row) {
    return (uint16_t)(row * VGA_WIDTH + col);
}

/**
 * hw_cursor_update — Actualizar la posición del cursor hardware VGA
 * Comunica la posición al controlador VGA a través de los puertos 0x3D4/0x3D5
 */
static void hw_cursor_update(uint8_t col, uint8_t row) {
    uint16_t pos = vga_index(col, row);

    /* Registro 14 (0x0E): byte alto de la posición del cursor */
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)(pos >> 8));

    /* Registro 15 (0x0F): byte bajo de la posición del cursor */
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

/* =============================================================================
 * INICIALIZACIÓN
 * ============================================================================= */

/**
 * vga_init — Inicializar el driver VGA
 * Limpia la pantalla, posiciona cursor en (0,0), activa cursor hardware.
 */
void vga_init(void) {
    current_attr = VGA_DEFAULT_ATTR;
    cursor_col   = 0;
    cursor_row   = 0;

    /* Activar cursor con líneas 14-15 (cursor de bloque bajo) */
    vga_cursor_enable(14, 15);

    /* Limpiar toda la pantalla con el color por defecto */
    vga_clear();
}

/* =============================================================================
 * CONTROL DE COLOR
 * ============================================================================= */

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    current_attr = VGA_ATTR(fg, bg);
}

void vga_set_attr(uint8_t attr) {
    current_attr = attr;
}

uint8_t vga_get_attr(void) {
    return current_attr;
}

/* =============================================================================
 * ESCRITURA DE CARACTERES
 * ============================================================================= */

/**
 * vga_putchar_at — Escribir un carácter en una posición concreta del buffer
 * No mueve el cursor, escribe directamente en la celda indicada.
 */
void vga_putchar_at(char c, uint8_t attr, uint8_t col, uint8_t row) {
    vga_buffer[vga_index(col, row)] = vga_entry(c, attr);
}

/**
 * vga_putchar — Escribir un carácter en la posición actual del cursor
 * Gestiona: \n (salto de línea), \r (retorno de carro), \t (tabulación),
 * \b (retroceso), scroll automático al llegar al final.
 */
void vga_putchar(char c) {
    switch (c) {

        case '\n':  /* Salto de línea */
            cursor_col = 0;
            cursor_row++;
            break;

        case '\r':  /* Retorno de carro */
            cursor_col = 0;
            break;

        case '\t':  /* Tabulación: avanzar hasta el siguiente múltiplo de 4 */
            cursor_col = (uint8_t)((cursor_col + 4) & ~3);
            if (cursor_col >= VGA_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
            break;

        case '\b':  /* Retroceso */
            if (cursor_col > 0) {
                cursor_col--;
            } else if (cursor_row > 0) {
                cursor_row--;
                cursor_col = VGA_WIDTH - 1;
            }
            /* Borrar el carácter en la nueva posición */
            vga_buffer[vga_index(cursor_col, cursor_row)] =
                vga_entry(' ', current_attr);
            break;

        default:    /* Carácter imprimible */
            vga_buffer[vga_index(cursor_col, cursor_row)] =
                vga_entry(c, current_attr);
            cursor_col++;

            /* Salto de línea automático al llegar al borde derecho */
            if (cursor_col >= VGA_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
            break;
    }

    /* Scroll si hemos llegado al final de la pantalla */
    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
    }

    /* Actualizar cursor hardware */
    hw_cursor_update(cursor_col, cursor_row);
}

/* =============================================================================
 * ESCRITURA DE STRINGS
 * ============================================================================= */

void vga_puts(const char* str) {
    if (!str) return;
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_puts_color(const char* str, vga_color_t fg, vga_color_t bg) {
    uint8_t saved = current_attr;
    vga_set_color(fg, bg);
    vga_puts(str);
    current_attr = saved;
}

/* =============================================================================
 * CONTROL DE PANTALLA
 * ============================================================================= */

/**
 * vga_clear — Limpiar la pantalla completamente
 * Llena todo el buffer con espacios en blanco con el color actual.
 */
void vga_clear(void) {
    uint16_t blank = vga_entry(' ', current_attr);
    for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    cursor_col = 0;
    cursor_row = 0;
    hw_cursor_update(0, 0);
}

void vga_clear_color(vga_color_t fg, vga_color_t bg) {
    vga_set_color(fg, bg);
    vga_clear();
}

/**
 * vga_scroll — Desplazar la pantalla una línea hacia arriba
 * Copia las filas 1..24 a las filas 0..23 y limpia la fila 24.
 */
void vga_scroll(void) {
    /* Mover cada fila una posición hacia arriba */
    for (uint8_t row = 0; row < VGA_HEIGHT - 1; row++) {
        for (uint8_t col = 0; col < VGA_WIDTH; col++) {
            vga_buffer[vga_index(col, row)] =
                vga_buffer[vga_index(col, (uint8_t)(row + 1))];
        }
    }

    /* Limpiar la última fila */
    uint16_t blank = vga_entry(' ', current_attr);
    for (uint8_t col = 0; col < VGA_WIDTH; col++) {
        vga_buffer[vga_index(col, VGA_HEIGHT - 1)] = blank;
    }

    /* Retroceder cursor a la última fila */
    if (cursor_row > 0) {
        cursor_row = VGA_HEIGHT - 1;
    }
}

/* =============================================================================
 * CONTROL DE CURSOR HARDWARE
 * ============================================================================= */

/**
 * vga_cursor_enable — Activar cursor parpadeante
 * @start: línea de inicio del cursor (0-15)
 * @end  : línea de fin del cursor (0-15)
 * Para cursor de subrayado: start=13, end=14
 * Para cursor de bloque:    start=0,  end=15
 */
void vga_cursor_enable(uint8_t start, uint8_t end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (uint8_t)((inb(0x3D5) & 0xC0) | start));
    outb(0x3D4, 0x0B);
    outb(0x3D5, (uint8_t)((inb(0x3D5) & 0xE0) | end));
}

/**
 * vga_cursor_disable — Desactivar cursor (ocultarlo)
 * Pone el bit 5 del registro de cursor a 1 (cursor off).
 */
void vga_cursor_disable(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_set_cursor(uint8_t col, uint8_t row) {
    cursor_col = col;
    cursor_row = row;
    hw_cursor_update(col, row);
}

void vga_get_cursor(uint8_t* col, uint8_t* row) {
    if (col) *col = cursor_col;
    if (row) *row = cursor_row;
}

uint8_t vga_col(void) { return cursor_col; }
uint8_t vga_row(void) { return cursor_row; }

/* =============================================================================
 * UTILIDADES DE IMPRESIÓN NUMÉRICA
 * ============================================================================= */

/**
 * vga_print_hex — Imprimir un uint32_t en hexadecimal (ej: 0x00100000)
 */
void vga_print_hex(uint32_t value) {
    static const char hex_chars[] = "0123456789ABCDEF";
    char buf[11];   /* "0x" + 8 dígitos + '\0' */
    buf[0]  = '0';
    buf[1]  = 'x';
    buf[10] = '\0';

    for (int i = 9; i >= 2; i--) {
        buf[i] = hex_chars[value & 0xF];
        value >>= 4;
    }
    vga_puts(buf);
}

/**
 * vga_print_dec — Imprimir un uint32_t en decimal
 */
void vga_print_dec(uint32_t value) {
    if (value == 0) {
        vga_putchar('0');
        return;
    }

    char buf[12];   /* máx 10 dígitos + '\0' */
    int i = 10;
    buf[11] = '\0';
    buf[10] = '\0';

    while (value > 0 && i >= 0) {
        buf[i--] = (char)('0' + (value % 10));
        value /= 10;
    }
    vga_puts(&buf[i + 1]);
}
