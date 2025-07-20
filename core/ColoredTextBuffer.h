#pragma once

#include <cstdint>
#include <deque>
#include <vector>

struct LineFragment {
  int pos;
  int color;
  int underline_color;
  int background_color;
};

struct ColoredLine {
  std::vector<char32_t> text;
  std::vector<LineFragment> fragments;
};

namespace MTerm {

class Window;

class ColoredTextBuffer {
 public:
  ColoredTextBuffer();

  void AddLine();

  std::deque<ColoredLine>& GetLines();

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

 private:
  static void ReplaceSubrange(std::vector<LineFragment>& fragments,
                              size_t start,
                              size_t end,
                              const LineFragment* replacement_ptr,
                              size_t replacement_size);

  static void MaybeAddFragment(LineFragment* fragments, int& size, LineFragment fragment);

  std::deque<ColoredLine> m_lines;
};

}  // namespace MTerm
