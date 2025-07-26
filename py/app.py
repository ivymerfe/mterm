import core
import config
import theme

from terminal import Terminal

class App(core.Window):
    CLOSE_BTN_IDX = -2
    MAX_BTN_IDX = -3
    MIN_BTN_IDX = -4

    def __init__(self):
        super().__init__()
        self.terminals = []
        self.next_terminal_id = 0
        self.active_terminal_id = -1

        self.terminal_selector_size = 0.2 # 20% of window width
        self.selector_width = 0
        self.terminal_width = 0
        self.hovered_button_index = -1

        self.is_resizing_selector = False

        self.mouse_x = 0
        self.mouse_y = 0

    def create_terminal(self):
        width = self.get_width() - config.BORDER_SIZE * 2
        height = self.get_height() - config.CAPTION_SIZE - config.BORDER_SIZE
        width = int(width * (1 - self.terminal_selector_size))
        line_height = self.get_line_height(config.BASE_FONT_SIZE)
        advance = self.get_advance(config.BASE_FONT_SIZE)
        if line_height == 0 or advance == 0:
            num_rows = config.NUM_ROWS
            num_columns = config.NUM_COLUMNS
        else:
            num_rows = int(height // line_height)
            num_columns = int(width // advance)
        terminal = Terminal(self, f"T-{self.next_terminal_id}", num_rows, num_columns)
        self.active_terminal_id = len(self.terminals)
        self.terminals.append(terminal)
        self.next_terminal_id += 1
    
    def destroy_terminal(self, terminal_id):
        if 0 <= terminal_id < len(self.terminals):
            del self.terminals[terminal_id]
            if terminal_id <= self.active_terminal_id:
                self.active_terminal_id -= 1
                self.active_terminal_id = max(0, self.active_terminal_id)

    def on_render(self):
        width = self.get_width()
        height = self.get_height()
        self.render_frame(width, height)

        x = theme.Window.OUTLINE_THICKNESS
        y = config.CAPTION_SIZE
        width = width - x - theme.Window.OUTLINE_THICKNESS
        height = height - y - theme.Window.OUTLINE_THICKNESS

        self.selector_width = int(width * self.terminal_selector_size)
        self.terminal_width = width - self.selector_width
        terminal_x = x + self.selector_width
        self.render_terminal_selector(x, y, self.selector_width, height)
        self.render_terminal(terminal_x, y, self.terminal_width, height)

    def on_mousemove(self, x, y):
        self.mouse_x = x
        self.mouse_y = y
        if self.is_resizing_selector:
            border_size = config.BORDER_SIZE
            width = self.get_width() - border_size * 2
            self.terminal_selector_size = (x - border_size) / width
            self.redraw()
            return

        hovered_button_idx = -1
        cursor_id = core.cursors.ARROW
        if y < config.CAPTION_SIZE:
            width = self.get_width()
            if x >= width - config.BUTTON_SIZE:
                hovered_button_idx = -2
            elif x >= width - config.BUTTON_SIZE * 2:
                hovered_button_idx = -3
            elif x >= width - config.BUTTON_SIZE * 3:
                hovered_button_idx = -4
        else:
            line_height = self.get_line_height(theme.Selector.FONT_SIZE)
            button_height = int(line_height + theme.Selector.BUTTON_PAD_HEIGHT * 2)
            sizebar_width = theme.Selector.SIZEBAR_WIDTH

            if x < self.selector_width - sizebar_width:
                hovered_button_idx = (y - config.CAPTION_SIZE) // button_height
            elif x < self.selector_width + sizebar_width:
                cursor_id = core.cursors.SIZEWE
            else:
                cursor_id = core.cursors.IBEAM

        self.set_cursor(cursor_id)
        if self.hovered_button_index != hovered_button_idx:
            self.hovered_button_index = hovered_button_idx
            self.redraw()
    
    def on_mousedown(self, button, x, y):
        if y < config.CAPTION_SIZE:
            if button == core.buttons.LEFT:
                width = self.get_width()
                if x >= width - config.BUTTON_SIZE:
                    pass
                elif x >= width - config.BUTTON_SIZE * 2:
                    pass
                elif x >= width - config.BUTTON_SIZE * 3:
                    pass
                else:
                    self.drag()
        elif self.selector_width - theme.Selector.SIZEBAR_WIDTH <= x <= self.selector_width + theme.Selector.SIZEBAR_WIDTH:
            if button == core.buttons.LEFT:
                self.is_resizing_selector = True
    
    def on_mouseup(self, button, x, y):
        if self.is_resizing_selector:
            self.is_resizing_selector = False
        
        if y < config.CAPTION_SIZE:
            if button == core.buttons.LEFT:
                width = self.get_width()
                if x >= width - config.BUTTON_SIZE:
                    self.destroy()
                elif x >= width - config.BUTTON_SIZE * 2:
                    if self.is_maximized():
                        self.restore()
                    else:
                        self.maximize()
                elif x >= width - config.BUTTON_SIZE * 3:
                    self.minimize()

        if self.hovered_button_index >= 0:
            if button == core.buttons.LEFT:
                if self.hovered_button_index < len(self.terminals):
                    if self.active_terminal_id != self.hovered_button_index:
                        self.active_terminal_id = self.hovered_button_index
                        self.redraw()
                elif self.hovered_button_index == len(self.terminals):
                    self.create_terminal()
                    self.redraw()
            elif button == core.buttons.MIDDLE:
                if self.hovered_button_index < len(self.terminals):
                    self.destroy_terminal(self.hovered_button_index)
                    self.redraw()
    
    def on_doubleclick(self, button, x, y):
        if y < config.CAPTION_SIZE:
            if button == core.buttons.LEFT:
                width = self.get_width()
                if x < width - config.BUTTON_SIZE * 3:
                    if self.is_maximized():
                        self.restore()
                    else:
                        self.maximize()
    
    def on_mouse_leave(self):
        self.is_resizing_selector = False
        self.hovered_button_index = -1
        self.redraw()
    
    def on_input(self, char):
        if self.active_terminal_id < len(self.terminals):
            terminal = self.terminals[self.active_terminal_id]
            terminal.on_input(char)
    
    def render_terminal_selector(self, x, y, width, height):
        self.rect(x, y, x + width, y + height, theme.Selector.BG, 1)
        
        line_height = self.get_line_height(theme.Selector.FONT_SIZE)
        advance = self.get_advance(theme.Selector.FONT_SIZE)
        button_height = int(line_height + theme.Selector.BUTTON_PAD_HEIGHT * 2)

        for i, terminal in enumerate(self.terminals):
            is_active = (i == self.active_terminal_id)
            is_hovered = (i == self.hovered_button_index)
            self.render_selector_button(terminal.title, line_height, advance, x, y + i * button_height, width, is_hovered, is_active)
        
        new_button_idx = len(self.terminals)
        is_new_button_hovered = (self.hovered_button_index == new_button_idx)
        new_button_y = y + new_button_idx * button_height
        self.render_selector_button("New", line_height, advance, x, new_button_y, width, is_new_button_hovered, False)

    def render_selector_button(self, text, line_height, advance, x, y, width, is_hovered, is_active):
        color = theme.Selector.BUTTON_ACTIVE if is_active else theme.Selector.BUTTON_HOVER if is_hovered else theme.Selector.BUTTON
        
        height = int(line_height + theme.Selector.BUTTON_PAD_HEIGHT * 2)
        self.rect(x, y, x + width, y + height, color, 1)

        text_x = x + theme.Selector.BUTTON_PAD_WIDTH
        text_y = y + theme.Selector.BUTTON_PAD_HEIGHT
        text_max_length = int((width - theme.Selector.BUTTON_PAD_WIDTH * 2) // advance)
        text = text[:text_max_length]
        self.text(text, theme.Selector.FONT_SIZE, text_x, text_y, theme.Selector.TEXT, -1, -1, 1.0)

    def render_terminal(self, x, y, width, height):
        self.rect(x, y, x + width, y + height, theme.Terminal.BG, 1)
        if self.active_terminal_id < len(self.terminals):
            terminal = self.terminals[self.active_terminal_id]
            buffer = terminal.buffer
            self.text_buffer(buffer, x, y, width, height, terminal.offset_x, terminal.offset_y, terminal.font_size)
            # TODO - cursor and line numbers

    def render_frame(self, width, height):
        self.clear(theme.Window.BG)

        caption_size = config.CAPTION_SIZE
        outline_thickness = theme.Window.OUTLINE_THICKNESS
        self.outline(0, 0, width, height, outline_thickness, theme.Window.OUTLINE, 1)
        width -= outline_thickness

        size = theme.Window.BUTTON_SIZE
        full_size = config.BUTTON_SIZE
        y = caption_size // 2
        
        center_offset = full_size // 2
        min_left = width - full_size * 3
        max_left = width - full_size * 2
        close_left = width - full_size

        if self.hovered_button_index == self.CLOSE_BTN_IDX:
            self.rect(close_left, outline_thickness, close_left + full_size, caption_size, theme.Window.BUTTON_HOVER, 1)
        elif self.hovered_button_index == self.MAX_BTN_IDX:
            self.rect(max_left, outline_thickness, max_left + full_size, caption_size, theme.Window.BUTTON_HOVER, 1)
        elif self.hovered_button_index == self.MIN_BTN_IDX:
            self.rect(min_left, outline_thickness, min_left + full_size, caption_size, theme.Window.BUTTON_HOVER, 1)

        min_x = min_left + center_offset
        self.rect(min_x - size, y, min_x + size, y + 1, theme.Window.MIN_BUTTON, 1)

        max_x = max_left + center_offset
        self.outline(max_x - size, y - size, max_x + size, y + size, 1, theme.Window.MAX_BUTTON, 1)

        close_x = close_left + center_offset
        self.line(close_x - size, y - size, close_x + size, y + size, 1, theme.Window.CLOSE_BUTTON, 1)
        self.line(close_x + size, y - size, close_x - size, y + size, 1, theme.Window.CLOSE_BUTTON, 1)

    def main(self):
        self.create_terminal()
        self.run(
            font_name=config.FONT_NAME,
            width=config.WINDOW_WIDTH,
            height=config.WINDOW_HEIGHT,
            min_width=config.MIN_WINDOW_WIDTH,
            min_height=config.MIN_WINDOW_HEIGHT,
            border_size=config.BORDER_SIZE,
            cursor_id=core.cursors.ARROW
        )

if __name__ == "__main__":
    app = App()
    app.main()
