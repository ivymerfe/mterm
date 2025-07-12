#include "MTerm.h"

#include <algorithm>

#include "defaults.h"

namespace MTerm {

MTerm::MTerm() {}

void MTerm::Init(void* window) {
  const char32_t text[] = U"I love you, wrld";
  m_buffer.AddLine();
  m_buffer.SetText(0, 0, text, _countof(text) - 1);
  m_buffer.SetColor(0, 0, 4, 0xff0000, 0x00ff00, 0x0000ff);
  m_buffer.SetColor(0, 5, 9, 0xffff00, 0xff0000, 0xff0000);
  m_buffer.SetColor(0, 10, 15, 0xffffff, 0x000000, 0xff0000);
  m_buffer.SetColor(0, 10, 15, 0xffffff, 0x000000, 0xff0000);
  m_buffer.SetColor(0, 12, 13, 0xff00ff, 0x000000, 0xff0000);
  m_buffer.SetColor(0, 11, 15, 0xffffff, 0x000000, 0xff0000);
  m_buffer.SetColor(0, 12, 15, 0xffffff, 0x000000, 0xff0000);
  m_buffer.SetColor(0, 12, 13, 0x0f0fff, 0x00f000, 0xff00f0);

  m_buffer.SetColor(0, 1, 2, 0xffff00, 0x00ff00, 0x000000);
  m_buffer.SetColor(0, 1, 3, 0xff0000, 0x00ff00, 0x0000ff);

  const char32_t text2[] = U"I hate you, wrld";
  m_buffer.AddLine();
  m_buffer.SetText(1, 0, text2, _countof(text2) - 1);
  m_buffer.SetColor(1, 0, 4, 0xff0000, 0x00ff00, 0x0000ff);
  m_buffer.SetColor(1, 5, 9, 0xffff00, 0xff0000, 0xff0000);
  m_buffer.SetColor(1, 10, 15, 0xffffff, 0x000000, 0xff0000);

  const char32_t text3[] = U"I>hope>this>line>has>one>fragment";
  m_buffer.AddLine();
  m_buffer.SetText(2, 0, text3, _countof(text3) - 1);
  m_buffer.SetColor(2, 0, _countof(text3)-1, 0xff0000, 0, 0);
  m_buffer.SetColor(2, 2, 4, 0xfff0a0, 0, 0);
  m_buffer.SetColor(2, 3, 5, 0x050ff0, 0, 0);
  m_buffer.SetColor(2, 6, 8, 0xffff00, 0, 0);
  m_buffer.SetColor(2, 7, 9, 0xaa00f0, 0, 0);
  m_buffer.SetColor(2, 8, 10, 0x652050, 0, 0);
  m_buffer.SetColor(2, 9, 11, 0xff00ff, 0, 0);
  m_buffer.SetColor(2, 1, 20, 0xff0000, 0, 0);

  m_renderer.Init(window, FONT_NAME, [this]() { this->Render(); });

  m_width = m_renderer.GetWidth();
  m_height = m_renderer.GetHeight();

  m_isInitialized = true;
}

void MTerm::Destroy() {
  m_renderer.Destroy();
}

bool MTerm::IsInitialized() {
  return m_isInitialized;
}

void MTerm::Redraw() {
  m_renderer.Redraw();
}

void MTerm::Resize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
  m_renderer.Resize(width, height);
}

void MTerm::Render() {
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

  m_buffer.Render(m_renderer, 50, 50, 500, 200, 0, 0, 14);
}

void MTerm::KeyDown(int key_code) {}

void MTerm::KeyUp(int key_code) {}

void MTerm::Input(char32_t key) {}

void MTerm::MouseMove(int x, int y) {}

void MTerm::MouseDown(int x, int y, int button) {}

void MTerm::MouseUp(int x, int y, int button) {}

void MTerm::Scroll(int x, int y, int delta) {
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

}  // namespace MTerm
