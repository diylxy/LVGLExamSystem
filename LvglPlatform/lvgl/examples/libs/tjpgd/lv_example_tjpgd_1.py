#!/opt/bin/lv_micropython -i
import lvgl as lv
import display_driver
import fs_driver

fs_drv = lv.fs_drv_t()
fs_driver.fs_register(fs_drv, 'S')

wp = lv.image(lv.screen_active())
# The File system is attached to letter 'S'

wp.set_src("S:img_lvgl_logo.jpg")
wp.center()
