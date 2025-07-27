import core
from core import PseudoConsole, ColoredTextBuffer
import theme
import weakref
import math


class Screen:
    def __init__(self):
        self.buffer = ColoredTextBuffer()
        self.start_pos = 0
        self.cursor_x = 0
        self.cursor_y = 0
        self.saved_cursor_x = 0
        self.saved_cursor_y = 0


class EscapeState:
    NONE = 0
    ESC = 1
    CSI = 2
    OSC = 3


class Terminal:
    def __init__(self, app, title):
        self.app = app
        self.title = title
        self.console = PseudoConsole()

        # Screen management
        self.main_screen = Screen()
        self.alt_screen = Screen()
        self.current_screen = self.main_screen
        self.is_alt_screen = False

        # Terminal dimensions
        self.font_size = theme.Terminal.BASE_FONT_SIZE
        self.num_rows = theme.Terminal.NUM_ROWS
        self.num_columns = theme.Terminal.NUM_COLUMNS

        # Scrolling state
        self.scroll_offset = 0
        self.line_count = 0

        # ANSI state
        self.foreground_color = theme.Terminal.TEXT
        self.background_color = -1
        self.underline_color = -1
        self.underline_enabled = False

        # ANSI parser state
        self.in_escape = False
        self.escape_state = EscapeState.NONE
        self.escape_buffer = ""

        # Initialize terminal size
        line_height = app.get_line_height(self.font_size)
        advance = app.get_advance(self.font_size)
        if line_height > 0 and advance > 0:
            self.num_rows = int(app.get_client_height() // line_height)
            self.num_columns = int(app.get_terminal_width() // advance)

        # Start the console
        self.console.start(
            self.num_rows,
            self.num_columns,
            self._make_callback(Terminal.on_console_output),
        )

    def _make_callback(self, method):
        weak_self = weakref.ref(self)

        def callback(*args, **kwargs):
            instance = weak_self()
            if instance is not None:
                return method(instance, *args, **kwargs)

        return callback

    def resize(self, width, height):
        width *= (1 - theme.Terminal.RESERVED_WIDTH)
        line_height = self.app.get_line_height(self.font_size)
        advance = self.app.get_advance(self.font_size)
        self.num_rows = int(height // line_height)
        self.num_columns = int(width // advance)
        self.console.resize(self.num_rows, self.num_columns)

    def on_console_output(self, output):
        self.process_ansi(output)
        self.app.redraw()

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
            visible_rows = self.app.get_client_height() / self.app.get_line_height(self.font_size)
            delta *= math.floor(theme.Terminal.SCROLL_SPEED * visible_rows)
            new_offset = min(self.line_count, self.scroll_offset + delta)
            if new_offset != self.scroll_offset:
                self.scroll_offset = new_offset
                self.app.redraw()

    def render(self, x, y, width, height):
        """Render the terminal"""
        self.app.rect(x, y, x + width, y + height, theme.Terminal.BG, 1)
        advance = self.app.get_advance(self.font_size)
        line_height = math.ceil(self.app.get_line_height(self.font_size))

        if self.is_alt_screen:
            self.app.text_buffer(
                self.alt_screen.buffer, x, y, width, height, 0, 0, self.font_size
            )
            cursor_x = math.floor(x + self.alt_screen.cursor_x * advance)
            cursor_y = y + self.alt_screen.cursor_y * line_height
            self.render_cursor(cursor_x, cursor_y)
        else:
            # Calculate buffer view based on scroll offset
            buffer_x = 0
            buffer_y = max(0, self.line_count - self.scroll_offset)

            # Render line numbers if enabled
            if theme.Terminal.LINE_NUMBERS:
                start_line = buffer_y + 1
                end_line = buffer_y + height // line_height
                max_len = math.floor(math.log10(max(1, end_line)) + 1)
                line_y = y

                for i in range(start_line, end_line + 1):
                    line_number = str(i)
                    self.app.text(
                        line_number,
                        self.font_size,
                        x,
                        line_y,
                        theme.Terminal.LINE_NUMBER,
                        -1,
                        -1,
                        1,
                    )
                    line_y += line_height

                # Adjust text buffer position for line numbers
                x += advance * (max_len + 1)

            # Render the main buffer
            self.app.text_buffer(
                self.main_screen.buffer,
                x,
                y,
                width,
                height,
                buffer_x,
                buffer_y,
                self.font_size,
            )
            local_cursor_y = (
                self.main_screen.cursor_y + self.main_screen.start_pos - buffer_y
            )
            if 0 <= local_cursor_y < self.num_rows:
                cursor_x = math.floor(x + self.main_screen.cursor_x * advance)
                cursor_y = y + local_cursor_y * line_height
                self.render_cursor(cursor_x, cursor_y)

    def render_cursor(self, x, y):
        """Render the cursor at the given position"""
        line_height = self.app.get_line_height(self.font_size)
        cursor_size = theme.Terminal.CURSOR_WIDTH
        self.app.rect(
            x,
            y,
            x + cursor_size,
            y + line_height,
            theme.Terminal.CURSOR,
            1,
        )

    def reset_escape_state(self):
        """Reset escape sequence parsing state"""
        self.in_escape = False
        self.escape_state = EscapeState.NONE
        self.escape_buffer = ""

    def process_ansi(self, text):
        """Process text with ANSI escape sequences"""
        text_buffer = ""

        for c in text:
            if self.in_escape:
                if ord(c) < 128:  # Only ASCII characters in escape sequences
                    self.escape_buffer += c

                    if self.escape_state == EscapeState.ESC:
                        if c == "[":
                            self.escape_state = EscapeState.CSI
                        elif c == "]":
                            self.escape_state = EscapeState.OSC
                        elif c in "78cDEHM":
                            self.handle_escape_sequence(self.escape_buffer)
                            self.reset_escape_state()
                        else:
                            self.reset_escape_state()
                    elif self.escape_state == EscapeState.CSI:
                        if 0x40 <= ord(c) <= 0x7E:  # Final byte of CSI sequence
                            self.handle_escape_sequence(self.escape_buffer)
                            self.reset_escape_state()
                    elif self.escape_state == EscapeState.OSC:
                        if c == "\x07" or (
                            c == "\\"
                            and len(self.escape_buffer) > 2
                            and self.escape_buffer[-2] == "\033"
                        ):
                            self.handle_escape_sequence(self.escape_buffer)
                            self.reset_escape_state()
                else:
                    # Non-ASCII character interrupts escape sequence
                    self.reset_escape_state()
                    text_buffer += c
            else:
                if c == "\033":  # ESC character
                    # Flush any pending text
                    if text_buffer:
                        self.insert_text(text_buffer)
                        text_buffer = ""

                    # Start collecting escape sequence
                    self.in_escape = True
                    self.escape_state = EscapeState.ESC
                    self.escape_buffer = c
                elif c == "\r":
                    # Flush any pending text
                    if text_buffer:
                        self.insert_text(text_buffer)
                        text_buffer = ""
                    self.handle_carriage_return()
                elif c == "\n":
                    # Flush any pending text
                    if text_buffer:
                        self.insert_text(text_buffer)
                        text_buffer = ""
                    self.handle_new_line()
                elif c == "\b":
                    # Flush any pending text
                    if text_buffer:
                        self.insert_text(text_buffer)
                        text_buffer = ""
                    self.handle_backspace()
                elif c == "\t":
                    # Flush any pending text
                    if text_buffer:
                        self.insert_text(text_buffer)
                        text_buffer = ""
                    self.handle_tab()
                elif ord(c) >= 32 or (
                    1 <= ord(c) <= 31 and c not in "\x07\x08\x09\x0a\x0d"
                ):
                    # Collect regular text (printable + some control chars, excluding handled ones)
                    text_buffer += c

        # Flush any remaining text
        if text_buffer:
            self.insert_text(text_buffer)

    def ensure_line_exists(self, line_index):
        """Make sure the specified line exists in the current buffer"""
        screen = self.current_screen
        if self.is_alt_screen:
            current_lines = screen.buffer.get_line_count()
            while current_lines <= line_index:
                screen.buffer.add_line()
                current_lines += 1

        else:
            current_lines = self.line_count - screen.start_pos
            while current_lines <= line_index:
                screen.buffer.add_line()
                current_lines += 1
                self.line_count += 1
                if (
                    self.scroll_offset - self.num_rows
                    < -theme.Terminal.KEEP_NEGATIVE_LINES
                ):
                    self.scroll_offset += 1

    def handle_new_line(self):
        """Process a newline character"""
        screen = self.current_screen
        screen.cursor_y += 1

        # Handle scrolling when cursor moves beyond the bottom
        if screen.cursor_y >= self.num_rows:
            if self.is_alt_screen:
                # In alt screen, just scroll contents and keep cursor at bottom
                screen.cursor_y = self.num_rows - 1
            else:
                screen.start_pos += 1
                screen.cursor_y = self.num_rows - 1
                pass

        self.ensure_line_exists(screen.cursor_y)

    def handle_carriage_return(self):
        """Process a carriage return character"""
        self.current_screen.cursor_x = 0

    def handle_backspace(self):
        """Process a backspace character - moves cursor back without deleting"""
        self.move_cursor_relative(0, -1)

    def handle_tab(self):
        """Process a tab character"""
        screen = self.current_screen
        # Move to the next tab stop (every 8 characters)
        new_x = ((screen.cursor_x // 8) + 1) * 8

        # In alt screen, don't allow cursor to go beyond right edge
        if self.is_alt_screen:
            new_x = min(new_x, self.num_columns - 1)

        self.ensure_line_exists(screen.cursor_y)
        screen.cursor_x = new_x

    def move_cursor(self, row, col):
        """Move cursor to absolute position"""
        screen = self.current_screen

        screen.cursor_y = max(0, row)
        screen.cursor_x = max(0, col)

        # In alt screen, constrain cursor to screen boundaries
        if self.is_alt_screen:
            screen.cursor_y = min(screen.cursor_y, self.num_rows - 1)
            screen.cursor_x = min(screen.cursor_x, self.num_columns - 1)

        self.ensure_line_exists(screen.cursor_y)

    def move_cursor_relative(self, rows, cols):
        """Move cursor by relative offset"""
        screen = self.current_screen

        screen.cursor_y = max(0, screen.cursor_y + rows)
        screen.cursor_x = max(0, screen.cursor_x + cols)

        # In alt screen, constrain cursor to screen boundaries
        if self.is_alt_screen:
            screen.cursor_y = min(screen.cursor_y, self.num_rows - 1)
            screen.cursor_x = min(screen.cursor_x, self.num_columns - 1)

        self.ensure_line_exists(screen.cursor_y)

    def switch_to_alt_screen(self):
        """Switch to alternate screen buffer"""
        if not self.is_alt_screen:
            self.is_alt_screen = True
            self.current_screen = self.alt_screen

            # Clear alternate screen
            line_count = self.alt_screen.buffer.get_line_count()
            if line_count > 0:
                self.alt_screen.buffer.remove_lines(0, line_count - 1)

            # Reset cursor position
            self.alt_screen.cursor_x = 0
            self.alt_screen.cursor_y = 0

            # Ensure we have enough lines
            for i in range(self.num_rows):
                self.alt_screen.buffer.add_line()

    def switch_to_main_screen(self):
        """Switch back to main screen buffer"""
        if self.is_alt_screen:
            self.is_alt_screen = False
            self.current_screen = self.main_screen

    def insert_text(self, text):
        """Insert text at current cursor position with current attributes"""
        if not text:
            return

        screen = self.current_screen
        self.ensure_line_exists(screen.cursor_y)

        # Insert the text
        screen.buffer.set_text(
            screen.cursor_y + screen.start_pos, screen.cursor_x, text
        )

        # Apply current attributes to the inserted text
        screen.buffer.set_color(
            screen.cursor_y + screen.start_pos,
            screen.cursor_x,
            screen.cursor_x + len(text) - 1,
            self.foreground_color,
            self.underline_color if self.underline_enabled else -1,
            self.background_color,
        )

        # Advance cursor
        screen.cursor_x += len(text)

    def clear_line(self, mode=0):
        """Clear part of the current line based on mode
        0: Clear from cursor to end of line
        1: Clear from start of line to cursor
        2: Clear entire line
        """
        screen = self.current_screen
        self.ensure_line_exists(screen.cursor_y)

        line_index = screen.cursor_y + screen.start_pos
        line_length = screen.buffer.get_line_length(line_index)

        if mode == 0:  # Clear from cursor to end
            if screen.cursor_x < line_length:
                screen.buffer.set_color(
                    line_index,
                    screen.cursor_x,
                    line_length - 1,
                    -1,
                    -1,
                    -1,
                )
        elif mode == 1:  # Clear from start to cursor
            if screen.cursor_x > 0:
                # Reset colors
                screen.buffer.set_color(
                    line_index,
                    0,
                    min(screen.cursor_x, line_length - 1),
                    -1,
                    -1,
                    -1,
                )
        elif mode == 2:  # Clear entire line
            screen.buffer.set_color(
                line_index,
                0,
                line_length - 1,
                -1,
                -1,
                -1,
            )

    def clear_screen(self, mode=0):
        """Clear part of the screen based on mode
        0: Clear from cursor to end of screen
        1: Clear from start of screen to cursor
        2: Clear entire screen
        """
        screen = self.current_screen

        if mode == 0:  # Clear from cursor to end of screen
            # First clear from cursor to end of current line
            self.clear_line(0)

            # Then clear all lines below cursor
            line_count = screen.buffer.get_line_count()
            line_index = screen.cursor_y + screen.start_pos + 1
            while line_index < line_count:
                screen.buffer.set_color(
                    line_index,
                    0,
                    screen.buffer.get_line_length(line_index) - 1,
                    -1,
                    -1,
                    -1,
                )
                line_index += 1

        elif mode == 1:  # Clear from start of screen to cursor
            # Clear all lines above current line
            for i in range(screen.start_pos, screen.cursor_y + screen.start_pos):
                screen.buffer.set_color(
                    i,
                    0,
                    screen.buffer.get_line_length(i) - 1,
                    -1,
                    -1,
                    -1,
                )

            # Clear current line from start to cursor
            self.clear_line(1)

        elif mode == 2:  # Clear entire screen
            for i in range(screen.start_pos, screen.buffer.get_line_count()):
                screen.buffer.set_color(
                    i,
                    0,
                    screen.buffer.get_line_length(i) - 1,
                    -1,
                    -1,
                    -1,
                )

    def insert_lines(self, count=1):
        screen = self.current_screen
        screen.buffer.insert_lines(screen.cursor_y + screen.start_pos, count)

    def delete_lines(self, count=1):
        screen = self.current_screen
        screen.buffer.remove_lines(
            screen.cursor_y + screen.start_pos,
            screen.cursor_y + screen.start_pos + count - 1,
        )

    def delete_characters(self, count=1):
        screen = self.current_screen
        screen.buffer.erase_in_line(
            screen.cursor_y + screen.start_pos,
            screen.cursor_x,
            screen.cursor_x + count - 1,
        )

    def erase_characters(self, count=1):
        screen = self.current_screen
        screen.buffer.set_color(
            screen.cursor_y + screen.start_pos,
            screen.cursor_x,
            screen.cursor_x + count - 1,
            -1,
            -1,
            -1,
        )

    def handle_escape_sequence(self, sequence):
        """Process an escape sequence"""
        if not sequence or len(sequence) < 2:
            return

        if sequence[1] == "[":  # CSI sequence
            self.handle_csi_sequence(sequence[2:])
        elif sequence[1] == "]":  # OSC sequence - ignored for now
            # OSC (Operating System Command) sequence
            # Example: ESC ] 0;title BEL or ESC ] 2;title BEL
            # Only handle set window title (0 or 2)
            parts = sequence[2:].split(";")
            if parts and parts[0] in ("0", "2"):
                # Title is everything after the first ';'
                title = ";".join(parts[1:]).rstrip("\x07")
                self.title = title
        elif len(sequence) == 2:
            command = sequence[1]
            screen = self.current_screen

            if command == "7":  # Save cursor
                screen.saved_cursor_x = screen.cursor_x
                screen.saved_cursor_y = screen.cursor_y
            elif command == "8":  # Restore cursor
                screen.cursor_x = screen.saved_cursor_x
                screen.cursor_y = screen.saved_cursor_y
                self.ensure_line_exists(screen.cursor_y)
            elif command == "c":  # Reset terminal
                self.clear_screen(2)
                self.foreground_color = theme.Terminal.TEXT
                self.background_color = -1
                self.underline_color = -1
                self.underline_enabled = False
            elif command == "D":  # Index (Line feed)
                self.handle_new_line()
            elif command == "E":  # Next line
                self.handle_new_line()
                self.handle_carriage_return()
            elif command == "M":  # Reverse index
                screen.cursor_y = max(0, screen.cursor_y - 1)

    def handle_csi_sequence(self, sequence):
        """Handle Control Sequence Introducer (CSI) sequences"""
        if not sequence:
            return

        # Extract command character (last character)
        command = sequence[-1]
        param_part = sequence[:-1]

        # Check for private mode
        is_private_mode = False
        if param_part.startswith("?"):
            is_private_mode = True
            param_part = param_part[1:]

        # Parse parameters
        params = []
        if param_part:
            for part in param_part.split(";"):
                try:
                    params.append(int(part) if part else 0)
                except ValueError:
                    params.append(0)

        if not params:
            params = [0]

        # Handle the command
        if is_private_mode:
            self.handle_private_mode(params, command)
        else:
            self.handle_csi(params, command)

    def handle_private_mode(self, params, command):
        """Handle DEC private mode sequences"""
        if command == "h":  # Set mode
            for param in params:
                if param in (47, 1047, 1049):  # Switch to alt screen
                    self.switch_to_alt_screen()
        elif command == "l":  # Reset mode
            for param in params:
                if param in (47, 1047, 1049):  # Switch to main screen
                    self.switch_to_main_screen()

    def handle_csi(self, params, command):
        """Handle standard CSI sequences"""
        if command == "A":  # Cursor Up
            self.move_cursor_relative(-max(1, params[0]), 0)
        elif command == "B":  # Cursor Down
            self.move_cursor_relative(max(1, params[0]), 0)
        elif command == "C":  # Cursor Forward
            self.move_cursor_relative(0, max(1, params[0]))
        elif command == "D":  # Cursor Back
            self.move_cursor_relative(0, -max(1, params[0]))
        elif command == "E":  # Cursor Next Line
            for _ in range(max(1, params[0])):
                self.handle_new_line()
            self.handle_carriage_return()
        elif command == "F":  # Cursor Previous Line
            self.move_cursor_relative(-max(1, params[0]), 0)
            self.handle_carriage_return()
        elif command == "G":  # Cursor Horizontal Absolute
            screen = self.current_screen
            screen.cursor_x = max(0, params[0] - 1)
            if self.is_alt_screen:
                screen.cursor_x = min(screen.cursor_x, self.num_columns - 1)
        elif command in ("H", "f"):  # Cursor Position
            row = params[0] - 1 if params else 0
            col = params[1] - 1 if len(params) > 1 else 0
            self.move_cursor(row, col)
        elif command == "J":  # Erase in Display
            self.clear_screen(params[0] if params else 0)
        elif command == "K":  # Erase in Line
            self.clear_line(params[0] if params else 0)
        elif command == "L":  # Insert Lines
            self.insert_lines(max(1, params[0]) if params else 1)
        elif command == "M":  # Delete Lines
            self.delete_lines(max(1, params[0]) if params else 1)
        elif command == "P":  # Delete Characters
            self.delete_characters(max(1, params[0]) if params else 1)
        elif command == "X":  # Erase Characters
            self.erase_characters(max(1, params[0]) if params else 1)
        elif command == "d":  # Line Position Absolute
            screen = self.current_screen
            screen.cursor_y = max(0, params[0] - 1)
            if self.is_alt_screen:
                screen.cursor_y = min(screen.cursor_y, self.num_rows - 1)
            self.ensure_line_exists(screen.cursor_y)
        elif command == "m":  # Select Graphic Rendition
            self.set_text_attributes(params)
        elif command == "s":  # Save cursor position
            screen = self.current_screen
            screen.saved_cursor_x = screen.cursor_x
            screen.saved_cursor_y = screen.cursor_y
        elif command == "u":  # Restore cursor position
            screen = self.current_screen
            screen.cursor_x = screen.saved_cursor_x
            screen.cursor_y = screen.saved_cursor_y
            self.ensure_line_exists(screen.cursor_y)

    def set_text_attributes(self, params):
        """Set text rendering attributes based on SGR parameters"""
        i = 0
        while i < len(params):
            param = params[i]

            if param == 0:  # Reset all attributes
                self.foreground_color = theme.Terminal.TEXT
                self.background_color = -1
                self.underline_color = -1
                self.underline_enabled = False
            elif param == 4:  # Underline
                self.underline_enabled = True
                self.underline_color = self.foreground_color
            elif param == 24:  # No underline
                self.underline_enabled = False
                self.underline_color = -1
            elif 30 <= param <= 37:  # Foreground color (standard)
                self.foreground_color = theme.Terminal.ANSI_COLORS[param - 30]
            elif param == 38:  # Extended foreground color
                if i + 2 < len(params) and params[i + 1] == 5:
                    # 8-bit color
                    self.foreground_color = self.get_256_color(params[i + 2])
                    i += 2
                elif i + 4 < len(params) and params[i + 1] == 2:
                    # 24-bit RGB
                    r, g, b = params[i + 2], params[i + 3], params[i + 4]
                    self.foreground_color = (r << 16) | (g << 8) | b
                    i += 4
            elif param == 39:  # Default foreground color
                self.foreground_color = theme.Terminal.TEXT
            elif 40 <= param <= 47:  # Background color (standard)
                self.background_color = theme.Terminal.ANSI_COLORS[param - 40]
            elif param == 48:  # Extended background color
                if i + 2 < len(params) and params[i + 1] == 5:
                    # 8-bit color
                    self.background_color = self.get_256_color(params[i + 2])
                    i += 2
                elif i + 4 < len(params) and params[i + 1] == 2:
                    # 24-bit RGB
                    r, g, b = params[i + 2], params[i + 3], params[i + 4]
                    self.background_color = (r << 16) | (g << 8) | b
                    i += 4
            elif param == 49:  # Default background color
                self.background_color = -1
            elif 90 <= param <= 97:  # Foreground color (bright)
                self.foreground_color = theme.Terminal.ANSI_BRIGHT_COLORS[param - 90]
            elif 100 <= param <= 107:  # Background color (bright)
                self.background_color = theme.Terminal.ANSI_BRIGHT_COLORS[param - 100]

            i += 1

    def get_256_color(self, index):
        """Convert 8-bit color index to RGB color"""
        if index < 8:  # Standard colors
            return theme.Terminal.ANSI_COLORS[index]
        elif index < 16:  # Bright colors
            return theme.Terminal.ANSI_BRIGHT_COLORS[index - 8]
        elif index >= 232 and index < 256:  # Grayscale
            level = ((index - 232) * 255) // 23
            return (level << 16) | (level << 8) | level
        else:  # 6x6x6 color cube
            index -= 16
            r = (index // 36) % 6
            g = (index // 6) % 6
            b = index % 6

            r = r * 40 + 55 if r > 0 else 0
            g = g * 40 + 55 if g > 0 else 0
            b = b * 40 + 55 if b > 0 else 0

            return (r << 16) | (g << 8) | b
