#
# Slider with opposite direction
#
def slider_event_cb(e):

    slider = e.get_target_obj()
    slider_label.set_text("{:d}%".format(slider.get_value()))
    slider_label.align_to(slider, lv.ALIGN.OUT_BOTTOM_MID, 0, 10)

# Create a slider in the center of the display
slider = lv.slider(lv.screen_active())
slider.center()
slider.add_event_cb(slider_event_cb, lv.EVENT.VALUE_CHANGED, None)
slider.set_range(100, 0)

# Create a label below the slider
slider_label = lv.label(lv.screen_active())
slider_label.set_text("0%")

slider_label.align_to(slider, lv.ALIGN.OUT_BOTTOM_MID, 0, 10)

