#include "Renderer.h"

#define WIN32_LEAN_AND_MEAN
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "defaults.h"

using namespace Microsoft::WRL;

static inline void ThrowIfFailed(HRESULT hr) {
  if (FAILED(hr)) {
    throw std::exception();
  }
}

namespace MTerm {

class Renderer::Impl {
 private:
  bool m_isInitialized = false;
  HWND m_hWindow;
  std::function<void()> m_renderCallback;

  D2D1_SIZE_U m_windowSize;
  ComPtr<ID2D1Factory> m_d2dFactory;
  ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
  ComPtr<IDWriteFactory> m_dwriteFactory;
  ComPtr<ID2D1SolidColorBrush> m_defaultBrush;

  ComPtr<IDWriteFontFace> m_fontFace;
  float m_advanceEm;
  float m_lineHeightEm;
  float m_baselineEm;
  float m_underlinePosEm;
  float m_underlineThicknessEm;
  std::unordered_map<char32_t, unsigned short> m_glyphIndexCache;
  std::vector<unsigned short> m_wcharIndexesVector;

  std::vector<unsigned short> m_textBuffer;
  unsigned int m_textBufferPos = 0;

  std::atomic<long long> m_renderVersion = 0;
  std::atomic<bool> m_stopRendering = false;
  std::thread m_renderThread;
  std::mutex m_renderMutex;
  std::condition_variable m_renderCv;
  std::atomic<bool> m_windowResized = false;
  std::mutex m_resizeMutex;

 public:
  bool Init(void* window,
            const wchar_t* font_name,
            std::function<void()> render_callback) {
    m_hWindow = (HWND)window;
    m_renderCallback = render_callback;

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 IID_PPV_ARGS(&m_d2dFactory)))) {
      return false;
    };

    RECT window_rect;
    GetWindowRect(m_hWindow, &window_rect);

    m_windowSize.width = window_rect.right - window_rect.left;
    m_windowSize.height = window_rect.bottom - window_rect.top;

    D2D1_RENDER_TARGET_PROPERTIES props =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE);
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props =
        D2D1::HwndRenderTargetProperties(m_hWindow, m_windowSize);

    if (FAILED(m_d2dFactory->CreateHwndRenderTarget(props, hwnd_props,
                                                    &m_renderTarget))) {
      return false;
    }

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   __uuidof(IDWriteFactory),
                                   &m_dwriteFactory))) {
      return false;
    }

    LoadFont(font_name);

    m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    if (FAILED(m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),
                                                     &m_defaultBrush))) {
      return false;
    }

    m_textBuffer.resize(TEXT_BUFFER_SIZE);
    m_wcharIndexesVector.resize(0xFFFF + 1);

    m_renderThread = std::thread([this]() { this->RenderThread(); });

    m_isInitialized = true;
    return true;
  }

  bool IsInitialized() const { return m_isInitialized; }

  void Destroy() {
    m_stopRendering.store(true);
    m_renderCv.notify_one();
    m_renderThread.join();
  }

  void Redraw() {
    m_renderVersion.fetch_add(1);
    m_renderCv.notify_one();
  }

  void RenderThread() {
    std::unique_lock<std::mutex> lock(m_renderMutex);
    long long rendered_version = -1;
    while (true) {
      m_renderCv.wait(lock, [this, &rendered_version]() {
        return rendered_version != m_renderVersion.load() ||
               this->m_stopRendering.load();
      });

      if (m_stopRendering.load()) {
        break;
      }
      rendered_version = m_renderVersion.load();
      std::unique_lock lock(m_resizeMutex);
      Render();
    }
  }

  void Render() {
    m_renderTarget->BeginDraw();

    m_textBufferPos = 0;
    m_renderCallback();

    ThrowIfFailed(m_renderTarget->EndDraw());
  }

  void Resize(unsigned int width, unsigned int height) {
    if (m_windowSize.width == width && m_windowSize.height == height) {
      return;
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

  float GetAdvance(float font_size) const { return m_advanceEm * font_size; }

  float GetLineWidth(float font_size, int num_chars) const {
    return m_advanceEm * font_size * num_chars;
  }

  float GetLineHeight(float font_size) const {
    return m_lineHeightEm * font_size;
  }

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
};

Renderer::Renderer() : m_impl(std::make_unique<Impl>()) {}

Renderer::~Renderer() {}

bool Renderer::Init(void* window,
                    const wchar_t* font_name,
                    std::function<void()> render_callback) {
  return m_impl->Init(window, font_name, render_callback);
}

void Renderer::Destroy() {
  m_impl->Destroy();
}

bool Renderer::IsInitialized() {
  return m_impl->IsInitialized();
}

void Renderer::Redraw() {
  m_impl->Redraw();
}

void Renderer::Resize(unsigned int width, unsigned int height) {
  m_impl->Resize(width, height);
}

int Renderer::GetWidth() {
  return m_impl->GetWidth();
}

int Renderer::GetHeight() {
  return m_impl->GetHeight();
}

void Renderer::Clear(int color) {
  m_impl->Clear(color);
}

void Renderer::Text(const char32_t* text,
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

void Renderer::Line(float start_x,
                    float start_y,
                    float end_x,
                    float end_y,
                    float thickness,
                    int color,
                    float opacity) {
  m_impl->Line(start_x, start_y, end_x, end_y, thickness, color, opacity);
}

void Renderer::Rect(float left,
                    float top,
                    float right,
                    float bottom,
                    int color,
                    float opacity) {
  m_impl->Rect(left, top, right, bottom, color, opacity);
}

void Renderer::Outline(float left,
                       float top,
                       float right,
                       float bottom,
                       float thickness,
                       int color,
                       float opacity) {
  m_impl->Outline(left, top, right, bottom, thickness, color, opacity);
}

float Renderer::GetAdvance(float font_size) const {
  return m_impl->GetAdvance(font_size);
}

float Renderer::GetLineWidth(float font_size, int num_chars) const {
  return m_impl->GetLineWidth(font_size, num_chars);
}

float Renderer::GetLineHeight(float font_size) const {
  return m_impl->GetLineHeight(font_size);
}

}  // namespace MTerm
