#include "Window.h"

#include "Windows.h"

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#include <d2d1.h>
#include <dwrite.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace Microsoft::WRL;

static inline void ThrowIfFailed(HRESULT hr) {
  if (FAILED(hr)) {
    throw std::exception();
  }
}

namespace MTerm {

class Window::Impl {
 private:
  bool m_isInitialized = false;
  Config m_config;
  HWND m_hWindow;
  std::atomic<HCURSOR> m_hCursor;

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

 public:
  int Create(Config config) {
    m_config = config;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    const wchar_t CLASS_NAME[] = L"mterm";
    WNDCLASSEX windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = CLASS_NAME;
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    SetCursor(config.cursor_id);
    windowClass.hCursor = m_hCursor;
    windowClass.hbrBackground = 0;

    HICON hIcon = (HICON)LoadImage(NULL, L"icon.ico", IMAGE_ICON, 128, 128,
                                   LR_LOADFROMFILE);
    HICON hIconSm = (HICON)LoadImage(NULL, L"icon.ico", IMAGE_ICON, 64, 64,
                                     LR_LOADFROMFILE);

    windowClass.hIcon = hIcon;
    windowClass.hIconSm = hIconSm;

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
      return 2;
    }
    SetCurrentProcessExplicitAppUserModelID(L"MTerm.Terminal.App.1.0");
    if (hIcon) {
      SendMessage(m_hWindow, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }
    if (hIconSm) {
      SendMessage(m_hWindow, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
    }
    ShowWindow(m_hWindow, SW_SHOWDEFAULT);
    std::thread init_thread([this]() { this->InitRenderer(); });
    init_thread.detach();

    MSG msg = {};
    int exit_code = 0;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
      if (msg.message == WM_QUIT) {
        exit_code = static_cast<int>(msg.wParam);
        break;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    StopRenderer();
    return exit_code;
  }

  void Destroy() { DestroyWindow(m_hWindow); }

  void SetCursor(int cursor_id) {
    HCURSOR hCursor = nullptr;
    switch (cursor_id) {
      case 0:  // ARROW
        hCursor = LoadCursor(NULL, IDC_ARROW);
        break;
      case 1:  // IBEAM
        hCursor = LoadCursor(NULL, IDC_IBEAM);
        break;
      case 2:  // WAIT
        hCursor = LoadCursor(NULL, IDC_WAIT);
        break;
      case 3:  // CROSS
        hCursor = LoadCursor(NULL, IDC_CROSS);
        break;
      case 4:  // UPARROW
        hCursor = LoadCursor(NULL, IDC_UPARROW);
        break;
      case 5:  // SIZENWSE
        hCursor = LoadCursor(NULL, IDC_SIZENWSE);
        break;
      case 6:  // SIZENESW
        hCursor = LoadCursor(NULL, IDC_SIZENESW);
        break;
      case 7:  // SIZEWE
        hCursor = LoadCursor(NULL, IDC_SIZEWE);
        break;
      case 8:  // SIZENS
        hCursor = LoadCursor(NULL, IDC_SIZENS);
        break;
      case 9:  // SIZEALL
        hCursor = LoadCursor(NULL, IDC_SIZEALL);
        break;
      case 10:  // HAND
        hCursor = LoadCursor(NULL, IDC_HAND);
        break;
      default:
        hCursor = LoadCursor(NULL, IDC_ARROW);
        break;
    }
    m_hCursor = hCursor;
  }

  void Drag() {
    if (!m_isInitialized) {
      return;
    }
    SendMessage(m_hWindow, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
  }

  void Maximize() {
    if (!m_isInitialized) {
      return;
    }
    ShowWindow(m_hWindow, SW_MAXIMIZE);
  }

  void Minimize() {
    if (!m_isInitialized) {
      return;
    }
    ShowWindow(m_hWindow, SW_MINIMIZE);
  }

  void Restore() {
    if (!m_isInitialized) {
      return;
    }
    ShowWindow(m_hWindow, SW_RESTORE);
  }

  bool IsMaximized() {
    if (!m_isInitialized) {
      return false;
    }
    return IsZoomed(m_hWindow) != 0;
  }

  void InitRenderer() {
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

    ThrowIfFailed(m_d2dFactory->CreateHwndRenderTarget(props, hwnd_props,
                                                       &m_renderTarget));

    ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                      __uuidof(IDWriteFactory),
                                      &m_dwriteFactory));

    LoadFont(m_config.font_name.c_str());

    m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    ThrowIfFailed(m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),
                                                        &m_defaultBrush));

    m_textBuffer.resize(TEXT_BUFFER_SIZE);
    m_wcharIndexesVector.resize(0xFFFF + 1);

    m_renderThread = std::thread([this]() { this->RenderThread(); });

    m_isInitialized = true;
    Redraw();
  }

  void StopRenderer() {
    m_stopRendering.store(true);
    m_renderCv.notify_one();
    m_renderThread.join();
  }

  void Redraw() {
    if (!m_isInitialized) {
      return;
    }
    m_contentVersion.fetch_add(1);
    m_renderCv.notify_one();
  }

  void RenderThread() {
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

  void Render() {
    if (!m_isInitialized) {
      return;
    }
    m_renderedVersion.store(m_contentVersion.load());
    m_renderTarget->BeginDraw();

    m_textBufferPos = 0;
    m_config.render_callback();

    ThrowIfFailed(m_renderTarget->EndDraw());
  }

  void Resize(unsigned int width, unsigned int height) {
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
    if (m_config.resize_callback) {
      m_config.resize_callback(width, height);
    }
    std::unique_lock lock(m_resizeMutex);

    m_windowSize.width = width;
    m_windowSize.height = height;
    m_renderTarget->Resize(m_windowSize);
    Render();
  }

  int GetWidth() const { return m_windowSize.width; }

  int GetHeight() const { return m_windowSize.height; }

  void Clear(int color) { m_renderTarget->Clear(D2D1::ColorF(color)); }

  void Text(const char32_t* text,
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
    m_defaultBrush->SetOpacity(opacity);
    if (background_color != -1) {
      float width = GetLineWidth(font_size, length);
      float height = GetLineHeight(font_size);
      m_defaultBrush->SetColor(D2D1::ColorF(background_color));
      m_renderTarget->FillRectangle({x, y, x + width, y + height},
                                    m_defaultBrush.Get());
    }
    if (color != -1) {
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
    }
    if (underline_color != -1) {
      float underline_y = y + m_underlinePosEm * font_size;
      float width = GetLineWidth(font_size, length);
      float thickness = m_underlineThicknessEm * font_size;
      m_defaultBrush->SetColor(D2D1::ColorF(underline_color));
      m_renderTarget->DrawLine({x, underline_y}, {x + width, underline_y},
                               m_defaultBrush.Get(), thickness);
    }
  }

  void Line(float start_x,
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

  void Rect(float left,
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

  void Outline(float left,
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

  void TextBuffer(ColoredTextBuffer* buffer,
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

  float GetAdvance(float font_size) const { return m_advanceEm * font_size; }

  float GetLineWidth(float font_size, int num_chars) const {
    return m_advanceEm * font_size * num_chars;
  }

  float GetLineHeight(float font_size) const {
    return m_lineHeightEm * font_size;
  }

 private:
  // TODO - refactor
  UINT16 GetGlyphIndex(char32_t codepoint) {
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

  void LoadFont(const wchar_t* font_name) {
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

  wchar_t pending_high_surrogate = 0;
  bool is_sizing = false;
  bool is_tracking = false;
  static LRESULT CALLBACK WindowProc(HWND hWnd,
                                     UINT uMsg,
                                     WPARAM wParam,
                                     LPARAM lParam) {
    Impl* window =
        reinterpret_cast<Impl*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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
      case WM_GETICON: {
        // Возвращаем нашу иконку когда система запрашивает её
        if (wParam == ICON_BIG) {
          HICON hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICON);
          if (hIcon)
            return (LRESULT)hIcon;
        } else if (wParam == ICON_SMALL) {
          HICON hIconSm = (HICON)GetClassLongPtr(hWnd, GCLP_HICONSM);
          if (hIconSm)
            return (LRESULT)hIconSm;
        }
        break;
      }
      case WM_NCHITTEST: {
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
        return HTCLIENT;
      }
      case WM_NCPAINT:
      case WM_SETTEXT:
        return 0;  // Block all default non-client painting
      case WM_NCACTIVATE: {
        // Tell Windows to not redraw the non-client area (title bar)
        return DefWindowProc(hWnd, uMsg, FALSE, -1);
      }
      case WM_GETMINMAXINFO: {
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
        window->is_sizing = true;
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
        if (!window->is_sizing) {
          window->Redraw();
        }
        window->is_sizing = false;
        return 0;
      }
      case WM_CHAR: {
        wchar_t ch = (wchar_t)wParam;

        if (ch >= 0xD800 && ch <= 0xDBFF) {
          // High surrogate, store it
          window->pending_high_surrogate = ch;
        } else if (ch >= 0xDC00 && ch <= 0xDFFF) {
          // Low surrogate
          if (window->pending_high_surrogate != 0) {
            // Combine into UTF-32
            char32_t codepoint =
                ((window->pending_high_surrogate - 0xD800) << 10) +
                (ch - 0xDC00) + 0x10000;
            window->pending_high_surrogate = 0;

            if (window->m_config.input_callback) {
              window->m_config.input_callback(codepoint);
            }
          } else {
            // Unexpected low surrogate — error or ignore
          }
        } else {
          char32_t codepoint = ch;
          window->pending_high_surrogate = 0;

          if (window->m_config.input_callback) {
            window->m_config.input_callback(codepoint);
          }
        }
        return 0;
      }
      case WM_SYSKEYDOWN:
      case WM_KEYDOWN: {
        if (window->m_config.keydown_callback) {
          window->m_config.keydown_callback(wParam);
        }
        return 0;
      }
      case WM_SYSKEYUP:
      case WM_KEYUP: {
        if (window->m_config.keyup_callback) {
          window->m_config.keyup_callback(wParam);
        }
        return 0;
      }
      case WM_MOUSEMOVE: {
        if (!window) {
          return 0;
        }
        if (window->m_config.mousemove_callback) {
          int x = GET_X_LPARAM(lParam);
          int y = GET_Y_LPARAM(lParam);
          window->m_config.mousemove_callback(x, y);
        }
        if (window->m_config.mouse_leave_callback && !window->is_tracking) {
          TRACKMOUSEEVENT tme = {};
          tme.cbSize = sizeof(TRACKMOUSEEVENT);
          tme.dwFlags = TME_LEAVE;
          tme.hwndTrack = hWnd;
          TrackMouseEvent(&tme);
          window->is_tracking = true;
        }
        return 0;
      }
      case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT) {
          ::SetCursor(window->m_hCursor);
          return TRUE;
        }
        break;
      }
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_XBUTTONDOWN: {
        if (window && window->m_config.mousedown_callback) {
          int button;
          if (uMsg == WM_LBUTTONDOWN) {
            button = 0;
          } else if (uMsg == WM_MBUTTONDOWN) {
            button = 1;
          } else if (uMsg == WM_RBUTTONDOWN) {
            button = 2;
          } else if (uMsg == WM_XBUTTONDOWN) {
            // LOWORD(wParam) gives which X button (1 = XBUTTON1, 2 = XBUTTON2)
            button = 3 + GET_XBUTTON_WPARAM(wParam) - 1;
          } else {
            button = -1;
          }
          int x = GET_X_LPARAM(lParam);
          int y = GET_Y_LPARAM(lParam);
          window->m_config.mousedown_callback(button, x, y);
        }
        return 0;
      }
      case WM_LBUTTONUP:
      case WM_MBUTTONUP:
      case WM_RBUTTONUP:
      case WM_XBUTTONUP: {
        if (window && window->m_config.mouseup_callback) {
          int button;
          if (uMsg == WM_LBUTTONUP) {
            button = 0;
          } else if (uMsg == WM_MBUTTONUP) {
            button = 1;
          } else if (uMsg == WM_RBUTTONUP) {
            button = 2;
          } else if (uMsg == WM_XBUTTONUP) {
            button = 3 + GET_XBUTTON_WPARAM(wParam) - 1;
          } else {
            button = -1;
          }
          int x = GET_X_LPARAM(lParam);
          int y = GET_Y_LPARAM(lParam);
          window->m_config.mouseup_callback(button, x, y);
        }
        return 0;
      }
      case WM_LBUTTONDBLCLK:
      case WM_MBUTTONDBLCLK:
      case WM_RBUTTONDBLCLK:
      case WM_XBUTTONDBLCLK: {
        if (window && window->m_config.doubleclick_callback) {
          int button;
          if (uMsg == WM_LBUTTONDBLCLK) {
            button = 0;
          } else if (uMsg == WM_MBUTTONDBLCLK) {
            button = 1;
          } else if (uMsg == WM_RBUTTONDBLCLK) {
            button = 2;
          } else if (uMsg == WM_XBUTTONDBLCLK) {
            button = 3 + GET_XBUTTON_WPARAM(wParam) - 1;
          } else {
            button = -1;
          }
          int x = GET_X_LPARAM(lParam);
          int y = GET_Y_LPARAM(lParam);
          window->m_config.doubleclick_callback(button, x, y);
        }
        return 0;
      }
      case WM_MOUSEWHEEL: {
        if (window && window->m_config.scroll_callback) {
          int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
          int x = GET_X_LPARAM(lParam);
          int y = GET_Y_LPARAM(lParam);
          window->m_config.scroll_callback(delta, x, y);
        }
        return 0;
      }
      case WM_MOUSELEAVE: {
        window->is_tracking = false;
        if (window && window->m_config.mouse_leave_callback) {
          window->m_config.mouse_leave_callback();
        }
        return 0;
      }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }
};

Window::Window() : m_impl(std::make_unique<Impl>()) {}

Window::~Window() {}

int Window::Create(Config config) {
  return m_impl->Create(config);
}

void Window::Destroy() {
  m_impl->Destroy();
}

void Window::SetCursor(int cursor_id) {
  m_impl->SetCursor(cursor_id);
}

void Window::Drag() {
  m_impl->Drag();
}

void Window::Maximize() {
  m_impl->Maximize();
}

void Window::Minimize() {
  m_impl->Minimize();
}

void Window::Restore() {
  m_impl->Restore();
}

bool Window::IsMaximized() {
  return m_impl->IsMaximized();
}

void Window::Redraw() {
  m_impl->Redraw();
}

int Window::GetWidth() const {
  return m_impl->GetWidth();
}

int Window::GetHeight() const {
  return m_impl->GetHeight();
}

void Window::Clear(int color) {
  m_impl->Clear(color);
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
  m_impl->Text(text, length, font_size, x, y, color, underline_color,
               background_color, opacity);
}

void Window::Line(float start_x,
                  float start_y,
                  float end_x,
                  float end_y,
                  float thickness,
                  int color,
                  float opacity) {
  m_impl->Line(start_x, start_y, end_x, end_y, thickness, color, opacity);
}

void Window::Rect(float left,
                  float top,
                  float right,
                  float bottom,
                  int color,
                  float opacity) {
  m_impl->Rect(left, top, right, bottom, color, opacity);
}

void Window::Outline(float left,
                     float top,
                     float right,
                     float bottom,
                     float thickness,
                     int color,
                     float opacity) {
  m_impl->Outline(left, top, right, bottom, thickness, color, opacity);
}

void Window::TextBuffer(ColoredTextBuffer* buffer,
                        float left,
                        float top,
                        float width,
                        float height,
                        int x_offset_chars,
                        int y_offset_lines,
                        float font_size) {
  m_impl->TextBuffer(buffer, left, top, width, height, x_offset_chars,
                     y_offset_lines, font_size);
}

float Window::GetAdvance(float font_size) const {
  return m_impl->GetAdvance(font_size);
}

float Window::GetLineWidth(float font_size, int num_chars) const {
  return m_impl->GetLineWidth(font_size, num_chars);
}

float Window::GetLineHeight(float font_size) const {
  return m_impl->GetLineHeight(font_size);
}

}  // namespace MTerm
