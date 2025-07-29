import base
import math
from user import theme


class SelectionType:
    NONE = 0
    LINES = 1
    BLOCK = 2


class TerminalWithSelection(base.BaseTerminal):
    def __init__(self, app, id):
        super().__init__(app, id)
        self.selection_type = SelectionType.LINES
        self.selection_start = None
        self.selection_end = None

    def get_buffer_position(self, x, y):
        x = max(0, x - self.app.get_selector_width())
        y = max(0, y - self.app.get_caption_height())
        line_height = math.ceil(self.app.get_line_height(self.font_size))
        advance = self.app.get_advance(self.font_size)
        if self.is_alt_screen:
            buffer_y = 0
        else:
            buffer_y = max(0, self.main_screen.start_pos - self.scroll_offset)
        row = int(y // line_height + buffer_y)
        col = int(x // advance)
        return row, col

    def get_selection_text(self):
        if not self.selection_start or not self.selection_end:
            return ""

        start_row, start_col = self.selection_start
        end_row, end_col = self.selection_end
        if start_row > end_row or (start_row == end_row and start_col > end_col):
            start_row, start_col, end_row, end_col = (
                end_row,
                end_col,
                start_row,
                start_col,
            )

        lines = []
        if self.selection_type == SelectionType.LINES:
            if start_row == end_row:
                line = self.current_screen.buffer.get_line_text(
                    start_row, start_col, end_col
                )
                lines.append(line.strip())
            else:
                for row in range(start_row, end_row + 1):
                    if row == start_row:
                        line = self.current_screen.buffer.get_line_text(
                            row, start_col, -1
                        )
                    elif row == end_row:
                        line = self.current_screen.buffer.get_line_text(row, 0, end_col)
                    else:
                        line = self.current_screen.buffer.get_line_text(row, 0, -1)
                    lines.append(line.strip())
        elif self.selection_type == SelectionType.BLOCK:
            for row in range(start_row, end_row + 1):
                line = self.current_screen.buffer.get_line_text(row, start_col, end_col)
                lines.append(line.strip())
        return "\n".join(lines)

    def render(self, x, y, width, height):
        super().render(x, y, width, height)
        self.render_selection(x, y, width, height)

    def render_selection(self, x, y, width, height):
        if (
            self.selection_type == SelectionType.NONE
            or not self.selection_start
            or not self.selection_end
        ):
            return

        line_height = math.ceil(self.app.get_line_height(self.font_size))
        advance = self.app.get_advance(self.font_size)
        if self.is_alt_screen:
            buffer_y = 0
        else:
            buffer_y = max(0, self.main_screen.start_pos - self.scroll_offset)

        start_row, start_col = self.selection_start
        end_row, end_col = self.selection_end
        start_row -= buffer_y
        end_row -= buffer_y
        if start_row > end_row or (start_row == end_row and start_col > end_col):
            start_row, start_col, end_row, end_col = (
                end_row,
                end_col,
                start_row,
                start_col,
            )

        start_x = x + start_col * advance
        start_y = y + start_row * line_height
        end_x = x + (end_col + 1) * advance
        end_y = y + end_row * line_height
        opacity = theme.Terminal.SELECTION_OPACITY

        if self.selection_type == SelectionType.LINES:
            if start_y == end_y:
                self.app.rect(
                    start_x,
                    start_y,
                    end_x,
                    start_y + line_height,
                    theme.Terminal.SELECTION,
                    opacity,
                )
            else:
                self.app.rect(
                    start_x,
                    start_y,
                    x + width,
                    start_y + line_height,
                    theme.Terminal.SELECTION,
                    opacity,
                )
                inner_rows = end_row - start_row - 1
                if inner_rows > 0:
                    self.app.rect(
                        x,
                        start_y + line_height,
                        x + width,
                        end_y,
                        theme.Terminal.SELECTION,
                        opacity,
                    )
                self.app.rect(
                    x,
                    end_y,
                    end_x,
                    end_y + line_height,
                    theme.Terminal.SELECTION,
                    opacity,
                )
        elif self.selection_type == SelectionType.BLOCK:
            self.app.rect(
                start_x,
                start_y,
                end_x,
                end_y + line_height,
                theme.Terminal.SELECTION,
                opacity,
            )
