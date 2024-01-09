button_cnt = 1

def float_button_event_cb(e):
    global button_cnt
    button_cnt += 1

    sub_page = lv.menu_page(menu, None)

    cont = lv.menu_cont(sub_page)
    label = lv.label(cont)
    label.set_text("Hello, I am hiding inside {:d}".format(button_cnt))

    cont = lv.menu_cont(main_page)
    label = lv.label(cont)
    label.set_text("Item {:d}".format(button_cnt))
    menu.set_load_page_event(cont, sub_page)

# Create a menu object
menu = lv.menu(lv.screen_active())
menu.set_size(320, 240)
menu.center()

# Create a sub page
sub_page = lv.menu_page(menu, None)

cont = lv.menu_cont(sub_page)
label = lv.label(cont)
label.set_text("Hello, I am hiding inside the first item")

# Create a main page
main_page = lv.menu_page(menu, None)

cont = lv.menu_cont(main_page)
label = lv.label(cont)
label.set_text("Item 1")
menu.set_load_page_event(cont, sub_page)

menu.set_page(main_page)

float_button = lv.button(lv.screen_active())
float_button.set_size(50, 50)
float_button.add_flag(lv.obj.FLAG.FLOATING)
float_button.align(lv.ALIGN.BOTTOM_RIGHT, -10, -10)
float_button.add_event_cb(float_button_event_cb, lv.EVENT.CLICKED, None)
float_button.set_style_radius(lv.RADIUS_CIRCLE, 0)
float_button.set_style_bg_image_src(lv.SYMBOL.PLUS, 0)
float_button.set_style_text_font(lv.theme_get_font_large(float_button), 0)