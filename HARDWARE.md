# Kosmo OS — Guía de Hardware y Compatibilidad

Guía completa para arrancar Kosmo OS en hardware físico.

---

## Requisitos mínimos

| Componente | Mínimo | Recomendado |
|---|---|---|
| CPU | x86 32-bit (Pentium II+) | Core 2 Duo / Athlon 64 |
| RAM | 64 MB | 256 MB+ |
| Almacenamiento | USB de 32 MB | USB 2.0 de 1 GB+ |
| Pantalla | VGA 800×600 | Cualquier LCD/CRT |
| BIOS | BIOS legacy (no UEFI puro) | BIOS con CSM/Legacy mode |
| Teclado | PS/2 o USB (en modo PS/2) | PS/2 nativo |
| Ratón | PS/2 (para GUI) | PS/2 nativo |

---

## Configuración de la BIOS

### Ajustes imprescindibles

```
Secure Boot        → DISABLED
Fast Boot          → DISABLED
Boot Mode          → Legacy / CSM  (NO UEFI puro)
Boot Order         → USB primero, luego HDD
USB Legacy Support → ENABLED
```

### Ajustes opcionales pero recomendados

```
ACPI               → S3/S4 habilitado (para ahorrar energía)
VT-x / AMD-V       → No necesario (Kosmo es ring 0 en hardware)
IOMMU              → No necesario
Virtualization     → Puede dejarse habilitado sin problema
```

### Acceder a la BIOS

| Fabricante | Tecla |
|---|---|
| ASUS | F2 / Del |
| Lenovo ThinkPad | F1 / Enter → F1 |
| HP | F10 / Esc → F10 |
| Dell | F2 |
| Acer | F2 / Del |
| MSI | Del |
| Toshiba | F2 / F12 |
| Genérico | Del / F2 / F10 |

---

## Flashear a USB

### Windows — Rufus (recomendado)

1. Descarga [Rufus](https://rufus.ie) (portable, sin instalación)
2. Inserta el USB (se borrará todo)
3. Configura:
   ```
   Device          : Tu USB
   Boot selection  : Disk or ISO image → SELECT → kosmo-os.iso
   Partition scheme: MBR
   Target system   : BIOS (or UEFI-CSM)
   File system     : FAT32
   ```
4. Haz clic en **START** → OK a la advertencia
5. Espera ~30 segundos

### Windows — Ventoy

1. Descarga [Ventoy](https://ventoy.net) e instálalo en el USB
2. Copia `build/kosmo-os.iso` a la partición Ventoy
3. Al arrancar, selecciona **Kosmo OS** del menú Ventoy

### Linux / WSL2 — dd

```bash
# CUIDADO: Reemplaza /dev/sdX con tu dispositivo USB real
# Verifica con: lsblk
sudo dd if=build/kosmo-os.iso of=/dev/sdX bs=4M status=progress
sudo sync
```

---

## Problemas comunes y soluciones

### ❌ Pantalla negra tras el logo de la BIOS

**Causa**: GRUB no encontró el kernel, o el USB no arrancó.

**Soluciones**:
1. Verifica que Secure Boot está **desactivado**
2. Verifica que Boot Mode es **Legacy/CSM** (no UEFI puro)
3. Reflashea el USB con Rufus usando **MBR + BIOS**
4. Prueba otro puerto USB (preferiblemente USB 2.0, no 3.0)
5. En la selección de boot (F12), elige explícitamente el USB

---

### ❌ GRUB arranca pero el kernel da error Multiboot

**Causa**: El binario `kernel.bin` no tiene la firma Multiboot válida.

**Diagnóstico**:
```bash
# Verificar que la firma 0x1BADB002 está en los primeros 8 KB
xxd build/kosmo-os.iso | grep -i "b002\|02b0"
# O con readelf:
readelf -a build/kernel.elf | grep -i multiboot
```

**Solución**: Recompila con `make clean && make all`

---

### ❌ Pantalla en modo texto en lugar de GUI

**Causa**: GRUB no pudo configurar el modo VESA 800×600×32.

**Diagnóstico**:
```
GRUB → 'e' para editar la entrada → busca la línea 'set gfxmode'
Cambia por: set gfxmode=800x600x24 o set gfxmode=1024x768x32
```

**Solución**: Kosmo OS tiene fallback automático a la terminal de texto. Funciona correctamente. Para forzar GUI, prueba diferentes modos VESA en `grub.cfg`:
```
set gfxmode=1024x768x32,800x600x32,640x480x32,auto
```

---

### ❌ El teclado no responde en la GUI

**Causa A**: El teclado USB no está en modo de emulación PS/2.

**Solución**:
- Activa **USB Legacy Keyboard Support** en la BIOS
- O usa un teclado PS/2 nativo
- Algunos portátiles tienen el teclado interno conectado al controlador PS/2 de forma directa → debería funcionar

**Causa B**: El controlador PS/2 no envía IRQ1.

**Diagnóstico QEMU**:
```bash
make run-debug   # Terminal 1
make gdb         # Terminal 2
# En GDB:
watch-irq        # Traza todas las interrupciones
```

---

### ❌ El ratón no mueve el cursor

**Causa**: No hay ratón PS/2 conectado, o BIOS no lo inicializó.

**Situaciones**:
- En portátiles: el touchpad puede no ser PS/2 (puede ser I2C/SMBus en portátiles modernos)
- En desktops: conecta un ratón PS/2 (o USB con adaptador + USB Legacy Mouse en BIOS)
- El cursor sigue siendo visible pero no se mueve → el driver de ratón arrancó pero no hay movimiento

**Workaround**: Kosmo OS funciona completamente con solo el teclado en modo texto (`Alt+F4` no disponible, pero todas las funciones del shell sí).

---

### ❌ Triple fault / reinicio inmediato

**Causa más común**: Stack overflow o acceso a memoria no válido.

**Diagnóstico**:
```bash
make run-debug
# En GDB:
hook-panic        # Ya configurado en kosmo.gdb
# El GDB se detendrá automáticamente y mostrará el estado
```

**Causas frecuentes**:
- `kernel/linker.ld`: El stack está definido en `.bss`. Verifica que `_bss_end` no choca con otro segmento.
- Acceso a la dirección `0x0` (puntero nulo dereferenciado)
- GDT mal configurada (far jump a selector inválido)

---

### ❌ QEMU funciona pero el hardware físico no

**Diferencias clave QEMU vs hardware real**:

| Aspecto | QEMU | Hardware real |
|---|---|---|
| A20 | Siempre activa | Puede requerir activación manual |
| VESA | Emulada | Depende de la BIOS del hardware |
| PS/2 | Emulado perfectamente | Varía por controlador |
| PIT timing | Preciso | Puede variar (APIC, HPET) |
| Memoria | Siempre disponible | E820 puede tener huecos |

**Para hardware real**, añadir al `grub.cfg`:
```
set gfxmode=800x600x32,auto
```

---

## Portátiles probados (referencia)

| Modelo | Estado | Notas |
|---|---|---|
| ThinkPad T60/T61 | ✅ Funciona | Ideal, PS/2 nativo, BIOS legacy |
| ThinkPad X220 | ✅ Funciona | Activar CSM en BIOS |
| Dell Latitude D630 | ✅ Funciona | Boot order USB first |
| HP Compaq 6910p | ✅ Funciona | — |
| Asus EeePC 901 | ⚠️ Parcial | Sin ratón PS/2 |
| MacBook (Intel) | ⚠️ Parcial | Requiere rEFInd + CSM |
| Portátiles 2015+ | ⚠️ Variable | Depende del soporte CSM |
| UEFI puro (sin CSM) | ❌ No funciona | Requiere Fase 10 (UEFI) |

---

## Checklist pre-arranque en hardware

```
□  1. make clean && make all      (ISO limpia)
□  2. Rufus: MBR + BIOS/Legacy
□  3. BIOS: Secure Boot = OFF
□  4. BIOS: Boot Mode = Legacy/CSM
□  5. BIOS: USB Legacy Support = ON
□  6. Boot order: USB primero
□  7. Conectar USB ANTES de encender
□  8. Arrancar y pulsar F12 (o equivalente) para el menú de boot
□  9. Seleccionar el USB explícitamente
□ 10. Si hay pantalla negra: esperar 10s (GRUB tiene timeout=5)
```

---

## Recopilar información de diagnóstico

Si Kosmo OS no arranca en tu hardware, recopila esta información:

```
1. Modelo exacto del portátil/PC
2. Versión de la BIOS (mostrada en el POST)
3. Si GRUB aparece (sí/no)
4. Si el kernel arranca (sí/no, qué se ve en pantalla)
5. Qué error aparece (si hay alguno)
6. Resultado de: make run  (¿funciona en QEMU?)
```
