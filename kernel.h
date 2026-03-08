/* =============================================================================
 * KOSMO OS — Kernel Header Principal
 * Archivo : kernel/core/kernel.h
 * ============================================================================= */

#ifndef KOSMO_KERNEL_H
#define KOSMO_KERNEL_H

#include "types.h"
#include "multiboot.h"

/* Versión del sistema */
#define KOSMO_VERSION_MAJOR     0
#define KOSMO_VERSION_MINOR     1
#define KOSMO_VERSION_PATCH     0
#define KOSMO_VERSION_STRING    "0.1.0"
#define KOSMO_NAME              "Kosmo OS"
#define KOSMO_CODENAME          "Genesis"

/* Símbolos exportados por el linker script */
extern uint32_t _bss_start;
extern uint32_t _bss_end;
extern uint32_t _kernel_end;

/* Puntero a la info multiboot (guardado en entry.asm) */
extern uint32_t multiboot_info_ptr;
extern uint32_t multiboot_magic_val;

/* Punto de entrada del kernel en C */
void kernel_main(uint32_t magic, multiboot_info_t* mbi);

/* Información del sistema */
typedef struct {
    uint32_t total_ram_kb;
    uint32_t kernel_size;
    uint32_t boot_magic;
    bool     multiboot_valid;
    char     bootloader_name[64];
} system_info_t;

extern system_info_t sys_info;

#endif /* KOSMO_KERNEL_H */
