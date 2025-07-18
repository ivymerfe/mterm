#include "Window.h"

#include <algorithm>
#include <stdexcept>

#undef min
#undef max

#include "resource.h"

using namespace Microsoft::WRL;

static inline void ThrowIfFailed(HRESULT hr) {
  if (FAILED(hr)) {
    throw std::exception();
  }
}

namespace MTerm {

Window::Window() {}

Window::~Window() {}

int Window::Create(Config config) {
  m_config = config;
  HINSTANCE hInstance = GetModuleHandle(NULL);

  const wchar_t CLASS_NAME[] = L"mterm";
  WNDCLASSEX windowClass = {};
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.lpfnWndProc = Window::WindowProc;
  windowClass.hInstance = hInstance;
  windowClass.lpszClassName = CLASS_NAME;
  windowClass.hCursor = LoadCursor(NULL, IDC_IBEAM);
  windowClass.hbrBackground = 0;
  windowClass.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON),
                                       IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR);
  windowClass.hIconSm =
      (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON, 64,
                       64, LR_DEFAULTCOLOR);

  RegisterClassEx(&windowClass);

  int width = m_config.window_width;
  int height = m_config.window_height;

  int pos_x = CW_USEDEFAULT;
  int pos_y = CW_USEDEFAULT;
  POINT pt;
  if (GetCursorPos(&pt)) {
    HMONITOR hMonitor = MonitorFromPoint(pt, NULL);
    if (hMonitor) {
      MONITORINFO mi = {sizeof(mi)};
      if (GetMonitorInfo(hMonitor, &mi)) {
        // Work area excludes taskbar
        RECT& rcWork = mi.rcWork;
        pos_x = rcWork.left + (rcWork.right - rcWork.left - width) / 2;
        pos_y = rcWork.top + (rcWork.bottom - rcWork.top - height) / 2;
      }
    }
  }

  m_hWindow = CreateWindowEx(
      NULL, CLASS_NAME,
      NULL,                                        // Window text
      WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,  // Window style
      pos_x, pos_y, width, height,
      NULL,       // Parent window
      NULL,       // Menu
      hInstance,  // Instance handle
      this        // Additional application data
  );
  if (m_hWindow == NULL) {
    return false;
  }
  ShowWindow(m_hWindow, SW_SHOWDEFAULT);
  std::thread init_thread([this]() { this->InitRenderer(); });
  init_thread.detach();

  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  init_thread.join();
  Destroy();
  return true;
}

void Window::InitRenderer() {
  ThrowIfFailed(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                  IID_PPV_ARGS(&m_d2dFactory)));

  RECT window_rect;
  GetWindowRect(m_hWindow, &window_rect);

  m_windowSize.width = window_rect.right - window_rect.left;
  m_windowSize.height = window_rect.bottom - window_rect.top;

  D2D1_RENDER_TARGET_PROPERTIES props =
      D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_HARDWARE);
  D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props =
      D2D1::HwndRenderTargetProperties(m_hWindow, m_windowSize);

  ThrowIfFailed(
      m_d2dFactory->CreateHwndRenderTarget(props, hwnd_props, &m_renderTarget));

  ThrowIfFailed(DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &m_dwriteFactory));

  LoadFont(m_config.font_name);

  m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

  ThrowIfFailed(m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),
                                                      &m_defaultBrush));

  m_textBuffer.resize(TEXT_BUFFER_SIZE);
  m_wcharIndexesVector.resize(0xFFFF + 1);

  m_renderThread = std::thread([this]() { this->RenderThread(); });

  m_isInitialized = true;
  Redraw();
}

void Window::Destroy() {
  m_stopRendering.store(true);
  m_renderCv.notify_one();
  m_renderThread.join();
}

LRESULT CALLBACK Window::WindowProc(HWND hWnd,
                                    UINT uMsg,
                                    WPARAM wParam,
                                    LPARAM lParam) {
  static wchar_t pending_high_surrogate = 0;
  static bool is_sizing = false;

  switch (uMsg) {
    case WM_CREATE: {
      CREATESTRUCT* create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
      Window* window_ptr =
          reinterpret_cast<Window*>(create_struct->lpCreateParams);
      SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)window_ptr);
      // If we add thickframe in CreateWindow, then there will be a white
      // caption for a moment.
      LONG style = GetWindowLong(hWnd, GWL_STYLE);
      style |= WS_THICKFRAME;
      SetWindowLong(hWnd, GWL_STYLE, style);
      return 0;
    }
    case WM_CLOSE:
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_NCCALCSIZE: {
      if (wParam == TRUE) {
        return 0;
      }
      break;
    }
    case WM_NCHITTEST: {
      Window* window =
          reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
      Config& config = window->m_config;

      RECT rect;
      GetWindowRect(hWnd, &rect);
      int x = GET_X_LPARAM(lParam);
      int y = GET_Y_LPARAM(lParam);

      if (!IsZoomed(hWnd)) {
        int border_size = config.border_size;
        int left = x < rect.left + border_size;
        int right = x > rect.right - border_size;
        int top = y < rect.top + border_size;
        int bottom = y > rect.bottom - border_size;

        if (top & left)
          return HTTOPLEFT;
        if (top & right)
          return HTTOPRIGHT;
        if (top)
          return HTTOP;
        if (bottom & left)
          return HTBOTTOMLEFT;
        if (bottom & right)
          return HTBOTTOMRIGHT;
        if (bottom)
          return HTBOTTOM;
        if (left)
          return HTLEFT;
        if (right)
          return HTRIGHT;
      }

      if (y < rect.top + config.caption_size) {
        int button_size = config.button_size;

        float close_button_mid = rect.right - config.close_button_offset;
        if (x >= close_button_mid - button_size &&
            x <= close_button_mid + button_size) {
          return HTCLOSE;
        }
        float max_button_mid = rect.right - config.max_button_offset;
        if (x >= max_button_mid - button_size &&
            x <= max_button_mid + button_size) {
          return HTMAXBUTTON;
        }
        float min_button_mid = rect.right - config.min_button_offset;
        if (x >= min_button_mid - button_size &&
            x <= min_button_mid + button_size) {
          return HTMINBUTTON;
        }
        return HTCAPTION;
      }
      return HTCLIENT;
    }
    case WM_NCPAINT:
    case WM_SETTEXT:
    case WM_SETICON:
      return 0;  // Block all default non-client painting
    case WM_NCACTIVATE: {
      // Tell Windows to not redraw the non-client area (title bar)
      return DefWindowProc(hWnd, uMsg, FALSE, -1);
    }
    case WM_NCLBUTTONDOWN: {
      if (wParam == HTCLOSE || wParam == HTMAXBUTTON || wParam == HTMINBUTTON) {
        return 0;
      }
      break;
    }
    case WM_NCLBUTTONUP: {
      if (wParam == HTCLOSE) {
        PostQuitMessage(0);
      }
      if (wParam == HTMAXBUTTON) {
        if (IsZoomed(hWnd)) {
          ShowWindow(hWnd, SW_RESTORE);
        } else {
          ShowWindow(hWnd, SW_MAXIMIZE);
        }
      }
      if (wParam == HTMINBUTTON) {
        ShowWindow(hWnd, SW_MINIMIZE);
      }
      break;
    }
    case WM_GETMINMAXINFO: {
      Window* window =
          reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
      MINMAXINFO* pMinMax = (MINMAXINFO*)lParam;

      pMinMax->ptMinTrackSize.x = window->m_config.window_min_width;
      pMinMax->ptMinTrackSize.y = window->m_config.window_min_height;
      // Get the monitor that the window is on
      HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
      if (hMonitor) {
        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfo(hMonitor, &mi)) {
          // Work area excludes taskbar
          RECT& rcWork = mi.rcWork;
          RECT& rcMonitor = mi.rcMonitor;

          // Set max size to work area size
          pMinMax->ptMaxSize.x = rcWork.right - rcWork.left;
          pMinMax->ptMaxSize.y = rcWork.bottom - rcWork.top;

          // Set max position to work area top-left
          pMinMax->ptMaxPosition.x = rcWork.left - rcMonitor.left;
          pMinMax->ptMaxPosition.y = rcWork.top - rcMonitor.top;
        }
      }
      return 0;
    }
    case WM_SIZE: {
      Window* window =
          reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
      is_sizing = true;
      UINT width = LOWORD(lParam);
      UINT height = HIWORD(lParam);
      window->Resize(width, height);
      return 0;
    }
    case WM_ERASEBKGND: {
      return 0;
    }
    case WM_PAINT: {
      ValidateRect(hWnd, NULL);
      if (!is_sizing) {
        Window* window =
            reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        window->Redraw();
      }
      is_sizing = false;
      return 0;
    }
    case WM_CHAR: {
      wchar_t ch = (wchar_t)wParam;

      if (ch >= 0xD800 && ch <= 0xDBFF) {
        // High surrogate, store it
        pending_high_surrogate = ch;
      } else if (ch >= 0xDC00 && ch <= 0xDFFF) {
        // Low surrogate
        if (pending_high_surrogate != 0) {
          // Combine into UTF-32
          char32_t codepoint = ((pending_high_surrogate - 0xD800) << 10) +
                               (ch - 0xDC00) + 0x10000;
          pending_high_surrogate = 0;

          Window* window =
              reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
          window->m_config.input_callback(codepoint);
        } else {
          // Unexpected low surrogate — error or ignore
        }
      } else {
        char32_t codepoint = ch;
        pending_high_surrogate = 0;
      }
      return 0;
    }
  }
  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Window::Redraw() {
  if (!m_isInitialized) {
    return;
  }
  m_contentVersion.fetch_add(1);
  m_renderCv.notify_one();
}

void Window::RenderThread() {
  std::unique_lock<std::mutex> lock(m_renderMutex);
  while (true) {
    m_renderCv.wait(lock, [this]() {
      return m_renderedVersion.load() != m_contentVersion.load() ||
             this->m_stopRendering.load();
    });

    if (m_stopRendering.load()) {
      break;
    }
    std::unique_lock lock(m_resizeMutex);
    Render();
  }
}

void Window::Render() {
  if (!m_isInitialized) {
    return;
  }
  m_renderedVersion.store(m_contentVersion.load());
  m_renderTarget->BeginDraw();

  m_textBufferPos = 0;
  m_config.render_callback();

  ThrowIfFailed(m_renderTarget->EndDraw());
}

void Window::Resize(unsigned int width, unsigned int height) {
  if (!m_isInitialized) {
    return;
  }
  if (width < m_config.window_min_width ||
      height < m_config.window_min_height) {
    return;
  }
  if (m_windowSize.width == width && m_windowSize.height == height) {
    return;
  }
  std::unique_lock lock(m_resizeMutex);

  m_windowSize.width = width;
  m_windowSize.height = height;
  m_renderTarget->Resize(m_windowSize);
  Render();
}

int Window::GetWidth() const {
  return m_windowSize.width;
}

int Window::GetHeight() const {
  return m_windowSize.height;
}

void Window::Clear(int color) {
  m_renderTarget->Clear(D2D1::ColorF(color));
}

void Window::Text(const char32_t* text,
                  int length,
                  float font_size,
                  float x,
                  float y,
                  int color,
                  int underline_color,
                  int background_color,
                  float opacity) {
  if (m_textBufferPos + length > TEXT_BUFFER_SIZE) {
    throw std::exception(
        "Text buffer overflow! Do you really want to draw so many "
        "characters?!");
  }

  m_defaultBrush->SetColor(D2D1::ColorF(color));
  m_defaultBrush->SetOpacity(opacity);

  if (background_color != -1) {
    float width = GetLineWidth(font_size, length);
    float height = GetLineHeight(font_size);
    m_defaultBrush->SetColor(D2D1::ColorF(background_color));
    m_renderTarget->FillRectangle({x, y, x + width, y + height},
                                  m_defaultBrush.Get());
  }

  int buffer_offset = m_textBufferPos;
  for (int i = 0; i < length; i++) {
    m_textBuffer[m_textBufferPos] = GetGlyphIndex(text[i]);
    m_textBufferPos++;
  }

  float baseline_y = y + m_baselineEm * font_size;

  DWRITE_GLYPH_RUN glyphRun = {};
  glyphRun.fontFace = m_fontFace.Get();
  glyphRun.fontEmSize = font_size;
  glyphRun.glyphCount = length;
  glyphRun.glyphIndices = m_textBuffer.data() + buffer_offset;
  glyphRun.glyphAdvances = nullptr;  // use natural advance
  glyphRun.isSideways = FALSE;
  glyphRun.bidiLevel = 0;

  m_defaultBrush->SetColor(D2D1::ColorF(color));

  m_renderTarget->DrawGlyphRun(D2D1::Point2F(x, baseline_y), &glyphRun,
                               m_defaultBrush.Get(),
                               DWRITE_MEASURING_MODE_NATURAL);

  if (underline_color != -1) {
    float underline_y = y + m_underlinePosEm * font_size;
    float width = GetLineWidth(font_size, length);
    float thickness = m_underlineThicknessEm * font_size;
    m_defaultBrush->SetColor(D2D1::ColorF(underline_color));
    m_renderTarget->DrawLine({x, underline_y}, {x + width, underline_y},
                             m_defaultBrush.Get(), thickness);
  }
}

void Window::Line(float start_x,
                  float start_y,
                  float end_x,
                  float end_y,
                  float thickness,
                  int color,
                  float opacity) {
  m_defaultBrush->SetColor(D2D1::ColorF(color));
  m_defaultBrush->SetOpacity(opacity);
  m_renderTarget->DrawLine({start_x, start_y}, {end_x, end_y},
                           m_defaultBrush.Get(), thickness);
}

void Window::Rect(float left,
                  float top,
                  float right,
                  float bottom,
                  int color,
                  float opacity) {
  m_defaultBrush->SetColor(D2D1::ColorF(color));
  m_defaultBrush->SetOpacity(opacity);
  D2D1_RECT_F rect = {left, top, right, bottom};
  m_renderTarget->FillRectangle(rect, m_defaultBrush.Get());
}

void Window::Outline(float left,
                     float top,
                     float right,
                     float bottom,
                     float thickness,
                     int color,
                     float opacity) {
  m_defaultBrush->SetColor(D2D1::ColorF(color));
  m_defaultBrush->SetOpacity(opacity);
  D2D1_RECT_F rect = {left, top, right, bottom};
  m_renderTarget->DrawRectangle(rect, m_defaultBrush.Get(), thickness);
}

void Window::TextBuffer(ColoredTextBuffer* buffer,
                        float left,
                        float top,
                        float width,
                        float height,
                        int x_offset_chars,
                        int y_offset_lines,
                        float font_size) {
  std::deque<ColoredLine>& lines = buffer->GetLines();

  float y = top;
  float line_height = ceil(GetLineHeight(font_size));
  float advance = GetAdvance(font_size);

  int max_visible_chars = static_cast<int>(width / advance);

  for (size_t i = y_offset_lines; i < lines.size(); ++i) {
    if (y > top + height)
      break;

    const auto& line = lines[i];
    const auto& text = line.text;
    const auto& fragments = line.fragments;

    // Binary search for first relevant fragment
    auto frag_less = [](const LineFragment& frag, int pos) {
      return frag.pos < pos;
    };
    auto it = std::lower_bound(fragments.begin(), fragments.end(),
                               x_offset_chars, frag_less);

    if (it != fragments.begin() &&
        (it == fragments.end() || it->pos > x_offset_chars)) {
      --it;
    }

    int remaining_chars = max_visible_chars;
    for (; it != fragments.end(); ++it) {
      int frag_start = it->pos;
      size_t frag_index = std::distance(fragments.begin(), it);
      int frag_end = (frag_index + 1 < fragments.size())
                         ? fragments[frag_index + 1].pos
                         : static_cast<int>(text.size());

      if (frag_end <= x_offset_chars)
        continue;
      if (frag_start >= static_cast<int>(text.size()))
        break;

      int visible_start = std::max(frag_start, x_offset_chars);
      int max_end = visible_start + remaining_chars;
      int visible_end =
          std::min({frag_end, static_cast<int>(text.size()), max_end});
      int visible_len = visible_end - visible_start;

      if (visible_len <= 0)
        break;  // No more visible chars in this line, stop rendering

      float x = left + advance * (visible_start - x_offset_chars);

      Text(&text[visible_start], visible_len, font_size, x, y, it->color,
           it->underline_color, it->background_color, 1.0f);

      remaining_chars -= visible_len;
    }

    y += line_height;
  }
}

float Window::GetAdvance(float font_size) const {
  return m_advanceEm * font_size;
}

float Window::GetLineWidth(float font_size, int num_chars) const {
  return m_advanceEm * font_size * num_chars;
}

float Window::GetLineHeight(float font_size) const {
  return m_lineHeightEm * font_size;
}

// TODO - refactor
UINT16 Window::GetGlyphIndex(char32_t codepoint) {
  if (codepoint <= 0xFFFF && m_wcharIndexesVector[codepoint] != 0) {
    return m_wcharIndexesVector[codepoint];
  } else if (m_glyphIndexCache.find(codepoint) != m_glyphIndexCache.end()) {
    return m_glyphIndexCache[codepoint];
  }
  UINT32 cp = static_cast<UINT32>(codepoint);
  UINT16 index;
  ThrowIfFailed(m_fontFace->GetGlyphIndicesW(&cp, 1, &index));
  if (codepoint <= 0xFFFF && index > 0) {
    m_wcharIndexesVector[codepoint] = index;
  } else {
    m_glyphIndexCache[codepoint] = index;
  }
  return index;
}

void Window::LoadFont(const wchar_t* font_name) {
  ComPtr<IDWriteFontCollection> fontCollection;
  ThrowIfFailed(m_dwriteFactory->GetSystemFontCollection(&fontCollection));

  UINT32 index = 0;
  BOOL exists = FALSE;
  ThrowIfFailed(fontCollection->FindFamilyName(font_name, &index, &exists));
  if (!exists)
    return;

  ComPtr<IDWriteFontFamily> fontFamily;
  ThrowIfFailed(fontCollection->GetFontFamily(index, &fontFamily));

  ComPtr<IDWriteFont> font;
  ThrowIfFailed(fontFamily->GetFirstMatchingFont(
      DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STRETCH_SEMI_CONDENSED,
      DWRITE_FONT_STYLE_NORMAL, &font));

  ThrowIfFailed(font->CreateFontFace(&m_fontFace));

  DWRITE_FONT_METRICS metrics;
  m_fontFace->GetMetrics(&metrics);

  FLOAT designUnitsPerEm = metrics.designUnitsPerEm;
  FLOAT lineHeightDesignUnits =
      metrics.ascent + metrics.descent + metrics.lineGap;

  m_lineHeightEm = lineHeightDesignUnits / designUnitsPerEm;
  m_baselineEm = metrics.ascent / designUnitsPerEm;
  m_underlinePosEm = m_baselineEm + metrics.underlinePosition /
                                        designUnitsPerEm * -1;  // y-top
  m_underlineThicknessEm = metrics.underlineThickness / designUnitsPerEm;

  UINT32 codePoint = U'M';
  UINT16 glyphIndex = 0;
  m_fontFace->GetGlyphIndicesW(&codePoint, 1, &glyphIndex);

  DWRITE_GLYPH_METRICS glyphMetrics = {};
  m_fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE);

  FLOAT advanceWidth = glyphMetrics.advanceWidth;  // Monospace

  m_advanceEm = (advanceWidth / designUnitsPerEm);
}

}  // namespace MTerm
