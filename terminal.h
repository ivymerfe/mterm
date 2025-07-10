#pragma once

#include <string>
#include <vector>
#include <atomic>

#include "defaults.h"
#include "terminal_renderer.h"
#include "pty.h"

namespace Mterm {

class Mterm;

class Terminal {
 public:
  Terminal(Mterm* mterm);

  void Render();

  void KeyDown(int key_code);
  void KeyUp(int key_code);
  void Input(char32_t key);

  void MouseMove(int x, int y);
  void MouseDown(int x, int y, int button);
  void MouseUp(int x, int y, int button);
  void Scroll(int x, int y, int delta);

 private:
  Mterm* m_mterm;
  PseudoConsole m_pty;
  TerminalRenderer m_contentRenderer;

  std::thread m_resizeThread;

  std::vector<uint16_t> m_lineNumbers;
  std::atomic<bool> m_shouldTrack = true;

  void OnOutput(const char* data, DWORD len);
};

}  // namespace Mterm
