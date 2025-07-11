#include "ColoredTextBuffer.h"

#include <algorithm>

#include "Renderer.h"

namespace MTerm {

ColoredTextBuffer::ColoredTextBuffer() {}

void ColoredTextBuffer::AddLine() {
  m_lines.emplace_back();
}

void ColoredTextBuffer::WriteToLine(size_t line_index,
                                    const char32_t* text,
                                    int length) {
  if (line_index >= m_lines.size() || length <= 0 || !text)
    return;
  auto& line = m_lines[line_index];
  line.text.insert(line.text.end(), text, text + length);

  if (line.fragments.empty()) {
    line.fragments.push_back({0, 0, 0, 0});
  }
}

void ColoredTextBuffer::SetText(size_t line_index,
                                int offset,
                                const char32_t* content,
                                int length) {
  if (line_index >= m_lines.size() || offset < 0 || length <= 0 || !content)
    return;

  auto& line = m_lines[line_index];
  size_t required = static_cast<size_t>(offset + length);
  if (line.text.size() < required) {
    line.text.resize(required, U' ');
  }

  for (int i = 0; i < length; ++i) {
    line.text[offset + i] = content[i];
  }

  if (line.fragments.empty()) {
    line.fragments.push_back({0, 0, 0, 0});
  }
}

// TODO: Not verified.
void ColoredTextBuffer::SetColor(size_t line_index,
                                 int start_pos,
                                 int end_pos,
                                 int color,
                                 int underline_color,
                                 int background_color) {
  if (line_index >= m_lines.size() || start_pos < 0)
    return;

  auto& line = m_lines[line_index];
  int line_last_pos = static_cast<int>(line.text.size() - 1);
  end_pos = std::min(end_pos, line_last_pos);
  if (start_pos > end_pos)
    return;

  auto& fragments = line.fragments;
  if (fragments.empty()) {
    fragments.push_back({0, 0, 0, 0});
  }

  auto it_start = std::upper_bound(
      fragments.begin(), fragments.end(), start_pos,
      [](int pos, const Fragment& frag) { return pos < frag.pos; });

  auto it_end = std::upper_bound(
      fragments.begin(), fragments.end(), end_pos,
      [](int pos, const Fragment& frag) { return pos < frag.pos; });

  // Since we want last element which is <= start_pos
  int index_start = std::distance(fragments.begin(), it_start) - 1;
  int index_end = std::distance(fragments.begin(), it_end) - 1;

  int index_current = index_start;
  int erase_start = index_start + 1;
  int erase_end = index_end;

  int moved_right = end_pos + 1;
  int next_begin;
  if (index_end == fragments.size() - 1) {
    next_begin = line_last_pos + 1;
  } else {
    next_begin = fragments[index_end + 1].pos;
  }
  if (moved_right < next_begin) {
    Fragment& last = fragments[index_end];
    if (index_end > index_start) {
      last.pos = moved_right;
      erase_end--;
    } else {
      fragments.insert(fragments.begin() + erase_end + 1,
                       {moved_right, last.color, last.underline_color,
                        last.background_color});
    }
  }

  Fragment& prev = fragments[index_start];
  if (prev.pos == start_pos) {
    prev.color = color;
    prev.underline_color = underline_color;
    prev.background_color = background_color;
  } else {
    if (prev.color != color || prev.underline_color != underline_color ||
        prev.background_color != background_color) {
      fragments.insert(fragments.begin() + erase_start,
                       {start_pos, color, underline_color, background_color});
      index_current++;
      erase_start++;
      erase_end++;
    }
  }

  if (erase_start <= erase_end) {
    fragments.erase(fragments.begin() + erase_start,
                    fragments.begin() + erase_end);
  }
  if (index_current < fragments.size() - 1) {
    Fragment& next = fragments[index_current + 1];
    if (next.color == color && next.underline_color == underline_color &&
        next.background_color == background_color) {
      fragments.erase(fragments.begin() + index_current + 1);
    }
  }
}

// AI generated
void ColoredTextBuffer::Render(Renderer& renderer,
                               float left,
                               float top,
                               float width,
                               float height,
                               int x_offset_chars,
                               int y_offset_lines,
                               float font_size) {
  float y = top;
  float line_height = ceil(renderer.GetLineHeight(font_size));
  float advance = renderer.GetAdvance(font_size);

  int max_visible_chars = static_cast<int>(width / advance);

  for (size_t i = y_offset_lines; i < m_lines.size(); ++i) {
    if (y > top + height)
      break;

    const auto& line = m_lines[i];
    const auto& text = line.text;
    const auto& fragments = line.fragments;

    // Binary search for first relevant fragment
    auto frag_less = [](const Fragment& frag, int pos) {
      return frag.pos < pos;
    };
    auto it = std::lower_bound(fragments.begin(), fragments.end(),
                               x_offset_chars, frag_less);

    if (it != fragments.begin() &&
        (it == fragments.end() || it->pos > x_offset_chars)) {
      --it;
    }

    int remaining_chars = max_visible_chars;
    for (; it != fragments.end(); ++it) {
      int frag_start = it->pos;
      size_t frag_index = std::distance(fragments.begin(), it);
      int frag_end = (frag_index + 1 < fragments.size())
                         ? fragments[frag_index + 1].pos
                         : static_cast<int>(text.size());

      if (frag_end <= x_offset_chars)
        continue;
      if (frag_start >= static_cast<int>(text.size()))
        break;

      int visible_start = std::max(frag_start, x_offset_chars);
      int max_end = visible_start + remaining_chars;
      int visible_end =
          std::min({frag_end, static_cast<int>(text.size()), max_end});
      int visible_len = visible_end - visible_start;

      if (visible_len <= 0)
        break;  // No more visible chars in this line, stop rendering

      float x = left + advance * (visible_start - x_offset_chars);

      renderer.Text(&text[visible_start], visible_len, font_size, x, y,
                    it->color, it->underline_color, it->background_color, 1.0f);

      remaining_chars -= visible_len;
    }

    y += line_height;
  }
}

}  // namespace MTerm
