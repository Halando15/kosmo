/* =============================================================================
 * KOSMO OS — Driver Ratón PS/2 (Implementación)
 * Archivo : drivers/input/mouse.c
 * ============================================================================= */

#include "mouse.h"
#include "idt.h"
#include "io.h"
#include "vesa.h"
#include "stdio.h"

/* =============================================================================
 * ESTADO INTERNO
 * ============================================================================= */
static mouse_state_t mouse_state = {
    .x = VESA_WIDTH  / 2,
    .y = VESA_HEIGHT / 2,
};

/* Buffer de paquetes: el ratón PS/2 envía paquetes de 3 bytes */
static uint8_t  packet[3];
static int      packet_idx = 0;
static bool     has_event  = false;

/* =============================================================================
 * COMUNICACIÓN CON EL CONTROLADOR PS/2
 * ============================================================================= */

static void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while (--timeout && (inb(KB_STATUS) & 0x02));
}

static void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (--timeout && !(inb(KB_STATUS) & 0x01));
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(KB_CMD, 0xD4);     /* Redirigir siguiente byte al ratón */
    mouse_wait_write();
    outb(KB_DATA, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(KB_DATA);
}

/* =============================================================================
 * HANDLER DE IRQ12 — PARSER DE PAQUETES
 * El ratón PS/2 estándar envía paquetes de 3 bytes:
 *   Byte 0: flags (botones + overflow + signo de dx/dy)
 *   Byte 1: movimiento en X (delta, complemento a 2 si el bit de signo está activo)
 *   Byte 2: movimiento en Y (delta, eje Y invertido respecto a pantalla)
 * ============================================================================= */
void mouse_irq_handler(void* frame) {
    (void)frame;

    if (!(inb(KB_STATUS) & 0x01)) return;   /* Sin dato */
    uint8_t data = inb(KB_DATA);

    switch (packet_idx) {
        case 0:
            /* Validar que el bit 3 (siempre 1) está activo → byte de flags válido */
            if (!(data & 0x08)) return;
            packet[0] = data;
            packet_idx = 1;
            break;

        case 1:
            packet[1] = data;
            packet_idx = 2;
            break;

        case 2: {
            packet[2] = data;
            packet_idx = 0;

            /* ── Parsear botones ─────────────────────────────────────── */
            mouse_state.btn_left   = (packet[0] & 0x01) != 0;
            mouse_state.btn_right  = (packet[0] & 0x02) != 0;
            mouse_state.btn_middle = (packet[0] & 0x04) != 0;

            /* ── Parsear movimiento (delta con signo de 9 bits) ──────── */
            int16_t dx = (int16_t)packet[1];
            int16_t dy = (int16_t)packet[2];

            /* Aplicar el bit de signo del byte de flags */
            if (packet[0] & 0x10) dx |= 0xFF00;   /* Bit 4: signo X */
            if (packet[0] & 0x20) dy |= 0xFF00;   /* Bit 5: signo Y */

            /* Ignorar overflow */
            if ((packet[0] & 0x40) || (packet[0] & 0x80)) break;

            mouse_state.dx = (int8_t)dx;
            mouse_state.dy = (int8_t)-dy;   /* Y está invertido */

            /* ── Actualizar posición absoluta con clipping ───────────── */
            int32_t nx = mouse_state.x + (int32_t)dx;
            int32_t ny = mouse_state.y + (int32_t)(-dy);

            if (nx < 0)                nx = 0;
            if (ny < 0)                ny = 0;
            if (nx >= (int32_t)VESA_WIDTH)  nx = (int32_t)VESA_WIDTH  - 1;
            if (ny >= (int32_t)VESA_HEIGHT) ny = (int32_t)VESA_HEIGHT - 1;

            if (nx != mouse_state.x || ny != mouse_state.y) {
                mouse_state.x    = nx;
                mouse_state.y    = ny;
                mouse_state.moved = true;
            }

            has_event = true;
            break;
        }
    }
}

/* =============================================================================
 * INICIALIZACIÓN
 * ============================================================================= */
void mouse_init(void) {
    /* Habilitar el puerto auxiliar (ratón) en el controlador PS/2 */
    mouse_wait_write();
    outb(KB_CMD, 0xA8);     /* Enable Auxiliary Device */

    /* Activar IRQ de ratón en el controlador */
    mouse_wait_write();
    outb(KB_CMD, 0x20);     /* Leer byte de comando */
    mouse_wait_read();
    uint8_t status = inb(KB_DATA) | 0x02;   /* Bit 1: enable mouse IRQ */
    status &= ~0x20;                         /* Bit 5: disable mouse clock = 0 → enable */
    mouse_wait_write();
    outb(KB_CMD, 0x60);     /* Escribir byte de comando */
    mouse_wait_write();
    outb(KB_DATA, status);

    /* Resetear el ratón y esperar ACK */
    mouse_write(0xFF);      /* Reset */
    mouse_read();           /* ACK (0xFA) */
    mouse_read();           /* Self-test passed (0xAA) */
    mouse_read();           /* ID del dispositivo (0x00) */

    /* Configurar resolución y frecuencia de muestreo */
    mouse_write(0xF6);      /* Set Defaults */
    mouse_read();           /* ACK */

    mouse_write(0xF3);      /* Set Sample Rate */
    mouse_read();           /* ACK */
    mouse_write(100);       /* 100 samples/second */
    mouse_read();           /* ACK */

    mouse_write(0xE8);      /* Set Resolution */
    mouse_read();           /* ACK */
    mouse_write(0x02);      /* 4 counts/mm */
    mouse_read();           /* ACK */

    /* Habilitar envío de datos (stream mode) */
    mouse_write(0xF4);      /* Enable Data Reporting */
    mouse_read();           /* ACK */

    /* Registrar handler en IRQ12 (vector 44 en IDT) */
    idt_register_handler(44, (isr_handler_t)mouse_irq_handler);
    pic_unmask_irq(12);

    /* Posición inicial: centro de la pantalla */
    mouse_state.x = VESA_WIDTH  / 2;
    mouse_state.y = VESA_HEIGHT / 2;

    kprintf("  [ OK ]  PS/2 Mouse (IRQ12, cursor at %dx%d)\n",
            mouse_state.x, mouse_state.y);
}

const mouse_state_t* mouse_get_state(void) { return &mouse_state; }
bool  mouse_has_event(void)  { return has_event; }
void  mouse_clear_event(void){ has_event = false; mouse_state.moved = false; }
