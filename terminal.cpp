#include "terminal.h"

#include "Windows.h"

#include "mterm.h"
#include "utils.h"

#include <charconv>
#include <functional>

namespace Mterm {

Terminal::Terminal(Mterm* mterm)
    : m_contentRenderer(mterm),
      m_pty(mterm->GetNumRows(), mterm->GetNumColumns()) {
  m_mterm = mterm;

  m_pty.Start(
      [this](const char* data, DWORD len) { this->OnOutput(data, len); });
}

void Terminal::OnOutput(const char* data, DWORD len) {
  m_contentRenderer.ProcessAnsi(data, len);
  m_mterm->Redraw();
}

void Terminal::Render() {
  int num_rows = m_mterm->GetNumRows();
  int num_columns = m_mterm->GetNumColumns();

  m_contentRenderer.Render(0, 0, num_rows, num_columns);
}

void Terminal::KeyDown(int key_code) {
  const char* seq = nullptr;

  switch (key_code) {
    case VK_UP:
      seq = "\x1b[A";
      break;
    case VK_DOWN:
      seq = "\x1b[B";
      break;
    case VK_RIGHT:
      seq = "\x1b[C";
      break;
    case VK_LEFT:
      seq = "\x1b[D";
      break;
    case VK_DELETE:
      seq = "\x1b[3~";
      break;
    case VK_HOME:
      seq = "\x1b[H";  // or "\x1b[1~" depending on terminal
      break;
    case VK_END:
      seq = "\x1b[F";  // or "\x1b[4~" depending on terminal
      break;
    case VK_PRIOR:  // Page Up
      seq = "\x1b[5~";
      break;
    case VK_NEXT:  // Page Down
      seq = "\x1b[6~";
      break;
    default:
      return;  // ignore other keys here
  }

  if (seq) {
    m_pty.Send(seq, strlen(seq));
  }
}

void Terminal::KeyUp(int key_code) {}

void Terminal::Input(char32_t key) {
  if (key == '\r') {
    m_shouldTrack.store(true);
  }

  char buf[4];
  int length = 0;
  Utils::Utf32CharToUtf8(key, buf, length);
  m_pty.Send(buf, length);
}

void Terminal::MouseMove(int x, int y) {}

void Terminal::MouseDown(int x, int y, int button) {}

void Terminal::MouseUp(int x, int y, int button) {
  if (button == 1) {
    if (OpenClipboard(nullptr)) {
      HANDLE hData = GetClipboardData(CF_UNICODETEXT);
      if (hData) {
        LPCWSTR pszText = static_cast<LPCWSTR>(GlobalLock(hData));
        if (pszText) {
          int len = lstrlenW(pszText);
          int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pszText, len, nullptr,
                                            0, nullptr, nullptr);
          if (utf8Len > 0) {
            std::string utf8Text(utf8Len, 0);
            WideCharToMultiByte(CP_UTF8, 0, pszText, len, utf8Text.data(),
                                utf8Len, nullptr, nullptr);
            m_pty.Send(utf8Text.data(), utf8Len);
          }
          GlobalUnlock(hData);
        }
      }
      CloseClipboard();
    }
  }
}

void Terminal::Scroll(int x, int y, int delta) {
  delta *= floor(0.07 * m_mterm->GetNumRows());
  int current_offset = m_contentRenderer.GetScrollOffset();
  if (m_contentRenderer.SetScrollOffset(current_offset + delta)) {
    if (delta < 0) {
      m_shouldTrack.store(false);
    }
    m_mterm->Redraw();
  }
}

}  // namespace Mterm
