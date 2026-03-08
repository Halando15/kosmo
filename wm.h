/* =============================================================================
 * KOSMO OS — Window Manager
 * Archivo : gui/wm.h
 * Función : Gestor de ventanas ligero: crea/destruye ventanas, gestiona foco,
 *           orden Z, arrastre con ratón, botones de control y redibujado.
 * ============================================================================= */

#ifndef KOSMO_WM_H
#define KOSMO_WM_H

#include "types.h"
#include "vesa.h"

/* =============================================================================
 * CONSTANTES DE VENTANAS
 * ============================================================================= */
#define WM_MAX_WINDOWS      8
#define WM_TITLE_BAR_H      24      /* Altura de la barra de título */
#define WM_BORDER           2       /* Grosor del borde */
#define WM_BTN_SIZE         16      /* Tamaño de botones de control */
#define WM_BTN_MARGIN       4       /* Margen entre botones y borde */
#define WM_TASKBAR_H        32      /* Altura de la barra de tareas */
#define WM_INVALID_ID       0xFF

/* Flags de ventana */
#define WM_FLAG_VISIBLE     (1 << 0)
#define WM_FLAG_DRAGGING    (1 << 1)
#define WM_FLAG_RESIZABLE   (1 << 2)
#define WM_FLAG_HAS_CLOSE   (1 << 3)
#define WM_FLAG_HAS_TITLE   (1 << 4)
#define WM_FLAG_MODAL       (1 << 5)
#define WM_FLAG_DIRTY       (1 << 6)   /* Necesita redibujarse */

/* Tipos de evento de ratón dentro de una ventana */
typedef enum {
    WM_EVENT_NONE = 0,
    WM_EVENT_MOUSE_MOVE,
    WM_EVENT_MOUSE_DOWN,
    WM_EVENT_MOUSE_UP,
    WM_EVENT_CLOSE,
    WM_EVENT_KEY,
} wm_event_type_t;

/* =============================================================================
 * ESTRUCTURAS
 * ============================================================================= */

/* Evento que se pasa al callback de la ventana */
typedef struct {
    wm_event_type_t type;
    int32_t         mouse_x;    /* Relativo al interior de la ventana */
    int32_t         mouse_y;
    bool            btn_left;
    bool            btn_right;
    char            key;        /* Para WM_EVENT_KEY */
    uint8_t         win_id;
} wm_event_t;

/* Callback de pintado: el cliente pinta el contenido interior */
typedef void (*wm_paint_fn)(uint8_t win_id, int cx, int cy, int cw, int ch);
/* Callback de evento */
typedef bool (*wm_event_fn)(const wm_event_t* event);

/* Descriptor de ventana */
typedef struct {
    uint8_t     id;
    char        title[48];
    int32_t     x, y;           /* Posición en pantalla */
    int32_t     w, h;           /* Dimensiones totales (incluyendo borde y título) */
    uint8_t     flags;
    uint8_t     z_order;        /* 0 = fondo, mayor = más arriba */
    wm_paint_fn on_paint;
    wm_event_fn on_event;
    /* Estado de arrastre */
    int32_t     drag_ox, drag_oy;
} wm_window_t;

/* =============================================================================
 * API DEL WINDOW MANAGER
 * ============================================================================= */

/* Inicializar el WM (llama a vesa_init internamente) */
void wm_init(void);

/* Bucle principal del WM — no retorna */
void wm_run(void);

/* Crear una ventana. Retorna su ID (WM_INVALID_ID si falla) */
uint8_t wm_create_window(const char* title,
                          int x, int y, int w, int h,
                          uint8_t flags,
                          wm_paint_fn on_paint,
                          wm_event_fn on_event);

/* Destruir una ventana */
void wm_destroy_window(uint8_t id);

/* Marcar la ventana como sucia (forzar redibujado) */
void wm_invalidate(uint8_t id);

/* Obtener puntero a una ventana */
wm_window_t* wm_get_window(uint8_t id);

/* Foco */
void wm_set_focus(uint8_t id);
uint8_t wm_get_focus(void);

/* Área cliente de una ventana (sin borde ni título) */
void wm_get_client_area(uint8_t id, int* cx, int* cy, int* cw, int* ch);

/* Dibujar todas las ventanas (llamado en el bucle principal) */
void wm_redraw_all(void);

/* Mover una ventana */
void wm_move_window(uint8_t id, int x, int y);

/* Dibujar cursor del ratón en su posición actual */
void wm_draw_cursor(int x, int y);

/* Limpiar la huella del cursor */
void wm_erase_cursor(int x, int y);

#endif /* KOSMO_WM_H */
