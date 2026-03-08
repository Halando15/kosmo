; =============================================================================
; KOSMO OS — Stage 2 Bootloader
; Archivo : boot/stage2/stage2.asm
; Función : Ejecutado tras Stage 1. Realiza las tareas que no caben en 512 bytes:
;           1. Muestra splash en modo real
;           2. Activa la línea A20
;           3. Obtiene mapa de memoria E820
;           4. Carga el kernel desde disco (LBA) a 0x100000
;           5. Configura GDT temporal
;           6. Entra en modo protegido 32 bits
;           7. Salta a kernel_entry (entry.asm del kernel)
; Cargado : En 0x8000 (segmento 0x0800, offset 0x0000)
; Compilar: nasm -f bin stage2.asm -o stage2.bin
; =============================================================================

[BITS 16]
[ORG 0x8000]

; -----------------------------------------------------------------------------
; CONSTANTES
; -----------------------------------------------------------------------------
KERNEL_LBA      equ 18              ; Sector LBA donde empieza el kernel
KERNEL_SECTORS  equ 200             ; Sectores del kernel (200 * 512 = 100KB)
KERNEL_LOAD_SEG equ 0x1000          ; Segmento temporal para cargar (0x10000)
KERNEL_LOAD_OFF equ 0x0000
KERNEL_FINAL    equ 0x00100000      ; Dirección física final del kernel (1MB)

MMAP_DEST       equ 0x5000          ; Donde guardamos el mapa de memoria E820

; =============================================================================
; PUNTO DE ENTRADA DE STAGE 2
; =============================================================================
stage2_start:
    ; Asegurar segmentos correctos
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x7C00
    sti

    mov     [boot_drive_s2], dl     ; Guardar boot drive que nos pasó Stage 1

    ; ------------------------------------------------------------------
    ; Splash de bienvenida en modo texto BIOS
    ; ------------------------------------------------------------------
    call    clear_screen_rm
    mov     si, msg_splash1
    call    print_string_rm
    mov     si, msg_splash2
    call    print_string_rm
    mov     si, msg_splash3
    call    print_string_rm
    mov     si, msg_blank
    call    print_string_rm

    ; ------------------------------------------------------------------
    ; Paso 1: Activar A20
    ; ------------------------------------------------------------------
    mov     si, msg_step_a20
    call    print_string_rm
    call    enable_a20              ; Definido en a20.asm (incluido abajo)

    ; ------------------------------------------------------------------
    ; Paso 2: Obtener mapa de memoria (INT 15h E820)
    ; ------------------------------------------------------------------
    mov     si, msg_step_mmap
    call    print_string_rm
    call    get_memory_map

    ; ------------------------------------------------------------------
    ; Paso 3: Cargar el kernel en memoria baja temporal (0x10000)
    ; El kernel luego se moverá a 0x100000 en modo protegido si hace falta,
    ; pero con GRUB esto lo hace GRUB. Para el bootloader propio:
    ; Cargamos directamente desde 0x10000 usando INT 13h extendido
    ; ------------------------------------------------------------------
    mov     si, msg_step_kernel
    call    print_string_rm
    call    load_kernel

    ; ------------------------------------------------------------------
    ; Paso 4: Configurar GDT y entrar en modo protegido
    ; ------------------------------------------------------------------
    mov     si, msg_step_pm
    call    print_string_rm
    call    enter_protected_mode    ; ¡No retorna! Salta al kernel.

    ; Si llegamos aquí hay un error
    jmp     $

; =============================================================================
; SUBRUTINA: clear_screen_rm
; Limpia la pantalla en modo real usando BIOS
; =============================================================================
clear_screen_rm:
    mov     ah, 0x00
    mov     al, 0x03                ; Modo 3: texto 80x25 color
    int     0x10
    ret

; =============================================================================
; SUBRUTINA: print_string_rm
; Entrada: SI = puntero al string (terminado en 0)
; =============================================================================
print_string_rm:
    pusha
    mov     ah, 0x0E
    mov     bh, 0x00
    mov     bl, 0x0F
.loop:
    lodsb
    test    al, al
    jz      .done
    int     0x10
    jmp     .loop
.done:
    popa
    ret

; =============================================================================
; SUBRUTINA: print_string_color
; Entrada: SI = puntero al string, BL = color atributo
; =============================================================================
print_string_color:
    pusha
    mov     ah, 0x0E
    mov     bh, 0x00
.loop:
    lodsb
    test    al, al
    jz      .done
    int     0x10
    jmp     .loop
.done:
    popa
    ret

; =============================================================================
; SUBRUTINA: get_memory_map
; Usa INT 15h, EAX=0xE820 para obtener el mapa de memoria
; Guarda las entradas en MMAP_DEST con formato:
;   [count: WORD] [entry0: 24 bytes] [entry1: 24 bytes] ...
; =============================================================================
get_memory_map:
    pusha
    mov     di, MMAP_DEST + 2       ; Primer entrada tras el contador
    xor     ebx, ebx                ; EBX = 0 (continuación = 0 = primera entrada)
    xor     bp, bp                  ; BP = contador de entradas

.loop:
    mov     eax, 0xE820             ; Función E820
    mov     edx, 0x534D4150         ; Firma: "SMAP"
    mov     ecx, 24                 ; Tamaño de entrada (24 bytes con ACPI 3.0)
    int     0x15
    jc      .done                   ; Carry = error o fin

    cmp     eax, 0x534D4150         ; Verificar firma de retorno
    jne     .done

    test    ecx, ecx
    jz      .skip

    inc     bp                      ; Incrementar contador
    add     di, 24                  ; Avanzar puntero

.skip:
    test    ebx, ebx                ; EBX=0 → última entrada
    jz      .done
    jmp     .loop

.done:
    mov     [MMAP_DEST], bp         ; Guardar número de entradas
    popa
    ret

; =============================================================================
; SUBRUTINA: load_kernel
; Carga el kernel desde disco a la dirección 0x10000 (KERNEL_LOAD_SEG:0)
; Usa INT 13h extendido (LBA)
; =============================================================================
load_kernel:
    pusha

    ; Configurar DAP para cargar el kernel
    mov     word [kernel_dap + 2],  KERNEL_SECTORS
    mov     word [kernel_dap + 4],  KERNEL_LOAD_OFF
    mov     word [kernel_dap + 6],  KERNEL_LOAD_SEG
    mov     dword [kernel_dap + 8], KERNEL_LBA
    mov     dword [kernel_dap + 12], 0

    mov     si, kernel_dap
    mov     ah, 0x42
    mov     dl, [boot_drive_s2]
    int     0x13
    jc      kernel_load_error

    mov     si, msg_kernel_ok
    call    print_string_rm

    popa
    ret

kernel_load_error:
    mov     si, msg_kernel_err
    call    print_string_rm
    jmp     $

; =============================================================================
; SUBRUTINA: enter_protected_mode
; 1. Deshabilita interrupciones
; 2. Carga GDTR
; 3. Activa bit PE en CR0
; 4. Far jump a selector de código 32 bits
; =============================================================================
enter_protected_mode:
    cli                             ; ¡Sin interrupciones en modo protegido sin IDT!

    ; Cargar GDT
    lgdt    [gdt_descriptor]

    ; Activar modo protegido: bit 0 de CR0
    mov     eax, cr0
    or      eax, 0x00000001
    mov     cr0, eax

    ; Far jump para limpiar la pipeline y cargar CS con selector 0x08
    jmp     0x08:pm_entry           ; 0x08 = selector del segmento de código (GDT entrada 1)

; =============================================================================
; CÓDIGO EN 32 BITS — entra aquí tras activar modo protegido
; =============================================================================
[BITS 32]
pm_entry:
    ; Cargar selectores de segmento de datos (GDT entrada 2 = 0x10)
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Configurar stack del kernel en 0x90000
    mov     esp, 0x90000

    ; Pasar información al kernel vía registros:
    ;   EBX = dirección del mapa de memoria
    ;   EDX = número de entradas del mapa
    ;   ESI = dirección donde está el kernel cargado (0x10000)
    movzx   ebx, word [MMAP_DEST]   ; Número de entradas E820
    mov     ecx, MMAP_DEST          ; Dirección del mapa
    mov     esi, (KERNEL_LOAD_SEG * 16) ; = 0x10000

    ; Saltar al entry point del kernel
    ; El kernel fue cargado en KERNEL_LOAD_SEG:0 = 0x10000
    ; entry.asm del kernel está al inicio del binario
    jmp     (KERNEL_LOAD_SEG * 16)  ; jmp 0x10000

    ; ¡No retorna!

; =============================================================================
; GDT — Global Descriptor Table temporal (plana, 4GB)
; Usamos una GDT simple de 3 entradas:
;   0x00 = Null descriptor (obligatorio)
;   0x08 = Code segment (ring 0, 32-bit, base 0, limit 4GB)
;   0x10 = Data segment (ring 0, 32-bit, base 0, limit 4GB)
; =============================================================================
[BITS 16]

gdt_start:

; Entrada 0: Null descriptor (8 bytes de cero, obligatorio)
gdt_null:
    dq 0x0000000000000000

; Entrada 1: Segmento de código (offset 0x08)
; Base=0, Limit=0xFFFFF, G=1(4KB), D/B=1(32bit), P=1, DPL=0, S=1, Type=0xA(Execute/Read)
gdt_code:
    dw 0xFFFF           ; Limit bits 0-15
    dw 0x0000           ; Base bits 0-15
    db 0x00             ; Base bits 16-23
    db 10011010b        ; Access: P=1, DPL=00, S=1, Type=1010 (code, exec/read)
    db 11001111b        ; Flags + Limit 16-19: G=1, D/B=1, L=0, AVL=0, Limit=0xF
    db 0x00             ; Base bits 24-31

; Entrada 2: Segmento de datos (offset 0x10)
; Base=0, Limit=0xFFFFF, G=1(4KB), D/B=1(32bit), P=1, DPL=0, S=1, Type=0x2(Read/Write)
gdt_data:
    dw 0xFFFF           ; Limit bits 0-15
    dw 0x0000           ; Base bits 0-15
    db 0x00             ; Base bits 16-23
    db 10010010b        ; Access: P=1, DPL=00, S=1, Type=0010 (data, read/write)
    db 11001111b        ; Flags + Limit 16-19
    db 0x00             ; Base bits 24-31

gdt_end:

; Descriptor de la GDT (para LGDT)
gdt_descriptor:
    dw gdt_end - gdt_start - 1     ; Tamaño de la GDT - 1
    dd gdt_start                    ; Dirección lineal de la GDT

; =============================================================================
; DAP del kernel para INT 13h extendido
; =============================================================================
kernel_dap:
    db 0x10             ; Tamaño del paquete
    db 0x00             ; Reservado
    dw 0x0000           ; Sectores (se rellena en load_kernel)
    dw 0x0000           ; Offset destino (se rellena en load_kernel)
    dw 0x0000           ; Segmento destino (se rellena en load_kernel)
    dq 0x0000           ; LBA inicio (se rellena en load_kernel)

; =============================================================================
; DATOS
; =============================================================================
boot_drive_s2:  db 0

msg_splash1:    db "  =========================================", 0x0D, 0x0A, 0
msg_splash2:    db "         KOSMO OS v0.1 - Booting...", 0x0D, 0x0A, 0
msg_splash3:    db "  =========================================", 0x0D, 0x0A, 0
msg_blank:      db 0x0D, 0x0A, 0
msg_step_a20:   db "[1/4] Enabling A20 line...", 0x0D, 0x0A, 0
msg_step_mmap:  db "[2/4] Reading memory map (E820)...", 0x0D, 0x0A, 0
msg_step_kernel:db "[3/4] Loading kernel from disk...", 0x0D, 0x0A, 0
msg_step_pm:    db "[4/4] Entering Protected Mode...", 0x0D, 0x0A, 0
msg_kernel_ok:  db "  -> Kernel loaded OK.", 0x0D, 0x0A, 0
msg_kernel_err: db "  -> KERNEL LOAD ERROR! Halted.", 0x0D, 0x0A, 0

; =============================================================================
; INCLUSIÓN de rutinas A20
; =============================================================================
%include "a20.asm"
