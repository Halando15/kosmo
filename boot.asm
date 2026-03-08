; =============================================================================
; KOSMO OS — Stage 1 Bootloader (MBR)
; Archivo : boot/stage1/boot.asm
; Función : Cargado por BIOS en 0x7C00. Inicializa segmentos,
;           carga Stage 2 desde el disco y le cede el control.
; Tamaño  : Exactamente 512 bytes (MBR)
; Compilar: nasm -f bin boot.asm -o boot.bin
; =============================================================================

[BITS 16]
[ORG 0x7C00]

; -----------------------------------------------------------------------------
; CONSTANTES
; -----------------------------------------------------------------------------
STAGE2_SEGMENT  equ 0x0800          ; Stage 2 se carga en 0x0800:0000 = 0x8000
STAGE2_OFFSET   equ 0x0000
STAGE2_SECTORS  equ 16              ; Cuántos sectores ocupa Stage 2
STAGE2_START    equ 2               ; Sector LBA donde empieza Stage 2 (sector 2)

; =============================================================================
; PUNTO DE ENTRADA
; =============================================================================
_start:
    ; ------------------------------------------------------------------
    ; Paso 1: Normalizar segmentos y pila
    ; BIOS puede saltar a 0x07C0:0000 o a 0x0000:7C00.
    ; Forzamos un far jump a 0x0000:real_start para asegurar CS=0x0000
    ; ------------------------------------------------------------------
    jmp     0x0000:real_start

real_start:
    cli                             ; Deshabilitar interrupciones mientras configuramos
    xor     ax, ax
    mov     ds, ax                  ; DS = 0
    mov     es, ax                  ; ES = 0
    mov     ss, ax                  ; SS = 0
    mov     sp, 0x7C00              ; Stack crece hacia abajo desde 0x7C00
    sti                             ; Habilitar interrupciones

    ; ------------------------------------------------------------------
    ; Paso 2: Guardar número de unidad de boot (DL viene de BIOS)
    ; ------------------------------------------------------------------
    mov     [boot_drive], dl        ; Guardar drive (0x80 = HDD, 0x00 = Floppy)

    ; ------------------------------------------------------------------
    ; Paso 3: Mostrar mensaje de arranque
    ; ------------------------------------------------------------------
    mov     si, msg_booting
    call    print_string

    ; ------------------------------------------------------------------
    ; Paso 4: Verificar si el BIOS soporta INT 13h extendidas (LBA)
    ; ------------------------------------------------------------------
    mov     ah, 0x41                ; Función: Verificar extensiones
    mov     bx, 0x55AA
    mov     dl, [boot_drive]
    int     0x13
    jc      .use_chs                ; Si carry=1, no hay soporte LBA → usar CHS
    cmp     bx, 0xAA55
    jne     .use_chs

    ; ------------------------------------------------------------------
    ; Paso 5a: Cargar Stage 2 usando LBA extendido (INT 13h, AH=0x42)
    ; ------------------------------------------------------------------
    mov     si, msg_lba
    call    print_string

    mov     si, disk_address_packet
    mov     ah, 0x42
    mov     dl, [boot_drive]
    int     0x13
    jc      disk_error

    jmp     .load_ok

    ; ------------------------------------------------------------------
    ; Paso 5b: Fallback — Cargar Stage 2 usando CHS (INT 13h, AH=0x02)
    ; ------------------------------------------------------------------
.use_chs:
    mov     si, msg_chs
    call    print_string

    mov     ax, STAGE2_SEGMENT
    mov     es, ax                  ; ES = segmento destino
    xor     bx, bx                  ; BX = 0 (offset)

    mov     ah, 0x02                ; Función: Leer sectores
    mov     al, STAGE2_SECTORS      ; Número de sectores a leer
    mov     ch, 0                   ; Cilindro 0
    mov     cl, STAGE2_START        ; Sector 2 (1-based en CHS)
    mov     dh, 0                   ; Cabeza 0
    mov     dl, [boot_drive]
    int     0x13
    jc      disk_error

.load_ok:
    ; ------------------------------------------------------------------
    ; Paso 6: Restaurar ES=0, pasar boot_drive a Stage 2 y saltar
    ; ------------------------------------------------------------------
    xor     ax, ax
    mov     es, ax
    mov     si, msg_ok
    call    print_string

    mov     dl, [boot_drive]        ; DL = boot drive para Stage 2
    jmp     STAGE2_SEGMENT:STAGE2_OFFSET   ; Far jump a Stage 2

; =============================================================================
; MANEJADOR DE ERROR DE DISCO
; =============================================================================
disk_error:
    mov     si, msg_disk_err
    call    print_string
    jmp     $                       ; Halt — bucle infinito

; =============================================================================
; SUBRUTINA: print_string
; Entrada: SI = puntero a string terminado en 0
; Usa BIOS INT 10h, AH=0x0E (TTY output)
; =============================================================================
print_string:
    pusha
    mov     ah, 0x0E
    mov     bh, 0x00                ; Página de video 0
    mov     bl, 0x0F                ; Color blanco sobre negro
.loop:
    lodsb                           ; AL = [SI], SI++
    test    al, al                  ; ¿Fin de string?
    jz      .done
    int     0x10
    jmp     .loop
.done:
    popa
    ret

; =============================================================================
; DATOS
; =============================================================================

; DAP (Disk Address Packet) para INT 13h extendido
disk_address_packet:
    db  0x10                        ; Tamaño del paquete (16 bytes)
    db  0x00                        ; Reservado
    dw  STAGE2_SECTORS              ; Número de sectores a leer
    dw  STAGE2_OFFSET               ; Offset destino
    dw  STAGE2_SEGMENT              ; Segmento destino
    dq  STAGE2_START                ; LBA de inicio (64 bits)

boot_drive:     db  0               ; Número de unidad de arranque

msg_booting:    db  "Kosmo OS Booting...", 0x0D, 0x0A, 0
msg_lba:        db  "[LBA] Loading stage2...", 0x0D, 0x0A, 0
msg_chs:        db  "[CHS] Loading stage2...", 0x0D, 0x0A, 0
msg_ok:         db  "Stage2 OK. Jumping...", 0x0D, 0x0A, 0
msg_disk_err:   db  "DISK ERROR! System halted.", 0x0D, 0x0A, 0

; =============================================================================
; PADDING Y FIRMA DE ARRANQUE
; El MBR debe tener exactamente 512 bytes.
; Los últimos 2 bytes deben ser 0x55, 0xAA (firma de arranque).
; =============================================================================
times 510 - ($ - $$) db 0          ; Relleno con ceros hasta el byte 510
dw 0xAA55                           ; Firma de arranque (little-endian)
