/* =============================================================================
 * KOSMO OS — PIT (Programmable Interval Timer)
 * Archivo : drivers/timer/pit.h
 * Función : Driver del temporizador hardware 8253/8254.
 *           Genera IRQ0 a una frecuencia programable (default: 100 Hz).
 *           Provee ticks del sistema, sleep(), uptime y medición de tiempo.
 * ============================================================================= */

#ifndef KOSMO_PIT_H
#define KOSMO_PIT_H

#include "types.h"

/* =============================================================================
 * CONSTANTES DEL PIT
 * ============================================================================= */

/* Frecuencia base del oscilador del PIT: 1.193182 MHz */
#define PIT_BASE_FREQ       1193182UL

/* Frecuencia deseada del timer del sistema (ticks por segundo) */
#define PIT_TICK_RATE       100         /* 100 Hz = 10ms por tick */

/* Divisor para PIT_TICK_RATE */
#define PIT_DIVISOR         (PIT_BASE_FREQ / PIT_TICK_RATE)

/* Puertos del PIT */
#define PIT_CH0_DATA        0x40        /* Canal 0 (IRQ0) */
#define PIT_CH1_DATA        0x41        /* Canal 1 (obsoleto) */
#define PIT_CH2_DATA        0x42        /* Canal 2 (Speaker) */
#define PIT_CMD_PORT        0x43        /* Registro de comando */

/* Byte de comando del PIT:
 * Bits 7-6: Canal (00=CH0, 01=CH1, 10=CH2, 11=read-back)
 * Bits 5-4: Modo acceso (00=latch, 01=LSB, 10=MSB, 11=LSB+MSB)
 * Bits 3-1: Modo operación (010 = rate generator)
 * Bit 0:    BCD/Binario (0 = binario de 16 bits) */
#define PIT_CMD_CH0_RATE    0x36        /* Canal 0, LSB+MSB, rate generator, binario */
#define PIT_CMD_CH2_ONESHOT 0xB2        /* Canal 2, LSB+MSB, one-shot, binario */

/* =============================================================================
 * API DEL DRIVER PIT
 * ============================================================================= */

/* Inicializar el PIT a PIT_TICK_RATE Hz */
void pit_init(void);

/* Obtener el número de ticks desde el arranque */
uint64_t pit_get_ticks(void);

/* Obtener segundos desde el arranque */
uint32_t pit_get_uptime(void);

/* Obtener milisegundos desde el arranque */
uint64_t pit_get_millis(void);

/* Esperar N milisegundos (bloquea el CPU con interrupciones habilitadas) */
void sleep_ms(uint32_t ms);

/* Esperar N segundos */
void sleep_sec(uint32_t sec);

/* Esperar N ticks del sistema */
void sleep_ticks(uint32_t ticks);

/* Handler IRQ0 del PIT (llamado desde la IDT) */
void pit_irq_handler(void* frame);

#endif /* KOSMO_PIT_H */
