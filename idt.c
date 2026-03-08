/* =============================================================================
 * KOSMO OS — IDT + PIC (Implementación)
 * Archivo : kernel/arch/x86/idt.c
 * Función : Configura la IDT con los 48 handlers (32 excepciones + 16 IRQs)
 *           y remapea el PIC 8259 para que las IRQs hardware no colisionen
 *           con las excepciones del CPU.
 * ============================================================================= */

#include "idt.h"
#include "gdt.h"
#include "io.h"
#include "types.h"
#include "string.h"
#include "stdio.h"

/* =============================================================================
 * ESTADO INTERNO
 * ============================================================================= */

/* La tabla IDT: 256 entradas de 8 bytes */
static idt_entry_t idt_table[IDT_ENTRIES];

/* Puntero para LIDT */
static idt_ptr_t idt_pointer;

/* Tabla de handlers C registrados (NULL = no registrado) */
static isr_handler_t isr_handlers[IDT_ENTRIES];

/* Nombres de las excepciones CPU (para mensajes de error) */
static const char* exception_names[] = {
    "Division By Zero",         /* 0  */
    "Debug",                    /* 1  */
    "Non-Maskable Interrupt",   /* 2  */
    "Breakpoint",               /* 3  */
    "Overflow",                 /* 4  */
    "Bound Range Exceeded",     /* 5  */
    "Invalid Opcode",           /* 6  */
    "Device Not Available",     /* 7  */
    "Double Fault",             /* 8  */
    "Coprocessor Seg Overrun",  /* 9  */
    "Invalid TSS",              /* 10 */
    "Segment Not Present",      /* 11 */
    "Stack Segment Fault",      /* 12 */
    "General Protection Fault", /* 13 */
    "Page Fault",               /* 14 */
    "Reserved",                 /* 15 */
    "x87 FPU Error",            /* 16 */
    "Alignment Check",          /* 17 */
    "Machine Check",            /* 18 */
    "SIMD Floating-Point",      /* 19 */
    "Virtualization",           /* 20 */
    "Control Protection",       /* 21 */
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved",
    "Hypervisor Injection",     /* 28 */
    "VMM Communication",        /* 29 */
    "Security Exception",       /* 30 */
    "Reserved"                  /* 31 */
};

/* =============================================================================
 * PIC 8259 — INICIALIZACIÓN Y REMAPEO
 * Por defecto, el PIC mapea IRQ0-IRQ7  a vectores 0x08-0x0F
 *                         y IRQ8-IRQ15 a vectores 0x70-0x77
 * Esto colisiona con las excepciones del CPU (0x00-0x1F).
 * Remapeamos a: IRQ0-IRQ7 → 0x20-0x27, IRQ8-IRQ15 → 0x28-0x2F
 * ============================================================================= */

#define PIC_EOI             0x20    /* End-Of-Interrupt */
#define PIC_ICW1_ICW4       0x01    /* ICW4 será enviado */
#define PIC_ICW1_INIT       0x10    /* Comando de inicialización */
#define PIC_ICW4_8086       0x01    /* Modo 8086/88 */

void pic_init(void) {
    /* Guardar máscaras actuales */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ── Inicialización en cascada (ICW1) ──────────────────────────────── */
    outb(PIC1_CMD,  PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD,  PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    /* ── Vector base (ICW2) ─────────────────────────────────────────────── */
    outb(PIC1_DATA, 0x20);  /* PIC maestro: IRQ0-7 → vectores 32-39 */
    io_wait();
    outb(PIC2_DATA, 0x28);  /* PIC esclavo: IRQ8-15 → vectores 40-47 */
    io_wait();

    /* ── Cascada (ICW3) ──────────────────────────────────────────────────── */
    outb(PIC1_DATA, 0x04);  /* Maestro: esclavo conectado a IRQ2 (bit 2) */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* Esclavo: número de línea cascada = 2 */
    io_wait();

    /* ── Modo 8086 (ICW4) ────────────────────────────────────────────────── */
    outb(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC2_DATA, PIC_ICW4_8086);
    io_wait();

    /* ── Restaurar máscaras ───────────────────────────────────────────────── */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/**
 * pic_send_eoi — Enviar End-Of-Interrupt al PIC
 * Debe llamarse al final de cada handler de IRQ para que el PIC
 * vuelva a aceptar interrupciones de la misma o menor prioridad.
 */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);    /* EOI al PIC esclavo */
    }
    outb(PIC1_CMD, PIC_EOI);        /* EOI al PIC maestro (siempre) */
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t val;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) | (uint8_t)(1 << irq);
    outb(port, val);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t val;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) & (uint8_t)~(1 << irq);
    outb(port, val);
}

/* =============================================================================
 * IDT — CONFIGURACIÓN
 * ============================================================================= */

/**
 * idt_set_gate — Instalar un handler en la IDT
 */
void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector,
                  uint8_t type_attr) {
    idt_table[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt_table[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt_table[vector].selector    = selector;
    idt_table[vector].zero        = 0;
    idt_table[vector].type_attr   = type_attr;
}

/**
 * idt_register_handler — Registrar un handler C para un vector
 */
void idt_register_handler(uint8_t vector, isr_handler_t handler) {
    isr_handlers[vector] = handler;
}

/**
 * idt_init — Inicializar la IDT completa
 */
void idt_init(void) {
    /* Limpiar tabla de handlers y entradas IDT */
    memset(idt_table,    0, sizeof(idt_table));
    memset(isr_handlers, 0, sizeof(isr_handlers));

    /* ── Instalar las 32 excepciones del CPU ─────────────────────────────── */
    uint8_t gate = IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_GATE_INT32;
    uint16_t sel = GDT_SEL_KCODE;

    idt_set_gate(0,  (uint32_t)isr0,  sel, gate);
    idt_set_gate(1,  (uint32_t)isr1,  sel, gate);
    idt_set_gate(2,  (uint32_t)isr2,  sel, gate);
    idt_set_gate(3,  (uint32_t)isr3,  sel, gate);
    idt_set_gate(4,  (uint32_t)isr4,  sel, gate);
    idt_set_gate(5,  (uint32_t)isr5,  sel, gate);
    idt_set_gate(6,  (uint32_t)isr6,  sel, gate);
    idt_set_gate(7,  (uint32_t)isr7,  sel, gate);
    idt_set_gate(8,  (uint32_t)isr8,  sel, gate);
    idt_set_gate(9,  (uint32_t)isr9,  sel, gate);
    idt_set_gate(10, (uint32_t)isr10, sel, gate);
    idt_set_gate(11, (uint32_t)isr11, sel, gate);
    idt_set_gate(12, (uint32_t)isr12, sel, gate);
    idt_set_gate(13, (uint32_t)isr13, sel, gate);
    idt_set_gate(14, (uint32_t)isr14, sel, gate);
    idt_set_gate(15, (uint32_t)isr15, sel, gate);
    idt_set_gate(16, (uint32_t)isr16, sel, gate);
    idt_set_gate(17, (uint32_t)isr17, sel, gate);
    idt_set_gate(18, (uint32_t)isr18, sel, gate);
    idt_set_gate(19, (uint32_t)isr19, sel, gate);
    idt_set_gate(20, (uint32_t)isr20, sel, gate);
    idt_set_gate(21, (uint32_t)isr21, sel, gate);
    idt_set_gate(22, (uint32_t)isr22, sel, gate);
    idt_set_gate(23, (uint32_t)isr23, sel, gate);
    idt_set_gate(24, (uint32_t)isr24, sel, gate);
    idt_set_gate(25, (uint32_t)isr25, sel, gate);
    idt_set_gate(26, (uint32_t)isr26, sel, gate);
    idt_set_gate(27, (uint32_t)isr27, sel, gate);
    idt_set_gate(28, (uint32_t)isr28, sel, gate);
    idt_set_gate(29, (uint32_t)isr29, sel, gate);
    idt_set_gate(30, (uint32_t)isr30, sel, gate);
    idt_set_gate(31, (uint32_t)isr31, sel, gate);

    /* ── Instalar los 16 IRQs hardware (vectores 32-47) ─────────────────── */
    idt_set_gate(32, (uint32_t)irq0,  sel, gate);
    idt_set_gate(33, (uint32_t)irq1,  sel, gate);
    idt_set_gate(34, (uint32_t)irq2,  sel, gate);
    idt_set_gate(35, (uint32_t)irq3,  sel, gate);
    idt_set_gate(36, (uint32_t)irq4,  sel, gate);
    idt_set_gate(37, (uint32_t)irq5,  sel, gate);
    idt_set_gate(38, (uint32_t)irq6,  sel, gate);
    idt_set_gate(39, (uint32_t)irq7,  sel, gate);
    idt_set_gate(40, (uint32_t)irq8,  sel, gate);
    idt_set_gate(41, (uint32_t)irq9,  sel, gate);
    idt_set_gate(42, (uint32_t)irq10, sel, gate);
    idt_set_gate(43, (uint32_t)irq11, sel, gate);
    idt_set_gate(44, (uint32_t)irq12, sel, gate);
    idt_set_gate(45, (uint32_t)irq13, sel, gate);
    idt_set_gate(46, (uint32_t)irq14, sel, gate);
    idt_set_gate(47, (uint32_t)irq15, sel, gate);

    /* ── Cargar la IDT ───────────────────────────────────────────────────── */
    idt_pointer.limit = (uint16_t)(sizeof(idt_table) - 1);
    idt_pointer.base  = (uint32_t)(uintptr_t)idt_table;
    idt_load(&idt_pointer);
}

/* =============================================================================
 * DISPATCHERS — llamados desde isr.asm / irq.asm
 * ============================================================================= */

/**
 * isr_dispatch — Manejador genérico de excepciones CPU
 * Si hay un handler registrado lo llama; si no, muestra la excepción.
 */
void isr_dispatch(interrupt_frame_t* frame) {
    uint32_t vec = frame->int_no;

    if (isr_handlers[vec]) {
        isr_handlers[vec](frame);
    } else {
        /* Excepción no manejada → mostrar info y kernel panic */
        kprintf("\n[EXCEPTION #%u] ", vec);
        if (vec < 32) {
            kprintf("%s", exception_names[vec]);
        }
        kprintf("\n  EIP=0x%08X  CS=0x%04X  EFLAGS=0x%08X\n",
                frame->eip, frame->cs, frame->eflags);
        kprintf("  EAX=0x%08X  EBX=0x%08X  ECX=0x%08X  EDX=0x%08X\n",
                frame->eax, frame->ebx, frame->ecx, frame->edx);
        if (vec == 14) {
            /* Page Fault: leer CR2 con el address que causó el fallo */
            uint32_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            kprintf("  Page Fault Address: 0x%08X\n", cr2);
            kprintf("  Error code: 0x%08X\n", frame->err_code);
        }

        /* Para excepciones fatales (Double Fault, GPF, etc.): halt */
        cli();
        for (;;) hlt();
    }
}

/**
 * irq_dispatch — Manejador genérico de IRQs hardware
 * Llama al handler registrado y envía EOI al PIC.
 */
void irq_dispatch(interrupt_frame_t* frame) {
    uint8_t irq_num = (uint8_t)(frame->int_no - 32);

    /* Llamar al handler si está registrado */
    if (isr_handlers[frame->int_no]) {
        isr_handlers[frame->int_no](frame);
    }

    /* Enviar End-Of-Interrupt al PIC */
    pic_send_eoi(irq_num);
}
