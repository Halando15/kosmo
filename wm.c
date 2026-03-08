/* =============================================================================
 * KOSMO OS — Window Manager (Implementación)
 * Archivo : gui/wm.c
 * ============================================================================= */

#include "wm.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "pit.h"
#include "stdio.h"
#include "string.h"
#include "vga.h"
#include "types.h"

/* =============================================================================
 * ESTADO GLOBAL
 * ============================================================================= */
static wm_window_t windows[WM_MAX_WINDOWS];
static uint8_t     window_count = 0;
static uint8_t     focused_id   = WM_INVALID_ID;

/* Backup de píxeles bajo el cursor para borrado limpio */
static uint32_t cursor_backup[16 * 16];
static int32_t  cursor_last_x = -1, cursor_last_y = -1;

/* Flag: ¿Se necesita redibujar el fondo? */
static bool desktop_dirty = true;

/* =============================================================================
 * CURSOR EN HARDWARE — Forma de flecha 11×18 px
 * 1 = píxel del cursor (blanco con borde negro), 0 = transparente
 * ============================================================================= */
static const uint16_t CURSOR_MASK[16] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111111111000000,
    0b1111111000000000,
    0b1101111000000000,
    0b1001111100000000,
    0b0000111100000000,
    0b0000111100000000,
    0b0000011000000000,
};

static const uint16_t CURSOR_BORDER[16] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1010000000000000,
    0b1001000000000000,
    0b1000100000000000,
    0b1000010000000000,
    0b1000001000000000,
    0b1000000100000000,
    0b1000000010000000,
    0b1111111111000000,
    0b1010001000000000,
    0b1101000100000000,
    0b1000100100000000,
    0b0000010010000000,
    0b0000010010000000,
    0b0000001100000000,
};

/* =============================================================================
 * CURSOR
 * ============================================================================= */

void wm_erase_cursor(int x, int y) {
    if (cursor_last_x < 0) return;
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            int px = cursor_last_x + col;
            int py = cursor_last_y + row;
            if (px < 0 || py < 0 ||
                px >= (int)VESA_WIDTH || py >= (int)VESA_HEIGHT) continue;
            vesa_fb[(uint32_t)py * vesa_pitch_px + (uint32_t)px] =
                cursor_backup[row * 16 + col];
        }
    }
    (void)x; (void)y;
}

void wm_draw_cursor(int x, int y) {
    /* Guardar fondo */
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            int px = x + col, py = y + row;
            if (px < 0 || py < 0 ||
                px >= (int)VESA_WIDTH || py >= (int)VESA_HEIGHT) {
                cursor_backup[row*16+col] = 0;
                continue;
            }
            cursor_backup[row*16+col] =
                vesa_fb[(uint32_t)py * vesa_pitch_px + (uint32_t)px];
        }
    }
    cursor_last_x = x;
    cursor_last_y = y;

    /* Dibujar: primero el borde negro, luego el relleno blanco */
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            int px = x + col, py = y + row;
            if (px < 0 || py < 0 ||
                px >= (int)VESA_WIDTH || py >= (int)VESA_HEIGHT) continue;
            uint16_t bit = (uint16_t)(0x8000 >> col);
            if (CURSOR_BORDER[row] & bit) {
                vesa_fb[(uint32_t)py * vesa_pitch_px + (uint32_t)px] = COLOR_BLACK;
            } else if (CURSOR_MASK[row] & bit) {
                vesa_fb[(uint32_t)py * vesa_pitch_px + (uint32_t)px] = COLOR_WHITE;
            }
        }
    }
}

/* =============================================================================
 * FONDO DE ESCRITORIO — gradiente azul oscuro con texto
 * ============================================================================= */
static void draw_desktop(void) {
    /* Gradiente de fondo */
    vesa_fill_gradient_v(0, 0, VESA_WIDTH, VESA_HEIGHT - WM_TASKBAR_H,
                          COLOR_DESKTOP_TOP, COLOR_DESKTOP_BOTTOM);

    /* Logo de fondo semitransparente */
    const char* logo[] = {
        "KOSMO OS",
        "v0.1.0 Genesis"
    };
    int lx = VESA_WIDTH / 2 - 4 * 16;
    int ly = VESA_HEIGHT / 2 - 40;
    vesa_draw_string_scaled(lx, ly,     logo[0], RGB(30,55,100), 0, true, 4);
    vesa_draw_string       (VESA_WIDTH/2 - (int)strlen(logo[1])*4,
                            ly + 40,   logo[1], RGB(40,65,110), 0, true);

    /* Barra de tareas */
    vesa_fill_rect(0, VESA_HEIGHT - WM_TASKBAR_H,
                   VESA_WIDTH, WM_TASKBAR_H, COLOR_TASKBAR_BG);
    /* Línea superior de la taskbar */
    vesa_draw_hline(0, VESA_HEIGHT - WM_TASKBAR_H,
                    VESA_WIDTH, COLOR_TASKBAR_BORDER);

    /* Botón "Kosmo" a la izquierda */
    vesa_fill_rect(4, VESA_HEIGHT - WM_TASKBAR_H + 4,
                   80, WM_TASKBAR_H - 8, COLOR_WIN_TITLE_ACTIVE);
    vesa_draw_rect(4, VESA_HEIGHT - WM_TASKBAR_H + 4,
                   80, WM_TASKBAR_H - 8, COLOR_ACCENT, 1);
    vesa_draw_string(12, VESA_HEIGHT - WM_TASKBAR_H + 10,
                     "  Kosmo", COLOR_TEXT_LIGHT, 0, true);

    /* Reloj a la derecha (estático — se actualiza en el loop) */
    draw_taskbar_clock();

    desktop_dirty = false;
}

void draw_taskbar_clock(void) {
    /* Fondo del área del reloj */
    int cx = VESA_WIDTH - 72;
    int cy = VESA_HEIGHT - WM_TASKBAR_H + 2;
    vesa_fill_rect(cx, cy, 70, WM_TASKBAR_H - 4, COLOR_TASKBAR_BG);

    /* Calcular tiempo desde arranque */
    uint32_t secs  = (uint32_t)(pit_get_ticks() / PIT_TICK_RATE);
    uint32_t mins  = secs / 60;
    uint32_t hours = mins / 60;
    secs %= 60; mins %= 60; hours %= 24;

    char timebuf[16];
    ksprintf(timebuf, "%02u:%02u:%02u", hours, mins, secs);
    vesa_draw_string(cx + 4, cy + 8, timebuf, COLOR_TEXT_LIGHT, 0, true);
}

/* =============================================================================
 * DIBUJO DE VENTANAS
 * ============================================================================= */

static void draw_window_frame(wm_window_t* w) {
    bool focused = (w->id == focused_id);

    /* Sombra */
    vesa_fill_rect(w->x + 4, w->y + 4, w->w, w->h, RGB(0,0,0));
    /* Fondo de borde */
    uint32_t border_col = focused ? COLOR_WIN_BORDER : RGB(70,80,110);
    vesa_draw_rect(w->x, w->y, w->w, w->h, border_col, WM_BORDER);

    /* Barra de título */
    uint32_t title_col = focused ? COLOR_WIN_TITLE_ACTIVE : COLOR_WIN_TITLE;
    vesa_fill_gradient_v(w->x + WM_BORDER,
                          w->y + WM_BORDER,
                          w->w - WM_BORDER*2,
                          WM_TITLE_BAR_H,
                          title_col,
                          RGB(10,30,80));

    /* Texto del título */
    if (w->flags & WM_FLAG_HAS_TITLE) {
        int tx = w->x + WM_BORDER + 8;
        int ty = w->y + WM_BORDER + (WM_TITLE_BAR_H - 8) / 2;
        vesa_draw_string(tx, ty, w->title, COLOR_TEXT_LIGHT, 0, true);
    }

    /* Botón de cierre (X roja en la esquina derecha) */
    if (w->flags & WM_FLAG_HAS_CLOSE) {
        int bx = w->x + w->w - WM_BORDER - WM_BTN_SIZE - WM_BTN_MARGIN;
        int by = w->y + WM_BORDER + (WM_TITLE_BAR_H - WM_BTN_SIZE) / 2;
        vesa_fill_rect(bx, by, WM_BTN_SIZE, WM_BTN_SIZE, COLOR_WIN_CLOSE);
        vesa_draw_rect(bx, by, WM_BTN_SIZE, WM_BTN_SIZE, RGB(255,120,120), 1);
        /* X */
        vesa_draw_line(bx+3, by+3, bx+WM_BTN_SIZE-4, by+WM_BTN_SIZE-4, COLOR_WHITE);
        vesa_draw_line(bx+WM_BTN_SIZE-4, by+3, bx+3, by+WM_BTN_SIZE-4, COLOR_WHITE);
    }

    /* Área cliente — fondo blanco/gris claro */
    int cx, cy, cw, ch;
    wm_get_client_area(w->id, &cx, &cy, &cw, &ch);
    vesa_fill_rect(cx, cy, cw, ch, COLOR_WIN_BG);
}

static void draw_window(wm_window_t* w) {
    if (!(w->flags & WM_FLAG_VISIBLE)) return;
    draw_window_frame(w);

    /* Llamar al callback de pintado del cliente */
    if (w->on_paint) {
        int cx, cy, cw, ch;
        wm_get_client_area(w->id, &cx, &cy, &cw, &ch);
        w->on_paint(w->id, cx, cy, cw, ch);
    }

    w->flags &= (uint8_t)~WM_FLAG_DIRTY;
}

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

void wm_init(void) {
    memset(windows, 0, sizeof(windows));
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        windows[i].id = WM_INVALID_ID;
    window_count = 0;
    focused_id   = WM_INVALID_ID;
}

wm_window_t* wm_get_window(uint8_t id) {
    if (id >= WM_MAX_WINDOWS) return NULL;
    if (windows[id].id == WM_INVALID_ID) return NULL;
    return &windows[id];
}

uint8_t wm_create_window(const char* title,
                          int x, int y, int w, int h,
                          uint8_t flags,
                          wm_paint_fn on_paint,
                          wm_event_fn on_event) {
    if (window_count >= WM_MAX_WINDOWS) return WM_INVALID_ID;

    /* Buscar slot libre */
    uint8_t id = WM_INVALID_ID;
    for (uint8_t i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].id == WM_INVALID_ID) { id = i; break; }
    }
    if (id == WM_INVALID_ID) return WM_INVALID_ID;

    wm_window_t* win = &windows[id];
    memset(win, 0, sizeof(wm_window_t));
    win->id       = id;
    win->x        = x;  win->y = y;
    win->w        = w;  win->h = h;
    win->flags    = flags | WM_FLAG_DIRTY;
    win->z_order  = window_count;
    win->on_paint = on_paint;
    win->on_event = on_event;
    strncpy(win->title, title ? title : "", 47);

    window_count++;
    wm_set_focus(id);
    return id;
}

void wm_destroy_window(uint8_t id) {
    wm_window_t* w = wm_get_window(id);
    if (!w) return;
    w->id = WM_INVALID_ID;
    window_count--;
    desktop_dirty = true;
    if (focused_id == id) focused_id = WM_INVALID_ID;
}

void wm_invalidate(uint8_t id) {
    wm_window_t* w = wm_get_window(id);
    if (w) w->flags |= WM_FLAG_DIRTY;
}

void wm_set_focus(uint8_t id) { focused_id = id; }
uint8_t wm_get_focus(void)    { return focused_id; }

void wm_get_client_area(uint8_t id, int* cx, int* cy, int* cw, int* ch) {
    wm_window_t* w = wm_get_window(id);
    if (!w) { *cx=*cy=*cw=*ch=0; return; }
    *cx = w->x + WM_BORDER;
    *cy = w->y + WM_BORDER + WM_TITLE_BAR_H;
    *cw = w->w - WM_BORDER * 2;
    *ch = w->h - WM_BORDER * 2 - WM_TITLE_BAR_H;
}

void wm_move_window(uint8_t id, int x, int y) {
    wm_window_t* w = wm_get_window(id);
    if (!w) return;
    /* Clip al área de trabajo (sin taskbar) */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w->w > (int)VESA_WIDTH)  x = (int)VESA_WIDTH  - w->w;
    if (y + w->h > (int)VESA_HEIGHT - WM_TASKBAR_H)
        y = (int)VESA_HEIGHT - WM_TASKBAR_H - w->h;
    w->x = x; w->y = y;
    desktop_dirty = true;
}

void wm_redraw_all(void) {
    if (desktop_dirty) draw_desktop();

    /* Dibujar ventanas en orden Z */
    for (uint8_t i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].id != WM_INVALID_ID &&
            (windows[i].flags & WM_FLAG_VISIBLE)) {
            draw_window(&windows[i]);
        }
    }
}

/* =============================================================================
 * DETECCIÓN DE HIT — ¿En qué parte de la ventana hizo clic el ratón?
 * ============================================================================= */
typedef enum { HIT_NONE, HIT_TITLE, HIT_CLOSE, HIT_CLIENT } wm_hit_t;

static wm_hit_t hit_test(wm_window_t* w, int mx, int my) {
    if (mx < w->x || mx >= w->x + w->w ||
        my < w->y || my >= w->y + w->h) return HIT_NONE;

    /* Botón de cierre */
    if (w->flags & WM_FLAG_HAS_CLOSE) {
        int bx = w->x + w->w - WM_BORDER - WM_BTN_SIZE - WM_BTN_MARGIN;
        int by = w->y + WM_BORDER + (WM_TITLE_BAR_H - WM_BTN_SIZE) / 2;
        if (mx >= bx && mx < bx + WM_BTN_SIZE &&
            my >= by && my < by + WM_BTN_SIZE) return HIT_CLOSE;
    }

    /* Barra de título */
    if (my < w->y + WM_BORDER + WM_TITLE_BAR_H) return HIT_TITLE;

    return HIT_CLIENT;
}

/* =============================================================================
 * BUCLE PRINCIPAL DEL WM
 * ============================================================================= */
void wm_run(void) {
    bool     btn_prev  = false;
    uint8_t  drag_id   = WM_INVALID_ID;
    int32_t  drag_ox   = 0, drag_oy = 0;
    uint64_t last_tick = 0;

    /* Dibujo inicial completo */
    wm_redraw_all();
    wm_draw_cursor(VESA_WIDTH/2, VESA_HEIGHT/2);

    while (true) {
        const mouse_state_t* ms = mouse_get_state();
        bool btn_down  = ms->btn_left;
        bool btn_click = btn_down && !btn_prev;
        bool btn_release = !btn_down && btn_prev;
        int32_t mx = ms->x, my = ms->y;

        bool needs_redraw = desktop_dirty;

        /* ── Inicio de arrastre ──────────────────────────────────────── */
        if (btn_click) {
            /* Buscar la ventana que está encima en el punto del clic */
            uint8_t top_id = WM_INVALID_ID;
            uint8_t top_z  = 0;
            for (uint8_t i = 0; i < WM_MAX_WINDOWS; i++) {
                wm_window_t* w = &windows[i];
                if (w->id == WM_INVALID_ID) continue;
                if (!(w->flags & WM_FLAG_VISIBLE)) continue;
                if (hit_test(w, (int)mx, (int)my) != HIT_NONE) {
                    if (w->z_order >= top_z) {
                        top_z  = w->z_order;
                        top_id = i;
                    }
                }
            }

            if (top_id != WM_INVALID_ID) {
                wm_set_focus(top_id);
                wm_window_t* w = &windows[top_id];
                wm_hit_t hit = hit_test(w, (int)mx, (int)my);

                if (hit == HIT_CLOSE) {
                    /* Notificar cierre */
                    wm_event_t ev = {.type=WM_EVENT_CLOSE, .win_id=top_id};
                    if (w->on_event) w->on_event(&ev);
                    wm_destroy_window(top_id);
                    needs_redraw = true;
                } else if (hit == HIT_TITLE) {
                    drag_id = top_id;
                    drag_ox = (int32_t)mx - w->x;
                    drag_oy = (int32_t)my - w->y;
                    w->flags |= WM_FLAG_DRAGGING;
                } else if (hit == HIT_CLIENT) {
                    int cx, cy, cw, ch;
                    wm_get_client_area(top_id, &cx, &cy, &cw, &ch);
                    wm_event_t ev = {
                        .type     = WM_EVENT_MOUSE_DOWN,
                        .win_id   = top_id,
                        .mouse_x  = (int32_t)mx - cx,
                        .mouse_y  = (int32_t)my - cy,
                        .btn_left = true,
                    };
                    if (w->on_event) w->on_event(&ev);
                    wm_invalidate(top_id);
                    needs_redraw = true;
                }
            }
        }

        /* ── Movimiento durante arrastre ─────────────────────────────── */
        if (drag_id != WM_INVALID_ID && btn_down) {
            int32_t new_x = mx - drag_ox;
            int32_t new_y = my - drag_oy;
            wm_move_window(drag_id, (int)new_x, (int)new_y);
            wm_invalidate(drag_id);
            needs_redraw = true;
        }

        /* ── Fin de arrastre ─────────────────────────────────────────── */
        if (btn_release && drag_id != WM_INVALID_ID) {
            wm_window_t* w = wm_get_window(drag_id);
            if (w) w->flags &= (uint8_t)~WM_FLAG_DRAGGING;
            drag_id = WM_INVALID_ID;
        }

        /* ── Redibujado ──────────────────────────────────────────────── */
        if (needs_redraw || ms->moved) {
            wm_erase_cursor(cursor_last_x, cursor_last_y);
            if (needs_redraw) wm_redraw_all();
            wm_draw_cursor((int)mx, (int)my);
        }

        /* ── Actualizar reloj cada segundo ───────────────────────────── */
        uint64_t now = pit_get_ticks();
        if (now - last_tick >= PIT_TICK_RATE) {
            last_tick = now;
            wm_erase_cursor(cursor_last_x, cursor_last_y);
            draw_taskbar_clock();
            wm_draw_cursor((int)mx, (int)my);
        }

        btn_prev = btn_down;
        mouse_clear_event();

        /* Pequeña pausa para no quemar la CPU */
        hlt();
    }
}
