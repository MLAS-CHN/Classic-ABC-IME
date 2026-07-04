#include "stdafx.h"
#include "IME.h"

#pragma warning(disable : 4996)

static BOOL g_is_winlogon = FALSE;

//
// IME export functions
//

BOOL WINAPI ImeInquire(IMEINFO* lpIMEInfo,
                       LPWSTR lpszUIClass,
                       DWORD dwSystemInfoFlags) {
  if (!lpIMEInfo || !lpszUIClass)
    return FALSE;

  if (dwSystemInfoFlags & IME_SYSINFO_WINLOGON) {
    // disable input method in winlogon.exe
    g_is_winlogon = TRUE;
  }

  wcscpy(lpszUIClass, IME::GetUIClassName());

  lpIMEInfo->dwPrivateDataSize = 0;
  lpIMEInfo->fdwProperty = IME_PROP_UNICODE | IME_PROP_SPECIAL_UI;
  lpIMEInfo->fdwConversionCaps = IME_CMODE_FULLSHAPE | IME_CMODE_NATIVE;
  lpIMEInfo->fdwSentenceCaps = IME_SMODE_NONE;
  lpIMEInfo->fdwUICaps = UI_CAP_2700;
  lpIMEInfo->fdwSCSCaps = 0;
  lpIMEInfo->fdwSelectCaps = SELECT_CAP_CONVERSION;

  return TRUE;
}

BOOL WINAPI ImeConfigure(HKL hKL, HWND hWnd, DWORD dwMode, LPVOID lpData) {
  return TRUE;
}

BOOL WINAPI ImeDestroy(UINT uForce) {
  return TRUE;
}

BOOL WINAPI ImeProcessKey(HIMC hIMC,
                          UINT vKey,
                          LPARAM lKeyData,
                          const LPBYTE lpbKeyState) {
  if (g_is_winlogon)
    return FALSE;

  BOOL accepted = FALSE;
  std::shared_ptr<IME> p = IME::GetInstance(hIMC);
  if (!p)
    return FALSE;
  accepted = p->ProcessKeyEvent(vKey, lKeyData, lpbKeyState);
  return accepted;
}

BOOL WINAPI ImeSelect(HIMC hIMC, BOOL fSelect) {
  if (g_is_winlogon)
    return TRUE;

  std::shared_ptr<IME> p = IME::GetInstance(hIMC);
  if (!p)
    return FALSE;
  HRESULT hr = p->OnIMESelect(fSelect);
  if (FAILED(hr))
    return FALSE;

  return TRUE;
}

BOOL WINAPI ImeSetActiveContext(HIMC hIMC, BOOL fFocus) {
  if (g_is_winlogon)
    return TRUE;

  if (hIMC) {
    std::shared_ptr<IME> p = IME::GetInstance(hIMC);
    if (!p)
      return FALSE;
    HRESULT hr = p->OnIMEFocus(fFocus);
    if (FAILED(hr))
      return FALSE;
  }

  return TRUE;
}

UINT WINAPI ImeToAsciiEx(UINT uVKey,
                         UINT uScanCode,
                         CONST LPBYTE lpbKeyState,
                         LPDWORD lpdwTransKey,
                         UINT fuState,
                         HIMC hIMC) {
  //
  return 0;
}

BOOL WINAPI NotifyIME(HIMC hIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue) {
  return TRUE;
}
