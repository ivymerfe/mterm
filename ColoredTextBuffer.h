#pragma once

#include <cstdint>
#include <deque>
#include <vector>

struct Fragment {
  int pos;
  int color;
  int underline_color;
  int background_color;
};

struct Line {
  std::vector<char32_t> text;
  std::vector<Fragment> fragments;
};

namespace MTerm {

class Renderer;

class ColoredTextBuffer {
 public:
  ColoredTextBuffer();

  void AddLine();

  void WriteToLine(size_t line_index, const char32_t* text, int length);

  void SetText(size_t line_index,
               int offset,
               const char32_t* content,
               int length);

  void SetColor(size_t line_index,
                int start_pos,
                int end_pos,
                int color,
                int underline_color,
                int background_color);

  void Render(Renderer& renderer,
              float left,
              float top,
              float width,
              float height,
              int x_offset_chars,
              int y_offset_lines,
              float font_size);

 private:
  static void ReplaceSubrange(std::vector<Fragment>& fragments,
                              size_t start,
                              size_t end,
                              const Fragment* replacement_ptr,
                              size_t replacement_size);

  static void MaybeAddFragment(Fragment* fragments, int& size, Fragment fragment);

  std::deque<Line> m_lines;
};

}  // namespace MTerm
