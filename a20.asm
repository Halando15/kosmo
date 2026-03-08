; =============================================================================
; KOSMO OS — Activación de la Línea A20
; Archivo : boot/stage2/a20.asm
; Función : Habilitar el bus de dirección A20 para acceder a memoria
;           por encima de 1MB. Prueba 3 métodos en orden de preferencia.
; Uso     : %include "a20.asm" dentro de stage2.asm
; =============================================================================

; -----------------------------------------------------------------------------
; enable_a20
; Intenta habilitar A20 usando 3 métodos:
;   1. BIOS INT 15h (más seguro)
;   2. Puerto del teclado 8042
;   3. Fast A20 (puerto 0x92)
; Retorna: nada (continúa si tiene éxito, cuelga si falla)
; -----------------------------------------------------------------------------
enable_a20:
    ; ── Método 1: BIOS INT 15h, AX=2401 ─────────────────────────────────────
    mov     ax, 0x2401
    int     0x15
    jc      .try_keyboard           ; Si error, intentar siguiente método
    test    ah, ah
    jnz     .try_keyboard
    call    a20_check
    jnz     .success
    ; Si BIOS dijo OK pero A20 no está activo, probar siguiente

.try_keyboard:
    ; ── Método 2: Controlador de teclado 8042 ────────────────────────────────
    call    a20_wait_input
    mov     al, 0xAD                ; Deshabilitar teclado
    out     0x64, al

    call    a20_wait_input
    mov     al, 0xD0                ; Leer byte de salida del controlador
    out     0x64, al

    call    a20_wait_output
    in      al, 0x60                ; Leer dato
    push    ax                      ; Guardar valor actual

    call    a20_wait_input
    mov     al, 0xD1                ; Preparar escritura al byte de salida
    out     0x64, al

    call    a20_wait_input
    pop     ax
    or      al, 0x02                ; Bit 1 = A20 enable
    out     0x60, al                ; Escribir con A20 activado

    call    a20_wait_input
    mov     al, 0xAE                ; Re-habilitar teclado
    out     0x64, al

    call    a20_wait_input

    ; Esperar y verificar
    mov     cx, 0xFFFF
.check_kbd:
    call    a20_check
    jnz     .success
    loop    .check_kbd

.try_fast:
    ; ── Método 3: Fast A20 (Puerto 0x92) ─────────────────────────────────────
    in      al, 0x92
    test    al, 0x02
    jnz     .fast_already           ; Ya está activo
    or      al, 0x02
    and     al, 0xFE                ; No resetear (bit 0 = reset)
    out     0x92, al

.fast_already:
    mov     cx, 0xFFFF
.check_fast:
    call    a20_check
    jnz     .success
    loop    .check_fast

    ; ── Todos los métodos fallaron ────────────────────────────────────────────
    mov     si, msg_a20_fail
    call    print_string_rm
    jmp     $                       ; Halt

.success:
    mov     si, msg_a20_ok
    call    print_string_rm
    ret

; -----------------------------------------------------------------------------
; a20_check — Verifica si A20 está activo
; Compara la posición 0x0000:0x0500 con 0xFFFF:0x0510
; (misma dirección física si A20 está desactivado)
; Retorna: ZF=0 si A20 está ACTIVO, ZF=1 si NO está activo
; -----------------------------------------------------------------------------
a20_check:
    pushf
    push    ds
    push    es
    push    di
    push    si

    cli

    xor     ax, ax                  ; AX = 0x0000
    mov     es, ax
    mov     di, 0x0500              ; ES:DI = 0x0000:0x0500

    mov     ax, 0xFFFF              ; AX = 0xFFFF
    mov     ds, ax
    mov     si, 0x0510              ; DS:SI = 0xFFFF:0x0510
                                    ; Dirección física = 0x100500 (con A20 activo)
                                    ;                  = 0x000500 (con A20 inactivo → misma que ES:DI)

    mov     al, byte [es:di]        ; Guardar valor original en 0x0500
    push    ax
    mov     al, byte [ds:si]        ; Guardar valor original en 0xFFFF:0x0510
    push    ax

    mov     byte [es:di], 0x00      ; Escribir 0x00 en 0x0500
    mov     byte [ds:si], 0xFF      ; Escribir 0xFF en 0xFFFF:0x0510

    cmp     byte [es:di], 0xFF      ; Si 0x0500 cambió a 0xFF → A20 inactivo
                                    ; (ambas direcciones son la misma físicamente)

    pop     ax
    mov     byte [ds:si], al        ; Restaurar valor en 0xFFFF:0x0510
    pop     ax
    mov     byte [es:di], al        ; Restaurar valor en 0x0500

    ; ZF=0 → A20 activo (los bytes son DISTINTOS → diferentes dir. físicas)
    ; ZF=1 → A20 inactivo

    pop     si
    pop     di
    pop     es
    pop     ds
    popf
    ret

; -----------------------------------------------------------------------------
; a20_wait_input — Espera a que el buffer de entrada del 8042 esté vacío
; -----------------------------------------------------------------------------
a20_wait_input:
    in      al, 0x64
    test    al, 0x02                ; Bit 1: input buffer lleno
    jnz     a20_wait_input
    ret

; -----------------------------------------------------------------------------
; a20_wait_output — Espera a que el buffer de salida del 8042 tenga datos
; -----------------------------------------------------------------------------
a20_wait_output:
    in      al, 0x64
    test    al, 0x01                ; Bit 0: output buffer con dato
    jz      a20_wait_output
    ret

msg_a20_ok:     db  "[A20] Enabled OK", 0x0D, 0x0A, 0
msg_a20_fail:   db  "[A20] FAILED! System halted.", 0x0D, 0x0A, 0
