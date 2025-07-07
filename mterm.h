#pragma once

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "defaults.h"
#include "terminal.h"

using Microsoft::WRL::ComPtr;

inline void ThrowIfFailed(HRESULT hr) {
  if (FAILED(hr)) {
    throw std::exception();
  }
}

class Mterm {
 public:
  Mterm();

  void Init(HWND hWnd);
  void Destroy();

  bool IsInitialized();

  void Redraw();

  UINT16 GetGlyphIndex(char32_t codepoint);
  void DrawGlyphs(const UINT16* indices, int count, int x, int y, int color);
  void DrawUnderline(int x, int y, int length, int color);
  void DrawCursor(int x, int y, int color);
  void DrawBackground(int start_x,
                      int start_y,
                      int end_x,
                      int end_y,
                      int color);

  int GetNumRows() const;
  int GetNumColumns() const;

  void Resize(unsigned int width, unsigned int height);

  ComPtr<ID2D1SolidColorBrush> CreateBrush(int color);

  void KeyDown(int key_code);
  void KeyUp(int key_code);
  void Input(char32_t key);

  void MouseMove(int x, int y);
  void MouseDown(int x, int y, int button);
  void MouseUp(int x, int y, int button);
  void Scroll(int x, int y, int delta);

 private:
  void RenderThread();
  void Render();

  void LoadFont();
  float GetAdvance() const;
  float GetLineWidth(int num_chars) const;
  float GetLineHeight() const;
  float GetBaselineOffset() const;

  bool m_isInitialized = false;

  std::atomic<int> m_renderVersion = 0;
  std::atomic<bool> m_stopRendering = false;
  std::thread m_renderThread;
  std::mutex m_renderMutex;
  std::condition_variable m_renderCv;
  std::atomic<bool> m_windowResized = false;

  HWND m_hWindow;
  D2D1_SIZE_U m_windowSize;
  ComPtr<ID2D1Factory> m_d2dFactory;
  ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
  ComPtr<IDWriteFactory> m_dwriteFactory;

  ComPtr<IDWriteFontFace> m_fontFace;
  float m_advanceEm;
  float m_lineHeightEm;
  float m_baselineEm;
  float m_underlinePosEm;
  float m_underlineThicknessEm;
  std::unordered_map<char32_t, UINT16> m_glyphIndexCache;

  float m_fontSize = DEFAULT_FONT_SIZE;

  ComPtr<ID2D1SolidColorBrush> m_defaultBrush;

  ComPtr<ID2D1SolidColorBrush> m_closeBrush;
  ComPtr<ID2D1SolidColorBrush> m_maxBrush;
  ComPtr<ID2D1SolidColorBrush> m_minBrush;

  Terminal* m_terminal = nullptr;
};
