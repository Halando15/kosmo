/* =============================================================================
 * KOSMO OS — Driver VGA Modo Texto
 * Archivo : drivers/video/vga.h
 * Función : API pública del driver de pantalla en modo texto 80x25.
 *           Gestiona escritura de caracteres, colores, scroll y cursor.
 * ============================================================================= */

#ifndef KOSMO_VGA_H
#define KOSMO_VGA_H

#include "types.h"

/* =============================================================================
 * CONSTANTES DEL MODO TEXTO VGA
 * ============================================================================= */

#define VGA_WIDTH       80          /* Columnas */
#define VGA_HEIGHT      25          /* Filas */
#define VGA_MEMORY      0xB8000     /* Dirección física del buffer VGA */

/* Cada celda VGA ocupa 2 bytes:
 *   Byte bajo  = código ASCII del carácter
 *   Byte alto  = atributo (color fondo + color texto)
 *   Formato atributo: [BBBBIIII]
 *     BBBB = color de fondo (4 bits)
 *     IIII = color de texto (4 bits, bit 3 = brillo)
 */

/* =============================================================================
 * COLORES VGA (16 colores)
 * ============================================================================= */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

/* Construir byte de atributo VGA a partir de color de fondo y texto */
#define VGA_ATTR(fg, bg)    (((bg) << 4) | (fg))

/* Colores por defecto del sistema */
#define VGA_DEFAULT_FG      VGA_COLOR_LIGHT_GREY
#define VGA_DEFAULT_BG      VGA_COLOR_BLACK
#define VGA_DEFAULT_ATTR    VGA_ATTR(VGA_DEFAULT_FG, VGA_DEFAULT_BG)

/* =============================================================================
 * API DEL DRIVER VGA
 * ============================================================================= */

/* Inicialización */
void vga_init(void);

/* Control de color */
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_set_attr(uint8_t attr);
uint8_t vga_get_attr(void);

/* Escritura de caracteres */
void vga_putchar(char c);
void vga_putchar_at(char c, uint8_t attr, uint8_t col, uint8_t row);

/* Escritura de strings */
void vga_puts(const char* str);
void vga_puts_color(const char* str, vga_color_t fg, vga_color_t bg);

/* Control de pantalla */
void vga_clear(void);
void vga_clear_color(vga_color_t fg, vga_color_t bg);
void vga_scroll(void);

/* Control de cursor */
void vga_set_cursor(uint8_t col, uint8_t row);
void vga_get_cursor(uint8_t* col, uint8_t* row);
void vga_cursor_enable(uint8_t start, uint8_t end);
void vga_cursor_disable(void);

/* Posición actual */
uint8_t vga_col(void);
uint8_t vga_row(void);

/* Utilidad: imprimir número hexadecimal */
void vga_print_hex(uint32_t value);
void vga_print_dec(uint32_t value);

#endif /* KOSMO_VGA_H */
