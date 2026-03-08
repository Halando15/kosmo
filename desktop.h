/* =============================================================================
 * KOSMO OS — Aplicaciones del Escritorio
 * Archivo : gui/desktop.c / desktop.h
 * Función : Define las ventanas del escritorio inicial de Kosmo OS:
 *           - Panel de bienvenida (About)
 *           - Explorador de archivos
 *           - Ventana de terminal gráfica
 *           - Iconos del escritorio
 * ============================================================================= */

#ifndef KOSMO_DESKTOP_H
#define KOSMO_DESKTOP_H

#include "wm.h"

/* Inicializar el escritorio: crea las ventanas iniciales */
void desktop_init(void);

/* Callbacks de pintado de cada ventana */
void paint_about_window    (uint8_t id, int cx, int cy, int cw, int ch);
void paint_files_window    (uint8_t id, int cx, int cy, int cw, int ch);
void paint_terminal_window (uint8_t id, int cx, int cy, int cw, int ch);
void paint_sysinfo_window  (uint8_t id, int cx, int cy, int cw, int ch);

/* Dibujar iconos del escritorio */
void desktop_draw_icons(void);

#endif /* KOSMO_DESKTOP_H */
