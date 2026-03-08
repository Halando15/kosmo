/* =============================================================================
 * KOSMO OS — Comandos de Sistema de Archivos
 * Archivo : shell/commands/cmd_fs.c
 * Función : Comandos del shell que interactúan con KosmoFS:
 *           ls, cat, write, rm, mkdir, rmdir, pwd, cd, df, find, touch, cp
 * ============================================================================= */

#include "commands.h"
#include "shell.h"
#include "kosmofs.h"
#include "vga.h"
#include "pit.h"
#include "stdio.h"
#include "string.h"
#include "types.h"

/* Directorio de trabajo actual (compartido con shell.c via extern) */
extern char cwd[64];

/* =============================================================================
 * UTILIDADES INTERNAS
 * ============================================================================= */

/* Construir ruta absoluta a partir del cwd y un argumento */
static void build_path(const char* arg, char* out, uint32_t out_size) {
    if (!arg || arg[0] == '\0') {
        strncpy(out, cwd, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    if (arg[0] == '/') {
        /* Ruta absoluta */
        strncpy(out, arg, out_size - 1);
    } else {
        /* Ruta relativa */
        if (strcmp(cwd, "/") == 0) {
            ksprintf(out, "/%s", arg);
        } else {
            ksprintf(out, "%s/%s", cwd, arg);
        }
    }
    out[out_size - 1] = '\0';
}

/* Formatear tamaño en bytes de forma legible */
static void format_size(uint32_t bytes, char* out) {
    if (bytes < 1024)
        ksprintf(out, "%4u B ", bytes);
    else if (bytes < 1024 * 1024)
        ksprintf(out, "%4u KB", bytes / 1024);
    else
        ksprintf(out, "%4u MB", bytes / (1024 * 1024));
}

/* Formatear ticks como tiempo HH:MM:SS */
static void format_time(uint32_t ticks, char* out) {
    uint32_t secs = ticks / PIT_TICK_RATE;
    uint32_t mins = secs / 60;
    uint32_t hrs  = mins / 60;
    ksprintf(out, "%02u:%02u:%02u", hrs % 24, mins % 60, secs % 60);
}

/* =============================================================================
 * CMD: ls — Listar directorio (versión real con KosmoFS)
 * Uso: ls [-l] [-a] [path]
 * ============================================================================= */
int cmd_ls(shell_args_t* args) {
    bool long_fmt = false;
    const char* path_arg = NULL;

    for (int i = 1; i < args->argc; i++) {
        if (strcmp(args->argv[i], "-l") == 0) long_fmt = true;
        else if (strcmp(args->argv[i], "-la") == 0 ||
                 strcmp(args->argv[i], "-al") == 0) long_fmt = true;
        else path_arg = args->argv[i];
    }

    char path[128];
    build_path(path_arg, path, sizeof(path));

    /* Verificar que el path existe */
    int inum = kfs_resolve_path(path);
    if (inum < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  ls: '%s': No such file or directory\n", path);
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }

    kfs_inode_t* node = kfs_get_inode((uint8_t)inum);

    /* Si es un archivo, mostrarlo directamente */
    if (node && node->type == KFS_TYPE_FILE) {
        char szb[16];
        format_size(node->size, szb);
        if (long_fmt) {
            char ts[12];
            format_time(node->modified_at, ts);
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            kprintf("  -rw-  %s  %s  %s\n", szb, ts, node->name);
        } else {
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            kprintf("  %s\n", node->name);
        }
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_OK;
    }

    /* Listar directorio */
    kfs_dirent_t entries[KFS_MAX_CHILDREN];
    int count = kfs_listdir(path, entries, KFS_MAX_CHILDREN);

    if (count < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  ls: '%s': Not a directory\n", path);
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }

    if (count == 0) {
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        kprintf("  (empty directory)\n");
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_OK;
    }

    kprintf("\n");
    if (long_fmt) {
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        kprintf("  Type  Perms  Size    Modified    Name\n");
        for (int i = 0; i < 50; i++) vga_putchar('-');
        kprintf("\n");
    }

    uint32_t total_size = 0;
    for (int i = 0; i < count; i++) {
        if (entries[i].type == KFS_TYPE_DIR) {
            vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            if (long_fmt) {
                char ts[12];
                format_time(entries[i].modified_at, ts);
                kprintf("  drwx  ----   ----    %s  %s/\n",
                        ts, entries[i].name);
            } else {
                kprintf("  %s/\n", entries[i].name);
            }
        } else {
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            total_size += entries[i].size;
            if (long_fmt) {
                char szb[16], ts[12];
                format_size(entries[i].size, szb);
                format_time(entries[i].modified_at, ts);
                kprintf("  -rw-  rw--  %s  %s  %s\n",
                        szb, ts, entries[i].name);
            } else {
                kprintf("  %s\n", entries[i].name);
            }
        }
    }

    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("\n  %d item%s", count, count == 1 ? "" : "s");
    if (total_size > 0) {
        char szb[16];
        format_size(total_size, szb);
        kprintf("  |  %s total", szb);
    }
    kprintf("\n\n");
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: cat — Leer un archivo y mostrarlo
 * Uso: cat <path>
 * ============================================================================= */
int cmd_cat(shell_args_t* args) {
    if (args->argc < 2) {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        kprintf("  Usage: cat <file>\n");
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }

    char path[128];
    build_path(args->argv[1], path, sizeof(path));

    int sz = kfs_get_size(path);
    if (sz < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  cat: '%s': No such file\n", path);
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }

    /* Comprobar que es archivo */
    int inum = kfs_resolve_path(path);
    kfs_inode_t* node = kfs_get_inode((uint8_t)inum);
    if (node && node->type == KFS_TYPE_DIR) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  cat: '%s': Is a directory\n", path);
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }

    if (sz == 0) {
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        kprintf("  (empty file)\n");
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_OK;
    }

    /* Leer y mostrar */
    static char buf[KFS_MAX_FILE_BLOCKS * KFS_BLOCK_SIZE + 1];
    int read = kfs_read(path, buf, sizeof(buf) - 1);
    if (read < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  cat: read error\n");
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }
    buf[read] = '\0';

    vga_putchar('\n');
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(buf);
    if (buf[read-1] != '\n') vga_putchar('\n');
    vga_putchar('\n');
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: write — Escribir texto en un archivo
 * Uso: write <path> <texto...>
 * ============================================================================= */
int cmd_write(shell_args_t* args) {
    if (args->argc < 3) {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        kprintf("  Usage: write <file> <text...>\n");
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }

    char path[128];
    build_path(args->argv[1], path, sizeof(path));

    /* Unir todos los argumentos desde el 2 con espacios */
    char content[KFS_MAX_FILE_BLOCKS * KFS_BLOCK_SIZE];
    content[0] = '\0';
    for (int i = 2; i < args->argc; i++) {
        if (i > 2) strcat(content, " ");
        strcat(content, args->argv[i]);
    }
    strcat(content, "\n");

    int r = kfs_write(path, content, (uint32_t)strlen(content));
    if (r == KFS_OK) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  Written %u bytes to '%s'\n",
                (uint32_t)strlen(content), path);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  write: error %d writing '%s'\n", r, path);
    }
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return (r == KFS_OK) ? SHELL_OK : SHELL_ERROR;
}

/* =============================================================================
 * CMD: append — Añadir texto al final de un archivo
 * Uso: append <path> <texto...>
 * ============================================================================= */
int cmd_append(shell_args_t* args) {
    if (args->argc < 3) {
        kprintf("  Usage: append <file> <text...>\n");
        return SHELL_ERROR;
    }
    char path[128];
    build_path(args->argv[1], path, sizeof(path));

    char content[512];
    content[0] = '\0';
    for (int i = 2; i < args->argc; i++) {
        if (i > 2) strcat(content, " ");
        strcat(content, args->argv[i]);
    }
    strcat(content, "\n");

    int r = kfs_append(path, content, (uint32_t)strlen(content));
    if (r == KFS_OK) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  Appended %u bytes to '%s'\n",
                (uint32_t)strlen(content), path);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  append: error %d\n", r);
    }
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return (r == KFS_OK) ? SHELL_OK : SHELL_ERROR;
}

/* =============================================================================
 * CMD: rm — Borrar archivo o directorio vacío
 * ============================================================================= */
int cmd_rm(shell_args_t* args) {
    if (args->argc < 2) {
        kprintf("  Usage: rm <file>\n");
        return SHELL_ERROR;
    }
    char path[128];
    build_path(args->argv[1], path, sizeof(path));

    int r = kfs_delete(path);
    if (r == KFS_OK) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  Deleted '%s'\n", path);
    } else if (r == KFS_ERR_NOT_FOUND) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  rm: '%s': Not found\n", path);
    } else if (r == KFS_ERR_NOT_EMPTY) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  rm: '%s': Directory not empty (use rmdir)\n", path);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  rm: error %d\n", r);
    }
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return (r == KFS_OK) ? SHELL_OK : SHELL_ERROR;
}

/* =============================================================================
 * CMD: mkdir — Crear directorio
 * ============================================================================= */
int cmd_mkdir(shell_args_t* args) {
    if (args->argc < 2) {
        kprintf("  Usage: mkdir <path>\n");
        return SHELL_ERROR;
    }
    char path[128];
    build_path(args->argv[1], path, sizeof(path));

    int r = kfs_mkdir(path);
    if (r == KFS_OK) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  Directory '%s' created\n", path);
    } else if (r == KFS_ERR_EXISTS) {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        kprintf("  mkdir: '%s': Already exists\n", path);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  mkdir: error %d creating '%s'\n", r, path);
    }
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return (r == KFS_OK) ? SHELL_OK : SHELL_ERROR;
}

/* =============================================================================
 * CMD: rmdir — Borrar directorio vacío
 * ============================================================================= */
int cmd_rmdir(shell_args_t* args) {
    if (args->argc < 2) {
        kprintf("  Usage: rmdir <path>\n");
        return SHELL_ERROR;
    }
    char path[128];
    build_path(args->argv[1], path, sizeof(path));
    return cmd_rm(args);   /* rm ya maneja dirs vacíos */
}

/* =============================================================================
 * CMD: pwd — Mostrar directorio actual
 * ============================================================================= */
int cmd_pwd(shell_args_t* args) {
    (void)args;
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("  %s\n", cwd);
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: cd — Cambiar directorio
 * ============================================================================= */
int cmd_cd(shell_args_t* args) {
    const char* dest = (args->argc >= 2) ? args->argv[1] : "/";

    char path[128];
    if (strcmp(dest, "..") == 0) {
        /* Subir un nivel */
        if (strcmp(cwd, "/") == 0) return SHELL_OK;
        /* Encontrar último '/' */
        int len = (int)strlen(cwd);
        while (len > 1 && cwd[len-1] != '/') len--;
        if (len == 1) {
            strcpy(cwd, "/");
        } else {
            strncpy(path, cwd, (uint32_t)(len-1));
            path[len-1] = '\0';
            strncpy(cwd, path, 63);
        }
        return SHELL_OK;
    }

    build_path(dest, path, sizeof(path));

    int inum = kfs_resolve_path(path);
    if (inum < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  cd: '%s': No such directory\n", path);
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }

    kfs_inode_t* node = kfs_get_inode((uint8_t)inum);
    if (!node || node->type != KFS_TYPE_DIR) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  cd: '%s': Not a directory\n", path);
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_ERROR;
    }

    strncpy(cwd, path, 63);
    cwd[63] = '\0';
    return SHELL_OK;
}

/* =============================================================================
 * CMD: df — Espacio en disco (filesystem stats)
 * ============================================================================= */
int cmd_df(shell_args_t* args) {
    (void)args;
    kfs_stat();
    return SHELL_OK;
}

/* =============================================================================
 * CMD: touch — Crear archivo vacío
 * ============================================================================= */
int cmd_touch(shell_args_t* args) {
    if (args->argc < 2) {
        kprintf("  Usage: touch <file>\n");
        return SHELL_ERROR;
    }
    char path[128];
    build_path(args->argv[1], path, sizeof(path));

    if (kfs_exists(path)) {
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        kprintf("  '%s' already exists (timestamps updated)\n", path);
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_OK;
    }

    int r = kfs_write(path, "", 0);
    if (r == KFS_OK) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  Created '%s'\n", path);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  touch: error %d\n", r);
    }
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return (r == KFS_OK) ? SHELL_OK : SHELL_ERROR;
}

/* =============================================================================
 * CMD: find — Buscar archivos por nombre
 * Uso: find [path] <nombre>
 * ============================================================================= */
static void find_recursive(const char* dir_path, const char* name) {
    kfs_dirent_t entries[KFS_MAX_CHILDREN];
    int count = kfs_listdir(dir_path, entries, KFS_MAX_CHILDREN);
    if (count < 0) return;

    for (int i = 0; i < count; i++) {
        char full[128];
        if (strcmp(dir_path, "/") == 0)
            ksprintf(full, "/%s", entries[i].name);
        else
            ksprintf(full, "%s/%s", dir_path, entries[i].name);

        /* Buscar nombre (substring match) */
        if (strstr(entries[i].name, name) != NULL) {
            if (entries[i].type == KFS_TYPE_DIR) {
                vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
                kprintf("  %s/\n", full);
            } else {
                char szb[16];
                format_size(entries[i].size, szb);
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                kprintf("  %-40s  %s\n", full, szb);
            }
        }

        /* Recursión en subdirectorios */
        if (entries[i].type == KFS_TYPE_DIR) {
            find_recursive(full, name);
        }
    }
}

int cmd_find(shell_args_t* args) {
    if (args->argc < 2) {
        kprintf("  Usage: find [path] <name>\n");
        return SHELL_ERROR;
    }

    const char* search_path;
    const char* name;
    if (args->argc >= 3) {
        search_path = args->argv[1];
        name        = args->argv[2];
    } else {
        search_path = cwd;
        name        = args->argv[1];
    }

    vga_putchar('\n');
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprintf("  Searching for '%s' in '%s'...\n\n", name, search_path);

    find_recursive(search_path, name);

    vga_putchar('\n');
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * REGISTRO DE COMANDOS FS (añadir en commands_register_all)
 * ============================================================================= */
void fs_commands_register(void) {
    static const shell_command_t fs_cmds[] = {
        { "ls",     "List directory",      "ls [-l] [path]",    cmd_ls     },
        { "cat",    "Print file contents", "cat <file>",        cmd_cat    },
        { "write",  "Write file",          "write <f> <text>",  cmd_write  },
        { "append", "Append to file",      "append <f> <text>", cmd_append },
        { "rm",     "Remove file",         "rm <file>",         cmd_rm     },
        { "mkdir",  "Create directory",    "mkdir <dir>",       cmd_mkdir  },
        { "rmdir",  "Remove directory",    "rmdir <dir>",       cmd_rmdir  },
        { "pwd",    "Print working dir",   "pwd",               cmd_pwd    },
        { "cd",     "Change directory",    "cd [path]",         cmd_cd     },
        { "df",     "Disk/FS statistics",  "df",                cmd_df     },
        { "touch",  "Create empty file",   "touch <file>",      cmd_touch  },
        { "find",   "Find files by name",  "find [path] <name>",cmd_find   },
    };
    for (uint32_t i = 0; i < 12; i++) {
        shell_register_command(&fs_cmds[i]);
    }
}
