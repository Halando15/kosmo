/* =============================================================================
 * KOSMO OS — Shell (Terminal Interactiva)
 * Archivo : shell/shell.h
 * Función : API pública de la terminal de Kosmo OS.
 *           Parser de comandos, historial, prompt con colores,
 *           y registro de comandos externos.
 * ============================================================================= */

#ifndef KOSMO_SHELL_H
#define KOSMO_SHELL_H

#include "types.h"

/* =============================================================================
 * CONSTANTES DEL SHELL
 * ============================================================================= */

#define SHELL_MAX_CMD_LEN       256     /* Longitud máxima de una línea */
#define SHELL_MAX_ARGS          16      /* Máximo número de argumentos */
#define SHELL_ARG_MAX_LEN       64      /* Longitud máxima de un argumento */
#define SHELL_HISTORY_SIZE      20      /* Entradas en el historial */
#define SHELL_MAX_COMMANDS      32      /* Comandos registrables */

/* Nombre del usuario y hostname para el prompt */
#define SHELL_USER              "root"
#define SHELL_HOSTNAME          "kosmo"

/* =============================================================================
 * ESTRUCTURAS
 * ============================================================================= */

/* Argumentos parseados de una línea de comando */
typedef struct {
    int   argc;                             /* Número de argumentos */
    char  argv[SHELL_MAX_ARGS]              /* Argumentos individuales */
               [SHELL_ARG_MAX_LEN];
    char  raw[SHELL_MAX_CMD_LEN];           /* Línea original sin parsear */
} shell_args_t;

/* Descriptor de un comando registrado */
typedef struct {
    const char* name;                       /* Nombre del comando */
    const char* description;               /* Descripción corta (para help) */
    const char* usage;                     /* Uso (ej: "echo [texto]") */
    int (*handler)(shell_args_t* args);    /* Función que ejecuta el comando */
} shell_command_t;

/* Códigos de retorno de comandos */
#define SHELL_OK        0
#define SHELL_ERROR     1
#define SHELL_EXIT      2       /* Señal para salir del shell (p.ej: shutdown) */

/* =============================================================================
 * API DEL SHELL
 * ============================================================================= */

/* Iniciar el shell (bucle principal — no retorna) */
void shell_start(void);

/* Registrar un nuevo comando */
void shell_register_command(const shell_command_t* cmd);

/* Ejecutar una cadena de comandos desde el kernel */
int shell_exec(const char* cmdline);

/* Mostrar el prompt */
void shell_print_prompt(void);

/* Parsear una línea en argumentos */
void shell_parse_args(const char* line, shell_args_t* out);

#endif /* KOSMO_SHELL_H */
