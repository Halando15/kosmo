/* =============================================================================
 * KOSMO OS — GDT (Implementación)
 * Archivo : kernel/arch/x86/gdt.c
 * Función : Configura la tabla de descriptores global con modelo de memoria
 *           plana (flat 4GB) para código y datos en ring 0 y ring 3.
 * ============================================================================= */

#include "gdt.h"
#include "types.h"

/* =============================================================================
 * DATOS ESTÁTICOS
 * ============================================================================= */

/* La tabla GDT: array de GDT_ENTRIES entradas de 8 bytes cada una */
static gdt_entry_t gdt_table[GDT_ENTRIES];

/* Puntero que cargamos con LGDT (dirección + tamaño) */
static gdt_ptr_t gdt_pointer;

/* =============================================================================
 * FUNCIONES
 * ============================================================================= */

/**
 * gdt_set_entry — Rellenar una entrada de la GDT
 * @index : índice en gdt_table (0 = null, 1 = kcode, 2 = kdata, ...)
 * @base  : dirección base del segmento (0x00000000 para flat)
 * @limit : límite del segmento (0xFFFFF con G=1 → 4GB)
 * @access: byte de acceso (ver GDT_ACCESS_* en gdt.h)
 * @flags : nibble de flags (ver GDT_FLAGS_* en gdt.h)
 */
void gdt_set_entry(uint8_t index, uint32_t base, uint32_t limit,
                   uint8_t access, uint8_t flags) {

    gdt_entry_t* e = &gdt_table[index];

    /* Base en 3 partes */
    e->base_low  = (uint16_t)(base & 0xFFFF);
    e->base_mid  = (uint8_t)((base >> 16) & 0xFF);
    e->base_high = (uint8_t)((base >> 24) & 0xFF);

    /* Límite en 2 partes */
    e->limit_low   = (uint16_t)(limit & 0xFFFF);
    /* Los 4 bits bajos del byte flags_limit = limit[16:19] */
    /* Los 4 bits altos = flags (G, D/B, L, AVL) */
    e->flags_limit = (uint8_t)((flags & 0xF0) | ((limit >> 16) & 0x0F));

    /* Byte de acceso completo */
    e->access = access;
}

/**
 * gdt_init — Instalar la GDT del kernel
 *
 * Entradas:
 *   0 → Null descriptor     (obligatorio por la arquitectura x86)
 *   1 → Código kernel       (ring 0, base 0, límite 4GB, 32 bits)
 *   2 → Datos kernel        (ring 0, base 0, límite 4GB, 32 bits)
 *   3 → Código usuario      (ring 3, base 0, límite 4GB, 32 bits)
 *   4 → Datos usuario       (ring 3, base 0, límite 4GB, 32 bits)
 *   5 → (reservada para TSS en fases futuras)
 */
void gdt_init(void) {

    /* ── Entrada 0: Null descriptor ─────────────────────────────────────── */
    gdt_set_entry(0, 0x00000000, 0x00000, 0x00, 0x00);

    /* ── Entrada 1: Segmento de código del kernel (ring 0) ───────────────
     * Base  = 0x00000000 (modelo plano — sin traducción de segmentos)
     * Limit = 0xFFFFF   (con G=1: 0xFFFFF * 4KB = 4GB)
     * Access= P=1, DPL=0, S=1, Type=1010 (Execute/Read)
     * Flags = G=1 (4KB granularity), D/B=1 (32-bit) */
    gdt_set_entry(1,
        0x00000000,
        0xFFFFF,
        GDT_ACCESS_KCODE,
        GDT_FLAGS_32);

    /* ── Entrada 2: Segmento de datos del kernel (ring 0) ────────────────
     * Idéntico al de código excepto Type=0010 (Read/Write, no ejecutable) */
    gdt_set_entry(2,
        0x00000000,
        0xFFFFF,
        GDT_ACCESS_KDATA,
        GDT_FLAGS_32);

    /* ── Entrada 3: Segmento de código de usuario (ring 3) ───────────────
     * DPL=3 — para procesos de espacio de usuario (Fase futura) */
    gdt_set_entry(3,
        0x00000000,
        0xFFFFF,
        GDT_ACCESS_UCODE,
        GDT_FLAGS_32);

    /* ── Entrada 4: Segmento de datos de usuario (ring 3) ────────────────*/
    gdt_set_entry(4,
        0x00000000,
        0xFFFFF,
        GDT_ACCESS_UDATA,
        GDT_FLAGS_32);

    /* ── Entrada 5: Reservada para TSS ──────────────────────────────────── */
    gdt_set_entry(5, 0x00000000, 0x00000, 0x00, 0x00);

    /* ── Configurar el descriptor GDTR ──────────────────────────────────── */
    gdt_pointer.limit = (uint16_t)(sizeof(gdt_table) - 1);
    gdt_pointer.base  = (uint32_t)(uintptr_t)gdt_table;

    /* ── Cargar la GDT y recargar selectores de segmento ─────────────────
     * gdt_flush() está definido en entry.asm:
     *   1. Ejecuta LGDT con nuestro descriptor
     *   2. Hace far jump al selector de código (0x08) para recargar CS
     *   3. Recarga DS, ES, FS, GS, SS con el selector de datos (0x10) */
    gdt_flush(&gdt_pointer);
}
