/* =============================================================================
 * KOSMO OS — Driver de Teclado PS/2
 * Archivo : drivers/input/keyboard.h
 * Función : Driver completo del teclado PS/2.
 *           - Captura scancodes por IRQ1
 *           - Traduce scancode → ASCII (set 1, QWERTY)
 *           - Gestiona modificadores: Shift, Ctrl, Alt, CapsLock, NumLock
 *           - Buffer circular de 256 teclas con lectura no bloqueante y bloqueante
 * ============================================================================= */

#ifndef KOSMO_KEYBOARD_H
#define KOSMO_KEYBOARD_H

#include "types.h"

/* =============================================================================
 * CONSTANTES
 * ============================================================================= */

#define KB_BUFFER_SIZE      256         /* Tamaño del buffer circular */

/* Teclas especiales (códigos virtuales internos, > 127) */
#define KEY_NONE            0x00
#define KEY_ESCAPE          0x1B
#define KEY_BACKSPACE       0x08
#define KEY_TAB             0x09
#define KEY_ENTER           0x0A
#define KEY_SPACE           0x20
#define KEY_DELETE          0x7F

/* Teclas de función */
#define KEY_F1              0x80
#define KEY_F2              0x81
#define KEY_F3              0x82
#define KEY_F4              0x83
#define KEY_F5              0x84
#define KEY_F6              0x85
#define KEY_F7              0x86
#define KEY_F8              0x87
#define KEY_F9              0x88
#define KEY_F10             0x89
#define KEY_F11             0x8A
#define KEY_F12             0x8B

/* Teclas de navegación */
#define KEY_UP              0x90
#define KEY_DOWN            0x91
#define KEY_LEFT            0x92
#define KEY_RIGHT           0x93
#define KEY_HOME            0x94
#define KEY_END             0x95
#define KEY_PAGE_UP         0x96
#define KEY_PAGE_DOWN       0x97
#define KEY_INSERT          0x98

/* Modificadores (estado de bits, NO en el buffer) */
#define KEY_MOD_SHIFT       (1 << 0)
#define KEY_MOD_CTRL        (1 << 1)
#define KEY_MOD_ALT         (1 << 2)
#define KEY_MOD_CAPSLOCK    (1 << 3)
#define KEY_MOD_NUMLOCK     (1 << 4)
#define KEY_MOD_SCROLLLOCK  (1 << 5)

/* =============================================================================
 * ESTRUCTURA DE UN EVENTO DE TECLA
 * ============================================================================= */
typedef struct {
    uint8_t  ascii;         /* Carácter ASCII (0 si no es imprimible) */
    uint8_t  scancode;      /* Scancode raw del teclado */
    uint8_t  modifiers;     /* Estado de modificadores en el momento */
    bool     pressed;       /* true = pulsada, false = soltada */
} key_event_t;

/* =============================================================================
 * API DEL DRIVER DE TECLADO
 * ============================================================================= */

/* Inicializar el driver */
void keyboard_init(void);

/* Leer el estado actual de los modificadores */
uint8_t keyboard_get_modifiers(void);

/* ── Lectura de caracteres ASCII ────────────────────────────────────────── */

/* Retorna el siguiente carácter ASCII del buffer (0 si buffer vacío) */
char keyboard_getchar(void);

/* Bloquea hasta que hay un carácter y lo retorna */
char keyboard_getchar_blocking(void);

/* Retorna true si hay datos en el buffer */
bool keyboard_has_data(void);

/* ── Lectura de línea completa ─────────────────────────────────────────── */

/* Leer una línea (hasta Enter), con eco en VGA. Retorna longitud.
 * buf debe tener al menos max_len bytes. Soporta Backspace. */
uint32_t keyboard_readline(char* buf, uint32_t max_len);

/* ── Handler IRQ (interno, llamado desde IDT) ──────────────────────────── */
void keyboard_irq_handler(void* frame);

#endif /* KOSMO_KEYBOARD_H */
