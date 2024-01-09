/**
 * @file lv_draw_sdl.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lv_draw.h"
#if LV_USE_DRAW_SDL
#include LV_SDL_INCLUDE_PATH
#include <SDL2/SDL_image.h>

#include "lv_draw_sdl.h"
#include "../../core/lv_refr.h"
#include "../../display/lv_display_private.h"
#include "../../stdlib/lv_string.h"
#include "../../dev/sdl/lv_sdl_window.h"

/*********************
 *      DEFINES
 *********************/
#define DRAW_UNIT_ID_SDL     100

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_draw_dsc_base_t * draw_dsc;
    int32_t w;
    int32_t h;
    SDL_Texture * texture;
} cache_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void execute_drawing(lv_draw_sdl_unit_t * u);

static int32_t dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer);

static int32_t evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task);

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
static uint8_t sdl_render_buf[2048 * 1024 * 4];

/**********************
 *  STATIC VARIABLES
 **********************/
static SDL_Texture * layer_get_texture(lv_layer_t * layer);

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_sdl_init(void)
{
    lv_draw_sdl_unit_t * draw_sdl_unit = lv_draw_create_unit(sizeof(lv_draw_sdl_unit_t));
    draw_sdl_unit->base_unit.dispatch_cb = dispatch;
    draw_sdl_unit->base_unit.evaluate_cb = evaluate;
    draw_sdl_unit->texture_cache_data_type = lv_cache_register_data_type();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static int32_t dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    lv_draw_sdl_unit_t * draw_sdl_unit = (lv_draw_sdl_unit_t *) draw_unit;

    /*Return immediately if it's busy with a draw task*/
    if(draw_sdl_unit->task_act) return 0;

    lv_draw_task_t * t = NULL;
    t = lv_draw_get_next_available_task(layer, NULL, DRAW_UNIT_ID_SDL);
    if(t == NULL) return -1;

    lv_display_t * disp = _lv_refr_get_disp_refreshing();
    SDL_Texture * texture = layer_get_texture(layer);
    if(layer != disp->layer_head && texture == NULL) {
        void * buf = lv_draw_layer_alloc_buf(layer);
        if(buf == NULL) return -1;

        SDL_Renderer * renderer = lv_sdl_window_get_renderer(disp);
        int32_t w = lv_area_get_width(&layer->buf_area);
        int32_t h = lv_area_get_height(&layer->buf_area);
        layer->user_data = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_TARGET, w, h);
    }

    t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
    draw_sdl_unit->base_unit.target_layer = layer;
    draw_sdl_unit->base_unit.clip_area = &t->clip_area;
    draw_sdl_unit->task_act = t;

    execute_drawing(draw_sdl_unit);

    draw_sdl_unit->task_act->state = LV_DRAW_TASK_STATE_READY;
    draw_sdl_unit->task_act = NULL;

    /*The draw unit is free now. Request a new dispatching as it can get a new task*/
    lv_draw_dispatch_request();
    return 1;
}

static int32_t evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task)
{
    LV_UNUSED(draw_unit);

    if(((lv_draw_dsc_base_t *)task->draw_dsc)->user_data == NULL) {
        task->preference_score = 0;
        task->preferred_draw_unit_id = DRAW_UNIT_ID_SDL;
    }
    return 0;
}

bool compare_cb(const void * data1, const void * data2, size_t data_size)
{
    LV_UNUSED(data_size);
    const cache_data_t * d1 = data1;
    const cache_data_t * d2 = data2;

    if(d1->w != d2->w) return false;
    if(d1->h != d2->h) return false;

    if(d1->draw_dsc->dsc_size != d2->draw_dsc->dsc_size) return false;

    if(memcmp(d1->draw_dsc, d2->draw_dsc, d1->draw_dsc->dsc_size)) return false;

    return true;

}

void invalidate_cb(lv_cache_entry_t * e)
{
    const cache_data_t * d = e->data;
    lv_free((void *)d->draw_dsc);
    SDL_DestroyTexture(d->texture);
    lv_free((void *)d);
    e->data = NULL;
    e->data_size = 0;
}

static lv_cache_entry_t * draw_to_texture(lv_draw_sdl_unit_t * u)
{
    lv_draw_task_t * task = u->task_act;

    lv_layer_t dest_layer;
    lv_memzero(&dest_layer, sizeof(dest_layer));
    dest_layer.buf = lv_draw_buf_align(sdl_render_buf, LV_COLOR_FORMAT_ARGB8888);
    dest_layer.color_format = LV_COLOR_FORMAT_ARGB8888;

    lv_area_t a;
    _lv_area_intersect(&a, u->base_unit.clip_area, &task->area);
    dest_layer.buf_area = task->area;
    dest_layer._clip_area = task->area;
    lv_memzero(sdl_render_buf, lv_area_get_size(&dest_layer.buf_area) * 4 + 100);

    lv_display_t * disp = _lv_refr_get_disp_refreshing();

    uint32_t tick = lv_tick_get();
    SDL_Texture * texture = NULL;
    switch(task->type) {
        case LV_DRAW_TASK_TYPE_FILL: {
                lv_draw_fill_dsc_t * fill_dsc = task->draw_dsc;
                lv_draw_rect_dsc_t rect_dsc;
                lv_draw_rect_dsc_init(&rect_dsc);
                rect_dsc.base.user_data = lv_sdl_window_get_renderer(disp);
                rect_dsc.bg_color = fill_dsc->color;
                rect_dsc.bg_grad = fill_dsc->grad;
                rect_dsc.radius = fill_dsc->radius;
                rect_dsc.bg_opa = fill_dsc->opa;

                lv_draw_rect(&dest_layer, &rect_dsc, &task->area);
            }
            break;
        case LV_DRAW_TASK_TYPE_BORDER: {
                lv_draw_border_dsc_t * border_dsc = task->draw_dsc;;
                lv_draw_rect_dsc_t rect_dsc;
                lv_draw_rect_dsc_init(&rect_dsc);
                rect_dsc.base.user_data = lv_sdl_window_get_renderer(disp);
                rect_dsc.bg_opa = LV_OPA_TRANSP;
                rect_dsc.radius = border_dsc->radius;
                rect_dsc.border_color = border_dsc->color;
                rect_dsc.border_opa = border_dsc->opa;
                rect_dsc.border_side = border_dsc->side;
                rect_dsc.border_width = border_dsc->width;
                lv_draw_rect(&dest_layer, &rect_dsc, &task->area);
                break;
            }
        case LV_DRAW_TASK_TYPE_LABEL: {
                lv_draw_label_dsc_t label_dsc;
                lv_draw_label_dsc_init(&label_dsc);
                lv_memcpy(&label_dsc, task->draw_dsc, sizeof(label_dsc));
                label_dsc.base.user_data = lv_sdl_window_get_renderer(disp);
                lv_draw_label(&dest_layer, &label_dsc, &task->area);
            }
            break;
        case LV_DRAW_TASK_TYPE_IMAGE: {
                lv_draw_image_dsc_t * image_dsc = task->draw_dsc;
                //              SDL_Surface* loadImage(std::string path) {
                const char * path = image_dsc->src;
                SDL_Surface * surface = IMG_Load(&path[2]);
                if(surface == NULL) {
                    fprintf(stderr, "could not load image: %s\n", IMG_GetError());
                    return NULL;
                }

                SDL_Renderer * renderer = lv_sdl_window_get_renderer(disp);
                texture = SDL_CreateTextureFromSurface(renderer, surface);
                break;
            }
        default:
            return NULL;
            break;
    }

    while(dest_layer.draw_task_head) {
        lv_draw_dispatch_layer(disp, &dest_layer);
        if(dest_layer.draw_task_head) {
            lv_draw_dispatch_wait_for_request();
        }
    }

    SDL_Rect rect;
    rect.x = dest_layer.buf_area.x1;
    rect.y = dest_layer.buf_area.y1;
    rect.w = lv_area_get_width(&dest_layer.buf_area);
    rect.h = lv_area_get_height(&dest_layer.buf_area);

    if(texture == NULL) {
        texture = SDL_CreateTexture(lv_sdl_window_get_renderer(disp), SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STATIC, rect.w, rect.h);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(texture, NULL, sdl_render_buf, rect.w * 4);
    }
    else {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    cache_data_t * data = lv_malloc(sizeof(cache_data_t));
    lv_draw_dsc_base_t * base_dsc = task->draw_dsc;

    data->draw_dsc = lv_malloc(base_dsc->dsc_size);
    lv_memcpy((void *)data->draw_dsc, base_dsc, base_dsc->dsc_size);
    data->w = lv_area_get_width(&task->area);
    data->h = lv_area_get_height(&task->area);
    data->texture = texture;

    lv_cache_entry_t * e = lv_cache_add(data, sizeof(cache_data_t), u->texture_cache_data_type,
                                        lv_area_get_size(&task->area) * 4);
    e->compare_cb = compare_cb;
    e->invalidate_cb = invalidate_cb;
    e->weight = lv_tick_elaps(tick);
    e->weight += lv_area_get_size(&task->area) / 10000;
    if(e->weight == 0) e->weight++;
    return e;
}

static void blend_texture_layer(lv_draw_sdl_unit_t * u)
{
    lv_display_t * disp = _lv_refr_get_disp_refreshing();
    SDL_Renderer * renderer = lv_sdl_window_get_renderer(disp);

    SDL_Rect clip_rect;
    clip_rect.x = u->base_unit.clip_area->x1;
    clip_rect.y = u->base_unit.clip_area->y1;
    clip_rect.w = lv_area_get_width(u->base_unit.clip_area);
    clip_rect.h = lv_area_get_height(u->base_unit.clip_area);

    lv_draw_task_t * t = u->task_act;
    SDL_Rect rect;
    rect.x = t->area.x1;
    rect.y = t->area.y1;
    rect.w = lv_area_get_width(&t->area);
    rect.h = lv_area_get_height(&t->area);

    lv_draw_image_dsc_t * draw_dsc = t->draw_dsc;
    lv_layer_t * src_layer = (lv_layer_t *)draw_dsc->src;
    SDL_Texture * src_texture = layer_get_texture(src_layer);

    SDL_SetTextureAlphaMod(src_texture, draw_dsc->opa);
    SDL_SetTextureBlendMode(src_texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, layer_get_texture(u->base_unit.target_layer));
    SDL_RenderSetClipRect(renderer, &clip_rect);
    SDL_RenderCopy(renderer, src_texture, NULL, &rect);
    SDL_DestroyTexture(src_texture);
    SDL_RenderSetClipRect(renderer, NULL);
}

static void draw_from_cached_texture(lv_draw_sdl_unit_t * u)
{
    lv_draw_task_t * t = u->task_act;

    cache_data_t data_to_find;
    data_to_find.draw_dsc = (lv_draw_dsc_base_t *)t->draw_dsc;

    data_to_find.w = lv_area_get_width(&t->area);
    data_to_find.h = lv_area_get_height(&t->area);
    data_to_find.texture = NULL;

    /*user_data stores the renderer to differentiate it from SW rendered tasks.
     *However the cached texture is independent from the renderer so use NULL user_data*/
    void * user_data_saved = data_to_find.draw_dsc->user_data;
    data_to_find.draw_dsc->user_data = NULL;

    lv_cache_lock();
    lv_cache_entry_t * e = lv_cache_find_by_data(&data_to_find, sizeof(data_to_find), u->texture_cache_data_type);
    data_to_find.draw_dsc->user_data = user_data_saved;
    if(e == NULL) {
        printf("cache_miss %d\n", t->type);
        e = draw_to_texture(u);
    }

    if(e == NULL) {
        lv_cache_unlock();
        return;
    }

    const cache_data_t * data_cached = lv_cache_get_data(e);
    SDL_Texture * texture = data_cached->texture;
    lv_display_t * disp = _lv_refr_get_disp_refreshing();
    SDL_Renderer * renderer = lv_sdl_window_get_renderer(disp);

    lv_layer_t * dest_layer = u->base_unit.target_layer;
    SDL_Rect clip_rect;
    clip_rect.x = u->base_unit.clip_area->x1 - dest_layer->buf_area.x1;
    clip_rect.y = u->base_unit.clip_area->y1 - dest_layer->buf_area.y1;
    clip_rect.w = lv_area_get_width(u->base_unit.clip_area);
    clip_rect.h = lv_area_get_height(u->base_unit.clip_area);

    SDL_Rect rect;

    SDL_SetRenderTarget(renderer, layer_get_texture(dest_layer));

    if(t->type == LV_DRAW_TASK_TYPE_IMAGE) {
        lv_draw_image_dsc_t * draw_dsc = t->draw_dsc;
        lv_area_t image_area;
        image_area.x1 = 0;
        image_area.y1 = 0;
        image_area.x2 = draw_dsc->header.w - 1;
        image_area.y2 = draw_dsc->header.h - 1;

        lv_area_move(&image_area, t->area.x1 - dest_layer->buf_area.x1, t->area.y1 - dest_layer->buf_area.y1);
        rect.x = image_area.x1;
        rect.y = image_area.y1;
        rect.w = lv_area_get_width(&image_area);
        rect.h = lv_area_get_height(&image_area);

        SDL_RenderSetClipRect(renderer, &clip_rect);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
    }
    else {
        rect.x = t->area.x1 - dest_layer->buf_area.x1;
        rect.y = t->area.y1 - dest_layer->buf_area.y1;
        rect.w = lv_area_get_width(&t->area);
        rect.h = lv_area_get_height(&t->area);

        SDL_RenderSetClipRect(renderer, &clip_rect);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
    }

    SDL_RenderSetClipRect(renderer, NULL);

    lv_cache_release(e);
    lv_cache_unlock();
}

static void execute_drawing(lv_draw_sdl_unit_t * u)
{
    lv_draw_task_t * t = u->task_act;

    if(t->type == LV_DRAW_TASK_TYPE_BOX_SHADOW) return;
    if(t->type == LV_DRAW_TASK_TYPE_LINE) return;
    if(t->type == LV_DRAW_TASK_TYPE_TRIANGLE) return;
    if(t->type == LV_DRAW_TASK_TYPE_BG_IMG) return;

    if(t->type == LV_DRAW_TASK_TYPE_LAYER) {
        blend_texture_layer(u);
    }
    else {
        draw_from_cached_texture(u);
    }
}

static SDL_Texture * layer_get_texture(lv_layer_t * layer)
{
    return layer->user_data;
}

#endif /*LV_USE_DRAW_SDL*/
