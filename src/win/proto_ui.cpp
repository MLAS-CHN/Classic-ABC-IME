// proto_ui.cpp - Candidate window UI implementation.
#include "proto_ui.h"
#include "proto_engine.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// --- state ---
static HINSTANCE g_inst = nullptr;
static const wchar_t kWndClass[] = L"ProtoCandWnd";
static HWND  g_wnd    = nullptr;
static bool  g_wclass = false;
static int   g_cw = 163, g_ch = 26;   // fixed window size

// Font
static HFONT g_font = nullptr;
static int   g_fh = 18, g_fw = 9;

// GDI+
static ULONG_PTR g_gdiToken = 0;

// Skin
static const ProtoIME::UI::NinePatchSkin* g_skin = nullptr;

// Settings bar
static const wchar_t kSettingsClass[] = L"ProtoSettingsWnd";
static HWND  g_settingsWnd = nullptr;
static bool  g_settingsClass = false;
static int   g_settingsX = -1, g_settingsY = -1;
static const int kSettingsW = 127, kSettingsH = 26;
static const ProtoIME::UI::NinePatchSkin* g_settingsSkin = nullptr;
static const ProtoIME::UI::NinePatchSkin* g_btnSkin = nullptr;  // button.png
static Gdiplus::Bitmap* g_btnIcons[5] = {};  // per-button PNG icons

// --- test ---
// Candidate list (test data, 10 rows of hardcoded candidates).
// In production this would be populated by the IME engine's lookup.
static const wchar_t* kTestCands[] = {
    L"1:\u5927", L"2:\u6253", L"3:\u8FBE", L"4:\u642D", L"5:\u55D2",
    L"6:\u7B54", L"7:\u6422", L"8:\u7422", L"9:\u6422", L"0:\u6424"
};
static const wchar_t kCandClass[] = L"ProtoCandListWnd";
static HWND  g_candWnd   = nullptr;
static bool  g_candClassRegistered = false;
static const int kCandW = 120, kCandH = 169;

// --- font ---
static void init_font() {
    if (g_font) return;
    g_font = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"System");
    HDC dc = GetDC(nullptr);
    if (dc && g_font) { HFONT old = (HFONT)SelectObject(dc, g_font); TEXTMETRICW tm = {}; GetTextMetricsW(dc, &tm);
                        g_fh = tm.tmHeight + tm.tmExternalLeading; g_fw = tm.tmAveCharWidth; SelectObject(dc, old); }
    if (dc) ReleaseDC(nullptr, dc);
}

// --- caret position ---
static POINT caret_pos() {
    POINT pt = {}; HWND fg = GetForegroundWindow();
    if (fg) { DWORD tid = GetWindowThreadProcessId(fg, nullptr); GUITHREADINFO gui = { sizeof(GUITHREADINFO) };
              if (GetGUIThreadInfo(tid, &gui) && gui.hwndCaret) { pt.x = gui.rcCaret.left; pt.y = gui.rcCaret.top; ClientToScreen(gui.hwndCaret, &pt); return pt; } }
    GetCursorPos(&pt); return pt;
}

// --- 9-patch ---
static void Draw9Patch(HDC dc, const RECT& rc) {
    if (!g_skin || !g_skin->hBmp) return;
    const auto& s = *g_skin;
    HDC memDC = CreateCompatibleDC(dc);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, s.hBmp);

    int L = s.marginL, T = s.marginT, R = s.marginR, B = s.marginB;
    int CW = s.srcW - L - R, CH = s.srcH - T - B;
    int dw = rc.right - rc.left, dh = rc.bottom - rc.top;

    BitBlt(dc, rc.left, rc.top, L, T, memDC, 0, 0, SRCCOPY);
    BitBlt(dc, rc.right - R, rc.top, R, T, memDC, s.srcW - R, 0, SRCCOPY);
    BitBlt(dc, rc.left, rc.bottom - B, L, B, memDC, 0, s.srcH - B, SRCCOPY);
    BitBlt(dc, rc.right - R, rc.bottom - B, R, B, memDC, s.srcW - R, s.srcH - B, SRCCOPY);

    int mw = dw - L - R; if (mw < 0) mw = 0;
    int mh = dh - T - B; if (mh < 0) mh = 0;
    StretchBlt(dc, rc.left + L, rc.top, mw, T, memDC, L, 0, CW, T, SRCCOPY);
    StretchBlt(dc, rc.left + L, rc.bottom - B, mw, B, memDC, L, s.srcH - B, CW, B, SRCCOPY);
    StretchBlt(dc, rc.left, rc.top + T, L, mh, memDC, 0, T, L, CH, SRCCOPY);
    StretchBlt(dc, rc.right - R, rc.top + T, R, mh, memDC, s.srcW - R, T, R, CH, SRCCOPY);
    StretchBlt(dc, rc.left + L, rc.top + T, mw, mh, memDC, L, T, CW, CH, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
}

// --- wndproc ---
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc);

        if (g_skin) {
            Draw9Patch(dc, rc);
        } else {
            HBRUSH bg = CreateSolidBrush(RGB(0xFF, 0xFB, 0xF0)); FillRect(dc, &rc, bg); DeleteObject(bg);
        }

        const auto& s = ProtoIME::Engine::CompStr();
        HFONT old = (HFONT)SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0, 0, 0));
        int tx = g_skin ? g_skin->marginL + 2 : 6;
        int ty = g_skin ? g_skin->marginT : 2;
        TextOutW(dc, tx, ty, s.c_str(), (int)s.size());

        if (!g_skin) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180)); HPEN op = (HPEN)SelectObject(dc, pen);
            HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); SelectObject(dc, nb);
            Rectangle(dc, 0, 0, rc.right, rc.bottom); SelectObject(dc, op); DeleteObject(pen);
        }

        SelectObject(dc, old); EndPaint(hwnd, &ps); return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

// --- public ---

bool ProtoIME::UI::Init(HINSTANCE hInst, int width, int height) {
    g_inst = hInst; g_cw = width; g_ch = height;

    Gdiplus::GdiplusStartupInput si;
    Gdiplus::GdiplusStartup(&g_gdiToken, &si, nullptr);
    return true;
}

void ProtoIME::UI::Shutdown() {
    if (g_wnd) { DestroyWindow(g_wnd); g_wnd = nullptr; }
    if (g_candWnd) { DestroyWindow(g_candWnd); g_candWnd = nullptr; }
    if (g_settingsWnd) { DestroyWindow(g_settingsWnd); g_settingsWnd = nullptr; }
    if (g_font) { DeleteObject(g_font); g_font = nullptr; }
    g_skin = nullptr; g_settingsSkin = nullptr; g_btnSkin = nullptr;
    for (int i = 0; i < 5; ++i) {
        if (g_btnIcons[i]) { delete g_btnIcons[i]; g_btnIcons[i] = nullptr; }
    }
    if (g_gdiToken) { Gdiplus::GdiplusShutdown(g_gdiToken); g_gdiToken = 0; }
}

void ProtoIME::UI::Show(bool visible) {
    if (g_wnd) ShowWindow(g_wnd, visible ? SW_SHOW : SW_HIDE);
}

void ProtoIME::UI::Update() {
    // Read composition from engine
    const auto& buf = ProtoIME::Engine::CompStr();
    if (buf.empty()) { if (g_wnd) ShowWindow(g_wnd, SW_HIDE); return; }

    if (!g_wnd) {
        init_font();
        if (!g_wclass) {
            WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) }; wc.lpfnWndProc = wndproc; wc.hInstance = g_inst;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = kWndClass; RegisterClassExW(&wc); g_wclass = true;
        }
        g_wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kWndClass, L"",
                                 WS_POPUP, 0, 0, g_cw, g_ch, nullptr, nullptr, g_inst, nullptr);
    }

    POINT cp = caret_pos(); int x = cp.x, y = cp.y + g_fh + 4;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    if (x + g_cw > sw) x = sw - g_cw; if (y + g_ch > sh) y = cp.y - g_ch - 4;
    if (x < 0) x = 0; if (y < 0) y = 0;
    SetWindowPos(g_wnd, HWND_TOPMOST, x, y, g_cw, g_ch, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_wnd, nullptr, TRUE);
}

// --- test ---
// Candidate list wndproc. Test-only: draws 10 hardcoded candidates in purple.
// Uses the same 9-patch shadow skin as the input window.
static LRESULT CALLBACK candWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        if (g_skin) {
            Draw9Patch(dc, rc);
        } else {
            HBRUSH bg = CreateSolidBrush(RGB(0xFF, 0xFB, 0xF0));
            FillRect(dc, &rc, bg); DeleteObject(bg);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
            HPEN op = (HPEN)SelectObject(dc, pen);
            HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); SelectObject(dc, nb);
            Rectangle(dc, 0, 0, rc.right, rc.bottom);
            SelectObject(dc, op); DeleteObject(pen);
        }

        HFONT old = (HFONT)SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(128, 0, 128));  // purple
        for (int i = 0; i < 10; ++i)
            TextOutW(dc, 6, 2 + i * (g_fh + 0), kTestCands[i], (int)wcslen(kTestCands[i]));
        SelectObject(dc, old);

        EndPaint(hwnd, &ps); return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

void ProtoIME::UI::ShowCand(bool visible) {
    if (visible) {
        if (!g_candWnd) {
            if (!g_candClassRegistered) {
                WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
                wc.lpfnWndProc = candWndProc; wc.hInstance = g_inst;
                wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
                wc.lpszClassName = kCandClass; RegisterClassExW(&wc);
                g_candClassRegistered = true;
            }
            g_candWnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                kCandClass, L"", WS_POPUP,
                0, 0, kCandW, kCandH,
                nullptr, nullptr, g_inst, nullptr);
        }
        ShowWindow(g_candWnd, SW_SHOWNOACTIVATE);
    } else {
        if (g_candWnd) ShowWindow(g_candWnd, SW_HIDE);
    }
}

void ProtoIME::UI::UpdateCand() {
    const auto& buf = ProtoIME::Engine::CompStr();
    // Test: show candidate list when composition has more than 1 character.
    // In production this would be driven by the IME engine's lookup logic.
    if (buf.size() <= 1) {
        ShowCand(false);
        return;
    }
    ShowCand(true);

    // Position: default to the RIGHT of the input window.
    // If no room on right, go LEFT. Then, try below the caret; if no room, go above.
    POINT cp = caret_pos();
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);

    // Horizontal: right of input window (same Y as input), fallback to left
    int inputL = cp.x;
    int inputR = cp.x + g_cw;
    int x = inputR + 4;                     // right of input
    if (x + kCandW > sw) x = inputL - kCandW - 4; // fallback: left of input

    // Vertical: below caret then upward if needed
    int inputY = cp.y + g_fh + 4;
    int y = inputY;                         // below caret
    if (y + kCandH > sh) y = cp.y - kCandH - 4;   // fallback: above caret

    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x + kCandW > sw) x = sw - kCandW;
    if (y + kCandH > sh) y = sh - kCandH;
    SetWindowPos(g_candWnd, HWND_TOPMOST, x, y, kCandW, kCandH,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_candWnd, nullptr, TRUE);
}

// --- settings bar wndproc ---
static void DrawSettings(HDC dc, const RECT& rc) {
    if (!g_settingsSkin || !g_settingsSkin->hBmp) {
        HBRUSH bg = CreateSolidBrush(RGB(0xF0, 0xF0, 0xF0));
        FillRect(dc, &rc, bg); DeleteObject(bg);
        return;
    }
    const auto& s = *g_settingsSkin;
    HDC memDC = CreateCompatibleDC(dc);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, s.hBmp);
    int L = s.marginL, T = s.marginT, R = s.marginR, B = s.marginB;
    int CW = s.srcW - L - R, CH = s.srcH - T - B;
    int dw = rc.right - rc.left, dh = rc.bottom - rc.top;
    BitBlt(dc, rc.left, rc.top, L, T, memDC, 0, 0, SRCCOPY);
    BitBlt(dc, rc.right - R, rc.top, R, T, memDC, s.srcW - R, 0, SRCCOPY);
    BitBlt(dc, rc.left, rc.bottom - B, L, B, memDC, 0, s.srcH - B, SRCCOPY);
    BitBlt(dc, rc.right - R, rc.bottom - B, R, B, memDC, s.srcW - R, s.srcH - B, SRCCOPY);
    int mw = dw - L - R; if (mw < 0) mw = 0;
    int mh = dh - T - B; if (mh < 0) mh = 0;
    StretchBlt(dc, rc.left + L, rc.top, mw, T, memDC, L, 0, CW, T, SRCCOPY);
    StretchBlt(dc, rc.left + L, rc.bottom - B, mw, B, memDC, L, s.srcH - B, CW, B, SRCCOPY);
    StretchBlt(dc, rc.left, rc.top + T, L, mh, memDC, 0, T, L, CH, SRCCOPY);
    StretchBlt(dc, rc.right - R, rc.top + T, R, mh, memDC, s.srcW - R, T, R, CH, SRCCOPY);
    StretchBlt(dc, rc.left + L, rc.top + T, mw, mh, memDC, L, T, CW, CH, SRCCOPY);
    SelectObject(memDC, oldBmp); DeleteDC(memDC);
}

// --- settings bar buttons (5 test buttons, common skin) ---
// 1:20x20  2:40x20  3:20x20  4:20x20  5:20x20  starting at (4,3)
static RECT g_btnRects[5];
static const int kBtnHeights[5] = { 20, 20, 20, 20, 20 };
static const int kBtnWidths[5]  = { 20, 40, 20, 20, 20 };

static void InitBtnRects() {
    int x = 4, y = 3;
    for (int i = 0; i < 5; ++i) {
        g_btnRects[i].left   = x;
        g_btnRects[i].top    = y;
        g_btnRects[i].right  = x + kBtnWidths[i];
        g_btnRects[i].bottom = y + kBtnHeights[i];
        x += kBtnWidths[i];
    }
}

static void DrawPatchAt(HDC dc, int x, int y, int w, int h, const ProtoIME::UI::NinePatchSkin* sk) {
    if (!sk || !sk->hBmp) return;
    const auto& s = *sk;
    HDC memDC = CreateCompatibleDC(dc);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, s.hBmp);
    int L = s.marginL, T = s.marginT, R = s.marginR, B = s.marginB;
    int CW = s.srcW - L - R, CH = s.srcH - T - B;
    int mw = w - L - R; if (mw < 0) mw = 0;
    int mh = h - T - B; if (mh < 0) mh = 0;
    // Corners (1:1)
    BitBlt(dc, x, y, L, T, memDC, 0, 0, SRCCOPY);
    BitBlt(dc, x + w - R, y, R, T, memDC, s.srcW - R, 0, SRCCOPY);
    BitBlt(dc, x, y + h - B, L, B, memDC, 0, s.srcH - B, SRCCOPY);
    BitBlt(dc, x + w - R, y + h - B, R, B, memDC, s.srcW - R, s.srcH - B, SRCCOPY);
    // Edges (stretched to fill)
    StretchBlt(dc, x + L, y, mw, T, memDC, L, 0, CW, T, SRCCOPY);
    StretchBlt(dc, x + L, y + h - B, mw, B, memDC, L, s.srcH - B, CW, B, SRCCOPY);
    StretchBlt(dc, x, y + T, L, mh, memDC, 0, T, L, CH, SRCCOPY);
    StretchBlt(dc, x + w - R, y + T, R, mh, memDC, s.srcW - R, T, R, CH, SRCCOPY);
    // Center (stretched both ways)
    StretchBlt(dc, x + L, y + T, mw, mh, memDC, L, T, CW, CH, SRCCOPY);
    SelectObject(memDC, oldBmp); DeleteDC(memDC);
}

static bool     g_dragging = false;
static POINT    g_dragBase = {};   // cursor pos at drag start
static POINT    g_dragOfs  = {};   // offset from window origin

static int HitTestBtn(POINT pt) {
    for (int i = 0; i < 5; ++i)
        if (PtInRect(&g_btnRects[i], pt)) return i;
    return -1;
}

static LRESULT CALLBACK settingsWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_SETCURSOR) {
        if (LOWORD(l) == HTCLIENT) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            SetCursor(LoadCursor(nullptr, HitTestBtn(pt) >= 0 ? IDC_HAND : IDC_SIZEALL));
            return TRUE;
        }
        return DefWindowProc(hwnd, msg, w, l);
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        DrawSettings(dc, rc);
        for (int i = 0; i < 5; ++i) {
            const RECT& br = g_btnRects[i];
            DrawPatchAt(dc, br.left, br.top,
                        br.right - br.left, br.bottom - br.top, g_btnSkin);
        }
        // Draw per-button icons on top of 9-patch backgrounds
        {
            Gdiplus::Graphics gfx(dc);
            for (int i = 0; i < 5; ++i) {
                if (!g_btnIcons[i]) continue;
                const RECT& br = g_btnRects[i];
                gfx.DrawImage(g_btnIcons[i], br.left, br.top,
                              br.right - br.left, br.bottom - br.top);
            }
        }
        EndPaint(hwnd, &ps); return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        POINT pt = { LOWORD(l), HIWORD(l) };
        // Button click (no drag) -- placeholder, no action yet
        if (HitTestBtn(pt) >= 0) return 0;
        // Start drag
        g_dragging = true;
        GetCursorPos(&g_dragBase);
        RECT rc; GetWindowRect(hwnd, &rc);
        g_dragOfs.x = g_dragBase.x - rc.left;
        g_dragOfs.y = g_dragBase.y - rc.top;
        SetCapture(hwnd);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && g_dragging) {
        POINT pt; GetCursorPos(&pt);
        int x = pt.x - g_dragOfs.x;
        int y = pt.y - g_dragOfs.y;
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }
    if (msg == WM_LBUTTONUP && g_dragging) {
        g_dragging = false;
        ReleaseCapture();
        RECT rc; GetWindowRect(hwnd, &rc);
        g_settingsX = rc.left; g_settingsY = rc.top;
        return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

void ProtoIME::UI::ShowSettings(bool visible) {
    if (visible) {
        if (!g_settingsWnd) {
        if (!g_settingsClass) {
            WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
                wc.lpfnWndProc = settingsWndProc; wc.hInstance = g_inst;
                wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
                wc.lpszClassName = kSettingsClass; RegisterClassExW(&wc);
                g_settingsClass = true;
            }
            // Default position: bottom-right of work area (above taskbar)
            if (g_settingsX < 0) {
                RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
                g_settingsX = wa.right - kSettingsW - 10;
                g_settingsY = wa.bottom - kSettingsH - 10;
            }
            g_settingsWnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                kSettingsClass, L"", WS_POPUP,
                g_settingsX, g_settingsY, kSettingsW, kSettingsH,
                nullptr, nullptr, g_inst, nullptr);
            InitBtnRects();
        }
        ShowWindow(g_settingsWnd, SW_SHOWNOACTIVATE);
    } else {
        if (g_settingsWnd) ShowWindow(g_settingsWnd, SW_HIDE);
    }
}

void ProtoIME::UI::SetSettingsSkin(const ProtoIME::UI::NinePatchSkin* skin) {
    g_settingsSkin = skin;
}

void ProtoIME::UI::SetBtnSkin(const ProtoIME::UI::NinePatchSkin* skin) {
    g_btnSkin = skin;
}

// --- skin ---

bool ProtoIME::UI::LoadSkin(const wchar_t* path, ProtoIME::UI::NinePatchSkin& skin,
                             int mL, int mT, int mR, int mB) {
    Gdiplus::Bitmap bmp(path);
    if (bmp.GetLastStatus() != Gdiplus::Ok) return false;
    HBITMAP hBmp = nullptr;
    bmp.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hBmp);
    if (!hBmp) return false;
    skin.hBmp = hBmp;
    skin.srcW = bmp.GetWidth();
    skin.srcH = bmp.GetHeight();
    skin.marginL = mL;
    skin.marginT = mT;
    skin.marginR = mR;
    skin.marginB = mB;
    return true;
}

void ProtoIME::UI::FreeSkin(ProtoIME::UI::NinePatchSkin& skin) {
    if (skin.hBmp) { DeleteObject(skin.hBmp); skin.hBmp = nullptr; }
}

void ProtoIME::UI::SetSkin(const ProtoIME::UI::NinePatchSkin* skin) {
    g_skin = skin;
}

bool ProtoIME::UI::SetBtnIcon(int idx, const wchar_t* path) {
    if (idx < 0 || idx >= 5) return false;
    if (g_btnIcons[idx]) { delete g_btnIcons[idx]; g_btnIcons[idx] = nullptr; }
    g_btnIcons[idx] = new Gdiplus::Bitmap(path);
    return g_btnIcons[idx]->GetLastStatus() == Gdiplus::Ok;
}
