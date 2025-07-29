import core
import user.theme as theme
from . import selector_color_helper


class BaseApp(core.Window):
    BUTTON_CAPTION = 0
    BUTTON_CLOSE = 1
    BUTTON_MAXIMIZE = 2
    BUTTON_MINIMIZE = 3

    def __init__(self):
        super().__init__()
        self.terminals = []
        self.active_terminal = None

        self.selected_ctl_button = -1
        self.selector_hovered_button = -1
        self.current_cursor = core.cursors.ARROW

    def get_client_width(self):
        return self.get_width()

    def get_caption_height(self):
        return theme.Window.CAPTION_SIZE

    def get_client_height(self):
        return self.get_height() - theme.Window.CAPTION_SIZE

    def get_selector_width(self):
        return theme.Selector.WIDTH

    def get_terminal_width(self):
        return self.get_client_width() - self.get_selector_width()

    def create_terminal(self):
        pass

    def destroy_terminal(self, terminal):
        if terminal in self.terminals:
            idx = self.terminals.index(terminal)
            new_idx = max(0, idx - 1)
            self.terminals.remove(terminal)
            if terminal == self.active_terminal:
                if self.terminals:
                    self.active_terminal = self.terminals[new_idx]
                else:
                    self.active_terminal = None

    def on_render(self):
        self.render_frame()

        self.selector_width = self.get_selector_width()
        self.terminal_width = self.get_terminal_width()
        height = self.get_client_height()
        y = theme.Window.CAPTION_SIZE
        self.render_terminal_selector(0, y, self.selector_width, height)
        if self.active_terminal is not None:
            self.active_terminal.render(
                self.selector_width, y, self.terminal_width, height
            )
            self.render_title(self.active_terminal.title)

    def set_cursor(self, cursor_id: int) -> None:
        if self.current_cursor != cursor_id:
            self.current_cursor = cursor_id
            super().set_cursor(cursor_id)

    def update_selection(self, x, y):
        selected_button = -1

        caption_size = theme.Window.CAPTION_SIZE
        if y <= caption_size:
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
            self.set_cursor(core.cursors.ARROW)

        if self.selected_ctl_button != selected_button:
            self.selected_ctl_button = selected_button
            return True
        return False

    def on_mousemove(self, x, y):
        if self.update_selection(x, y):
            self.redraw()

    def on_mousedown(self, button, x, y):
        if (
            button == core.buttons.LEFT
            and self.selected_ctl_button == self.BUTTON_CAPTION
        ):
            self.drag()

    def on_mouseup(self, button, x, y):
        if button == core.buttons.LEFT:
            if self.selected_ctl_button == self.BUTTON_CLOSE:
                self.destroy()
            elif self.selected_ctl_button == self.BUTTON_MAXIMIZE:
                if self.is_maximized():
                    self.restore()
                else:
                    self.maximize()
            elif self.selected_ctl_button == self.BUTTON_MINIMIZE:
                self.minimize()

    def on_doubleclick(self, button, x, y):
        if (
            button == core.buttons.LEFT
            and self.selected_ctl_button == self.BUTTON_CAPTION
        ):
            if self.is_maximized():
                self.restore()
            else:
                self.maximize()

    def on_mouseleave(self):
        self.selected_ctl_button = -1
        self.redraw()

    def render_terminal_selector(self, x, y, width, height):
        self.rect(x, y, x + width, y + height, theme.Selector.BG, 1)

        line_height = self.get_line_height(theme.Selector.FONT_SIZE)
        advance = self.get_advance(theme.Selector.FONT_SIZE)
        button_height = width

        for i, terminal in enumerate(self.terminals):
            is_active = terminal == self.active_terminal
            is_hovered = i == self.selector_hovered_button
            self.render_selector_button(
                str(terminal.id),
                terminal.id + 1,
                line_height,
                advance,
                x,
                y + i * button_height,
                width,
                is_hovered,
                is_active,
            )

        new_button_idx = len(self.terminals)
        is_new_button_hovered = new_button_idx == self.selector_hovered_button
        new_button_y = y + new_button_idx * button_height
        self.render_selector_button(
            "+",
            0,
            line_height,
            advance,
            x,
            new_button_y,
            width,
            is_new_button_hovered,
            False,
        )

    def render_selector_button(
        self, text, color_idx, line_height, advance, x, y, width, is_hovered, is_active
    ):
        if is_active:
            color = selector_color_helper.get_active_color(color_idx)
        elif is_hovered:
            color = selector_color_helper.get_hovered_color(color_idx)
        else:
            color = selector_color_helper.get_normal_color(color_idx)

        height = width
        self.rect(x, y, x + width, y + height, color, 1)
        
        text_max_length = int(width // advance)
        text = text[:text_max_length]
        text_width = int(advance * len(text))

        text_x = x + (width - text_width) // 2
        text_y = y + (height - line_height) // 2

        self.text(
            text,
            theme.Selector.FONT_SIZE,
            text_x,
            text_y,
            theme.Selector.TEXT,
            -1,
            -1,
            1.0,
        )

    def render_title(self, title):
        caption_size = theme.Window.CAPTION_SIZE
        button_area_width = theme.Window.BUTTON_SIZE * 3
        caption_width = self.get_width() - button_area_width

        max_title_length = int(
            caption_width // self.get_advance(theme.Window.TITLE_FONT_SIZE)
        )
        title = title[:max_title_length]

        font_size = theme.Window.TITLE_FONT_SIZE
        line_height = self.get_line_height(font_size)
        advance = self.get_advance(font_size)
        text_width = int(advance * len(title))

        text_x = (caption_width - text_width) // 2
        text_y = (caption_size - line_height) // 2

        self.text(
            title, font_size, text_x, text_y, theme.Window.TITLE_TEXT_COLOR, -1, -1, 1.0
        )

    def render_frame(self):
        self.clear(theme.Window.BG)

        width = self.get_width()
        caption_size = theme.Window.CAPTION_SIZE

        size = theme.Window.BUTTON_ICON_SIZE
        full_size = theme.Window.BUTTON_SIZE
        y = caption_size // 2

        center_offset = full_size // 2
        min_left = width - full_size * 3
        max_left = width - full_size * 2
        close_left = width - full_size

        if self.selected_ctl_button == self.BUTTON_CLOSE:
            self.rect(
                close_left,
                0,
                close_left + full_size,
                caption_size,
                theme.Window.BUTTON_HOVER,
                1,
            )
        elif self.selected_ctl_button == self.BUTTON_MAXIMIZE:
            self.rect(
                max_left,
                0,
                max_left + full_size,
                caption_size,
                theme.Window.BUTTON_HOVER,
                1,
            )
        elif self.selected_ctl_button == self.BUTTON_MINIMIZE:
            self.rect(
                min_left,
                0,
                min_left + full_size,
                caption_size,
                theme.Window.BUTTON_HOVER,
                1,
            )

        min_x = min_left + center_offset
        self.rect(min_x - size, y, min_x + size, y + 1, theme.Window.MIN_BUTTON, 1)

        max_x = max_left + center_offset
        self.outline(
            max_x - size,
            y - size,
            max_x + size,
            y + size,
            1,
            theme.Window.MAX_BUTTON,
            1,
        )

        close_x = close_left + center_offset
        self.line(
            close_x - size,
            y - size,
            close_x + size,
            y + size,
            1,
            theme.Window.CLOSE_BUTTON,
            1,
        )
        self.line(
            close_x + size,
            y - size,
            close_x - size,
            y + size,
            1,
            theme.Window.CLOSE_BUTTON,
            1,
        )

    def main(self):
        self.create_terminal()
        self.run(
            font_name=theme.Window.FONT,
            width=theme.Window.WIDTH,
            height=theme.Window.HEIGHT,
            min_width=theme.Window.MIN_WIDTH,
            min_height=theme.Window.MIN_HEIGHT,
            border_size=theme.Window.BORDER_SIZE,
            cursor_id=core.cursors.ARROW,
        )


if __name__ == "__main__":
    app = BaseApp()
    app.main()
