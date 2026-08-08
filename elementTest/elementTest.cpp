// elementTest.cpp - Win32 控件样式对比测试。
// 同一个窗口展示多种输入框/按钮控件：
//   elementTest_Classic.exe   - 经典样式（无 ComCtl32 v6 manifest）
//   elementTest_Modern.exe    - 现代主题（/DENABLE_V6 编译，启用视觉样式）
// 编译（x64，需 VS2022 环境）：
//   cl /nologo /EHsc /utf-8 /O2 elementTest.cpp /Fe:elementTest_Classic.exe /link /SUBSYSTEM:WINDOWS
//   cl /nologo /EHsc /utf-8 /O2 /DENABLE_V6 elementTest.cpp /Fe:elementTest_Modern.exe /link /SUBSYSTEM:WINDOWS
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <string>

#ifdef ENABLE_V6
// 启用 ComCtl32 v6 视觉样式（现代主题外观）。
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static HFONT g_font = nullptr;
static HFONT g_songFonts[5] = {};  // 宋体 9/10/11/12/13px
static HWND g_editPlain, g_editSunken, g_editBorder, g_editMulti, g_editReadOnly;
static HWND g_btnDefault, g_btnNormal, g_btnToggle;
static HWND g_chk, g_rad1, g_rad2, g_combo, g_spin;
static HWND g_status;

static const int IDC_TOGGLE = 200;

// 每行布局：标签 x=12 宽 150，控件 x=170 宽 200
static int g_rowY = 0;
static void NextRow() { g_rowY += 26; }

static HWND Make(HWND parent, LPCWSTR cls, LPCWSTR text, DWORD style, DWORD exstyle, int x, int w, int h, int id) {
    HWND c = CreateWindowExW(exstyle, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, g_rowY, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    if (c && g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

static void AddLabel(HWND parent, const wchar_t* text) {
    Make(parent, L"STATIC", text, SS_RIGHT, 0, 12, 150, 20, 0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        g_font = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        for (int i = 0; i < 5; ++i) {
            g_songFonts[i] = CreateFontW(-(9 + i), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"SimSun");
        }

        // 宋体大小对比（9~13px）
        for (int i = 0; i < 5; ++i) {
            wchar_t label[64];
            swprintf_s(label, L"%dpx \u5B8B\u4F53\u6D4B\u8BD5\u6587\u672C ABC 0123", 9 + i);
            HWND h = CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
                                     12, g_rowY, 380, 24, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (g_songFonts[i]) SendMessageW(h, WM_SETFONT, (WPARAM)g_songFonts[i], TRUE);
            NextRow();
        }
        NextRow();

        AddLabel(hwnd, L"1. 无边框 EDIT:");
        g_editPlain = Make(hwnd, L"EDIT", L"plain (no border)", WS_TABSTOP | ES_AUTOHSCROLL, 0, 170, 200, 22, 1);
        NextRow();

        AddLabel(hwnd, L"2. WS_EX_CLIENTEDGE:");
        g_editSunken = Make(hwnd, L"EDIT", L"clientedge (sunken)", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, 170, 200, 22, 2);
        NextRow();

        AddLabel(hwnd, L"3. WS_BORDER:");
        g_editBorder = Make(hwnd, L"EDIT", L"border (thin)", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 0, 170, 200, 22, 3);
        NextRow();

        AddLabel(hwnd, L"4. 多行 EDIT:");
        g_editMulti = Make(hwnd, L"EDIT", L"multi line\nsecond line", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 0, 170, 200, 48, 4);
        NextRow(); NextRow();

        AddLabel(hwnd, L"5. 只读 EDIT:");
        g_editReadOnly = Make(hwnd, L"EDIT", L"readonly", WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, 170, 200, 22, 5);
        NextRow();

        AddLabel(hwnd, L"6. 微调框+EDIT:");
        g_editPlain;  // 复用标签行
        {
            HWND e = Make(hwnd, L"EDIT", L"10", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, 170, 160, 22, 6);
            g_spin = CreateWindowExW(0, UPDOWN_CLASSW, L"", WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
                                     170 + 160, g_rowY, 40, 22, hwnd, (HMENU)(INT_PTR)101, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(g_spin, UDM_SETRANGE, 0, MAKELPARAM(100, 1));
            SendMessageW(g_spin, UDM_SETBUDDY, (WPARAM)e, 0);
            if (g_font) SendMessageW(g_spin, WM_SETFONT, (WPARAM)g_font, TRUE);
        }
        NextRow();

        AddLabel(hwnd, L"7. 按钮(默认):");
        g_btnDefault = Make(hwnd, L"BUTTON", L"Default", WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 170, 90, 24, 7);
        NextRow();

        AddLabel(hwnd, L"8. 按钮(普通):");
        g_btnNormal = Make(hwnd, L"BUTTON", L"Normal", WS_TABSTOP | BS_PUSHBUTTON, 0, 170, 90, 24, 8);
        NextRow();

        AddLabel(hwnd, L"9. 复选框:");
        g_chk = Make(hwnd, L"BUTTON", L"Check me", WS_TABSTOP | BS_AUTOCHECKBOX, 0, 170, 120, 22, 9);
        NextRow();

        AddLabel(hwnd, L"10. 单选按钮:");
        g_rad1 = Make(hwnd, L"BUTTON", L"Radio A", WS_TABSTOP | BS_AUTORADIOBUTTON, 0, 170, 90, 22, 10);
        g_rad2 = Make(hwnd, L"BUTTON", L"Radio B", WS_TABSTOP | BS_AUTORADIOBUTTON, 0, 265, 90, 22, 11);
        NextRow();

        AddLabel(hwnd, L"11. 下拉框:");
        g_combo = Make(hwnd, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 170, 200, 120, 12);
        SendMessageW(g_combo, CB_ADDSTRING, 0, (LPARAM)L"Item One");
        SendMessageW(g_combo, CB_ADDSTRING, 0, (LPARAM)L"Item Two");
        SendMessageW(g_combo, CB_ADDSTRING, 0, (LPARAM)L"Item Three");
        SendMessageW(g_combo, CB_SETCURSEL, 0, 0);
        NextRow();

        AddLabel(hwnd, L"12. 开关凹陷(2号):");
        g_btnToggle = Make(hwnd, L"BUTTON", L"Toggle Sunken", WS_TABSTOP | BS_PUSHBUTTON, 0, 170, 110, 24, IDC_TOGGLE);
        NextRow();

        // 状态行
        wchar_t buf[256];
        BOOL themed = FALSE;
        if (IsThemeActive()) { HWND th = g_editPlain; if (th) themed = IsThemeActive(); }
        swprintf_s(buf, L"v6 theme active: %s   app themed: %s",
                   (IsThemeActive() ? L"YES" : L"NO"),
                   (IsAppThemed() ? L"YES" : L"NO"));
        g_status = CreateWindowExW(0, L"STATIC", buf, WS_CHILD | WS_VISIBLE, 12, 6, 420, 20,
                                   hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (g_font) SendMessageW(g_status, WM_SETFONT, (WPARAM)g_font, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == IDC_TOGGLE) {
            LONG ex = GetWindowLongW(g_editSunken, GWL_EXSTYLE);
            SetWindowLongW(g_editSunken, GWL_EXSTYLE, ex ^ WS_EX_CLIENTEDGE);
            SetWindowPos(g_editSunken, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            InvalidateRect(g_editSunken, nullptr, TRUE);
        }
        if (LOWORD(w) == 9) {
            wchar_t t[64];
            swprintf_s(t, L"Check me [%s]", SendMessageW(g_chk, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"ON" : L"OFF");
            SetWindowTextW(g_chk, t);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH bg = (HBRUSH)(COLOR_BTNFACE + 1);
        FillRect(dc, &rc, bg);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        if (g_font) DeleteObject(g_font);
        for (int i = 0; i < 5; ++i)
            if (g_songFonts[i]) DeleteObject(g_songFonts[i]);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, w, l);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ElementTestWnd";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"ElementTestWnd",
#ifdef ENABLE_V6
                                L"Element Test - MODERN (v6 theme)",
#else
                                L"Element Test - CLASSIC (no v6)",
#endif
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 420, 600,
                                nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
