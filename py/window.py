import mterm
import cursors


class Window(mterm.Window):
    def on_render(self):
        self.clear(0xff0000)
        self.line(100, 100, 200, 200, 1, 0x00ff00)
        self.rect(300, 300, 500, 500, 0x0000ff)
        self.text("Hello!!!", 32.0, 100.0, 100.0, 0x00ff00, -1, -1, 1.0)

    def on_resize(self, width, height):
        pass

    def on_keydown(self, key, ctrl_down, shift_down, alt_down):
        pass

    def on_keyup(self, key, ctrl_down, shift_down, alt_down):
        pass

    def on_input(self, chr):
        print(chr, end='')
        self.destroy()

    def on_mousemove(self, x, y):
        pass

    def on_mousedown(self, button, x, y):
        pass

    def on_mouseup(self, button, x, y):
        pass

    def on_scroll(self, delta, x, y):
        pass

    def run(self, font_name="Cascadia Mono", width=1000, height=600, min_width=375, min_height=225, caption_size=30,
            border_size=5, button_size=30, close_button_offset=30, max_button_offset=0, min_button_offset=90,
            cursor_id=cursors.ARROW):
        config = mterm.Config()
        config.font_name = font_name
        config.window_width = width
        config.window_height = height
        config.window_min_width = min_width
        config.window_min_height = min_height
        config.caption_size = caption_size
        config.border_size = border_size
        config.button_size = button_size
        config.close_button_offset = close_button_offset
        config.max_button_offset = max_button_offset
        config.min_button_offset = min_button_offset
        config.cursor_id = cursor_id

        config.render_callback = self.on_render
        config.resize_callback = self.on_resize
        config.keydown_callback = self.on_keydown
        config.keyup_callback = self.on_keyup
        config.input_callback = self.on_input
        config.mousemove_callback = self.on_mousemove
        config.mousedown_callback = self.on_mousedown
        config.mouseup_callback = self.on_mouseup
        config.scroll_callback = self.on_scroll

        return self.create(config)


window = Window()
exit(window.run())
