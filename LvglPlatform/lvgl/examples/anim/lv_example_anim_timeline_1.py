class LV_ExampleAnimTimeline_1(object):

    def __init__(self):
        self.obj_width = 120
        self.obj_height = 150
        #
        # Create an animation timeline
        #

        self.par = lv.screen_active()
        self.par.set_flex_flow(lv.FLEX_FLOW.ROW)
        self.par.set_flex_align(lv.FLEX_ALIGN.SPACE_AROUND, lv.FLEX_ALIGN.CENTER, lv.FLEX_ALIGN.CENTER)

        self.button_run = lv.button(self.par)
        self.button_run.add_event_cb(self.button_run_event_handler, lv.EVENT.VALUE_CHANGED, None)
        self.button_run.add_flag(lv.obj.FLAG.IGNORE_LAYOUT)
        self.button_run.add_flag(lv.obj.FLAG.CHECKABLE)
        self.button_run.align(lv.ALIGN.TOP_MID, -50, 20)

        self.label_run = lv.label(self.button_run)
        self.label_run.set_text("Run")
        self.label_run.center()

        self.button_del = lv.button(self.par)
        self.button_del.add_event_cb(self.button_delete_event_handler, lv.EVENT.CLICKED, None)
        self.button_del.add_flag(lv.obj.FLAG.IGNORE_LAYOUT)
        self.button_del.align(lv.ALIGN.TOP_MID, 50, 20)

        self.label_del = lv.label(self.button_del)
        self.label_del.set_text("Stop")
        self.label_del.center()

        self.slider = lv.slider(self.par)
        self.slider.add_event_cb(self.slider_prg_event_handler, lv.EVENT.VALUE_CHANGED, None)
        self.slider.add_flag(lv.obj.FLAG.IGNORE_LAYOUT)
        self.slider.align(lv.ALIGN.BOTTOM_RIGHT, -20, -20)
        self.slider.set_range(0, 65535)

        self.obj1 = lv.obj(self.par)
        self.obj1.set_size(self.obj_width, self.obj_height)

        self.obj2 = lv.obj(self.par)
        self.obj2.set_size(self.obj_width, self.obj_height)

        self.obj3 = lv.obj(self.par)
        self.obj3.set_size(self.obj_width, self.obj_height)

        self.anim_timeline = None

    def set_width(self,obj, v):
        obj.set_width(v)

    def set_height(self,obj, v):
        obj.set_height(v)

    def anim_timeline_create(self):
        # obj1
        self.a1 = lv.anim_t()
        self.a1.init()
        self.a1.set_values(0, self.obj_width)
        self.a1.set_custom_exec_cb(lambda a,v: self.set_width(self.obj1,v))
        self.a1.set_path_cb(lv.anim_t.path_overshoot)
        self.a1.set_time(300)

        self.a2 = lv.anim_t()
        self.a2.init()
        self.a2.set_values(0, self.obj_height)
        self.a2.set_custom_exec_cb(lambda a,v: self.set_height(self.obj1,v))
        self.a2.set_path_cb(lv.anim_t.path_ease_out)
        self.a2.set_time(300)

        # obj2
        self.a3=lv.anim_t()
        self.a3.init()
        self.a3.set_values(0, self.obj_width)
        self.a3.set_custom_exec_cb(lambda a,v: self.set_width(self.obj2,v))
        self.a3.set_path_cb(lv.anim_t.path_overshoot)
        self.a3.set_time(300)

        self.a4 = lv.anim_t()
        self.a4.init()
        self.a4.set_values(0, self.obj_height)
        self.a4.set_custom_exec_cb(lambda a,v: self.set_height(self.obj2,v))
        self.a4.set_path_cb(lv.anim_t.path_ease_out)
        self.a4.set_time(300)

        # obj3
        self.a5 = lv.anim_t()
        self.a5.init()
        self.a5.set_values(0, self.obj_width)
        self.a5.set_custom_exec_cb(lambda a,v: self.set_width(self.obj3,v))
        self.a5.set_path_cb(lv.anim_t.path_overshoot)
        self.a5.set_time(300)

        self.a6 = lv.anim_t()
        self.a6.init()
        self.a6.set_values(0, self.obj_height)
        self.a6.set_custom_exec_cb(lambda a,v: self.set_height(self.obj3,v))
        self.a6.set_path_cb(lv.anim_t.path_ease_out)
        self.a6.set_time(300)

        # Create anim timeline
        print("Create new anim_timeline")
        self.anim_timeline = lv.anim_timeline_create()
        self.anim_timeline.add(0, self.a1)
        self.anim_timeline.add(0, self.a2)
        self.anim_timeline.add(200, self.a3)
        self.anim_timeline.add(200, self.a4)
        self.anim_timeline.add(400, self.a5)
        self.anim_timeline.add(400, self.a6)

    def slider_prg_event_handler(self,e):
        slider = e.get_target_obj()

        if  not self.anim_timeline:
            self.anim_timeline_create()

        progress = slider.get_value()
        self.anim_timeline.set_progress(progress)


    def button_run_event_handler(self,e):
        button = e.get_target_obj()
        if not self.anim_timeline:
            self.anim_timeline_create()

        reverse = button.has_state(lv.STATE.CHECKED)
        self.anim_timeline.set_reverse(reverse)
        self.anim_timeline.start()

    def button_delete_event_handler(self,e):
        if self.anim_timeline:
            self.anim_timeline.delete()
        self.anim_timeline = None


lv_example_anim_timeline_1 = LV_ExampleAnimTimeline_1()
