#include "A_Config.h"
#include "animations.h"
void lv_obj_push_down(lv_obj_t* obj, uint16_t distance,
    uint16_t time, uint16_t waitBeforeStart)
{
    lv_anim_t a;
    uint16_t p;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    p = lv_obj_get_style_y(obj, 0);
    lv_anim_set_values(&a, p - distance, p);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_time(&a, time);
    lv_anim_set_delay(&a, waitBeforeStart);
    lv_anim_start(&a);
    lv_obj_fade_in(obj, time, waitBeforeStart);
}

void lv_obj_pop_up(lv_obj_t* obj, uint16_t distance,
    uint16_t time, uint16_t waitBeforeStart)
{
    lv_anim_t a;
    uint16_t p;
    lv_obj_set_style_opa(obj, 0, 0);
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    p = lv_obj_get_style_y(obj, 0);
    lv_anim_set_values(&a, p + distance, p);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_time(&a, time);
    lv_anim_set_delay(&a, waitBeforeStart);
    lv_anim_start(&a);
    lv_obj_fade_in(obj, time, waitBeforeStart);
}

void lv_obj_fall_down(lv_obj_t* obj, uint16_t distance,
    uint16_t time, uint16_t waitBeforeStart)
{
    lv_anim_t a;
    uint16_t p;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    p = lv_obj_get_style_y(obj, 0);
    /*
    lv_anim_set_values(&a, p, p - distance / 4);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_time(&a, time / 5 * 2);
    lv_anim_set_delay(&a, waitBeforeStart);
    lv_anim_start(&a);

    lv_anim_set_values(&a, p - distance / 4, p + distance);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_time(&a, time / 5 * 3);
    lv_anim_set_delay(&a, waitBeforeStart + time / 5 * 2);
    lv_anim_start(&a);
    */
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, p, p + distance);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_time(&a, time);
    lv_anim_set_delay(&a, waitBeforeStart);
    a.user_data = (void*)distance;
    lv_anim_set_ready_cb(&a, [](lv_anim_t* a) {
        lv_obj_set_y((lv_obj_t*)(a->var), lv_obj_get_style_y((lv_obj_t*)(a->var), 0) - (int)a->user_data);
        });
    lv_anim_start(&a);
    lv_obj_fade_out(obj, time / 5 * 3, waitBeforeStart + time / 5 * 2);
}

void lv_obj_floating_add(lv_obj_t* obj, uint16_t waitBeforeStart)
{
    lv_anim_t a;
    uint16_t p;

    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a, GUI_ANIM_FLOATING_TIME);
    lv_anim_set_delay(&a, waitBeforeStart);
    lv_anim_set_playback_time(&a, GUI_ANIM_FLOATING_TIME);
    lv_anim_set_playback_delay(&a, GUI_ANIM_FLOATING_TIME);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);

    p = lv_obj_get_style_y(obj, 0);
    lv_anim_set_values(&a, p, p + GUI_ANIM_FLOATING_RANGE);
    lv_anim_start(&a);
}

void lv_obj_move_anim(lv_obj_t* obj, int16_t x, int16_t y,
    uint16_t time, uint16_t waitBeforeStart)
{
    lv_anim_t a;
    int16_t p;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    p = lv_obj_get_style_x(obj, 0);
    lv_anim_set_values(&a, p, x);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_time(&a, time);
    lv_anim_set_delay(&a, waitBeforeStart);
    if(x != 0x7fff)
        lv_anim_start(&a);
    p = lv_obj_get_style_y(obj, 0);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, p, y);
    if(y != 0x7fff)
		lv_anim_start(&a);
}

