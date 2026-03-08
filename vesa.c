/* =============================================================================
 * KOSMO OS — Driver VESA (Implementación)
 * Archivo : drivers/video/vesa.c
 * ============================================================================= */

#include "vesa.h"
#include "font8x8.h"
#include "string.h"
#include "stdio.h"
#include "vga.h"

/* =============================================================================
 * ESTADO GLOBAL DEL FRAMEBUFFER
 * ============================================================================= */
uint32_t* vesa_fb       = NULL;
uint32_t  vesa_pitch_px = VESA_WIDTH;   /* Pitch en píxeles */

static uint32_t fb_width  = 0;
static uint32_t fb_height = 0;
static uint32_t fb_stride = 0;         /* Bytes por fila */
static bool     fb_ready  = false;

/* =============================================================================
 * INICIALIZACIÓN
 * ============================================================================= */

/**
 * vesa_init — Leer el framebuffer desde la info Multiboot de GRUB
 * GRUB configura el modo gráfico y rellena los campos VBE en multiboot_info_t.
 * Necesita que grub.cfg contenga: set gfxmode=800x600x32; set gfxpayload=keep
 */
bool vesa_init(multiboot_info_t* mbi) {
    if (!mbi) return false;

    /* Verificar que GRUB proporcionó info VBE (bit 11 del flags) */
    if (!(mbi->flags & MULTIBOOT_FLAG_VBE)) {
        vga_puts_color("  [WARN] No VBE info from GRUB. Using text mode.\n",
                       VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        return false;
    }

    /* Obtener puntero a ModeInfoBlock */
    vbe_mode_info_t* mi = (vbe_mode_info_t*)(uintptr_t)mbi->vbe.vbe_mode_info;
    if (!mi) return false;

    /* Verificar que es 32bpp y que tenemos framebuffer lineal */
    if (mi->bits_per_pixel != 32 || mi->phys_base_ptr == 0) {
        vga_puts_color("  [WARN] VBE mode not 32bpp or no linear FB.\n",
                       VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        return false;
    }

    /* Configurar el framebuffer */
    vesa_fb       = (uint32_t*)(uintptr_t)mi->phys_base_ptr;
    fb_width      = mi->x_resolution;
    fb_height     = mi->y_resolution;
    fb_stride     = mi->bytes_per_scan_line;
    vesa_pitch_px = fb_stride / VESA_BPP_BYTES;
    fb_ready      = true;

    kprintf("  [ OK ]  VESA Framebuffer %ux%u @ 0x%08X (%u KB)\n",
            fb_width, fb_height, mi->phys_base_ptr,
            (fb_stride * fb_height) / 1024);
    return true;
}

bool     vesa_is_available(void) { return fb_ready; }
uint32_t vesa_width(void)        { return fb_width;  }
uint32_t vesa_height(void)       { return fb_height; }
uint32_t vesa_stride(void)       { return fb_stride; }
uint32_t* vesa_framebuffer(void) { return vesa_fb;   }

/* =============================================================================
 * PRIMITIVAS DE DIBUJO
 * ============================================================================= */

void vesa_clear(uint32_t color) {
    if (!fb_ready) return;
    uint32_t total = vesa_pitch_px * fb_height;
    for (uint32_t i = 0; i < total; i++) vesa_fb[i] = color;
}

void vesa_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!fb_ready) return;
    /* Clip */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_width)  w = (int)fb_width  - x;
    if (y + h > (int)fb_height) h = (int)fb_height - y;
    if (w <= 0 || h <= 0) return;

    for (int row = y; row < y + h; row++) {
        uint32_t* dst = vesa_fb + (uint32_t)row * vesa_pitch_px + (uint32_t)x;
        for (int col = 0; col < w; col++) dst[col] = color;
    }
}

void vesa_draw_hline(int x, int y, int w, uint32_t color) {
    vesa_fill_rect(x, y, w, 1, color);
}

void vesa_draw_vline(int x, int y, int h, uint32_t color) {
    vesa_fill_rect(x, y, 1, h, color);
}

void vesa_draw_rect(int x, int y, int w, int h, uint32_t color, int t) {
    vesa_fill_rect(x,         y,         w, t, color);   /* Top */
    vesa_fill_rect(x,         y+h-t,     w, t, color);   /* Bottom */
    vesa_fill_rect(x,         y,         t, h, color);   /* Left */
    vesa_fill_rect(x+w-t,     y,         t, h, color);   /* Right */
}

/* Algoritmo de Bresenham */
void vesa_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    if (!fb_ready) return;
    int dx = x1 - x0, sx = dx < 0 ? -1 : 1;
    int dy = y1 - y0, sy = dy < 0 ? -1 : 1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = (dx > dy ? dx : -dy) / 2;

    while (true) {
        vesa_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

/* Algoritmo de Midpoint para círculo */
void vesa_draw_circle(int cx, int cy, int r, uint32_t color) {
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        vesa_put_pixel(cx+x, cy+y, color); vesa_put_pixel(cx-x, cy+y, color);
        vesa_put_pixel(cx+x, cy-y, color); vesa_put_pixel(cx-x, cy-y, color);
        vesa_put_pixel(cx+y, cy+x, color); vesa_put_pixel(cx-y, cy+x, color);
        vesa_put_pixel(cx+y, cy-x, color); vesa_put_pixel(cx-y, cy-x, color);
        y++;
        if (d <= 0) d += 2*y + 1;
        else { x--; d += 2*(y-x) + 1; }
    }
}

void vesa_fill_circle(int cx, int cy, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)__builtin_sqrt((double)(r*r - dy*dy));  /* NOLINT */
        vesa_fill_rect(cx - dx, cy + dy, 2*dx + 1, 1, color);
    }
}

/**
 * vesa_fill_gradient_v — Gradiente vertical lineal entre dos colores
 */
void vesa_fill_gradient_v(int x, int y, int w, int h,
                           uint32_t c_top, uint32_t c_bot) {
    if (!fb_ready || h <= 0) return;

    uint8_t r0 = (uint8_t)((c_top >> 16) & 0xFF);
    uint8_t g0 = (uint8_t)((c_top >>  8) & 0xFF);
    uint8_t b0 = (uint8_t)( c_top        & 0xFF);
    uint8_t r1 = (uint8_t)((c_bot >> 16) & 0xFF);
    uint8_t g1 = (uint8_t)((c_bot >>  8) & 0xFF);
    uint8_t b1 = (uint8_t)( c_bot        & 0xFF);

    for (int row = 0; row < h; row++) {
        uint32_t t = (uint32_t)(row * 255) / (uint32_t)h;
        uint8_t r = (uint8_t)(r0 + ((int)(r1-r0) * (int)t) / 255);
        uint8_t g = (uint8_t)(g0 + ((int)(g1-g0) * (int)t) / 255);
        uint8_t b = (uint8_t)(b0 + ((int)(b1-b0) * (int)t) / 255);
        vesa_fill_rect(x, y + row, w, 1, RGB(r, g, b));
    }
}

/* =============================================================================
 * TEXTO GRÁFICO — usa fuente 8x8 de font8x8.c
 * ============================================================================= */

void vesa_draw_char(int x, int y, char c,
                    uint32_t fg, uint32_t bg, bool transp) {
    if (!fb_ready) return;
    const uint8_t* glyph = font8x8_get_glyph((uint8_t)c);
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                vesa_put_pixel(x + col, y + row, fg);
            } else if (!transp) {
                vesa_put_pixel(x + col, y + row, bg);
            }
        }
    }
}

void vesa_draw_string(int x, int y, const char* s,
                      uint32_t fg, uint32_t bg, bool transp) {
    if (!s) return;
    int ox = x;
    while (*s) {
        if (*s == '\n') { y += 10; x = ox; s++; continue; }
        vesa_draw_char(x, y, *s, fg, bg, transp);
        x += 8;
        s++;
    }
}

void vesa_draw_string_scaled(int x, int y, const char* s,
                              uint32_t fg, uint32_t bg, bool transp, int sc) {
    if (!s || sc <= 1) { vesa_draw_string(x, y, s, fg, bg, transp); return; }
    int ox = x;
    while (*s) {
        if (*s == '\n') { y += 10 * sc; x = ox; s++; continue; }
        const uint8_t* glyph = font8x8_get_glyph((uint8_t)*s);
        for (int row = 0; row < 8; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                uint32_t px = (bits & (0x80 >> col)) ? fg : bg;
                if ((bits & (0x80 >> col)) || !transp)
                    vesa_fill_rect(x + col*sc, y + row*sc, sc, sc, px);
            }
        }
        x += 8 * sc;
        s++;
    }
}

int vesa_text_width(const char* s) {
    int len = 0;
    while (s && *s++) len++;
    return len * 8;
}

int vesa_text_height(void) { return 8; }

/* =============================================================================
 * BLIT Y SCROLL
 * ============================================================================= */

void vesa_blit(int dx, int dy, const uint32_t* src, int sw, int sh) {
    for (int row = 0; row < sh; row++) {
        if (dy + row < 0 || dy + row >= (int)fb_height) continue;
        for (int col = 0; col < sw; col++) {
            if (dx + col < 0 || dx + col >= (int)fb_width) continue;
            uint32_t px = src[row * sw + col];
            if (px != 0xFF000000)   /* 0xFF000000 = transparente */
                vesa_fb[(uint32_t)(dy+row) * vesa_pitch_px + (uint32_t)(dx+col)] = px;
        }
    }
}

void vesa_scroll_region(int x, int y, int w, int h, int dy_scroll) {
    if (dy_scroll == 0) return;
    if (dy_scroll > 0) {
        for (int row = y; row < y + h - dy_scroll; row++) {
            uint32_t* dst = vesa_fb + (uint32_t)row * vesa_pitch_px + (uint32_t)x;
            uint32_t* src = vesa_fb + (uint32_t)(row + dy_scroll) * vesa_pitch_px + (uint32_t)x;
            for (int col = 0; col < w; col++) dst[col] = src[col];
        }
    }
}
