#include <stdio.h>
#include <lvgl.h>
#include "sqlite3.h"
#include <mutex>
#include "A_Config.h"
#include "iconv.h"

iconv_t iconv_U2G = NULL;			// UTF8转GBK，用于转换剪贴板
iconv_t iconv_G2U = NULL;			// GBK转UTF8，用于转换剪贴板
sqlite3* db = NULL;

lv_obj_t* debug_label;
lv_obj_t* debug_window;
lv_obj_t* obj_answerSheet;
lv_obj_t* answerSheetTable;
lv_obj_t* lbl_time_remain;		//用于给倒计时增加淡入动画

#define QUESTION_MAX 10000
int cfg_question_count = 100;
int cfg_examTime = 10 * 60;
int cfg_scorePerQuestion = 10;
bool cfg_darkMode = false;
extern std::mutex lv_update_mtx;
extern volatile bool g_WindowQuitSignal;
extern HWND g_WindowHandle;
extern int volatile g_WindowWidth, g_WindowHeight;
int time_remain = 0;
bool countdown_began = false;
bool answerSheetVisable = true;
bool isSubmitPressed = false;
char temp_string[1000];
char* clipBoardText = NULL;
lv_obj_t* question_cards[4];
question_show_t question_cards_data[4];
int question_show_current_index = 0;		//当前屏幕中央的题目（从0开始）
int question_card_center_index = 0;			//题目卡片中央的题目（从0开始，最左边是0）
question_t question_list_data[QUESTION_MAX];//本次抽到的题目列表
int answer_list[QUESTION_MAX];			//答题结果
int question_count = 0;				//本次抽到的题目总数
int question_total = 0;				//数据库中的题目总数
int scorePerQuestion = 10;				//每题分数
bool showAnswerImmediately = false;	//是否立即显示答案
bool showAnimation = true;			//是否显示题目切换动画
bool autoNext = true;				//是否自动下一题
const int questionCardPoss[5] = { -93 * 2, -93, 0, 93, 93 * 2 };//问题卡片位置id对应坐标
bool isInHomeScreen = false;	//是否处于主界面

lv_style_t style_lbl_question;
lv_style_t style_lbl_choice;
lv_style_t style_obj_right;
lv_style_t style_obj_wrong;
lv_style_t style_obj_selected;
lv_style_t style_obj_marked;
lv_style_t style_btn_genetic;
lv_style_t style_btn_close;
lv_style_t style_card;
lv_style_t style_card_orange;
lv_style_t style_btn_question;
lv_style_t style_line;
void init_style()
{
	lv_style_init(&style_lbl_question);
	lv_style_init(&style_lbl_choice);
	lv_style_init(&style_btn_genetic);
	lv_style_init(&style_obj_selected);
	lv_style_init(&style_obj_right);
	lv_style_init(&style_obj_wrong);
	lv_style_init(&style_obj_marked);
	lv_style_init(&style_card);
	lv_style_init(&style_card_orange);
	lv_style_init(&style_line);
	lv_style_init(&style_btn_close);
	lv_style_init(&style_btn_question);


}
void refresh_style(bool dark)
{
	lv_style_set_text_font(&style_lbl_question, medium_font);
	lv_style_set_text_font(&style_lbl_choice, medium_font);
	lv_style_set_text_font(&style_btn_question, medium_font);

	lv_style_set_radius(&style_card, 30);
	lv_style_set_radius(&style_btn_question, 10);
	lv_style_set_shadow_width(&style_card, 5);
	lv_style_set_shadow_ofs_x(&style_card, 2);
	lv_style_set_shadow_ofs_y(&style_card, 2);
	lv_style_set_shadow_opa(&style_card, 128);
	lv_style_set_shadow_width(&style_btn_question, 5);
	lv_style_set_shadow_ofs_x(&style_btn_question, 2);
	lv_style_set_shadow_ofs_y(&style_btn_question, 2);
	lv_style_set_shadow_opa(&style_btn_question, 128);
	if (dark == false)
	{
		lv_style_set_bg_color(&style_btn_question, lv_palette_lighten(LV_PALETTE_BLUE, 1));
		lv_style_set_bg_color(&style_btn_genetic, lv_palette_lighten(LV_PALETTE_BLUE, 1));
		lv_style_set_bg_color(&style_obj_selected, lv_palette_lighten(LV_PALETTE_YELLOW, 1));
		lv_style_set_bg_color(&style_obj_right, lv_palette_lighten(LV_PALETTE_GREEN, 1));
		lv_style_set_bg_color(&style_obj_wrong, lv_palette_lighten(LV_PALETTE_RED, 1));
		lv_style_set_bg_color(&style_card, lv_palette_lighten(LV_PALETTE_BLUE, 4));
		lv_style_set_bg_color(&style_obj_marked, lv_palette_lighten(LV_PALETTE_PURPLE, 1));
		lv_style_set_bg_color(&style_card_orange, lv_palette_lighten(LV_PALETTE_ORANGE, 4));
		lv_style_set_bg_color(&style_btn_close, lv_palette_lighten(LV_PALETTE_RED, 1));
		lv_style_set_shadow_color(&style_card, lv_color_black());
		lv_style_set_shadow_color(&style_btn_question, lv_color_black());
		lv_style_set_text_color(&style_btn_question, lv_color_black());
		lv_style_set_line_color(&style_line, lv_palette_lighten(LV_PALETTE_BLUE, 3));
	}
	else
	{
		lv_style_set_bg_color(&style_btn_question, lv_palette_darken(LV_PALETTE_BLUE, 1));
		lv_style_set_bg_color(&style_btn_genetic, lv_palette_darken(LV_PALETTE_BLUE, 1));
		lv_style_set_bg_color(&style_obj_selected, lv_palette_darken(LV_PALETTE_YELLOW, 1));
		lv_style_set_bg_color(&style_obj_right, lv_palette_darken(LV_PALETTE_GREEN, 1));
		lv_style_set_bg_color(&style_obj_wrong, lv_palette_darken(LV_PALETTE_RED, 1));
		lv_style_set_bg_color(&style_card, lv_palette_darken(LV_PALETTE_BLUE_GREY, 3));
		lv_style_set_bg_color(&style_obj_marked, lv_palette_darken(LV_PALETTE_PURPLE, 1));
		lv_style_set_bg_color(&style_card_orange, lv_palette_darken(LV_PALETTE_DEEP_ORANGE, 4));
		lv_style_set_bg_color(&style_btn_close, lv_palette_darken(LV_PALETTE_RED, 1));
		lv_style_set_shadow_color(&style_card, lv_color_white());
		lv_style_set_shadow_color(&style_btn_question, lv_color_white());
		lv_style_set_text_color(&style_btn_question, lv_color_white());
		lv_style_set_line_color(&style_line, lv_palette_darken(LV_PALETTE_BLUE, 3));
	}
}
void setDisplayMode(bool dark)
{
	//lv_theme_mono_init(lv_disp_get_default(), dark, medium_font);
	lv_theme_default_init(lv_disp_get_default(), lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), dark, medium_font);
	cfg_darkMode = dark;
	refresh_style(dark);
}
void executeOnFullscreen();
lv_obj_t* buildMainWindow();
bool open_database()
{
	int rc = sqlite3_open("questions.db", &db);
	if (rc)
		return false;
	return true;
}
void saveConfig()
{
	//用数据库存储配置
	/*
	表结构
	Create  TABLE MAIN.[SETTINGS](
[id] integer PRIMARY KEY ASC ON CONFLICT Fail AUTOINCREMENT
,[name] text UNIQUE
,[value] text
	*/
	char buf[1000];
	//首先检查表是否存在
	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db, "SELECT * FROM sqlite_master WHERE type='table' AND name='SETTINGS'", -1, &stmt, NULL);
	if (sqlite3_step(stmt) != SQLITE_ROW)
	{
		sqlite3_finalize(stmt);
		sqlite3_exec(db, "CREATE TABLE SETTINGS (id integer PRIMARY KEY ASC ON CONFLICT Fail AUTOINCREMENT, name text UNIQUE, value integer)", NULL, NULL, NULL);
		//创建默认配置
		sprintf(buf, "INSERT INTO SETTINGS (name,value) VALUES ('question_count',%d)", cfg_question_count);
		sqlite3_exec(db, buf, NULL, NULL, NULL);
		sprintf(buf, "INSERT INTO SETTINGS (name,value) VALUES ('scorePerQuestion',%d)", cfg_scorePerQuestion);
		sqlite3_exec(db, buf, NULL, NULL, NULL);
		sprintf(buf, "INSERT INTO SETTINGS (name,value) VALUES ('examTime',%d)", cfg_examTime);
		sqlite3_exec(db, buf, NULL, NULL, NULL);
		sprintf(buf, "INSERT INTO SETTINGS (name,value) VALUES ('darkMode',%d)", cfg_darkMode == 1 ? 1 : 0);
		sqlite3_exec(db, buf, NULL, NULL, NULL);
		sqlite3_exec(db, "Drop Trigger If Exists MAIN.[trigger_question];\nCreate  Trigger MAIN.[trigger_question] AFTER DELETE On[question]\nBEGIN\nUPDATE question SET 'ID' = (ID - 1) WHERE ID > old.ID;\nEND; ", NULL, NULL, NULL);
		return;
	}
	sprintf(buf, "UPDATE SETTINGS SET value=%d WHERE name='question_count'", cfg_question_count);
	sqlite3_exec(db, buf, NULL, NULL, NULL);
	sprintf(buf, "UPDATE SETTINGS SET value=%d WHERE name='scorePerQuestion'", cfg_scorePerQuestion);
	sqlite3_exec(db, buf, NULL, NULL, NULL);
	sprintf(buf, "UPDATE SETTINGS SET value=%d WHERE name='examTime'", cfg_examTime);
	sqlite3_exec(db, buf, NULL, NULL, NULL);
	sprintf(buf, "UPDATE SETTINGS SET value=%d WHERE name='darkMode'", cfg_darkMode == 1 ? 1 : 0);
	sqlite3_exec(db, buf, NULL, NULL, NULL);
}
void loadConfig()
{
	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db, "SELECT * FROM sqlite_master WHERE type='table' AND name='SETTINGS'", -1, &stmt, NULL);
	if (sqlite3_step(stmt) != SQLITE_ROW)
	{
		sqlite3_finalize(stmt);
		saveConfig();
		return;
	}
	sqlite3_finalize(stmt);
	sqlite3_prepare_v2(db, "SELECT * FROM SETTINGS", -1, &stmt, NULL);
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const unsigned char* name = sqlite3_column_text(stmt, 1);
		int value = sqlite3_column_int(stmt, 2);
		if (strcmp((char*)name, "question_count") == 0)
			cfg_question_count = value;
		else if (strcmp((char*)name, "scorePerQuestion") == 0)
			cfg_scorePerQuestion = value;
		else if (strcmp((char*)name, "examTime") == 0)
			cfg_examTime = value;
		else if (strcmp((char*)name, "darkMode") == 0)
			cfg_darkMode = value;
	}
}
void alert_bug()
{
	msgbox("请不要尝试卡BUG", "你干嘛哈哈哎呦~");
}
void setFullScreen(bool enable)
{
	if (enable)
	{
		LONG lStyle;
		lStyle = WS_VISIBLE | WS_POPUP;
		SetWindowLong(g_WindowHandle, GWL_STYLE, lStyle);
		SetWindowPos(g_WindowHandle, HWND_TOP, -2, -2, GetSystemMetrics(SM_CXSCREEN) + 4, GetSystemMetrics(SM_CYSCREEN) + 4, SWP_SHOWWINDOW);
	}
	else
	{
		LONG lStyle;
		lStyle = WS_OVERLAPPEDWINDOW;
		SetWindowLong(g_WindowHandle, GWL_STYLE, lStyle);
		SetWindowPos(g_WindowHandle, HWND_TOP, (GetSystemMetrics(SM_CXSCREEN) - 1280) / 2, (GetSystemMetrics(SM_CYSCREEN) - 720) / 2, 1280, 720, SWP_SHOWWINDOW);
	}
}
void add_timer_flyaway(lv_obj_t* obj)
{
	//因为LVGL有bug，所以这里用了一个定时器来把隐藏的按钮移动到屏幕外
	lv_timer_t* timer = lv_timer_create([](lv_timer_t* timer) {
		lv_obj_t* obj = (lv_obj_t*)lv_timer_get_user_data(timer);
		lv_obj_set_pos(obj, -1000, -1000);
		lv_timer_delete(timer);
		}, 400, obj);
}
const char* getClipBoard()
{
	if (clipBoardText)
		free(clipBoardText);
	clipBoardText = NULL;
	if (OpenClipboard(NULL))
	{
		HANDLE hData = GetClipboardData(CF_TEXT);
		if (hData == NULL)
			return "";
		else
		{
			char* pszText = static_cast<char*>(GlobalLock(hData));
			if (pszText == NULL)
				return "";
			else
			{
				char* ptrInBuffer = pszText;
				char* ptrOutBuffer;
				size_t insize = strlen(pszText);
				size_t outsize = insize * 2;
				clipBoardText = (char*)malloc(outsize + 1);
				if (clipBoardText == NULL)
					return "";
				ptrOutBuffer = clipBoardText;
				size_t ret = iconv(iconv_G2U, &ptrInBuffer, (size_t*)&insize, &ptrOutBuffer, (size_t*)&outsize);
				*ptrOutBuffer = 0;
				GlobalUnlock(hData);
			}
		}
		CloseClipboard();
	}
	if (clipBoardText == NULL)
		return "";
	return clipBoardText;
}
bool setClipBoard(const char* text)
{
	if (OpenClipboard(NULL))
	{
		int insize = strlen(text);
		EmptyClipboard();
		HGLOBAL clipbuffer;
		char* buffer = NULL;
		clipbuffer = GlobalAlloc(GMEM_DDESHARE, strlen(text) + 1);
		if (clipbuffer == NULL)
			return false;
		buffer = (char*)GlobalLock(clipbuffer);
		if (buffer == NULL)
			return false;
		char* outBuffer = buffer;
		iconv(iconv_U2G, (char**)&text, (size_t*)&insize, &outBuffer, (size_t*)&insize);
		*outBuffer = 0;
		GlobalUnlock(clipbuffer);
		SetClipboardData(CF_TEXT, clipbuffer);
		CloseClipboard();
		return true;
	}
	return false;
}
void attachClipBoardToolToTextarea(lv_obj_t* textarea)
{
	lv_font_t* font = lv_freetype_font_create("fa.ttf", 22, LV_FREETYPE_FONT_STYLE_NORMAL);
	lv_obj_t* obj_box = lv_obj_create(lv_scr_act());
	lv_obj_set_style_opa(obj_box, 0, 0);
	lv_obj_set_style_margin_all(obj_box, 2, 0);
	lv_obj_set_style_pad_all(obj_box, 2, 0);
	lv_obj_set_size(obj_box, lv_pct(10), lv_pct(5));

	lv_obj_t* btn_cut = lv_btn_create(obj_box);
	lv_obj_set_size(btn_cut, lv_pct(30), lv_pct(100));
	lv_obj_align(btn_cut, LV_ALIGN_LEFT_MID, 0, 0);
	lv_obj_t* lbl_cut = lv_label_create(btn_cut);
	lv_label_set_text(lbl_cut, LV_SYMBOL_CUT);
	lv_obj_set_style_text_font(lbl_cut, font, 0);
	lv_obj_center(lbl_cut);

	lv_obj_t* btn_copy = lv_btn_create(obj_box);
	lv_obj_set_size(btn_copy, lv_pct(30), lv_pct(100));
	lv_obj_align(btn_copy, LV_ALIGN_CENTER, 0, 0);
	lv_obj_t* lbl_copy = lv_label_create(btn_copy);
	lv_label_set_text(lbl_copy, LV_SYMBOL_COPY);
	lv_obj_set_style_text_font(lbl_copy, font, 0);
	lv_obj_center(lbl_copy);

	lv_obj_t* btn_paste = lv_btn_create(obj_box);
	lv_obj_set_size(btn_paste, lv_pct(30), lv_pct(100));
	lv_obj_align(btn_paste, LV_ALIGN_RIGHT_MID, 0, 0);
	lv_obj_t* lbl_paste = lv_label_create(btn_paste);
	lv_label_set_text(lbl_paste, LV_SYMBOL_PASTE);
	lv_obj_set_style_text_font(lbl_paste, font, 0);
	lv_obj_center(lbl_paste);

	lv_obj_add_event_cb(textarea, [](lv_event_t* e) {
		if (e->code == LV_EVENT_FOCUSED)
		{
			if (lv_obj_get_style_opa((lv_obj_t*)lv_event_get_user_data(e), 0) != 255)
			{
				lv_obj_align_to((lv_obj_t*)lv_event_get_user_data(e), lv_event_get_target_obj(e), LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
				lv_obj_fade_in((lv_obj_t*)lv_event_get_user_data(e), 300, 0);
			}
		}
		else if (e->code == LV_EVENT_DEFOCUSED)
		{
			if (lv_obj_get_style_opa((lv_obj_t*)lv_event_get_user_data(e), 0) != 0)
			{
				lv_obj_fall_down((lv_obj_t*)lv_event_get_user_data(e), lv_pct(15), 300, 0);
				add_timer_flyaway((lv_obj_t*)lv_event_get_user_data(e));
			}
		}
		}, LV_EVENT_ALL, obj_box);
	lv_obj_add_event_cb(btn_cut, [](lv_event_t* e) {
		if (lv_obj_is_valid((lv_obj_t*)lv_event_get_user_data(e)) == false)
			return;
		if (e->code == LV_EVENT_CLICKED)
		{
			const char* text = lv_textarea_get_text((lv_obj_t*)lv_event_get_user_data(e));
			if (text)
			{
				if (setClipBoard(text))
					lv_textarea_set_text((lv_obj_t*)lv_event_get_user_data(e), "");
			}
		}
		}, LV_EVENT_ALL, textarea);
	lv_obj_add_event_cb(btn_copy, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			const char* text = lv_textarea_get_text((lv_obj_t*)lv_event_get_user_data(e));
			if (text)
			{
				setClipBoard(text);
			}
		}
		}, LV_EVENT_ALL, textarea);
	lv_obj_add_event_cb(btn_paste, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			const char* text = getClipBoard();
			if (text)
			{
				lv_textarea_add_text((lv_obj_t*)lv_event_get_user_data(e), text);
			}
		}
		}, LV_EVENT_ALL, textarea);
}
void read_questions(char* query)
{
	char* zErrMsg = 0;
	int rc;
	const char* sql;
	sqlite3_stmt* stmt;
	sql = query;
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		sprintf(temp_string, "SQL error: %s\n", zErrMsg);
		lv_label_ins_text(debug_label, -1, temp_string);
		sqlite3_free(zErrMsg);
	}
	else
	{
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			int id = sqlite3_column_int(stmt, 0);
			const unsigned char* question = sqlite3_column_text(stmt, 1);
			const unsigned char* answerA = sqlite3_column_text(stmt, 2);
			const unsigned char* answerB = sqlite3_column_text(stmt, 3);
			const unsigned char* answerC = sqlite3_column_text(stmt, 4);
			const unsigned char* answerD = sqlite3_column_text(stmt, 5);
			const unsigned char* answer = sqlite3_column_text(stmt, 6);
			sprintf(temp_string, "id = %d, question = %s, answerA = %s, answerB = %s, answerC = %s, answerD = %s, answer = %s\n", id, question, answerA, answerB, answerC, answerD, answer);
			lv_label_ins_text(debug_label, -1, temp_string);
		}
	}
	sqlite3_finalize(stmt);
}
void read_questions_by_id_debug(int id)
{
	char* zErrMsg = 0;
	int rc;
	const char* sql;
	sqlite3_stmt* stmt;
	sql = "Select * From question where id = ?";
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		sprintf(temp_string, "SQL error: %s\n", zErrMsg);
		lv_label_ins_text(debug_label, -1, temp_string);
		sqlite3_free(zErrMsg);
	}
	else
	{
		sqlite3_bind_int(stmt, 1, id);
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			int id = sqlite3_column_int(stmt, 0);
			const unsigned char* question = sqlite3_column_text(stmt, 1);
			const unsigned char* answerA = sqlite3_column_text(stmt, 2);
			const unsigned char* answerB = sqlite3_column_text(stmt, 3);
			const unsigned char* answerC = sqlite3_column_text(stmt, 4);
			const unsigned char* answerD = sqlite3_column_text(stmt, 5);
			const unsigned char* answer = sqlite3_column_text(stmt, 6);
			sprintf(temp_string, "id = %d, question = %s, answerA = %s, answerB = %s, answerC = %s, answerD = %s, answer = %s\n", id, question, answerA, answerB, answerC, answerD, answer);
			lv_label_ins_text(debug_label, -1, temp_string);
		}
	}
	sqlite3_finalize(stmt);
}
void add_debug_window()
{
	lv_obj_t* obj_box = lv_obj_create(lv_scr_act());
	lv_obj_set_style_text_font(obj_box, medium_font, 0);
	lv_obj_set_style_opa(obj_box, 0, 0);
	lv_obj_set_width(obj_box, lv_pct(50));
	lv_obj_set_height(obj_box, lv_pct(50));
	lv_obj_center(obj_box);

	debug_label = lv_label_create(obj_box);
	lv_obj_set_style_text_font(debug_label, small_font, 0);
	lv_label_set_text(debug_label, "");

	lv_obj_set_size(debug_label, lv_pct(90), lv_pct(85));
	lv_obj_set_align(debug_label, LV_ALIGN_BOTTOM_MID);
	lv_label_set_long_mode(debug_label, LV_LABEL_LONG_WRAP);

	//创建文本框
	lv_obj_t* ta = lv_textarea_create(obj_box);
	lv_textarea_set_text(ta, "Select * From question");
	lv_obj_set_width(ta, lv_pct(85));
	lv_obj_set_height(ta, 50);
	lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 0, 0);
	attachClipBoardToolToTextarea(ta);
	//在它右边创建一个按钮
	lv_obj_t* btn = lv_btn_create(obj_box);
	lv_obj_set_width(btn, 50);
	lv_obj_set_height(btn, 50);
	lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -70, 0);
	lv_obj_set_style_text_font(btn, icon_font, 0);
	lv_obj_t* lbl = lv_label_create(btn);
	lv_label_set_text(lbl, LV_SYMBOL_REFRESH);
	lv_obj_add_event_cb(btn, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			lv_label_set_text(debug_label, "");
			read_questions((char*)lv_textarea_get_text((lv_obj_t*)lv_event_get_user_data(e)));
		}
		}, LV_EVENT_ALL, ta);

	//创建一个关闭按钮
	lv_obj_t* btn_close = lv_btn_create(obj_box);
	lv_obj_set_width(btn_close, 50);
	lv_obj_set_height(btn_close, 50);
	lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
	lv_obj_set_style_text_font(btn_close, icon_font, 0);
	lv_obj_t* lbl_close = lv_label_create(btn_close);
	lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
	lv_obj_add_event_cb(btn_close, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			lv_obj_fall_down((lv_obj_t*)lv_event_get_user_data(e), lv_pct(15), 300, 0);
			lv_obj_delete_delayed(debug_window, 300);
		}
		}, LV_EVENT_ALL, obj_box);

	lv_obj_pop_up(obj_box, lv_pct(15), 300, 0);
	if (lv_obj_is_valid(debug_window))
	{
		lv_obj_fall_down(debug_window, lv_pct(15), 300, 0);
		lv_obj_delete_delayed(debug_window, 300);
	}
	debug_window = obj_box;
	read_questions_by_id_debug(1);
}
void add_button_exit()
{
	lv_obj_t* btn1 = lv_btn_create(lv_layer_top());

	lv_obj_remove_flag(lv_layer_top(), LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_text_font(btn1, icon_font, 0);
	lv_obj_add_style(btn1, &style_btn_close, 0);
	lv_obj_t* lbl = lv_label_create(btn1);
	lv_label_set_text(lbl, LV_SYMBOL_SETTINGS);
	lv_obj_align(btn1, LV_ALIGN_TOP_RIGHT, -5, 5);
	//设置obj
	lv_obj_t* obj_box = lv_obj_create(lv_layer_top());
	lv_obj_set_style_text_font(obj_box, medium_font, 0);
	lv_obj_set_style_opa(obj_box, 0, 0);
	lv_obj_set_width(obj_box, LV_SIZE_CONTENT);
	lv_obj_set_height(obj_box, LV_SIZE_CONTENT);

	lv_obj_t* lbl_enable_anim = lv_label_create(obj_box);
	lv_label_set_text(lbl_enable_anim, "题目动画");
	lv_obj_align(lbl_enable_anim, LV_ALIGN_TOP_LEFT, 0, 0);
	lv_obj_t* sw_enable_anim = lv_switch_create(obj_box);
	lv_obj_align_to(sw_enable_anim, lbl_enable_anim, LV_ALIGN_OUT_RIGHT_MID, 25, 0);
	lv_obj_add_state(sw_enable_anim, LV_STATE_CHECKED);


	lv_obj_t* lbl_enable_fullscreen = lv_label_create(obj_box);
	lv_label_set_text(lbl_enable_fullscreen, "全屏模式");
	lv_obj_align_to(lbl_enable_fullscreen, lbl_enable_anim, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
	lv_obj_t* sw_enable_fullscreen = lv_switch_create(obj_box);
	lv_obj_align_to(sw_enable_fullscreen, lbl_enable_fullscreen, LV_ALIGN_OUT_RIGHT_MID, 25, 0);
	lv_obj_add_state(sw_enable_fullscreen, LV_STATE_CHECKED);

	lv_obj_t* lbl_auto_next = lv_label_create(obj_box);
	lv_label_set_text(lbl_auto_next, "自动下一题");
	lv_obj_align_to(lbl_auto_next, lbl_enable_fullscreen, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
	lv_obj_t* sw_auto_next = lv_switch_create(obj_box);
	lv_obj_align_to(sw_auto_next, lbl_auto_next, LV_ALIGN_OUT_RIGHT_MID, 25, 0);
	lv_obj_add_state(sw_auto_next, LV_STATE_CHECKED);

	lv_obj_t* lbl_dark_mode = lv_label_create(obj_box);
	lv_label_set_text(lbl_dark_mode, "深色模式");
	lv_obj_align_to(lbl_dark_mode, lbl_auto_next, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
	lv_obj_t* sw_dark_mode = lv_switch_create(obj_box);
	lv_obj_align_to(sw_dark_mode, lbl_dark_mode, LV_ALIGN_OUT_RIGHT_MID, 25, 0);
	if (cfg_darkMode)
		lv_obj_add_state(sw_dark_mode, LV_STATE_CHECKED);

	lv_obj_t* btn_debug = lv_btn_create(obj_box);
	lv_obj_set_size(btn_debug, 120, 45);
	lv_obj_t* lbl_debug = lv_label_create(btn_debug);
	lv_label_set_text(lbl_debug, "调试");
	lv_obj_center(lbl_debug);
	lv_obj_align_to(btn_debug, lbl_dark_mode, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 25);
	lv_obj_set_style_opa(btn_debug, 0, 0);

	lv_obj_t* btn_exit = lv_btn_create(obj_box);
	lv_obj_set_size(btn_exit, 120, 45);
	lv_obj_set_style_bg_color(btn_exit, lv_theme_get_color_secondary(btn_exit), 0);
	lv_obj_t* lbl_exit = lv_label_create(btn_exit);
	lv_label_set_text(lbl_exit, "退出");
	lv_obj_center(lbl_exit);
	lv_obj_align_to(btn_exit, btn_debug, LV_ALIGN_OUT_RIGHT_MID, 25, 0);

	lv_obj_add_event_cb(sw_enable_anim, [](lv_event_t* e) {
		if (e->code == LV_EVENT_VALUE_CHANGED)
		{
			if (lv_obj_has_state((lv_obj_t*)lv_event_get_target_obj(e), LV_STATE_CHECKED))
				showAnimation = true;
			else
				showAnimation = false;
		}
		}, LV_EVENT_ALL, NULL);

	lv_obj_add_event_cb(sw_auto_next, [](lv_event_t* e) {
		if (e->code == LV_EVENT_VALUE_CHANGED)
		{
			if (lv_obj_has_state((lv_obj_t*)lv_event_get_target_obj(e), LV_STATE_CHECKED))
				autoNext = true;
			else
				autoNext = false;
		}
		}, LV_EVENT_ALL, NULL);

	lv_obj_add_event_cb(sw_dark_mode, [](lv_event_t* e) {
		if (e->code == LV_EVENT_VALUE_CHANGED)
		{
			if (lv_obj_has_state((lv_obj_t*)lv_event_get_target_obj(e), LV_STATE_CHECKED))
				setDisplayMode(true);
			else
				setDisplayMode(false);
			saveConfig();
		}
		}, LV_EVENT_ALL, NULL);

	lv_obj_add_event_cb(sw_enable_fullscreen, [](lv_event_t* e) {
		if (e->code == LV_EVENT_VALUE_CHANGED)
		{
			lv_obj_t* btn = (lv_obj_t*)lv_event_get_user_data(e);
			setFullScreen(lv_obj_has_state((lv_obj_t*)lv_event_get_target_obj(e), LV_STATE_CHECKED));
			lv_timer_t* refresh_timer = lv_timer_create([](lv_timer_t* timer) {
				lv_obj_t* btn = (lv_obj_t*)lv_timer_get_user_data(timer);
				lv_obj_align_to((lv_obj_t*)lv_obj_get_user_data(btn), btn, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);
				executeOnFullscreen();
				lv_timer_delete(timer);
				}, 40, btn);
		}
		}, LV_EVENT_ALL, btn1);

	lv_obj_add_event_cb(btn_exit, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			g_WindowQuitSignal = true;
		}
		}, LV_EVENT_ALL, NULL);
	/*
	lv_obj_add_event_cb(btn_debug, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			add_debug_window();
		}
		}, LV_EVENT_ALL, NULL);
		*/
	lv_obj_add_event_cb(btn1, [](lv_event_t* e) {
		if (lv_event_get_code(e) == LV_EVENT_CLICKED)
		{
			lv_obj_t* obj = (lv_obj_t*)lv_event_get_user_data(e);
			if (lv_obj_get_style_opa(obj, 0) != 255)
			{
				lv_obj_fade_in(obj, 300, 0);
				lv_obj_align_to(obj, lv_event_get_target_obj(e), LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);
			}
			else
			{
				lv_obj_fall_down(obj, lv_pct(15), 300, 0);
				add_timer_flyaway((lv_obj_t*)lv_event_get_user_data(e));
			}
		}
		}, LV_EVENT_ALL, obj_box);
	lv_obj_align_to(obj_box, btn1, LV_ALIGN_OUT_BOTTOM_RIGHT, -1000, -1000);
	lv_obj_set_user_data(btn1, obj_box);
}
static int32_t my_anim_path_cubic_bezier(const lv_anim_t* a, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
	/*Calculate the current step*/
	uint32_t t = lv_map(a->act_time, 0, a->duration, 0, LV_BEZIER_VAL_MAX);
	int32_t step = lv_cubic_bezier(t, x1, y1, x2, y2);

	int32_t new_value;
	new_value = step * (a->end_value - a->start_value);
	new_value = new_value >> LV_BEZIER_VAL_SHIFT;
	new_value += a->start_value;

	return new_value;
}
int32_t my_anim_path_ease_out(const lv_anim_t* a)
{
	return my_anim_path_cubic_bezier(a, LV_BEZIER_VAL_FLOAT(0), LV_BEZIER_VAL_FLOAT(0.72),
		LV_BEZIER_VAL_FLOAT(0.39), LV_BEZIER_VAL_FLOAT(1));
}
int32_t my_anim_path_pause_center(const lv_anim_t* a)//快——慢——快
{
	return my_anim_path_cubic_bezier(a, LV_BEZIER_VAL_FLOAT(1), LV_BEZIER_VAL_FLOAT(-0.3),
		LV_BEZIER_VAL_FLOAT(0), LV_BEZIER_VAL_FLOAT(1));
}
int32_t my_pct_to_px(int32_t pct, int32_t max)
{
	return (pct * max) / 100;
}
void switchScreen(lv_obj_t* scr_new, lv_screen_load_anim_t direction, bool remove_old = true)
{
	lv_screen_load_anim(scr_new, direction, 1000, 0, remove_old, (lv_anim_path_cb_t*)my_anim_path_pause_center);
}
int getTotalQuestionsFromDB()
{
	char* zErrMsg = 0;
	int rc;
	const char* sql;
	sqlite3_stmt* stmt;
	sql = "Select count(*) From question";
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		sprintf(temp_string, "SQL error: %s\n", zErrMsg);
		lv_label_ins_text(debug_label, -1, temp_string);
		sqlite3_free(zErrMsg);
	}
	else
	{
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			question_total = sqlite3_column_int(stmt, 0);
		}
	}
	sqlite3_finalize(stmt);
	return question_total;
}
void clearQuestion(question_t* q)
{
	if (q->question)
		free(q->question);
	if (q->answerA)
		free(q->answerA);
	if (q->answerB)
		free(q->answerB);
	if (q->answerC)
		free(q->answerC);
	if (q->answerD)
		free(q->answerD);
	memset(q, 0, sizeof(question_t));
}
void clearQuestionList()
{
	for (int i = 0; i < QUESTION_MAX; ++i)
	{
		clearQuestion(&question_list_data[i]);
	}
}
void clearAnswerList()
{
	memset(answer_list, -1, sizeof(answer_list));
}
int getQuestionsRandomly(int count)
{
	int rc;
	const char* sql;
	sqlite3_stmt* stmt;
	clearQuestionList();

	question_count = 0;
	if (count > QUESTION_MAX)count = QUESTION_MAX;
	sql = "Select * From question order by random() limit ?";
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, count);
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			int id = sqlite3_column_int(stmt, 0);
			const unsigned char* question = sqlite3_column_text(stmt, 1);
			const unsigned char* answerA = sqlite3_column_text(stmt, 2);
			const unsigned char* answerB = sqlite3_column_text(stmt, 3);
			const unsigned char* answerC = sqlite3_column_text(stmt, 4);
			const unsigned char* answerD = sqlite3_column_text(stmt, 5);
			const unsigned char* answer = sqlite3_column_text(stmt, 6);
			question_list_data[question_count].id = id;
			question_list_data[question_count].question = (char*)malloc(strlen((char*)question) + 1);
			strcpy(question_list_data[question_count].question, (char*)question);
			question_list_data[question_count].answerA = (char*)malloc(strlen((char*)answerA) + 1);
			strcpy(question_list_data[question_count].answerA, (char*)answerA);
			question_list_data[question_count].answerB = (char*)malloc(strlen((char*)answerB) + 1);
			strcpy(question_list_data[question_count].answerB, (char*)answerB);
			question_list_data[question_count].answerC = (char*)malloc(strlen((char*)answerC) + 1);
			strcpy(question_list_data[question_count].answerC, (char*)answerC);
			question_list_data[question_count].answerD = (char*)malloc(strlen((char*)answerD) + 1);
			strcpy(question_list_data[question_count].answerD, (char*)answerD);
			question_list_data[question_count].answer = ((char*)answer)[0];
			question_count++;
		}
	}
	sqlite3_finalize(stmt);
	return question_count;
}
bool getQuestionByID(int id, question_t* result)
{
	int rc;
	const char* sql;
	sqlite3_stmt* stmt;
	sql = "Select * From question where id = ?";
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, id);
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			int id = sqlite3_column_int(stmt, 0);
			const unsigned char* question = sqlite3_column_text(stmt, 1);
			const unsigned char* answerA = sqlite3_column_text(stmt, 2);
			const unsigned char* answerB = sqlite3_column_text(stmt, 3);
			const unsigned char* answerC = sqlite3_column_text(stmt, 4);
			const unsigned char* answerD = sqlite3_column_text(stmt, 5);
			const unsigned char* answer = sqlite3_column_text(stmt, 6);
			result->id = id;
			result->question = (char*)malloc(strlen((char*)question) + 1);
			strcpy(result->question, (char*)question);
			result->answerA = (char*)malloc(strlen((char*)answerA) + 1);
			strcpy(result->answerA, (char*)answerA);
			result->answerB = (char*)malloc(strlen((char*)answerB) + 1);
			strcpy(result->answerB, (char*)answerB);
			result->answerC = (char*)malloc(strlen((char*)answerC) + 1);
			strcpy(result->answerC, (char*)answerC);
			result->answerD = (char*)malloc(strlen((char*)answerD) + 1);
			strcpy(result->answerD, (char*)answerD);
			result->answer = ((char*)answer)[0];
			return true;
		}
	}
	sqlite3_finalize(stmt);
	return false;
}
void applyQuestionToCard(lv_obj_t* card, question_t* q, question_show_t* show, int question_id)
{
	lv_obj_t* obj_label = lv_obj_get_child(card, 0);   //题目
	lv_label_set_text(obj_label, q->question);
	lv_obj_t* obj_btn = lv_obj_get_child(card, 1);   //答案A
	lv_obj_t* obj_label_btn = lv_obj_get_child(obj_btn, 0);
	lv_label_set_text(obj_label_btn, q->answerA);
	obj_btn = lv_obj_get_child(card, 2);   //答案B
	obj_label_btn = lv_obj_get_child(obj_btn, 0);
	lv_label_set_text(obj_label_btn, q->answerB);
	obj_btn = lv_obj_get_child(card, 3);   //答案C
	obj_label_btn = lv_obj_get_child(obj_btn, 0);
	lv_label_set_text(obj_label_btn, q->answerC);
	obj_btn = lv_obj_get_child(card, 4);   //答案D
	obj_label_btn = lv_obj_get_child(obj_btn, 0);
	lv_label_set_text(obj_label_btn, q->answerD);
	show->answer = &(q->answer);
	obj_label_btn = lv_obj_get_child(card, 5);   //题号
	lv_label_set_text_fmt(obj_label_btn, "%d.", question_id + 1);
}
void setQuestionCardPos(int index, int pos)
{
	if (pos > 4 || pos < 0)
	{
		question_cards_data[index].visable = false;
		lv_obj_set_style_opa(question_cards[index], 0, 0);
		question_cards_data[index].current_pos = pos;
		return;
	}
	lv_obj_set_x(question_cards[index], my_pct_to_px(questionCardPoss[pos], g_WindowWidth));
	question_cards_data[index].current_pos = pos;
}
void updateQuestionCards(bool updatePos = true)
{
	for (int i = 0; i < 4; ++i)
	{
		if (question_cards_data[i].visable)
			lv_obj_set_style_opa(question_cards[i], 255, 0);
		else
			lv_obj_set_style_opa(question_cards[i], 0, 0);

		for (int j = 1; j < 5; ++j)//遍历四个选项
		{
			lv_obj_t* obj_btn = lv_obj_get_child(question_cards[i], j);
			lv_obj_clear_state(obj_btn, LV_STATE_USER_1 | LV_STATE_USER_2 | LV_STATE_USER_3);
		}
		if (updatePos)
			setQuestionCardPos(i, question_cards_data[i].current_pos);
		if (question_cards_data[i].visable == false)continue;
		if (*question_cards_data[i].selected_answer != -1)
		{
			if (showAnswerImmediately)
			{
				for (int j = 1; j < 5; ++j)//遍历四个选项
				{
					lv_obj_t* obj_btn = lv_obj_get_child(question_cards[i], j);
					if (j == *question_cards_data[i].selected_answer + 1)
					{
						if (*question_cards_data[i].selected_answer == *question_cards_data[i].answer - 'A')
							lv_obj_add_state(obj_btn, LV_STATE_USER_2);
						else
							lv_obj_add_state(obj_btn, LV_STATE_USER_3);
					}
					else if (j == *question_cards_data[i].answer - 'A' + 1)
					{
						lv_obj_add_state(obj_btn, LV_STATE_USER_2);
					}
				}
			}
			else
			{
				lv_obj_t* obj_btn = lv_obj_get_child(question_cards[i], *question_cards_data[i].selected_answer + 1);
				lv_obj_add_state(obj_btn, LV_STATE_USER_1);
			}
		}
		else
			lv_obj_clear_state(question_cards[i], LV_STATE_USER_1);
	}
}
void switchToQuestion(int id)
{
	if (id > question_count || id < 0)
		return;
	question_card_center_index = 1;
	for (int i = 0; i < 4; ++i)
	{
		int index = id - 1 + i;			//这一道题的id
		if (index < 0 || index >= question_count)
		{
			question_cards_data[i].visable = false;
			continue;
		}
		else
		{
			question_cards_data[i].visable = true;
			applyQuestionToCard(question_cards[i], &question_list_data[index], &question_cards_data[i], index);
			question_cards_data[i].current_pos = i + 1;
			question_cards_data[i].selected_answer = &answer_list[index];
		}
	}
	updateQuestionCards();
	question_show_current_index = id;
}
void moveEverything(int direction)  //1:向右移动，查看上一道题，-1:向左移动，查看下一道题
{
	if (question_show_current_index - direction >= question_count || question_show_current_index - direction < 0)
		return;
	int left_idx = (question_card_center_index - 1 + 4) % 4;				//用来存储左边的卡片
	int right_idx = (question_card_center_index + 1) % 4;				//用来存储右边的卡片
	int next_idx = (question_card_center_index + 2) % 4;				//用来存储下一道题的卡片
	question_show_current_index -= direction;							//更新中间的题号
	for (int i = 0; i < 4; ++i)
	{
		if (question_cards_data[i].visable == false)//因为这个对应的题目不存在，所以不用移动
		{
			question_cards_data[i].current_pos += (5 + direction);
			question_cards_data[i].current_pos %= 5;
			question_cards_data[i].current_pos += direction;
			continue;
		}
		if (i == next_idx)
		{
			//提前计算好下一道题的卡片对应位置
			if (direction == 1)
				question_cards_data[i].current_pos = 0;
			else
				question_cards_data[i].current_pos = 4;
			continue;
		}
		//除了最后一题的卡片，其它的都按照坐标移动一个
		//因为中间和左边的卡片已经确定，所以不用担心数组越界的问题。但初始化时要注意定好左右的卡片
		lv_anim_t a;
		lv_anim_init(&a);
		lv_anim_set_var(&a, question_cards[i]);
		lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
		lv_anim_set_values(&a, my_pct_to_px(questionCardPoss[question_cards_data[i].current_pos], g_WindowWidth), my_pct_to_px(questionCardPoss[question_cards_data[i].current_pos + direction], g_WindowWidth));
		lv_anim_set_path_cb(&a, my_anim_path_ease_out);
		lv_anim_set_time(&a, showAnimation ? 400 : 10);
		lv_anim_start(&a);
		question_cards_data[i].current_pos += direction;
	}
	question_card_center_index -= direction;
	question_card_center_index = (question_card_center_index + 4) % 4;
	// 接下来利用next_idx卡片存储题目
	if (question_show_current_index == 0 || question_show_current_index == question_count - 1)
	{
		question_cards_data[next_idx].visable = false;
	}
	else
	{
		int next_pos = direction == -1 ? 3 : 1;
		question_cards_data[next_idx].visable = true;
		applyQuestionToCard(question_cards[next_idx], &question_list_data[question_show_current_index - direction], &question_cards_data[next_idx], question_show_current_index - direction);
		question_cards_data[next_idx].current_pos = next_pos;
		question_cards_data[next_idx].selected_answer = &answer_list[question_show_current_index - direction];
		lv_anim_t a;
		lv_anim_init(&a);
		lv_anim_set_var(&a, question_cards[next_idx]);
		lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
		lv_anim_set_values(&a, my_pct_to_px(questionCardPoss[question_cards_data[next_idx].current_pos - direction], g_WindowWidth), my_pct_to_px(questionCardPoss[question_cards_data[next_idx].current_pos], g_WindowWidth));
		lv_anim_set_path_cb(&a, my_anim_path_ease_out);
		lv_anim_set_time(&a, showAnimation ? 400 : 10);
		lv_anim_start(&a);
	}

	updateQuestionCards(false);  //更新显示状态
}
static void my_obj_set_gap(void* obj, int32_t x)
{
	lv_obj_set_style_margin_top((lv_obj_t*)obj, x, 0);
}
lv_obj_t* createAnswerInfoCard(lv_obj_t* flex_view, int qn, int anim_delay, bool showAnswers = true)		//利用答题情况追加错题内容卡片
{
	lv_obj_t* obj_card = lv_obj_create(flex_view);
	lv_obj_add_style(obj_card, &style_card, 0);
	lv_obj_set_style_text_font(obj_card, medium_font, 0);
	lv_obj_set_width(obj_card, lv_pct(100));
	lv_obj_set_height(obj_card, LV_SIZE_CONTENT);

	lv_obj_t* lbl_question = lv_label_create(obj_card);
	lv_label_set_text_fmt(lbl_question, "%d. %s\n    A: %s\n    B: %s\n    C: %s\n    D: %s", qn + 1, question_list_data[qn].question, question_list_data[qn].answerA, question_list_data[qn].answerB, question_list_data[qn].answerC, question_list_data[qn].answerD);
	lv_obj_set_width(lbl_question, lv_pct(100));

	if (showAnswers)	//用来复用代码，可以用在数据库管理
	{
		lv_obj_t* obj_answer_right = lv_obj_create(obj_card);//正确答案卡片，放在左下角
		lv_obj_add_style(obj_answer_right, &style_obj_right, 0);
		lv_obj_set_width(obj_answer_right, lv_pct(40));
		lv_obj_set_height(obj_answer_right, LV_SIZE_CONTENT);
		lv_obj_align_to(obj_answer_right, lbl_question, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
		lv_obj_t* lbl_answer_right = lv_label_create(obj_answer_right);
		lv_label_set_text_fmt(lbl_answer_right, "正确答案：%c", question_list_data[qn].answer);
		lv_obj_center(lbl_answer_right);

		lv_obj_t* obj_answer_wrong = lv_obj_create(obj_card);//错误答案卡片，放在右下角
		lv_obj_add_style(obj_answer_wrong, &style_obj_wrong, 0);
		lv_obj_set_width(obj_answer_wrong, lv_pct(40));
		lv_obj_set_height(obj_answer_wrong, LV_SIZE_CONTENT);
		lv_obj_align_to(obj_answer_wrong, lbl_question, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 0);
		lv_obj_t* lbl_answer_wrong = lv_label_create(obj_answer_wrong);
		if (answer_list[qn] == -1)
			lv_label_set_text(lbl_answer_wrong, "你没有作答");
		else
			lv_label_set_text_fmt(lbl_answer_wrong, "你的答案：%c", 'A' + answer_list[qn]);
		lv_obj_center(lbl_answer_wrong);
	}
	else
	{
		char str[100];
		sprintf(str, "\n正确答案：%c", question_list_data[qn].answer);
		lv_label_ins_text(lbl_question, -1, str);
	}

	//下面是上浮动画
	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_var(&a, obj_card);
	lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)my_obj_set_gap);
	lv_anim_set_time(&a, 800);
	lv_anim_set_delay(&a, anim_delay);
	lv_anim_set_values(&a, 100, 0);
	lv_anim_set_path_cb(&a, my_anim_path_ease_out);
	lv_anim_start(&a);
	lv_obj_fade_in(obj_card, 800, anim_delay);
	//返回卡片
	return obj_card;
}
void reallyHandIn()
{
	lv_font_t* font_big = lv_freetype_font_create("msyh.ttc", 180, LV_FREETYPE_FONT_STYLE_NORMAL);
	lv_font_t* font_small = lv_freetype_font_create("msyhl.ttc", 64, LV_FREETYPE_FONT_STYLE_NORMAL);

	lv_obj_t* scr_statistic = lv_obj_create(NULL);
	lv_obj_remove_flag(scr_statistic, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t* lbl_score_info = lv_label_create(scr_statistic);
	lv_obj_set_style_text_font(lbl_score_info, font_small, 0);
	lv_label_set_text(lbl_score_info, showAnswerImmediately == false ? "你的考试成绩" : "你的练习成绩");
	lv_obj_align(lbl_score_info, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_fade_in(lbl_score_info, 800, 2200);

	lv_obj_t* lbl_score = lv_label_create(scr_statistic);
	lv_obj_set_style_text_font(lbl_score, font_big, 0);
	lv_obj_align(lbl_score, LV_ALIGN_TOP_MID, 0, my_pct_to_px(30, g_WindowHeight));
	lv_obj_move_anim(lbl_score, 0x7FFF, my_pct_to_px(5, g_WindowHeight), 800, 1000);
	//下面创建一个卡片，显示错题
	lv_obj_t* flex_view = lv_obj_create(scr_statistic);
	lv_obj_add_style(flex_view, &style_card_orange, 0);
	lv_obj_set_style_radius(flex_view, 30, 0);
	lv_obj_set_flex_flow(flex_view, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_width(flex_view, lv_pct(85));
	lv_obj_set_height(flex_view, lv_pct(70));
	lv_obj_set_align(flex_view, LV_ALIGN_TOP_MID);
	lv_obj_set_pos(flex_view, 0, my_pct_to_px(30, g_WindowHeight));
	lv_obj_pop_up(flex_view, my_pct_to_px(20, g_WindowHeight), 800, 1500);
	int initial_delay = 2000;
	int correct = 0;
	int blank = 0;
	countdown_began = false;		//停止倒计时
	for (int i = 0; i < question_count; ++i)
	{
		if (answer_list[i] == question_list_data[i].answer - 'A')
		{
			correct++;
		}
		else if (answer_list[i] != -1)
		{
			createAnswerInfoCard(flex_view, i, initial_delay);
			initial_delay += 200;
		}
		else
		{
			createAnswerInfoCard(flex_view, i, initial_delay);
			initial_delay += 200;
			blank++;
		}
	}
	if (correct == question_count)
	{
		lv_obj_del(flex_view);
		lv_label_set_text_fmt(lbl_score, "%d 分", correct * scorePerQuestion);
	}
	else if (blank == question_count)
		lv_label_set_text(lbl_score, "你没有作答");
	else
		lv_label_set_text_fmt(lbl_score, "%d 分", correct * scorePerQuestion);

	lv_obj_t* btn1 = lv_btn_create(scr_statistic);

	lv_obj_set_style_text_font(btn1, icon_font, 0);
	lv_obj_t* lbl = lv_label_create(btn1);
	lv_label_set_text(lbl, LV_SYMBOL_LEFT);
	lv_obj_align(btn1, LV_ALIGN_TOP_LEFT, 5, 5);
	lv_obj_add_event_cb(btn1, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			switchScreen(buildMainWindow(), LV_SCR_LOAD_ANIM_MOVE_RIGHT, true);
		}
		}, LV_EVENT_ALL, NULL);
	lv_obj_fade_in(btn1, 500, 4000);
	switchScreen(scr_statistic, LV_SCR_LOAD_ANIM_MOVE_LEFT, true);
}
void handIn()		//交卷
{
	if (isSubmitPressed == true)
	{
		alert_bug();
		return;
	}
	isSubmitPressed = true;
	lv_obj_send_event(obj_answerSheet, LV_EVENT_CLICKED, NULL);
	int blank = 0;		//检查是否有未作答的题目
	for (int i = 0; i < question_count; ++i)
	{
		if (answer_list[i] == -1)
		{
			blank++;
		}
	}
	char buf[100];
	if (blank > 0)
		sprintf(buf, "你还有%d道题目未作答，确认交卷？", blank);
	else
		sprintf(buf, "确认交卷？此操作不能撤销");
	msgbox_yn(buf, "交卷！", "当我没说", [](lv_event_t* e) {reallyHandIn(); }, [](lv_event_t* e) {msgbox("提示", "如果答题卡没有正常关闭，请先单击答题卡空白处，再单击答题卡外的任意区域"); isSubmitPressed = false; });
}
lv_obj_t* add_question_card(question_show_t* show, lv_obj_t* scr)
{
	lv_obj_t* card = lv_obj_create(scr);
	lv_obj_set_size(card, lv_pct(90), lv_pct(85));
	lv_obj_add_style(card, &style_card, 0);
	lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -10);

	lv_obj_t* question = lv_label_create(card);
	lv_label_set_text(question, "问题");
	lv_obj_add_style(question, &style_lbl_question, 0);
	lv_obj_align(question, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_set_width(question, lv_pct(90));
	lv_obj_set_height(question, lv_pct(25));
	lv_label_set_long_mode(question, LV_LABEL_LONG_WRAP);


	lv_obj_t* answerA = lv_button_create(card);
	lv_obj_add_style(answerA, &style_btn_question, 0);
	lv_obj_set_size(answerA, lv_pct(90), lv_pct(13));
	lv_obj_align(answerA, LV_ALIGN_TOP_MID, 0, lv_pct(30));
	lv_obj_t* answerA_label = lv_label_create(answerA);
	lv_label_set_text(answerA_label, "A");
	lv_obj_add_style(answerA_label, &style_lbl_choice, 0);
	lv_obj_center(answerA_label);

	lv_obj_t* answerB = lv_button_create(card);
	lv_obj_add_style(answerB, &style_btn_question, 0);
	lv_obj_set_size(answerB, lv_pct(90), lv_pct(13));
	lv_obj_align(answerB, LV_ALIGN_TOP_MID, 0, lv_pct(48));
	lv_obj_t* answerB_label = lv_label_create(answerB);
	lv_label_set_text(answerB_label, "B");
	lv_obj_add_style(answerB_label, &style_lbl_choice, 0);
	lv_obj_center(answerB_label);

	lv_obj_t* answerC = lv_button_create(card);
	lv_obj_add_style(answerC, &style_btn_question, 0);
	lv_obj_set_size(answerC, lv_pct(90), lv_pct(13));
	lv_obj_align(answerC, LV_ALIGN_TOP_MID, 0, lv_pct(66));
	lv_obj_t* answerC_label = lv_label_create(answerC);
	lv_label_set_text(answerC_label, "C");
	lv_obj_add_style(answerC_label, &style_lbl_choice, 0);
	lv_obj_center(answerC_label);

	lv_obj_t* answerD = lv_button_create(card);
	lv_obj_add_style(answerD, &style_btn_question, 0);
	lv_obj_set_size(answerD, lv_pct(90), lv_pct(13));
	lv_obj_align(answerD, LV_ALIGN_TOP_MID, 0, lv_pct(84));
	lv_obj_t* answerD_label = lv_label_create(answerD);
	lv_label_set_text(answerD_label, "D");
	lv_obj_add_style(answerD_label, &style_lbl_choice, 0);
	lv_obj_center(answerD_label);
	//transition
	static const lv_style_prop_t props[] = { LV_STYLE_BG_COLOR, 0 };
	static lv_style_transition_dsc_t trans_def;

	lv_style_transition_dsc_init(&trans_def, props, lv_anim_path_linear, 300, 0, NULL);

	lv_style_set_transition(&style_btn_genetic, &trans_def);
	lv_style_set_transition(&style_obj_right, &trans_def);
	lv_style_set_transition(&style_obj_wrong, &trans_def);
	lv_style_set_transition(&style_obj_selected, &trans_def);
	lv_obj_add_style(answerA, &style_btn_genetic, 0);
	lv_obj_add_style(answerB, &style_btn_genetic, 0);
	lv_obj_add_style(answerC, &style_btn_genetic, 0);
	lv_obj_add_style(answerD, &style_btn_genetic, 0);
	lv_obj_add_style(answerA, &style_obj_selected, LV_STATE_USER_1);
	lv_obj_add_style(answerB, &style_obj_selected, LV_STATE_USER_1);
	lv_obj_add_style(answerC, &style_obj_selected, LV_STATE_USER_1);
	lv_obj_add_style(answerD, &style_obj_selected, LV_STATE_USER_1);
	lv_obj_add_style(answerA, &style_obj_right, LV_STATE_USER_2);
	lv_obj_add_style(answerB, &style_obj_right, LV_STATE_USER_2);
	lv_obj_add_style(answerC, &style_obj_right, LV_STATE_USER_2);
	lv_obj_add_style(answerD, &style_obj_right, LV_STATE_USER_2);
	lv_obj_add_style(answerA, &style_obj_wrong, LV_STATE_USER_3);
	lv_obj_add_style(answerB, &style_obj_wrong, LV_STATE_USER_3);
	lv_obj_add_style(answerC, &style_obj_wrong, LV_STATE_USER_3);
	lv_obj_add_style(answerD, &style_obj_wrong, LV_STATE_USER_3);
	lv_obj_set_user_data(answerA, (void*)0);
	lv_obj_set_user_data(answerB, (void*)1);
	lv_obj_set_user_data(answerC, (void*)2);
	lv_obj_set_user_data(answerD, (void*)3);

	lv_event_cb_t cb = [](lv_event_t* e) {
		if (lv_obj_get_style_opa(lv_event_get_target_obj(e), 0) == 0)return;
		if (e->code == LV_EVENT_CLICKED)
		{
			lv_obj_t* parent = lv_obj_get_parent(lv_event_get_target_obj(e));
			for (int i = 1; i < 5; ++i)
			{
				lv_obj_t* child = lv_obj_get_child(parent, i);
				lv_obj_clear_state(child, LV_STATE_USER_1 | LV_STATE_USER_2 | LV_STATE_USER_3);
			}
			if (showAnswerImmediately)
			{
				for (int i = 1; i < 5; ++i)
				{
					lv_obj_t* child = lv_obj_get_child(parent, i);
					if ((int)lv_obj_get_user_data(child) == (*(((question_show_t*)lv_event_get_user_data(e))->answer) - 'A'))
						lv_obj_add_state(child, LV_STATE_USER_2);
					else if (child == lv_event_get_target_obj(e))
						lv_obj_add_state(child, LV_STATE_USER_3);
				}
			}
			else
			{
				lv_obj_add_state(lv_event_get_target_obj(e), LV_STATE_USER_1);
			}
			*((question_show_t*)lv_event_get_user_data(e))->selected_answer = (int)lv_obj_get_user_data(lv_event_get_target_obj(e));
			if (autoNext)
				moveEverything(-1);
		}
		};
	lv_obj_add_event_cb(answerA, cb, LV_EVENT_ALL, show);
	lv_obj_add_event_cb(answerB, cb, LV_EVENT_ALL, show);
	lv_obj_add_event_cb(answerC, cb, LV_EVENT_ALL, show);
	lv_obj_add_event_cb(answerD, cb, LV_EVENT_ALL, show);
	//添加题号,child=5
	lv_obj_t* obj_label = lv_label_create(card);
	lv_obj_set_style_text_font(obj_label, medium_font, 0);
	lv_label_set_text(obj_label, "1.");
	lv_obj_align(obj_label, LV_ALIGN_TOP_LEFT, 10, 0);

	lv_obj_add_event_cb(card, [](lv_event_t* e) {
		if (lv_obj_get_style_opa(lv_event_get_target_obj(e), 0) == 0)return;
		if (e->code == LV_EVENT_CLICKED)
		{
			if (((question_show_t*)lv_event_get_user_data(e))->current_pos == 1)
			{
				moveEverything(1);
			}
			else if (((question_show_t*)lv_event_get_user_data(e))->current_pos == 3)
			{
				moveEverything(-1);
			}
		}
		}, LV_EVENT_ALL, show);
	return card;
}
lv_obj_t* add_answerSheetTable(int n)
{
	int rows = sqrt(n);
	if (rows * rows < n)rows++;
	static lv_coord_t row_dsc[100];
	for (int i = 0; i < rows; ++i)
	{
		row_dsc[i] = LV_GRID_FR(1);
	}
	row_dsc[rows] = LV_GRID_TEMPLATE_LAST;
	lv_obj_t* cont = lv_obj_create(obj_answerSheet);
	lv_obj_clear_flag(cont, LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_set_style_grid_column_dsc_array(cont, row_dsc, 0);
	lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);
	lv_obj_set_size(cont, lv_pct(90), lv_pct(70));
	lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, lv_pct(10));
	lv_obj_set_layout(cont, LV_LAYOUT_GRID);
	lv_obj_set_style_text_font(cont, large_font, 0);
	lv_obj_set_style_border_opa(cont, 0, 0);
	lv_obj_set_style_bg_opa(cont, 0, 0);
	//transition

	static const lv_style_prop_t props[] = { LV_STYLE_BG_COLOR, 0 };
	static lv_style_transition_dsc_t trans_def;

	lv_style_transition_dsc_init(&trans_def, props, lv_anim_path_linear, 300, 0, NULL);

	lv_style_set_transition(&style_btn_genetic, &trans_def);
	lv_style_set_transition(&style_obj_right, &trans_def);
	lv_style_set_transition(&style_obj_wrong, &trans_def);
	lv_style_set_transition(&style_obj_selected, &trans_def);
	lv_style_set_transition(&style_obj_marked, &trans_def);


	lv_obj_t* label;
	lv_obj_t* obj;
	uint32_t i;
	for (i = 0; i < n; i++) {
		uint8_t col = i % rows;
		uint8_t row = i / rows;

		obj = lv_btn_create(cont);
		lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, col, 1,
			LV_GRID_ALIGN_STRETCH, row, 1);

		label = lv_label_create(obj);
		lv_label_set_text_fmt(label, "%d", i + 1);
		lv_obj_center(label);
		lv_obj_set_user_data(obj, (void*)i);
		lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
		lv_obj_add_event_cb(obj, [](lv_event_t* e) {
			if (e->code == LV_EVENT_CLICKED)
			{
				if (question_show_current_index != (int)lv_obj_get_user_data(lv_event_get_target_obj(e)))
				{
					switchToQuestion((int)lv_obj_get_user_data(lv_event_get_target_obj(e)));
				}
				else
				{
					if (lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_USER_4))
						lv_obj_clear_state(lv_event_get_target_obj(e), LV_STATE_USER_4);
					else
						lv_obj_add_state(lv_event_get_target_obj(e), LV_STATE_USER_4);
				}
			}
			}, LV_EVENT_ALL, NULL);
		lv_obj_add_style(obj, &style_btn_genetic, 0);
		lv_obj_add_style(obj, &style_obj_selected, LV_STATE_USER_1);
		lv_obj_add_style(obj, &style_obj_right, LV_STATE_USER_2);
		lv_obj_add_style(obj, &style_obj_wrong, LV_STATE_USER_3);
		lv_obj_add_style(obj, &style_obj_marked, LV_STATE_USER_4);
	}
	return cont;
}
void updateQuestionTable(lv_obj_t* tbl)
{
	lv_obj_t* obj_button;
	for (int i = 0; i < question_count; i++) {
		obj_button = lv_obj_get_child(tbl, i);
		if (showAnswerImmediately)
		{
			lv_obj_clear_state(obj_button, LV_STATE_USER_1 | LV_STATE_USER_2 | LV_STATE_USER_3);
			if (answer_list[i] != -1)
			{
				if (answer_list[i] == question_list_data[i].answer - 'A')
					lv_obj_add_state(obj_button, LV_STATE_USER_2);
				else
					lv_obj_add_state(obj_button, LV_STATE_USER_3);
			}
		}
		else
		{
			if (answer_list[i] == -1)
				lv_obj_clear_state(obj_button, LV_STATE_USER_1);
			else
				lv_obj_add_state(obj_button, LV_STATE_USER_1);
		}
	}
}
lv_obj_t* add_answer_sheet(lv_obj_t* scr)
{
	lv_obj_t* obj_box = lv_obj_create(scr);
	lv_obj_set_size(obj_box, my_pct_to_px(95, g_WindowWidth), my_pct_to_px(80, g_WindowHeight));
	lv_obj_add_style(obj_box, &style_card, 0);
	lv_obj_align(obj_box, LV_ALIGN_TOP_MID, 0, my_pct_to_px(5, g_WindowHeight));
	//倒计时
	lv_obj_t* obj_label = lv_label_create(obj_box);
	lv_font_t* font = lv_freetype_font_create("msyhl.ttc", 60, LV_FREETYPE_FONT_STYLE_NORMAL);
	lv_obj_set_style_text_font(obj_label, font, 0);
	lv_label_set_text(obj_label, "剩余 00:00");
	lv_obj_align(obj_label, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_timer_create([](lv_timer_t* timer) {
		if (lv_obj_is_valid((lv_obj_t*)lv_timer_get_user_data(timer)) == false)
		{
			lv_timer_delete(timer);
			return;
		}
		if (time_remain <= 0)
		{
			lv_label_set_text((lv_obj_t*)lv_timer_get_user_data(timer), "时间到！");
			if (countdown_began == false)
				return;
		}
		else
		{
			int minute = time_remain / 60;
			int second = time_remain % 60;
			static char buf[100];
			sprintf(buf, "剩余 %02d:%02d", minute, second);
			lv_label_set_text((lv_obj_t*)lv_timer_get_user_data(timer), buf);
		}
		if (time_remain == 0 && countdown_began == true)
		{
			countdown_began = false;
			reallyHandIn();				// 时间到了，自动交卷
		}
		else if (countdown_began == true)
			--time_remain;
		}, 1000, obj_label);
	lbl_time_remain = obj_label;
	//展开动画
	lv_obj_add_event_cb(obj_box, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED && *(bool*)(lv_event_get_user_data(e)) == false)
		{
			*(bool*)(lv_event_get_user_data(e)) = true;
			lv_obj_set_size(lv_event_get_target_obj(e), my_pct_to_px(80, g_WindowWidth), my_pct_to_px(80, g_WindowHeight));
			lv_anim_t a;
			lv_anim_init(&a);
			lv_anim_set_var(&a, lv_event_get_target(e));
			lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
			lv_anim_set_values(&a, my_pct_to_px(80, g_WindowWidth), my_pct_to_px(95, g_WindowWidth));
			lv_anim_set_time(&a, 600);
			lv_anim_set_path_cb(&a, my_anim_path_ease_out);
			lv_anim_start(&a);
			lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
			lv_anim_set_values(&a, my_pct_to_px(-70, g_WindowHeight), my_pct_to_px(5, g_WindowHeight));
			lv_anim_start(&a);
			lv_timer_create([](lv_timer_t* timer) {
				updateQuestionTable(answerSheetTable);
				lv_timer_delete(timer);
				}, 600, NULL);
		}
		else if ((e->code == LV_EVENT_DEFOCUSED) && *(bool*)(lv_event_get_user_data(e)) == true)
		{
			*(bool*)(lv_event_get_user_data(e)) = false;
			lv_obj_set_size(lv_event_get_target_obj(e), my_pct_to_px(95, g_WindowWidth), my_pct_to_px(80, g_WindowHeight));
			lv_anim_t a;
			lv_anim_init(&a);
			lv_anim_set_var(&a, lv_event_get_target(e));
			lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
			lv_anim_set_values(&a, my_pct_to_px(95, g_WindowWidth), my_pct_to_px(80, g_WindowWidth));
			lv_anim_set_time(&a, 600);
			lv_anim_set_path_cb(&a, my_anim_path_ease_out);
			lv_anim_start(&a);
			lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
			lv_anim_set_values(&a, my_pct_to_px(5, g_WindowHeight), my_pct_to_px(-70, g_WindowHeight));
			lv_anim_start(&a);
		}
		}, LV_EVENT_ALL, &answerSheetVisable);
	obj_label = lv_label_create(obj_box);
	lv_obj_set_style_text_font(obj_label, font, 0);
	lv_label_set_text(obj_label, "答题卡");
	lv_obj_align(obj_label, LV_ALIGN_TOP_MID, 0, 0);
	//交卷按钮，在右下角
	lv_obj_t* obj_btn = lv_btn_create(obj_box);
	lv_obj_set_size(obj_btn, lv_pct(12), lv_pct(8));
	lv_obj_align(obj_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
	lv_obj_add_style(obj_btn, &style_obj_right, 0);
	lv_obj_t* obj_label_btn = lv_label_create(obj_btn);
	lv_obj_set_style_text_font(obj_label_btn, large_font, 0);
	lv_label_set_text(obj_label_btn, "交卷");
	lv_obj_center(obj_label_btn);
	lv_obj_clear_flag(obj_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_add_event_cb(obj_btn, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			handIn();
		}
		}, LV_EVENT_ALL, NULL);
	return obj_box;
}
void executeOnFullscreen()
{
	if (lv_obj_is_valid(question_cards[0]))
		updateQuestionCards(true);
	if (lv_obj_is_valid(obj_answerSheet))
	{
		if (answerSheetVisable == false)
		{
			lv_obj_set_size(obj_answerSheet, my_pct_to_px(80, g_WindowWidth), my_pct_to_px(80, g_WindowHeight));
			lv_obj_align(obj_answerSheet, LV_ALIGN_TOP_MID, 0, my_pct_to_px(-70, g_WindowHeight));
		}
		else
		{
			lv_obj_set_size(obj_answerSheet, my_pct_to_px(95, g_WindowWidth), my_pct_to_px(80, g_WindowHeight));
			lv_obj_align(obj_answerSheet, LV_ALIGN_TOP_MID, 0, my_pct_to_px(5, g_WindowHeight));
		}
	}
}
void app_init()
{
	setFullScreen(true);
	open_database();
	loadConfig();
	GUI_Init();
	init_style();
	setDisplayMode(cfg_darkMode);
	iconv_G2U = iconv_open("UTF-8", "GBK");
	iconv_U2G = iconv_open("GBK", "UTF-8");
	iconv(iconv_G2U, NULL, NULL, NULL, NULL);
	iconv(iconv_U2G, NULL, NULL, NULL, NULL);
}
void init_question_cards(int n, lv_obj_t* scr)//随机抽题并初始化屏幕上的卡片
{
	//初始化卡片
	memset(question_cards_data, 0, sizeof(question_cards_data));
	clearAnswerList();
	clearQuestionList();

	getQuestionsRandomly(n);
	for (int i = 0; i < 4; ++i)
	{
		question_cards[i] = add_question_card(&question_cards_data[i], scr);
	}
	for (int i = 0; i < 4; ++i)
	{
		question_cards_data[i].visable = false;
		setQuestionCardPos(i, i + 1);
		if (i == 0)		//从i=1开始存储第一题
			continue;
		if (i > question_count)
			continue;
		question_cards_data[i].selected_answer = &answer_list[i - 1];
		applyQuestionToCard(question_cards[i], &question_list_data[i - 1], &question_cards_data[i], i - 1);
		question_cards_data[i].visable = true;
	}
	question_show_current_index = 0;
	question_card_center_index = 1;
	updateQuestionCards(false);
}
void addPrepareWindow(lv_obj_t* scr)
{
	lv_font_t* font = lv_freetype_font_create("msyhl.ttc", 60, LV_FREETYPE_FONT_STYLE_NORMAL);
	lv_font_t* font1 = lv_freetype_font_create("msyhl.ttc", 120, LV_FREETYPE_FONT_STYLE_NORMAL);
	lv_font_t* font2 = lv_freetype_font_create("msyh.ttc", 180, LV_FREETYPE_FONT_STYLE_NORMAL);
	lv_font_t* font3 = lv_freetype_font_create("msyhl.ttc", 50, LV_FREETYPE_FONT_STYLE_NORMAL);
	lv_obj_t* obj_shadow = lv_obj_create(scr);
	lv_obj_set_size(obj_shadow, lv_pct(100), lv_pct(100));
	lv_obj_set_style_radius(obj_shadow, 0, 0);
	lv_obj_set_style_opa(obj_shadow, 128, 0);
	lv_obj_set_style_bg_color(obj_shadow, lv_color_black(), 0);
	//下面是准备窗口的box
	lv_obj_t* obj_box = lv_obj_create(scr);
	lv_obj_set_size(obj_box, lv_pct(85), lv_pct(85));
	lv_obj_add_style(obj_box, &style_card, 0);
	//画一条分割线
	lv_obj_t* obj_line = lv_line_create(obj_box);
	static lv_point_t line_points[] = { {0, 0}, {8000, 0} };
	lv_line_set_points(obj_line, (lv_point_precise_t*)line_points, 2);
	lv_obj_add_style(obj_line, &style_line, 0);
	lv_obj_set_style_line_width(obj_line, 2, 0);
	lv_obj_set_width(obj_line, lv_pct(70));
	lv_obj_align(obj_line, LV_ALIGN_TOP_MID, 0, lv_pct(20));
	//下面是标题
	lv_obj_t* obj_label = lv_label_create(obj_box);
	lv_obj_set_style_text_font(obj_label, font1, 0);
	lv_label_set_text(obj_label, "请准备");
	lv_obj_align(obj_label, LV_ALIGN_TOP_MID, 0, 0);
	//下面是倒计时预览
	lv_obj_t* lbl_countdown_preview = lv_label_create(obj_box);
	lv_obj_set_style_text_font(lbl_countdown_preview, font2, 0);
	lv_label_set_text_fmt(lbl_countdown_preview, "%02d:%02d", time_remain / 60, time_remain % 60);
	lv_obj_align(lbl_countdown_preview, LV_ALIGN_CENTER, 0, lv_pct(-5));
	//下面是题量
	obj_label = lv_label_create(obj_box);
	lv_obj_set_style_text_font(obj_label, font3, 0);
	lv_label_set_text_fmt(obj_label, "共%d道题，每道题%d分", question_count, scorePerQuestion);
	lv_obj_align(obj_label, LV_ALIGN_CENTER, 0, lv_pct(25));
	//下面是按钮
	lv_obj_t* obj_btn = lv_btn_create(obj_box);
	lv_obj_set_size(obj_btn, lv_pct(60), LV_SIZE_CONTENT);
	lv_obj_align(obj_btn, LV_ALIGN_BOTTOM_MID, 0, lv_pct(-5));
	lv_obj_t* obj_label_btn = lv_label_create(obj_btn);
	lv_obj_set_style_text_font(obj_label_btn, font, 0);
	lv_label_set_text(obj_label_btn, "开始考试");
	lv_obj_set_size(obj_label_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_center(obj_label_btn);
	lv_obj_align(obj_label_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_add_event_cb(obj_btn, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			lv_obj_fade_out((lv_obj_t*)lv_event_get_user_data(e), 500, 0);
			lv_obj_fall_down(lv_obj_get_parent(lv_event_get_target_obj(e)), 200, 500, 0);
			lv_obj_delete_delayed(lv_obj_get_parent(lv_event_get_target_obj(e)), 500);
			lv_obj_delete_delayed((lv_obj_t*)lv_event_get_user_data(e), 500);
			lv_label_set_text_fmt(lv_obj_get_child(lv_obj_get_parent(lv_event_get_target_obj(e)), 2), "%02d:%02d", (time_remain - 1) / 60, (time_remain - 1) % 60);
			countdown_began = true;
			lv_obj_add_state(obj_answerSheet, LV_STATE_FOCUSED);
			lv_timer_create([](lv_timer_t* timer) {
				lv_obj_send_event(obj_answerSheet, LV_EVENT_DEFOCUSED, NULL);
				lv_timer_delete(timer);
				}, 1000, lbl_time_remain);
			lv_obj_fade_in(lbl_time_remain, 1000, 2000);
		}
		}, LV_EVENT_ALL, obj_shadow);
	lv_obj_center(obj_box);
}
int getQuestionsByKeyword(const char* keywd)
{
	int rc;
	const char* sql;
	static char keywd_buf[5000] = { 0 };
	sqlite3_stmt* stmt;
	clearQuestionList();
	if (keywd != NULL)
		strcpy(keywd_buf, keywd);
	else
		keywd = keywd_buf;
	question_count = 0;
	sql = "SELECT * FROM question";		//like子句不好用，这里查询全部
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			int id = sqlite3_column_int(stmt, 0);
			const unsigned char* question = sqlite3_column_text(stmt, 1);
			const unsigned char* answerA = sqlite3_column_text(stmt, 2);
			const unsigned char* answerB = sqlite3_column_text(stmt, 3);
			const unsigned char* answerC = sqlite3_column_text(stmt, 4);
			const unsigned char* answerD = sqlite3_column_text(stmt, 5);
			const unsigned char* answer = sqlite3_column_text(stmt, 6);
			if (keywd != NULL)
				if (strstr((char*)question, keywd) == NULL
					&& strstr((char*)answerA, keywd) == NULL
					&& strstr((char*)answerB, keywd) == NULL
					&& strstr((char*)answerC, keywd) == NULL
					&& strstr((char*)answerD, keywd) == NULL)
					continue;
			question_list_data[question_count].id = id;
			question_list_data[question_count].question = (char*)malloc(strlen((char*)question) + 1);
			strcpy(question_list_data[question_count].question, (char*)question);
			question_list_data[question_count].answerA = (char*)malloc(strlen((char*)answerA) + 1);
			strcpy(question_list_data[question_count].answerA, (char*)answerA);
			question_list_data[question_count].answerB = (char*)malloc(strlen((char*)answerB) + 1);
			strcpy(question_list_data[question_count].answerB, (char*)answerB);
			question_list_data[question_count].answerC = (char*)malloc(strlen((char*)answerC) + 1);
			strcpy(question_list_data[question_count].answerC, (char*)answerC);
			question_list_data[question_count].answerD = (char*)malloc(strlen((char*)answerD) + 1);
			strcpy(question_list_data[question_count].answerD, (char*)answerD);
			question_list_data[question_count].answer = ((char*)answer)[0];
			question_count++;
		}
	}
	sqlite3_finalize(stmt);
	return question_count;
}
void updateFlexView(lv_obj_t* o)
{
	static lv_obj_t* last_obj = o;
	static lv_obj_t* flex_view = NULL;
	if (lv_obj_is_valid(flex_view) == true)
	{
		lv_obj_fade_out(flex_view, 300, 0);
		lv_obj_delete_delayed(flex_view, 300);
	}
	if (o == NULL)
	{
		o = last_obj;
	}
	else
	{
		last_obj = o;
	}
	if (lv_obj_is_valid(o) == false)
		return;
	getQuestionsByKeyword(lv_textarea_get_text(o));
	flex_view = lv_obj_create(lv_obj_get_parent(o));
	lv_obj_set_size(flex_view, lv_pct(90), lv_pct(80));
	lv_obj_align(flex_view, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_set_flex_flow(flex_view, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_bg_opa(flex_view, 0, 0);
	for (int i = 0; i < question_count; ++i)
	{
		lv_obj_t* card = createAnswerInfoCard(flex_view, i, i * 200, false);
		lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_add_event_cb(card, [](lv_event_t* e) {
			if (e->code == LV_EVENT_CLICKED)
			{
				void msgbox_question_updateOrIns(int id);
				msgbox_question_updateOrIns((int)lv_event_get_user_data(e));
			}
			}, LV_EVENT_ALL, (void*)i);
	}
	lv_obj_fade_in(flex_view, 400, 0);
}
void msgbox_question_updateOrIns(int id)
{
	//id==-1:增加
	lv_obj_t* obj_box = lv_obj_create(lv_scr_act());
	lv_obj_set_size(obj_box, lv_pct(80), lv_pct(80));
	lv_obj_center(obj_box);
	lv_obj_add_style(obj_box, &style_card, 0);
	//右上角的关闭按钮
	lv_obj_t* btn_close = lv_btn_create(obj_box);
	lv_obj_set_size(btn_close, 50, 50);
	lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
	lv_obj_add_style(btn_close, &style_btn_close, 0);
	lv_obj_t* label_close = lv_label_create(btn_close);
	lv_obj_set_style_text_font(label_close, icon_font, 0);
	lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
	lv_obj_center(label_close);
	lv_obj_add_event_cb(btn_close, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			lv_obj_fall_down(lv_obj_get_parent(lv_event_get_target_obj(e)), 100, 300, 0);
			lv_obj_delete_delayed(lv_obj_get_parent(lv_event_get_target_obj(e)), 300);
		}
		}, LV_EVENT_ALL, NULL);

	//编辑题目
	lv_obj_t* lbl_title = lv_label_create(obj_box);
	lv_obj_set_style_text_font(lbl_title, large_font, 0);
	if (id == -1)
		lv_label_set_text(lbl_title, "新增题目");
	else
		lv_label_set_text(lbl_title, "编辑题目");
	lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 0);
	// 一个文本框用来编辑题目
	lv_obj_t* ta = lv_textarea_create(obj_box);
	lv_textarea_set_text(ta, "");
	lv_textarea_set_placeholder_text(ta, "请输入题目");
	lv_obj_set_width(ta, lv_pct(85));
	lv_obj_set_height(ta, lv_pct(30));
	lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 70);
	if (id != -1)
		lv_textarea_set_text(ta, question_list_data[id].question);

	// 一个文本框用来编辑选项A
	lv_obj_t* taA = lv_textarea_create(obj_box);
	lv_textarea_set_text(taA, "");
	lv_textarea_set_placeholder_text(taA, "选项A");
	lv_obj_set_width(taA, lv_pct(85));
	lv_obj_set_height(taA, lv_pct(10));
	lv_obj_align(taA, LV_ALIGN_TOP_MID, 0, lv_pct(40));
	if (id != -1)
		lv_textarea_set_text(taA, question_list_data[id].answerA);

	// 一个文本框用来编辑选项B
	lv_obj_t* taB = lv_textarea_create(obj_box);
	lv_textarea_set_text(taB, "");
	lv_textarea_set_placeholder_text(taB, "选项B");
	lv_obj_set_width(taB, lv_pct(85));
	lv_obj_set_height(taB, lv_pct(10));
	lv_obj_align(taB, LV_ALIGN_TOP_MID, 0, lv_pct(50 + 1));
	if (id != -1)
		lv_textarea_set_text(taB, question_list_data[id].answerB);

	// 一个文本框用来编辑选项C
	lv_obj_t* taC = lv_textarea_create(obj_box);
	lv_textarea_set_text(taC, "");
	lv_textarea_set_placeholder_text(taC, "选项C");
	lv_obj_set_width(taC, lv_pct(85));
	lv_obj_set_height(taC, lv_pct(10));
	lv_obj_align(taC, LV_ALIGN_TOP_MID, 0, lv_pct(60 + 2));
	if (id != -1)
		lv_textarea_set_text(taC, question_list_data[id].answerC);

	// 一个文本框用来编辑选项D
	lv_obj_t* taD = lv_textarea_create(obj_box);
	lv_textarea_set_text(taD, "");
	lv_textarea_set_placeholder_text(taD, "选项D");
	lv_obj_set_width(taD, lv_pct(85));
	lv_obj_set_height(taD, lv_pct(10));
	lv_obj_align(taD, LV_ALIGN_TOP_MID, 0, lv_pct(70 + 3));
	if (id != -1)
		lv_textarea_set_text(taD, question_list_data[id].answerD);

	//一个选择框用来选择答案
	lv_obj_t* ddlist = lv_dropdown_create(obj_box);
	lv_obj_set_width(ddlist, lv_pct(20));
	lv_dropdown_set_options(ddlist, "A\nB\nC\nD");
	lv_dropdown_set_selected(ddlist, 0);
	lv_obj_t* lbl_ddlist = lv_label_create(obj_box);
	lv_obj_set_style_text_font(lbl_ddlist, medium_font, 0);
	lv_label_set_text(lbl_ddlist, "正确答案：");
	lv_obj_align(lbl_ddlist, LV_ALIGN_BOTTOM_LEFT, 0, 0);
	lv_obj_align_to(ddlist, lbl_ddlist, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
	if (id != -1)
		lv_dropdown_set_selected(ddlist, question_list_data[id].answer - 'A');
	attachClipBoardToolToTextarea(ta);
	attachClipBoardToolToTextarea(taA);
	attachClipBoardToolToTextarea(taB);
	attachClipBoardToolToTextarea(taC);
	attachClipBoardToolToTextarea(taD);
	//一个按钮用来删除
	lv_obj_t* btn_delete = lv_btn_create(obj_box);
	lv_obj_set_width(btn_delete, lv_pct(15));
	lv_obj_add_style(btn_delete, &style_obj_wrong, 0);
	lv_obj_t* lbl_delete = lv_label_create(btn_delete);
	lv_obj_set_style_text_font(lbl_delete, medium_font, 0);
	lv_label_set_text(lbl_delete, "删除");
	lv_obj_center(lbl_delete);
	lv_obj_align(btn_delete, LV_ALIGN_BOTTOM_RIGHT, lv_pct(-30), 0);
	lv_obj_add_event_cb(btn_delete, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			int id = (int)lv_event_get_user_data(e);
			if (id != -1)
			{
				char buf[100];
				sprintf(buf, "DELETE FROM question WHERE id=%d", question_list_data[id].id);
				sqlite3_exec(db, buf, NULL, NULL, NULL);
			}
			lv_obj_fall_down(lv_obj_get_parent(lv_event_get_target_obj(e)), 100, 300, 0);
			lv_obj_delete_delayed(lv_obj_get_parent(lv_event_get_target_obj(e)), 300);
			updateFlexView(NULL);
		}
		}, LV_EVENT_ALL, (void*)id);

	//一个按钮用来保存
	lv_obj_t* btn_save = lv_btn_create(obj_box);
	lv_obj_pop_up(obj_box, 100, 300, 0);
	lv_obj_set_width(btn_save, lv_pct(15));
	lv_obj_add_style(btn_save, &style_obj_right, 0);
	lv_obj_t* lbl_save = lv_label_create(btn_save);
	lv_obj_set_style_text_font(lbl_save, medium_font, 0);
	lv_label_set_text(lbl_save, "保存");
	lv_obj_center(lbl_save);
	lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
	struct tmp_userdata_t {
		lv_obj_t* ta, * taA, * taB, * taC, * taD, * ddlist;
		int id;
	};
	static struct tmp_userdata_t tmp_userdata;
	tmp_userdata.ta = ta;
	tmp_userdata.taA = taA;
	tmp_userdata.taB = taB;
	tmp_userdata.taC = taC;
	tmp_userdata.taD = taD;
	tmp_userdata.ddlist = ddlist;
	tmp_userdata.id = id;
	lv_obj_add_event_cb(btn_save, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			struct tmp_userdata_t* tmp_userdata = (struct tmp_userdata_t*)lv_event_get_user_data(e);
			char buf[3000];
			if (tmp_userdata->id == -1)
			{
				sprintf(buf, "INSERT INTO question (id,content,answer_A,answer_B,answer_C,answer_D,answer) VALUES (%d,'%s','%s','%s','%s','%s','%c')", ++question_count,
					lv_textarea_get_text(tmp_userdata->ta), lv_textarea_get_text(tmp_userdata->taA), lv_textarea_get_text(tmp_userdata->taB), lv_textarea_get_text(tmp_userdata->taC), lv_textarea_get_text(tmp_userdata->taD), lv_dropdown_get_selected(tmp_userdata->ddlist) + 'A');
			}
			else
			{
				sprintf(buf, "UPDATE question SET content='%s',answer_A='%s',answer_B='%s',answer_C='%s',answer_D='%s',answer='%c' WHERE id=%d", lv_textarea_get_text(tmp_userdata->ta), lv_textarea_get_text(tmp_userdata->taA), lv_textarea_get_text(tmp_userdata->taB), lv_textarea_get_text(tmp_userdata->taC), lv_textarea_get_text(tmp_userdata->taD), lv_dropdown_get_selected(tmp_userdata->ddlist) + 'A', question_list_data[tmp_userdata->id].id);
			}
			sqlite3_exec(db, buf, NULL, NULL, NULL);
			lv_obj_fall_down(lv_obj_get_parent(lv_event_get_target_obj(e)), 100, 300, 0);
			lv_obj_delete_delayed(lv_obj_get_parent(lv_event_get_target_obj(e)), 300);
			updateFlexView(NULL);
		}
		}, LV_EVENT_ALL, &tmp_userdata);
}
void msgboxQuestionManager()
{
	lv_obj_t* obj_box = lv_obj_create(lv_scr_act());
	lv_obj_set_size(obj_box, lv_pct(85), lv_pct(85));
	lv_obj_center(obj_box);
	lv_obj_add_style(obj_box, &style_card, 0);

	//右上角的关闭按钮
	lv_obj_t* btn_close = lv_btn_create(obj_box);
	lv_obj_set_size(btn_close, 50, 50);
	lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
	lv_obj_add_style(btn_close, &style_btn_close, 0);
	lv_obj_t* label_close = lv_label_create(btn_close);
	lv_obj_set_style_text_font(label_close, icon_font, 0);
	lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
	lv_obj_center(label_close);
	lv_obj_add_event_cb(btn_close, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			lv_obj_fall_down(lv_obj_get_parent(lv_event_get_target_obj(e)), 100, 300, 0);
			lv_obj_delete_delayed(lv_obj_get_parent(lv_event_get_target_obj(e)), 300);
		}
		}, LV_EVENT_ALL, NULL);

	//窗口标题
	lv_obj_t* label_title = lv_label_create(obj_box);
	lv_obj_set_style_text_font(label_title, large_font, 0);
	lv_label_set_text(label_title, "题库管理");
	lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 0);
	// 一个文本框用来查询题目
	//创建文本框
	lv_obj_t* ta = lv_textarea_create(obj_box);
	lv_textarea_set_text(ta, "");
	lv_textarea_set_placeholder_text(ta, "请输入关键词，或留空以查询全部");
	lv_obj_set_width(ta, lv_pct(85));
	lv_obj_set_height(ta, 60);
	lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 0, 60);
	attachClipBoardToolToTextarea(ta);
	//在它右边创建一个按钮
	lv_obj_t* btn = lv_btn_create(obj_box);
	lv_obj_set_width(btn, 50);
	lv_obj_set_height(btn, 50);
	lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -150, 60);
	lv_obj_set_style_text_font(btn, icon_font, 0);
	lv_obj_t* lbl = lv_label_create(btn);
	lv_label_set_text(lbl, LV_SYMBOL_REFRESH);
	lv_obj_add_event_cb(btn, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			updateFlexView((lv_obj_t*)lv_event_get_user_data(e));
		}
		}, LV_EVENT_ALL, ta);
	btn = lv_btn_create(obj_box);	//新建
	lv_obj_set_width(btn, 50);
	lv_obj_set_height(btn, 50);
	lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -70, 60);
	lv_obj_set_style_text_font(btn, icon_font, 0);
	lbl = lv_label_create(btn);
	lv_label_set_text(lbl, LV_SYMBOL_PLUS);
	lv_obj_add_event_cb(btn, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			msgbox_question_updateOrIns(-1);
		}
		}, LV_EVENT_ALL, NULL);

	lv_obj_pop_up(obj_box, 100, 300, 0);

}
lv_obj_t* buildSettingsWindow()
{
	char buf[100];
	lv_obj_t* scr = lv_obj_create(NULL);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_t* obj_box = lv_obj_create(scr);
	lv_obj_set_size(obj_box, lv_pct(70), lv_pct(70));
	lv_obj_center(obj_box);

	lv_obj_t* lbl_title = lv_label_create(obj_box);
	lv_obj_set_style_text_font(lbl_title, large_font, 0);
	lv_label_set_text(lbl_title, "系统设置");
	lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 0);

	lv_obj_t* lbl_question_count = lv_label_create(obj_box);
	lv_label_set_text(lbl_question_count, "考试题量：");
	lv_obj_align(lbl_question_count, LV_ALIGN_TOP_LEFT, 0, lv_pct(10));
	lv_obj_t* ta_question_count = lv_textarea_create(obj_box);
	sprintf(buf, "%d", cfg_question_count);
	lv_textarea_set_text(ta_question_count, buf);
	lv_textarea_set_placeholder_text(ta_question_count, "请输入题量");
	lv_obj_set_width(ta_question_count, lv_pct(60));
	lv_obj_set_height(ta_question_count, 70);
	lv_obj_align(ta_question_count, LV_ALIGN_TOP_RIGHT, 0, lv_pct(10));

	lv_obj_t* lbl_scorePerQuestion = lv_label_create(obj_box);
	lv_label_set_text(lbl_scorePerQuestion, "每题分数：");
	lv_obj_align(lbl_scorePerQuestion, LV_ALIGN_TOP_LEFT, 0, lv_pct(30));
	lv_obj_t* ta_scorePerQuestion = lv_textarea_create(obj_box);
	sprintf(buf, "%d", cfg_scorePerQuestion);
	lv_textarea_set_text(ta_scorePerQuestion, buf);
	lv_textarea_set_placeholder_text(ta_scorePerQuestion, "请输入每题分数");
	lv_obj_set_width(ta_scorePerQuestion, lv_pct(60));
	lv_obj_set_height(ta_scorePerQuestion, 70);
	lv_obj_align(ta_scorePerQuestion, LV_ALIGN_TOP_RIGHT, 0, lv_pct(30));

	lv_obj_t* lbl_examTime = lv_label_create(obj_box);
	lv_label_set_text(lbl_examTime, "考试时间：");
	lv_obj_align(lbl_examTime, LV_ALIGN_TOP_LEFT, 0, lv_pct(50));
	lv_obj_t* ta_examTime = lv_textarea_create(obj_box);
	sprintf(buf, "%d", cfg_examTime / 60);
	lv_textarea_set_text(ta_examTime, buf);
	lv_textarea_set_placeholder_text(ta_examTime, "请输入考试时间（分钟）");
	lv_obj_set_width(ta_examTime, lv_pct(60));
	lv_obj_set_height(ta_examTime, 70);
	lv_obj_align(ta_examTime, LV_ALIGN_TOP_RIGHT, 0, lv_pct(50));

	//左下角管理题库按钮
	lv_obj_t* btn_question_manager = lv_btn_create(obj_box);
	lv_obj_set_size(btn_question_manager, lv_pct(15), lv_pct(8));
	lv_obj_align(btn_question_manager, LV_ALIGN_BOTTOM_LEFT, 0, 0);
	lv_obj_add_style(btn_question_manager, &style_obj_wrong, 0);
	lv_obj_t* lbl_question_manager = lv_label_create(btn_question_manager);
	lv_obj_set_style_text_font(lbl_question_manager, large_font, 0);
	lv_label_set_text(lbl_question_manager, "管理题库");
	lv_obj_center(lbl_question_manager);
	lv_obj_add_event_cb(btn_question_manager, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			msgboxQuestionManager();
		}
		}, LV_EVENT_ALL, NULL);
	//保存设置按钮
	lv_obj_t* btn_save = lv_btn_create(obj_box);
	lv_obj_set_width(btn_save, lv_pct(15));
	lv_obj_add_style(btn_save, &style_obj_right, 0);
	lv_obj_t* lbl_save = lv_label_create(btn_save);
	lv_obj_set_style_text_font(lbl_save, large_font, 0);
	lv_label_set_text(lbl_save, "保存");
	lv_obj_center(lbl_save);
	lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
	struct tmp_userdata_t {
		lv_obj_t* ta_question_count, * ta_scorePerQuestion, * ta_examTime;
	};
	static struct tmp_userdata_t tmp_userdata;
	tmp_userdata.ta_question_count = ta_question_count;
	tmp_userdata.ta_scorePerQuestion = ta_scorePerQuestion;
	tmp_userdata.ta_examTime = ta_examTime;

	lv_obj_add_event_cb(btn_save, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			struct tmp_userdata_t* tmp_userdata = (struct tmp_userdata_t*)lv_event_get_user_data(e);
			cfg_question_count = atoi(lv_textarea_get_text(tmp_userdata->ta_question_count));
			cfg_scorePerQuestion = atoi(lv_textarea_get_text(tmp_userdata->ta_scorePerQuestion));
			cfg_examTime = atoi(lv_textarea_get_text(tmp_userdata->ta_examTime)) * 60;
			saveConfig();
			switchScreen(buildMainWindow(), LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
		}
		}, LV_EVENT_ALL, &tmp_userdata);
	return scr;
}
void startTest(bool exam, int n, int score, int time_sec)
{
	showAnswerImmediately = exam == false;
	time_remain = time_sec;
	scorePerQuestion = score;
	answerSheetVisable = true;
	isSubmitPressed = false;
	lv_obj_t* scr = lv_obj_create(NULL);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	init_question_cards(n, scr);
	//下面这两行一定要在init_question_cards之后
	obj_answerSheet = add_answer_sheet(scr);
	answerSheetTable = add_answerSheetTable(question_count);
	addPrepareWindow(scr);
	lv_obj_set_style_opa(lbl_time_remain, 0, 0);
	switchScreen(scr, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
}
const char* getGreeting(int hr)
{
	if (hr >= 5 && hr < 9)
		return "早上好";
	else if (hr >= 9 && hr < 11)
		return "上午好";
	else if (hr >= 11 && hr < 13)
		return "中午好";
	else if (hr >= 13 && hr < 18)
		return "下午好";
	else if (hr >= 18 && hr < 23)
		return "晚上好";
	else
		return "加油！";
}
lv_obj_t* buildMainWindow()
{
	lv_obj_t* scr = lv_obj_create(NULL);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t* obj_time = lv_label_create(scr);
	lv_font_t* font_time = lv_freetype_font_create("msyhl.ttc", 64, LV_FREETYPE_FONT_STYLE_NORMAL);
	lv_obj_set_style_text_font(obj_time, font_time, 0);
	SYSTEMTIME timenow;
	GetLocalTime(&timenow);			//没用time_t是因为有些Windows系统注册表中设置了UTC时间，而不是本地时间
	lv_label_set_text(obj_time, getGreeting(timenow.wHour));
	lv_obj_align(obj_time, LV_ALIGN_TOP_LEFT, 20, 10);
	//显示日期与时间
	lv_obj_t* obj_datetime = lv_label_create(scr);
	lv_obj_set_style_text_font(obj_datetime, large_font, 0);
	lv_label_set_text_fmt((lv_obj_t*)obj_datetime, "%d年%02d月%02d日 %02d:%02d:%02d", timenow.wYear, timenow.wMonth, timenow.wDay, timenow.wHour, timenow.wMinute, timenow.wSecond);
	lv_timer_create([](lv_timer_t* timer) {
		if (lv_obj_is_valid((lv_obj_t*)lv_timer_get_user_data(timer)) == false || isInHomeScreen == false)
		{
			lv_timer_delete(timer);
			return;
		}
		time_t t = time(NULL);
		SYSTEMTIME timenow;
		GetLocalTime(&timenow);
		lv_label_set_text_fmt((lv_obj_t*)lv_timer_get_user_data(timer), "%d年%02d月%02d日 %02d:%02d:%02d", timenow.wYear, timenow.wMonth, timenow.wDay, timenow.wHour, timenow.wMinute, timenow.wSecond);
		}, 1000, obj_datetime);
	lv_obj_align(obj_datetime, LV_ALIGN_TOP_MID, 0, 0);
	//下面是三个按钮，练习、考试、设置
	lv_obj_t* obj_btn = lv_btn_create(scr);
	lv_obj_set_size(obj_btn, lv_pct(70), LV_SIZE_CONTENT);
	lv_obj_align(obj_btn, LV_ALIGN_TOP_MID, 0, lv_pct(30));
	lv_obj_t* obj_label_btn = lv_label_create(obj_btn);
	lv_obj_set_style_text_font(obj_label_btn, font_time, 0);
	lv_label_set_text(obj_label_btn, "练习");
	lv_obj_center(obj_label_btn);
	lv_obj_add_event_cb(obj_btn, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			isInHomeScreen = false;
			startTest(false, cfg_question_count, cfg_scorePerQuestion, cfg_examTime);
		}
		}, LV_EVENT_ALL, NULL);
	lv_obj_t* obj_btn2 = lv_btn_create(scr);
	lv_obj_set_size(obj_btn2, lv_pct(70), LV_SIZE_CONTENT);
	lv_obj_align(obj_btn2, LV_ALIGN_TOP_MID, 0, lv_pct(50));
	obj_label_btn = lv_label_create(obj_btn2);
	lv_obj_set_style_text_font(obj_label_btn, font_time, 0);
	lv_label_set_text(obj_label_btn, "考试");
	lv_obj_center(obj_label_btn);
	lv_obj_add_event_cb(obj_btn2, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			isInHomeScreen = false;
			startTest(true, cfg_question_count, cfg_scorePerQuestion, cfg_examTime);
		}
		}, LV_EVENT_ALL, NULL);
	lv_obj_t* obj_btn3 = lv_btn_create(scr);
	lv_obj_set_size(obj_btn3, lv_pct(70), LV_SIZE_CONTENT);
	lv_obj_align(obj_btn3, LV_ALIGN_TOP_MID, 0, lv_pct(70));
	obj_label_btn = lv_label_create(obj_btn3);
	lv_obj_set_style_text_font(obj_label_btn, font_time, 0);
	lv_label_set_text(obj_label_btn, "设置");
	lv_obj_center(obj_label_btn);
	lv_obj_add_event_cb(obj_btn3, [](lv_event_t* e) {
		if (e->code == LV_EVENT_CLICKED)
		{
			isInHomeScreen = false;
			switchScreen(buildSettingsWindow(), LV_SCR_LOAD_ANIM_MOVE_TOP);
		}
		}, LV_EVENT_ALL, NULL);
	isInHomeScreen = true;
	return scr;
}

int app_main()
{
	LOCKLV();
	add_button_exit();
	lv_screen_load(buildMainWindow());
	UNLOCKLV();
	return 0;
}
