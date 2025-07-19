#pragma once

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <thread>

#include "ColoredTextBuffer.h"

namespace MTerm {

constexpr auto TEXT_BUFFER_SIZE = 1024 * 1024;

struct Config {
  const wchar_t* font_name;
  int window_width;
  int window_height;
  int window_min_width;
  int window_min_height;
  int caption_size;
  int border_size;
  int button_size;
  int close_button_offset;
  int max_button_offset;
  int min_button_offset;

  std::function<void()> render_callback;
  std::function<void(char32_t)> input_callback;
};

class Window {
 public:
  Window();
  ~Window();

  int Create(Config config);
  void Destroy();

  void InitRenderer();

  void Redraw();

  void Resize(unsigned int width, unsigned int height);
  int GetWidth() const;
  int GetHeight() const;

  void Input(char32_t codepoint);

  void Clear(int color);

  void Text(const char32_t* text,
            int length,
            float font_size,
            float x,
            float y,
            int color,
            int underline_color,
            int background_color,
            float opacity);

  void Line(float start_x,
            float start_y,
            float end_x,
            float end_y,
            float thickness,
            int color,
            float opacity);

  void Rect(float left,
            float top,
            float right,
            float bottom,
            int color,
            float opacity);

  void Outline(float left,
               float top,
               float right,
               float bottom,
               float thickness,
               int color,
               float opacity);

  void TextBuffer(ColoredTextBuffer* buffer,
                  float left,
                  float top,
                  float width,
                  float height,
                  int x_offset_chars,
                  int y_offset_lines,
                  float font_size);

  float GetAdvance(float font_size) const;
  float GetLineWidth(float font_size, int num_chars) const;
  float GetLineHeight(float font_size) const;

 private:
  static LRESULT CALLBACK WindowProc(HWND hWnd,
                                     UINT uMsg,
                                     WPARAM wParam,
                                     LPARAM lParam);
  void RenderThread();
  void Render();
  UINT16 GetGlyphIndex(char32_t codepoint);
  void LoadFont(const wchar_t* font_name);

  Config m_config;

  bool m_isInitialized = false;
  HWND m_hWindow;

  D2D1_SIZE_U m_windowSize;
  Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
  Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
  Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_defaultBrush;

  Microsoft::WRL::ComPtr<IDWriteFontFace> m_fontFace;
  float m_advanceEm;
  float m_lineHeightEm;
  float m_baselineEm;
  float m_underlinePosEm;
  float m_underlineThicknessEm;
  std::unordered_map<char32_t, unsigned short> m_glyphIndexCache;
  std::vector<unsigned short> m_wcharIndexesVector;

  std::vector<unsigned short> m_textBuffer;
  unsigned int m_textBufferPos = 0;

  std::atomic<long long> m_contentVersion = 0;
  std::atomic<long long> m_renderedVersion = 0;
  std::atomic<bool> m_stopRendering = false;
  std::thread m_renderThread;
  std::mutex m_renderMutex;
  std::condition_variable m_renderCv;
  std::atomic<bool> m_windowResized = false;
  std::mutex m_resizeMutex;
};

}  // namespace MTerm
