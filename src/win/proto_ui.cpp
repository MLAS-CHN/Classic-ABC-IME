// proto_ui.cpp - Candidate window UI implementation.
#include "proto_ui.h"
#include "proto_core.h"
#include "../settings.h"
#include "../util.h"
#include <gdiplus.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#pragma comment(lib, "gdiplus.lib")
// 启用 ComCtl32 v6 视觉样式：标准控件（EDIT/BUTTON/CHECKBOX）使用现代主题外观
// 而非经典样式（凹陷输入框、主题按钮）。
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

struct BuiltinSkinSpec {
    const uint32_t* pixels;
    int width;
    int height;
    int marginL;
    int marginT;
    int marginR;
    int marginB;
};

static const uint32_t kButtonSkinPixels[] = {
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF000000,
    0xFFFFFFFF, 0xFFC0C0C0, 0xFFC0C0C0, 0xFF808080, 0xFF000000,
    0xFFFFFFFF, 0xFFC0C0C0, 0xFFC0C0C0, 0xFF808080, 0xFF000000,
    0xFFFFFFFF, 0xFF808080, 0xFF808080, 0xFF808080, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000
};

static const uint32_t kCommonSkinPixels[] = {
    0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFF000000,
    0xFFC0C0C0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF808080, 0xFF000000,
    0xFFC0C0C0, 0xFFFFFFFF, 0xFFC0C0C0, 0xFF808080, 0xFF000000,
    0xFFC0C0C0, 0xFF808080, 0xFF808080, 0xFF808080, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000
};

static const uint32_t kShadowSkinPixels[] = {
    0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFF000000,
    0xFFC0C0C0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF808080, 0xFF000000,
    0xFFC0C0C0, 0xFFFFFFFF, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFF808080, 0xFF000000,
    0xFFC0C0C0, 0xFFFFFFFF, 0xFFC0C0C0, 0xFF808080, 0xFF808080, 0xFFFFFFFF, 0xFFC0C0C0, 0xFF808080, 0xFF000000,
    0xFFC0C0C0, 0xFFFFFFFF, 0xFFC0C0C0, 0xFF808080, 0xFFC0C0C0, 0xFFFFFFFF, 0xFFC0C0C0, 0xFF808080, 0xFF000000,
    0xFFC0C0C0, 0xFFFFFFFF, 0xFFC0C0C0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFC0C0C0, 0xFF808080, 0xFF000000,
    0xFFC0C0C0, 0xFFFFFFFF, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFFC0C0C0, 0xFF808080, 0xFF000000,
    0xFFC0C0C0, 0xFF808080, 0xFF808080, 0xFF808080, 0xFF808080, 0xFF808080, 0xFF808080, 0xFF808080, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000
};

static HBITMAP create_bitmap_from_argb_pixels(const uint32_t* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0) return nullptr;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        return nullptr;
    }

    std::memcpy(bits, pixels, static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(uint32_t));
    return bmp;
}

static bool fill_skin_from_spec(ClassicABC::UI::NinePatchSkin& skin, const BuiltinSkinSpec& spec) {
    HBITMAP hBmp = create_bitmap_from_argb_pixels(spec.pixels, spec.width, spec.height);
    if (!hBmp) return false;
    skin.hBmp = hBmp;
    skin.srcW = spec.width;
    skin.srcH = spec.height;
    skin.marginL = spec.marginL;
    skin.marginT = spec.marginT;
    skin.marginR = spec.marginR;
    skin.marginB = spec.marginB;
    return true;
}

}  // namespace

// --- state ---
static HINSTANCE g_inst = nullptr;
static const wchar_t kWndClass[] = L"ProtoCandWnd";
static HWND  g_wnd    = nullptr;
static bool  g_wclass = false;
static int   g_cw = 163, g_ch = 26;   // fixed window size

// Font
static HFONT g_font = nullptr;
static int   g_fh = 18, g_fw = 9;      // 拼音框字体（固定 System 12px）及度量
static HFONT g_candFont = nullptr;     // 候选词主字体（可配置）
static int   g_candFh = 18, g_candFw = 9;
static HFONT g_fallbackFont = nullptr; // 候选词备选字体（可配置，独立字号）
static int   g_fallbackFh = 18, g_fallbackFw = 9;

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

// Settings dialog (right-click on settings bar)
static const wchar_t kSettingsDlgClass[] = L"ProtoSettingsDlgWnd";
static HWND g_settingsDlg = nullptr;
static bool g_settingsDlgClass = false;
static HFONT g_settingsDlgFont = nullptr;  // 对话框 UI 字体（窗口销毁时释放）
static const int kDlgW = 250, kDlgH = 316;  // 含系统标题栏
enum { kDlgEditSize = 101, kDlgCheckLog = 102, kDlgBtnSave = 103, kDlgComboLevel = 104, kDlgComboFont = 105, kDlgComboFontSize = 106, kDlgComboFallbackFont = 107, kDlgComboFallbackSize = 108 };

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
// 拼音框字体：固定 System 12px（不受设置影响）。
static HFONT create_pinyin_font() {
    return CreateFontW(
        12, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"System");
}

// 候选词字体：从设置读取字体名和字号。
static HFONT create_candidate_font() {
    std::string font_name = get_candidate_font();
    std::wstring wname(font_name.begin(), font_name.end());
    int size_px = get_candidate_font_size();
    return CreateFontW(
        size_px, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        wname.c_str());
}

static void apply_candidate_font() {
    if (g_candFont) { DeleteObject(g_candFont); g_candFont = nullptr; }
    g_candFont = create_candidate_font();
    HDC dc = GetDC(nullptr);
    if (dc && g_candFont) { HFONT old = (HFONT)SelectObject(dc, g_candFont); TEXTMETRICW tm = {}; GetTextMetricsW(dc, &tm);
                            g_candFh = tm.tmHeight + tm.tmExternalLeading; g_candFw = tm.tmAveCharWidth; SelectObject(dc, old); }
    if (dc) ReleaseDC(nullptr, dc);
}

// 备选字体：从设置读取字体名和字号（独立配置）。
static HFONT create_fallback_font() {
    std::string font_name = get_fallback_font();
    std::wstring wname(font_name.begin(), font_name.end());
    int size_px = get_fallback_font_size();
    return CreateFontW(
        size_px, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        wname.c_str());
}

static void apply_fallback_font() {
    if (g_fallbackFont) { DeleteObject(g_fallbackFont); g_fallbackFont = nullptr; }
    g_fallbackFont = create_fallback_font();
    HDC dc = GetDC(nullptr);
    if (dc && g_fallbackFont) { HFONT old = (HFONT)SelectObject(dc, g_fallbackFont); TEXTMETRICW tm = {}; GetTextMetricsW(dc, &tm);
                                g_fallbackFh = tm.tmHeight + tm.tmExternalLeading; g_fallbackFw = tm.tmAveCharWidth; SelectObject(dc, old); }
    if (dc) ReleaseDC(nullptr, dc);
}

static void init_font() {
    if (!g_font) {
        g_font = create_pinyin_font();
        HDC dc = GetDC(nullptr);
        if (dc && g_font) { HFONT old = (HFONT)SelectObject(dc, g_font); TEXTMETRICW tm = {}; GetTextMetricsW(dc, &tm);
                            g_fh = tm.tmHeight + tm.tmExternalLeading; g_fw = tm.tmAveCharWidth; SelectObject(dc, old); }
        if (dc) ReleaseDC(nullptr, dc);
    }
    if (!g_candFont) apply_candidate_font();
    if (!g_fallbackFont) apply_fallback_font();
}

// 候选字体配置变更（设置窗口下拉框）：重建主/备字体 + 重绘候选框，立即生效。
static void refresh_candidate_font() {
    apply_candidate_font();
    apply_fallback_font();
    if (g_candWnd) {
        ClassicABC::UI::UpdateCand();
        InvalidateRect(g_candWnd, nullptr, TRUE);
    }
}

// ---- 字体回退（fallback）----
// 主字体 + 单个备选字体（设置里各选一个、各配字号）。
// 主字体缺字形（如生僻字"靐"）时用备选字体；两者都缺则画方块（缺字形由 GDI 呈现）。
// 性能：主备各做一次整串 GetGlyphIndicesW 批量探测，GDI 调用数 = O(2 + 段数)。
static void TextOutWithFallback(HDC dc, int x, int y,
                                const wchar_t* text, int len,
                                HFONT primary, HFONT fallback) {
    if (len <= 0) return;
    if (!fallback) {  // 无备选字体：直接主字体整串画。
        HFONT old = (HFONT)SelectObject(dc, primary);
        TextOutW(dc, x, y, text, len);
        SelectObject(dc, old);
        return;
    }

    // chosen[i]：0=主字体可显示，1=主缺但备选可显示，2=两者都缺。
    std::vector<int> chosen((size_t)len, 0);
    std::vector<WORD> glyphs((size_t)len);
    {
        HFONT old = (HFONT)SelectObject(dc, primary);
        bool ok = GetGlyphIndicesW(dc, text, len, glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR;
        if (ok) {
            for (int i = 0; i < len; ++i)
                if (glyphs[i] == 0xFFFF) chosen[i] = -1;
        } else {
            for (int i = 0; i < len; ++i) chosen[i] = -1;
        }
        SelectObject(dc, old);
    }

    // 全显示：直接用主字体（最常见路径，零额外开销）。
    bool any_missing = false;
    for (int i = 0; i < len; ++i) if (chosen[i] == -1) { any_missing = true; break; }
    if (!any_missing) {
        HFONT old = (HFONT)SelectObject(dc, primary);
        TextOutW(dc, x, y, text, len);
        SelectObject(dc, old);
        return;
    }

    // 备选字体一次整串探测，补齐缺字；仍缺的标 2（画方块）。
    {
        HFONT old = (HFONT)SelectObject(dc, fallback);
        bool ok = GetGlyphIndicesW(dc, text, len, glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR;
        if (ok) {
            for (int i = 0; i < len; ++i) {
                if (chosen[i] == -1)
                    chosen[i] = (glyphs[i] == 0xFFFF) ? 2 : 1;
            }
        } else {
            for (int i = 0; i < len; ++i)
                if (chosen[i] == -1) chosen[i] = 2;
        }
        SelectObject(dc, old);
    }

    // 分段绘制：连续同字体合成一段。标 1 或 2 都用备选字体画
    // （标 2 时 GDI 自动呈现缺字形方块）。
    int cx = x;
    int i = 0;
    while (i < len) {
        HFONT curFont = (chosen[i] == 0) ? primary : fallback;
        int j = i + 1;
        while (j < len && (chosen[j] == 0) == (chosen[i] == 0)) ++j;
        HFONT old = (HFONT)SelectObject(dc, curFont);
        SIZE sz = {};
        GetTextExtentPoint32W(dc, text + i, j - i, &sz);
        TextOutW(dc, cx, y, text + i, j - i);
        SelectObject(dc, old);
        cx += sz.cx;
        i = j;
    }
}

// --- caret position ---
static POINT caret_pos() {
    POINT pt = {}; HWND fg = GetForegroundWindow();
    if (fg) { DWORD tid = GetWindowThreadProcessId(fg, nullptr); GUITHREADINFO gui = { sizeof(GUITHREADINFO) };
              if (GetGUIThreadInfo(tid, &gui) && gui.hwndCaret) { pt.x = gui.rcCaret.left; pt.y = gui.rcCaret.top; ClientToScreen(gui.hwndCaret, &pt); return pt; } }
    GetCursorPos(&pt); return pt;
}

// 取 pt 所在显示器的工作区（多屏支持：候选框/拼音框/设置栏都按光标所在
// 屏幕的边界定位，而不是主屏幕，否则第二屏上窗口会被钳到主屏内消失）。
static RECT monitor_work_area(POINT pt) {
    RECT wa = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    HMONITOR hm = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (hm) {
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfoW(hm, &mi)) wa = mi.rcWork;
    }
    return wa;
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
    int navY = 6 + count * g_candFh + 1;
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

        const auto& s = ClassicABC::GetCompositionString();
        HFONT old = (HFONT)SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0, 0, 0));
        int tx = g_skin ? g_skin->marginL + 2 : 6;
        int ty = g_skin ? g_skin->marginT : 2;
        SelectObject(dc, old);
        TextOutWithFallback(dc, tx, ty, s.c_str(), (int)s.size(), g_font, g_fallbackFont);
        if (!g_skin) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180)); HPEN op = (HPEN)SelectObject(dc, pen);
            HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); SelectObject(dc, nb);
            Rectangle(dc, 0, 0, rc.right, rc.bottom); SelectObject(dc, op); DeleteObject(pen);
        }

        SelectObject(dc, old); EndPaint(hwnd, &ps); return 0;
    }
    if (msg == WM_USER + 5) {
        // Dictionary cache finished building on the background thread.
        ClassicABC::RefreshCandidates();
        return 0;
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
    if (g_candFont) { DeleteObject(g_candFont); g_candFont = nullptr; }
    if (g_fallbackFont) { DeleteObject(g_fallbackFont); g_fallbackFont = nullptr; }
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
    const auto& buf = ClassicABC::GetCompositionString();
    if (buf.empty()) {
        if (g_wnd) ShowWindow(g_wnd, SW_HIDE);
        ShowCand(false);
        return;
    }

    // 候选数为 0 时，强制隐藏候选框，避免残留显示。
    if (ClassicABC::GetCandidateCount() == 0) {
        ShowCand(false);
    }

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
    RECT wa = monitor_work_area(cp);
    int sw = wa.right - wa.left, sh = wa.bottom - wa.top;
    if (x + g_cw > wa.right) x = wa.right - g_cw; if (y + g_ch > wa.bottom) y = cp.y - g_ch - 4;
    if (x < wa.left) x = wa.left; if (y < wa.top) y = wa.top;
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
        HFONT old = (HFONT)SelectObject(dc, g_candFont);
        SetBkMode(dc, TRANSPARENT);
        COLORREF candColor = ClassicABC::IsDelMode() ? RGB(255, 0, 0) : RGB(128, 0, 128);
        size_t sel = ClassicABC::GetSelectedIndex();
        size_t count = ClassicABC::GetCandidateCount();
        for (size_t i = 0; i < count; ++i) {
            std::wstring text = ClassicABC::GetCandidateText(i);
            if (text.empty()) continue;
            int n = (int)i + 1;
            if (n == 10) n = 0;
            wchar_t num[4]; wsprintfW(num, L"%d:", n);
            COLORREF rowColor = (i == sel) ? RGB(0, 0, 255) : candColor;
            // 序号固定 System 字体（不随候选字体设置变化）。
            SetTextColor(dc, rowColor);
            SelectObject(dc, g_font);
            TextOutW(dc, 4, 6 + (int)i * g_candFh, num, (int)wcslen(num));
            // 候选词本体用可配置字体（带回退）。
            SetTextColor(dc, rowColor);
            SelectObject(dc, g_candFont);
            TextOutWithFallback(dc, 4 + g_candFw * 2, 6 + (int)i * g_candFh,
                                text.c_str(), (int)text.size(), g_candFont, g_fallbackFont);
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
            // 页码固定 System 字体。
            SelectObject(dc, g_font);
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
        bool handled = false;
        for (int i = 0; i < 4; ++i) {
            if (PtInRect(&g_navBtnRects[i], pt)) {
                switch (i) {
                    case 0: ClassicABC::GoFirstPage(); break;
                    case 1: ClassicABC::GoLastPage(); break;
                    case 2: ClassicABC::GoNextPage(); break;
                    case 3: ClassicABC::GoPrevPage(); break;
                }
                handled = true;
                break;
            }
        }
        if (!handled) {
            // 点击候选行：直接输出该候选。
            size_t count = ClassicABC::GetCandidateCount();
            int row = (pt.y - 6) / g_candFh;
            if (pt.y >= 6 && row >= 0 && (size_t)row < count) {
                ClassicABC::PickCandidate((size_t)row);
            }
        }
        return 0;
    }
    if (msg == WM_USER + 5) {
        // Dictionary cache finished building on the background thread:
        // rebuild the candidate list so words appear without user input.
        ClassicABC::RefreshCandidates();
        return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

HWND ClassicABC::UI::GetCandidateWindow() {
    // Prefer the candidate list window; fall back to the pinyin bar window.
    return g_candWnd ? g_candWnd : g_wnd;
}

void ClassicABC::UI::ShowCand(bool visible) {
    if (visible) {
        if (ClassicABC::GetCandidateCount() == 0) {
            if (g_candWnd) ShowWindow(g_candWnd, SW_HIDE);
            return;
        }
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
    int h = 6 + (int)count * g_candFh + kNavBtnSize + 6;
    if (h < 30) h = 30;

    // 自适应宽度：至少 120px，随最长候选文本扩展（汉字按 g_candFw 宽计）。
    int max_text_w = 0;
    HDC meas_dc = GetDC(g_candWnd ? g_candWnd : nullptr);
    HFONT old_font = nullptr;
    if (meas_dc) old_font = (HFONT)SelectObject(meas_dc, g_candFont);
    for (size_t i = 0; i < count; ++i) {
        std::wstring text = ClassicABC::GetCandidateText(i);
        int tw = 0;
        if (meas_dc) {
            SIZE sz;
            if (GetTextExtentPoint32W(meas_dc, text.c_str(), (int)text.size(), &sz))
                tw = sz.cx;
            else
                tw = (int)text.size() * g_candFw;
        } else {
            tw = (int)text.size() * g_candFw;
        }
        if (tw > max_text_w) max_text_w = tw;
    }
    if (meas_dc) {
        if (old_font) SelectObject(meas_dc, old_font);
        ReleaseDC(g_candWnd ? g_candWnd : nullptr, meas_dc);
    }
    int wantW = 4 + g_candFw * 2 + max_text_w + 4;   // 左边距 + 序号 + 文本 + 右边距
    g_candW = (wantW > 120) ? wantW : 120;
    write_log("UI: UpdateCand count=" + std::to_string(count) + " max_text_w=" + std::to_string(max_text_w) +
                  " wantW=" + std::to_string(wantW) + " g_candW=" + std::to_string(g_candW) + " g_candFw=" + std::to_string(g_candFw),
              LOG_INFO);

    // Position: default to the RIGHT of the input window.
    // If no room on right, go LEFT. Then, try below the caret; if no room, go above.
    POINT cp = caret_pos();
    RECT wa = monitor_work_area(cp);
    int sw = wa.right - wa.left, sh = wa.bottom - wa.top;

    // Horizontal: right of input window, fallback to left (use actual pinyin bar rect)
    RECT pr; GetWindowRect(g_wnd, &pr);
    int inputL = pr.left;
    int inputR = pr.right;
    int x = inputR + 4;                     // right of input
    if (x + g_candW > wa.right) x = inputL - g_candW - 4; // fallback: left of input

    // Vertical: try below caret; if candidate doesn't fit, go above (and move pinyin bar up too)
    int belowY = cp.y + g_fh + 4;
    int y = belowY;
    if (y + h > wa.bottom) {
        y = cp.y - h - 4;
        int pinyinY = cp.y - g_ch - 4;
        if (pinyinY < wa.top) pinyinY = wa.top;
        RECT pr; GetWindowRect(g_wnd, &pr);
        SetWindowPos(g_wnd, HWND_TOPMOST, pr.left, pinyinY, 0, 0,
                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }

    if (x < wa.left) x = wa.left; if (y < wa.top) y = wa.top;
    if (x + g_candW > wa.right) x = wa.right - g_candW;
    if (y + h > wa.bottom) y = wa.bottom - h;
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

// --- settings dialog ---
// 标准 Win32 对话框：背景/控件全部交给系统主题绘制，不做任何自绘。
static LRESULT CALLBACK settingsDlgProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_COMMAND) {
        int id = LOWORD(w);
        if (id == kDlgCheckLog && HIWORD(w) == BN_CLICKED) {
            bool checked = SendMessageW((HWND)l, BM_GETCHECK, 0, 0) == BST_CHECKED;
            set_log_enabled(checked);  // 立即生效 + 写盘
            return 0;
        }
        if (id == kDlgBtnSave) {
            HWND edit = GetDlgItem(hwnd, kDlgEditSize);
            wchar_t buf[16] = {};
            GetWindowTextW(edit, buf, 16);
            int v = _wtoi(buf);
            set_page_size(v);  // 钳制 5~10
            save_settings();
            ClassicABC::SetPageSize((size_t)get_page_size());
            wsprintfW(buf, L"%d", get_page_size());
            SetWindowTextW(edit, buf);  // 显示钳制后的值，留在窗口
            return 0;
        }
        if (id == kDlgComboLevel && HIWORD(w) == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0);
            set_log_level(sel);  // 立即生效 + 写盘
            return 0;
        }
        if (id == kDlgComboFont && HIWORD(w) == CBN_SELCHANGE) {
            wchar_t buf[128] = {};
            SendMessageW((HWND)l, CB_GETLBTEXT, (WPARAM)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0), (LPARAM)buf);
            std::wstring wname(buf);
            std::string name(wname.begin(), wname.end());
            set_candidate_font(name);  // 立即生效 + 写盘
            refresh_candidate_font();
            return 0;
        }
        if (id == kDlgComboFontSize && HIWORD(w) == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0);
            wchar_t buf[16] = {};
            SendMessageW((HWND)l, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)buf);
            set_candidate_font_size(_wtoi(buf));  // 立即生效 + 写盘
            refresh_candidate_font();
            return 0;
        }
        if (id == kDlgComboFallbackFont && HIWORD(w) == CBN_SELCHANGE) {
            wchar_t buf[128] = {};
            SendMessageW((HWND)l, CB_GETLBTEXT, (WPARAM)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0), (LPARAM)buf);
            std::wstring wname(buf);
            std::string name(wname.begin(), wname.end());
            set_fallback_font(name);  // 立即生效 + 写盘
            refresh_candidate_font();
            return 0;
        }
        if (id == kDlgComboFallbackSize && HIWORD(w) == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0);
            wchar_t buf[16] = {};
            SendMessageW((HWND)l, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)buf);
            set_fallback_font_size(_wtoi(buf));  // 立即生效 + 写盘
            refresh_candidate_font();
            return 0;
        }
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        if (g_settingsDlg == hwnd) {
            g_settingsDlg = nullptr;
            if (g_settingsDlgFont) { DeleteObject(g_settingsDlgFont); g_settingsDlgFont = nullptr; }
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

static void ShowSettingsDialog() {
    if (!g_settingsDlgClass) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = settingsDlgProc; wc.hInstance = g_inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);  // 系统对话框灰底，跟随主题
        wc.lpszClassName = kSettingsDlgClass; RegisterClassExW(&wc);
        g_settingsDlgClass = true;
    }
    if (!g_settingsDlg) {
        // 标准 Win32 窗口：系统标题栏 + 系统叉按钮 + 系统拖动。
        g_settingsDlg = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            kSettingsDlgClass, L"\u8BBE\u7F6E",  // 设置
            WS_POPUP | WS_CAPTION | WS_SYSMENU,
            0, 0, kDlgW, kDlgH, nullptr, nullptr, g_inst, nullptr);
        if (!g_settingsDlg) return;

        // 宋体 12px（用户指定）。
        g_settingsDlgFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                        DEFAULT_PITCH | FF_DONTCARE, L"SimSun");
        HFONT uiFont = g_settingsDlgFont;

        HWND label = CreateWindowExW(0, L"STATIC", L"\u5019\u9009\u8BCD\u6BCF\u9875\u6570\u91CF (5-10):",
                                     WS_CHILD | WS_VISIBLE, 14, 44, 160, 18,
                                     g_settingsDlg, nullptr, g_inst, nullptr);
        SendMessageW(label, WM_SETFONT, (WPARAM)uiFont, TRUE);
        // 标准输入框外观：WS_EX_CLIENTEDGE（凹陷边框）+ 主题字体。
        HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
                                    176, 40, 52, 24,
                                    g_settingsDlg, (HMENU)kDlgEditSize, g_inst, nullptr);
        wchar_t buf[16];
        wsprintfW(buf, L"%d", get_page_size());
        SetWindowTextW(edit, buf);
        SendMessageW(edit, WM_SETFONT, (WPARAM)uiFont, TRUE);

        HWND chk = CreateWindowExW(0, L"BUTTON", L"\u751F\u6210\u65E5\u5FD7\u6587\u4EF6",
                                   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                   14, 72, 140, 22,
                                   g_settingsDlg, (HMENU)kDlgCheckLog, g_inst, nullptr);
        SendMessageW(chk, BM_SETCHECK, is_log_enabled() ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(chk, WM_SETFONT, (WPARAM)uiFont, TRUE);

        HWND levelLabel = CreateWindowExW(0, L"STATIC", L"\u65E5\u5FD7\u7B49\u7EA7:",
                                          WS_CHILD | WS_VISIBLE, 14, 100, 160, 18,
                                          g_settingsDlg, nullptr, g_inst, nullptr);
        SendMessageW(levelLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);
        HWND combo = CreateWindowExW(0, L"COMBOBOX", L"",
                                     WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                     176, 98, 60, 60,
                                     g_settingsDlg, (HMENU)kDlgComboLevel, g_inst, nullptr);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"INFO");
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"DEBUG");
        SendMessageW(combo, CB_SETCURSEL, get_log_level(), 0);
        SendMessageW(combo, WM_SETFONT, (WPARAM)uiFont, TRUE);

        HWND fontLabel = CreateWindowExW(0, L"STATIC", L"\u5019\u9009\u5B57\u4F53:",
                                         WS_CHILD | WS_VISIBLE, 14, 128, 160, 18,
                                         g_settingsDlg, nullptr, g_inst, nullptr);
        SendMessageW(fontLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);
        HWND fontCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                                         WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                         176, 126, 60, 140,
                                         g_settingsDlg, (HMENU)kDlgComboFont, g_inst, nullptr);
        {
            // 枚举系统全部已安装字体（去重 + 按名称排序）。
            std::vector<std::wstring> fontNames;
            HDC dc = GetDC(nullptr);
            if (dc) {
                LOGFONTW lf = {};
                lf.lfCharSet = DEFAULT_CHARSET;
                struct EnumCtx { std::vector<std::wstring>* names; };
                EnumFontFamiliesExW(dc, &lf,
                    [](const LOGFONTW* plf, const TEXTMETRICW*, DWORD, LPARAM l) -> int CALLBACK {
                        auto* ctx = reinterpret_cast<EnumCtx*>(l);
                        bool dup = false;
                        for (const auto& n : *ctx->names)
                            if (_wcsicmp(n.c_str(), plf->lfFaceName) == 0) { dup = true; break; }
                        if (!dup) ctx->names->push_back(plf->lfFaceName);
                        return TRUE;
                    },
                    (LPARAM)&EnumCtx{&fontNames}, 0);
                ReleaseDC(nullptr, dc);
            }
            std::sort(fontNames.begin(), fontNames.end(),
                      [](const std::wstring& a, const std::wstring& b) {
                          return _wcsicmp(a.c_str(), b.c_str()) < 0;
                      });
            for (const auto& n : fontNames) SendMessageW(fontCombo, CB_ADDSTRING, 0, (LPARAM)n.c_str());
            std::string cur = get_candidate_font();
            std::wstring wcur(cur.begin(), cur.end());
            int idx = SendMessageW(fontCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)wcur.c_str());
            SendMessageW(fontCombo, CB_SETCURSEL, (idx == CB_ERR) ? 0 : (WPARAM)idx, 0);
        }
        SendMessageW(fontCombo, WM_SETFONT, (WPARAM)uiFont, TRUE);

        HWND sizeLabel = CreateWindowExW(0, L"STATIC", L"\u4E3B\u5B57\u53F7:",
                                         WS_CHILD | WS_VISIBLE, 14, 156, 160, 18,
                                         g_settingsDlg, nullptr, g_inst, nullptr);
        SendMessageW(sizeLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);
        HWND sizeCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                                         WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                         176, 154, 60, 140,
                                         g_settingsDlg, (HMENU)kDlgComboFontSize, g_inst, nullptr);
        {
            static const wchar_t* kSizes[] = { L"9", L"10", L"11", L"12", L"13", L"14", L"15", L"16", L"18", L"20", L"24" };
            for (auto s : kSizes) SendMessageW(sizeCombo, CB_ADDSTRING, 0, (LPARAM)s);
            wchar_t szbuf[16];
            wsprintfW(szbuf, L"%d", get_candidate_font_size());
            int idx = SendMessageW(sizeCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)szbuf);
            SendMessageW(sizeCombo, CB_SETCURSEL, (idx == CB_ERR) ? 0 : (WPARAM)idx, 0);
        }
        SendMessageW(sizeCombo, WM_SETFONT, (WPARAM)uiFont, TRUE);

        HWND fbLabel = CreateWindowExW(0, L"STATIC", L"\u5907\u9009\u5B57\u4F53:",
                                       WS_CHILD | WS_VISIBLE, 14, 184, 160, 18,
                                       g_settingsDlg, nullptr, g_inst, nullptr);
        SendMessageW(fbLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);
        HWND fbCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                                       WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                       176, 182, 60, 140,
                                       g_settingsDlg, (HMENU)kDlgComboFallbackFont, g_inst, nullptr);
        {
            std::vector<std::wstring> fontNames;
            HDC dc = GetDC(nullptr);
            if (dc) {
                LOGFONTW lf = {};
                lf.lfCharSet = DEFAULT_CHARSET;
                struct EnumCtx { std::vector<std::wstring>* names; };
                EnumFontFamiliesExW(dc, &lf,
                    [](const LOGFONTW* plf, const TEXTMETRICW*, DWORD, LPARAM l) -> int CALLBACK {
                        auto* ctx = reinterpret_cast<EnumCtx*>(l);
                        bool dup = false;
                        for (const auto& n : *ctx->names)
                            if (_wcsicmp(n.c_str(), plf->lfFaceName) == 0) { dup = true; break; }
                        if (!dup) ctx->names->push_back(plf->lfFaceName);
                        return TRUE;
                    },
                    (LPARAM)&EnumCtx{&fontNames}, 0);
                ReleaseDC(nullptr, dc);
            }
            std::sort(fontNames.begin(), fontNames.end(),
                      [](const std::wstring& a, const std::wstring& b) {
                          return _wcsicmp(a.c_str(), b.c_str()) < 0;
                      });
            for (const auto& n : fontNames) SendMessageW(fbCombo, CB_ADDSTRING, 0, (LPARAM)n.c_str());
            std::string cur = get_fallback_font();
            std::wstring wcur(cur.begin(), cur.end());
            int idx = SendMessageW(fbCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)wcur.c_str());
            SendMessageW(fbCombo, CB_SETCURSEL, (idx == CB_ERR) ? 0 : (WPARAM)idx, 0);
        }
        SendMessageW(fbCombo, WM_SETFONT, (WPARAM)uiFont, TRUE);

        HWND fbsLabel = CreateWindowExW(0, L"STATIC", L"\u5907\u9009\u5B57\u53F7:",
                                        WS_CHILD | WS_VISIBLE, 14, 212, 160, 18,
                                        g_settingsDlg, nullptr, g_inst, nullptr);
        SendMessageW(fbsLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);
        HWND fbsCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                        176, 210, 60, 140,
                                        g_settingsDlg, (HMENU)kDlgComboFallbackSize, g_inst, nullptr);
        {
            static const wchar_t* kSizes[] = { L"9", L"10", L"11", L"12", L"13", L"14", L"15", L"16", L"18", L"20", L"24" };
            for (auto s : kSizes) SendMessageW(fbsCombo, CB_ADDSTRING, 0, (LPARAM)s);
            wchar_t szbuf[16];
            wsprintfW(szbuf, L"%d", get_fallback_font_size());
            int idx = SendMessageW(fbsCombo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)szbuf);
            SendMessageW(fbsCombo, CB_SETCURSEL, (idx == CB_ERR) ? 0 : (WPARAM)idx, 0);
        }
        SendMessageW(fbsCombo, WM_SETFONT, (WPARAM)uiFont, TRUE);

        HWND save = CreateWindowExW(0, L"BUTTON", L"\u4FDD\u5B58",  // 保存
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    50, 244, 70, 28,
                                    g_settingsDlg, (HMENU)kDlgBtnSave, g_inst, nullptr);
        SendMessageW(save, WM_SETFONT, (WPARAM)uiFont, TRUE);
    }

    // 定位：设置栏下方，超出屏幕则放设置栏上方（按设置栏所在显示器边界）。
    int x = g_settingsX, y = g_settingsY + kSettingsH + 4;
    POINT sp = { g_settingsX, g_settingsY };
    RECT wa = monitor_work_area(sp);
    if (y + kDlgH > wa.bottom) y = g_settingsY - kDlgH - 4;
    if (y < wa.top) y = wa.top;
    if (x + kDlgW > wa.right) x = wa.right - kDlgW;
    if (x < wa.left) x = wa.left;
    SetWindowPos(g_settingsDlg, HWND_TOPMOST, x, y, kDlgW, kDlgH,
                 SWP_SHOWWINDOW);
    SetFocus(g_settingsDlg);  // 标准窗口：激活设置窗口（原窗口暂时失焦）
}

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
                    if (ClassicABC::IsCapsLockActive())
                        mode = 0; // capital
                    else if (!ClassicABC::IsChineseMode())
                        mode = 1; // english
                    icon = g_modeIcons[mode];
                }
                // Button 3: sign_en icon when not Chinese mode
                if (i == 3 && g_signEnIcon) {
                    bool notChinese = ClassicABC::IsCapsLockActive() ||
                                      !ClassicABC::IsChineseMode();
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
    if (msg == WM_RBUTTONUP) {
        // 整条设置栏（含按钮）右键：弹出菜单，目前只有"设置"。
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 1, L"\u8BBE\u7F6E");  // 设置
        POINT pt; GetCursorPos(&pt);
        SetForegroundWindow(hwnd);  // 让菜单能自动关闭
        int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                 pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(menu);
        PostMessageW(hwnd, WM_NULL, 0, 0);
        if (cmd == 1) ShowSettingsDialog();
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

bool ClassicABC::UI::LoadBuiltinSkin(BuiltinSkinId skin_id, ClassicABC::UI::NinePatchSkin& skin) {
    switch (skin_id) {
        case BuiltinSkinId::Candidate:
            return fill_skin_from_spec(skin, {kShadowSkinPixels, 9, 9, 4, 4, 4, 4});
        case BuiltinSkinId::Settings:
            return fill_skin_from_spec(skin, {kCommonSkinPixels, 5, 5, 2, 2, 2, 2});
        case BuiltinSkinId::Button:
            return fill_skin_from_spec(skin, {kButtonSkinPixels, 5, 5, 2, 2, 2, 2});
    }
    return false;
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
