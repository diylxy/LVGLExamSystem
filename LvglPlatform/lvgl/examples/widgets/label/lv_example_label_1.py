#!/opt/bin/lv_micropython -i

import lvgl as lv
import display_driver

#
# Show line wrap, re-color, line align and text scrolling.
#
label1 = lv.label(lv.screen_active())
label1.set_long_mode(lv.label.LONG.WRAP)      # Break the long lines*/
label1.set_text("Recolor is not supported for v9 now.")
label1.set_width(150)                         # Set smaller width to make the lines wrap
label1.set_style_text_align(lv.TEXT_ALIGN.CENTER, 0)
label1.align(lv.ALIGN.CENTER, 0, -40)


label2 = lv.label(lv.screen_active())
label2.set_long_mode(lv.label.LONG.SCROLL_CIRCULAR) # Circular scroll
label2.set_width(150)
label2.set_text("It is a circularly scrolling text. ")
label2.align(lv.ALIGN.CENTER, 0, 40)

