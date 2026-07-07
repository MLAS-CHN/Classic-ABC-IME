// IME.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <algorithm>
#include "IME.h"
#include "../util.h"

HINSTANCE IME::s_hModule = 0;
HIMCMap IME::s_instances;

IME::IME(HIMC hIMC)
    : m_hIMC(hIMC) {
}

HINSTANCE IME::GetModuleInstance() {
  return s_hModule;
}

void IME::SetModuleInstance(HINSTANCE hModule) {
  s_hModule = hModule;
}

HRESULT IME::RegisterUIClass() {
  WNDCLASSEX wc;
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_IME;
  wc.lpfnWndProc = IME::UIWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 2 * sizeof(LONG);
  wc.hInstance = GetModuleInstance();
  wc.hCursor = NULL;
  wc.hIcon = NULL;
  wc.lpszMenuName = NULL;
  wc.lpszClassName = GetUIClassName();
  wc.hbrBackground = NULL;
  wc.hIconSm = NULL;

  if (RegisterClassExW(&wc) == 0) {
    DWORD dwErr = GetLastError();
    return HRESULT_FROM_WIN32(dwErr);
  }

  return S_OK;
}

HRESULT IME::UnregisterUIClass() {
  if (!UnregisterClassW(GetUIClassName(), GetModuleInstance())) {
    DWORD dwErr = GetLastError();
    return HRESULT_FROM_WIN32(dwErr);
  }
  return S_OK;
}

LPCWSTR IME::GetUIClassName() {
  return L"TSFClass";
}

LRESULT WINAPI IME::UIWndProc(HWND hWnd,
                                    UINT uMsg,
                                    WPARAM wp,
                                    LPARAM lp) {
  HIMC hIMC = (HIMC)GetWindowLongPtr(hWnd, 0);
  if (hIMC) {
    std::shared_ptr<IME> p = IME::GetInstance(hIMC);
    if (!p)
      return 0;
    return p->OnUIMessage(hWnd, uMsg, wp, lp);
  } else {
    if (!IsIMEMessage(uMsg)) {
      return DefWindowProcW(hWnd, uMsg, wp, lp);
    }
  }

  return 0;
}

BOOL IME::IsIMEMessage(UINT uMsg) {
  switch (uMsg) {
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_NOTIFY:
    case WM_IME_SETCONTEXT:
    case WM_IME_CONTROL:
    case WM_IME_COMPOSITIONFULL:
    case WM_IME_SELECT:
    case WM_IME_CHAR:
      return TRUE;
    default:
      return FALSE;
  }

  return FALSE;
}

std::shared_ptr<IME> IME::GetInstance(HIMC hIMC) {
  if (!s_instances.is_valid()) {
    return std::shared_ptr<IME>();
  }
  std::lock_guard<std::mutex> lock(s_instances.get_mutex());
  std::shared_ptr<IME>& p = s_instances[hIMC];
  if (!p) {
    p.reset(new IME(hIMC));
  }
  return p;
}

void IME::Cleanup() {
  std::for_each(s_instances.begin(), s_instances.end(),
                [](std::pair<const HIMC, std::shared_ptr<IME>>& pair) {
                  pair.second->OnIMESelect(FALSE);
                });
  std::lock_guard<std::mutex> lock(s_instances.get_mutex());
  s_instances.clear();
}

LRESULT IME::OnIMESelect(BOOL fSelect) {
  ImmSetOpenStatus(m_hIMC, fSelect);
  if (fSelect) {
    return _Initialize();
  } else {
    return _Finalize();
  }
}

LRESULT IME::OnIMEFocus(BOOL fFocus) {
  LPINPUTCONTEXT lpIMC = ImmLockIMC(m_hIMC);
  if (!lpIMC) {
    return 0;
  }
  if (fFocus) {
    if (!(lpIMC->fdwInit & INIT_COMPFORM)) {
      lpIMC->cfCompForm.dwStyle = CFS_DEFAULT;
      GetCursorPos(&lpIMC->cfCompForm.ptCurrentPos);
      ScreenToClient(lpIMC->hWnd, &lpIMC->cfCompForm.ptCurrentPos);
      lpIMC->fdwInit |= INIT_COMPFORM;
    }
    if (_engineReady)
      ClassicABC::SetFocused(true);
  } else {
    if (_engineReady)
      ClassicABC::SetFocused(false);
  }
  ImmUnlockIMC(m_hIMC);

  return 0;
}

LRESULT IME::OnUIMessage(HWND hWnd, UINT uMsg, WPARAM wp, LPARAM lp) {
  LPINPUTCONTEXT lpIMC = (LPINPUTCONTEXT)ImmLockIMC(m_hIMC);
  if (!IsIMEMessage(uMsg)) {
    ImmUnlockIMC(m_hIMC);
    return DefWindowProcW(hWnd, uMsg, wp, lp);
  }
  ImmUnlockIMC(m_hIMC);
  return 0;
}

LRESULT IME::_OnIMENotify(LPINPUTCONTEXT lpIMC, WPARAM wp, LPARAM lp) {
  return 0;
}

BOOL IME::ProcessKeyEvent(UINT vKey,
                                KeyInfo kinfo,
                                const LPBYTE lpbKeyState) {
  if (!ImmGetOpenStatus(m_hIMC))
    return FALSE;
  if (!_engineReady)
    return FALSE;

  if (ClassicABC::TestKeyDown(vKey)) {
    ClassicABC::SetFocused(true);
    ClassicABC::OnKeyDown(vKey);
    return TRUE;
  }
  return FALSE;
}

void IME::_InitEngine() {
  if (_engineReady) return;

  wchar_t dllDir[MAX_PATH] = {0};
  GetModuleFileNameW(s_hModule, dllDir, MAX_PATH);
  wchar_t* p = wcsrchr(dllDir, L'\\');
  if (p) *p = L'\0';
  _dllDir = dllDir;

  if (GetFileAttributesW((_dllDir + L"\\data").c_str()) == INVALID_FILE_ATTRIBUTES) {
    wchar_t pDataDir[MAX_PATH] = {0};
    DWORD len = GetEnvironmentVariableW(L"ProgramData", pDataDir, MAX_PATH);
    if (len > 0) {
      std::wstring fallback = std::wstring(pDataDir) + L"\\ClassicABC";
      if (GetFileAttributesW((fallback + L"\\data").c_str()) != INVALID_FILE_ATTRIBUTES)
        _dllDir = fallback;
    }
  }

  {
    int n = WideCharToMultiByte(CP_UTF8, 0, dllDir, -1, nullptr, 0, nullptr, nullptr);
    if (n > 0) {
      std::string dir((size_t)(n - 1), '\0');
      WideCharToMultiByte(CP_UTF8, 0, dllDir, -1, &dir[0], n, nullptr, nullptr);
      init_logger_with_dir(dir);
    }
  }

  std::wstring dataPath = _dllDir + L"\\data";
  if (GetFileAttributesW((dataPath + L"\\pinyin_map.txt").c_str()) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxW(NULL, (L"找不到词库文件！\n请将 data 目录放在 DLL 同级目录或 %ProgramData%\\ClassicABC\\data 下。\n\nDLL 目录: " + _dllDir).c_str(),
                L"经典ABC - 错误", MB_OK | MB_ICONERROR);
    return;
  }

  std::wstring skinPath = _dllDir + L"\\res\\shadow.png";
  if (GetFileAttributesW(skinPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxW(NULL, (L"找不到皮肤文件！\n请将 res 目录放在 DLL 同级目录或 %ProgramData%\\ClassicABC\\res 下。\n\nDLL 目录: " + _dllDir).c_str(),
                L"经典ABC - 错误", MB_OK | MB_ICONERROR);
    return;
  }

  ClassicABC::SetDataDir(dataPath.c_str());
  ClassicABC::Initialize(s_hModule);
  ClassicABC::SetActive(true);

  if (ClassicABC::LoadSkinFromFile(skinPath.c_str(), _skin, 4, 4, 4, 4))
    ClassicABC::SetSkin(&_skin);
  std::wstring commonPath = _dllDir + L"\\res\\common.png";
  if (ClassicABC::LoadSkinFromFile(commonPath.c_str(), _settingsSkin, 2, 2, 2, 2))
    ClassicABC::SetSettingsSkin(&_settingsSkin);
  std::wstring btnPath = _dllDir + L"\\res\\button.png";
  if (ClassicABC::LoadSkinFromFile(btnPath.c_str(), _btnSkin, 2, 2, 2, 2))
    ClassicABC::SetBtnSkin(&_btnSkin);

  ClassicABC::SetBtnIcon(0, (_dllDir + L"\\res\\ABC_ICON.png").c_str());
  ClassicABC::SetBtnIcon(2, (_dllDir + L"\\res\\half.png").c_str());
  ClassicABC::SetBtnIcon(3, (_dllDir + L"\\res\\sign.png").c_str());
  ClassicABC::SetBtnIcon(4, (_dllDir + L"\\res\\keyboard.png").c_str());
  ClassicABC::SetModeIcon(0, (_dllDir + L"\\res\\capital.png").c_str());
  ClassicABC::SetModeIcon(1, (_dllDir + L"\\res\\english.png").c_str());
  ClassicABC::SetModeIcon(2, (_dllDir + L"\\res\\pinyin.png").c_str());
  ClassicABC::SetLockIcon((_dllDir + L"\\res\\ABC_ICON_GRAY.png").c_str());
  ClassicABC::SetSignEnIcon((_dllDir + L"\\res\\sign_en.png").c_str());

  _engineReady = true;
}

void IME::_ShutdownEngine() {
  if (!_engineReady) return;
  ClassicABC::SetActive(false);
  ClassicABC::SetSkin(nullptr);
  ClassicABC::FreeSkin(_skin);
  ClassicABC::SetSettingsSkin(nullptr);
  ClassicABC::FreeSkin(_settingsSkin);
  ClassicABC::SetBtnSkin(nullptr);
  ClassicABC::FreeSkin(_btnSkin);
  ClassicABC::Shutdown();
  _engineReady = false;
}

HRESULT IME::_Initialize() {
  LPINPUTCONTEXT lpIMC = ImmLockIMC(m_hIMC);
  if (!lpIMC)
    return E_FAIL;

  lpIMC->fOpen = TRUE;

  HIMCC& hIMCC = lpIMC->hCompStr;
  if (!hIMCC)
    hIMCC = ImmCreateIMCC(sizeof(CompositionInfo));
  else
    hIMCC = ImmReSizeIMCC(hIMCC, sizeof(CompositionInfo));
  if (!hIMCC) {
    ImmUnlockIMC(m_hIMC);
    return E_FAIL;
  }

  CompositionInfo* pInfo = (CompositionInfo*)ImmLockIMCC(hIMCC);
  if (!pInfo) {
    ImmUnlockIMC(m_hIMC);
    return E_FAIL;
  }

  pInfo->Reset();
  ImmUnlockIMCC(hIMCC);
  ImmUnlockIMC(m_hIMC);

  _InitEngine();

  return S_OK;
}

HRESULT IME::_Finalize() {
  _ShutdownEngine();

  LPINPUTCONTEXT lpIMC = ImmLockIMC(m_hIMC);
  if (lpIMC) {
    lpIMC->fOpen = FALSE;
    if (lpIMC->hCompStr) {
      ImmDestroyIMCC(lpIMC->hCompStr);
      lpIMC->hCompStr = NULL;
    }
  }
  ImmUnlockIMC(m_hIMC);

  return S_OK;
}
