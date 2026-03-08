; =============================================================================
; KOSMO OS — ISR Stubs (Interrupt Service Routines)
; Archivo : kernel/arch/x86/isr.asm
; Función : Stubs ASM para las 48 interrupciones x86 (32 excepciones CPU +
;           16 IRQs hardware remapeadas). Cada stub:
;             1. Empuja un código de error (o 0 si la CPU no lo provee)
;             2. Empuja el número de interrupción
;             3. Salta al manejador común que llama al handler C
; =============================================================================

[BITS 32]

; Importar el dispatcher C de interrupciones
extern isr_dispatch
extern irq_dispatch

; =============================================================================
; MACRO: ISR sin código de error (la mayoría de excepciones)
; La CPU NO empuja código de error → empujamos 0 manualmente
; =============================================================================
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push    dword 0         ; Código de error ficticio
    push    dword %1        ; Número de interrupción
    jmp     isr_common_stub
%endmacro

; =============================================================================
; MACRO: ISR con código de error (excepciones 8,10,11,12,13,14,17,21)
; La CPU ya empuja el código de error automáticamente
; =============================================================================
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push    dword %1        ; Número de interrupción (error ya está en stack)
    jmp     isr_common_stub
%endmacro

; =============================================================================
; MACRO: IRQ hardware (0-15)
; Se remap an a vectores 32-47 para no colisionar con excepciones CPU
; =============================================================================
%macro IRQ_STUB 2
global irq%1
irq%1:
    cli
    push    dword 0         ; Sin código de error
    push    dword %2        ; Número de vector (32 + irq_number)
    jmp     irq_common_stub
%endmacro

; =============================================================================
; EXCEPCIONES DE LA CPU (vectores 0-31)
; Referencia: Intel Manual Vol.3A, Table 6-1
; =============================================================================

ISR_NOERR  0    ; Division By Zero
ISR_NOERR  1    ; Debug
ISR_NOERR  2    ; Non-Maskable Interrupt (NMI)
ISR_NOERR  3    ; Breakpoint (INT3)
ISR_NOERR  4    ; Overflow (INTO)
ISR_NOERR  5    ; Bound Range Exceeded
ISR_NOERR  6    ; Invalid Opcode (UD2)
ISR_NOERR  7    ; Device Not Available (FPU/SIMD)
ISR_ERR    8    ; Double Fault              ← tiene código de error
ISR_NOERR  9    ; Coprocessor Segment Overrun (obsoleto)
ISR_ERR   10    ; Invalid TSS               ← tiene código de error
ISR_ERR   11    ; Segment Not Present       ← tiene código de error
ISR_ERR   12    ; Stack Segment Fault       ← tiene código de error
ISR_ERR   13    ; General Protection Fault  ← tiene código de error
ISR_ERR   14    ; Page Fault                ← tiene código de error
ISR_NOERR 15    ; Reservada
ISR_NOERR 16    ; x87 FPU Error
ISR_ERR   17    ; Alignment Check           ← tiene código de error
ISR_NOERR 18    ; Machine Check
ISR_NOERR 19    ; SIMD Floating-Point Exception
ISR_NOERR 20    ; Virtualization Exception
ISR_ERR   21    ; Control Protection        ← tiene código de error
ISR_NOERR 22    ; Reservada
ISR_NOERR 23    ; Reservada
ISR_NOERR 24    ; Reservada
ISR_NOERR 25    ; Reservada
ISR_NOERR 26    ; Reservada
ISR_NOERR 27    ; Reservada
ISR_NOERR 28    ; Hypervisor Injection
ISR_NOERR 29    ; VMM Communication
ISR_ERR   30    ; Security Exception
ISR_NOERR 31    ; Reservada

; =============================================================================
; IRQs HARDWARE (vectores 32-47, remapeados desde 0x08/0x70)
; IRQ0  = PIT Timer          → vector 32
; IRQ1  = Teclado PS/2       → vector 33
; IRQ2  = PIC Cascade        → vector 34
; IRQ3  = COM2               → vector 35
; IRQ4  = COM1               → vector 36
; IRQ5  = LPT2 / Audio       → vector 37
; IRQ6  = Floppy             → vector 38
; IRQ7  = LPT1 / Spurious    → vector 39
; IRQ8  = CMOS RTC           → vector 40
; IRQ9  = Open               → vector 41
; IRQ10 = Open               → vector 42
; IRQ11 = Open               → vector 43
; IRQ12 = Ratón PS/2         → vector 44
; IRQ13 = FPU/Coprocessor    → vector 45
; IRQ14 = ATA Primario       → vector 46
; IRQ15 = ATA Secundario     → vector 47
; =============================================================================

IRQ_STUB  0, 32
IRQ_STUB  1, 33
IRQ_STUB  2, 34
IRQ_STUB  3, 35
IRQ_STUB  4, 36
IRQ_STUB  5, 37
IRQ_STUB  6, 38
IRQ_STUB  7, 39
IRQ_STUB  8, 40
IRQ_STUB  9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

; =============================================================================
; MANEJADOR COMÚN DE EXCEPCIONES
; En este punto el stack contiene (de abajo a arriba):
;   [SS, ESP_user] ← solo si cambio de privilegio
;   EFLAGS
;   CS
;   EIP            ← empujados automáticamente por el CPU
;   err_code       ← empujado por la CPU o por nuestro stub (0)
;   int_no         ← empujado por nuestro stub
;
; Empujamos todos los registros con PUSHAD, ajustamos segmentos y
; llamamos a isr_dispatch(interrupt_frame_t* frame) en C.
; =============================================================================
isr_common_stub:
    pusha                   ; Empujar EAX,ECX,EDX,EBX,ESP_orig,EBP,ESI,EDI

    ; Guardar y normalizar los segmentos de datos al segmento del kernel
    mov     ax, ds
    push    eax             ; Guardar DS
    mov     ax, 0x10        ; Selector de datos del kernel
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    ; Pasar puntero al frame de interrupción como argumento
    push    esp             ; ESP apunta al inicio del interrupt_frame_t

    call    isr_dispatch    ; Llamar al dispatcher C

    add     esp, 4          ; Limpiar argumento de la pila

    ; Restaurar segmentos
    pop     eax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    popa                    ; Restaurar registros generales
    add     esp, 8          ; Limpiar err_code + int_no del stack
    sti
    iret                    ; Retornar: restaura EIP, CS, EFLAGS (y ESP/SS si hubo cambio)

; =============================================================================
; MANEJADOR COMÚN DE IRQs
; Idéntico al de excepciones pero llama a irq_dispatch y envía EOI al PIC.
; =============================================================================
irq_common_stub:
    pusha

    mov     ax, ds
    push    eax
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    push    esp
    call    irq_dispatch
    add     esp, 4

    pop     eax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    popa
    add     esp, 8
    sti
    iret
