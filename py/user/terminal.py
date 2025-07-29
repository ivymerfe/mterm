import core
import math
from base import TerminalWithSelection, SelectionType
from . import theme


class Terminal(TerminalWithSelection):
    def __init__(self, app, id):
        super().__init__(app, id)
        self.is_selecting = False

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

    def on_input(self, input_text):
        if self.selection_type != SelectionType.NONE:
            if core.is_key_down(ord('C')):
                self.copy_selection(append=core.is_key_down(core.keys.LSHIFT))
                return
        self.console.send(input_text)

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
        if self.is_selecting:
            buffer_pos = self.get_buffer_position(x, y)
            if buffer_pos != self.selection_end:
                self.selection_end = buffer_pos
                self.app.redraw()

    def on_mousedown(self, button, x, y):
        if x <= self.app.get_selector_width() or y <= self.app.get_caption_height():
            return
        current_selection_type = (
            SelectionType.LINES
            if button == core.buttons.LEFT
            else (
                SelectionType.BLOCK
                if button == core.buttons.MIDDLE
                else SelectionType.NONE
            )
        )

        if core.is_key_down(core.keys.LSHIFT):
            # Extend selection with Shift+click
            self.is_selecting = self.selection_type == current_selection_type
            self.selection_type = current_selection_type
            self.selection_end = self.get_buffer_position(x, y)
            self.app.redraw()
            return

        self.is_selecting = True
        self.selection_type = current_selection_type
        self.selection_start = self.get_buffer_position(x, y)
        self.selection_end = self.selection_start
        self.app.redraw()

    def on_mouseup(self, button, x, y):
        if button == core.buttons.LEFT and self.selection_type == SelectionType.LINES:
            self.is_selecting = False
        if button == core.buttons.MIDDLE and self.selection_type == SelectionType.BLOCK:
            self.is_selecting = False
        if button == core.buttons.RIGHT:
            if self.selection_type != SelectionType.NONE:
                if not self.is_selecting:
                    self.selection_type = SelectionType.NONE
                    self.app.redraw()
            else:
                self.console.send(core.clipboard_paste())

    def on_doubleclick(self, button, x, y):
        if button == core.buttons.LEFT:
            row, col = self.get_buffer_position(x, y)
            line = self.current_screen.buffer.get_line_text(row, 0, -1)
            if line[col] == " ":
                return
            typ = line[col].isalnum()
            start = col
            end = col
            while start > 0 and (line[start - 1].isalnum() == typ):
                start -= 1
            while end < len(line) - 1 and (line[end + 1].isalnum() == typ):
                end += 1
            self.selection_type = SelectionType.LINES
            self.selection_start = (row, start)
            self.selection_end = (row, end)
            self.app.redraw()

    def on_mouseleave(self):
        self.is_selecting = False

    def switch_to_alt_screen(self):
        super().switch_to_alt_screen()
        self.selection_type = SelectionType.NONE

    def switch_to_main_screen(self):
        super().switch_to_main_screen()
        self.selection_type = SelectionType.NONE

    def copy_selection(self, append=False):
        if self.selection_type != SelectionType.NONE:
            text = self.get_selection_text()
            if append:
                prev_text = core.clipboard_paste()
                if prev_text:
                    core.clipboard_copy(prev_text + "\n" + text)
                else:
                    core.clipboard_copy(text)
            else:
                core.clipboard_copy(text)
            self.selection_type = SelectionType.NONE
            self.app.redraw()
