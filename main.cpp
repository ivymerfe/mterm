#include "Window.h"

using namespace MTerm;

Window window;

static void OnRender() {
  OutputDebugString(L"Render callback!\n");
  window.Clear(0xff0000);
  window.Text(U"Hello window!", 13, 30, 100, 100, 0x00ff00, 0x000000, 0xffffff,
              1);
}

static void OnInput(char32_t codepoint)
{
  OutputDebugString(L"User input\n");
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR pCmdLine,
                   int nCmdShow) {
  Config config = {0};
  config.font_name = L"Cascadia Mono";
  config.window_width = 1000;
  config.window_height = 600;
  config.window_min_width = 375;
  config.window_min_height = 225;
  config.caption_size = 30;
  config.border_size = 5;
  config.button_size = 30;
  config.close_button_offset = 30;
  config.max_button_offset = 0;
  config.min_button_offset = 90;

  config.render_callback = OnRender;
  config.input_callback = OnInput;

  return window.Create(config);
}
