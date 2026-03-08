/* =============================================================================
 * KOSMO OS — Driver de Teclado PS/2 (Implementación)
 * Archivo : drivers/input/keyboard.c
 * Función : Captura IRQ1 del teclado PS/2, decodifica scancodes del Set 1
 *           (PC XT/AT), gestiona modificadores y llena el buffer circular.
 *
 * Scancode Set 1 (el más compatible):
 *   Byte 0x01-0x58: tecla pulsada
 *   Byte 0x81-0xD8: tecla soltada (= pulsada | 0x80)
 *   Byte 0xE0:      prefijo de tecla extendida (flechas, Insert, etc.)
 * ============================================================================= */

#include "keyboard.h"
#include "idt.h"
#include "io.h"
#include "vga.h"
#include "stdio.h"
#include "string.h"

/* =============================================================================
 * TABLAS DE SCANCODES → ASCII
 * Scancode Set 1, layout QWERTY US (compatible con la mayoría de PCs)
 * Índice = scancode (1-58)
 * Valor  = carácter ASCII sin shift / con Shift
 * ============================================================================= */

/* Sin Shift / Sin CapsLock */
static const char scancode_normal[128] = {
    0,      /* 0x00 - No usado */
    0x1B,   /* 0x01 - Escape */
    '1',    /* 0x02 */
    '2',    /* 0x03 */
    '3',    /* 0x04 */
    '4',    /* 0x05 */
    '5',    /* 0x06 */
    '6',    /* 0x07 */
    '7',    /* 0x08 */
    '8',    /* 0x09 */
    '9',    /* 0x0A */
    '0',    /* 0x0B */
    '-',    /* 0x0C */
    '=',    /* 0x0D */
    '\b',   /* 0x0E - Backspace */
    '\t',   /* 0x0F - Tab */
    'q',    /* 0x10 */
    'w',    /* 0x11 */
    'e',    /* 0x12 */
    'r',    /* 0x13 */
    't',    /* 0x14 */
    'y',    /* 0x15 */
    'u',    /* 0x16 */
    'i',    /* 0x17 */
    'o',    /* 0x18 */
    'p',    /* 0x19 */
    '[',    /* 0x1A */
    ']',    /* 0x1B */
    '\n',   /* 0x1C - Enter */
    0,      /* 0x1D - Left Ctrl */
    'a',    /* 0x1E */
    's',    /* 0x1F */
    'd',    /* 0x20 */
    'f',    /* 0x21 */
    'g',    /* 0x22 */
    'h',    /* 0x23 */
    'j',    /* 0x24 */
    'k',    /* 0x25 */
    'l',    /* 0x26 */
    ';',    /* 0x27 */
    '\'',   /* 0x28 */
    '`',    /* 0x29 */
    0,      /* 0x2A - Left Shift */
    '\\',   /* 0x2B */
    'z',    /* 0x2C */
    'x',    /* 0x2D */
    'c',    /* 0x2E */
    'v',    /* 0x2F */
    'b',    /* 0x30 */
    'n',    /* 0x31 */
    'm',    /* 0x32 */
    ',',    /* 0x33 */
    '.',    /* 0x34 */
    '/',    /* 0x35 */
    0,      /* 0x36 - Right Shift */
    '*',    /* 0x37 - Keypad * */
    0,      /* 0x38 - Left Alt */
    ' ',    /* 0x39 - Space */
    0,      /* 0x3A - CapsLock */
    KEY_F1, /* 0x3B */
    KEY_F2, /* 0x3C */
    KEY_F3, /* 0x3D */
    KEY_F4, /* 0x3E */
    KEY_F5, /* 0x3F */
    KEY_F6, /* 0x40 */
    KEY_F7, /* 0x41 */
    KEY_F8, /* 0x42 */
    KEY_F9, /* 0x43 */
    KEY_F10,/* 0x44 */
    0,      /* 0x45 - NumLock */
    0,      /* 0x46 - ScrollLock */
    '7',    /* 0x47 - Keypad 7 / Home */
    '8',    /* 0x48 - Keypad 8 / Up */
    '9',    /* 0x49 - Keypad 9 / PgUp */
    '-',    /* 0x4A - Keypad - */
    '4',    /* 0x4B - Keypad 4 / Left */
    '5',    /* 0x4C - Keypad 5 */
    '6',    /* 0x4D - Keypad 6 / Right */
    '+',    /* 0x4E - Keypad + */
    '1',    /* 0x4F - Keypad 1 / End */
    '2',    /* 0x50 - Keypad 2 / Down */
    '3',    /* 0x51 - Keypad 3 / PgDn */
    '0',    /* 0x52 - Keypad 0 / Ins */
    '.',    /* 0x53 - Keypad . / Del */
    0, 0, 0,/* 0x54-0x56 */
    KEY_F11,/* 0x57 */
    KEY_F12,/* 0x58 */
    /* 0x59-0x7F: sin definir */
};

/* Con Shift (o CapsLock para letras) */
static const char scancode_shift[128] = {
    0,
    0x1B,   /* Escape */
    '!',    /* 1 */
    '@',    /* 2 */
    '#',    /* 3 */
    '$',    /* 4 */
    '%',    /* 5 */
    '^',    /* 6 */
    '&',    /* 7 */
    '*',    /* 8 */
    '(',    /* 9 */
    ')',    /* 0 */
    '_',    /* - */
    '+',    /* = */
    '\b',   /* Backspace */
    '\t',   /* Tab */
    'Q',    /* q */
    'W',    /* w */
    'E',    /* e */
    'R',    /* r */
    'T',    /* t */
    'Y',    /* y */
    'U',    /* u */
    'I',    /* i */
    'O',    /* o */
    'P',    /* p */
    '{',    /* [ */
    '}',    /* ] */
    '\n',   /* Enter */
    0,      /* Ctrl */
    'A',    /* a */
    'S',    /* s */
    'D',    /* d */
    'F',    /* f */
    'G',    /* g */
    'H',    /* h */
    'J',    /* j */
    'K',    /* k */
    'L',    /* l */
    ':',    /* ; */
    '"',    /* ' */
    '~',    /* ` */
    0,      /* Left Shift */
    '|',    /* \ */
    'Z',    /* z */
    'X',    /* x */
    'C',    /* c */
    'V',    /* v */
    'B',    /* b */
    'N',    /* n */
    'M',    /* m */
    '<',    /* , */
    '>',    /* . */
    '?',    /* / */
    0,      /* Right Shift */
    '*',
    0,      /* Alt */
    ' ',
    0,      /* CapsLock */
};

/* =============================================================================
 * ESTADO INTERNO DEL DRIVER
 * ============================================================================= */

/* Buffer circular de caracteres ASCII */
static volatile char     kb_buffer[KB_BUFFER_SIZE];
static volatile uint32_t kb_buf_read  = 0;   /* Índice de lectura */
static volatile uint32_t kb_buf_write = 0;   /* Índice de escritura */

/* Estado de modificadores */
static volatile uint8_t  kb_modifiers = 0;

/* Prefijo E0: la próxima tecla es extendida */
static volatile bool     kb_extended  = false;

/* =============================================================================
 * BUFFER CIRCULAR — OPERACIONES INTERNAS
 * ============================================================================= */

static inline bool buf_full(void) {
    return ((kb_buf_write + 1) % KB_BUFFER_SIZE) == kb_buf_read;
}

static inline bool buf_empty(void) {
    return kb_buf_read == kb_buf_write;
}

static inline void buf_push(char c) {
    if (!buf_full()) {
        kb_buffer[kb_buf_write] = c;
        kb_buf_write = (kb_buf_write + 1) % KB_BUFFER_SIZE;
    }
}

static inline char buf_pop(void) {
    if (buf_empty()) return 0;
    char c = kb_buffer[kb_buf_read];
    kb_buf_read = (kb_buf_read + 1) % KB_BUFFER_SIZE;
    return c;
}

/* =============================================================================
 * HANDLER DE IRQ1 — NÚCLEO DEL DRIVER
 * ============================================================================= */

/**
 * keyboard_irq_handler — Llamado por la IDT en cada pulsación/suelta
 * Lee el scancode del puerto 0x60 y lo procesa.
 */
void keyboard_irq_handler(void* frame) {
    (void)frame;

    /* Esperar a que el buffer de salida del controlador esté listo */
    uint8_t status = inb(KB_STATUS);
    if (!(status & 0x01)) return;       /* No hay dato disponible */

    uint8_t scancode = inb(KB_DATA);    /* Leer scancode */

    /* ── Prefijo E0: siguiente scancode es tecla extendida ─────────────── */
    if (scancode == 0xE0) {
        kb_extended = true;
        return;
    }

    /* ── Determinar si es pulsación (bit 7 = 0) o suelta (bit 7 = 1) ──── */
    bool released = (scancode & 0x80) != 0;
    uint8_t sc    = scancode & 0x7F;    /* Scancode sin el bit de suelta */

    /* ── Procesar teclas extendidas (prefijo E0) ─────────────────────── */
    if (kb_extended) {
        kb_extended = false;
        if (!released) {
            char vk = 0;
            switch (sc) {
                case 0x48: vk = KEY_UP;        break;
                case 0x50: vk = KEY_DOWN;       break;
                case 0x4B: vk = KEY_LEFT;       break;
                case 0x4D: vk = KEY_RIGHT;      break;
                case 0x47: vk = KEY_HOME;       break;
                case 0x4F: vk = KEY_END;        break;
                case 0x49: vk = KEY_PAGE_UP;    break;
                case 0x51: vk = KEY_PAGE_DOWN;  break;
                case 0x52: vk = KEY_INSERT;     break;
                case 0x53: vk = KEY_DELETE;     break;
                default:   break;
            }
            if (vk) buf_push(vk);
        }
        return;
    }

    /* ── Actualizar estado de modificadores ─────────────────────────────── */
    switch (sc) {
        case 0x2A:  /* Left Shift */
        case 0x36:  /* Right Shift */
            if (released) kb_modifiers &= ~KEY_MOD_SHIFT;
            else           kb_modifiers |=  KEY_MOD_SHIFT;
            return;

        case 0x1D:  /* Left Ctrl */
            if (released) kb_modifiers &= ~KEY_MOD_CTRL;
            else           kb_modifiers |=  KEY_MOD_CTRL;
            return;

        case 0x38:  /* Left Alt */
            if (released) kb_modifiers &= ~KEY_MOD_ALT;
            else           kb_modifiers |=  KEY_MOD_ALT;
            return;

        case 0x3A:  /* CapsLock — toggle en pulsación */
            if (!released) {
                kb_modifiers ^= KEY_MOD_CAPSLOCK;
                /* Actualizar LEDs del teclado */
                outb(KB_DATA, 0xED);
                while (inb(KB_STATUS) & 0x02);     /* Esperar ACK */
                uint8_t leds = 0;
                if (kb_modifiers & KEY_MOD_SCROLLLOCK) leds |= 0x01;
                if (kb_modifiers & KEY_MOD_NUMLOCK)    leds |= 0x02;
                if (kb_modifiers & KEY_MOD_CAPSLOCK)   leds |= 0x04;
                outb(KB_DATA, leds);
            }
            return;

        case 0x45:  /* NumLock */
            if (!released) kb_modifiers ^= KEY_MOD_NUMLOCK;
            return;

        case 0x46:  /* ScrollLock */
            if (!released) kb_modifiers ^= KEY_MOD_SCROLLLOCK;
            return;
    }

    /* ── Ignorar eventos de suelta para teclas normales ─────────────────── */
    if (released) return;
    if (sc >= 128) return;

    /* ── Traducir scancode → ASCII ──────────────────────────────────────── */
    char ascii = 0;
    bool shift_active = (kb_modifiers & KEY_MOD_SHIFT) != 0;
    bool caps_active  = (kb_modifiers & KEY_MOD_CAPSLOCK) != 0;

    if (shift_active) {
        ascii = scancode_shift[sc];
    } else {
        ascii = scancode_normal[sc];
    }

    /* CapsLock solo afecta a letras (no a números ni símbolos) */
    if (caps_active && ascii >= 'a' && ascii <= 'z') {
        ascii = (char)(ascii - 32);     /* a-z → A-Z */
    } else if (caps_active && ascii >= 'A' && ascii <= 'Z' && shift_active) {
        ascii = (char)(ascii + 32);     /* Shift+CapsLock → minúsculas */
    }

    /* ── Ctrl + tecla: generar código de control ────────────────────────── */
    if ((kb_modifiers & KEY_MOD_CTRL) && ascii >= 'a' && ascii <= 'z') {
        ascii = (char)(ascii - 'a' + 1);    /* Ctrl+A = 0x01, Ctrl+C = 0x03, etc. */
    } else if ((kb_modifiers & KEY_MOD_CTRL) && ascii >= 'A' && ascii <= 'Z') {
        ascii = (char)(ascii - 'A' + 1);
    }

    /* ── Meter en el buffer si es un carácter válido ─────────────────────── */
    if (ascii != 0) {
        buf_push(ascii);
    }
}

/* =============================================================================
 * INICIALIZACIÓN
 * ============================================================================= */

/**
 * keyboard_init — Inicializar el driver de teclado
 * Resetea el estado, registra el handler en IRQ1 y activa la interrupción.
 */
void keyboard_init(void) {
    /* Limpiar estado */
    kb_buf_read  = 0;
    kb_buf_write = 0;
    kb_modifiers = 0;
    kb_extended  = false;
    memset((void*)kb_buffer, 0, KB_BUFFER_SIZE);

    /* Vaciar el buffer de datos del controlador PS/2 */
    while (inb(KB_STATUS) & 0x01) {
        inb(KB_DATA);
    }

    /* Registrar handler en IRQ1 (vector 33 en IDT) */
    idt_register_handler(33, (isr_handler_t)keyboard_irq_handler);

    /* Desenmascarar IRQ1 en el PIC */
    pic_unmask_irq(1);

    kprintf("  [ OK ]  PS/2 Keyboard (IRQ1, Scancode Set 1, QWERTY)\n");
}

/* =============================================================================
 * API DE LECTURA
 * ============================================================================= */

uint8_t keyboard_get_modifiers(void) {
    return kb_modifiers;
}

bool keyboard_has_data(void) {
    return !buf_empty();
}

char keyboard_getchar(void) {
    return buf_pop();
}

/**
 * keyboard_getchar_blocking — Espera hasta que haya una tecla
 * Usa HLT para no consumir CPU mientras espera.
 */
char keyboard_getchar_blocking(void) {
    while (buf_empty()) {
        __asm__ volatile("hlt");
    }
    return buf_pop();
}

/**
 * keyboard_readline — Leer una línea completa con eco en pantalla
 * @buf    : buffer de destino
 * @max_len: tamaño máximo (incluyendo '\0')
 * Retorna : longitud de la línea (sin '\0')
 *
 * Soporta:
 *  - Backspace: borrar carácter anterior
 *  - Enter: finalizar
 *  - Ctrl+C: cancelar (retorna 0, buf vacío)
 *  - Ctrl+L: limpiar pantalla
 */
uint32_t keyboard_readline(char* buf, uint32_t max_len) {
    if (!buf || max_len == 0) return 0;

    uint32_t pos = 0;
    memset(buf, 0, max_len);

    while (true) {
        char c = keyboard_getchar_blocking();

        /* Enter — fin de línea */
        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            buf[pos] = '\0';
            return pos;
        }

        /* Backspace */
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                buf[pos] = '\0';
                vga_putchar('\b');  /* El driver VGA maneja el borrado */
            }
            continue;
        }

        /* Ctrl+C — cancelar */
        if (c == 0x03) {
            vga_puts_color("^C\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            buf[0] = '\0';
            return 0;
        }

        /* Ctrl+L — limpiar pantalla */
        if (c == 0x0C) {
            vga_clear();
            continue;
        }

        /* Ignorar caracteres de control no manejados */
        if (c < 0x20 && c != '\t') continue;

        /* Añadir carácter al buffer si hay espacio */
        if (pos < max_len - 1) {
            buf[pos++] = c;
            vga_putchar(c);     /* Eco en pantalla */
        }
    }
}
