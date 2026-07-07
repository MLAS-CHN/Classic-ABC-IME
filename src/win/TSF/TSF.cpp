#include "stdafx.h"
#include "TSF.h"
#include "../util.h"

TSF::TSF() {
  _cRef = 1;
  _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
  _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
  _fTestKeyDownPending = FALSE;
  _fTestKeyUpPending = FALSE;
  _engineReady = false;
  DllAddRef();
}

TSF::~TSF() {
  DllRelease();
}

STDAPI TSF::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL) return E_INVALIDARG;
  *ppvObject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
    *ppvObject = (ITfTextInputProcessor*)this;
  else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    *ppvObject = (ITfTextInputProcessorEx*)this;
  else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    *ppvObject = (ITfThreadMgrEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextEditSink))
    *ppvObject = (ITfTextEditSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextLayoutSink))
    *ppvObject = (ITfTextLayoutSink*)this;
  else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    *ppvObject = (ITfKeyEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfCompositionSink))
    *ppvObject = (ITfCompositionSink*)this;
  else if (IsEqualIID(riid, IID_ITfEditSession))
    *ppvObject = (ITfEditSession*)this;
  else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    *ppvObject = (ITfThreadFocusSink*)this;

  if (*ppvObject) { AddRef(); return S_OK; }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) TSF::AddRef() { return InterlockedIncrement(&_cRef); }

STDAPI_(ULONG) TSF::Release() {
  LONG cr = InterlockedDecrement(&_cRef);
  assert(cr >= 0);
  if (cr == 0) delete this;
  return cr;
}

STDAPI TSF::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
  return ActivateEx(pThreadMgr, tfClientId, 0U);
}

STDAPI TSF::Deactivate() {
  _ShutdownEngine();
  _InitTextEditSink(com_ptr<ITfDocumentMgr>());
  _UninitThreadMgrEventSink();
  _UninitKeyEventSink();
  _UninitCompartment();
  _UninitThreadFocusSink();
  _pThreadMgr = NULL;
  _tfClientId = TF_CLIENTID_NULL;
  return S_OK;
}

STDAPI TSF::ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD dwFlags) {
  com_ptr<ITfDocumentMgr> pDocMgrFocus;
  _pThreadMgr = pThreadMgr;
  _tfClientId = tfClientId;

  if (!_InitThreadMgrEventSink()) goto ExitError;
  if ((_pThreadMgr->GetFocus(&pDocMgrFocus) == S_OK) && (pDocMgrFocus != NULL))
    _InitTextEditSink(pDocMgrFocus);
  if (!_InitKeyEventSink()) goto ExitError;
  if (!_IsKeyboardOpen()) _SetKeyboardOpen(TRUE);
  if (!_InitCompartment()) goto ExitError;
  if (!_InitThreadFocusSink()) goto ExitError;
  _InitEngine();
  return S_OK;

ExitError:
  Deactivate();
  return E_FAIL;
}

void TSF::_InitEngine() {
  if (_engineReady) return;

  wchar_t dllDir[MAX_PATH] = {0};
  GetModuleFileNameW(g_hInst, dllDir, MAX_PATH);
  wchar_t* p = wcsrchr(dllDir, L'\\');
  if (p) *p = L'\0';
  _dllDir = dllDir;

  if (GetFileAttributesW((_dllDir + L"\\data").c_str()) == INVALID_FILE_ATTRIBUTES) {
    wchar_t pDataDir[MAX_PATH] = {0};
    DWORD len = GetEnvironmentVariableW(L"ProgramData", pDataDir, MAX_PATH);
    if (len > 0) {
      std::wstring fallback = std::wstring(pDataDir) + L"\\ProtoIME";
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
  {
    std::wstring flag = _dllDir + L"\\proto_debug_enable.flag";
    if (GetFileAttributesW(flag.c_str()) != INVALID_FILE_ATTRIBUTES)
      set_log_level(LOG_DEBUG);
  }

  std::wstring dataPath = _dllDir + L"\\data";
  if (GetFileAttributesW((dataPath + L"\\pinyin_map.txt").c_str()) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxW(NULL, (L"找不到词库文件！\n请将 data 目录放在 DLL 同级目录或 %ProgramData%\\ProtoIME\\data 下。\n\nDLL 目录: " + _dllDir).c_str(),
                L"智能ABC - 错误", MB_OK | MB_ICONERROR);
    return;
  }

  std::wstring skinPath = _dllDir + L"\\res\\shadow.png";
  if (GetFileAttributesW(skinPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxW(NULL, (L"找不到皮肤文件！\n请将 res 目录放在 DLL 同级目录或 %ProgramData%\\ProtoIME\\res 下。\n\nDLL 目录: " + _dllDir).c_str(),
                L"智能ABC - 错误", MB_OK | MB_ICONERROR);
    return;
  }

  ProtoIME::SetDataDir(dataPath.c_str());
  ProtoIME::Initialize(g_hInst);
  ProtoIME::SetActive(true);
  write_log("TSF: ProtoIME initialized", LOG_INFO);

  if (ProtoIME::LoadSkinFromFile(skinPath.c_str(), _skin, 4, 4, 4, 4))
    ProtoIME::SetSkin(&_skin);
  std::wstring commonPath = _dllDir + L"\\res\\common.png";
  if (ProtoIME::LoadSkinFromFile(commonPath.c_str(), _settingsSkin, 2, 2, 2, 2))
    ProtoIME::SetSettingsSkin(&_settingsSkin);
  std::wstring btnPath = _dllDir + L"\\res\\button.png";
  if (ProtoIME::LoadSkinFromFile(btnPath.c_str(), _btnSkin, 2, 2, 2, 2))
    ProtoIME::SetBtnSkin(&_btnSkin);

  ProtoIME::SetBtnIcon(0, (_dllDir + L"\\res\\ABC_ICON.png").c_str());
  ProtoIME::SetBtnIcon(2, (_dllDir + L"\\res\\half.png").c_str());
  ProtoIME::SetBtnIcon(3, (_dllDir + L"\\res\\sign.png").c_str());
  ProtoIME::SetBtnIcon(4, (_dllDir + L"\\res\\keyboard.png").c_str());
  ProtoIME::SetModeIcon(0, (_dllDir + L"\\res\\capital.png").c_str());
  ProtoIME::SetModeIcon(1, (_dllDir + L"\\res\\english.png").c_str());
  ProtoIME::SetModeIcon(2, (_dllDir + L"\\res\\pinyin.png").c_str());
  ProtoIME::SetLockIcon((_dllDir + L"\\res\\ABC_ICON_GRAY.png").c_str());
  ProtoIME::SetSignEnIcon((_dllDir + L"\\res\\sign_en.png").c_str());

  ProtoIME::SetNavIcon(0, (_dllDir + L"\\res\\first_page.png").c_str());
  ProtoIME::SetNavIcon(1, (_dllDir + L"\\res\\last_page.png").c_str());
  ProtoIME::SetNavIcon(2, (_dllDir + L"\\res\\next_page.png").c_str());
  ProtoIME::SetNavIcon(3, (_dllDir + L"\\res\\prev_page.png").c_str());

  ITfDocumentMgr* pdm = nullptr;
  if (SUCCEEDED(_pThreadMgr->GetFocus(&pdm)) && pdm) {
    ProtoIME::SetFocused(true);
    pdm->Release();
  }

  _engineReady = true;
}

void TSF::_ShutdownEngine() {
  if (!_engineReady) return;
  ProtoIME::SetActive(false);
  ProtoIME::SetSkin(nullptr);
  ProtoIME::FreeSkin(_skin);
  ProtoIME::SetSettingsSkin(nullptr);
  ProtoIME::FreeSkin(_settingsSkin);
  ProtoIME::SetBtnSkin(nullptr);
  ProtoIME::FreeSkin(_btnSkin);
  ProtoIME::Shutdown();
  _engineReady = false;
  write_log("TSF: ProtoIME shutdown", LOG_INFO);
}

STDMETHODIMP TSF::OnSetThreadFocus() {
  if (_engineReady) ProtoIME::SetFocused(true);
  return S_OK;
}
STDMETHODIMP TSF::OnKillThreadFocus() {
  _AbortComposition();
  if (_engineReady) ProtoIME::SetFocused(false);
  return S_OK;
}

BOOL TSF::_InitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource))) return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink, (ITfThreadFocusSink*)this, &_dwThreadFocusSinkCookie))) return FALSE;
  return TRUE;
}

void TSF::_UninitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource))) return;
  if (FAILED(pSource->UnadviseSink(_dwThreadFocusSinkCookie))) return;
}

STDMETHODIMP TSF::OnActivated(REFCLSID clsid, REFGUID guidProfile, BOOL isActivated) {
  return S_OK;
}

STDAPI TSF::DoEditSession(TfEditCookie ec) {
  return S_OK;
}

STDAPI TSF::OnCompositionTerminated(TfEditCookie ecWrite,
                                          ITfComposition* pComposition) {
  return S_OK;
}

void TSF::_AbortComposition(bool clear) {
  if (_engineReady)
    ProtoIME::FlushPendingAndHideUI();
}
