#include "mterm.h"

#include <algorithm>

#include "defaults.h"

namespace Mterm {

Mterm::Mterm() {}

void Mterm::Init(void* window) {
  m_renderer.Init(window, FONT_NAME, [this]() { this->Render(); });

  m_width = m_renderer.GetWidth();
  m_height = m_renderer.GetHeight();

  m_isInitialized = true;
}

void Mterm::Destroy() {
  m_renderer.Destroy();
}

bool Mterm::IsInitialized() {
  return m_isInitialized;
}

void Mterm::Redraw() {
  m_renderer.Redraw();
}

void Mterm::Resize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
  m_renderer.Resize(width, height);
}

void Mterm::Render() {
  m_renderer.Clear(WINDOW_BG_COLOR);

  float size = BUTTON_SIZE - BUTTON_MARGIN;
  float y = CAPTION_SIZE - size - BUTTON_OFFSET;
  float min_x = m_width - MIN_BUTTON_OFFSET;
  float close_x = m_width - CLOSE_BUTTON_OFFSET;
  m_renderer.Rect(min_x - size, y, min_x + size, y + 1, MIN_BUTTON_COLOR, 1);
  m_renderer.Line(close_x - size, y - size, close_x + size, y + size, 1,
                  CLOSE_BUTTON_COLOR, 1);
  m_renderer.Line(close_x - size, y + size, close_x + size, y - size, 1,
                  CLOSE_BUTTON_COLOR, 1);

  const char32_t txt[] = U"Hello, wonderful world!";
  m_renderer.Text(txt, _countof(txt), 16, 100.5, 100.5, 0xff0000, 0x00ff00,
                  0x0000ff, 1);
}

void Mterm::KeyDown(int key_code) {

}

void Mterm::KeyUp(int key_code) {

}

void Mterm::Input(char32_t key) {

}

void Mterm::MouseMove(int x, int y) {

}

void Mterm::MouseDown(int x, int y, int button) {

}

void Mterm::MouseUp(int x, int y, int button) {
  
}

void Mterm::Scroll(int x, int y, int delta) {
  // if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) {
  //   float new_font_size = m_fontSize + delta;
  //   if (2 < new_font_size && new_font_size < 200) {
  //     m_fontSize = new_font_size;
  //     Redraw();
  //   }
  // } else {
  //   if (m_terminal != nullptr) {
  //     m_terminal->Scroll(x, y, delta);
  //   }
  // }
}

}  // namespace Mterm
