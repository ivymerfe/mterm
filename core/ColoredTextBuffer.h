#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <string>

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

  size_t GetLineCount() const;

  void InsertLines(size_t index, size_t count);

  void RemoveLines(size_t start_index, size_t end_index);

  void WriteToLine(size_t line_index, const char32_t* text, int length);

  void EraseInLine(size_t line_index, int start_pos, int end_pos);

  int GetLineLength(size_t line_index) const;

  std::string GetLineText(size_t line_index, int start_pos = 0, int end_pos = -1) const;

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
