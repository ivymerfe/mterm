#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#include <thread>

#include "MTerm.h"
#include "resource.h"
#include "defaults.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND g_hWindow;
MTerm::MTerm g_MTerm;

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR pCmdLine,
                   int nCmdShow) {
  const wchar_t CLASS_NAME[] = L"mterm";
  WNDCLASSEX windowClass = {};
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.lpfnWndProc = WindowProc;
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

  int width = WINDOW_WIDTH;
  int height = WINDOW_HEIGHT;

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

  g_hWindow = CreateWindowEx(NULL,
                             CLASS_NAME,
                             NULL,           // Window text
                             WS_POPUP | WS_MINIMIZEBOX |
                                 WS_MAXIMIZEBOX,  // Window style
                             pos_x, pos_y, width, height,
                             NULL,       // Parent window
                             NULL,       // Menu
                             hInstance,  // Instance handle
                             NULL        // Additional application data
  );
  if (g_hWindow == NULL) {
    return 0;
  }
  ShowWindow(g_hWindow, nCmdShow);

  std::thread init_thread([]() {
    g_MTerm.Init(g_hWindow);
  });
  init_thread.detach();

  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  g_MTerm.Destroy();
  return 0;
}

static int HitTest(HWND hWnd, WPARAM wparam, LPARAM lParam) {
  RECT rect;
  GetWindowRect(hWnd, &rect);
  int x = GET_X_LPARAM(lParam);
  int y = GET_Y_LPARAM(lParam);

  if (!IsZoomed(hWnd))
  {
    int left = x < rect.left + BORDER_SIZE;
    int right = x > rect.right - BORDER_SIZE;
    int top = y < rect.top + BORDER_SIZE;
    int bottom = y > rect.bottom - BORDER_SIZE;

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

  if (y < rect.top + CAPTION_SIZE) {
    float close_button_mid = rect.right - CLOSE_BUTTON_OFFSET;
    if (x >= close_button_mid - BUTTON_SIZE && x <= close_button_mid + BUTTON_SIZE) {
      return HTCLOSE;
    }
    float min_button_mid = rect.right - MIN_BUTTON_OFFSET;
    if (x >= min_button_mid - BUTTON_SIZE && x <= min_button_mid + BUTTON_SIZE) {
      return HTMINBUTTON;
    }
    return HTCAPTION;
  }
  return HTCLIENT;
}

LRESULT CALLBACK WindowProc(HWND hWnd,
                            UINT uMsg,
                            WPARAM wParam,
                            LPARAM lParam) {
  static wchar_t pending_high_surrogate = 0;
  static bool is_sizing = false;

  switch (uMsg) {
    case WM_CREATE: {
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
    case WM_NCHITTEST:
      return HitTest(hWnd, wParam, lParam);
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
      MINMAXINFO* pMinMax = (MINMAXINFO*)lParam;

      pMinMax->ptMinTrackSize.x = WINDOW_MIN_WIDTH;
      pMinMax->ptMinTrackSize.y = WINDOW_MIN_HEIGHT;
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
      is_sizing = true;
      if (g_MTerm.IsInitialized()) {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        if (width > WINDOW_MIN_WIDTH && height > WINDOW_MIN_HEIGHT) {
          g_MTerm.Resize(width, height);
        }
      }
      return 0;
    }
    case WM_ERASEBKGND: {
      return 0;
    }
    case WM_PAINT: {
      ValidateRect(hWnd, NULL);
      if (g_MTerm.IsInitialized() && !is_sizing) {
        g_MTerm.Redraw();
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

          g_MTerm.Input(codepoint);
        } else {
          // Unexpected low surrogate — error or ignore
        }
      } else {
        char32_t codepoint = ch;
        pending_high_surrogate = 0;
        g_MTerm.Input(codepoint);
      }
      return 0;
    }
    case WM_SYSKEYDOWN: {
      if (wParam == 'E') {
        PostQuitMessage(0);
      }
      if (wParam == 'Q') {
        RECT window_rect;
        GetWindowRect(hWnd, &window_rect);
        POINT cursor_pt;
        if (GetCursorPos(&cursor_pt)) {
          if (cursor_pt.x - window_rect.left > WINDOW_MIN_WIDTH) {
            window_rect.right = cursor_pt.x;
          }
          if (cursor_pt.y - window_rect.top > WINDOW_MIN_HEIGHT) {
            window_rect.bottom = cursor_pt.y;
          }
          SetWindowPos(hWnd, 0, 0, 0, window_rect.right - window_rect.left,
                       window_rect.bottom - window_rect.top,
                       SWP_NOMOVE | SWP_NOZORDER);
        }
      }
      break;
    }
    case WM_KEYDOWN: {
      g_MTerm.KeyDown(wParam);
      break;
    }
    case WM_KEYUP: {
      g_MTerm.KeyUp(wParam);
      break;
    }
    case WM_LBUTTONDOWN: {
      if (GetAsyncKeyState(VK_LMENU) & 0x8000) {
        return DefWindowProc(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
      }
      break;
    }
    case WM_RBUTTONUP: {
      g_MTerm.MouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 1);
      break;
    }
    case WM_MOUSEWHEEL: {
      short delta = GET_WHEEL_DELTA_WPARAM(wParam);
      int x = GET_X_LPARAM(lParam);
      int y = GET_Y_LPARAM(lParam);
      g_MTerm.Scroll(x, y, delta / WHEEL_DELTA);
      return 0;
    }
  }
  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
