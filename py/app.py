import core
import theme

from terminal import Terminal

class App(core.Window):
    GROUP_CONTROLS = 0
    GROUP_SELECTOR = 1
    GROUP_TERMINAL = 2

    BUTTON_CAPTION = 0
    BUTTON_CLOSE = 1
    BUTTON_MAXIMIZE = 2
    BUTTON_MINIMIZE = 3
    BUTTON_SIZEBAR = 4

    def __init__(self):
        super().__init__()
        self.terminals = []
        self.next_terminal_id = 0
        self.active_terminal_id = -1

        self.terminal_selector_size = theme.Selector.BASE_WIDTH
        self.selector_width = 0
        self.terminal_width = 0
        
        self.selected_group = -1
        self.selected_button = -1
        self.is_resizing_selector = False

        self.mouse_x = 0
        self.mouse_y = 0
    
    def get_client_width(self):
        return self.get_width() - theme.Window.OUTLINE_THICKNESS * 2
    
    def get_client_height(self):
        return self.get_height() - theme.Window.CAPTION_SIZE - theme.Window.OUTLINE_THICKNESS

    def get_selector_width(self):
        return int(self.get_client_width() * self.terminal_selector_size)
    
    def get_terminal_width(self):
        return self.get_client_width() - self.get_selector_width()

    def create_terminal(self):
        terminal = Terminal(self, f"T-{self.next_terminal_id}")
        self.active_terminal_id = len(self.terminals)
        self.terminals.append(terminal)
        self.next_terminal_id += 1
    
    def destroy_terminal(self, terminal_id):
        if 0 <= terminal_id < len(self.terminals):
            del self.terminals[terminal_id]
            if terminal_id <= self.active_terminal_id:
                self.active_terminal_id -= 1
                self.active_terminal_id = max(0, self.active_terminal_id)

    def resize_active_terminal(self):
        if 0 <= self.active_terminal_id < len(self.terminals):
            terminal = self.terminals[self.active_terminal_id]
            terminal.resize(self.get_terminal_width(), self.get_client_height())
            self.redraw()

    def on_render(self):
        pass
        self.render_frame()

        x = theme.Window.OUTLINE_THICKNESS
        y = theme.Window.CAPTION_SIZE

        self.selector_width = self.get_selector_width()
        self.terminal_width = self.get_terminal_width()
        height = self.get_client_height()
        terminal_x = x + self.selector_width
        self.render_terminal_selector(x, y, self.selector_width, height)
        if 0 <= self.active_terminal_id < len(self.terminals):
            terminal = self.terminals[self.active_terminal_id]
            terminal.render(terminal_x, y, self.terminal_width, height)

    def update_selection(self, x, y):
        self.mouse_x = x
        self.mouse_y = y
        selected_group = -1
        selected_button = -1

        caption_size = theme.Window.CAPTION_SIZE
        if y < caption_size:
            selected_group = self.GROUP_CONTROLS
            width = self.get_width()
            button_size = theme.Window.BUTTON_SIZE
            if x >= width - button_size:
                selected_button = self.BUTTON_CLOSE
            elif x >= width - button_size * 2:
                selected_button = self.BUTTON_MAXIMIZE
            elif x >= width - button_size * 3:
                selected_button = self.BUTTON_MINIMIZE
            else:
                selected_button = self.BUTTON_CAPTION
        else:
            line_height = self.get_line_height(theme.Selector.FONT_SIZE)
            button_height = int(line_height + theme.Selector.BUTTON_PAD_HEIGHT * 2)
            sizebar_width = theme.Selector.SIZEBAR_WIDTH

            if x < self.selector_width - sizebar_width:
                selected_group = self.GROUP_SELECTOR
                selected_button = (y - caption_size) // button_height
            elif x < self.selector_width + sizebar_width:
                selected_group = self.GROUP_CONTROLS
                selected_button = self.BUTTON_SIZEBAR
            else:
                selected_group = self.GROUP_TERMINAL
        
        if self.selected_group != selected_group or self.selected_button != selected_button:
            self.selected_group = selected_group
            self.selected_button = selected_button
            self.update_cursor()
            self.redraw()
    
    def update_cursor(self):
        if self.is_resizing_selector:
            self.set_cursor(core.cursors.SIZEWE)
        elif self.selected_group == self.GROUP_CONTROLS and self.selected_button == self.BUTTON_SIZEBAR:
            self.set_cursor(core.cursors.SIZEWE)
        elif self.selected_group == self.GROUP_TERMINAL:
            self.set_cursor(core.cursors.IBEAM)
        else:
            self.set_cursor(core.cursors.ARROW)
    
    def on_mousemove(self, x, y):
        self.update_selection(x, y)
        if self.is_resizing_selector:
            border_size = theme.Window.BORDER_SIZE
            width = self.get_width() - border_size * 2
            self.terminal_selector_size = max(theme.Selector.MIN_WIDTH, (x - border_size) / width)
            self.redraw()
    
    def on_mousedown(self, button, x, y):
        self.update_selection(x, y)
        if button == core.buttons.LEFT:
            if self.selected_group == self.GROUP_CONTROLS:
                if self.selected_button == self.BUTTON_CAPTION:
                    self.drag()
                elif self.selected_button == self.BUTTON_SIZEBAR:
                    self.is_resizing_selector = True
    
    def on_mouseup(self, button, x, y):
        self.update_selection(x, y)
        if self.is_resizing_selector:
            self.is_resizing_selector = False
        
        if self.selected_group == self.GROUP_CONTROLS:
            if button != core.buttons.LEFT:
                return
            
            if self.selected_button == self.BUTTON_CLOSE:
                self.destroy()
            elif self.selected_button == self.BUTTON_MAXIMIZE:
                if self.is_maximized():
                    self.restore()
                else:
                    self.maximize()
            elif self.selected_button == self.BUTTON_MINIMIZE:
                self.minimize()
        elif self.selected_group == self.GROUP_SELECTOR:
            if button == core.buttons.LEFT:
                if self.selected_button >= 0 and self.selected_button < len(self.terminals):
                    if self.active_terminal_id != self.selected_button:
                        self.active_terminal_id = self.selected_button
                        self.redraw()
                elif self.selected_button == len(self.terminals):
                    self.create_terminal()
                    self.redraw()
            elif button == core.buttons.MIDDLE:
                if self.selected_button >= 0 and self.selected_button < len(self.terminals):
                    self.destroy_terminal(self.selected_button)
                    self.redraw()
    
    def on_doubleclick(self, button, x, y):
        self.update_selection(x, y)
        if self.selected_group == self.GROUP_CONTROLS and self.selected_button == self.BUTTON_CAPTION:
            if button == core.buttons.LEFT:
                if self.is_maximized():
                    self.restore()
                else:
                    self.maximize()
    
    def on_mouse_leave(self):
        self.is_resizing_selector = False
        self.selected_group = -1
        self.selected_button = -1
        self.redraw()
    
    def on_scroll(self, delta, x, y):
        if 0 <= self.active_terminal_id < len(self.terminals):
            terminal = self.terminals[self.active_terminal_id]
            terminal.on_scroll(delta)
    
    def on_input(self, char):
        if 0 <= self.active_terminal_id < len(self.terminals):
            terminal = self.terminals[self.active_terminal_id]
            terminal.on_input(char)
    
    def on_keydown(self, key):
        if core.is_key_down(core.keys.LMENU):
            return
        if 0 <= self.active_terminal_id < len(self.terminals):
            terminal = self.terminals[self.active_terminal_id]
            terminal.on_keydown(key)
    
    def on_keyup(self, key):
        if core.is_key_down(core.keys.LMENU):
            if key == ord('Q'):
                self.destroy()
            elif key == ord('R'):
                self.resize_active_terminal()
            return
        
        if 0 <= self.active_terminal_id < len(self.terminals):
            terminal = self.terminals[self.active_terminal_id]
            terminal.on_keyup(key)
    
    def render_terminal_selector(self, x, y, width, height):
        self.rect(x, y, x + width, y + height, theme.Selector.BG, 1)
        
        line_height = self.get_line_height(theme.Selector.FONT_SIZE)
        advance = self.get_advance(theme.Selector.FONT_SIZE)
        button_height = int(line_height + theme.Selector.BUTTON_PAD_HEIGHT * 2)

        for i, terminal in enumerate(self.terminals):
            is_active = (i == self.active_terminal_id)
            is_hovered = self.selected_group == self.GROUP_SELECTOR and (i == self.selected_button)
            self.render_selector_button(terminal.title, line_height, advance, x, y + i * button_height, width, is_hovered, is_active)
        
        new_button_idx = len(self.terminals)
        is_new_button_hovered = (self.selected_group == self.GROUP_SELECTOR and new_button_idx == self.selected_button)
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

    def render_frame(self):
        self.clear(theme.Window.BG)

        width = self.get_width()
        height = self.get_height()
        caption_size = theme.Window.CAPTION_SIZE
        outline_thickness = theme.Window.OUTLINE_THICKNESS
        self.outline(0, 0, width, height, outline_thickness, theme.Window.OUTLINE, 1)
        width -= outline_thickness

        size = theme.Window.BUTTON_ICON_SIZE
        full_size = theme.Window.BUTTON_SIZE
        y = caption_size // 2
        
        center_offset = full_size // 2
        min_left = width - full_size * 3
        max_left = width - full_size * 2
        close_left = width - full_size

        if self.selected_group == self.GROUP_CONTROLS:
            if self.selected_button == self.BUTTON_CLOSE:
                self.rect(close_left, outline_thickness, close_left + full_size, caption_size, theme.Window.BUTTON_HOVER, 1)
            elif self.selected_button == self.BUTTON_MAXIMIZE:
                self.rect(max_left, outline_thickness, max_left + full_size, caption_size, theme.Window.BUTTON_HOVER, 1)
            elif self.selected_button == self.BUTTON_MINIMIZE:
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
            font_name=theme.Window.FONT,
            width=theme.Window.WIDTH,
            height=theme.Window.HEIGHT,
            min_width=theme.Window.MIN_WIDTH,
            min_height=theme.Window.MIN_HEIGHT,
            border_size=theme.Window.BORDER_SIZE,
            cursor_id=core.cursors.ARROW
        )

if __name__ == "__main__":
    app = App()
    app.main()
