/* =============================================================================
 * KOSMO OS — Framework de Tests del Kernel
 * Archivo : tests/ktest.h
 * Función : Sistema de pruebas unitarias que se ejecutan durante el arranque
 *           (antes de lanzar el shell/GUI). Sólo activo si KOSMO_TEST=1.
 * ============================================================================= */

#ifndef KOSMO_KTEST_H
#define KOSMO_KTEST_H

#include "types.h"
#include "vga.h"
#include "stdio.h"

/* =============================================================================
 * MACROS DE ASERCIÓN
 * ============================================================================= */

/* Contador global de tests */
extern uint32_t ktest_passed;
extern uint32_t ktest_failed;
extern uint32_t ktest_total;
extern const char* ktest_current_suite;

/* Iniciar una suite de tests */
#define KTEST_SUITE(name) \
    do { \
        ktest_current_suite = (name); \
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK); \
        kprintf("\n  [TEST] Suite: %s\n", name); \
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK); \
    } while(0)

/* Aserción booleana */
#define KTEST_ASSERT(cond) \
    do { \
        ktest_total++; \
        if (cond) { \
            ktest_passed++; \
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK); \
            kprintf("    ✓  %-50s\n", #cond); \
        } else { \
            ktest_failed++; \
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK); \
            kprintf("    ✗  %-50s  [%s:%d]\n", #cond, __FILE__, __LINE__); \
        } \
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK); \
    } while(0)

/* Aserción con igualdad y mensaje */
#define KTEST_EQ(a, b) \
    do { \
        ktest_total++; \
        if ((a) == (b)) { \
            ktest_passed++; \
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK); \
            kprintf("    ✓  %s == %s\n", #a, #b); \
        } else { \
            ktest_failed++; \
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK); \
            kprintf("    ✗  %s != %s  (got %d, expected %d)  [line %d]\n", \
                    #a, #b, (int)(a), (int)(b), __LINE__); \
        } \
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK); \
    } while(0)

/* Aserción de strings */
#define KTEST_STREQ(a, b) \
    do { \
        ktest_total++; \
        if (strcmp((a), (b)) == 0) { \
            ktest_passed++; \
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK); \
            kprintf("    ✓  strcmp(%s, %s) == 0\n", #a, #b); \
        } else { \
            ktest_failed++; \
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK); \
            kprintf("    ✗  \"%s\" != \"%s\"  [line %d]\n", (a), (b), __LINE__); \
        } \
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK); \
    } while(0)

/* =============================================================================
 * API
 * ============================================================================= */

/* Ejecutar todos los tests. Retorna 0 si todos pasan. */
int ktest_run_all(void);

/* Suites individuales */
void ktest_suite_string(void);
void ktest_suite_stdio(void);
void ktest_suite_kosmofs(void);
void ktest_suite_memory(void);
void ktest_suite_pit(void);
void ktest_suite_gdt_idt(void);

/* Mostrar resumen final */
void ktest_print_summary(void);

#endif /* KOSMO_KTEST_H */
