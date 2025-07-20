#include "ColoredTextBuffer.h"

#include <algorithm>

namespace MTerm {

ColoredTextBuffer::ColoredTextBuffer() {}

void ColoredTextBuffer::AddLine() {
  m_lines.emplace_back();
}

std::deque<ColoredLine>& ColoredTextBuffer::GetLines() {
  return m_lines;
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

void ColoredTextBuffer::ReplaceSubrange(std::vector<LineFragment>& fragments,
                                        size_t start,
                                        size_t end,
                                        const LineFragment* replacement_ptr,
                                        size_t replacement_size) {
  size_t old_len = end - start;
  size_t new_len = replacement_size;
  size_t min_len = std::min(old_len, new_len);

  // Overwrite the common part
  std::copy_n(replacement_ptr, min_len, fragments.begin() + start);

  if (new_len > old_len) {
    // Insert the remaining new elements
    fragments.insert(fragments.begin() + start + old_len,
                     replacement_ptr + old_len, replacement_ptr + new_len);
  } else if (new_len < old_len) {
    // Erase the leftover elements
    fragments.erase(fragments.begin() + start + new_len,
                    fragments.begin() + end);
  }
}

void ColoredTextBuffer::MaybeAddFragment(LineFragment* fragments,
                                         int& size,
                                         LineFragment fragment) {
  if (size == 0) {
    fragments[size++] = fragment;
    return;
  }
  LineFragment& back_fragment = fragments[size-1];
  if (fragment.color != back_fragment.color ||
      fragment.underline_color != back_fragment.underline_color ||
      fragment.background_color != back_fragment.background_color) {
    fragments[size++] = fragment;
  }
}

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
      [](int pos, const LineFragment& frag) { return pos < frag.pos; });

  auto it_end = std::upper_bound(
      fragments.begin(), fragments.end(), end_pos,
      [](int pos, const LineFragment& frag) { return pos < frag.pos; });

  // Since we want last element which is <= start_pos
  int index_start = std::distance(fragments.begin(), it_start) - 1;
  int index_end = std::distance(fragments.begin(), it_end);

  LineFragment new_fragments[5];
  int new_fragments_size = 0;

  LineFragment& first = fragments[index_start];
  if (index_start > 0) {
    LineFragment& prev = fragments[index_start - 1];
    MaybeAddFragment(new_fragments, new_fragments_size, prev);
    index_start--;
  }
  if (first.pos < start_pos) {
    MaybeAddFragment(new_fragments, new_fragments_size, first);
  }
  MaybeAddFragment(new_fragments, new_fragments_size,
                   {start_pos, color, underline_color, background_color});

  int moved_right = end_pos + 1;
  int next_begin;
  if (index_end == fragments.size()) {
    next_begin = line_last_pos + 1;
  } else {
    next_begin = fragments[index_end].pos;
  }
  if (moved_right < next_begin) {
    LineFragment last = fragments[index_end - 1];
    last.pos = moved_right;
    MaybeAddFragment(new_fragments, new_fragments_size, last);
  }
  if (index_end < fragments.size()) {
    MaybeAddFragment(new_fragments, new_fragments_size, fragments[index_end]);
    index_end += 1;  // Replace this element too
  }

  ReplaceSubrange(fragments, index_start, index_end, new_fragments,
                  new_fragments_size);
}

}  // namespace MTerm
