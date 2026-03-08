/* =============================================================================
 * KOSMO OS — KosmoFS (Implementación)
 * Archivo : fs/kosmofs.c
 * Función : Sistema de archivos en memoria RAM (ramdisk).
 *           Soporta archivos y directorios con una tabla de inodos fija
 *           y un pool de bloques de 512 bytes.
 * ============================================================================= */

#include "kosmofs.h"
#include "pit.h"
#include "stdio.h"
#include "string.h"
#include "vga.h"
#include "types.h"

/* =============================================================================
 * INSTANCIA GLOBAL DEL FILESYSTEM
 * Declarada estática para residir en .bss (se zereará al arrancar).
 * ============================================================================= */
static kfs_t kfs;

/* =============================================================================
 * BITMAP — UTILIDADES
 * ============================================================================= */

static inline bool bitmap_test(uint8_t* bm, uint8_t idx) {
    return (bm[idx / 8] >> (idx % 8)) & 1;
}
static inline void bitmap_set(uint8_t* bm, uint8_t idx) {
    bm[idx / 8] |= (uint8_t)(1 << (idx % 8));
}
static inline void bitmap_clear(uint8_t* bm, uint8_t idx) {
    bm[idx / 8] &= (uint8_t)~(1 << (idx % 8));
}

/* Buscar el primer bit a 0 en el bitmap */
static int bitmap_find_free(uint8_t* bm, int count) {
    for (int i = 0; i < count; i++) {
        if (!bitmap_test(bm, (uint8_t)i)) return i;
    }
    return -1;
}

/* =============================================================================
 * GESTIÓN DE INODOS Y BLOQUES
 * ============================================================================= */

static int alloc_inode(void) {
    return bitmap_find_free(kfs.inode_bitmap, KFS_MAX_INODES);
}

static int alloc_block(void) {
    return bitmap_find_free(kfs.block_bitmap, KFS_MAX_BLOCKS);
}

static void free_inode(uint8_t num) {
    bitmap_clear(kfs.inode_bitmap, num);
    memset(&kfs.inodes[num], 0, sizeof(kfs_inode_t));
}

static void free_block(uint8_t num) {
    bitmap_clear(kfs.block_bitmap, num);
    memset(kfs.blocks[num], 0, KFS_BLOCK_SIZE);
}

kfs_inode_t* kfs_get_inode(uint8_t num) {
    if (num >= KFS_MAX_INODES) return NULL;
    if (!bitmap_test(kfs.inode_bitmap, num)) return NULL;
    return &kfs.inodes[num];
}

uint32_t kfs_free_blocks(void) {
    uint32_t free = 0;
    for (int i = 0; i < KFS_MAX_BLOCKS; i++)
        if (!bitmap_test(kfs.block_bitmap, (uint8_t)i)) free++;
    return free;
}

uint32_t kfs_free_inodes(void) {
    uint32_t free = 0;
    for (int i = 0; i < KFS_MAX_INODES; i++)
        if (!bitmap_test(kfs.inode_bitmap, (uint8_t)i)) free++;
    return free;
}

/* =============================================================================
 * RESOLUCIÓN DE RUTAS
 * ============================================================================= */

/**
 * path_split_last — Separar la última componente de una ruta
 * "/etc/kosmo.cfg" → dir="/etc", name="kosmo.cfg"
 */
static void path_split_last(const char* path, char* dir_out, char* name_out) {
    const char* last_slash = NULL;
    const char* p = path;

    while (*p) {
        if (*p == '/') last_slash = p;
        p++;
    }

    if (!last_slash || last_slash == path) {
        /* Sin barra, o solo "/" al inicio */
        strcpy(dir_out, "/");
        strncpy(name_out, (last_slash ? last_slash + 1 : path), KFS_NAME_LEN - 1);
    } else {
        int dir_len = (int)(last_slash - path);
        strncpy(dir_out, path, (uint32_t)dir_len);
        dir_out[dir_len] = '\0';
        strncpy(name_out, last_slash + 1, KFS_NAME_LEN - 1);
    }
    name_out[KFS_NAME_LEN - 1] = '\0';
}

/**
 * kfs_resolve_path — Resolver una ruta absoluta al número de inodo
 * Retorna el número de inodo, o KFS_ERR_NOT_FOUND.
 * Soporta:
 *   "/"        → inodo 0 (raíz)
 *   "/etc"     → busca "etc" en raíz
 *   "/etc/x"   → busca "x" en inodo de "etc"
 */
int kfs_resolve_path(const char* path) {
    if (!path || path[0] != '/') return KFS_ERR_INVALID;
    if (path[1] == '\0') return KFS_ROOT_INODE;   /* Raíz */

    int current_inode = KFS_ROOT_INODE;
    char component[KFS_NAME_LEN];
    const char* p = path + 1;   /* Saltar '/' inicial */

    while (*p) {
        /* Extraer componente hasta la próxima '/' */
        int clen = 0;
        while (*p && *p != '/' && clen < KFS_NAME_LEN - 1) {
            component[clen++] = *p++;
        }
        component[clen] = '\0';
        if (*p == '/') p++;
        if (clen == 0) continue;

        /* Buscar component en el directorio current_inode */
        kfs_inode_t* dir = &kfs.inodes[current_inode];
        if (dir->type != KFS_TYPE_DIR) return KFS_ERR_NOT_DIR;

        bool found = false;
        for (int i = 0; i < dir->child_count; i++) {
            uint8_t child_idx = dir->children[i];
            if (!bitmap_test(kfs.inode_bitmap, child_idx)) continue;
            kfs_inode_t* child = &kfs.inodes[child_idx];
            if (strcmp(child->name, component) == 0) {
                current_inode = child_idx;
                found = true;
                break;
            }
        }

        if (!found) return KFS_ERR_NOT_FOUND;
    }

    return current_inode;
}

/* kfs_resolve_parent — Resolver el directorio padre y extraer el nombre final */
int kfs_resolve_parent(const char* path, char* name_out) {
    char dir_part[128];
    path_split_last(path, dir_part, name_out);
    return kfs_resolve_path(dir_part);
}

/* =============================================================================
 * INICIALIZACIÓN — CREAR ESTRUCTURA INICIAL
 * ============================================================================= */

/**
 * kfs_init — Montar el filesystem en memoria y crear el árbol inicial
 */
void kfs_init(void) {
    memset(&kfs, 0, sizeof(kfs));

    /* ── Crear directorio raíz (inodo 0) ──────────────────────────────── */
    bitmap_set(kfs.inode_bitmap, KFS_ROOT_INODE);
    kfs_inode_t* root = &kfs.inodes[KFS_ROOT_INODE];
    root->type        = KFS_TYPE_DIR;
    root->permissions = KFS_PERM_READ | KFS_PERM_WRITE | KFS_PERM_EXEC;
    root->inode_num   = KFS_ROOT_INODE;
    root->parent_inode= KFS_ROOT_INODE;   /* El padre de / es él mismo */
    strcpy(root->name, "/");
    root->created_at  = (uint32_t)pit_get_ticks();
    root->modified_at = root->created_at;

    kfs.total_dirs = 1;
    kfs.mounted    = true;

    /* ── Crear árbol de directorios del sistema ──────────────────────── */
    kfs_mkdir("/etc");
    kfs_mkdir("/bin");
    kfs_mkdir("/home");
    kfs_mkdir("/tmp");
    kfs_mkdir("/boot");
    kfs_mkdir("/dev");
    kfs_mkdir("/home/user");

    /* ── Crear archivos del sistema con contenido real ───────────────── */

    kfs_write("/etc/hostname",
        "kosmo\n", 6);

    kfs_write("/etc/version",
        "Kosmo OS v0.1.0 \"Genesis\"\nBuilt: 2025\nArch: x86 32-bit\n", 57);

    kfs_write("/etc/motd",
        "Welcome to Kosmo OS!\n"
        "A lightweight operating system for old hardware.\n"
        "Type 'help' for available commands.\n", 103);

    kfs_write("/etc/config",
        "# Kosmo OS Configuration\n"
        "shell=/bin/ksh\n"
        "term=vga-text\n"
        "width=80\n"
        "height=25\n"
        "colors=16\n"
        "tick_rate=100\n", 108);

    kfs_write("/home/user/readme.txt",
        "Welcome to your home directory!\n"
        "You can create files here with: write <file> <content>\n"
        "List files with: ls /home/user\n", 97);

    kfs_write("/tmp/.keep",
        "", 0);

    kfs_write("/boot/grub.cfg",
        "# Kosmo OS GRUB config\n"
        "set timeout=5\n"
        "menuentry \"Kosmo OS\" {\n"
        "    multiboot /boot/kernel.bin\n"
        "}\n", 87);

    kprintf("  [ OK ]  KosmoFS mounted (RAM disk: %u KB, %u inodes free)\n",
            (KFS_MAX_BLOCKS * KFS_BLOCK_SIZE) / 1024,
            kfs_free_inodes());
}

bool kfs_is_mounted(void) { return kfs.mounted; }

/* =============================================================================
 * CREAR ARCHIVOS Y DIRECTORIOS
 * ============================================================================= */

/**
 * kfs_create — Crear un nuevo inodo (archivo o directorio)
 * @path: ruta absoluta
 * @type: KFS_TYPE_FILE o KFS_TYPE_DIR
 */
int kfs_create(const char* path, uint8_t type) {
    if (!path || strlen(path) == 0) return KFS_ERR_INVALID;

    /* Verificar que no existe */
    if (kfs_resolve_path(path) >= 0) return KFS_ERR_EXISTS;

    /* Resolver directorio padre */
    char name[KFS_NAME_LEN];
    int parent_num = kfs_resolve_parent(path, name);
    if (parent_num < 0) return KFS_ERR_NOT_FOUND;
    if (strlen(name) == 0) return KFS_ERR_INVALID;
    if (strlen(name) >= KFS_NAME_LEN) return KFS_ERR_NAME_TOO_LONG;

    kfs_inode_t* parent = &kfs.inodes[parent_num];
    if (parent->type != KFS_TYPE_DIR) return KFS_ERR_NOT_DIR;
    if (parent->child_count >= KFS_MAX_CHILDREN) return KFS_ERR_NO_SPACE;

    /* Asignar nuevo inodo */
    int new_num = alloc_inode();
    if (new_num < 0) return KFS_ERR_NO_SPACE;

    bitmap_set(kfs.inode_bitmap, (uint8_t)new_num);
    kfs_inode_t* node = &kfs.inodes[new_num];
    memset(node, 0, sizeof(kfs_inode_t));

    node->type         = type;
    node->permissions  = KFS_PERM_DEFAULT;
    node->inode_num    = (uint8_t)new_num;
    node->parent_inode = (uint8_t)parent_num;
    strncpy(node->name, name, KFS_NAME_LEN - 1);
    node->created_at   = (uint32_t)pit_get_ticks();
    node->modified_at  = node->created_at;

    /* Registrar como hijo del padre */
    parent->children[parent->child_count++] = (uint8_t)new_num;
    parent->modified_at = node->created_at;

    if (type == KFS_TYPE_FILE) kfs.total_files++;
    else                        kfs.total_dirs++;

    return new_num;
}

int kfs_mkdir(const char* path) {
    int r = kfs_create(path, KFS_TYPE_DIR);
    return (r >= 0) ? KFS_OK : r;
}

/* =============================================================================
 * ESCRITURA
 * ============================================================================= */

/**
 * kfs_write — Escribir datos en un archivo (sobreescribe el contenido actual)
 */
int kfs_write(const char* path, const void* data, uint32_t size) {
    /* Crear el archivo si no existe */
    int inum = kfs_resolve_path(path);
    if (inum == KFS_ERR_NOT_FOUND) {
        inum = kfs_create(path, KFS_TYPE_FILE);
        if (inum < 0) return inum;
    }
    if (inum < 0) return inum;

    kfs_inode_t* node = &kfs.inodes[inum];
    if (node->type != KFS_TYPE_FILE) return KFS_ERR_NOT_FILE;

    /* Liberar bloques existentes */
    for (int i = 0; i < node->block_count; i++) {
        free_block(node->blocks[i]);
        node->blocks[i] = KFS_INVALID_BLOCK;
    }
    node->block_count = 0;
    node->size        = 0;

    if (size == 0) return KFS_OK;

    /* Calcular cuántos bloques necesitamos */
    uint32_t blocks_needed = (size + KFS_BLOCK_SIZE - 1) / KFS_BLOCK_SIZE;
    if (blocks_needed > KFS_MAX_FILE_BLOCKS) return KFS_ERR_TOO_BIG;
    if ((int)blocks_needed > bitmap_find_free(kfs.block_bitmap, KFS_MAX_BLOCKS))
        return KFS_ERR_NO_SPACE;

    /* Asignar bloques y copiar datos */
    const uint8_t* src = (const uint8_t*)data;
    uint32_t remaining = size;

    for (uint32_t b = 0; b < blocks_needed; b++) {
        int bnum = alloc_block();
        if (bnum < 0) return KFS_ERR_NO_SPACE;

        bitmap_set(kfs.block_bitmap, (uint8_t)bnum);
        node->blocks[b] = (uint8_t)bnum;
        node->block_count++;

        uint32_t to_copy = (remaining > KFS_BLOCK_SIZE) ? KFS_BLOCK_SIZE : remaining;
        memcpy(kfs.blocks[bnum], src, to_copy);
        src       += to_copy;
        remaining -= to_copy;
    }

    node->size        = size;
    node->modified_at = (uint32_t)pit_get_ticks();
    return KFS_OK;
}

/**
 * kfs_append — Añadir datos al final de un archivo
 */
int kfs_append(const char* path, const void* data, uint32_t size) {
    int inum = kfs_resolve_path(path);
    if (inum < 0) return kfs_write(path, data, size);   /* Crear si no existe */

    kfs_inode_t* node = &kfs.inodes[inum];
    if (node->type != KFS_TYPE_FILE) return KFS_ERR_NOT_FILE;

    /* Leer contenido actual */
    static uint8_t tmp[KFS_MAX_FILE_BLOCKS * KFS_BLOCK_SIZE];
    uint32_t old_size = node->size;
    if (old_size > 0) {
        kfs_read(path, tmp, old_size);
    }

    /* Concatenar */
    if (old_size + size > KFS_MAX_FILE_BLOCKS * KFS_BLOCK_SIZE)
        return KFS_ERR_TOO_BIG;

    memcpy(tmp + old_size, data, size);
    return kfs_write(path, tmp, old_size + size);
}

/* =============================================================================
 * LECTURA
 * ============================================================================= */

/**
 * kfs_read — Leer el contenido completo de un archivo en un buffer
 * Retorna número de bytes leídos, o error negativo.
 */
int kfs_read(const char* path, void* buf, uint32_t max_size) {
    int inum = kfs_resolve_path(path);
    if (inum < 0) return KFS_ERR_NOT_FOUND;

    kfs_inode_t* node = &kfs.inodes[inum];
    if (node->type != KFS_TYPE_FILE) return KFS_ERR_NOT_FILE;

    uint32_t to_read = (node->size < max_size) ? node->size : max_size;
    uint8_t* dst     = (uint8_t*)buf;
    uint32_t copied  = 0;

    for (int b = 0; b < node->block_count && copied < to_read; b++) {
        uint8_t bnum = node->blocks[b];
        uint32_t chunk = KFS_BLOCK_SIZE;
        if (copied + chunk > to_read) chunk = to_read - copied;
        memcpy(dst + copied, kfs.blocks[bnum], chunk);
        copied += chunk;
    }

    return (int)copied;
}

int kfs_get_size(const char* path) {
    int inum = kfs_resolve_path(path);
    if (inum < 0) return KFS_ERR_NOT_FOUND;
    return (int)kfs.inodes[inum].size;
}

bool kfs_exists(const char* path) {
    return kfs_resolve_path(path) >= 0;
}

/* =============================================================================
 * LISTADO DE DIRECTORIO
 * ============================================================================= */

/**
 * kfs_listdir — Listar entradas de un directorio
 * Retorna número de entradas encontradas, o error.
 */
int kfs_listdir(const char* path, kfs_dirent_t* out, uint32_t max_entries) {
    int inum = kfs_resolve_path(path);
    if (inum < 0) return KFS_ERR_NOT_FOUND;

    kfs_inode_t* dir = &kfs.inodes[inum];
    if (dir->type != KFS_TYPE_DIR) return KFS_ERR_NOT_DIR;

    uint32_t count = 0;
    for (int i = 0; i < dir->child_count && count < max_entries; i++) {
        uint8_t cidx = dir->children[i];
        if (!bitmap_test(kfs.inode_bitmap, cidx)) continue;

        kfs_inode_t* child = &kfs.inodes[cidx];
        strncpy(out[count].name, child->name, KFS_NAME_LEN - 1);
        out[count].name[KFS_NAME_LEN - 1] = '\0';
        out[count].type        = child->type;
        out[count].size        = child->size;
        out[count].inode_num   = cidx;
        out[count].modified_at = child->modified_at;
        count++;
    }
    return (int)count;
}

int kfs_get_child_count(const char* path) {
    int inum = kfs_resolve_path(path);
    if (inum < 0) return KFS_ERR_NOT_FOUND;
    return kfs.inodes[inum].child_count;
}

/* =============================================================================
 * BORRADO
 * ============================================================================= */

/**
 * kfs_delete — Borrar un archivo
 */
int kfs_delete(const char* path) {
    int inum = kfs_resolve_path(path);
    if (inum < 0) return KFS_ERR_NOT_FOUND;
    if (inum == KFS_ROOT_INODE) return KFS_ERR_INVALID;

    kfs_inode_t* node = &kfs.inodes[inum];
    if (node->type == KFS_TYPE_DIR && node->child_count > 0)
        return KFS_ERR_NOT_EMPTY;

    /* Liberar bloques de datos */
    for (int i = 0; i < node->block_count; i++) {
        free_block(node->blocks[i]);
    }

    /* Quitar de la lista del padre */
    kfs_inode_t* parent = &kfs.inodes[node->parent_inode];
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == (uint8_t)inum) {
            /* Mover el último al hueco */
            parent->children[i] = parent->children[--parent->child_count];
            break;
        }
    }

    if (node->type == KFS_TYPE_FILE) kfs.total_files--;
    else                              kfs.total_dirs--;

    free_inode((uint8_t)inum);
    return KFS_OK;
}

int kfs_rmdir(const char* path) {
    return kfs_delete(path);
}

/* =============================================================================
 * ESTADÍSTICAS
 * ============================================================================= */

void kfs_stat(void) {
    uint32_t used_blocks  = KFS_MAX_BLOCKS  - kfs_free_blocks();
    uint32_t used_inodes  = KFS_MAX_INODES  - kfs_free_inodes();
    uint32_t used_kb      = (used_blocks * KFS_BLOCK_SIZE) / 1024;
    uint32_t total_kb     = (KFS_MAX_BLOCKS * KFS_BLOCK_SIZE) / 1024;

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n  KosmoFS Statistics\n");
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    for (int i = 0; i < 40; i++) vga_putchar('-');
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    kprintf("  Filesystem    KosmoFS v1 (RAM disk)\n");
    kprintf("  Total space   %u KB (%u blocks x %u B)\n",
            total_kb, KFS_MAX_BLOCKS, KFS_BLOCK_SIZE);
    kprintf("  Used space    %u KB (%u blocks)\n", used_kb, used_blocks);
    kprintf("  Free space    %u KB (%u blocks)\n",
            total_kb - used_kb, kfs_free_blocks());
    kprintf("  Inodes total  %u\n", (uint32_t)KFS_MAX_INODES);
    kprintf("  Inodes used   %u (files: %u, dirs: %u)\n",
            used_inodes, kfs.total_files, kfs.total_dirs);
    kprintf("  Inodes free   %u\n", kfs_free_inodes());
    kprintf("\n");
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
}
