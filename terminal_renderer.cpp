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
  int emptyLinesAtEnd = 0;
  for (int i = static_cast<int>(m_mainScreen.lines.size()) - 1; i >= 0; --i) {
    if (m_mainScreen.lines[i].glyphs.empty() &&
        m_mainScreen.lines[i].fragments.empty()) {
      emptyLinesAtEnd++;
    } else {
      break;
    }
  }

  while (static_cast<int>(m_mainScreen.lines.size()) - emptyLinesAtEnd > rows) {
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

  std::vector<char32_t> textBuffer;
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
        textBuffer.push_back(c);
      }
    } else {
      if (c == '\033') {
        if (!textBuffer.empty()) {
          InsertText(textBuffer);
          textBuffer.clear();
        }
        m_inEscape = true;
        m_escapeState = EscapeState::ESC;
        m_escapeBuffer.clear();
        m_escapeBuffer += static_cast<char>(c);
      } else if (c == '\r') {
        if (!textBuffer.empty()) {
          InsertText(textBuffer);
          textBuffer.clear();
        }
        HandleCarriageReturn();
      } else if (c == '\n') {
        if (!textBuffer.empty()) {
          InsertText(textBuffer);
          textBuffer.clear();
        }
        HandleNewLine();
      } else if (c == '\b') {
        if (!textBuffer.empty()) {
          InsertText(textBuffer);
          textBuffer.clear();
        }
        HandleBackspace();
      } else if (c == '\t') {
        if (!textBuffer.empty()) {
          InsertText(textBuffer);
          textBuffer.clear();
        }
        HandleTab();
      } else if (c >= 32 || (c >= 1 && c <= 31 && c != 7 && c != 8 && c != 9 &&
                             c != 10 && c != 13)) {
        textBuffer.push_back(c);
      }
    }
  }

  if (!textBuffer.empty()) {
    InsertText(textBuffer);
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
  std::vector<unsigned short> newGlyphs = GetIndices(utf32);

  // Проверяем переполнение строки
  if (screen.cursorX + static_cast<int>(newGlyphs.size()) >= m_cols) {
    if (m_useAlternativeScreen) {
      // В альтернативном экране обрезаем
      int availableSpace = m_cols - screen.cursorX;
      if (availableSpace > 0) {
        newGlyphs.resize(availableSpace);
      } else {
        return;  // Нет места
      }
    }
    // В основном экране текст может выходить за пределы строки
  }

  // Вставляем глифы
  for (size_t i = 0; i < newGlyphs.size(); i++) {
    if (screen.cursorX + i < line.glyphs.size()) {
      line.glyphs[screen.cursorX + i] = newGlyphs[i];
    } else {
      line.glyphs.push_back(newGlyphs[i]);
    }
  }

  // Удаляем пересекающиеся фрагменты
  auto& fragments = line.fragments;
  int startPos = screen.cursorX;
  int endPos = screen.cursorX + newGlyphs.size() - 1;

  for (auto it = fragments.begin(); it != fragments.end();) {
    if (it->start >= startPos && it->start <= endPos) {
      it = fragments.erase(it);
    } else if (it->end >= startPos && it->end <= endPos) {
      if (it->start < startPos) {
        it->end = startPos - 1;
        ++it;
      } else {
        it = fragments.erase(it);
      }
    } else if (it->start < startPos && it->end > endPos) {
      // Фрагмент охватывает всю область вставки - разбиваем его
      TextFragment rightPart = *it;
      rightPart.start = endPos + 1;
      it->end = startPos - 1;
      it = fragments.insert(std::next(it), rightPart);
      ++it;
    } else {
      ++it;
    }
  }

  if (utf32.size() > 4) {
    std::u32string str(utf32.begin(), utf32.end());
    if (str.starts_with(U"PS ")) {
      m_foregroundColor = ANSI_BRIGHT_COLORS[2];
    }
  }

  // Создаем фрагмент для нового текста
  if (!newGlyphs.empty()) {
    TextFragment fragment;
    fragment.start = startPos;
    fragment.end = endPos;
    fragment.color = m_foregroundColor;
    fragment.background_color = m_backgroundColor;
    fragment.underline_color = m_underlineEnabled ? m_underlineColor : -1;
    fragments.push_back(fragment);
  }

  // Двигаем курсор
  screen.cursorX += newGlyphs.size();
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

  int newX = ((screen.cursorX / 8) + 1) * 8;
  if (m_useAlternativeScreen) {
    newX = std::min(newX, m_cols - 1);
  }

  EnsureLineExists(screen.cursorY);
  TextLine& line = screen.lines[screen.cursorY];

  while (screen.cursorX < newX) {
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
      int actualLength =
          std::min(length, static_cast<int>(line.glyphs.size()) - start);

      if (actualLength > 0) {
        if (fragment.background_color != -1) {
          m_mterm->DrawBackground(fragment_x, line_y,
                                 fragment_x + actualLength - 1, line_y,
                                 fragment.background_color);
        }
        if (fragment.underline_color != -1) {
          m_mterm->DrawUnderline(fragment_x, line_y, actualLength,
                                fragment.underline_color);
        }
        m_mterm->DrawGlyphs(line.glyphs.data() + start, actualLength, fragment_x,
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
    int totalLines =
        static_cast<int>(m_scrollback.size() + m_mainScreen.lines.size());
    int startLine = std::max(0, totalLines - num_rows - m_scrollOffset);
    int cursorGlobalLine =
        static_cast<int>(m_scrollback.size()) + screen.cursorY;

    if (cursorGlobalLine >= startLine &&
        cursorGlobalLine < startLine + num_rows) {
      int cursorVisibleLine = cursorGlobalLine - startLine;
      if (screen.cursorX >= 0 && screen.cursorX < num_cols &&
          cursorVisibleLine >= 0 && cursorVisibleLine < num_rows) {
        m_mterm->DrawCursor(x + screen.cursorX, y + cursorVisibleLine,
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
  if (m_useAlternativeScreen)
    return 0;
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
    int deleteEnd =
        std::min(screen.cursorX + count, static_cast<int>(line.glyphs.size()));
    int deletedCount = deleteEnd - screen.cursorX;

    line.glyphs.erase(line.glyphs.begin() + screen.cursorX,
                      line.glyphs.begin() + deleteEnd);

    // Обновляем фрагменты
    for (auto it = line.fragments.begin(); it != line.fragments.end();) {
      if (it->start >= deleteEnd) {
        it->start -= deletedCount;
        it->end -= deletedCount;
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

  int eraseEnd =
      std::min(screen.cursorX + count, static_cast<int>(line.glyphs.size()));

  for (int i = screen.cursorX; i < eraseEnd; i++) {
    line.glyphs[i] = m_mterm->GetGlyphIndex(' ');
  }

  // Удаляем фрагменты в стираемой области
  for (auto it = line.fragments.begin(); it != line.fragments.end();) {
    if (it->start >= screen.cursorX && it->end < eraseEnd) {
      it = line.fragments.erase(it);
    } else if (it->start >= screen.cursorX && it->start < eraseEnd) {
      it->start = eraseEnd;
      if (it->start > it->end) {
        it = line.fragments.erase(it);
      } else {
        ++it;
      }
    } else if (it->end >= screen.cursorX && it->end < eraseEnd) {
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
  if (count <= 0)
    return;

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
  if (count <= 0)
    return;

  std::unique_lock lock(m_mutex);
  Screen& screen = m_useAlternativeScreen ? m_alternativeScreen : m_mainScreen;

  EnsureLineExists(screen.cursorY);

  int deleteEnd =
      std::min(screen.cursorY + count, static_cast<int>(screen.lines.size()));
  if (deleteEnd > screen.cursorY) {
    screen.lines.erase(screen.lines.begin() + screen.cursorY,
                       screen.lines.begin() + deleteEnd);
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
            int clearTo = std::min(screen.cursorX + 1,
                                   static_cast<int>(line.glyphs.size()));
            line.glyphs.erase(line.glyphs.begin(),
                              line.glyphs.begin() + clearTo);

            // Обновляем фрагменты
            for (auto& fragment : line.fragments) {
              fragment.start = std::max(0, fragment.start - clearTo);
              fragment.end = std::max(-1, fragment.end - clearTo);
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
        int clearTo =
            std::min(screen.cursorX + 1, static_cast<int>(line.glyphs.size()));
        line.glyphs.erase(line.glyphs.begin(), line.glyphs.begin() + clearTo);

        for (auto& fragment : line.fragments) {
          fragment.start = std::max(0, fragment.start - clearTo);
          fragment.end = std::max(-1, fragment.end - clearTo);
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

void TerminalRenderer::HandleCSISequence(const std::string& csiPart) {
  if (csiPart.empty())
    return;

  char command = csiPart.back();
  std::string paramString = csiPart.substr(0, csiPart.length() - 1);

  bool isPrivateMode = false;
  if (!paramString.empty() && paramString[0] == '?') {
    isPrivateMode = true;
    paramString = paramString.substr(1);
  }

  std::vector<int> params;
  if (!paramString.empty()) {
    std::stringstream ss(paramString);
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

  if (isPrivateMode) {
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
