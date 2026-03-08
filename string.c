/* =============================================================================
 * KOSMO OS — String Library (Implementación)
 * Archivo : libc/string.c
 * Función : Implementación de funciones de string/memoria para el kernel.
 *           Sin dependencias de la librería C estándar (freestanding).
 * ============================================================================= */

#include "string.h"

/* =============================================================================
 * FUNCIONES DE MEMORIA
 * ============================================================================= */

void* memset(void* dest, int val, size_t n) {
    uint8_t* p = (uint8_t*)dest;
    while (n--) *p++ = (uint8_t)val;
    return dest;
}

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* p = (const uint8_t*)a;
    const uint8_t* q = (const uint8_t*)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

/* =============================================================================
 * FUNCIONES DE STRING
 * ============================================================================= */

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i = 0;
    while (i < n && src[i]) { dest[i] = src[i]; i++; }
    while (i < n) dest[i++] = '\0';
    return dest;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char* strcat(char* dest, const char* src) {
    char* d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == 0) ? (char*)s : NULL;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0)
            return (char*)haystack;
        haystack++;
    }
    return NULL;
}

/* =============================================================================
 * CONVERSIÓN NUMÉRICA
 * ============================================================================= */

int atoi(const char* s) {
    int result = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        result = result * 10 + (*s++ - '0');
    return sign * result;
}

void itoa(int value, char* buf, int base) {
    static const char digits[] = "0123456789ABCDEF";
    char tmp[34];
    int i = 0, neg = 0;

    if (value == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    if (base == 10 && value < 0) { neg = 1; value = -value; }

    while (value) {
        tmp[i++] = digits[value % base];
        value /= base;
    }
    if (neg) tmp[i++] = '-';

    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

void utoa(uint32_t value, char* buf, int base) {
    static const char digits[] = "0123456789ABCDEF";
    char tmp[34];
    int i = 0;

    if (value == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (value) {
        tmp[i++] = digits[value % (uint32_t)base];
        value /= (uint32_t)base;
    }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}
