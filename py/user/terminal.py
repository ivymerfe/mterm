import core
import base
import math
from . import theme


class Terminal(base.BaseTerminal):
    def __init__(self, app, id):
        super().__init__(app, id)

    def on_input(self, input_text):
        self.console.send(input_text)

    def on_keydown(self, key):
        if key == core.keys.LEFT:
            self.console.send("\x1b[D")  # Move cursor left
        elif key == core.keys.RIGHT:
            self.console.send("\x1b[C")
        elif key == core.keys.UP:
            self.console.send("\x1b[A")
        elif key == core.keys.DOWN:
            self.console.send("\x1b[B")
        elif key == core.keys.HOME:
            self.console.send("\x1b[H")
        elif key == core.keys.END:
            self.console.send("\x1b[F")
        elif key == core.keys.UP:
            self.console.send("\x1b[1;5A")
        elif key == core.keys.DOWN:
            self.console.send("\x1b[1;5B")

    def on_keyup(self, key):
        pass

    def on_scroll(self, delta):
        if core.is_key_down(core.keys.LCONTROL):
            # Zoom in/out with Ctrl+scroll
            font_size = max(4, self.font_size + delta)
            if font_size != self.font_size:
                self.font_size = font_size
                self.app.redraw()
        elif not self.is_alt_screen:  # Scrolling only works in main screen
            visible_rows = self.app.get_client_height() / self.app.get_line_height(
                self.font_size
            )
            delta *= math.floor(theme.Terminal.SCROLL_SPEED * visible_rows)
            new_offset = max(
                0, min(self.main_screen.start_pos, self.scroll_offset + delta)
            )
            if new_offset != self.scroll_offset:
                self.scroll_offset = new_offset
                self.app.redraw()
    
    def on_mousemove(self, x, y):
        pass
    
    def on_mousedown(self, button, x, y):
        pass

    def on_mouseup(self, button, x, y):
        if button == core.buttons.RIGHT:
            self.console.send(core.clipboard_paste())

    def on_doubleclick(self, button, x, y):
        pass

    def on_mouseleave(self):
        pass
