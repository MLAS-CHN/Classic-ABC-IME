#include "stdafx.h"
#include "TSF.h"
#include "../util.h"

static std::string vk_name(UINT vk) {
  if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
  switch (vk) {
    case VK_SHIFT:   return "Shift";
    case VK_LSHIFT:  return "LShift";
    case VK_RSHIFT:  return "RShift";
    case VK_CONTROL: return "Ctrl";
    case VK_CAPITAL: return "CapsLock";
    case VK_SPACE:   return "Space";
    case VK_BACK:    return "Back";
    case VK_RETURN:  return "Enter";
    case VK_ESCAPE:  return "Esc";
    case VK_DELETE:  return "Del";
    case VK_LEFT:    return "Left";
    case VK_RIGHT:   return "Right";
    default: {
      char buf[16];
      sprintf_s(buf, "0x%02X", vk);
      return buf;
    }
  }
}

void TSF::_ProcessKeyEvent(WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
  if (_IsKeyboardDisabled()) {
    *pfEaten = FALSE;
    return;
  }
  if (!_engineReady) {
    *pfEaten = FALSE;
    return;
  }

  UINT vKey = static_cast<UINT>(wParam);
  bool claimed = ClassicABC::TestKeyDown(vKey);
  *pfEaten = claimed ? TRUE : FALSE;
  write_log("TSF: _ProcessKeyEvent vk=" + vk_name(vKey) + " claimed=" + (claimed ? "YES" : "no"), LOG_DEBUG);
}

STDAPI TSF::OnSetFocus(BOOL fForeground) {
  if (_engineReady)
    ClassicABC::SetFocused(fForeground != FALSE);
  if (!fForeground)
    _AbortComposition();
  return S_OK;
}

STDAPI TSF::OnTestKeyDown(ITfContext* pContext,
                                WPARAM wParam,
                                LPARAM lParam,
                                BOOL* pfEaten) {
  _fTestKeyUpPending = FALSE;
  if (_fTestKeyDownPending) {
    *pfEaten = TRUE;
    return S_OK;
  }
  _ProcessKeyEvent(wParam, lParam, pfEaten);
  if (*pfEaten)
    _fTestKeyDownPending = TRUE;
  return S_OK;
}

STDAPI TSF::OnKeyDown(ITfContext* pContext,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL* pfEaten) {
  _fTestKeyUpPending = FALSE;
  if (_fTestKeyDownPending) {
    _fTestKeyDownPending = FALSE;
    *pfEaten = TRUE;
    if (_engineReady) {
      UINT vKey = static_cast<UINT>(wParam);
      bool eaten = ClassicABC::OnKeyDown(vKey);
      *pfEaten = eaten ? TRUE : FALSE;
      write_log("TSF: OnKeyDown vk=" + vk_name(vKey) + " eaten=" + (eaten ? "YES" : "no"), LOG_DEBUG);
    }
  } else {
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    if (*pfEaten && _engineReady) {
      UINT vKey = static_cast<UINT>(wParam);
      bool eaten = ClassicABC::OnKeyDown(vKey);
      *pfEaten = eaten ? TRUE : FALSE;
      write_log("TSF: OnKeyDown(fallback) vk=" + vk_name(vKey) + " eaten=" + (eaten ? "YES" : "no"), LOG_DEBUG);
    }
  }
  return S_OK;
}

STDAPI TSF::OnTestKeyUp(ITfContext* pContext,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL* pfEaten) {
  _fTestKeyDownPending = FALSE;
  if (_fTestKeyUpPending) {
    *pfEaten = TRUE;
    return S_OK;
  }
  UINT vKey = static_cast<UINT>(wParam);
  if (vKey == VK_SHIFT || vKey == VK_LSHIFT || vKey == VK_RSHIFT) {
    *pfEaten = TRUE;
    return S_OK;
  }
  *pfEaten = FALSE;
  return S_OK;
}

STDAPI TSF::OnKeyUp(ITfContext* pContext,
                          WPARAM wParam,
                          LPARAM lParam,
                          BOOL* pfEaten) {
  _fTestKeyDownPending = FALSE;
  UINT vKey = static_cast<UINT>(wParam);
  if (_fTestKeyUpPending) {
    _fTestKeyUpPending = FALSE;
    *pfEaten = TRUE;
  } else {
    if (vKey == VK_SHIFT || vKey == VK_LSHIFT || vKey == VK_RSHIFT) {
      _fTestKeyUpPending = TRUE;
    }
    if (_engineReady) {
      bool eaten = ClassicABC::OnKeyUp(vKey);
      *pfEaten = eaten ? TRUE : FALSE;
    } else {
      *pfEaten = FALSE;
    }
  }
  return S_OK;
}

STDAPI TSF::OnPreservedKey(ITfContext* pContext,
                                 REFGUID rguid,
                                 BOOL* pfEaten) {
  *pfEaten = FALSE;
  return S_OK;
}

BOOL TSF::_InitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
  HRESULT hr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return FALSE;

  hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink*)this,
                                         TRUE);

  return (hr == S_OK);
}

void TSF::_UninitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return;

  pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
}
