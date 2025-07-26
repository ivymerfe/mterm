from core import PseudoConsole, ColoredTextBuffer
import config
import theme
import weakref

class Terminal:
    def __init__(self, app, title, num_rows, num_columns):
        self.app = app
        self.title = title
        self.console = PseudoConsole()
        self.buffer = ColoredTextBuffer()
        self.font_size = config.BASE_FONT_SIZE
        self.offset_x = 0
        self.offset_y = 0
        self.line_index = -1
        self.console.start(num_rows, num_columns, self._make_callback(Terminal.on_console_output))

    def _make_callback(self, method):
        weak_self = weakref.ref(self)
        def callback(*args, **kwargs):
            instance = weak_self()
            if instance is not None:
                return method(instance, *args, **kwargs)
        return callback

    def on_console_output(self, output):
        for line in output.split("\r\n"):
            self.buffer.add_line()
            self.line_index += 1
            text = line.replace('\x1b', 'ESC')
            self.buffer.write_to_line(self.line_index, text)
            self.buffer.set_color(self.line_index, 0, len(text), theme.Terminal.TEXT, -1, -1)
        
        self.app.redraw()

    def on_resize(self, width, height):
        pass

    def on_input(self, input_text):
        self.console.send(input_text)

    def on_key_down(self):
        pass

    def on_key_up(self, key):
        pass

    def on_scroll(self, delta):
        pass

    # def __del__(self):
    #     print(f"Terminal '{self.title}' is being destroyed.")
