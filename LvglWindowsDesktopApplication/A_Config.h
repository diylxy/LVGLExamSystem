#pragma once
#include <lvgl.h>
#include <Windows.h>
#include <mutex>
extern std::mutex lv_update_mtx;
#define LOCKLV() lv_update_mtx.lock()
#define UNLOCKLV() lv_update_mtx.unlock()
#define GUI_ANIM_FLOATING_RANGE 8
#define GUI_ANIM_FLOATING_TIME 1000
#include <animations.h>
/// //GUI
extern lv_font_t* small_font;
extern lv_font_t* medium_font;
extern lv_font_t* large_font;
extern lv_font_t* icon_font;
void GUI_Init();
void msgbox_yn(const char* str, const char* yes_s, const char* no_s, lv_event_cb_t yes, lv_event_cb_t no);
void msgbox(const char* title, const char* str);


typedef struct
{
	bool valid;
	int id;
	char* question;
	char* answerA;
	char* answerB;
	char* answerC;
	char* answerD;
	char answer;
} question_t;
typedef struct {
	int current_pos;
	char *answer;		// A:A, B:B, C:C, D:D
	bool visable;
	int *selected_answer;			//指向答案数组中的对应元素, 0:A, 1:B, 2:C, 3:D
} question_show_t;