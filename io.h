/* =============================================================================
 * KOSMO OS — Port I/O (E/S de Puertos Hardware)
 * Archivo : include/io.h
 * Función : Funciones inline para leer/escribir en puertos de E/S de x86.
 *           En x86, el hardware (teclado, timer, etc.) se comunica mediante
 *           el espacio de puertos I/O (0x0000 - 0xFFFF), accesible con
 *           las instrucciones IN y OUT del procesador.
 * ============================================================================= */

#ifndef KOSMO_IO_H
#define KOSMO_IO_H

#include "types.h"

/* =============================================================================
 * ESCRITURA EN PUERTOS (OUT)
 * ============================================================================= */

/**
 * outb — Escribir 1 byte en un puerto I/O
 * @port : número de puerto (0x0000 - 0xFFFF)
 * @value: byte a escribir
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile (
        "outb %0, %1"
        :                           /* sin outputs */
        : "a"(value), "Nd"(port)   /* inputs: AL=value, DX=port */
        : "memory"
    );
}

/**
 * outw — Escribir 2 bytes (word) en un puerto I/O
 * @port : número de puerto
 * @value: word a escribir
 */
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile (
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
        : "memory"
    );
}

/**
 * outl — Escribir 4 bytes (dword) en un puerto I/O
 * @port : número de puerto
 * @value: dword a escribir
 */
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile (
        "outl %0, %1"
        :
        : "a"(value), "Nd"(port)
        : "memory"
    );
}

/* =============================================================================
 * LECTURA DE PUERTOS (IN)
 * ============================================================================= */

/**
 * inb — Leer 1 byte de un puerto I/O
 * @port  : número de puerto
 * @return: byte leído
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile (
        "inb %1, %0"
        : "=a"(ret)                 /* output: ret = AL */
        : "Nd"(port)                /* input: DX = port */
        : "memory"
    );
    return ret;
}

/**
 * inw — Leer 2 bytes (word) de un puerto I/O
 * @port  : número de puerto
 * @return: word leído
 */
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile (
        "inw %1, %0"
        : "=a"(ret)
        : "Nd"(port)
        : "memory"
    );
    return ret;
}

/**
 * inl — Leer 4 bytes (dword) de un puerto I/O
 * @port  : número de puerto
 * @return: dword leído
 */
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile (
        "inl %1, %0"
        : "=a"(ret)
        : "Nd"(port)
        : "memory"
    );
    return ret;
}

/* =============================================================================
 * DELAY DE I/O
 * Escribe en el puerto 0x80 (POST diagnostic port) para generar
 * un pequeño delay. Necesario al programar el PIC, CMOS, etc.
 * ============================================================================= */

/**
 * io_wait — Genera un pequeño retardo de I/O (~1-4 microsegundos)
 * Usado después de escribir a hardware lento (PIC, teclado, etc.)
 */
static inline void io_wait(void) {
    outb(0x80, 0x00);   /* Puerto de diagnóstico POST, siempre ignorado */
}

/* =============================================================================
 * CONTROL DE INTERRUPCIONES
 * ============================================================================= */

/**
 * sti — Habilitar interrupciones (STI = Set Interrupt Flag)
 */
static inline void sti(void) {
    __asm__ volatile ("sti" ::: "memory");
}

/**
 * cli — Deshabilitar interrupciones (CLI = Clear Interrupt Flag)
 */
static inline void cli(void) {
    __asm__ volatile ("cli" ::: "memory");
}

/**
 * hlt — Detener el CPU hasta la próxima interrupción
 */
static inline void hlt(void) {
    __asm__ volatile ("hlt" ::: "memory");
}

/**
 * cpu_halt — Deshabilitar interrupciones y detener el CPU permanentemente
 * Usado en kernel panic.
 */
static inline void NORETURN cpu_halt(void) {
    cli();
    for (;;) {
        hlt();
    }
}

/**
 * irq_save — Guardar y deshabilitar interrupciones
 * @return: flags del registro EFLAGS antes del CLI
 */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile (
        "pushfl\n\t"
        "popl %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

/**
 * irq_restore — Restaurar flags de interrupciones guardados
 * @flags: valor guardado por irq_save()
 */
static inline void irq_restore(uint32_t flags) {
    __asm__ volatile (
        "pushl %0\n\t"
        "popfl"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

/* =============================================================================
 * PUERTOS COMUNES DEL HARDWARE x86
 * Referencia rápida para el resto del kernel
 * ============================================================================= */

/* PIC (Programmable Interrupt Controller) */
#define PIC1_CMD    0x20    /* PIC maestro: comando */
#define PIC1_DATA   0x21    /* PIC maestro: datos/máscara */
#define PIC2_CMD    0xA0    /* PIC esclavo: comando */
#define PIC2_DATA   0xA1    /* PIC esclavo: datos/máscara */

/* PIT (Programmable Interval Timer) */
#define PIT_CH0     0x40    /* Canal 0 (IRQ0 - timer del sistema) */
#define PIT_CH1     0x41    /* Canal 1 (RAM refresh, obsoleto) */
#define PIT_CH2     0x42    /* Canal 2 (speaker) */
#define PIT_CMD     0x43    /* Registro de control */

/* Teclado PS/2 */
#define KB_DATA     0x60    /* Buffer de datos del teclado */
#define KB_STATUS   0x64    /* Registro de estado del controlador */
#define KB_CMD      0x64    /* Registro de comandos del controlador */

/* CMOS / RTC */
#define CMOS_ADDR   0x70    /* Registro de dirección CMOS */
#define CMOS_DATA   0x71    /* Registro de datos CMOS */

/* Speaker del PC */
#define SPEAKER_PORT 0x61   /* Puerto del speaker */

/* Fast A20 */
#define FAST_A20    0x92    /* Puerto de Fast A20 */

#endif /* KOSMO_IO_H */
