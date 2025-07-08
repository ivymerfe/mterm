#include "terminal_renderer.h"

#include <algorithm>
#include <charconv>
#include <sstream>

#include "mterm.h"
#include "utils.h"

#undef min
#undef max

TerminalRenderer::TerminalRenderer(Mterm* mterm) : m_mterm(mterm) {
  SetTerminalSize(mterm->GetNumRows(), mterm->GetNumColumns());
}

void TerminalRenderer::SetTerminalSize(int rows, int cols) {
  if (m_rows == rows && m_cols == cols) {
    return;
  }

  std::unique_lock lock(m_mutex);
  m_rows = rows;
  m_cols = cols;

  // Считаем пустые строки с конца
  int empty_lines_at_end = 0;
  for (int i = static_cast<int>(m_mainScreen.lines.size()) - 1; i >= 0; --i) {
    if (m_mainScreen.lines[i].glyphs.empty() &&
        m_mainScreen.lines[i].fragments.empty()) {
      empty_lines_at_end++;
    } else {
      break;
    }
  }

  while (static_cast<int>(m_mainScreen.lines.size()) - empty_lines_at_end > rows) {
    m_scrollback.push_back(std::move(m_mainScreen.lines.front()));
    m_mainScreen.lines.erase(m_mainScreen.lines.begin());
    if (m_scrollback.size() > MAX_SCROLLBACK_LINES) {
      m_scrollback.pop_front();
    }
    if (m_mainScreen.cursorY > 0) {
      m_mainScreen.cursorY--;
    }
  }

  m_mainScreen.lines.resize(rows);
  m_mainScreen.cursorX = std::min(m_mainScreen.cursorX, cols - 1);
  m_mainScreen.cursorY = std::min(m_mainScreen.cursorY, rows - 1);

  m_alternativeScreen.lines.clear();
  m_alternativeScreen.lines.resize(rows);
  m_alternativeScreen.cursorX = std::min(m_alternativeScreen.cursorX, cols - 1);
  m_alternativeScreen.cursorY = std::min(m_alternativeScreen.cursorY, rows - 1);
}

void TerminalRenderer::ProcessAnsi(const char* text, int length) {
  std::vector<char> utf8(text, text + length);
  std::vector<char32_t> input = Utf8ToUtf32(utf8);

  std::vector<char32_t> text_buffer;
  for (char32_t c : input) {
    if (m_inEscape) {
      if (c < 128) {
        m_escapeBuffer += static_cast<char>(c);

        if (m_escapeState == EscapeState::ESC) {
          if (c == '[') {
            m_escapeState = EscapeState::CSI;
          } else if (c == ']') {
            m_escapeState = EscapeState::OSC;
          } else if (c == '7' || c == '8' || c == 'c' || c == 'D' || c == 'E' ||
                     c == 'H' || c == 'M') {
            ProcessEscapeSequence(m_escapeBuffer);
            ResetEscapeState();
          } else {
            ResetEscapeState();
          }
        } else if (m_escapeState == EscapeState::CSI) {
          if (c >= 0x40 && c <= 0x7E) {
            ProcessEscapeSequence(m_escapeBuffer);
            ResetEscapeState();
          }
        } else if (m_escapeState == EscapeState::OSC) {
          if (c == 0x07 ||
              (c == '\\' && m_escapeBuffer.size() > 2 &&
               m_escapeBuffer[m_escapeBuffer.size() - 2] == '\033')) {
            ProcessEscapeSequence(m_escapeBuffer);
            ResetEscapeState();
          }
        }
      } else {
        ResetEscapeState();
        text_buffer.push_back(c);
      }
    } else {
      if (c == '\033') {
        if (!text_buffer.empty()) {
          InsertText(text_buffer);
          text_buffer.clear();
        }
        m_inEscape = true;
        m_escapeState = EscapeState::ESC;
        m_escapeBuffer.clear();
        m_escapeBuffer += static_cast<char>(c);
      } else if (c == '\r') {
        if (!text_buffer.empty()) {
          InsertText(text_buffer);
          text_buffer.clear();
        }
        HandleCarriageReturn();
      } else if (c == '\n') {
        if (!text_buffer.empty()) {
          InsertText(text_buffer);
          text_buffer.clear();
        }
        HandleNewLine();
      } else if (c == '\b') {
        if (!text_buffer.empty()) {
          InsertText(text_buffer);
          text_buffer.clear();
        }
        HandleBackspace();
      } else if (c == '\t') {
        if (!text_buffer.empty()) {
          InsertText(text_buffer);
          text_buffer.clear();
        }
        HandleTab();
      } else if (c >= 32 || (c >= 1 && c <= 31 && c != 7 && c != 8 && c != 9 &&
                             c != 10 && c != 13)) {
        text_buffer.push_back(c);
      }
    }
  }

  if (!text_buffer.empty()) {
    InsertText(text_buffer);
  }
}

void TerminalRenderer::InsertText(const std::vector<char32_t>& utf32) {
  if (utf32.empty())
    return;

  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  EnsureLineExists(screen.cursorY);
  TextLine& line = screen.lines[screen.cursorY];

  // Расширяем строку пробелами если нужно
  while (static_cast<int>(line.glyphs.size()) < screen.cursorX) {
    line.glyphs.push_back(m_mterm->GetGlyphIndex(' '));
  }

  // Вставляем глифы
  std::vector<unsigned short> new_glyphs = GetIndices(utf32);

  // Проверяем переполнение строки
  if (screen.cursorX + static_cast<int>(new_glyphs.size()) >= m_cols) {
    if (m_useAlternativeScreen) {
      // В альтернативном экране обрезаем
      int availableSpace = m_cols - screen.cursorX;
      if (availableSpace > 0) {
        new_glyphs.resize(availableSpace);
      } else {
        return;  // Нет места
      }
    }
    // В основном экране текст может выходить за пределы строки
  }

  // Вставляем глифы
  for (size_t i = 0; i < new_glyphs.size(); i++) {
    if (screen.cursorX + i < line.glyphs.size()) {
      line.glyphs[screen.cursorX + i] = new_glyphs[i];
    } else {
      line.glyphs.push_back(new_glyphs[i]);
    }
  }

  // Удаляем пересекающиеся фрагменты
  auto& fragments = line.fragments;
  int start_pos = screen.cursorX;
  int end_pos = screen.cursorX + new_glyphs.size() - 1;

  for (auto it = fragments.begin(); it != fragments.end();) {
    if (it->start >= start_pos && it->start <= end_pos) {
      it = fragments.erase(it);
    } else if (it->end >= start_pos && it->end <= end_pos) {
      if (it->start < start_pos) {
        it->end = start_pos - 1;
        ++it;
      } else {
        it = fragments.erase(it);
      }
    } else if (it->start < start_pos && it->end > end_pos) {
      // Фрагмент охватывает всю область вставки - разбиваем его
      TextFragment rightPart = *it;
      rightPart.start = end_pos + 1;
      it->end = start_pos - 1;
      it = fragments.insert(std::next(it), rightPart);
      ++it;
    } else {
      ++it;
    }
  }
  
  if (!new_glyphs.empty()) {
    std::vector<TextFragment> new_fragments =
        CreateFragments(utf32, start_pos, end_pos);
    fragments.insert(fragments.end(), new_fragments.begin(), new_fragments.end());
  }

  // Двигаем курсор
  screen.cursorX += new_glyphs.size();
}

std::vector<TextFragment> TerminalRenderer::CreateFragments(
    const std::vector<char32_t>& utf32,
    int start_pos,
    int end_pos) {
  std::vector<TextFragment> result;

  int fragment_color = m_foregroundColor;
  if (utf32.size() > 4) {
    std::u32string str(utf32.begin(), utf32.end());
    if (str.starts_with(U"PS ")) {
      fragment_color = ANSI_BRIGHT_COLORS[2];
    }
  }
  TextFragment fragment;
  fragment.start = start_pos;
  fragment.end = end_pos;
  fragment.color = fragment_color;
  fragment.background_color = m_backgroundColor;
  fragment.underline_color = m_underlineEnabled ? m_underlineColor : -1;
  result.push_back(fragment);
  return result;
}

void TerminalRenderer::HandleNewLine() {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  screen.cursorY++;

  if (screen.cursorY >= m_rows) {
    if (m_useAlternativeScreen) {
      // В альтернативном экране скроллим содержимое
      screen.lines.erase(screen.lines.begin());
      screen.lines.emplace_back();
      screen.cursorY = m_rows - 1;
    } else {
      // В основном экране используем scrollback
      ScrollUp();
      screen.cursorY = m_rows - 1;
    }
  } else {
    EnsureLineExists(screen.cursorY);
  }
}

void TerminalRenderer::ScrollUp() {
  if (!m_mainScreen.lines.empty()) {
    m_scrollback.push_back(std::move(m_mainScreen.lines.front()));
    m_mainScreen.lines.erase(m_mainScreen.lines.begin());
    m_mainScreen.lines.emplace_back();

    // Ограничиваем размер scrollback
    if (m_scrollback.size() > MAX_SCROLLBACK_LINES) {
      m_scrollback.pop_front();
    }
  }
}

void TerminalRenderer::HandleCarriageReturn() {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;
  screen.cursorX = 0;
}

void TerminalRenderer::HandleBackspace() {
  // In Windows terminal backspace moves cursor, without removing character
  MoveCursorRelative(0, -1);
  return;

  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  if (screen.cursorX > 0) {
    screen.cursorX--;
    EnsureLineExists(screen.cursorY);
    TextLine& line = screen.lines[screen.cursorY];

    if (screen.cursorX < static_cast<int>(line.glyphs.size())) {
      line.glyphs.erase(line.glyphs.begin() + screen.cursorX);

      // Обновляем фрагменты
      for (auto it = line.fragments.begin(); it != line.fragments.end();) {
        if (it->start > screen.cursorX) {
          it->start--;
          it->end--;
          ++it;
        } else if (it->end >= screen.cursorX) {
          if (it->start == screen.cursorX && it->end == screen.cursorX) {
            it = line.fragments.erase(it);
          } else {
            it->end--;
            if (it->start > it->end) {
              it = line.fragments.erase(it);
            } else {
              ++it;
            }
          }
        } else {
          ++it;
        }
      }
    }
  }
}

void TerminalRenderer::HandleTab() {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  int new_x = ((screen.cursorX / 8) + 1) * 8;
  if (m_useAlternativeScreen) {
    new_x = std::min(new_x, m_cols - 1);
  }

  EnsureLineExists(screen.cursorY);
  TextLine& line = screen.lines[screen.cursorY];

  while (screen.cursorX < new_x) {
    if (screen.cursorX >= static_cast<int>(line.glyphs.size())) {
      line.glyphs.push_back(m_mterm->GetGlyphIndex(' '));
    }
    screen.cursorX++;
  }
}

void TerminalRenderer::Render(int x, int y, int num_rows, int num_cols) {
  std::shared_lock lock(m_mutex);

  m_numVisibleRows = num_rows;
  if (m_useAlternativeScreen) {
    // Альтернативный экран - просто рендерим как есть
    for (int i = 0;
         i <
         std::min(num_rows, static_cast<int>(m_alternativeScreen.lines.size()));
         i++) {
      RenderLine(m_alternativeScreen.lines[i], y + i, x, num_cols);
    }
    RenderCursor(x, y, num_rows, num_cols);
  } else {
    // Основной экран с scrollback

    int total_lines =
        static_cast<int>(m_scrollback.size() + m_mainScreen.lines.size());
    int start_line = std::max(0, total_lines - num_rows - m_scrollOffset);
    RenderLineNumbers(x, y, num_rows);

    int max_line_number = start_line + num_rows + 1;
    int offset = std::max(2.0, ceil(log10(max_line_number)) + 1);
    x += offset;

    int rendered_lines = 0;
    if (start_line < static_cast<int>(m_scrollback.size())) {
      int scrollbackStart = start_line;
      int scrollbackEnd = std::min(static_cast<int>(m_scrollback.size()),
                                   start_line + num_rows - rendered_lines);

      for (int i = scrollbackStart; i < scrollbackEnd; i++) {
        RenderLine(m_scrollback[i], y + rendered_lines, x, num_cols);
        rendered_lines++;
      }
    }
    int main_start =
        std::max(0, start_line - static_cast<int>(m_scrollback.size()));
    for (int i = main_start; i < static_cast<int>(m_mainScreen.lines.size()) &&
                            rendered_lines < num_rows;
         i++) {
      RenderLine(m_mainScreen.lines[i], y + rendered_lines, x, num_cols);
      rendered_lines++;
    }
    RenderCursor(x, y, num_rows, num_cols);
  }
}

void TerminalRenderer::RenderLineNumbers(int x, int y, int num_rows) {
  int total_lines =
      static_cast<int>(m_scrollback.size() + m_mainScreen.lines.size());
  int start_line = std::max(0, total_lines - num_rows - m_scrollOffset);

  m_lineNumberGlyphs.resize(num_rows * MAX_LINE_NUMBER_SIZE);
  int line_y = y;
  for (int i = 0; i < num_rows; i++) {
    char buffer[MAX_LINE_NUMBER_SIZE];
    auto [ptr, ec] =
        std::to_chars(buffer, buffer + sizeof(buffer), start_line + i + 1);
    if (ec == std::errc()) {
      int length = ptr - buffer;
      int offset = i * MAX_LINE_NUMBER_SIZE;

      for (int j = 0; j < length; j++) {
        m_lineNumberGlyphs[offset + j] = m_mterm->GetGlyphIndex(buffer[j]);
      }
      m_mterm->DrawGlyphs(m_lineNumberGlyphs.data() + offset, length, 0, line_y,
                         LINE_NUMBER_COLOR);
    }
    line_y++;
  }
}

void TerminalRenderer::RenderLine(const TextLine& line,
                                  int line_y,
                                  int x,
                                  int num_cols) {
  for (const TextFragment& fragment : line.fragments) {
    int start = fragment.start;
    int end = std::min(fragment.end, num_cols - 1);
    if (start <= end && start < static_cast<int>(line.glyphs.size())) {
      int fragment_x = fragment.start + x;
      int length = end - start + 1;
      int actual_length =
          std::min(length, static_cast<int>(line.glyphs.size()) - start);

      if (actual_length > 0) {
        if (fragment.background_color != -1) {
          m_mterm->DrawBackground(fragment_x, line_y,
                                 fragment_x + actual_length - 1, line_y,
                                 fragment.background_color);
        }
        if (fragment.underline_color != -1) {
          m_mterm->DrawUnderline(fragment_x, line_y, actual_length,
                                fragment.underline_color);
        }
        m_mterm->DrawGlyphs(line.glyphs.data() + start, actual_length, fragment_x,
                           line_y, fragment.color);
      }
    }
  }
}

void TerminalRenderer::RenderCursor(int x, int y, int num_rows, int num_cols) {
  const Screen& screen =
      m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  if (m_useAlternativeScreen) {
    // В альтернативном экране курсор всегда видим
    if (screen.cursorX >= 0 && screen.cursorX < num_cols &&
        screen.cursorY >= 0 && screen.cursorY < num_rows) {
      m_mterm->DrawCursor(x + screen.cursorX, y + screen.cursorY, CURSOR_COLOR);
    }
  } else {
    // В основном экране проверяем видимость
    int total_lines =
        static_cast<int>(m_scrollback.size() + m_mainScreen.lines.size());
    int start_line = std::max(0, total_lines - num_rows - m_scrollOffset);
    int cursor_global_line =
        static_cast<int>(m_scrollback.size()) + screen.cursorY;

    if (cursor_global_line >= start_line &&
        cursor_global_line < start_line + num_rows) {
      int cursor_visible_line = cursor_global_line - start_line;
      if (screen.cursorX >= 0 && screen.cursorX < num_cols &&
          cursor_visible_line >= 0 && cursor_visible_line < num_rows) {
        m_mterm->DrawCursor(x + screen.cursorX, y + cursor_visible_line,
                           CURSOR_COLOR);
      }
    }
  }
}

bool TerminalRenderer::SetScrollOffset(int offset) {
  std::unique_lock lock(m_mutex);
  int new_offset = std::min(offset, GetMaxScrollOffset());
  if (new_offset != m_scrollOffset) {
    m_scrollOffset = new_offset;
    return true;
  }
  return false;
}

int TerminalRenderer::GetMaxScrollOffset() const {
  if (m_useAlternativeScreen) {
    return 0;
  }
  return std::max(
      0, static_cast<int>(m_scrollback.size() + m_mainScreen.lines.size()) -
             m_numVisibleRows);
}

void TerminalRenderer::EnsureLineExists(int line) {
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;
  while (static_cast<int>(screen.lines.size()) <= line) {
    screen.lines.emplace_back();
  }
  if (m_useAlternativeScreen &&
      screen.lines.size() > static_cast<size_t>(m_rows)) {
    screen.lines.resize(m_rows);
  }
}

void TerminalRenderer::MoveCursor(int row, int col) {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  screen.cursorY = std::max(0, row);
  screen.cursorX = std::max(0, col);

  if (m_useAlternativeScreen) {
    screen.cursorY = std::min(screen.cursorY, m_rows - 1);
    screen.cursorX = std::min(screen.cursorX, m_cols - 1);
  }
  EnsureLineExists(screen.cursorY);
}

void TerminalRenderer::MoveCursorRelative(int rows, int cols) {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  screen.cursorY = std::max(0, screen.cursorY + rows);
  screen.cursorX = std::max(0, screen.cursorX + cols);

  if (m_useAlternativeScreen) {
    screen.cursorY = std::min(screen.cursorY, m_rows - 1);
    screen.cursorX = std::min(screen.cursorX, m_cols - 1);
  }
  EnsureLineExists(screen.cursorY);
}

void TerminalRenderer::DeleteCharacters(int count) {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  EnsureLineExists(screen.cursorY);
  TextLine& line = screen.lines[screen.cursorY];

  if (screen.cursorX < static_cast<int>(line.glyphs.size())) {
    int delete_end =
        std::min(screen.cursorX + count, static_cast<int>(line.glyphs.size()));
    int deleted_count = delete_end - screen.cursorX;

    line.glyphs.erase(line.glyphs.begin() + screen.cursorX,
                      line.glyphs.begin() + delete_end);

    // Обновляем фрагменты
    for (auto it = line.fragments.begin(); it != line.fragments.end();) {
      if (it->start >= delete_end) {
        it->start -= deleted_count;
        it->end -= deleted_count;
        ++it;
      } else if (it->end < screen.cursorX) {
        ++it;
      } else {
        if (it->start >= screen.cursorX) {
          it = line.fragments.erase(it);
        } else {
          it->end = std::min(it->end, screen.cursorX - 1);
          if (it->start > it->end) {
            it = line.fragments.erase(it);
          } else {
            ++it;
          }
        }
      }
    }
  }
}

void TerminalRenderer::EraseCharacters(int count) {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  EnsureLineExists(screen.cursorY);
  TextLine& line = screen.lines[screen.cursorY];

  int erase_end =
      std::min(screen.cursorX + count, static_cast<int>(line.glyphs.size()));

  for (int i = screen.cursorX; i < erase_end; i++) {
    line.glyphs[i] = m_mterm->GetGlyphIndex(' ');
  }

  // Удаляем фрагменты в стираемой области
  for (auto it = line.fragments.begin(); it != line.fragments.end();) {
    if (it->start >= screen.cursorX && it->end < erase_end) {
      it = line.fragments.erase(it);
    } else if (it->start >= screen.cursorX && it->start < erase_end) {
      it->start = erase_end;
      if (it->start > it->end) {
        it = line.fragments.erase(it);
      } else {
        ++it;
      }
    } else if (it->end >= screen.cursorX && it->end < erase_end) {
      it->end = screen.cursorX - 1;
      if (it->start > it->end) {
        it = line.fragments.erase(it);
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }
}

void TerminalRenderer::InsertLines(int count) {
  if (count <= 0) {
    return;
  }

  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;
  EnsureLineExists(screen.cursorY);

  for (int i = 0; i < count; i++) {
    screen.lines.insert(screen.lines.begin() + screen.cursorY, TextLine{});
  }
  if (m_useAlternativeScreen &&
      screen.lines.size() > static_cast<size_t>(m_rows)) {
    screen.lines.resize(m_rows);

  } else if (!m_useAlternativeScreen) {
    while (static_cast<int>(screen.lines.size()) > m_rows) {
      m_scrollback.push_back(std::move(screen.lines.front()));
      screen.lines.erase(screen.lines.begin());
      if (m_scrollback.size() > MAX_SCROLLBACK_LINES) {
        m_scrollback.pop_front();
      }
    }
  }
}

void TerminalRenderer::DeleteLines(int count) {
  if (count <= 0) {
    return;
  }

  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  EnsureLineExists(screen.cursorY);

  int delete_end =
      std::min(screen.cursorY + count, static_cast<int>(screen.lines.size()));
  if (delete_end > screen.cursorY) {
    screen.lines.erase(screen.lines.begin() + screen.cursorY,
                       screen.lines.begin() + delete_end);
    EnsureLineExists(screen.cursorY);
  }
}

void TerminalRenderer::ClearScreen(int mode) {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  switch (mode) {
    case 0:  // От курсора до конца экрана
      if (screen.cursorY < static_cast<int>(screen.lines.size())) {
        // Очищаем текущую строку от курсора
        TextLine& line = screen.lines[screen.cursorY];
        if (screen.cursorX < static_cast<int>(line.glyphs.size())) {
          line.glyphs.erase(line.glyphs.begin() + screen.cursorX,
                            line.glyphs.end());

          // Обновляем фрагменты
          for (auto it = line.fragments.begin(); it != line.fragments.end();) {
            if (it->start >= screen.cursorX) {
              it = line.fragments.erase(it);
            } else if (it->end >= screen.cursorX) {
              it->end = screen.cursorX - 1;
              if (it->start > it->end) {
                it = line.fragments.erase(it);
              } else {
                ++it;
              }
            } else {
              ++it;
            }
          }
        }

        // Очищаем все строки ниже
        screen.lines.erase(screen.lines.begin() + screen.cursorY + 1,
                           screen.lines.end());
      }
      break;

    case 1:  // От начала экрана до курсора
      if (screen.cursorY < static_cast<int>(screen.lines.size())) {
        // Очищаем все строки выше
        if (screen.cursorY > 0) {
          screen.lines.erase(screen.lines.begin(),
                             screen.lines.begin() + screen.cursorY);
          screen.cursorY = 0;
        }

        // Очищаем текущую строку до курсора включительно
        if (!screen.lines.empty()) {
          TextLine& line = screen.lines[0];
          if (screen.cursorX > 0 && !line.glyphs.empty()) {
            int clear_to = std::min(screen.cursorX + 1,
                                   static_cast<int>(line.glyphs.size()));
            line.glyphs.erase(line.glyphs.begin(),
                              line.glyphs.begin() + clear_to);

            // Обновляем фрагменты
            for (auto& fragment : line.fragments) {
              fragment.start = std::max(0, fragment.start - clear_to);
              fragment.end = std::max(-1, fragment.end - clear_to);
            }
            line.fragments.erase(
                std::remove_if(line.fragments.begin(), line.fragments.end(),
                               [](const TextFragment& f) { return f.end < 0; }),
                line.fragments.end());
            screen.cursorX = 0;
          }
        }
      }
      break;

    case 2:  // Весь экран
      screen.lines.clear();
      screen.lines.resize(m_rows);
      screen.cursorX = 0;
      screen.cursorY = 0;
      break;
  }
}

void TerminalRenderer::ClearLine(int mode) {
  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  EnsureLineExists(screen.cursorY);
  TextLine& line = screen.lines[screen.cursorY];

  switch (mode) {
    case 0:  // От курсора до конца строки
      if (screen.cursorX < static_cast<int>(line.glyphs.size())) {
        line.glyphs.erase(line.glyphs.begin() + screen.cursorX,
                          line.glyphs.end());

        for (auto it = line.fragments.begin(); it != line.fragments.end();) {
          if (it->start >= screen.cursorX) {
            it = line.fragments.erase(it);
          } else if (it->end >= screen.cursorX) {
            it->end = screen.cursorX - 1;
            if (it->start > it->end) {
              it = line.fragments.erase(it);
            } else {
              ++it;
            }
          } else {
            ++it;
          }
        }
      }
      break;

    case 1:  // От начала строки до курсора включительно
      if (screen.cursorX >= 0 && !line.glyphs.empty()) {
        int clear_to =
            std::min(screen.cursorX + 1, static_cast<int>(line.glyphs.size()));
        line.glyphs.erase(line.glyphs.begin(), line.glyphs.begin() + clear_to);

        for (auto& fragment : line.fragments) {
          fragment.start = std::max(0, fragment.start - clear_to);
          fragment.end = std::max(-1, fragment.end - clear_to);
        }
        line.fragments.erase(
            std::remove_if(line.fragments.begin(), line.fragments.end(),
                           [](const TextFragment& f) { return f.end < 0; }),
            line.fragments.end());
      }
      break;

    case 2:  // Вся строка
      line.glyphs.clear();
      line.fragments.clear();
      screen.cursorX = 0;
      break;
  }
}

void TerminalRenderer::SwitchToAlternativeScreen() {
  std::unique_lock lock(m_mutex);
  if (!m_useAlternativeScreen) {
    m_useAlternativeScreen = true;
    m_alternativeScreen.lines.clear();
    m_alternativeScreen.lines.resize(m_rows);
    m_alternativeScreen.cursorX = 0;
    m_alternativeScreen.cursorY = 0;
  }
}

void TerminalRenderer::SwitchToMainScreen() {
  std::unique_lock lock(m_mutex);
  m_useAlternativeScreen = false;
}

void TerminalRenderer::ProcessEscapeSequence(const std::string& sequence) {
  if (sequence.length() < 2)
    return;

  if (sequence[1] == '[') {
    HandleCSISequence(sequence.substr(2));
  } else if (sequence[1] == ']') {
    HandleOSC(sequence.substr(2));
  } else if (sequence.length() == 2) {
    char command = sequence[1];
    Screen& screen =
        m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

    switch (command) {
      case '7':  // Save cursor
        screen.savedCursorX = screen.cursorX;
        screen.savedCursorY = screen.cursorY;
        break;
      case '8':  // Restore cursor
        screen.cursorX = screen.savedCursorX;
        screen.cursorY = screen.savedCursorY;
        EnsureLineExists(screen.cursorY);
        break;
      case 'c':  // Reset terminal
        ClearScreen(2);
        m_foregroundColor = TEXT_COLOR;
        m_backgroundColor = -1;
        m_underlineColor = -1;
        m_underlineEnabled = false;
        break;
      case 'D':  // Line feed
        HandleNewLine();
        break;
      case 'E':  // Next line
        HandleNewLine();
        HandleCarriageReturn();
        break;
      case 'M':  // Reverse line feed
        screen.cursorY = std::max(0, screen.cursorY - 1);
        break;
    }
  }
}

void TerminalRenderer::HandleCSI(const std::vector<int>& params, char command) {
  switch (command) {
    case 'A':  // Cursor Up
      MoveCursorRelative(-std::max(1, params.empty() ? 1 : params[0]), 0);
      break;
    case 'B':  // Cursor Down
      MoveCursorRelative(std::max(1, params.empty() ? 1 : params[0]), 0);
      break;
    case 'C':  // Cursor Forward
      MoveCursorRelative(0, std::max(1, params.empty() ? 1 : params[0]));
      break;
    case 'D':  // Cursor Back
      MoveCursorRelative(0, -std::max(1, params.empty() ? 1 : params[0]));
      break;
    case 'E':  // Cursor Next Line
      for (int i = 0; i < std::max(1, params.empty() ? 1 : params[0]); i++) {
        HandleNewLine();
      }
      HandleCarriageReturn();
      break;
    case 'F':  // Cursor Previous Line
      MoveCursorRelative(-std::max(1, params.empty() ? 1 : params[0]), 0);
      HandleCarriageReturn();
      break;
    case 'G':  // Cursor Horizontal Absolute
    {
      Screen& screen =
          m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;
      screen.cursorX = std::max(0, (params.empty() ? 1 : params[0]) - 1);
      if (m_useAlternativeScreen) {
        screen.cursorX = std::min(screen.cursorX, m_cols - 1);
      }
    } break;
    case 'H':  // Cursor Position
    case 'f':
      MoveCursor(params.size() > 0 ? params[0] - 1 : 0,
                 params.size() > 1 ? params[1] - 1 : 0);
      break;
    case 'J':  // Erase in Display
      ClearScreen(params.size() > 0 ? params[0] : 0);
      break;
    case 'K':  // Erase in Line
      ClearLine(params.size() > 0 ? params[0] : 0);
      break;
    case 'L':  // Insert Lines
      InsertLines(params.size() > 0 ? std::max(1, params[0]) : 1);
      break;
    case 'M':  // Delete Lines
      DeleteLines(params.size() > 0 ? std::max(1, params[0]) : 1);
      break;
    case 'P':  // Delete Characters
      DeleteCharacters(params.size() > 0 ? std::max(1, params[0]) : 1);
      break;
    case 'X':  // Erase Characters
      EraseCharacters(params.size() > 0 ? std::max(1, params[0]) : 1);
      break;
    case 'd':  // Line Position Absolute
    {
      Screen& screen =
          m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;
      screen.cursorY = std::max(0, (params.empty() ? 1 : params[0]) - 1);
      if (m_useAlternativeScreen) {
        screen.cursorY = std::min(screen.cursorY, m_rows - 1);
      }
      EnsureLineExists(screen.cursorY);
    } break;
    case 'm':  // Select Graphic Rendition
      SetTextAttributes(params);
      break;
    case 's':  // Save cursor position
    {
      Screen& screen =
          m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;
      screen.savedCursorX = screen.cursorX;
      screen.savedCursorY = screen.cursorY;
    } break;
    case 'u':  // Restore cursor position
    {
      Screen& screen =
          m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;
      screen.cursorX = screen.savedCursorX;
      screen.cursorY = screen.savedCursorY;
      EnsureLineExists(screen.cursorY);
    } break;
  }
}

void TerminalRenderer::HandleCSISequence(const std::string& csi_part) {
  if (csi_part.empty())
    return;

  char command = csi_part.back();
  std::string param_string = csi_part.substr(0, csi_part.length() - 1);

  bool is_private_mode = false;
  if (!param_string.empty() && param_string[0] == '?') {
    is_private_mode = true;
    param_string = param_string.substr(1);
  }

  std::vector<int> params;
  if (!param_string.empty()) {
    std::stringstream ss(param_string);
    std::string param;
    while (std::getline(ss, param, ';')) {
      if (!param.empty()) {
        try {
          params.push_back(std::stoi(param));
        } catch (...) {
          params.push_back(0);
        }
      } else {
        params.push_back(0);
      }
    }
  }

  if (params.empty()) {
    params.push_back(0);
  }

  if (is_private_mode) {
    HandlePrivateMode(params, command);
  } else {
    HandleCSI(params, command);
  }
}

void TerminalRenderer::HandlePrivateMode(const std::vector<int>& params,
                                         char command) {
  switch (command) {
    case 'h':  // Set mode
      for (int param : params) {
        switch (param) {
          case 1049:  // Alternative screen buffer + save cursor
          case 47:    // Alternative screen buffer (old)
          case 1047:  // Alternative screen buffer
            SwitchToAlternativeScreen();
            break;
        }
      }
      break;
    case 'l':  // Reset mode
      for (int param : params) {
        switch (param) {
          case 1049:  // Alternative screen buffer + restore cursor
          case 47:    // Alternative screen buffer (old)
          case 1047:  // Alternative screen buffer
            SwitchToMainScreen();
            break;
        }
      }
      break;
  }
}

void TerminalRenderer::HandleOSC(const std::string& params) {
  // OSC sequences (window title etc.) - ignore for now
}

void TerminalRenderer::ResetEscapeState() {
  m_inEscape = false;
  m_escapeState = EscapeState::NONE;
  m_escapeBuffer.clear();
}

void TerminalRenderer::SetTextAttributes(const std::vector<int>& params) {
  for (size_t i = 0; i < params.size(); i++) {
    int param = params[i];

    if (param == 0) {
      m_foregroundColor = TEXT_COLOR;
      m_backgroundColor = -1;
      m_underlineColor = -1;
      m_underlineEnabled = false;
    } else if (param == 4) {
      m_underlineEnabled = true;
      m_underlineColor = m_foregroundColor;
    } else if (param == 24) {
      m_underlineEnabled = false;
      m_underlineColor = -1;
    } else if (param >= 30 && param <= 37) {
      m_foregroundColor = ANSI_COLORS[param - 30];
    } else if (param == 38) {
      if (i + 2 < params.size() && params[i + 1] == 5) {
        m_foregroundColor = Get256Color(params[i + 2]);
        i += 2;
      } else if (i + 4 < params.size() && params[i + 1] == 2) {
        int r = params[i + 2];
        int g = params[i + 3];
        int b = params[i + 4];
        m_foregroundColor = (r << 16) | (g << 8) | b;
        i += 4;
      }
    } else if (param == 39) {
      m_foregroundColor = TEXT_COLOR;
    } else if (param >= 40 && param <= 47) {
      m_backgroundColor = ANSI_COLORS[param - 40];
    } else if (param == 48) {
      if (i + 2 < params.size() && params[i + 1] == 5) {
        m_backgroundColor = Get256Color(params[i + 2]);
        i += 2;
      } else if (i + 4 < params.size() && params[i + 1] == 2) {
        int r = params[i + 2];
        int g = params[i + 3];
        int b = params[i + 4];
        m_backgroundColor = (r << 16) | (g << 8) | b;
        i += 4;
      }
    } else if (param == 49) {
      m_backgroundColor = -1;
    } else if (param >= 90 && param <= 97) {
      m_foregroundColor = ANSI_BRIGHT_COLORS[param - 90];
    } else if (param >= 100 && param <= 107) {
      m_backgroundColor = ANSI_BRIGHT_COLORS[param - 100];
    }
  }
}

int TerminalRenderer::Get256Color(int index) {
  if (index < 8) {
    return ANSI_COLORS[index];
  } else if (index < 16) {
    return ANSI_BRIGHT_COLORS[index - 8];
  } else if (index >= 232 && index < 256) {
    int level = ((index - 232) * 255) / 23;
    return (level << 16) | (level << 8) | level;
  } else {
    index -= 16;
    int r = (index / 36) % 6;
    int g = (index / 6) % 6;
    int b = index % 6;

    r = r > 0 ? (r * 40 + 55) : 0;
    g = g > 0 ? (g * 40 + 55) : 0;
    b = b > 0 ? (b * 40 + 55) : 0;

    return (r << 16) | (g << 8) | b;
  }
}

std::vector<unsigned short> TerminalRenderer::GetIndices(
    const std::vector<char32_t>& chars) {
  std::vector<unsigned short> indices(chars.size());
  for (size_t i = 0; i < chars.size(); i++) {
    indices[i] = m_mterm->GetGlyphIndex(chars[i]);
  }
  return indices;
}
