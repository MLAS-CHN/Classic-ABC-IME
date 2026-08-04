#include "stdafx.h"
#include "TSF.h"
#include "../util.h"
#include "../proto_engine.h"

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
  ClassicABC::ClearCommitTextFn();
  _ShutdownEngine();
  _InitTextEditSink(com_ptr<ITfDocumentMgr>());
  _UninitThreadMgrEventSink();
  _UninitKeyEventSink();
  _UninitCompartment();
  _UninitThreadFocusSink();
  _pThreadMgr = NULL;
  _tfClientId = TF_CLIENTID_NULL;
  _pActiveContext = NULL;
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
  ClassicABC::SetCommitTextFn(&TSF::_CommitTextCallback, this);
  return S_OK;

ExitError:
  Deactivate();
  return E_FAIL;
}

void TSF::_SetActiveContext(ITfContext* pContext) {
  _pActiveContext = pContext;
}

void TSF::_CommitTextCallback(const wchar_t* text, size_t len, void* userdata) {
  if (text == nullptr || userdata == nullptr) return;
  TSF* self = static_cast<TSF*>(userdata);
  self->_CommitText(text, len);
}

void TSF::_CommitText(const wchar_t* text, size_t len) {
  if (text == nullptr || len == 0 || _pActiveContext == nullptr) return;

  _commit_text.assign(text, len);
  _commit_pending = true;

  HRESULT hr = E_FAIL;
  _pActiveContext->RequestEditSession(_tfClientId, (ITfEditSession*)this,
                                      TF_ES_SYNC | TF_ES_READWRITE, &hr);
  if (FAILED(hr)) {
    // Edit session could not run synchronously (or SetText failed): feed the
    // text through the SendInput path so nothing is lost.
    _commit_pending = false;
    _commit_text.clear();
    ClassicABC::Engine::SendTextFallback(text, len);
  }
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
  {
    std::wstring flag = _dllDir + L"\\proto_debug_enable.flag";
    if (GetFileAttributesW(flag.c_str()) != INVALID_FILE_ATTRIBUTES)
      set_log_level(LOG_DEBUG);
  }

  std::wstring dataPath = _dllDir + L"\\data";
  if (GetFileAttributesW((dataPath + L"\\pinyin_map.txt").c_str()) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxW(NULL, (L"找不到词库文件！\n请将 data 目录放在 DLL 同级目录或 %ProgramData%\\ClassicABC\\data 下。\n\nDLL 目录: " + _dllDir).c_str(),
                L"经典ABC - 错误", MB_OK | MB_ICONERROR);
    return;
  }

  ClassicABC::SetDataDir(dataPath.c_str());
  ClassicABC::Initialize(g_hInst);
  ClassicABC::SetActive(true);
  write_log("TSF: ClassicABC initialized", LOG_INFO);

  if (ClassicABC::LoadBuiltinSkin(ClassicABC::BuiltinSkinId::Candidate, _skin))
    ClassicABC::SetSkin(&_skin);
  if (ClassicABC::LoadBuiltinSkin(ClassicABC::BuiltinSkinId::Settings, _settingsSkin))
    ClassicABC::SetSettingsSkin(&_settingsSkin);
  if (ClassicABC::LoadBuiltinSkin(ClassicABC::BuiltinSkinId::Button, _btnSkin))
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

  ClassicABC::SetNavIcon(0, (_dllDir + L"\\res\\first_page.png").c_str());
  ClassicABC::SetNavIcon(1, (_dllDir + L"\\res\\last_page.png").c_str());
  ClassicABC::SetNavIcon(2, (_dllDir + L"\\res\\next_page.png").c_str());
  ClassicABC::SetNavIcon(3, (_dllDir + L"\\res\\prev_page.png").c_str());

  ITfDocumentMgr* pdm = nullptr;
  if (SUCCEEDED(_pThreadMgr->GetFocus(&pdm)) && pdm) {
    ClassicABC::SetFocused(true);
    pdm->Release();
  }

  _engineReady = true;
}

void TSF::_ShutdownEngine() {
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
  write_log("TSF: ClassicABC shutdown", LOG_INFO);
}

STDMETHODIMP TSF::OnSetThreadFocus() {
  if (_engineReady) ClassicABC::SetFocused(true);
  return S_OK;
}
STDMETHODIMP TSF::OnKillThreadFocus() {
  _AbortComposition();
  if (_engineReady) ClassicABC::SetFocused(false);
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
  if (!_commit_pending || _pActiveContext == nullptr) return E_FAIL;
  _commit_pending = false;
  if (_commit_text.empty()) return S_OK;

  TF_SELECTION sel;
  ULONG cFetched = 0;
  HRESULT hr = _pActiveContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched);
  if (FAILED(hr) || cFetched == 0) {
    _commit_text.clear();
    return E_FAIL;
  }

  com_ptr<ITfRange> pRange = sel.range;
  hr = pRange->SetText(ec, 0, _commit_text.c_str(), (LONG)_commit_text.size());
  if (SUCCEEDED(hr)) {
    TF_SELECTION newSel;
    newSel.range = pRange;
    newSel.style.ase = TF_AE_END;
    newSel.style.fInterimChar = FALSE;
    hr = _pActiveContext->SetSelection(ec, 1, &newSel);
  }
  sel.range->Release();

  _commit_text.clear();
  return hr;
}

STDAPI TSF::OnCompositionTerminated(TfEditCookie ecWrite,
                                          ITfComposition* pComposition) {
  return S_OK;
}

void TSF::_AbortComposition(bool clear) {
  if (_engineReady)
    ClassicABC::FlushPendingAndHideUI();
}
