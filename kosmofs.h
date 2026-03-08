/* =============================================================================
 * KOSMO OS — KosmoFS (Sistema de Archivos en Memoria)
 * Archivo : fs/kosmofs.h
 * Función : Sistema de archivos propio, minimalista y residente en RAM.
 *           Diseñado para portátiles con poca memoria.
 *
 * DISEÑO:
 *   - Tabla de inodos fija (256 entradas máximo)
 *   - Datos de archivo almacenados en un pool de bloques de 512 bytes
 *   - Pool total: 256 bloques × 512 B = 128 KB en RAM
 *   - Nombre de archivo: hasta 56 caracteres
 *   - Directorios: inodo especial con lista de hijos
 *   - Sin fragmentación (bloques contiguos por archivo)
 *   - Timestamps usando ticks del PIT
 *
 * LAYOUT EN MEMORIA:
 *   [inode_table: 256 × sizeof(kfs_inode_t)]
 *   [block_pool:  256 × 512 bytes]
 *   [block_bitmap: 256 bits = 32 bytes]
 * ============================================================================= */

#ifndef KOSMO_KOSMOFS_H
#define KOSMO_KOSMOFS_H

#include "types.h"

/* =============================================================================
 * CONSTANTES
 * ============================================================================= */

#define KFS_MAX_INODES      256         /* Inodos totales (archivos + dirs) */
#define KFS_MAX_BLOCKS      256         /* Bloques de datos */
#define KFS_BLOCK_SIZE      512         /* Bytes por bloque */
#define KFS_NAME_LEN        56          /* Longitud máxima del nombre */
#define KFS_MAX_CHILDREN    32          /* Hijos máximos por directorio */
#define KFS_MAX_FILE_BLOCKS 16          /* Bloques máximos por archivo = 8KB */
#define KFS_ROOT_INODE      0           /* Inodo del directorio raíz */
#define KFS_INVALID_INODE   0xFF        /* Inodo inválido / vacío */
#define KFS_INVALID_BLOCK   0xFF        /* Bloque inválido / libre */

/* Tipos de inodo */
#define KFS_TYPE_FREE       0           /* Inodo libre */
#define KFS_TYPE_FILE       1           /* Archivo regular */
#define KFS_TYPE_DIR        2           /* Directorio */

/* Permisos (simplificados) */
#define KFS_PERM_READ       (1 << 0)
#define KFS_PERM_WRITE      (1 << 1)
#define KFS_PERM_EXEC       (1 << 2)
#define KFS_PERM_DEFAULT    (KFS_PERM_READ | KFS_PERM_WRITE)

/* Resultados de operaciones */
#define KFS_OK              0
#define KFS_ERR_NOT_FOUND   (-1)
#define KFS_ERR_EXISTS      (-2)
#define KFS_ERR_NO_SPACE    (-3)
#define KFS_ERR_NOT_DIR     (-4)
#define KFS_ERR_NOT_FILE    (-5)
#define KFS_ERR_NAME_TOO_LONG (-6)
#define KFS_ERR_TOO_BIG     (-7)
#define KFS_ERR_INVALID     (-8)
#define KFS_ERR_NOT_EMPTY   (-9)

/* =============================================================================
 * ESTRUCTURA DE INODO
 * 128 bytes por inodo (alineado a potencia de 2)
 * ============================================================================= */
typedef struct PACKED {
    /* Identificación */
    uint8_t  type;                          /* KFS_TYPE_* */
    uint8_t  permissions;                   /* KFS_PERM_* */
    uint8_t  inode_num;                     /* Número de este inodo */
    uint8_t  parent_inode;                  /* Inodo del directorio padre */

    /* Nombre */
    char     name[KFS_NAME_LEN];            /* Nombre del archivo/directorio */

    /* Metadatos de tamaño */
    uint32_t size;                          /* Tamaño en bytes (archivos) */
    uint8_t  block_count;                   /* Número de bloques usados */
    uint8_t  blocks[KFS_MAX_FILE_BLOCKS];   /* Índices de bloques de datos */

    /* Para directorios: lista de inodos hijos */
    uint8_t  child_count;                   /* Número de entradas en el dir */
    uint8_t  children[KFS_MAX_CHILDREN];    /* Inodos hijos */

    /* Timestamps (en ticks del sistema) */
    uint32_t created_at;
    uint32_t modified_at;

    /* Padding para alinear a 128 bytes */
    uint8_t  _pad[128 - (4 + KFS_NAME_LEN + 4 + 1 + KFS_MAX_FILE_BLOCKS
                         + 1 + KFS_MAX_CHILDREN + 8)];
} kfs_inode_t;

/* =============================================================================
 * ESTRUCTURA PRINCIPAL DEL FILESYSTEM
 * ============================================================================= */
typedef struct {
    kfs_inode_t inodes[KFS_MAX_INODES];                     /* Tabla de inodos */
    uint8_t     blocks[KFS_MAX_BLOCKS][KFS_BLOCK_SIZE];     /* Pool de bloques */
    uint8_t     inode_bitmap[KFS_MAX_INODES / 8];           /* Bitmap inodos */
    uint8_t     block_bitmap[KFS_MAX_BLOCKS / 8];           /* Bitmap bloques */
    uint32_t    total_files;
    uint32_t    total_dirs;
    bool        mounted;
} kfs_t;

/* =============================================================================
 * DESCRIPTOR DE ARCHIVO (para lectura/escritura secuencial)
 * ============================================================================= */
typedef struct {
    uint8_t  inode_num;     /* Inodo del archivo abierto */
    uint32_t position;      /* Posición de lectura/escritura */
    bool     valid;         /* ¿Está abierto? */
} kfs_file_t;

/* =============================================================================
 * INFORMACIÓN DE DIRECTORIO (para ls)
 * ============================================================================= */
typedef struct {
    char     name[KFS_NAME_LEN];
    uint8_t  type;          /* KFS_TYPE_FILE o KFS_TYPE_DIR */
    uint32_t size;
    uint8_t  inode_num;
    uint32_t modified_at;
} kfs_dirent_t;

/* =============================================================================
 * API PÚBLICA DE KOSMOFS
 * ============================================================================= */

/* ── Inicialización ────────────────────────────────────────────────────────── */
void kfs_init(void);
bool kfs_is_mounted(void);

/* ── Navegación de rutas ───────────────────────────────────────────────────── */
int  kfs_resolve_path(const char* path);                /* → número de inodo */
int  kfs_resolve_parent(const char* path, char* name_out);

/* ── Operaciones con archivos ─────────────────────────────────────────────── */
int  kfs_create(const char* path, uint8_t type);
int  kfs_delete(const char* path);
int  kfs_write(const char* path, const void* data, uint32_t size);
int  kfs_read (const char* path, void* buf, uint32_t max_size);
int  kfs_append(const char* path, const void* data, uint32_t size);
int  kfs_get_size(const char* path);
bool kfs_exists(const char* path);

/* ── Operaciones con directorios ──────────────────────────────────────────── */
int  kfs_mkdir(const char* path);
int  kfs_rmdir(const char* path);
int  kfs_listdir(const char* path, kfs_dirent_t* out, uint32_t max_entries);
int  kfs_get_child_count(const char* path);

/* ── Estadísticas ─────────────────────────────────────────────────────────── */
void kfs_stat(void);     /* Imprime estadísticas por pantalla */
uint32_t kfs_free_blocks(void);
uint32_t kfs_free_inodes(void);

/* ── Inodo directo (uso interno) ──────────────────────────────────────────── */
kfs_inode_t* kfs_get_inode(uint8_t num);

#endif /* KOSMO_KOSMOFS_H */
