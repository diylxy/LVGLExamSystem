#pragma once

void lv_obj_push_down(lv_obj_t* obj, uint16_t distance, uint16_t time, uint16_t waitBeforeStart);

void lv_obj_pop_up(lv_obj_t* obj, uint16_t distance, uint16_t time, uint16_t waitBeforeStart);

void lv_obj_fall_down(lv_obj_t* obj, uint16_t distance, uint16_t time, uint16_t waitBeforeStart);

void lv_obj_floating_add(lv_obj_t* obj, uint16_t waitBeforeStart);

void lv_obj_move_anim(lv_obj_t* obj, int16_t x, int16_t y, uint16_t time, uint16_t waitBeforeStart);
