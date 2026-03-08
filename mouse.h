/* =============================================================================
 * KOSMO OS — Driver Ratón PS/2
 * Archivo : drivers/input/mouse.h / mouse.c
 * Función : Inicializa el ratón PS/2, recibe paquetes de 3 bytes por IRQ12
 *           y mantiene la posición absoluta del cursor.
 * ============================================================================= */

#ifndef KOSMO_MOUSE_H
#define KOSMO_MOUSE_H

#include "types.h"

/* Estado actual del ratón */
typedef struct {
    int32_t x, y;               /* Posición absoluta (0,0 = esquina superior izquierda) */
    int8_t  dx, dy;             /* Movimiento relativo del último evento */
    bool    btn_left;
    bool    btn_right;
    bool    btn_middle;
    bool    moved;              /* true si hubo movimiento desde la última lectura */
} mouse_state_t;

/* Inicializar el driver del ratón */
void mouse_init(void);

/* Obtener el estado actual */
const mouse_state_t* mouse_get_state(void);

/* Handler IRQ12 — llamado desde la IDT */
void mouse_irq_handler(void* frame);

/* Consultar si el ratón tiene datos pendientes */
bool mouse_has_event(void);

/* Resetear el flag de evento */
void mouse_clear_event(void);

#endif /* KOSMO_MOUSE_H */
