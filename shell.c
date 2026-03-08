/* =============================================================================
 * KOSMO OS — Shell (Motor Principal)
 * Archivo : shell/shell.c
 * Función : Bucle principal de la terminal, parser de argumentos,
 *           historial de comandos circular, prompt con colores,
 *           y dispatcher de comandos registrados.
 * ============================================================================= */

#include "shell.h"
#include "commands/commands.h"
#include "vga.h"
#include "keyboard.h"
#include "pit.h"
#include "stdio.h"
#include "string.h"
#include "types.h"

/* =============================================================================
 * ESTADO INTERNO DEL SHELL
 * ============================================================================= */

/* Tabla de comandos registrados */
static shell_command_t cmd_table[SHELL_MAX_COMMANDS];
static uint32_t        cmd_count = 0;

/* Historial de comandos (ring buffer) */
static char     history[SHELL_HISTORY_SIZE][SHELL_MAX_CMD_LEN];
static int      history_count = 0;      /* Total de entradas guardadas */
static int      history_head  = 0;      /* Índice del más reciente */

/* Buffer de entrada activo */
static char     input_buf[SHELL_MAX_CMD_LEN];
static uint32_t input_len  = 0;
static int      history_nav = -1;       /* -1 = no navegando historial */

/* Directorio de trabajo actual */
static char cwd[64] = "/";

/* =============================================================================
 * REGISTRO DE COMANDOS
 * ============================================================================= */

void shell_register_command(const shell_command_t* cmd) {
    if (cmd_count >= SHELL_MAX_COMMANDS) return;
    cmd_table[cmd_count++] = *cmd;
}

/* =============================================================================
 * HISTORIAL
 * ============================================================================= */

static void history_push(const char* line) {
    if (!line || line[0] == '\0') return;

    /* No guardar si es igual al último */
    if (history_count > 0) {
        int last = (history_head - 1 + SHELL_HISTORY_SIZE) % SHELL_HISTORY_SIZE;
        if (strcmp(history[last], line) == 0) return;
    }

    strncpy(history[history_head], line, SHELL_MAX_CMD_LEN - 1);
    history[history_head][SHELL_MAX_CMD_LEN - 1] = '\0';
    history_head  = (history_head + 1) % SHELL_HISTORY_SIZE;
    if (history_count < SHELL_HISTORY_SIZE) history_count++;
}

static const char* history_get(int offset) {
    /* offset=0 → más reciente, offset=1 → anterior, etc. */
    if (offset < 0 || offset >= history_count) return NULL;
    int idx = (history_head - 1 - offset + SHELL_HISTORY_SIZE * 2)
              % SHELL_HISTORY_SIZE;
    return history[idx];
}

/* =============================================================================
 * PARSER DE ARGUMENTOS
 * ============================================================================= */

/**
 * shell_parse_args — Dividir una línea en argc/argv
 * Soporta comillas dobles para argumentos con espacios.
 * Ej: echo "hola mundo" -> argc=2, argv[0]="echo", argv[1]="hola mundo"
 */
void shell_parse_args(const char* line, shell_args_t* out) {
    memset(out, 0, sizeof(shell_args_t));
    strncpy(out->raw, line, SHELL_MAX_CMD_LEN - 1);

    const char* p   = line;
    int         argc = 0;

    while (*p && argc < SHELL_MAX_ARGS) {
        /* Saltar espacios */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        char* dst  = out->argv[argc];
        int   dlen = 0;
        bool  in_quotes = false;

        if (*p == '"') {
            in_quotes = true;
            p++;
        }

        while (*p && dlen < SHELL_ARG_MAX_LEN - 1) {
            if (in_quotes) {
                if (*p == '"') { p++; break; }
            } else {
                if (*p == ' ' || *p == '\t') break;
            }
            dst[dlen++] = *p++;
        }
        dst[dlen] = '\0';

        if (dlen > 0) argc++;
    }
    out->argc = argc;
}

/* =============================================================================
 * EJECUCIÓN DE COMANDOS
 * ============================================================================= */

/**
 * shell_exec — Buscar y ejecutar un comando por nombre
 */
int shell_exec(const char* cmdline) {
    /* Saltar espacios iniciales */
    while (*cmdline == ' ') cmdline++;
    if (*cmdline == '\0') return SHELL_OK;

    shell_args_t args;
    shell_parse_args(cmdline, &args);
    if (args.argc == 0) return SHELL_OK;

    /* Buscar en la tabla de comandos */
    for (uint32_t i = 0; i < cmd_count; i++) {
        if (strcmp(cmd_table[i].name, args.argv[0]) == 0) {
            return cmd_table[i].handler(&args);
        }
    }

    /* Comando no encontrado */
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    kprintf("  '%s': command not found. Type 'help' for a list.\n",
            args.argv[0]);
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_ERROR;
}

/* =============================================================================
 * PROMPT
 * ============================================================================= */

void shell_print_prompt(void) {
    /* Estilo: root@kosmo:/$ */
    vga_set_color(VGA_COLOR_LIGHT_GREEN,  VGA_COLOR_BLACK);
    kprintf("%s@%s", SHELL_USER, SHELL_HOSTNAME);
    vga_set_color(VGA_COLOR_LIGHT_GREY,   VGA_COLOR_BLACK);
    kprintf(":");
    vga_set_color(VGA_COLOR_LIGHT_CYAN,   VGA_COLOR_BLACK);
    kprintf("%s", cwd);
    vga_set_color(VGA_COLOR_LIGHT_GREY,   VGA_COLOR_BLACK);
    kprintf("# ");
    vga_set_color(VGA_COLOR_WHITE,        VGA_COLOR_BLACK);
}

/* =============================================================================
 * LECTURA DE LÍNEA CON HISTORIAL Y FLECHAS
 * Gestiona: caracteres imprimibles, Backspace, Enter,
 *           flecha arriba/abajo (navegar historial),
 *           Ctrl+C (cancelar), Ctrl+L (limpiar pantalla).
 * ============================================================================= */

static uint32_t shell_readline_with_history(char* buf, uint32_t max_len) {
    input_len   = 0;
    history_nav = -1;
    memset(input_buf, 0, sizeof(input_buf));
    memset(buf,       0, max_len);

    /* Guardar posición del cursor al inicio del prompt de entrada */
    uint8_t prompt_col, prompt_row;
    vga_get_cursor(&prompt_col, &prompt_row);

    while (true) {
        char c = keyboard_getchar_blocking();

        /* ── Flecha arriba: historial hacia atrás ──────────────────────── */
        if ((uint8_t)c == KEY_UP) {
            int next_nav = history_nav + 1;
            const char* entry = history_get(next_nav);
            if (entry) {
                history_nav = next_nav;
                /* Borrar la línea actual en pantalla */
                vga_set_cursor(prompt_col, prompt_row);
                for (uint32_t i = 0; i < input_len; i++) vga_putchar(' ');
                vga_set_cursor(prompt_col, prompt_row);
                /* Escribir entrada del historial */
                strncpy(input_buf, entry, max_len - 1);
                input_len = (uint32_t)strlen(input_buf);
                vga_puts(input_buf);
            }
            continue;
        }

        /* ── Flecha abajo: historial hacia adelante ─────────────────────── */
        if ((uint8_t)c == KEY_DOWN) {
            int next_nav = history_nav - 1;
            vga_set_cursor(prompt_col, prompt_row);
            for (uint32_t i = 0; i < input_len; i++) vga_putchar(' ');
            vga_set_cursor(prompt_col, prompt_row);

            if (next_nav < 0) {
                history_nav = -1;
                input_len   = 0;
                memset(input_buf, 0, sizeof(input_buf));
            } else {
                const char* entry = history_get(next_nav);
                if (entry) {
                    history_nav = next_nav;
                    strncpy(input_buf, entry, max_len - 1);
                    input_len = (uint32_t)strlen(input_buf);
                    vga_puts(input_buf);
                }
            }
            continue;
        }

        /* ── Enter: confirmar ────────────────────────────────────────────── */
        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            input_buf[input_len] = '\0';
            strncpy(buf, input_buf, max_len - 1);
            return input_len;
        }

        /* ── Backspace ───────────────────────────────────────────────────── */
        if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                input_buf[input_len] = '\0';
                vga_putchar('\b');
            }
            continue;
        }

        /* ── Ctrl+C: cancelar ────────────────────────────────────────────── */
        if (c == 0x03) {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_puts("^C\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            input_buf[0] = '\0';
            buf[0]       = '\0';
            return 0;
        }

        /* ── Ctrl+L: limpiar pantalla ─────────────────────────────────────── */
        if (c == 0x0C) {
            vga_clear();
            shell_print_prompt();
            /* Redibujar el buffer actual */
            if (input_len > 0) vga_puts(input_buf);
            vga_get_cursor(&prompt_col, &prompt_row);
            /* Recalcular prompt_col */
            prompt_col = (uint8_t)(vga_col() - input_len % VGA_WIDTH);
            continue;
        }

        /* ── Tab: autocompletar nombre de comando ────────────────────────── */
        if (c == '\t') {
            if (input_len > 0) {
                /* Buscar comandos que empiecen por lo escrito */
                int matches = 0;
                const char* match = NULL;
                for (uint32_t i = 0; i < cmd_count; i++) {
                    if (strncmp(cmd_table[i].name, input_buf, input_len) == 0) {
                        matches++;
                        match = cmd_table[i].name;
                    }
                }
                if (matches == 1 && match) {
                    /* Completar la única coincidencia */
                    uint32_t mlen = (uint32_t)strlen(match);
                    /* Borrar lo escrito y reescribir completo */
                    for (uint32_t i = 0; i < input_len; i++) vga_putchar('\b');
                    strncpy(input_buf, match, SHELL_MAX_CMD_LEN - 1);
                    input_len = mlen;
                    vga_puts(input_buf);
                } else if (matches > 1) {
                    /* Mostrar todas las coincidencias */
                    vga_putchar('\n');
                    for (uint32_t i = 0; i < cmd_count; i++) {
                        if (strncmp(cmd_table[i].name, input_buf, input_len) == 0) {
                            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                            kprintf("  %s\n", cmd_table[i].name);
                        }
                    }
                    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                    shell_print_prompt();
                    vga_puts(input_buf);
                }
            }
            continue;
        }

        /* ── Carácter imprimible ─────────────────────────────────────────── */
        if (c >= 0x20 && c < 0x7F && input_len < max_len - 1) {
            input_buf[input_len++] = c;
            input_buf[input_len]   = '\0';
            history_nav = -1;
            vga_putchar(c);
        }
    }
}

/* =============================================================================
 * BUCLE PRINCIPAL DEL SHELL
 * ============================================================================= */

/**
 * shell_start — Iniciar la terminal interactiva
 * Esta función NO retorna (bucle infinito).
 */
void shell_start(void) {
    /* Registrar todos los comandos built-in */
    commands_register_all();

    /* Pantalla de bienvenida */
    vga_clear_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n");
    kprintf("  +--------------------------------------------------+\n");
    kprintf("  |          KOSMO OS v0.1 - Terminal                |\n");
    kprintf("  |  Type 'help' for a list of available commands.   |\n");
    kprintf("  +--------------------------------------------------+\n");
    kprintf("\n");
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);

    char cmd_buf[SHELL_MAX_CMD_LEN];

    /* ── Bucle infinito del shell ─────────────────────────────────────── */
    while (true) {
        shell_print_prompt();

        /* Leer línea con soporte de historial */
        uint32_t len = shell_readline_with_history(cmd_buf, sizeof(cmd_buf));

        if (len == 0) continue;         /* Línea vacía o Ctrl+C */

        /* Guardar en historial */
        history_push(cmd_buf);

        /* Ejecutar el comando */
        int result = shell_exec(cmd_buf);

        /* Si el comando devuelve SHELL_EXIT, apagar el sistema */
        if (result == SHELL_EXIT) {
            vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            kprintf("\n  System halted. Safe to power off.\n");
            vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
            cli();
            for (;;) hlt();
        }
    }
}
