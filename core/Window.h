#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ColoredTextBuffer.h"

namespace MTerm {

constexpr auto TEXT_BUFFER_SIZE = 1024 * 1024;

struct Config {
  std::wstring font_name;
  int window_width;
  int window_height;
  int window_min_width;
  int window_min_height;
  int caption_size;
  int border_size;
  int button_size;
  int close_button_offset;
  int max_button_offset;
  int min_button_offset;
  int cursor_id;

  std::function<void()> render_callback;
  std::function<void(int width, int height)> resize_callback;
  std::function<
      void(int keycode, bool control_down, bool shift_down, bool alt_down)>
      keydown_callback;
  std::function<
      void(int keycode, bool control_down, bool shift_down, bool alt_down)>
      keyup_callback;
  std::function<void(char32_t chr)> input_callback;
  std::function<void(int x, int y)> mousemove_callback;
  std::function<void(int button, int x, int y)> mousedown_callback;
  std::function<void(int button, int x, int y)> mouseup_callback;
  std::function<void(int delta, int x, int y)> scroll_callback;
};

class Window {
 public:
  Window();
  ~Window();

  int Create(Config config);
  void Destroy();

  void SetCursor(int cursor_id);

  void Redraw();

  void Resize(unsigned int width, unsigned int height);
  int GetWidth() const;
  int GetHeight() const;

  void Clear(int color);

  void Text(const char32_t* text,
            int length,
            float font_size,
            float x,
            float y,
            int color,
            int underline_color,
            int background_color,
            float opacity);

  void Line(float start_x,
            float start_y,
            float end_x,
            float end_y,
            float thickness,
            int color,
            float opacity);

  void Rect(float left,
            float top,
            float right,
            float bottom,
            int color,
            float opacity);

  void Outline(float left,
               float top,
               float right,
               float bottom,
               float thickness,
               int color,
               float opacity);

  void TextBuffer(ColoredTextBuffer* buffer,
                  float left,
                  float top,
                  float width,
                  float height,
                  int x_offset_chars,
                  int y_offset_lines,
                  float font_size);

  float GetAdvance(float font_size) const;
  float GetLineWidth(float font_size, int num_chars) const;
  float GetLineHeight(float font_size) const;

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace MTerm
