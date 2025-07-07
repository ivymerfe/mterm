#include "mterm.h"

#include <algorithm>

Mterm::Mterm() {}

void Mterm::Init(HWND hWnd) {
  m_hWindow = hWnd;

  ThrowIfFailed(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                  IID_PPV_ARGS(&m_d2dFactory)));

  RECT window_rect;
  GetWindowRect(hWnd, &window_rect);

  m_windowSize.width = window_rect.right - window_rect.left;
  m_windowSize.height = window_rect.bottom - window_rect.top;

  D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
  D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props =
      D2D1::HwndRenderTargetProperties(hWnd, m_windowSize);

  ThrowIfFailed(
      m_d2dFactory->CreateHwndRenderTarget(props, hwnd_props, &m_renderTarget));

  ThrowIfFailed(DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &m_dwriteFactory));

  LoadFont();

  m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

  m_defaultBrush = CreateBrush(0xffffff);
  m_closeBrush = CreateBrush(CLOSE_BUTTON_COLOR);
  m_maxBrush = CreateBrush(MAX_BUTTON_COLOR);
  m_minBrush = CreateBrush(MIN_BUTTON_COLOR);

  m_terminal = new Terminal(this);
  Render();

  m_renderThread = std::thread([this]() { this->RenderThread(); });

  m_isInitialized = true;
}

void Mterm::Destroy() {
  m_stopRendering.store(true);
  m_renderCv.notify_one();
  m_renderThread.join();
}

bool Mterm::IsInitialized() {
  return m_isInitialized;
}

void Mterm::Redraw() {
  m_renderVersion.fetch_add(1);
  m_renderCv.notify_one();
}

void Mterm::RenderThread() {
  std::unique_lock<std::mutex> lock(m_renderMutex);
  int rendered_version = -1;
  while (true) {
    m_renderCv.wait(lock, [this, &rendered_version]() {
      return rendered_version != m_renderVersion.load() ||
             this->m_stopRendering.load();
    });

    if (m_stopRendering.load()) {
      break;
    }
    rendered_version = m_renderVersion.load();
    Render();
  }
}

void Mterm::Render() {
  if (m_windowResized.load()) {
    m_renderTarget->Resize(m_windowSize);
    m_windowResized.store(false);
  }

  m_renderTarget->BeginDraw();
  m_renderTarget->Clear(D2D1::ColorF(WINDOW_BG_COLOR));

  if (m_terminal != nullptr) {
    m_terminal->Render();
  }

  float width = m_windowSize.width;
  float height = m_windowSize.height;
  float margin = 7;
  float min_button_y = floor((CAPTION_SIZE + margin) / 2);

  D2D1_RECT_F min_btn_rect = {
      width - MIN_BUTTON_OFFSET + margin, min_button_y,
      width - MIN_BUTTON_OFFSET + SYS_BUTTON_SIZE - margin, min_button_y + 1};

  m_renderTarget->FillRectangle(min_btn_rect, m_minBrush.Get());

  D2D1_RECT_F max_btn_rect = {
      width - MAX_BUTTON_OFFSET + margin, margin,
      width - MAX_BUTTON_OFFSET + SYS_BUTTON_SIZE - margin - 1.5,
      CAPTION_SIZE - 1.5};

  m_renderTarget->DrawRectangle(max_btn_rect, m_maxBrush.Get(), 1.5);

  m_renderTarget->DrawLine(
      {width - CLOSE_BUTTON_OFFSET + margin, margin},
      {width - CLOSE_BUTTON_OFFSET + SYS_BUTTON_SIZE - margin, CAPTION_SIZE},
      m_closeBrush.Get(), 1.5);

  m_renderTarget->DrawLine(
      {width - CLOSE_BUTTON_OFFSET + margin, CAPTION_SIZE},
      {width - CLOSE_BUTTON_OFFSET + SYS_BUTTON_SIZE - margin, margin},
      m_closeBrush.Get(), 1.5);

  ThrowIfFailed(m_renderTarget->EndDraw());
}

UINT16 Mterm::GetGlyphIndex(char32_t codepoint) {
  if (m_glyphIndexCache.find(codepoint) != m_glyphIndexCache.end()) {
    return m_glyphIndexCache[codepoint];
  }
  UINT32 cp = static_cast<UINT32>(codepoint);
  UINT16 index;
  ThrowIfFailed(m_fontFace->GetGlyphIndicesW(&cp, 1, &index));
  m_glyphIndexCache[codepoint] = index;
  return index;
}

// Warning - indices should be valid when EndDraw is called
void Mterm::DrawGlyphs(const UINT16* indices, int count, int x, int y, int color) {
  float pos_x = round(x * GetAdvance());
  float baseline_y = round(y * GetLineHeight() + GetBaselineOffset() + CAPTION_SIZE);

  DWRITE_GLYPH_RUN glyphRun = {};
  glyphRun.fontFace = m_fontFace.Get();
  glyphRun.fontEmSize = m_fontSize;
  glyphRun.glyphCount = count;
  glyphRun.glyphIndices = indices;
  glyphRun.glyphAdvances = nullptr;  // use natural advance
  glyphRun.isSideways = FALSE;
  glyphRun.bidiLevel = 0;

  m_defaultBrush->SetColor(D2D1::ColorF(color));

  m_renderTarget->DrawGlyphRun(D2D1::Point2F(pos_x, baseline_y), &glyphRun,
                               m_defaultBrush.Get(),
                               DWRITE_MEASURING_MODE_NATURAL);
}

void Mterm::DrawUnderline(int x, int y, int length, int color) {
  float pos_x = round(x * GetAdvance());
  float pos_y =
      y * GetLineHeight() + m_underlinePosEm * m_fontSize + CAPTION_SIZE;
  float width = GetLineWidth(length);
  float thickness = m_underlineThicknessEm * m_fontSize * 2;
  D2D1_RECT_F rect = {pos_x, pos_y, pos_x + width, pos_y + thickness};

  m_defaultBrush->SetColor(D2D1::ColorF(color));
  m_renderTarget->FillRectangle(&rect, m_defaultBrush.Get());
}

void Mterm::DrawCursor(int x, int y, int color) {
  float pos_x = x * GetAdvance();
  float line_height = GetLineHeight();
  float start_y = y * line_height + CAPTION_SIZE;
  float end_y = start_y + line_height;
  float half_width = 0.5;
  D2D1_RECT_F rect = {round(pos_x - half_width), start_y,
                      round(pos_x + half_width), end_y};

  m_defaultBrush->SetColor(D2D1::ColorF(color));
  m_renderTarget->FillRectangle(&rect, m_defaultBrush.Get());
}

void Mterm::DrawBackground(int start_x,
                          int start_y,
                          int end_x,
                          int end_y,
                          int color) {
  D2D1_RECT_F rect = {round(start_x * GetAdvance()),
                      round(start_y * GetLineHeight() + CAPTION_SIZE),
                      round((end_x + 1) * GetAdvance()),
                      round((end_y + 1) * GetLineHeight() + CAPTION_SIZE)};

  m_defaultBrush->SetColor(D2D1::ColorF(color));
  m_defaultBrush->SetOpacity(TEXT_BACKGROUND_OPACITY);
  m_renderTarget->FillRectangle(&rect, m_defaultBrush.Get());
  m_defaultBrush->SetOpacity(1.0);
}

int Mterm::GetNumRows() const {
  return floor(float(m_windowSize.height - CAPTION_SIZE) / GetLineHeight());
}

int Mterm::GetNumColumns() const {
  return floor(float(m_windowSize.width) / (m_advanceEm * m_fontSize));
}

void Mterm::Resize(unsigned int width, unsigned int height) {
  if (m_windowSize.width == width && m_windowSize.height == height) {
    return;
  }
  m_windowSize.width = width;
  m_windowSize.height = height;
  m_windowResized.store(true);
  Redraw();
}

void Mterm::LoadFont() {
  ComPtr<IDWriteFontCollection> fontCollection;
  ThrowIfFailed(m_dwriteFactory->GetSystemFontCollection(&fontCollection));

  UINT32 index = 0;
  BOOL exists = FALSE;
  ThrowIfFailed(fontCollection->FindFamilyName(FONT_NAME, &index, &exists));
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

ComPtr<ID2D1SolidColorBrush> Mterm::CreateBrush(int color) {
  ComPtr<ID2D1SolidColorBrush> brush;
  ThrowIfFailed(
      m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(color), &brush));
  return brush;
}

float Mterm::GetAdvance() const {
  return m_advanceEm * m_fontSize;
}

float Mterm::GetLineWidth(int num_chars) const {
  return round(m_advanceEm * m_fontSize * num_chars);
}

float Mterm::GetLineHeight() const {
  return round(m_lineHeightEm * m_fontSize);
}

float Mterm::GetBaselineOffset() const {
  return round(m_baselineEm * m_fontSize);
}

void Mterm::KeyDown(int key_code) {
  if (m_terminal != nullptr) {
    m_terminal->KeyDown(key_code);
  }
}

void Mterm::KeyUp(int key_code) {
  if (m_terminal != nullptr) {
    m_terminal->KeyUp(key_code);
  }
}

void Mterm::Input(char32_t key) {
  if (m_terminal != nullptr) {
    m_terminal->Input(key);
  }
}

void Mterm::MouseMove(int x, int y) {
  if (m_terminal != nullptr) {
    m_terminal->MouseMove(x, y);
  }
}

void Mterm::MouseDown(int x, int y, int button) {
  if (m_terminal != nullptr) {
    m_terminal->MouseDown(x, y, button);
  }
}

void Mterm::MouseUp(int x, int y, int button) {
  if (m_terminal != nullptr) {
    m_terminal->MouseUp(x, y, button);
  }
}

void Mterm::Scroll(int x, int y, int delta) {
  if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) {
    float new_font_size = m_fontSize + delta;
    if (2 < new_font_size && new_font_size < 200) {
      m_fontSize = new_font_size;
      Redraw();
    }
  } else {
    if (m_terminal != nullptr) {
      m_terminal->Scroll(x, y, delta);
    }
  }
}
