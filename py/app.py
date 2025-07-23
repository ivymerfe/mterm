import core
import config
import theme

class App(core.Window):
    def __init__(self):
        super().__init__()

    def on_render(self):
        width = self.get_width()
        height = self.get_height()
        self.clear(theme.BG_COLOR)

        caption_size = config.CAPTION_SIZE
        self.outline(0, 0, width, height, theme.OUTLINE_THICKNESS, theme.OUTLINE_COLOR, 1)
        self.line(0, caption_size, width, caption_size, theme.CAPTION_UNDERLINE_THICKNESS, theme.CAPTION_UNDERLINE_COLOR, 1.0)

        size = theme.BUTTON_SIZE
        y = (caption_size / 2)
        
        min_x = width - config.MIN_BUTTON_OFFSET
        self.rect(min_x - size, y, min_x + size, y + 1, theme.MIN_BUTTON_COLOR, 1)
        
        close_x = width - config.CLOSE_BUTTON_OFFSET
        self.line(close_x - size, y - size, close_x + size, y + size, 1, theme.CLOSE_BUTTON_COLOR, 1)
        self.line(close_x - size, y + size, close_x + size, y - size, 1, theme.CLOSE_BUTTON_COLOR, 1)
    
    def run(self):
        super().run(
            font_name=config.FONT_NAME,
            width=config.WINDOW_WIDTH,
            height=config.WINDOW_HEIGHT,
            min_width=config.MIN_WINDOW_WIDTH,
            min_height=config.MIN_WINDOW_HEIGHT,
            caption_size=config.CAPTION_SIZE,
            border_size=config.BORDER_SIZE,
            button_size=config.BUTTON_SIZE,
            close_button_offset=config.CLOSE_BUTTON_OFFSET,
            max_button_offset=0,
            min_button_offset=config.MIN_BUTTON_OFFSET,
            cursor_id=core.cursors.ARROW
        )

if __name__ == "__main__":
    app = App()
    app.run()
