from . import mterm, cursors


class Window(mterm.Window):
    def on_render(self):
        pass

    def on_resize(self, width, height):
        pass

    def on_keydown(self, key):
        pass

    def on_keyup(self, key):
        pass

    def on_input(self, char):
        pass

    def on_mousemove(self, x, y):
        pass

    def on_mousedown(self, button, x, y):
        pass

    def on_mouseup(self, button, x, y):
        pass

    def on_doubleclick(self, button, x, y):
        pass

    def on_scroll(self, delta, x, y):
        pass

    def on_mouseleave(self):
        pass

    def run(
        self,
        font_name="Cascadia Mono",
        icon_path="icon.ico",
        width=1000,
        height=600,
        min_width=375,
        min_height=225,
        border_size=5,
        cursor_id=cursors.ARROW,
    ):
        config = mterm.Config()
        config.font_name = font_name
        config.icon_path = icon_path
        config.window_width = width
        config.window_height = height
        config.window_min_width = min_width
        config.window_min_height = min_height
        config.border_size = border_size
        config.cursor_id = cursor_id

        config.render_callback = self.on_render
        config.resize_callback = self.on_resize
        config.keydown_callback = self.on_keydown
        config.keyup_callback = self.on_keyup
        config.input_callback = self.on_input
        config.mousemove_callback = self.on_mousemove
        config.mousedown_callback = self.on_mousedown
        config.mouseup_callback = self.on_mouseup
        config.doubleclick_callback = self.on_doubleclick
        config.scroll_callback = self.on_scroll
        config.mouseleave_callback = self.on_mouseleave

        return self.create(config)
