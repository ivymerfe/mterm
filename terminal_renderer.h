#pragma once

#include <deque>
#include <shared_mutex>
#include <string>
#include <vector>

#include "defaults.h"

class Mterm;

struct TextFragment {
  int start;
  int end;
  int color;
  int background_color;
  int underline_color;
};

struct TextLine {
  std::vector<unsigned short> glyphs;
  std::vector<TextFragment> fragments;
};

enum class EscapeState { NONE, ESC, CSI, OSC };

struct Screen {
  std::vector<TextLine> lines;
  int cursorX = 0;
  int cursorY = 0;
  int savedCursorX = 0;
  int savedCursorY = 0;
};

class TerminalRenderer {
 public:
  TerminalRenderer(Mterm* mterm);

  void Render(int x, int y, int num_rows, int num_cols);
  void RenderLineNumbers(int x, int y, int num_rows);
  void ProcessAnsi(const char* text, int length);
  void SetTerminalSize(int rows, int cols);

  // Управление скроллингом
  bool SetScrollOffset(int offset);
  int GetScrollOffset() const { return m_scrollOffset; }
  int GetMaxScrollOffset() const;

 private:
  // Упрощенные методы рендеринга
  void RenderLine(const TextLine& line, int line_y, int x, int num_cols);
  void RenderCursor(int x, int y, int num_rows, int num_cols);

  // Обработка текста
  void InsertText(const std::vector<char32_t>& utf32);
  void HandleNewLine();
  void HandleCarriageReturn();
  void HandleBackspace();
  void HandleTab();

  // Управление курсором
  void MoveCursor(int row, int col);
  void MoveCursorRelative(int rows, int cols);

  // Редактирование
  void DeleteCharacters(int count);
  void EraseCharacters(int count);
  void InsertLines(int count);
  void DeleteLines(int count);

  // ANSI escape sequences
  void ProcessEscapeSequence(const std::string& sequence);
  void HandleCSI(const std::vector<int>& params, char command);
  void HandleCSISequence(const std::string& csiPart);
  void HandlePrivateMode(const std::vector<int>& params, char command);
  void HandleOSC(const std::string& params);
  void ResetEscapeState();

  // Очистка экрана
  void ClearScreen(int mode);
  void ClearLine(int mode);

  // Управление строками
  void EnsureLineExists(int line);
  void ScrollUp();

  // Переключение экранов
  void SwitchToAlternativeScreen();
  void SwitchToMainScreen();

  // Атрибуты текста
  void SetTextAttributes(const std::vector<int>& params);
  int Get256Color(int index);

  // Утилиты
  std::vector<unsigned short> GetIndices(const std::vector<char32_t>& chars);

  Mterm* m_mterm;
  mutable std::shared_mutex m_mutex;

  Screen m_mainScreen;
  Screen m_alternativeScreen;
  bool m_useAlternativeScreen = false;

  std::deque<TextLine> m_scrollback;
  int m_scrollOffset = 0;  // Сколько строк scrollback показываем

  // Номера строк
  std::vector<unsigned short> m_lineNumberGlyphs;

  // Размеры терминала
  int m_rows = 24;
  int m_cols = 80;
  int m_numVisibleRows = 24;

  // ANSI состояние
  int m_foregroundColor = TEXT_COLOR;
  int m_backgroundColor = -1;
  int m_underlineColor = -1;
  bool m_underlineEnabled = false;

  // Escape sequences
  bool m_inEscape = false;
  EscapeState m_escapeState = EscapeState::NONE;
  std::string m_escapeBuffer;

  // Константы
  static constexpr int MAX_SCROLLBACK_LINES = 1000000;
  static constexpr int MAX_LINE_NUMBER_SIZE = 16;
};
