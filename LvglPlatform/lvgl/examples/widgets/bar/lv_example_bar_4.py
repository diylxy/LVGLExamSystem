# Create an image from the png file
try:
    with open('../../assets/img_strip.png','rb') as f:
        png_data = f.read()
except:
    print("Could not find img_strip.png")
    sys.exit()

image_skew_strip_dsc = lv.image_dsc_t({
  'data_size': len(png_data),
  'data': png_data
})

#
# Bar with stripe pattern and ranged value
#

style_indic = lv.style_t()

style_indic.init()
style_indic.set_bg_image_src(image_skew_strip_dsc)
style_indic.set_bg_image_tiled(True)
style_indic.set_bg_image_opa(lv.OPA._30)

bar = lv.bar(lv.screen_active())
bar.add_style(style_indic, lv.PART.INDICATOR)

bar.set_size(260, 20)
bar.center()
bar.set_mode(lv.bar.MODE.RANGE)
bar.set_value(90, lv.ANIM.OFF)
bar.set_start_value(20, lv.ANIM.OFF)



