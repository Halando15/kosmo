/* =============================================================================
 * KOSMO OS — Kernel Linker Script
 * Archivo : kernel/linker.ld
 * Función : Define cómo se organiza el binario del kernel en memoria.
 *           - El kernel se carga en 0x100000 (1MB) por GRUB
 *           - La cabecera Multiboot debe estar en los primeros 8KB
 *           - Orden de secciones: .multiboot, .text, .rodata, .data, .bss
 * ============================================================================= */

/* Punto de entrada del kernel */
ENTRY(_start)

/* Dirección de carga del kernel: 1MB (0x100000)
   GRUB carga el kernel aquí automáticamente. */
KERNEL_LOAD_ADDR = 0x100000;

SECTIONS
{
    /* -----------------------------------------------------------------------
     * Dirección de inicio: 1MB
     * ----------------------------------------------------------------------- */
    . = KERNEL_LOAD_ADDR;

    /* -----------------------------------------------------------------------
     * .multiboot — Cabecera Multiboot
     * DEBE ser la primera sección para que GRUB la encuentre en los primeros
     * 8KB del archivo. No se puede comprimir ni reubicar.
     * ----------------------------------------------------------------------- */
    .multiboot ALIGN(4) :
    {
        *(.multiboot)
    }

    /* -----------------------------------------------------------------------
     * .text — Código ejecutable del kernel
     * ----------------------------------------------------------------------- */
    .text ALIGN(4096) :
    {
        *(.text)        /* Todo el código C y ASM */
        *(.text.*)
    }

    /* -----------------------------------------------------------------------
     * .rodata — Datos de solo lectura (strings literales, tablas constantes)
     * ----------------------------------------------------------------------- */
    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* -----------------------------------------------------------------------
     * .data — Datos inicializados (variables globales con valor inicial)
     * ----------------------------------------------------------------------- */
    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    /* -----------------------------------------------------------------------
     * .bss — Datos no inicializados (se zerean al arrancar)
     * El kernel_main() se encarga de zerear esta sección.
     * ----------------------------------------------------------------------- */
    .bss ALIGN(4096) :
    {
        _bss_start = .;     /* Símbolo de inicio de BSS (usado en kernel.c) */
        *(COMMON)
        *(.bss)
        *(.bss.*)
        _bss_end = .;       /* Símbolo de fin de BSS */
    }

    /* -----------------------------------------------------------------------
     * Símbolo de fin del kernel
     * Usado por el PMM para saber dónde termina el kernel en memoria.
     * ----------------------------------------------------------------------- */
    _kernel_end = .;

    /* -----------------------------------------------------------------------
     * Descartar secciones innecesarias (debug info, notas, etc.)
     * Reduce el tamaño del binario final.
     * ----------------------------------------------------------------------- */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.GNU-stack)
        *(.note.*)
    }
}
