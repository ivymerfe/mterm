#pragma once

#include <functional>
#include <memory>

namespace Mterm {

class Renderer {
 public:
  Renderer();
  ~Renderer();

  bool Init(void* window,
            const wchar_t* font_name,
            std::function<void()> render_callback);
  void Destroy();

  bool IsInitialized();

  void Redraw();

  void Resize(unsigned int width, unsigned int height);
  int GetWidth();
  int GetHeight();

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

  float GetAdvance(float font_size) const;
  float GetLineWidth(float font_size, int num_chars) const;
  float GetLineHeight(float font_size) const;

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace Mterm
