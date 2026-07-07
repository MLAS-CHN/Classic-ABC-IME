// proto_ui.cpp - Candidate window UI implementation.
#include "proto_ui.h"
#include "proto_engine.h"
#include "proto_core.h"
#include "../util.h"
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
static const ClassicABC::UI::NinePatchSkin* g_skin = nullptr;

// Settings bar
static const wchar_t kSettingsClass[] = L"ProtoSettingsWnd";
static HWND  g_settingsWnd = nullptr;
static bool  g_settingsClass = false;
static int   g_settingsX = -1, g_settingsY = -1;
static const int kSettingsW = 127, kSettingsH = 26;
static const ClassicABC::UI::NinePatchSkin* g_settingsSkin = nullptr;
static const ClassicABC::UI::NinePatchSkin* g_btnSkin = nullptr;  // button.png
static Gdiplus::Bitmap* g_btnIcons[5] = {};  // per-button PNG icons
static Gdiplus::Bitmap* g_modeIcons[3] = {}; // 0=capital 1=english 2=pinyin (for button 1)
static Gdiplus::Bitmap* g_lockIcon = nullptr; // button 0 locked state icon (ABC_ICON_GRAY)
static Gdiplus::Bitmap* g_signEnIcon = nullptr; // button 3 English/CapsLock variant (sign_en.png)

// Candidate nav bar icons (0=first 1=last 2=next 3=prev)
static Gdiplus::Bitmap* g_navIcons[4] = {};
static RECT g_navBtnRects[4];
static const int kNavBtnSize = 13;

// --- test ---
static const wchar_t kCandClass[] = L"ProtoCandListWnd";
static HWND  g_candWnd;
static bool  g_candClassRegistered;
static int   g_candW = 120, g_candH = 200;

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

// --- candidate nav bar ---
static void ComputeNavBtnRects(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int count = (int)ClassicABC::GetCandidateCount();
    int navY = 6 + count * g_fh + 1;
    int winW = rc.right;
    int margin = 4;
    g_navBtnRects[0] = { margin, navY, margin + kNavBtnSize, navY + kNavBtnSize };
    g_navBtnRects[1] = { margin + kNavBtnSize + 1, navY, margin + kNavBtnSize * 2 + 1, navY + kNavBtnSize };
    g_navBtnRects[2] = { winW - margin - kNavBtnSize, navY, winW - margin, navY + kNavBtnSize };
    g_navBtnRects[3] = { winW - margin - kNavBtnSize * 2 - 1, navY, winW - margin - kNavBtnSize - 1, navY + kNavBtnSize };
}

// --- candidate wndproc ---
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc);

        if (g_skin) {
            Draw9Patch(dc, rc);
        } else {
            HBRUSH bg = CreateSolidBrush(RGB(0xFF, 0xFB, 0xF0)); FillRect(dc, &rc, bg); DeleteObject(bg);
        }

        const auto& s = ClassicABC::Engine::CompStr();
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

bool ClassicABC::UI::Init(HINSTANCE hInst, int width, int height) {
    g_inst = hInst; g_cw = width; g_ch = height;

    if (g_gdiToken == 0) {
        Gdiplus::GdiplusStartupInput si;
        Gdiplus::GdiplusStartup(&g_gdiToken, &si, nullptr);
    }
    write_log("UI: Init() hInst=" + std::to_string((uintptr_t)hInst) + " size=" + std::to_string(width) + "x" + std::to_string(height), LOG_DEBUG);
    return true;
}

void ClassicABC::UI::Shutdown() {
    if (g_wnd) { DestroyWindow(g_wnd); g_wnd = nullptr; }
    if (g_candWnd) { DestroyWindow(g_candWnd); g_candWnd = nullptr; }
    if (g_settingsWnd) { DestroyWindow(g_settingsWnd); g_settingsWnd = nullptr; }
    if (g_font) { DeleteObject(g_font); g_font = nullptr; }
    g_skin = nullptr; g_settingsSkin = nullptr; g_btnSkin = nullptr;
    for (int i = 0; i < 5; ++i) {
        if (g_btnIcons[i]) { delete g_btnIcons[i]; g_btnIcons[i] = nullptr; }
    }
    for (int i = 0; i < 3; ++i) {
        if (g_modeIcons[i]) { delete g_modeIcons[i]; g_modeIcons[i] = nullptr; }
    }
    if (g_lockIcon) { delete g_lockIcon; g_lockIcon = nullptr; }
    if (g_signEnIcon) { delete g_signEnIcon; g_signEnIcon = nullptr; }
    for (int i = 0; i < 4; ++i) {
        if (g_navIcons[i]) { delete g_navIcons[i]; g_navIcons[i] = nullptr; }
    }
    g_wclass = false;
    g_candClassRegistered = false;
    g_settingsClass = false;
    if (g_gdiToken) { Gdiplus::GdiplusShutdown(g_gdiToken); g_gdiToken = 0; }
}

void ClassicABC::UI::Show(bool visible) {
    if (g_wnd) ShowWindow(g_wnd, visible ? SW_SHOW : SW_HIDE);
}

void ClassicABC::UI::Update() {
    // Read composition from engine
    const auto& buf = ClassicABC::Engine::CompStr();
    if (buf.empty()) { if (g_wnd) ShowWindow(g_wnd, SW_HIDE); return; }

    if (!g_wnd) {
        write_log("UI: Update creating candidate input window", LOG_DEBUG);
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

// --- candidate wndproc ---
static LRESULT CALLBACK candWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_SETCURSOR) {
        if (LOWORD(l) == HTCLIENT) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            ComputeNavBtnRects(hwnd);
            for (int i = 0; i < 4; ++i) {
                if (PtInRect(&g_navBtnRects[i], pt)) {
                    SetCursor(LoadCursor(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
        }
        return DefWindowProc(hwnd, msg, w, l);
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        if (g_skin) { Draw9Patch(dc, rc); }
        else {
            HBRUSH bg = CreateSolidBrush(RGB(0xFF, 0xFB, 0xF0)); FillRect(dc, &rc, bg); DeleteObject(bg);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
            HPEN op = (HPEN)SelectObject(dc, pen);
            HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); SelectObject(dc, nb);
            Rectangle(dc, 0, 0, rc.right, rc.bottom);
            SelectObject(dc, op); DeleteObject(pen);
        }
        HFONT old = (HFONT)SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        COLORREF candColor = ClassicABC::IsDelMode() ? RGB(255, 0, 0) : RGB(128, 0, 128);
        size_t count = ClassicABC::GetCandidateCount();
        for (size_t i = 0; i < count; ++i) {
            std::wstring text = ClassicABC::GetCandidateText(i);
            if (text.empty()) continue;
            int n = (int)i + 1;
            if (n == 10) n = 0;
            wchar_t num[4]; wsprintfW(num, L"%d:", n);
            SetTextColor(dc, candColor);
            TextOutW(dc, 4, 6 + (int)i * g_fh, num, (int)wcslen(num));
            SetTextColor(dc, candColor);
            TextOutW(dc, 4 + g_fw * 2, 6 + (int)i * g_fh, text.c_str(), (int)text.size());
        }
        // Nav bar
        ComputeNavBtnRects(hwnd);
        {
            Gdiplus::Graphics gfx(dc);
            for (int i = 0; i < 4; ++i) {
                if (g_navIcons[i]) {
                    gfx.DrawImage(g_navIcons[i], g_navBtnRects[i].left, g_navBtnRects[i].top,
                                  kNavBtnSize, kNavBtnSize);
                }
            }
        }
        size_t totalPages = ClassicABC::GetTotalPages();
        if (totalPages > 0) {
            size_t curPage = ClassicABC::GetCandidatePage();
            wchar_t pageText[16];
            wsprintfW(pageText, L"%d/%d", (int)(curPage + 1), (int)totalPages);
            SetTextColor(dc, RGB(0, 0, 255));
            int textW = (int)wcslen(pageText) * g_fw;
            int navY = g_navBtnRects[0].top;
            int centerX = (rc.right - textW) / 2;
            TextOutW(dc, centerX, navY, pageText, (int)wcslen(pageText));
        }
        SelectObject(dc, old);
        EndPaint(hwnd, &ps); return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        POINT pt = { LOWORD(l), HIWORD(l) };
        ComputeNavBtnRects(hwnd);
        for (int i = 0; i < 4; ++i) {
            if (PtInRect(&g_navBtnRects[i], pt)) {
                switch (i) {
                    case 0: ClassicABC::GoFirstPage(); break;
                    case 1: ClassicABC::GoLastPage(); break;
                    case 2: ClassicABC::GoNextPage(); break;
                    case 3: ClassicABC::GoPrevPage(); break;
                }
                break;
            }
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

void ClassicABC::UI::ShowCand(bool visible) {
    if (visible) {
        if (!g_candWnd) {
            write_log("UI: ShowCand creating candidate list window", LOG_DEBUG);
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
                0, 0, g_candW, g_candH,
                nullptr, nullptr, g_inst, nullptr);
        }
        ShowWindow(g_candWnd, SW_SHOWNOACTIVATE);
    } else {
        if (g_candWnd) ShowWindow(g_candWnd, SW_HIDE);
    }
}

void ClassicABC::UI::UpdateCand() {
    size_t count = ClassicABC::GetCandidateCount();
    if (count == 0) { ShowCand(false); return; }
    ShowCand(true);
    int h = 6 + (int)count * g_fh + kNavBtnSize + 6;
    if (h < 30) h = 30;

    // Position: default to the RIGHT of the input window.
    // If no room on right, go LEFT. Then, try below the caret; if no room, go above.
    POINT cp = caret_pos();
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);

    // Horizontal: right of input window, fallback to left (use actual pinyin bar rect)
    RECT pr; GetWindowRect(g_wnd, &pr);
    int inputL = pr.left;
    int inputR = pr.right;
    int x = inputR + 4;                     // right of input
    if (x + g_candW > sw) x = inputL - g_candW - 4; // fallback: left of input

    // Vertical: try below caret; if candidate doesn't fit, go above (and move pinyin bar up too)
    int belowY = cp.y + g_fh + 4;
    int y = belowY;
    if (y + h > sh) {
        y = cp.y - h - 4;
        int pinyinY = cp.y - g_ch - 4;
        if (pinyinY < 0) pinyinY = 0;
        RECT pr; GetWindowRect(g_wnd, &pr);
        SetWindowPos(g_wnd, HWND_TOPMOST, pr.left, pinyinY, 0, 0,
                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }

    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x + g_candW > sw) x = sw - g_candW;
    if (y + h > sh) y = sh - h;
    SetWindowPos(g_candWnd, HWND_TOPMOST, x, y, g_candW, h,
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

static void DrawPatchAt(HDC dc, int x, int y, int w, int h, const ClassicABC::UI::NinePatchSkin* sk) {
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
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_SETCURSOR) {
        if (LOWORD(l) == HTCLIENT) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            int btn = HitTestBtn(pt);
            // Button 3 (sign) is not clickable — use normal arrow cursor
            if (btn == 3)
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            else if (btn >= 0)
                SetCursor(LoadCursor(nullptr, IDC_HAND));
            else
                SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
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
                const RECT& br = g_btnRects[i];
                Gdiplus::Bitmap* icon = g_btnIcons[i];
                // Button 0: lock icon when locked
                if (i == 0 && g_lockIcon && ClassicABC::IsLocked()) {
                    icon = g_lockIcon;
                }
                // Button 1: dynamic mode icon (capital/english/pinyin)
                if (i == 1) {
                    int mode = 2; // default pinyin
                    if (GetKeyState(VK_CAPITAL) & 0x0001)
                        mode = 0; // capital
                    else if (!ClassicABC::Engine::IsChineseMode())
                        mode = 1; // english
                    icon = g_modeIcons[mode];
                }
                // Button 3: sign_en icon when not Chinese mode
                if (i == 3 && g_signEnIcon) {
                    bool notChinese = (GetKeyState(VK_CAPITAL) & 0x0001) ||
                                      !ClassicABC::Engine::IsChineseMode();
                    if (notChinese) icon = g_signEnIcon;
                }
                if (!icon) continue;
                gfx.DrawImage(icon, br.left, br.top,
                              br.right - br.left, br.bottom - br.top);
            }
        }
        EndPaint(hwnd, &ps); return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        POINT pt = { LOWORD(l), HIWORD(l) };
        int btnIdx = HitTestBtn(pt);
        if (btnIdx >= 0) {
            if (btnIdx == 0) {
                ClassicABC::ToggleLock();
                InvalidateRect(hwnd, &g_btnRects[0], TRUE);
            }
            else if (btnIdx == 1) {
                ClassicABC::ToggleMode();
                InvalidateRect(hwnd, &g_btnRects[1], TRUE);
            }
            return 0;
        }
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

void ClassicABC::UI::ShowSettings(bool visible) {
    if (visible) {
        if (!g_settingsWnd) {
            write_log("UI: ShowSettings creating settings window", LOG_DEBUG);
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
            write_log("UI: ShowSettings created hwnd=" + std::to_string((uintptr_t)g_settingsWnd) + " at " + std::to_string(g_settingsX) + "," + std::to_string(g_settingsY), LOG_DEBUG);
            InitBtnRects();
        }
        write_log("UI: ShowSettings showing hwnd=" + std::to_string((uintptr_t)g_settingsWnd) + " IsVisible=" + std::to_string(IsWindowVisible(g_settingsWnd)), LOG_DEBUG);
        ShowWindow(g_settingsWnd, SW_SHOWNOACTIVATE);
    } else {
        if (g_settingsWnd) { write_log("UI: ShowSettings hiding hwnd=" + std::to_string((uintptr_t)g_settingsWnd), LOG_DEBUG); ShowWindow(g_settingsWnd, SW_HIDE); }
    }
}

void ClassicABC::UI::SetSettingsSkin(const ClassicABC::UI::NinePatchSkin* skin) {
    g_settingsSkin = skin;
}

void ClassicABC::UI::SetBtnSkin(const ClassicABC::UI::NinePatchSkin* skin) {
    g_btnSkin = skin;
}

// --- skin ---

bool ClassicABC::UI::LoadSkin(const wchar_t* path, ClassicABC::UI::NinePatchSkin& skin,
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

void ClassicABC::UI::FreeSkin(ClassicABC::UI::NinePatchSkin& skin) {
    if (skin.hBmp) { DeleteObject(skin.hBmp); skin.hBmp = nullptr; }
}

void ClassicABC::UI::SetSkin(const ClassicABC::UI::NinePatchSkin* skin) {
    g_skin = skin;
}

bool ClassicABC::UI::SetBtnIcon(int idx, const wchar_t* path) {
    if (idx < 0 || idx >= 5) return false;
    if (g_btnIcons[idx]) { delete g_btnIcons[idx]; g_btnIcons[idx] = nullptr; }
    g_btnIcons[idx] = new Gdiplus::Bitmap(path);
    return g_btnIcons[idx]->GetLastStatus() == Gdiplus::Ok;
}

bool ClassicABC::UI::SetModeIcon(int idx, const wchar_t* path) {
    if (idx < 0 || idx >= 3) return false;
    if (g_modeIcons[idx]) { delete g_modeIcons[idx]; g_modeIcons[idx] = nullptr; }
    g_modeIcons[idx] = new Gdiplus::Bitmap(path);
    return g_modeIcons[idx]->GetLastStatus() == Gdiplus::Ok;
}

bool ClassicABC::UI::SetLockIcon(const wchar_t* path) {
    if (g_lockIcon) { delete g_lockIcon; g_lockIcon = nullptr; }
    g_lockIcon = new Gdiplus::Bitmap(path);
    return g_lockIcon->GetLastStatus() == Gdiplus::Ok;
}

bool ClassicABC::UI::SetSignEnIcon(const wchar_t* path) {
    if (g_signEnIcon) { delete g_signEnIcon; g_signEnIcon = nullptr; }
    g_signEnIcon = new Gdiplus::Bitmap(path);
    return g_signEnIcon->GetLastStatus() == Gdiplus::Ok;
}

bool ClassicABC::UI::SetNavIcon(int idx, const wchar_t* path) {
    if (idx < 0 || idx >= 4) return false;
    if (g_navIcons[idx]) { delete g_navIcons[idx]; g_navIcons[idx] = nullptr; }
    g_navIcons[idx] = new Gdiplus::Bitmap(path);
    return g_navIcons[idx]->GetLastStatus() == Gdiplus::Ok;
}

void ClassicABC::UI::RefreshSettings() {
    if (g_settingsWnd)
        InvalidateRect(g_settingsWnd, nullptr, TRUE);
}
