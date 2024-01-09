import gc
import lvgl as lv

# Measure memory usage
gc.enable()
gc.collect()
mem_free = gc.mem_free()

label = lv.label(lv.screen_active())
label.align(lv.ALIGN.BOTTOM_MID, 0, -10)
label.set_text(" memory free:" + str(mem_free/1024) + " kB")

# Create an image from the png file
try:
    with open('../../assets/img_star.png','rb') as f:
        png_data = f.read()
except:
    print("Could not find star.png")
    sys.exit()

image_star = lv.image_dsc_t({
  'data_size': len(png_data),
  'data': png_data
})

def event_cb(e, snapshot_obj):
    image = e.get_target_obj()

    if snapshot_obj:
        # no need to free the old source for snapshot_obj, gc will free it for us.

        # take a new snapshot, overwrite the old one
        dsc = lv.snapshot_take(image.get_parent(), lv.COLOR_FORMAT.NATIVE_ALPHA)
        snapshot_obj.set_src(dsc)

    gc.collect()
    mem_used = mem_free - gc.mem_free()
    label.set_text("memory used:" + str(mem_used/1024) + " kB")

root = lv.screen_active()
root.set_style_bg_color(lv.palette_main(lv.PALETTE.LIGHT_BLUE), 0)

# Create an image object to show snapshot
snapshot_obj = lv.image(root)
snapshot_obj.set_style_bg_color(lv.palette_main(lv.PALETTE.PURPLE), 0)
snapshot_obj.set_style_bg_opa(lv.OPA.COVER, 0)
snapshot_obj.set_scale(128)

# Create the container and its children
container = lv.obj(root)
container.align(lv.ALIGN.CENTER, 0, 0)
container.set_size(180, 180)
container.set_flex_flow(lv.FLEX_FLOW.ROW_WRAP)
container.set_flex_align(lv.FLEX_ALIGN.SPACE_EVENLY, lv.FLEX_ALIGN.CENTER, lv.FLEX_ALIGN.CENTER)
container.set_style_radius(50, 0)

for i in range(4):
    image = lv.image(container)
    image.set_src(image_star)
    image.set_style_bg_color(lv.palette_main(lv.PALETTE.GREY), 0)
    image.set_style_bg_opa(lv.OPA.COVER, 0)
    image.set_style_transform_scale(400, lv.STATE.PRESSED)
    image.add_flag(image.FLAG.CLICKABLE)
    image.add_event_cb(lambda e: event_cb(e, snapshot_obj), lv.EVENT.PRESSED, None)
    image.add_event_cb(lambda e: event_cb(e, snapshot_obj), lv.EVENT.RELEASED, None)
