#include "A_Config.h"

lv_font_t* small_font;
lv_font_t* medium_font;
lv_font_t* large_font;
lv_font_t* icon_font;
void GUI_Init()
{
    small_font = lv_freetype_font_create("msyhl.ttc", 18, LV_FREETYPE_FONT_STYLE_NORMAL);
    medium_font = lv_freetype_font_create("msyhl.ttc", 26, LV_FREETYPE_FONT_STYLE_NORMAL);
    large_font = lv_freetype_font_create("msyh.ttc", 36, LV_FREETYPE_FONT_STYLE_NORMAL);
    icon_font = lv_freetype_font_create("fa.ttf", 28, LV_FREETYPE_FONT_STYLE_NORMAL);
}
void event_cb_delete_user_data_obj(lv_event_t* e)
{
    if (e->user_data)
    {
        lv_obj_fall_down((lv_obj_t *)(e->user_data), lv_pct(15), 300, 0);
        lv_obj_delete_delayed((lv_obj_t*)(e->user_data), 300);
    }
}
void msgbox_yn(const char* str, const char *yes_s, const char *no_s, lv_event_cb_t yes, lv_event_cb_t no)
{
    lv_obj_t* window = lv_obj_create(lv_layer_top());
    lv_obj_t* lbl = lv_label_create(window);
    lv_obj_t* btns[2];

    lv_obj_set_align(window, LV_ALIGN_CENTER);
    lv_obj_set_size(window, lv_pct(50), lv_pct(40));
    lv_obj_pop_up(window, lv_pct(15), 300, 0);

    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font(lbl, large_font, 0);
    lv_label_set_text(lbl, str);

    btns[0] = lv_btn_create(window);
    btns[1] = lv_btn_create(window);
    lv_obj_set_width(btns[0], lv_pct(40));
    lv_obj_set_height(btns[0], lv_pct(20));
    lv_obj_set_width(btns[1], lv_pct(40));
    lv_obj_set_height(btns[1], lv_pct(20));

    lv_obj_align(btns[0], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_align(btns[1], LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_t* lbls[2];
    lbls[0] = lv_label_create(btns[0]);
    lbls[1] = lv_label_create(btns[1]);
    lv_obj_set_style_text_font(lbls[0], medium_font, 0);
    lv_obj_set_style_text_font(lbls[1], medium_font, 0);
    lv_label_set_text(lbls[0], no_s);
    lv_label_set_text(lbls[1], yes_s);
    lv_obj_center(lbls[0]);
    lv_obj_center(lbls[1]);

    lv_obj_add_event_cb(btns[0], event_cb_delete_user_data_obj, LV_EVENT_CLICKED, window);
    lv_obj_add_event_cb(btns[1], event_cb_delete_user_data_obj, LV_EVENT_CLICKED, window);
    lv_obj_add_event_cb(btns[0], no, LV_EVENT_CLICKED, NULL);  /*Assign a callback to the button*/
    lv_obj_add_event_cb(btns[1], yes, LV_EVENT_CLICKED, NULL); /*Assign a callback to the button*/
}

void msgbox(const char* title, const char *str)
{
    lv_obj_t* window = lv_obj_create(lv_layer_top());
    lv_obj_t* lbl, *lbl_content;
    lv_obj_t* btn;

    lv_obj_set_align(window, LV_ALIGN_CENTER);
    lv_obj_set_size(window, lv_pct(50), lv_pct(40));
    lv_obj_pop_up(window, lv_pct(15), 300, 0);

    lbl = lv_label_create(window);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(70));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font(lbl, large_font, 0);
    lv_label_set_text(lbl, title);

    lbl_content = lv_label_create(window);
    lv_label_set_long_mode(lbl_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_content, lv_pct(95));
    lv_obj_set_style_text_align(lbl_content, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align_to(lbl_content, lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_set_style_text_font(lbl_content, medium_font, 0);
    lv_label_set_text(lbl_content, str);


    btn = lv_btn_create(window);
    lv_obj_set_width(btn, lv_pct(40));
    lv_obj_set_height(btn, lv_pct(20));

    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_text_font(btn, medium_font, 0);
    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "行");
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, event_cb_delete_user_data_obj, LV_EVENT_CLICKED, window);
}
