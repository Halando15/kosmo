/* =============================================================================
 * KOSMO OS — Comandos del Shell
 * Archivo : shell/commands/commands.h
 * Función : Declaraciones de todos los comandos internos del shell.
 * ============================================================================= */

#ifndef KOSMO_COMMANDS_H
#define KOSMO_COMMANDS_H

#include "shell.h"

/* Función que registra todos los comandos en el shell */
void commands_register_all(void);

/* Declaración de cada handler de comando */
int cmd_help   (shell_args_t* args);
int cmd_clear  (shell_args_t* args);
int cmd_about  (shell_args_t* args);
int cmd_echo   (shell_args_t* args);
int cmd_reboot (shell_args_t* args);
int cmd_halt   (shell_args_t* args);
int cmd_ver    (shell_args_t* args);
int cmd_uptime (shell_args_t* args);
int cmd_mem    (shell_args_t* args);
int cmd_ls     (shell_args_t* args);
int cmd_color  (shell_args_t* args);
int cmd_history(shell_args_t* args);

#endif /* KOSMO_COMMANDS_H */
