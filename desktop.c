/* =============================================================================
 * KOSMO OS — Escritorio (Implementación)
 * Archivo : gui/desktop.c
 * ============================================================================= */

#include "desktop.h"
#include "wm.h"
#include "vesa.h"
#include "kosmofs.h"
#include "pit.h"
#include "stdio.h"
#include "string.h"
#include "kernel.h"
#include "types.h"

/* ============================================================================
 * UTILIDADES DE WIDGETS LIGEROS
 * ============================================================================ */

/* Botón simple */
static void draw_button(int x, int y, int w, int h,
                        const char* label, bool hovered) {
    uint32_t bg = hovered ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG;
    vesa_fill_gradient_v(x, y, w, h, bg,
                          hovered ? COLOR_ACCENT : RGB(40,70,130));
    vesa_draw_rect(x, y, w, h, COLOR_ACCENT, 1);
    int tx = x + (w - vesa_text_width(label)) / 2;
    int ty = y + (h - 8) / 2;
    vesa_draw_string(tx, ty, label, COLOR_WHITE, 0, true);
}

/* Separador horizontal */
static void draw_separator(int x, int y, int w) {
    vesa_draw_hline(x, y,   w, RGB(180,185,200));
    vesa_draw_hline(x, y+1, w, COLOR_WHITE);
}

/* Etiqueta con icono cuadrado de color */
static void draw_icon_label(int x, int y, uint32_t icon_color,
                             const char* label, uint32_t text_color) {
    vesa_fill_rect(x, y, 12, 12, icon_color);
    vesa_draw_rect(x, y, 12, 12, RGB(0,0,0), 1);
    vesa_draw_string(x + 16, y + 2, label, text_color, 0, true);
}

/* Barra de progreso */
static void draw_progress_bar(int x, int y, int w, int h,
                               uint32_t pct,   /* 0-100 */
                               uint32_t fg, uint32_t bg) {
    vesa_fill_rect(x, y, w, h, bg);
    vesa_draw_rect(x, y, w, h, RGB(100,110,130), 1);
    int filled = (int)((uint32_t)w * pct / 100);
    if (filled > 2) {
        vesa_fill_gradient_v(x+1, y+1, filled-1, h-2, fg,
                              RGB(0, 160, 80));
    }
}

/* ============================================================================
 * VENTANA: ABOUT
 * ============================================================================ */
void paint_about_window(uint8_t id, int cx, int cy, int cw, int ch) {
    (void)id; (void)cw; (void)ch;

    int y = cy + 12;

    /* Logo pequeño en azul */
    vesa_draw_string_scaled(cx + 14, y, "K", COLOR_ACCENT, 0, true, 3);
    vesa_draw_string_scaled(cx + 38, y, "O", COLOR_ACCENT2, 0, true, 3);
    vesa_draw_string_scaled(cx + 62, y, "S", COLOR_ACCENT, 0, true, 3);
    vesa_draw_string_scaled(cx + 86, y, "M", COLOR_ACCENT2, 0, true, 3);
    vesa_draw_string_scaled(cx +110, y, "O", COLOR_ACCENT, 0, true, 3);
    y += 32;

    vesa_draw_string(cx + 14, y, "Operating System", COLOR_GREY, 0, true);
    y += 14;

    draw_separator(cx + 8, y, cw - 16);
    y += 10;

    struct { const char* k; const char* v; } info[] = {
        { "Version",  KOSMO_VERSION_STRING " (Genesis)"  },
        { "Arch",     "x86 32-bit Protected Mode"         },
        { "Boot",     "GRUB2 + Multiboot"                 },
        { "Kernel",   "Monolithic, flat memory model"     },
        { "FS",       "KosmoFS v1 (RAM disk, 128 KB)"     },
        { "Display",  "VESA VBE 2.0  800x600x32"          },
        { "Input",    "PS/2 Keyboard + Mouse (IRQ1/12)"   },
    };

    for (uint32_t i = 0; i < sizeof(info)/sizeof(info[0]); i++) {
        vesa_draw_string(cx+14, y, info[i].k, COLOR_GREY, 0, true);
        vesa_draw_string(cx+80, y, info[i].v, COLOR_TEXT_DARK, 0, true);
        y += 13;
    }

    draw_separator(cx + 8, y, cw - 16);
    y += 10;

    vesa_draw_string(cx+14, y, "\"Fast, light, and real.\"",
                     COLOR_ACCENT, 0, true);
}

/* ============================================================================
 * VENTANA: EXPLORADOR DE ARCHIVOS
 * ============================================================================ */
void paint_files_window(uint8_t id, int cx, int cy, int cw, int ch) {
    (void)id; (void)ch;

    /* Barra de dirección */
    vesa_fill_rect(cx, cy, cw, 20, RGB(225,228,235));
    vesa_draw_hline(cx, cy+20, cw, RGB(180,185,200));
    vesa_draw_string(cx+6, cy+6, "  /  (root)", COLOR_TEXT_DARK, 0, true);

    int y = cy + 28;

    /* Cabecera de columnas */
    vesa_fill_rect(cx, y, cw, 16, RGB(210,215,228));
    vesa_draw_string(cx+8,     y+4, "Name",     RGB(60,60,80), 0, true);
    vesa_draw_string(cx+180,   y+4, "Type",     RGB(60,60,80), 0, true);
    vesa_draw_string(cx+240,   y+4, "Size",     RGB(60,60,80), 0, true);
    draw_separator(cx, y+16, cw);
    y += 18;

    /* Listar "/" */
    kfs_dirent_t entries[32];
    int count = kfs_listdir("/", entries, 32);

    for (int i = 0; i < count && y < cy + ch - 20; i++) {
        /* Fondo alternado */
        uint32_t row_bg = (i % 2 == 0) ? COLOR_WIN_BG : RGB(232,234,242);
        vesa_fill_rect(cx, y, cw, 16, row_bg);

        if (entries[i].type == KFS_TYPE_DIR) {
            /* Icono carpeta */
            vesa_fill_rect(cx+6, y+2, 12, 10, RGB(255,200,60));
            vesa_fill_rect(cx+6, y+4, 6,  3,  RGB(255,220,100));
            vesa_draw_rect(cx+6, y+2, 12, 10, RGB(180,140,0), 1);

            char name_slash[64];
            ksprintf(name_slash, "%s/", entries[i].name);
            vesa_draw_string(cx+22, y+4, name_slash, COLOR_TEXT_DARK, 0, true);
            vesa_draw_string(cx+180,y+4, "DIR",  COLOR_GREY, 0, true);
            vesa_draw_string(cx+240,y+4, "-",    COLOR_GREY, 0, true);
        } else {
            /* Icono archivo */
            vesa_fill_rect(cx+6, y+2, 10, 12, COLOR_WHITE);
            vesa_draw_rect(cx+6, y+2, 10, 12, COLOR_GREY, 1);
            vesa_draw_hline(cx+7, y+6,  8, RGB(180,180,200));
            vesa_draw_hline(cx+7, y+8,  8, RGB(180,180,200));
            vesa_draw_hline(cx+7, y+10, 6, RGB(180,180,200));

            vesa_draw_string(cx+22, y+4, entries[i].name,
                             COLOR_TEXT_DARK, 0, true);
            vesa_draw_string(cx+180, y+4, "FILE", COLOR_GREY, 0, true);

            char szbuf[12];
            if (entries[i].size < 1024)
                ksprintf(szbuf, "%u B", entries[i].size);
            else
                ksprintf(szbuf, "%u KB", entries[i].size/1024);
            vesa_draw_string(cx+240, y+4, szbuf, COLOR_GREY, 0, true);
        }
        y += 17;
    }

    /* Barra de estado */
    vesa_fill_rect(cx, cy+ch-18, cw, 18, RGB(215,220,232));
    vesa_draw_hline(cx, cy+ch-18, cw, RGB(180,185,200));
    char status[64];
    ksprintf(status, "  %d items  |  Free: %u KB",
             count, kfs_free_blocks() * KFS_BLOCK_SIZE / 1024);
    vesa_draw_string(cx+4, cy+ch-13, status, RGB(80,85,100), 0, true);
}

/* ============================================================================
 * VENTANA: TERMINAL GRÁFICA
 * ============================================================================ */

/* Buffer de texto de la terminal gráfica */
#define GTERM_COLS  50
#define GTERM_ROWS  16
static char   gterm_buf[GTERM_ROWS][GTERM_COLS+1];
static int    gterm_row = 0, gterm_col = 0;
static bool   gterm_initialized = false;

static void gterm_print(const char* s) {
    if (!gterm_initialized) {
        for (int r = 0; r < GTERM_ROWS; r++)
            for (int c = 0; c <= GTERM_COLS; c++)
                gterm_buf[r][c] = '\0';
        /* Mensaje inicial */
        const char* welcome[] = {
            "Kosmo OS v0.1 - Graphical Terminal",
            "-----------------------------------",
            "Type commands in the VGA terminal.",
            "This window shows system output.",
            "",
            "[ Subsystems ]",
            "  GDT/IDT  : OK",
            "  PIC/PIT  : OK (100 Hz)",
            "  PS/2 KB  : OK",
            "  PS/2 MSE : OK",
            "  KosmoFS  : OK (128 KB RAM disk)",
            "  VESA VBE : OK (800x600x32)",
            "  WM       : OK",
            "",
            "Ready.",
            "",
        };
        for (int i = 0; i < GTERM_ROWS; i++) {
            int len = (int)strlen(welcome[i]);
            if (len > GTERM_COLS) len = GTERM_COLS;
            for (int c = 0; c < len; c++) gterm_buf[i][c] = welcome[i][c];
            gterm_buf[i][len] = '\0';
        }
        gterm_row = GTERM_ROWS - 1;
        gterm_initialized = true;
    }
    while (*s) {
        if (*s == '\n' || gterm_col >= GTERM_COLS) {
            gterm_col = 0;
            gterm_row = (gterm_row + 1) % GTERM_ROWS;
            gterm_buf[gterm_row][0] = '\0';
        }
        if (*s != '\n') {
            gterm_buf[gterm_row][gterm_col++] = *s;
            gterm_buf[gterm_row][gterm_col]   = '\0';
        }
        s++;
    }
}

void paint_terminal_window(uint8_t id, int cx, int cy, int cw, int ch) {
    (void)id; (void)cw;

    /* Fondo negro terminal */
    vesa_fill_rect(cx, cy, cw, ch, RGB(12, 12, 20));

    if (!gterm_initialized) gterm_print("");

    int y = cy + 4;
    int start_row = (gterm_row + 1) % GTERM_ROWS;
    for (int i = 0; i < GTERM_ROWS; i++) {
        int row = (start_row + i) % GTERM_ROWS;
        if (gterm_buf[row][0] != '\0') {
            /* Colorear líneas especiales */
            uint32_t col = RGB(0, 220, 100);
            if (gterm_buf[row][0] == '[') col = COLOR_ACCENT;
            else if (gterm_buf[row][0] == ' ' && gterm_buf[row][1] == ' ')
                col = COLOR_TEXT_LIGHT;
            else if (gterm_buf[row][0] == '-') col = RGB(70,80,100);
            else if (gterm_buf[row][0] == 'R') col = COLOR_HIGHLIGHT;

            vesa_draw_string(cx+4, y, gterm_buf[row], col, 0, true);
        }
        y += 10;
    }

    /* Cursor parpadeante (siempre encendido, sin tick) */
    vesa_fill_rect(cx+4 + gterm_col * 8, cy + 4 + gterm_row * 10,
                   6, 9, RGB(0,220,100));
}

/* ============================================================================
 * VENTANA: INFORMACIÓN DEL SISTEMA
 * ============================================================================ */
void paint_sysinfo_window(uint8_t id, int cx, int cy, int cw, int ch) {
    (void)id; (void)ch;

    int y = cy + 10;

    vesa_draw_string(cx+8, y, "System Monitor", COLOR_ACCENT, 0, true);
    y += 14;
    draw_separator(cx+4, y, cw-8);
    y += 10;

    /* CPU / Uptime */
    uint32_t ticks = (uint32_t)pit_get_ticks();
    uint32_t secs  = ticks / PIT_TICK_RATE;
    uint32_t mins  = secs / 60;
    uint32_t hrs   = mins / 60;

    draw_icon_label(cx+8, y, COLOR_ACCENT, "CPU", COLOR_TEXT_DARK);
    char tbuf[32];
    ksprintf(tbuf, "x86 32-bit (i686)");
    vesa_draw_string(cx+90, y, tbuf, COLOR_GREY, 0, true);
    y += 16;

    draw_icon_label(cx+8, y, COLOR_ACCENT2, "Uptime", COLOR_TEXT_DARK);
    ksprintf(tbuf, "%02u:%02u:%02u", hrs%24, mins%60, secs%60);
    vesa_draw_string(cx+90, y, tbuf, COLOR_GREY, 0, true);
    y += 16;

    draw_icon_label(cx+8, y, RGB(200,80,60), "RAM", COLOR_TEXT_DARK);
    uint32_t total_kb = sys_info.total_ram_kb;
    uint32_t used_kb  = (KFS_MAX_BLOCKS - kfs_free_blocks()) * KFS_BLOCK_SIZE / 1024
                        + 256; /* kernel approx */
    ksprintf(tbuf, "%u KB / %u KB", used_kb, total_kb);
    vesa_draw_string(cx+90, y, tbuf, COLOR_GREY, 0, true);
    y += 6;

    /* Barra de RAM */
    uint32_t pct = total_kb > 0 ? (used_kb * 100 / total_kb) : 0;
    if (pct > 100) pct = 100;
    draw_progress_bar(cx+8, y, cw-16, 10, pct,
                      COLOR_ACCENT, RGB(200,210,225));
    y += 18;

    draw_icon_label(cx+8, y, COLOR_HIGHLIGHT, "FS", COLOR_TEXT_DARK);
    uint32_t fs_used = (KFS_MAX_BLOCKS - kfs_free_blocks()) * KFS_BLOCK_SIZE / 1024;
    uint32_t fs_total = KFS_MAX_BLOCKS * KFS_BLOCK_SIZE / 1024;
    ksprintf(tbuf, "%u KB / %u KB", fs_used, fs_total);
    vesa_draw_string(cx+90, y, tbuf, COLOR_GREY, 0, true);
    y += 6;

    uint32_t fs_pct = fs_total > 0 ? (fs_used * 100 / fs_total) : 0;
    draw_progress_bar(cx+8, y, cw-16, 10, fs_pct,
                      COLOR_HIGHLIGHT, RGB(200,210,225));
    y += 18;

    draw_separator(cx+4, y, cw-8);
    y += 10;

    /* Ticks del sistema */
    ksprintf(tbuf, "Ticks: %u", ticks);
    vesa_draw_string(cx+8, y, tbuf, COLOR_GREY, 0, true);
    y += 12;
    ksprintf(tbuf, "PIT: %u Hz", (uint32_t)PIT_TICK_RATE);
    vesa_draw_string(cx+8, y, tbuf, COLOR_GREY, 0, true);
    y += 12;
    ksprintf(tbuf, "VBE: 800x600x32");
    vesa_draw_string(cx+8, y, tbuf, COLOR_GREY, 0, true);
}

/* ============================================================================
 * ICONOS DEL ESCRITORIO
 * ============================================================================ */

typedef struct {
    int x, y;
    const char* label;
    uint32_t    color;
} desktop_icon_t;

static const desktop_icon_t icons[] = {
    { 20,  30, "Files",   RGB(255,200,60)  },
    { 20,  95, "Term",    RGB(0,  200,100) },
    { 20, 160, "System",  RGB(100,150,255) },
    { 20, 225, "About",   COLOR_ACCENT     },
};

static void draw_desktop_icon(int x, int y, const char* label,
                               uint32_t col, bool selected) {
    /* Sombra */
    vesa_fill_rect(x+3, y+3, 44, 44, RGB(0,0,0));
    /* Cuerpo del icono */
    vesa_fill_gradient_v(x, y, 44, 44, col,
                          RGB((col>>16)&0xFF >> 1,
                              (col>>8 )&0xFF >> 1,
                               col    &0xFF >> 1));
    vesa_draw_rect(x, y, 44, 44,
                   selected ? COLOR_WHITE : RGB(255,255,255), 1);
    /* Texto del icono (centrado debajo) */
    int tx = x + (44 - vesa_text_width(label)) / 2;
    vesa_draw_string(tx, y + 48, label, COLOR_WHITE, 0, true);
}

void desktop_draw_icons(void) {
    for (uint32_t i = 0; i < sizeof(icons)/sizeof(icons[0]); i++) {
        draw_desktop_icon(icons[i].x, icons[i].y,
                          icons[i].label, icons[i].color, false);
    }
}

/* ============================================================================
 * INICIALIZACIÓN DEL ESCRITORIO
 * ============================================================================ */
void desktop_init(void) {
    uint8_t flags = WM_FLAG_VISIBLE | WM_FLAG_HAS_CLOSE | WM_FLAG_HAS_TITLE;

    /* Ventana de bienvenida / About */
    wm_create_window("About Kosmo OS",
                     260, 60, 300, 260,
                     flags, paint_about_window, NULL);

    /* Explorador de archivos */
    wm_create_window("File Manager  —  /",
                     50, 320, 340, 220,
                     flags, paint_files_window, NULL);

    /* Terminal gráfica */
    wm_create_window("Terminal",
                     400, 200, 350, 210,
                     flags, paint_terminal_window, NULL);

    /* Monitor del sistema */
    wm_create_window("System Info",
                     410, 55, 230, 230,
                     flags, paint_sysinfo_window, NULL);
}
