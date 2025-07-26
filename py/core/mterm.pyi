from typing import Callable, Optional, List

# Type aliases для удобства
RenderCallback = Callable[[], None]
ResizeCallback = Callable[[int, int], None]
KeyCallback = Callable[[int], None]
InputCallback = Callable[[str], None]
MouseMoveCallback = Callable[[int, int], None]
MouseButtonCallback = Callable[[int, int, int], None]
ScrollCallback = Callable[[int, int, int], None]
MouseLeaveCallback = Callable[[], None]
ConsoleDataCallback = Callable[[str], None]

class LineFragment:
    pos: int
    color: int
    underline_color: int
    background_color: int

    def __init__(self) -> None: ...


class ColoredLine:
    text: str
    fragments: List[LineFragment]

    def __init__(self) -> None: ...


class Config:
    font_name: Optional[str]
    window_width: int
    window_height: int
    window_min_width: int
    window_min_height: int
    border_size: int
    cursor_id: int

    render_callback: Optional[RenderCallback]
    resize_callback: Optional[ResizeCallback]
    keydown_callback: Optional[KeyCallback]
    keyup_callback: Optional[KeyCallback]
    input_callback: Optional[InputCallback]
    mousemove_callback: Optional[MouseMoveCallback]
    mousedown_callback: Optional[MouseButtonCallback]
    mouseup_callback: Optional[MouseButtonCallback]
    doubleclick_callback: Optional[MouseButtonCallback]
    scroll_callback: Optional[ScrollCallback]
    mouse_leave_callback: Optional[MouseLeaveCallback]

    def __init__(self) -> None: ...


class PseudoConsole:
    def __init__(self) -> None: ...

    def start(self, num_rows: int, num_columns: int, callback: ConsoleDataCallback) -> bool: ...

    def send(self, data: str) -> bool: ...

    def resize(self, num_rows: int, num_columns: int) -> None: ...

    def close(self) -> None: ...


class ColoredTextBuffer:
    def __init__(self) -> None: ...

    def add_line(self) -> None: ...

    def get_lines(self) -> List[ColoredLine]: ...

    def write_to_line(self, line_index: int, text: str) -> None: ...

    def set_text(self, line_index: int, offset: int, content: str) -> None: ...

    def set_color(
            self,
            line_index: int,
            start_pos: int,
            end_pos: int,
            color: int,
            underline_color: int,
            background_color: int
    ) -> None: ...


class Window:
    def __init__(self) -> None: ...

    def create(self, config: Config) -> int: ...

    def destroy(self) -> None: ...

    def set_cursor(self, cursor_id: int) -> None: ...

    def drag(self) -> None: ...

    def maximize(self) -> None: ...

    def minimize(self) -> None: ...

    def restore(self) -> None: ...

    def is_maximized(self) -> bool: ...

    def redraw(self) -> None: ...

    def get_width(self) -> int: ...

    def get_height(self) -> int: ...

    def clear(self, color: int) -> None: ...

    def text(
            self,
            text: str,
            font_size: float,
            x: float,
            y: float,
            color: int,
            underline_color: int = -1,
            background_color: int = -1,
            opacity: float = 1.0
    ) -> None: ...

    def line(
            self,
            start_x: float,
            start_y: float,
            end_x: float,
            end_y: float,
            thickness: float,
            color: int,
            opacity: float = 1.0
    ) -> None: ...

    def rect(
            self,
            left: float,
            top: float,
            right: float,
            bottom: float,
            color: int,
            opacity: float = 1.0
    ) -> None: ...

    def outline(
            self,
            left: float,
            top: float,
            right: float,
            bottom: float,
            thickness: float,
            color: int,
            opacity: float = 1.0
    ) -> None: ...

    def text_buffer(
            self,
            buffer: ColoredTextBuffer,
            left: float,
            top: float,
            width: float,
            height: float,
            x_offset_chars: int,
            y_offset_lines: int,
            font_size: float
    ) -> None: ...

    def get_advance(self, font_size: float) -> float: ...

    def get_line_width(self, font_size: float, num_chars: int) -> float: ...

    def get_line_height(self, font_size: float) -> float: ...


def is_key_down(key: int) -> bool: ...

def clipboard_copy(text: str) -> None: ...

def clipboard_paste() -> str: ...

# Константы
PTY_BUFFER_SIZE: int
TEXT_BUFFER_SIZE: int
