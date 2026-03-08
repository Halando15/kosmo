/* =============================================================================
 * KOSMO OS — kprintf (Implementación)
 * Archivo : libc/stdio.c
 * Función : printf mínimo para el kernel. Soporta:
 *           %c  carácter          %s  string
 *           %d  decimal con signo %u  decimal sin signo
 *           %x  hexadecimal lower %X  hexadecimal upper
 *           %p  puntero (hex)     %%  literal '%'
 *           %b  binario           %o  octal
 * ============================================================================= */

#include "stdio.h"
#include "string.h"
#include "vga.h"

/* Librería de argumentos variables (compatible freestanding con GCC) */
typedef __builtin_va_list va_list;
#define va_start(v, l)  __builtin_va_start(v, l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v, T)    __builtin_va_arg(v, T)

/* =============================================================================
 * IMPLEMENTACIÓN INTERNA
 * ============================================================================= */

/* Escribir string con longitud en el buffer o en VGA */
static void write_str(char* out, int* out_pos, const char* s, int limit) {
    while (*s) {
        if (out) {
            if (*out_pos < limit - 1) out[(*out_pos)++] = *s;
        } else {
            vga_putchar(*s);
        }
        s++;
    }
}

static void write_char(char* out, int* out_pos, char c, int limit) {
    if (out) {
        if (*out_pos < limit - 1) out[(*out_pos)++] = c;
    } else {
        vga_putchar(c);
    }
}

/* Motor interno compartido por kprintf y ksprintf */
static int kprintf_internal(char* out, int limit, const char* fmt, va_list args) {
    int pos = 0;
    char numbuf[34];

    for (int i = 0; fmt[i]; i++) {
        if (fmt[i] != '%') {
            write_char(out, &pos, fmt[i], limit);
            continue;
        }

        i++;    /* Saltar '%' */

        /* Flags y ancho mínimo (soporte básico) */
        int zero_pad = 0, width = 0;
        if (fmt[i] == '0') { zero_pad = 1; i++; }
        while (fmt[i] >= '1' && fmt[i] <= '9') {
            width = width * 10 + (fmt[i++] - '0');
        }

        switch (fmt[i]) {

            case 'c': {
                char c = (char)va_arg(args, int);
                write_char(out, &pos, c, limit);
                break;
            }

            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                /* Padding */
                int slen = (int)strlen(s);
                for (int p = slen; p < width; p++)
                    write_char(out, &pos, ' ', limit);
                write_str(out, &pos, s, limit);
                break;
            }

            case 'd': {
                int val = va_arg(args, int);
                itoa(val, numbuf, 10);
                int nlen = (int)strlen(numbuf);
                char pad = zero_pad ? '0' : ' ';
                for (int p = nlen; p < width; p++)
                    write_char(out, &pos, pad, limit);
                write_str(out, &pos, numbuf, limit);
                break;
            }

            case 'u': {
                uint32_t val = va_arg(args, uint32_t);
                utoa(val, numbuf, 10);
                int nlen = (int)strlen(numbuf);
                char pad = zero_pad ? '0' : ' ';
                for (int p = nlen; p < width; p++)
                    write_char(out, &pos, pad, limit);
                write_str(out, &pos, numbuf, limit);
                break;
            }

            case 'x': {
                uint32_t val = va_arg(args, uint32_t);
                utoa(val, numbuf, 16);
                /* Convertir a minúsculas */
                for (int k = 0; numbuf[k]; k++)
                    if (numbuf[k] >= 'A' && numbuf[k] <= 'F')
                        numbuf[k] += 32;
                int nlen = (int)strlen(numbuf);
                char pad = zero_pad ? '0' : ' ';
                for (int p = nlen; p < width; p++)
                    write_char(out, &pos, pad, limit);
                write_str(out, &pos, numbuf, limit);
                break;
            }

            case 'X': {
                uint32_t val = va_arg(args, uint32_t);
                utoa(val, numbuf, 16);
                int nlen = (int)strlen(numbuf);
                char pad = zero_pad ? '0' : ' ';
                for (int p = nlen; p < width; p++)
                    write_char(out, &pos, pad, limit);
                write_str(out, &pos, numbuf, limit);
                break;
            }

            case 'p': {
                uint32_t val = (uint32_t)(uintptr_t)va_arg(args, void*);
                write_str(out, &pos, "0x", limit);
                utoa(val, numbuf, 16);
                /* Padding a 8 dígitos para punteros */
                int nlen = (int)strlen(numbuf);
                for (int p = nlen; p < 8; p++)
                    write_char(out, &pos, '0', limit);
                write_str(out, &pos, numbuf, limit);
                break;
            }

            case 'b': {
                uint32_t val = va_arg(args, uint32_t);
                utoa(val, numbuf, 2);
                write_str(out, &pos, numbuf, limit);
                break;
            }

            case 'o': {
                uint32_t val = va_arg(args, uint32_t);
                utoa(val, numbuf, 8);
                write_str(out, &pos, numbuf, limit);
                break;
            }

            case '%': {
                write_char(out, &pos, '%', limit);
                break;
            }

            default: {
                write_char(out, &pos, '%', limit);
                write_char(out, &pos, fmt[i], limit);
                break;
            }
        }
    }

    if (out) out[pos] = '\0';
    return pos;
}

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kprintf_internal(NULL, 0x7FFFFFFF, fmt, args);
    va_end(args);
}

int ksprintf(char* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = kprintf_internal(buf, 0x7FFFFFFF, fmt, args);
    va_end(args);
    return n;
}
