/* =============================================================================
 * KOSMO OS — Estructura Multiboot
 * Archivo : include/multiboot.h
 * Función : Define la estructura multiboot_info que GRUB2 pasa al kernel
 *           al arrancar. Contiene información sobre la memoria disponible,
 *           módulos cargados, mapa de memoria, etc.
 *           Basada en la especificación Multiboot 1.
 * ============================================================================= */

#ifndef KOSMO_MULTIBOOT_H
#define KOSMO_MULTIBOOT_H

#include "types.h"

/* =============================================================================
 * CONSTANTES MULTIBOOT
 * ============================================================================= */

/* Magia que GRUB pone en EAX al saltar al kernel */
#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002

/* Flags del campo 'flags' en multiboot_info */
#define MULTIBOOT_FLAG_MEM          BIT(0)  /* mem_lower y mem_upper válidos */
#define MULTIBOOT_FLAG_DEVICE       BIT(1)  /* boot_device válido */
#define MULTIBOOT_FLAG_CMDLINE      BIT(2)  /* cmdline válido */
#define MULTIBOOT_FLAG_MODS         BIT(3)  /* mods_count y mods_addr válidos */
#define MULTIBOOT_FLAG_AOUT         BIT(4)  /* tabla de símbolos a.out válida */
#define MULTIBOOT_FLAG_ELF          BIT(5)  /* tabla de secciones ELF válida */
#define MULTIBOOT_FLAG_MMAP         BIT(6)  /* mmap_length y mmap_addr válidos */
#define MULTIBOOT_FLAG_DRIVES       BIT(7)  /* drives_length y drives_addr válidos */
#define MULTIBOOT_FLAG_CONFIG       BIT(8)  /* config_table válido */
#define MULTIBOOT_FLAG_LOADER_NAME  BIT(9)  /* boot_loader_name válido */
#define MULTIBOOT_FLAG_APM          BIT(10) /* tabla APM válida */
#define MULTIBOOT_FLAG_VBE          BIT(11) /* información VBE válida */

/* Tipos de entradas en el mapa de memoria E820 */
#define MMAP_TYPE_AVAILABLE         1   /* Memoria RAM disponible */
#define MMAP_TYPE_RESERVED          2   /* Reservada (hardware, BIOS, etc.) */
#define MMAP_TYPE_ACPI_RECLAIMABLE  3   /* ACPI (recuperable) */
#define MMAP_TYPE_ACPI_NVS          4   /* ACPI NVS (no volátil) */
#define MMAP_TYPE_BAD               5   /* Memoria defectuosa */

/* =============================================================================
 * ESTRUCTURAS MULTIBOOT
 * Todas deben ser PACKED para que coincidan exactamente con la especificación.
 * ============================================================================= */

/* Entrada del mapa de memoria E820 */
typedef struct PACKED {
    uint32_t size;          /* Tamaño de esta entrada - 4 (tamaño excluye este campo) */
    uint64_t base_addr;     /* Dirección física base */
    uint64_t length;        /* Longitud en bytes */
    uint32_t type;          /* Tipo (ver MMAP_TYPE_*) */
} multiboot_mmap_entry_t;

/* Módulo cargado por GRUB */
typedef struct PACKED {
    uint32_t mod_start;     /* Dirección física de inicio del módulo */
    uint32_t mod_end;       /* Dirección física de fin del módulo */
    uint32_t cmdline;       /* Línea de comandos del módulo (puntero a string) */
    uint32_t reserved;      /* Reservado (siempre 0) */
} multiboot_module_t;

/* Tabla de símbolos ELF (cuando se carga un kernel ELF) */
typedef struct PACKED {
    uint32_t num;           /* Número de entradas de sección */
    uint32_t size;          /* Tamaño de cada entrada */
    uint32_t addr;          /* Dirección de la tabla de secciones ELF */
    uint32_t shndx;         /* Índice de la sección de nombres de sección */
} multiboot_elf_section_header_table_t;

/* Información VBE (framebuffer gráfico) */
typedef struct PACKED {
    uint32_t vbe_control_info;      /* Dirección de VbeInfoBlock */
    uint32_t vbe_mode_info;         /* Dirección de ModeInfoBlock */
    uint16_t vbe_mode;              /* Modo VBE actual */
    uint16_t vbe_interface_seg;     /* Segmento de interfaz protegida VBE */
    uint16_t vbe_interface_off;     /* Offset de interfaz protegida VBE */
    uint16_t vbe_interface_len;     /* Longitud de interfaz protegida VBE */
} multiboot_vbe_info_t;

/* =============================================================================
 * ESTRUCTURA PRINCIPAL: multiboot_info
 * GRUB escribe esta estructura en memoria y pasa su dirección en EBX.
 * ============================================================================= */
typedef struct PACKED {
    /* Byte 0-3: Flags — indica qué campos son válidos */
    uint32_t flags;

    /* Bytes 4-11: Información de memoria (válido si FLAG_MEM) */
    uint32_t mem_lower;         /* Kilobytes de memoria bajo 1MB (típico: 640) */
    uint32_t mem_upper;         /* Kilobytes de memoria encima de 1MB */

    /* Bytes 12-15: Dispositivo de arranque (válido si FLAG_DEVICE) */
    uint32_t boot_device;       /* BIOS drive + partición */

    /* Bytes 16-19: Línea de comandos (válido si FLAG_CMDLINE) */
    uint32_t cmdline;           /* Dirección física del string de comandos */

    /* Bytes 20-27: Módulos cargados (válido si FLAG_MODS) */
    uint32_t mods_count;        /* Número de módulos */
    uint32_t mods_addr;         /* Dirección de la tabla de módulos */

    /* Bytes 28-43: Símbolos (válido si FLAG_AOUT o FLAG_ELF) */
    union {
        multiboot_elf_section_header_table_t elf_sec;
        uint8_t raw[16];
    } syms;

    /* Bytes 44-51: Mapa de memoria E820 (válido si FLAG_MMAP) */
    uint32_t mmap_length;       /* Tamaño total del mapa de memoria en bytes */
    uint32_t mmap_addr;         /* Dirección física del mapa de memoria */

    /* Bytes 52-59: Drives (válido si FLAG_DRIVES) */
    uint32_t drives_length;
    uint32_t drives_addr;

    /* Bytes 60-63: Tabla de configuración (válido si FLAG_CONFIG) */
    uint32_t config_table;

    /* Bytes 64-67: Nombre del bootloader (válido si FLAG_LOADER_NAME) */
    uint32_t boot_loader_name;  /* Dirección del string con nombre del bootloader */

    /* Bytes 68-71: Tabla APM (válido si FLAG_APM) */
    uint32_t apm_table;

    /* Bytes 72-87: Información VBE (válido si FLAG_VBE) */
    multiboot_vbe_info_t vbe;

} multiboot_info_t;

/* =============================================================================
 * FUNCIONES DE UTILIDAD PARA MULTIBOOT
 * ============================================================================= */

/* Verificar si Multiboot fue cargado correctamente */
static inline bool multiboot_is_valid(uint32_t magic) {
    return (magic == MULTIBOOT_BOOTLOADER_MAGIC);
}

/* Verificar si el mapa de memoria está disponible */
static inline bool multiboot_has_mmap(multiboot_info_t* mbi) {
    return (mbi->flags & MULTIBOOT_FLAG_MMAP) != 0;
}

/* Verificar si la información de memoria básica está disponible */
static inline bool multiboot_has_mem(multiboot_info_t* mbi) {
    return (mbi->flags & MULTIBOOT_FLAG_MEM) != 0;
}

/* Obtener total de RAM en KB */
static inline uint32_t multiboot_total_ram_kb(multiboot_info_t* mbi) {
    if (multiboot_has_mem(mbi)) {
        return mbi->mem_lower + mbi->mem_upper;
    }
    return 0;
}

#endif /* KOSMO_MULTIBOOT_H */
