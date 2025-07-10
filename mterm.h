#pragma once

#include <vector>

#include "renderer.h"

namespace Mterm {

class Mterm {
 public:
  Mterm();

  void Init(void* window);
  void Destroy();

  bool IsInitialized();

  void Redraw();
  void Resize(unsigned int width, unsigned int height);
  void Render();

  void KeyDown(int key_code);
  void KeyUp(int key_code);
  void Input(char32_t key);

  void MouseMove(int x, int y);
  void MouseDown(int x, int y, int button);
  void MouseUp(int x, int y, int button);
  void Scroll(int x, int y, int delta);

 private:
  bool m_isInitialized = false;

  void* m_window;
  unsigned int m_width;
  unsigned int m_height;

  Renderer m_renderer;
};

}  // namespace Mterm
