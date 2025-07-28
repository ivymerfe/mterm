import core
import user.theme as theme

from terminal import Terminal

class App(core.Window):
    GROUP_CONTROLS = 0
    GROUP_SELECTOR = 1
    GROUP_TERMINAL = 2

    BUTTON_CAPTION = 0
    BUTTON_CLOSE = 1
    BUTTON_MAXIMIZE = 2
    BUTTON_MINIMIZE = 3

    def __init__(self):
        super().__init__()
        self.terminals = []
        self.next_terminal_id = 0
        self.active_terminal_idx = -1

        self.selector_width = 0
        self.terminal_width = 0
        
        self.selected_group = -1
        self.selected_button = -1

        self.mouse_x = 0
        self.mouse_y = 0
    
    def get_client_width(self):
        return self.get_width() - theme.Window.OUTLINE_THICKNESS * 2
    
    def get_client_height(self):
        return self.get_height() - theme.Window.CAPTION_SIZE - theme.Window.OUTLINE_THICKNESS

    def get_selector_width(self):
        return theme.Selector.WIDTH
    
    def get_terminal_width(self):
        return self.get_client_width() - self.get_selector_width()

    def create_terminal(self):
        terminal = Terminal(self, f"T-{self.next_terminal_id}")
        self.active_terminal_idx = len(self.terminals)
        self.terminals.append(terminal)
        self.next_terminal_id += 1
    
    def destroy_terminal(self, terminal_id):
        if 0 <= terminal_id < len(self.terminals):
            del self.terminals[terminal_id]
            if terminal_id <= self.active_terminal_idx:
                self.active_terminal_idx -= 1
                self.active_terminal_idx = max(0, self.active_terminal_idx)

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
        if 0 <= self.active_terminal_idx < len(self.terminals):
            terminal = self.terminals[self.active_terminal_idx]
            terminal.render(terminal_x, y, self.terminal_width, height)
            self.render_title(terminal.title)

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
            if x < self.selector_width:
                selected_group = self.GROUP_SELECTOR
                selected_button = (y - caption_size) // theme.Selector.WIDTH
            else:
                selected_group = self.GROUP_TERMINAL
        
        if self.selected_group != selected_group or self.selected_button != selected_button:
            self.selected_group = selected_group
            self.selected_button = selected_button
            self.update_cursor()
            self.redraw()
    
    def update_cursor(self):
        if self.selected_group == self.GROUP_TERMINAL:
            self.set_cursor(core.cursors.IBEAM)
        else:
            self.set_cursor(core.cursors.ARROW)
    
    def on_mousemove(self, x, y):
        self.update_selection(x, y)
    
    def on_mousedown(self, button, x, y):
        self.update_selection(x, y)
        if button == core.buttons.LEFT:
            if self.selected_group == self.GROUP_CONTROLS:
                if self.selected_button == self.BUTTON_CAPTION:
                    self.drag()
    
    def on_mouseup(self, button, x, y):
        self.update_selection(x, y)

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
                    if self.active_terminal_idx != self.selected_button:
                        self.active_terminal_idx = self.selected_button
                        self.redraw()
                elif self.selected_button == len(self.terminals):
                    self.create_terminal()
                    self.redraw()
            elif button == core.buttons.MIDDLE:
                if self.selected_button >= 0 and self.selected_button < len(self.terminals):
                    self.destroy_terminal(self.selected_button)
                    self.redraw()
        elif self.selected_group == self.GROUP_TERMINAL:
            if 0 <= self.active_terminal_idx < len(self.terminals):
                terminal = self.terminals[self.active_terminal_idx]
                terminal.on_mouseup(button, x - self.selector_width, y - theme.Window.CAPTION_SIZE)
    
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
        if 0 <= self.active_terminal_idx < len(self.terminals):
            terminal = self.terminals[self.active_terminal_idx]
            terminal.on_scroll(delta)
    
    def on_input(self, char):
        if 0 <= self.active_terminal_idx < len(self.terminals):
            terminal = self.terminals[self.active_terminal_idx]
            terminal.on_input(char)
    
    def on_keydown(self, key):
        if core.is_key_down(core.keys.LMENU):
            return
        if 0 <= self.active_terminal_idx < len(self.terminals):
            terminal = self.terminals[self.active_terminal_idx]
            terminal.on_keydown(key)
    
    def on_keyup(self, key):
        if 0 <= self.active_terminal_idx < len(self.terminals):
            terminal = self.terminals[self.active_terminal_idx]
            terminal.on_keyup(key)
    
    def render_terminal_selector(self, x, y, width, height):
        self.rect(x, y, x + width, y + height, theme.Selector.BG, 1)
        
        line_height = self.get_line_height(theme.Selector.FONT_SIZE)
        advance = self.get_advance(theme.Selector.FONT_SIZE)
        button_height = width

        for i in range(len(self.terminals)):
            is_active = (i == self.active_terminal_idx)
            is_hovered = self.selected_group == self.GROUP_SELECTOR and (i == self.selected_button)
            self.render_selector_button(str(i), line_height, advance, x, y + i * button_height, width, is_hovered, is_active)
        
        new_button_idx = len(self.terminals)
        is_new_button_hovered = (self.selected_group == self.GROUP_SELECTOR and new_button_idx == self.selected_button)
        new_button_y = y + new_button_idx * button_height
        self.render_selector_button("+", line_height, advance, x, new_button_y, width, is_new_button_hovered, False)

    def render_selector_button(self, text, line_height, advance, x, y, width, is_hovered, is_active):
        color = theme.Selector.BUTTON_ACTIVE if is_active else theme.Selector.BUTTON_HOVER if is_hovered else theme.Selector.BUTTON
        
        height = width
        self.rect(x, y, x + width, y + height, color, 1)
        text_max_length = int(width // advance)
        text = text[:text_max_length]
        text_width = int(advance * len(text))

        text_x = x + (width - text_width) // 2
        text_y = y + (height - line_height) // 2

        self.text(text, theme.Selector.FONT_SIZE, text_x, text_y, theme.Selector.TEXT, -1, -1, 1.0)
    
    def render_title(self, title):
        outline_thickness = theme.Window.OUTLINE_THICKNESS
        caption_size = theme.Window.CAPTION_SIZE + outline_thickness
        button_area_width = theme.Window.BUTTON_SIZE * 3
        caption_left = outline_thickness
        caption_right = self.get_width() - button_area_width - outline_thickness
        caption_width = caption_right - caption_left

        max_title_length = int(caption_width // self.get_advance(theme.Window.TITLE_FONT_SIZE))
        title = title[:max_title_length]

        font_size = theme.Window.TITLE_FONT_SIZE
        line_height = self.get_line_height(font_size)
        advance = self.get_advance(font_size)
        text_width = int(advance * len(title))

        text_x = caption_left + (caption_width - text_width) // 2
        text_y = (caption_size - line_height) // 2

        self.text(title, font_size, text_x, text_y, theme.Window.TITLE_TEXT_COLOR, -1, -1, 1.0)

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
