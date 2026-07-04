#pragma once
#include <KeyEvent.h>
#include <string>
#include "proto_core.h"

#define MAX_COMPOSITION_SIZE 256

struct CompositionInfo {
  COMPOSITIONSTRING cs;
  WCHAR szCompStr[MAX_COMPOSITION_SIZE];
  WCHAR szResultStr[MAX_COMPOSITION_SIZE];
  void Reset() {
    memset(this, 0, sizeof(*this));
    cs.dwSize = sizeof(*this);
    cs.dwCompStrOffset = (DWORD)((ptrdiff_t)&szCompStr - (ptrdiff_t)this);
    cs.dwResultStrOffset = (DWORD)((ptrdiff_t)&szResultStr - (ptrdiff_t)this);
  }
};

class IME;

class HIMCMap : public std::map<HIMC, std::shared_ptr<IME> > {
 public:
  HIMCMap() : m_valid(true) {}
  ~HIMCMap() { m_valid = false; }
  std::mutex& get_mutex() { return m_mutex; }
  bool is_valid() const { return m_valid; }

 private:
  bool m_valid;
  std::mutex m_mutex;
};

class IME {
 public:
  static HINSTANCE GetModuleInstance();
  static void SetModuleInstance(HINSTANCE hModule);
  static HRESULT RegisterUIClass();
  static HRESULT UnregisterUIClass();
  static LPCWSTR GetUIClassName();
  static LRESULT WINAPI UIWndProc(HWND hWnd, UINT uMsg, WPARAM wp, LPARAM lp);
  static BOOL IsIMEMessage(UINT uMsg);
  static std::shared_ptr<IME> GetInstance(HIMC hIMC);
  static void Cleanup();

  IME(HIMC hIMC);
  LRESULT OnIMESelect(BOOL fSelect);
  LRESULT OnIMEFocus(BOOL fFocus);
  LRESULT OnUIMessage(HWND hWnd, UINT uMsg, WPARAM wp, LPARAM lp);
  BOOL ProcessKeyEvent(UINT vKey, KeyInfo kinfo, const LPBYTE lpbKeyState);

 private:
  HRESULT _Initialize();
  HRESULT _Finalize();
  LRESULT _OnIMENotify(LPINPUTCONTEXT lpIMC, WPARAM wp, LPARAM lp);

  void _InitEngine();
  void _ShutdownEngine();

  static HINSTANCE s_hModule;
  static HIMCMap s_instances;
  HIMC m_hIMC;

  std::wstring _dllDir;
  bool _engineReady = false;
  ProtoIME::NinePatchSkin _skin;
  ProtoIME::NinePatchSkin _settingsSkin;
  ProtoIME::NinePatchSkin _btnSkin;
};
