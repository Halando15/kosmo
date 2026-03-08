; =============================================================================
; KOSMO OS — Kernel Entry Point
; Archivo : kernel/arch/x86/entry.asm
; Función : Punto de entrada del kernel en modo protegido 32 bits.
;           - Define la cabecera Multiboot (para GRUB2)
;           - Configura la pila del kernel
;           - Inicializa el estado de la FPU
;           - Llama a kernel_main() en C
;           - Maneja el retorno (no debería ocurrir)
; Compilar: nasm -f elf32 entry.asm -o entry.o
; =============================================================================

[BITS 32]

; -----------------------------------------------------------------------------
; CABECERA MULTIBOOT
; GRUB busca esta firma en los primeros 8KB del kernel.
; Debe estar alineada a 4 bytes.
; -----------------------------------------------------------------------------

; Constantes Multiboot 1 (compatible con GRUB2)
MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_FLAGS     equ 0x00000003   ; Bit 0: alinear módulos a 4KB
                                     ; Bit 1: proveer mapa de memoria
                                     ; Bit 2: no usado aquí
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC              ; Firma mágica
    dd MULTIBOOT_FLAGS              ; Flags
    dd MULTIBOOT_CHECKSUM           ; Checksum (magic + flags + checksum = 0)

; -----------------------------------------------------------------------------
; STACK DEL KERNEL
; Reservamos 16KB para el stack inicial del kernel.
; Declarado en .bss (sin ocupar espacio en el binario).
; -----------------------------------------------------------------------------
section .bss
align 16
stack_bottom:
    resb 16384                      ; 16 KB de stack
stack_top:

; -----------------------------------------------------------------------------
; SECCIÓN DE DATOS DE ARRANQUE
; -----------------------------------------------------------------------------
section .data
align 4

; Guardamos la información que nos pasa GRUB (puntero a struct multiboot_info)
global multiboot_info_ptr
multiboot_info_ptr: dd 0

global multiboot_magic_val
multiboot_magic_val: dd 0

; -----------------------------------------------------------------------------
; SECCIÓN DE CÓDIGO
; -----------------------------------------------------------------------------
section .text

; Declarar kernel_main como externo (definido en kernel.c)
extern kernel_main

; Exportar el símbolo _start (entry point)
global _start
global kernel_entry

; =============================================================================
; _start — Entry point del kernel
; GRUB salta aquí con:
;   EAX = 0x2BADB002 (magia de confirmación Multiboot)
;   EBX = dirección física de la estructura multiboot_info
;   Modo protegido 32 bits activo
;   Interrupciones deshabilitadas
;   Segmentos configurados por GRUB
; =============================================================================
_start:
kernel_entry:
    ; ------------------------------------------------------------------
    ; Paso 1: Deshabilitar interrupciones (por si GRUB las dejó activas)
    ; ------------------------------------------------------------------
    cli

    ; ------------------------------------------------------------------
    ; Paso 2: Guardar información Multiboot antes de tocar el stack
    ; ------------------------------------------------------------------
    mov     [multiboot_magic_val], eax      ; Guardar magia (0x2BADB002)
    mov     [multiboot_info_ptr],  ebx      ; Guardar puntero a multiboot_info

    ; ------------------------------------------------------------------
    ; Paso 3: Configurar el stack del kernel
    ; ------------------------------------------------------------------
    mov     esp, stack_top                  ; ESP apunta al tope del stack
    xor     ebp, ebp                        ; EBP = 0 (base del stack frame)

    ; ------------------------------------------------------------------
    ; Paso 4: Limpiar registros de propósito general
    ; ------------------------------------------------------------------
    xor     eax, eax
    xor     ecx, ecx
    xor     edx, edx
    xor     esi, esi
    xor     edi, edi

    ; ------------------------------------------------------------------
    ; Paso 5: Inicializar FPU (x87 Floating Point Unit)
    ; ------------------------------------------------------------------
    fninit                                  ; Inicializar FPU sin excepciones

    ; ------------------------------------------------------------------
    ; Paso 6: Verificar la firma Multiboot
    ; Si no coincide, el kernel fue cargado por nuestro bootloader propio,
    ; lo cual también es válido.
    ; ------------------------------------------------------------------
    cmp     dword [multiboot_magic_val], 0x2BADB002
    je      .multiboot_ok
    ; No es Multiboot → kernel cargado por nuestro Stage 2
    ; Continuamos igual, kernel_main manejará esto.
.multiboot_ok:

    ; ------------------------------------------------------------------
    ; Paso 7: Llamar a kernel_main()
    ; Pasamos los argumentos en la pila (convención cdecl):
    ;   Argumento 1: magic value (EAX original)
    ;   Argumento 2: puntero a multiboot_info (EBX original)
    ; ------------------------------------------------------------------
    push    dword [multiboot_info_ptr]      ; Argumento 2: multiboot_info*
    push    dword [multiboot_magic_val]     ; Argumento 1: uint32_t magic

    call    kernel_main                     ; ¡Llamar al kernel en C!

    ; ------------------------------------------------------------------
    ; Paso 8: kernel_main() NO debería retornar jamás.
    ; Si lo hace, provocamos un halt explícito.
    ; ------------------------------------------------------------------
    add     esp, 8                          ; Limpiar argumentos del stack

.halt:
    cli                                     ; Deshabilitar interrupciones
    hlt                                     ; Detener el CPU
    jmp     .halt                           ; Si hay NMI, volver a halt

; =============================================================================
; MANEJADORES DE INTERRUPCIÓN STUB
; Estos son los stubs ASM que llaman a los handlers C de la IDT.
; Cada uno empuja el número de interrupción y llama al handler genérico.
; Se completan en FASE 3 junto con la IDT.
; =============================================================================

; Exportar función de recarga de segmentos (útil tras configurar GDT)
global gdt_flush
extern gdt_ptr                              ; Definido en gdt.c

gdt_flush:
    mov     eax, [esp + 4]                  ; Obtener argumento (puntero a GDT)
    lgdt    [eax]                           ; Cargar GDT

    ; Recargar registros de segmento con selector de datos (0x10)
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Recargar CS con un far return (no podemos hacer far jmp directo en 32-bit sin ASM)
    jmp     0x08:.reload_cs
.reload_cs:
    ret

; Exportar función de carga de IDT
global idt_load
idt_load:
    mov     eax, [esp + 4]                  ; Puntero al IDT descriptor
    lidt    [eax]                           ; Cargar IDT
    ret
