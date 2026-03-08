/* =============================================================================
 * KOSMO OS — PIT Driver (Implementación)
 * Archivo : drivers/timer/pit.c
 * Función : Configura el canal 0 del PIT 8253/8254 a 100 Hz.
 *           Mantiene el contador global de ticks y provee funciones de espera.
 *           El handler se registra en IRQ0 de la IDT.
 * ============================================================================= */

#include "pit.h"
#include "idt.h"
#include "io.h"
#include "stdio.h"

/* =============================================================================
 * ESTADO INTERNO
 * ============================================================================= */

/* Contador global de ticks (incrementado en cada IRQ0) */
static volatile uint64_t system_ticks = 0;

/* =============================================================================
 * HANDLER DE IRQ0
 * ============================================================================= */

/**
 * pit_irq_handler — Llamado por la IDT en cada tick del timer (100 Hz)
 * Se ejecuta 100 veces por segundo. Incrementa el contador de ticks.
 * En fases futuras: también llamará al scheduler de procesos.
 */
void pit_irq_handler(void* frame) {
    (void)frame;            /* Parámetro no usado por ahora */
    system_ticks++;

    /* Futuro: llamar al scheduler aquí */
    /* if (system_ticks % SCHEDULER_QUANTUM == 0) scheduler_tick(); */
}

/* =============================================================================
 * INICIALIZACIÓN
 * ============================================================================= */

/**
 * pit_init — Inicializar el PIT a PIT_TICK_RATE Hz (100 Hz por defecto)
 *
 * El PIT tiene un oscilador de 1.193182 MHz.
 * Para obtener 100 Hz: divisor = 1193182 / 100 = 11931
 * Cada 11931 pulsos del oscilador → 1 tick → 1 IRQ0 → 10ms
 */
void pit_init(void) {
    uint16_t divisor = (uint16_t)(PIT_DIVISOR & 0xFFFF);

    /* Enviar comando: canal 0, modo rate generator, LSB+MSB, binario */
    outb(PIT_CMD_PORT, PIT_CMD_CH0_RATE);
    io_wait();

    /* Enviar divisor en dos partes (LSB primero, luego MSB) */
    outb(PIT_CH0_DATA, (uint8_t)(divisor & 0xFF));          /* Byte bajo */
    io_wait();
    outb(PIT_CH0_DATA, (uint8_t)((divisor >> 8) & 0xFF));   /* Byte alto */
    io_wait();

    /* Registrar el handler en IRQ0 (vector 32 en la IDT) */
    idt_register_handler(32, (isr_handler_t)pit_irq_handler);

    /* Desenmascarar IRQ0 en el PIC */
    pic_unmask_irq(0);

    kprintf("  [ OK ]  PIT Timer (%u Hz, %u ms/tick)\n",
            PIT_TICK_RATE, 1000 / PIT_TICK_RATE);
}

/* =============================================================================
 * FUNCIONES DE TIEMPO
 * ============================================================================= */

uint64_t pit_get_ticks(void) {
    return system_ticks;
}

uint32_t pit_get_uptime(void) {
    return (uint32_t)(system_ticks / PIT_TICK_RATE);
}

uint64_t pit_get_millis(void) {
    return system_ticks * (1000 / PIT_TICK_RATE);
}

/* =============================================================================
 * FUNCIONES DE ESPERA (BUSY-WAIT CON HLT)
 * ============================================================================= */

/**
 * sleep_ticks — Esperar N ticks del sistema
 * Usa HLT para no quemar CPU mientras espera.
 * REQUIERE que las interrupciones estén habilitadas (STI).
 */
void sleep_ticks(uint32_t ticks) {
    uint64_t target = system_ticks + ticks;
    while (system_ticks < target) {
        __asm__ volatile("hlt");    /* Pausar hasta la próxima IRQ */
    }
}

void sleep_ms(uint32_t ms) {
    /* Convertir ms a ticks: ticks = ms / (1000/TICK_RATE) = ms * TICK_RATE / 1000 */
    uint32_t ticks = (ms * PIT_TICK_RATE) / 1000;
    if (ticks == 0) ticks = 1;     /* Mínimo 1 tick */
    sleep_ticks(ticks);
}

void sleep_sec(uint32_t sec) {
    sleep_ticks(sec * PIT_TICK_RATE);
}
