// Milestone 0 (Windows): custom frameless window with title bar, min/max/close, resize, snap.
// Icons: Adwaita-style SVG icons rendered via NanoSVG.
// Button states: hover, pressed, active — tracked via WM_LBUTTONDOWN/UP + TrackMouseEvent.
#include <windows.h>
#include <dwmapi.h>
#include <cstdio>
#include <cstring>
#include <cmath>

// GDI+ for antialiased circle drawing
#define NOMINMAX
#include <gdiplus.h>
using namespace Gdiplus;

static ULONG_PTR g_gdiplusToken = 0;

// NanoSVG: header-only SVG parser + rasterizer
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg.h"
#include "nanosvgrast.h"

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWM_COLOR_NONE
#define DWM_COLOR_NONE 0xFFFFFFFE
#endif

enum class Theme { Light, Dark };
static Theme g_theme = Theme::Light;

static const int TITLE_H   = 32;
static const int BORDER    = 8;
static const int ICON_SIZE = 16;   // SVG canvas size (px)
static const int ICON_PAD  = 0;    // no trimming: icon drawn at full ICON_SIZE
static const int BTN_PAD   = 6;    // padding from icon edge to button edge
static const int BTN_W     = ICON_SIZE + 2*BTN_PAD;  // button slot width
static const int BTN_H     = BTN_W;                  // square button slot

// Adwaita-style SVG icons (16×16, paths use fill/stroke "currentColor")
static const char* SVG_MINIMIZE =
    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'>"
    "<line x1='4' y1='8' x2='12' y2='8' stroke='%06X' stroke-width='1.5' stroke-linecap='round'/>"
    "</svg>";

static const char* SVG_MAXIMIZE =
    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'>"
    "<rect x='3.5' y='3.5' width='9' height='9' rx='1' fill='none' stroke='%06X' stroke-width='1.5'/>"
    "</svg>";

static const char* SVG_RESTORE =
    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'>"
    "<rect x='5.5' y='3.5' width='7' height='7' rx='1' fill='none' stroke='%06X' stroke-width='1.5'/>"
    "<rect x='3.5' y='5.5' width='7' height='7' rx='1' fill='none' stroke='%06X' stroke-width='1.5'/>"
    "</svg>";

static const char* SVG_CLOSE =
    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'>"
    "<line x1='4.5' y1='4.5' x2='11.5' y2='11.5' stroke='%06X' stroke-width='1.5' stroke-linecap='round'/>"
    "<line x1='11.5' y1='4.5' x2='4.5' y2='11.5' stroke='%06X' stroke-width='1.5' stroke-linecap='round'/>"
    "</svg>";

// -------------------------------------------------------------------------
// Theme colours
// -------------------------------------------------------------------------
static COLORREF bg_color()     { return g_theme == Theme::Light ? RGB(242,242,242) : RGB(32,32,32); }
static COLORREF fg_color()     { return g_theme == Theme::Light ? RGB(30,30,30)   : RGB(220,220,220); }
static COLORREF title_bg()     { return g_theme == Theme::Light ? RGB(235,235,235) : RGB(28,28,28); }

// Adwaita-like button background colours per state
static COLORREF btn_hover_bg()   { return g_theme == Theme::Light ? RGB(205,205,205) : RGB(60,60,60); }
static COLORREF btn_press_bg()   { return g_theme == Theme::Light ? RGB(180,180,180) : RGB(45,45,45); }
static COLORREF close_hover_bg() { return RGB(192, 28, 40);  }  // Adwaita red
static COLORREF close_press_bg() { return RGB(162, 20, 30);  }  // darker red
static COLORREF icon_color()     { return g_theme == Theme::Light ? RGB(60,60,60) : RGB(200,200,200); }
static COLORREF close_icon_hover_color() { return RGB(255,255,255); }

// -------------------------------------------------------------------------
// Button hover/press state  (0=min, 1=max, 2=close, -1=none)
// -------------------------------------------------------------------------
static int  g_hover   = -1;
static int  g_pressed = -1;   // which button is currently held down
static bool g_tracking = false;

// -------------------------------------------------------------------------
// SVG icon rendering via NanoSVG
// -------------------------------------------------------------------------

// Blit a 16×16 RGBA pixel buffer onto hdc at (x,y), centred inside rect r.
static void blit_icon(HDC hdc, const RECT& r, unsigned char* pixels, int iconW, int iconH) {
    int cx = (r.left + r.right)  / 2;
    int cy = (r.top  + r.bottom) / 2;
    int dx = cx - iconW / 2;
    int dy = cy - iconH / 2;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = iconW;
    bmi.bmiHeader.biHeight      = -iconH; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // NanoSVG gives RGBA; GDI DIB uses BGRA, pre-multiplied alpha needed for AlphaBlend.
    // We convert and use AlphaBlend for smooth anti-aliasing.
    unsigned char* bgra = new unsigned char[iconW * iconH * 4];
    for (int i = 0; i < iconW * iconH; ++i) {
        unsigned char r2 = pixels[i*4+0];
        unsigned char g  = pixels[i*4+1];
        unsigned char b  = pixels[i*4+2];
        unsigned char a  = pixels[i*4+3];
        // pre-multiply
        bgra[i*4+0] = (unsigned char)((b * a) / 255);
        bgra[i*4+1] = (unsigned char)((g * a) / 255);
        bgra[i*4+2] = (unsigned char)((r2* a) / 255);
        bgra[i*4+3] = a;
    }

    HDC memdc = CreateCompatibleDC(hdc);
    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(memdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (hbm && bits) {
        memcpy(bits, bgra, iconW * iconH * 4);
        HBITMAP old = (HBITMAP)SelectObject(memdc, hbm);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        AlphaBlend(hdc, dx, dy, iconW, iconH, memdc, 0, 0, iconW, iconH, bf);
        SelectObject(memdc, old);
        DeleteObject(hbm);
    }
    DeleteDC(memdc);
    delete[] bgra;
}

// Convert an RGB COLORREF to a 6-hex-digit value for embedding in SVG fill/stroke.
// NanoSVG parses colours as RRGGBB hex.
static unsigned int colorref_to_svg_hex(COLORREF c) {
    // COLORREF is 0x00BBGGRR, we need RRGGBB
    unsigned int r = GetRValue(c);
    unsigned int g = GetGValue(c);
    unsigned int b = GetBValue(c);
    return (r << 16) | (g << 8) | b;
}

// Render an SVG string at a specific pixel size, centred inside rect r.
// iconPixelSize < ICON_SIZE effectively adds inset padding.
static void draw_svg_icon(HDC hdc, const RECT& r, const char* svgFmt, COLORREF color,
                          int iconPixelSize = ICON_SIZE - 2*ICON_PAD) {
    char buf[2048];
    unsigned int hex = colorref_to_svg_hex(color);
    snprintf(buf, sizeof(buf), svgFmt, hex, hex);

    NSVGimage* img = nsvgParse(buf, "px", 96.0f);
    if (!img) return;

    int iconW = iconPixelSize, iconH = iconPixelSize;
    // Scale so the 16-unit SVG canvas maps to iconPixelSize pixels.
    float scale = (float)iconPixelSize / (float)ICON_SIZE;
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    unsigned char* pixels = new unsigned char[iconW * iconH * 4]();
    nsvgRasterize(rast, img, 0, 0, scale, pixels, iconW, iconH, iconW * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(img);

    blit_icon(hdc, r, pixels, iconW, iconH);
    delete[] pixels;
}

// Also handles the RESTORE icon which has four %06X placeholders.
static void draw_svg_restore_icon(HDC hdc, const RECT& r, COLORREF color,
                                  int iconPixelSize = ICON_SIZE - 2*ICON_PAD) {
    char buf[2048];
    unsigned int hex = colorref_to_svg_hex(color);
    snprintf(buf, sizeof(buf), SVG_RESTORE, hex, hex, hex, hex);

    NSVGimage* img = nsvgParse(buf, "px", 96.0f);
    if (!img) return;

    int iconW = iconPixelSize, iconH = iconPixelSize;
    float scale = (float)iconPixelSize / (float)ICON_SIZE;
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    unsigned char* pixels = new unsigned char[iconW * iconH * 4]();
    nsvgRasterize(rast, img, 0, 0, scale, pixels, iconW, iconH, iconW * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(img);

    blit_icon(hdc, r, pixels, iconW, iconH);
    delete[] pixels;
}

// -------------------------------------------------------------------------
// Paint a single window-control button
// -------------------------------------------------------------------------
static void paint_wc_button(HDC hdc, const RECT& r, int btnIdx, bool isClose,
                             bool isMaximized, HWND hwnd) {
    bool hovered = (g_hover   == btnIdx);
    bool pressed = (g_pressed == btnIdx);

    // --- background ---
    COLORREF bgC;
    if (pressed && hovered) {
        bgC = isClose ? close_press_bg() : btn_press_bg();
    } else if (hovered) {
        bgC = isClose ? close_hover_bg() : btn_hover_bg();
    } else {
        bgC = title_bg();  // transparent-to-title; just fill with title colour
    }

    // For non-hovered buttons, don't draw any background (blend with title bar).
    if (hovered || pressed) {
        // Antialiased circle via GDI+ — derive a square bounding box centred in the button.
        int cx = (r.left + r.right)  / 2;
        int cy = (r.top  + r.bottom) / 2;
        int bh = r.bottom - r.top;
        int bw = r.right  - r.left;
        int radius = ((bw < bh ? bw : bh) - 2) / 2;  // 1px margin so circle fills button slot

        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);

        BYTE ra = GetRValue(bgC), ga = GetGValue(bgC), ba = GetBValue(bgC);
        SolidBrush brush(Color(255, ra, ga, ba));
        g.FillEllipse(&brush,
            (INT)(cx - radius), (INT)(cy - radius),
            (INT)(radius * 2),  (INT)(radius * 2));
    }

    // --- icon colour ---
    COLORREF iconC = (isClose && hovered) ? close_icon_hover_color() : icon_color();

    // --- draw icon ---
    if (btnIdx == 0) {
        draw_svg_icon(hdc, r, SVG_MINIMIZE, iconC);
    } else if (btnIdx == 1) {
        if (isMaximized)
            draw_svg_restore_icon(hdc, r, iconC);
        else
            draw_svg_icon(hdc, r, SVG_MAXIMIZE, iconC);
    } else {
        draw_svg_icon(hdc, r, SVG_CLOSE, iconC);
    }
}

// -------------------------------------------------------------------------
// Compute button rects from client rect
// -------------------------------------------------------------------------
static void get_button_rects(const RECT& rc,
                              RECT& minR, RECT& maxR, RECT& closeR) {
    int y0 = (TITLE_H - BTN_H) / 2;
    closeR = { rc.right - BTN_W,         y0, rc.right,           y0 + BTN_H };
    maxR   = { rc.right - 2*BTN_W,       y0, rc.right - BTN_W,   y0 + BTN_H };
    minR   = { rc.right - 3*BTN_W,       y0, rc.right - 2*BTN_W, y0 + BTN_H };
}

// -------------------------------------------------------------------------
// WndProc
// -------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        // -----------------------------------------------------------------
        case WM_CREATE: {
            MARGINS margins = {0,0,0,0};
            DwmExtendFrameIntoClientArea(hwnd, &margins);
            BOOL corner = DWMWCP_ROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
            int ncrp = DWMNCRP_DISABLED;
            DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
            COLORREF none = DWM_COLOR_NONE;
            DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &none, sizeof(none));
            return 0;
        }

        // -----------------------------------------------------------------
        case WM_NCCALCSIZE:
            if (wp == TRUE) return 0;
            break;

        // -----------------------------------------------------------------
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);

            // Double-buffer
            HDC memdc = CreateCompatibleDC(hdc);
            HBITMAP membmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldbmp = (HBITMAP)SelectObject(memdc, membmp);

            // Background
            HBRUSH br = CreateSolidBrush(bg_color());
            FillRect(memdc, &rc, br); DeleteObject(br);

            // Title bar background
            RECT titleR = {0, 0, rc.right, TITLE_H};
            HBRUSH tbr = CreateSolidBrush(title_bg());
            FillRect(memdc, &titleR, tbr); DeleteObject(tbr);

            // Title text
            SetBkMode(memdc, TRANSPARENT);
            SetTextColor(memdc, fg_color());
            HFONT hf = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            HFONT oldf = (HFONT)SelectObject(memdc, hf);
            RECT tr = {12, 0, rc.right - 3*BTN_W, TITLE_H};
            DrawTextW(memdc, L"cpp-ui-toolkit", -1, &tr, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
            SelectObject(memdc, oldf); DeleteObject(hf);

            // Window control buttons
            RECT minR, maxR, closeR;
            get_button_rects(rc, minR, maxR, closeR);
            bool zoomed = IsZoomed(hwnd) != 0;
            paint_wc_button(memdc, minR,   0, false, false,   hwnd);
            paint_wc_button(memdc, maxR,   1, false, zoomed,  hwnd);
            paint_wc_button(memdc, closeR, 2, true,  false,   hwnd);

            // Body text
            RECT body = {12, TITLE_H + 12, rc.right - 12, rc.bottom - 12};
            HFONT bf = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            HFONT oldbf = (HFONT)SelectObject(memdc, bf);
            SetTextColor(memdc, fg_color());
            DrawTextW(memdc, L"Custom frame: drag title bar, resize edges, min/max/close. Press T to toggle theme.",
                      -1, &body, DT_LEFT|DT_TOP|DT_WORDBREAK);
            SelectObject(memdc, oldbf); DeleteObject(bf);

            // Flush
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memdc, 0, 0, SRCCOPY);
            SelectObject(memdc, oldbmp);
            DeleteObject(membmp);
            DeleteDC(memdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        // -----------------------------------------------------------------
        // Mouse move: update hover state and track mouse leave
        case WM_MOUSEMOVE: {
            POINT pt = { LOWORD(lp), HIWORD(lp) };
            RECT rc; GetClientRect(hwnd, &rc);
            RECT minR, maxR, closeR;
            get_button_rects(rc, minR, maxR, closeR);

            int newHover = -1;
            if (PtInRect(&minR,   pt)) newHover = 0;
            else if (PtInRect(&maxR,   pt)) newHover = 1;
            else if (PtInRect(&closeR, pt)) newHover = 2;

            if (newHover != g_hover) {
                g_hover = newHover;
                InvalidateRect(hwnd, nullptr, FALSE);
            }

            // Start tracking so we get WM_MOUSELEAVE when cursor leaves
            if (!g_tracking) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                g_tracking = true;
            }
            return 0;
        }

        // -----------------------------------------------------------------
        case WM_MOUSELEAVE: {
            g_tracking = false;
            if (g_hover != -1 || g_pressed != -1) {
                g_hover   = -1;
                g_pressed = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        // -----------------------------------------------------------------
        case WM_LBUTTONDOWN: {
            POINT pt = { LOWORD(lp), HIWORD(lp) };
            RECT rc; GetClientRect(hwnd, &rc);
            RECT minR, maxR, closeR;
            get_button_rects(rc, minR, maxR, closeR);

            if (PtInRect(&minR, pt))   { g_pressed = 0; SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
            if (PtInRect(&maxR, pt))   { g_pressed = 1; SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
            if (PtInRect(&closeR, pt)) { g_pressed = 2; SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
            break;
        }

        // -----------------------------------------------------------------
        case WM_LBUTTONUP: {
            ReleaseCapture();
            int wasPressed = g_pressed;
            g_pressed = -1;

            // Update hover under cursor now
            POINT pt = { LOWORD(lp), HIWORD(lp) };
            RECT rc; GetClientRect(hwnd, &rc);
            RECT minR, maxR, closeR;
            get_button_rects(rc, minR, maxR, closeR);
            int newHover = -1;
            if      (PtInRect(&minR,   pt)) newHover = 0;
            else if (PtInRect(&maxR,   pt)) newHover = 1;
            else if (PtInRect(&closeR, pt)) newHover = 2;
            g_hover = newHover;
            InvalidateRect(hwnd, nullptr, FALSE);

            // Fire action only if released over the same button that was pressed
            if (wasPressed == 0 && newHover == 0) { ShowWindow(hwnd, SW_MINIMIZE); return 0; }
            if (wasPressed == 1 && newHover == 1) { ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE); return 0; }
            if (wasPressed == 2 && newHover == 2) { PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0; }
            return 0;
        }

        // -----------------------------------------------------------------
        case WM_NCHITTEST: {
            POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
            ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);

            // Resize borders (take priority over title bar)
            bool atLeft   = pt.x < BORDER;
            bool atRight  = pt.x >= rc.right - BORDER;
            bool atTop    = pt.y < BORDER;
            bool atBottom = pt.y >= rc.bottom - BORDER;
            if (atTop    && atLeft)  return HTTOPLEFT;
            if (atTop    && atRight) return HTTOPRIGHT;
            if (atBottom && atLeft)  return HTBOTTOMLEFT;
            if (atBottom && atRight) return HTBOTTOMRIGHT;
            if (atLeft)   return HTLEFT;
            if (atRight)  return HTRIGHT;
            if (atTop)    return HTTOP;
            if (atBottom) return HTBOTTOM;

            // Title bar region (including buttons)
            if (pt.y < TITLE_H) {
                // Return HTCLIENT for buttons so WM_LBUTTONDOWN/UP/MOUSEMOVE reach us
                RECT minR, maxR, closeR;
                get_button_rects(rc, minR, maxR, closeR);
                if (PtInRect(&closeR, pt)) return HTCLIENT;
                if (PtInRect(&maxR,   pt)) return HTCLIENT;
                if (PtInRect(&minR,   pt)) return HTCLIENT;
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        // -----------------------------------------------------------------
        case WM_KEYDOWN:
            if (wp == 'T') {
                g_theme = (g_theme == Theme::Light) ? Theme::Dark : Theme::Light;
                InvalidateRect(hwnd, nullptr, TRUE);
            } else if (wp == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            return 0;

        // -----------------------------------------------------------------
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// -------------------------------------------------------------------------
// WinMain
// -------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    // Initialise GDI+ once for the lifetime of the process.
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
    const wchar_t* cls = L"CppUIToolkitM0";
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = cls;
    wc.style         = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    int w = 900, h = 600;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        cls, L"cpp-ui-toolkit",
        WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        x, y, w, h,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    GdiplusShutdown(g_gdiplusToken);
    return (int)msg.wParam;
}
