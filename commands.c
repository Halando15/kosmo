/* =============================================================================
 * KOSMO OS — Comandos Built-in del Shell
 * Archivo : shell/commands/commands.c
 * Función : Implementación de todos los comandos internos:
 *           help, clear, about, echo, reboot, halt,
 *           ver, uptime, mem, ls, color, history
 * ============================================================================= */

#include "commands.h"
#include "shell.h"
#include "vga.h"
#include "pit.h"
#include "stdio.h"
#include "string.h"
#include "io.h"
#include "types.h"
#include "kernel.h"
#include "multiboot.h"

/* =============================================================================
 * UTILIDADES INTERNAS DE FORMATO
 * ============================================================================= */

/* Imprimir una línea horizontal de ancho fijo */
static void print_hline(char ch, int width, vga_color_t color) {
    vga_set_color(color, VGA_COLOR_BLACK);
    for (int i = 0; i < width; i++) vga_putchar(ch);
    vga_putchar('\n');
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
}

/* Imprimir etiqueta + valor con colores */
static void print_kv(const char* key, const char* val) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN,  VGA_COLOR_BLACK);
    kprintf("  %-18s", key);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("%s\n", val);
}

static void print_kv_uint(const char* key, uint32_t val) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN,  VGA_COLOR_BLACK);
    kprintf("  %-18s", key);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("%u\n", val);
}

/* =============================================================================
 * CMD: help — Listar todos los comandos disponibles
 * ============================================================================= */
int cmd_help(shell_args_t* args) {
    /* Si se da un comando específico, mostrar su ayuda detallada */
    if (args->argc >= 2) {
        extern shell_command_t cmd_table[];
        extern uint32_t cmd_count;

        /* Acceder a los campos internos del shell vía shell_exec */
        /* Hacemos una búsqueda directa pasando por shell_exec con --help */
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        kprintf("\n  No detailed help for '%s' available.\n\n",
                args->argv[1]);
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_OK;
    }

    vga_putchar('\n');
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("  Kosmo OS — Built-in Commands\n");
    print_hline('-', 50, VGA_COLOR_DARK_GREY);

    /* Tabla con columnas: nombre (12) + descripción */
    struct { const char* name; const char* desc; const char* usage; } cmds[] = {
        { "help",    "Show this help message",            "help [command]"    },
        { "clear",   "Clear the screen",                  "clear"             },
        { "about",   "About Kosmo OS",                    "about"             },
        { "ver",     "Show OS version",                   "ver"               },
        { "echo",    "Print text to screen",              "echo [text...]"    },
        { "uptime",  "Show system uptime",                "uptime"            },
        { "mem",     "Show memory information",           "mem"               },
        { "ls",      "List directory contents",           "ls [path]"         },
        { "color",   "Change terminal colors",            "color [fg] [bg]"   },
        { "history", "Show command history",              "history"           },
        { "reboot",  "Restart the computer",              "reboot"            },
        { "halt",    "Shutdown the computer",             "halt"              },
    };

    for (uint32_t i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  %-12s", cmds[i].name);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        kprintf("%-28s", cmds[i].desc);
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        kprintf("%s\n", cmds[i].usage);
    }

    print_hline('-', 50, VGA_COLOR_DARK_GREY);
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("  Tip: Use TAB to autocomplete. UP/DOWN for history.\n\n");
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: clear — Limpiar la pantalla
 * ============================================================================= */
int cmd_clear(shell_args_t* args) {
    (void)args;
    vga_clear_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: about — Información de Kosmo OS
 * ============================================================================= */
int cmd_about(shell_args_t* args) {
    (void)args;

    vga_putchar('\n');

    /* Logo pequeño */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("  ██╗  ██╗ ██████╗ ███████╗███╗   ███╗ ██████╗ \n");
    kprintf("  ██║ ██╔╝██╔═══██╗██╔════╝████╗ ████║██╔═══██╗\n");
    kprintf("  █████╔╝ ██║   ██║███████╗██╔████╔██║██║   ██║\n");
    kprintf("  ██╔═██╗ ██║   ██║╚════██║██║╚██╔╝██║██║   ██║\n");
    kprintf("  ██║  ██╗╚██████╔╝███████║██║ ╚═╝ ██║╚██████╔╝\n");
    kprintf("  ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝     ╚═╝ ╚═════╝ \n");

    vga_putchar('\n');
    print_hline('=', 50, VGA_COLOR_DARK_GREY);

    print_kv("OS Name:",      KOSMO_NAME " " KOSMO_VERSION_STRING);
    print_kv("Codename:",     KOSMO_CODENAME);
    print_kv("Architecture:", "x86 32-bit (i686)");
    print_kv("Boot Mode:",    "BIOS / Protected Mode");
    print_kv("Bootloader:",   sys_info.bootloader_name);
    print_kv("License:",      "Open Source (MIT)");
    print_kv("Purpose:",      "Lightweight OS for old laptops");

    print_hline('=', 50, VGA_COLOR_DARK_GREY);

    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("  Built with GCC + NASM | x86 Assembly + C99\n");
    kprintf("  \"Fast, light, and real.\"\n\n");
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: ver — Versión corta
 * ============================================================================= */
int cmd_ver(shell_args_t* args) {
    (void)args;
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("\n  %s v%s \"%s\" (x86 32-bit)\n\n",
            KOSMO_NAME, KOSMO_VERSION_STRING, KOSMO_CODENAME);
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: echo — Imprimir texto
 * ============================================================================= */
int cmd_echo(shell_args_t* args) {
    /* Opciones: -n (no nueva línea), -e (interpretar \n, \t) */
    bool newline    = true;
    bool interpret  = false;
    int  start      = 1;

    if (args->argc >= 2 && strcmp(args->argv[1], "-n") == 0) {
        newline = false;
        start   = 2;
    }
    if (args->argc >= 2 && strcmp(args->argv[1], "-e") == 0) {
        interpret = true;
        start     = 2;
    }

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    for (int i = start; i < args->argc; i++) {
        if (i > start) vga_putchar(' ');

        if (interpret) {
            const char* p = args->argv[i];
            while (*p) {
                if (*p == '\\' && *(p+1)) {
                    p++;
                    switch (*p) {
                        case 'n':  vga_putchar('\n'); break;
                        case 't':  vga_putchar('\t'); break;
                        case '\\': vga_putchar('\\'); break;
                        default:   vga_putchar(*p);   break;
                    }
                } else {
                    vga_putchar(*p);
                }
                p++;
            }
        } else {
            vga_puts(args->argv[i]);
        }
    }

    if (newline) vga_putchar('\n');
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: uptime — Tiempo desde el arranque
 * ============================================================================= */
int cmd_uptime(shell_args_t* args) {
    (void)args;

    uint64_t ticks   = pit_get_ticks();
    uint32_t seconds = (uint32_t)(ticks / PIT_TICK_RATE);
    uint32_t minutes = seconds / 60;
    uint32_t hours   = minutes / 60;
    uint32_t days    = hours / 24;

    seconds %= 60;
    minutes %= 60;
    hours   %= 24;

    vga_putchar('\n');
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("  System Uptime\n");
    print_hline('-', 40, VGA_COLOR_DARK_GREY);

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("  ");
    if (days > 0) {
        kprintf("%u day%s, ", days, days == 1 ? "" : "s");
    }
    kprintf("%02u:%02u:%02u", hours, minutes, seconds);

    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("  (%u ticks @ %u Hz)\n\n", (uint32_t)ticks, PIT_TICK_RATE);
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: mem — Información de memoria
 * ============================================================================= */
int cmd_mem(shell_args_t* args) {
    (void)args;

    vga_putchar('\n');
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("  Memory Information\n");
    print_hline('-', 50, VGA_COLOR_DARK_GREY);

    uint32_t total_kb  = sys_info.total_ram_kb;
    uint32_t total_mb  = total_kb / 1024;
    uint32_t kernel_end = (uint32_t)(uintptr_t)&_kernel_end;
    uint32_t kernel_kb  = (kernel_end - 0x100000) / 1024;

    char buf[32];

    /* Total RAM */
    ksprintf(buf, "%u KB (%u MB)", total_kb, total_mb);
    print_kv("Total RAM:", buf);

    /* Kernel */
    ksprintf(buf, "%u KB  [0x100000 - 0x%X]", kernel_kb, kernel_end);
    print_kv("Kernel size:", buf);

    /* Stack */
    uint32_t esp_val;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp_val));
    ksprintf(buf, "0x%08X", esp_val);
    print_kv("Stack (ESP):", buf);

    /* VGA Buffer */
    print_kv("VGA Buffer:", "0xB8000  (Text 80x25)");

    /* Mapa de memoria simplificado */
    print_hline('-', 50, VGA_COLOR_DARK_GREY);
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprintf("  Memory Map:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("  0x00000000 - 0x000003FF  IVT (BIOS)\n");
    kprintf("  0x00007C00 - 0x00007DFF  Bootloader\n");
    kprintf("  0x00090000 - 0x0009FFFF  Kernel Stack\n");
    kprintf("  0x000B8000 - 0x000BFFFF  VGA Text Buffer\n");
    kprintf("  0x00100000 - 0x%08X  Kernel\n", kernel_end);
    kprintf("  0x%08X - 0x%08X  Available (heap)\n",
            ALIGN_UP(kernel_end, 4096),
            total_kb * 1024);
    vga_putchar('\n');
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: ls — Listar archivos (stub hasta Fase 6)
 * ============================================================================= */
int cmd_ls(shell_args_t* args) {
    const char* path = (args->argc >= 2) ? args->argv[1] : "/";

    vga_putchar('\n');
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("  Directory: %s\n", path);
    print_hline('-', 40, VGA_COLOR_DARK_GREY);

    /* Stub: mostrar un sistema de archivos ficticio hasta Fase 6 */
    struct { const char* name; const char* type; const char* size; } entries[] = {
        { "boot/",      "DIR ", "  -" },
        { "kernel/",    "DIR ", "  -" },
        { "etc/",       "DIR ", "  -" },
        { "bin/",       "DIR ", "  -" },
        { "README.txt", "FILE", "512" },
        { "kosmo.cfg",  "FILE", "128" },
    };

    for (uint32_t i = 0; i < sizeof(entries)/sizeof(entries[0]); i++) {
        if (entries[i].name[strlen(entries[i].name)-1] == '/') {
            vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        } else {
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }
        kprintf("  [%s]  %-20s  %s B\n",
                entries[i].type,
                entries[i].name,
                entries[i].size);
    }

    print_hline('-', 40, VGA_COLOR_DARK_GREY);
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("  6 entries  (full FS in Phase 6)\n\n");
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: color — Cambiar colores del terminal
 * Uso: color <fg> <bg>
 * Colores: black, blue, green, cyan, red, magenta, brown,
 *          grey, dark_grey, light_blue, light_green, light_cyan,
 *          light_red, light_magenta, yellow, white
 * ============================================================================= */

static vga_color_t parse_color_name(const char* name) {
    struct { const char* n; vga_color_t c; } map[] = {
        { "black",        VGA_COLOR_BLACK         },
        { "blue",         VGA_COLOR_BLUE          },
        { "green",        VGA_COLOR_GREEN         },
        { "cyan",         VGA_COLOR_CYAN          },
        { "red",          VGA_COLOR_RED           },
        { "magenta",      VGA_COLOR_MAGENTA       },
        { "brown",        VGA_COLOR_BROWN         },
        { "grey",         VGA_COLOR_LIGHT_GREY    },
        { "dark_grey",    VGA_COLOR_DARK_GREY     },
        { "light_blue",   VGA_COLOR_LIGHT_BLUE    },
        { "light_green",  VGA_COLOR_LIGHT_GREEN   },
        { "light_cyan",   VGA_COLOR_LIGHT_CYAN    },
        { "light_red",    VGA_COLOR_LIGHT_RED     },
        { "light_magenta",VGA_COLOR_LIGHT_MAGENTA },
        { "yellow",       VGA_COLOR_YELLOW        },
        { "white",        VGA_COLOR_WHITE         },
    };
    for (uint32_t i = 0; i < 16; i++) {
        if (strcmp(map[i].n, name) == 0) return map[i].c;
    }
    return VGA_COLOR_WHITE; /* Default */
}

int cmd_color(shell_args_t* args) {
    if (args->argc < 3) {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        kprintf("\n  Usage: color <fg> <bg>\n");
        kprintf("  Colors: black blue green cyan red magenta brown\n");
        kprintf("          grey dark_grey light_blue light_green\n");
        kprintf("          light_cyan light_red yellow white\n");
        kprintf("  Example: color white black\n");
        kprintf("           color light_green black\n\n");
        vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
        return SHELL_OK;
    }

    vga_color_t fg = parse_color_name(args->argv[1]);
    vga_color_t bg = parse_color_name(args->argv[2]);
    vga_set_color(fg, bg);
    vga_clear_color(fg, bg);
    kprintf("  Colors changed: fg=%s bg=%s\n\n",
            args->argv[1], args->argv[2]);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: history — Mostrar historial de comandos
 * ============================================================================= */
int cmd_history(shell_args_t* args) {
    (void)args;

    vga_putchar('\n');
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("  Command History\n");
    print_hline('-', 40, VGA_COLOR_DARK_GREY);

    /* Acceder al historial externo definido en shell.c */
    /* Re-ejecutamos con la señal interna — usamos extern */
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("  (Use UP/DOWN arrows to navigate)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("  History is stored in memory only.\n\n");
    vga_set_color(VGA_DEFAULT_FG, VGA_COLOR_BLACK);
    return SHELL_OK;
}

/* =============================================================================
 * CMD: reboot — Reiniciar el sistema
 * ============================================================================= */
int cmd_reboot(shell_args_t* args) {
    (void)args;

    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprintf("\n  Rebooting Kosmo OS...\n\n");

    /* Pequeña pausa antes de reiniciar */
    sleep_ms(1000);

    /* Método 1: Pulso de reset a través del controlador de teclado 8042 */
    /* Vaciamos el buffer de entrada del controlador */
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);   /* Comando de reset del CPU */
    io_wait();

    /* Método 2: Triple Fault (IDT vacía + interrupción) — fallback */
    cli();
    /* Cargar una IDT inválida de tamaño 0 y lanzar una interrupción */
    volatile uint8_t idtr[6] = {0};
    __asm__ volatile("lidt (%0)\n\t"
                     "int $3"
                     :: "r"(idtr));

    /* No debería llegar aquí */
    for (;;) hlt();
    return SHELL_OK;
}

/* =============================================================================
 * CMD: halt — Apagar el sistema
 * ============================================================================= */
int cmd_halt(shell_args_t* args) {
    (void)args;
    return SHELL_EXIT;   /* El shell principal maneja el SHELL_EXIT */
}

/* =============================================================================
 * REGISTRO DE TODOS LOS COMANDOS
 * ============================================================================= */

void commands_register_all(void) {
    static const shell_command_t cmds[] = {
        { "help",    "Show available commands",    "help [cmd]",    cmd_help    },
        { "clear",   "Clear the screen",           "clear",         cmd_clear   },
        { "about",   "About Kosmo OS",             "about",         cmd_about   },
        { "ver",     "Show version",               "ver",           cmd_ver     },
        { "echo",    "Print text",                 "echo [text]",   cmd_echo    },
        { "uptime",  "Show uptime",                "uptime",        cmd_uptime  },
        { "mem",     "Memory info",                "mem",           cmd_mem     },
        { "ls",      "List files",                 "ls [path]",     cmd_ls      },
        { "color",   "Change colors",              "color fg bg",   cmd_color   },
        { "history", "Command history",            "history",       cmd_history },
        { "reboot",  "Reboot system",              "reboot",        cmd_reboot  },
        { "halt",    "Halt system",                "halt",          cmd_halt    },
    };

    for (uint32_t i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++) {
        shell_register_command(&cmds[i]);
    }
}
