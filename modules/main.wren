import "renderer" for Renderer
import "config" for Config

class App {
  foreign static getWidth()
  foreign static getHeight()

  construct new() {
    _renderer = Renderer.new()
  }

  render() {
    _renderer.clear(Config.background_color)
    var size = Config.button_size
    var caption_size = Config.caption_size
    var y = (caption_size / 2).floor
    var width = App.getWidth()
    var height = App.getHeight()

    _renderer.outline(0, 0, width, height, Config.outline_thickness, Config.outline_color, 1)
    _renderer.line(0, caption_size, width, caption_size, Config.caption_underline_thickness, Config.caption_underline_color, 1)

    var min_x = width - Config.min_button_offset
    var close_x = width - Config.close_button_offset
    _renderer.rect(min_x - size, y, min_x + size, y + 1, Config.min_button_color, 1)
    _renderer.line(close_x - size, y - size, close_x + size, y + size, 1, Config.close_button_color, 1)
    _renderer.line(close_x - size, y + size, close_x + size, y - size, 1, Config.close_button_color, 1)
  }
}
