/* =============================================================================
 * KOSMO OS — Driver VESA/VBE Framebuffer
 * Archivo : drivers/video/vesa.h
 * Función : Acceso al framebuffer lineal VESA VBE 2.0.
 *           Operaciones de píxel, rectángulos, líneas y texto gráfico.
 *           GRUB configura el modo gráfico y nos pasa la dirección física
 *           del framebuffer en la estructura Multiboot (VBE info).
 * ============================================================================= */

#ifndef KOSMO_VESA_H
#define KOSMO_VESA_H

#include "types.h"
#include "multiboot.h"

/* =============================================================================
 * CONSTANTES DE PANTALLA
 * ============================================================================= */
#define VESA_WIDTH      800
#define VESA_HEIGHT     600
#define VESA_BPP        32          /* Bits por píxel */
#define VESA_BPP_BYTES  4           /* Bytes por píxel */

/* Stride = bytes por fila (puede ser mayor que width * bpp_bytes) */
#define VESA_STRIDE     (VESA_WIDTH * VESA_BPP_BYTES)

/* =============================================================================
 * COLORES EN FORMATO 0xAARRGGBB
 * ============================================================================= */
#define RGB(r,g,b)      ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))
#define RGBA(r,g,b,a)   ((uint32_t)(((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))

/* Paleta de colores Kosmo OS */
#define COLOR_BLACK         RGB(0,   0,   0  )
#define COLOR_WHITE         RGB(255, 255, 255)
#define COLOR_DARK_GREY     RGB(64,  64,  64 )
#define COLOR_GREY          RGB(128, 128, 128)
#define COLOR_LIGHT_GREY    RGB(192, 192, 192)
#define COLOR_SILVER        RGB(220, 220, 220)

/* Colores del sistema (tema oscuro Kosmo) */
#define COLOR_DESKTOP_TOP       RGB(15,  30,  60 )   /* Azul muy oscuro */
#define COLOR_DESKTOP_BOTTOM    RGB(5,   10,  25 )   /* Casi negro */
#define COLOR_TASKBAR_BG        RGB(20,  20,  35 )   /* Fondo barra tareas */
#define COLOR_TASKBAR_BORDER    RGB(60,  80,  140)   /* Borde azul */
#define COLOR_WIN_TITLE         RGB(30,  60,  120)   /* Título ventana */
#define COLOR_WIN_TITLE_ACTIVE  RGB(0,   90,  180)   /* Título activo */
#define COLOR_WIN_BG            RGB(240, 240, 245)   /* Interior ventana */
#define COLOR_WIN_BORDER        RGB(100, 120, 180)   /* Borde ventana */
#define COLOR_WIN_CLOSE         RGB(200, 50,  50 )   /* Botón cerrar */
#define COLOR_WIN_CLOSE_HOVER   RGB(255, 80,  80 )
#define COLOR_BUTTON_BG         RGB(70,  100, 160)
#define COLOR_BUTTON_HOVER      RGB(90,  130, 200)
#define COLOR_TEXT_DARK         RGB(20,  20,  40 )
#define COLOR_TEXT_LIGHT        RGB(230, 235, 255)
#define COLOR_ACCENT            RGB(0,   140, 255)
#define COLOR_ACCENT2           RGB(0,   200, 140)
#define COLOR_HIGHLIGHT         RGB(255, 200, 0  )

/* =============================================================================
 * ESTRUCTURA VBE ModeInfoBlock (subset usado por el kernel)
 * Offset 0x28 = PhysBasePtr (dirección física del framebuffer lineal)
 * ============================================================================= */
typedef struct PACKED {
    uint16_t mode_attributes;       /* 0x00 */
    uint8_t  win_a_attributes;      /* 0x02 */
    uint8_t  win_b_attributes;      /* 0x03 */
    uint16_t win_granularity;       /* 0x04 */
    uint16_t win_size;              /* 0x06 */
    uint16_t win_a_segment;         /* 0x08 */
    uint16_t win_b_segment;         /* 0x0A */
    uint32_t win_func_ptr;          /* 0x0C */
    uint16_t bytes_per_scan_line;   /* 0x10 */
    uint16_t x_resolution;          /* 0x12 */
    uint16_t y_resolution;          /* 0x14 */
    uint8_t  x_char_size;           /* 0x16 */
    uint8_t  y_char_size;           /* 0x17 */
    uint8_t  number_of_planes;      /* 0x18 */
    uint8_t  bits_per_pixel;        /* 0x19 */
    uint8_t  number_of_banks;       /* 0x1A */
    uint8_t  memory_model;          /* 0x1B */
    uint8_t  bank_size;             /* 0x1C */
    uint8_t  number_of_image_pages; /* 0x1D */
    uint8_t  reserved0;             /* 0x1E */
    uint8_t  red_mask_size;         /* 0x1F */
    uint8_t  red_field_position;    /* 0x20 */
    uint8_t  green_mask_size;       /* 0x21 */
    uint8_t  green_field_position;  /* 0x22 */
    uint8_t  blue_mask_size;        /* 0x23 */
    uint8_t  blue_field_position;   /* 0x24 */
    uint8_t  rsvd_mask_size;        /* 0x25 */
    uint8_t  rsvd_field_position;   /* 0x26 */
    uint8_t  direct_color_mode;     /* 0x27 */
    uint32_t phys_base_ptr;         /* 0x28 ← ¡DIRECCIÓN DEL FRAMEBUFFER! */
    uint32_t offscreen_mem_offset;  /* 0x2C */
    uint16_t offscreen_mem_size;    /* 0x30 */
} vbe_mode_info_t;

/* =============================================================================
 * API DEL DRIVER VESA
 * ============================================================================= */

/* Inicialización — lee info de GRUB y configura el framebuffer */
bool vesa_init(multiboot_info_t* mbi);
bool vesa_is_available(void);

/* Información del modo actual */
uint32_t vesa_width(void);
uint32_t vesa_height(void);
uint32_t vesa_stride(void);         /* Bytes por fila */
uint32_t* vesa_framebuffer(void);   /* Puntero al inicio del framebuffer */

/* ── Primitivas de dibujo ──────────────────────────────────────────────────── */

/* Píxel individual */
static inline void vesa_put_pixel(int x, int y, uint32_t color);

/* Rectángulos */
void vesa_fill_rect(int x, int y, int w, int h, uint32_t color);
void vesa_draw_rect(int x, int y, int w, int h, uint32_t color, int thickness);

/* Líneas */
void vesa_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void vesa_draw_hline(int x, int y, int w, uint32_t color);
void vesa_draw_vline(int x, int y, int h, uint32_t color);

/* Círculo / Elipse */
void vesa_draw_circle(int cx, int cy, int r, uint32_t color);
void vesa_fill_circle(int cx, int cy, int r, uint32_t color);

/* Gradiente vertical */
void vesa_fill_gradient_v(int x, int y, int w, int h,
                           uint32_t color_top, uint32_t color_bottom);

/* Limpiar pantalla */
void vesa_clear(uint32_t color);

/* ── Texto gráfico ─────────────────────────────────────────────────────────── */
void vesa_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg_or_transparent,
                    bool transparent_bg);
void vesa_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg,
                      bool transparent_bg);
void vesa_draw_string_scaled(int x, int y, const char* str, uint32_t fg, uint32_t bg,
                              bool transparent, int scale);

/* Dimensiones de texto */
int  vesa_text_width(const char* str);     /* Ancho en píxeles */
int  vesa_text_height(void);               /* Altura siempre 8 (fuente 8x8) */

/* ── Copia de regiones ─────────────────────────────────────────────────────── */
void vesa_blit(int dst_x, int dst_y, const uint32_t* src,
               int src_w, int src_h);
void vesa_scroll_region(int x, int y, int w, int h, int dy);

/* Píxel inline para máxima velocidad */
static inline void vesa_put_pixel(int x, int y, uint32_t color) {
    extern uint32_t* vesa_fb;
    extern uint32_t  vesa_pitch_px;
    if ((uint32_t)x < VESA_WIDTH && (uint32_t)y < VESA_HEIGHT)
        vesa_fb[(uint32_t)y * vesa_pitch_px + (uint32_t)x] = color;
}

/* Variables externas (definidas en vesa.c) */
extern uint32_t* vesa_fb;
extern uint32_t  vesa_pitch_px;

#endif /* KOSMO_VESA_H */
