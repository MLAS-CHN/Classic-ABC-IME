// proto.cpp - TSF (Text Services Framework) adapter.
// COM/TSF boilerplate only. Key handling & UI delegated to proto_core.
#include <windows.h>
#include <string>
#include <new>
#include <initguid.h>
#include <msctf.h>
#include <ctfutb.h>
#include "proto_core.h"
#include "../util.h"

// ========== GUIDs ==========
// {F4E5D6C7-B8A9-40BC-ABCD-EF1234567890}
DEFINE_GUID(CLSID_Proto, 0xF4E5D6C7, 0xB8A9, 0x40BC, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90);
// {E5F6A7B8-C9D0-41CD-ABCD-EF1234567890}
DEFINE_GUID(GUID_ProtoPrf, 0xE5F6A7B8, 0xC9D0, 0x41CD, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90);
const LANGID kLang = 0x0804;
static HINSTANCE g_hinst;
static ProtoIME::NinePatchSkin g_skin;        // candidate window (shadow)
static ProtoIME::NinePatchSkin g_settingsSkin; // settings bar (common)
static ProtoIME::NinePatchSkin g_btnSkin;      // settings bar buttons (button)

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
        case VK_OEM_7:   return "'";
        case VK_OEM_PLUS:return "=";
        case VK_OEM_MINUS:return "-";
        case VK_OEM_5:   return "\\";
        case VK_OEM_COMMA: return ",";
        case VK_OEM_PERIOD:return ".";
        case VK_OEM_1:   return ";";
        case VK_OEM_2:   return "/";
        default: {
            char buf[16];
            wsprintfA(buf, "0x%02X", vk);
            return buf;
        }
    }
}

// ========== LangBar Button ==========
class ProtoButton : public ITfLangBarItemButton {
    LONG m_ref = 1;
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER; *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) || IsEqualIID(riid, IID_ITfLangBarItemButton))
            { *ppv = (ITfLangBarItemButton*)this; AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&m_ref); if (r == 0) delete this; return r; }
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* p) override {
        if (p) { p->clsidService = CLSID_Proto; p->guidItem = GUID_ProtoPrf; p->dwStyle = TF_LBI_STYLE_BTN_BUTTON; p->ulSort = 0; wcscpy_s(p->szDescription, L"Proto"); }
        return S_OK;
    }
    STDMETHODIMP GetStatus(DWORD* s) override { *s = 0; return S_OK; }
    STDMETHODIMP Show(BOOL) override { return S_OK; }
    STDMETHODIMP GetTooltipString(BSTR* s) override { *s = SysAllocString(L"Proto IME"); return S_OK; }
    STDMETHODIMP OnClick(TfLBIClick, POINT, const RECT*) override { return S_OK; }
    STDMETHODIMP GetText(BSTR* s) override { *s = SysAllocString(L"Proto"); return S_OK; }
    STDMETHODIMP GetIcon(HICON* i) override { *i = nullptr; return E_NOTIMPL; }
    STDMETHODIMP InitMenu(ITfMenu*) override { return E_NOTIMPL; }
    STDMETHODIMP OnMenuSelect(UINT) override { return S_OK; }
};

// ========== Text Service (thin wrapper around ProtoIME) ==========
class ProtoService : public ITfTextInputProcessor, public ITfKeyEventSink, public ITfThreadMgrEventSink {
    LONG m_ref = 1;
    ITfThreadMgr* m_tm = nullptr;
    TfClientId m_id = 0;
    DWORD m_cookie = TF_INVALID_COOKIE;

public:
    ~ProtoService() { ProtoIME::Shutdown(); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER; *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
            { *ppv = (ITfTextInputProcessor*)this; AddRef(); return S_OK; }
        if (IsEqualIID(riid, IID_ITfKeyEventSink))
            { *ppv = (ITfKeyEventSink*)this; AddRef(); return S_OK; }
        if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
            { *ppv = (ITfThreadMgrEventSink*)this; AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&m_ref); if (r == 0) delete this; return r; }

    // --- ITfTextInputProcessor ---
    STDMETHODIMP Activate(ITfThreadMgr* tm, TfClientId id) override {
        m_tm = tm; m_id = id; tm->AddRef();

        // Set data dir relative to DLL
        wchar_t dllDir[MAX_PATH];
        GetModuleFileNameW(g_hinst, dllDir, MAX_PATH);
        *wcsrchr(dllDir, L'\\') = L'\0';

        // Init logger in DLL directory
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, dllDir, -1, nullptr, 0, nullptr, nullptr);
            if (n > 0) {
                std::string dir((size_t)(n - 1), '\0');
                WideCharToMultiByte(CP_UTF8, 0, dllDir, -1, &dir[0], n, nullptr, nullptr);
                init_logger_with_dir(dir);
            }
        }
        // Check debug flag: create proto_debug_enable.flag in DLL dir to enable DEBUG logs
        {
            std::wstring flag = std::wstring(dllDir) + L"\\proto_debug_enable.flag";
            if (GetFileAttributesW(flag.c_str()) != INVALID_FILE_ATTRIBUTES)
                set_log_level(DEBUG);
        }
        write_log("Proto: Activate() called, TfClientId=" + std::to_string(id), INFO);

        ProtoIME::SetDataDir((std::wstring(dllDir) + L"\\data").c_str());

        ProtoIME::Initialize(g_hinst);
        ProtoIME::SetActive(true);

        write_log("Proto: Engine+UI initialized, active=true", DEBUG);

        // Load 9-patch skin (shadow.png) relative to DLL
        std::wstring skinPath = std::wstring(dllDir) + L"\\res\\shadow.png";
        if (ProtoIME::LoadSkinFromFile(skinPath.c_str(), g_skin, 4, 4, 4, 4)) {
            ProtoIME::SetSkin(&g_skin);
        }
        std::wstring commonPath = std::wstring(dllDir) + L"\\res\\common.png";
        if (ProtoIME::LoadSkinFromFile(commonPath.c_str(), g_settingsSkin, 2, 2, 2, 2)) {
            ProtoIME::SetSettingsSkin(&g_settingsSkin);
        }
        std::wstring btnPath = std::wstring(dllDir) + L"\\res\\button.png";
        if (ProtoIME::LoadSkinFromFile(btnPath.c_str(), g_btnSkin, 2, 2, 2, 2)) {
            ProtoIME::SetBtnSkin(&g_btnSkin);
        }
        std::wstring iconPath = std::wstring(dllDir) + L"\\res\\ABC_ICON.png";
        ProtoIME::SetBtnIcon(0, iconPath.c_str());
        ProtoIME::SetBtnIcon(2, (std::wstring(dllDir) + L"\\res\\half.png").c_str());
        ProtoIME::SetBtnIcon(3, (std::wstring(dllDir) + L"\\res\\sign.png").c_str());
        ProtoIME::SetBtnIcon(4, (std::wstring(dllDir) + L"\\res\\keyboard.png").c_str());

        // Mode icons for button 1 (40x20)
        ProtoIME::SetModeIcon(0, (std::wstring(dllDir) + L"\\res\\capital.png").c_str());
        ProtoIME::SetModeIcon(1, (std::wstring(dllDir) + L"\\res\\english.png").c_str());
        ProtoIME::SetModeIcon(2, (std::wstring(dllDir) + L"\\res\\pinyin.png").c_str());

        // Lock mode icon for button 0 (shown when locked)
        ProtoIME::SetLockIcon((std::wstring(dllDir) + L"\\res\\ABC_ICON_GRAY.png").c_str());

        ITfSource* src = nullptr;
        if (SUCCEEDED(tm->QueryInterface(IID_ITfSource, (void**)&src))) {
            src->AdviseSink(IID_ITfThreadMgrEventSink, (ITfThreadMgrEventSink*)this, &m_cookie); src->Release();
        }
        ITfKeystrokeMgr* km = nullptr;
        if (SUCCEEDED(tm->QueryInterface(IID_ITfKeystrokeMgr, (void**)&km))) {
            km->AdviseKeyEventSink(id, (ITfKeyEventSink*)this, TRUE); km->Release();
        }
        ITfLangBarItemMgr* lb = nullptr;
        if (SUCCEEDED(tm->QueryInterface(IID_ITfLangBarItemMgr, (void**)&lb))) {
            auto* btn = new (std::nothrow) ProtoButton();
            if (btn) { lb->AddItem(btn); btn->Release(); } lb->Release();
        }

        // Check if we already have document focus (OnSetFocus won't fire on activate)
        ITfDocumentMgr* pdm = nullptr;
        if (SUCCEEDED(tm->GetFocus(&pdm)) && pdm) {
            ProtoIME::SetFocused(true);
            pdm->Release();
        }

        return S_OK;
    }
    STDMETHODIMP Deactivate() override {
        write_log("Proto: Deactivate() called", INFO);
        // Unadvise sinks before releasing thread manager
        if (m_tm) {
            ITfKeystrokeMgr* km = nullptr;
            if (SUCCEEDED(m_tm->QueryInterface(IID_ITfKeystrokeMgr, (void**)&km))) {
                km->UnadviseKeyEventSink(m_id); km->Release();
            }
            ITfSource* src = nullptr;
            if (SUCCEEDED(m_tm->QueryInterface(IID_ITfSource, (void**)&src))) {
                if (m_cookie != TF_INVALID_COOKIE) {
                    src->UnadviseSink(m_cookie);
                    m_cookie = TF_INVALID_COOKIE;
                }
                src->Release();
            }
            m_tm->Release(); m_tm = nullptr;
        }
        ProtoIME::SetActive(false);
        ProtoIME::SetSkin(nullptr);
        ProtoIME::FreeSkin(g_skin);
        ProtoIME::SetSettingsSkin(nullptr);
        ProtoIME::FreeSkin(g_settingsSkin);
        ProtoIME::SetBtnSkin(nullptr);
        ProtoIME::FreeSkin(g_btnSkin);
        ProtoIME::Shutdown();
        return S_OK;
    }

    // --- ITfKeyEventSink ---
    STDMETHODIMP OnTestKeyDown(ITfContext*, WPARAM w, LPARAM, BOOL* e) override {
        ProtoIME::SetFocused(true);
        bool claimed = ProtoIME::TestKeyDown((UINT)w);
        *e = claimed ? TRUE : FALSE;
        write_log("Proto: OnTestKeyDown vk=" + vk_name((UINT)w) + " claimed=" + (claimed ? "YES" : "no"), DEBUG);
        return S_OK;
    }
    STDMETHODIMP OnKeyDown(ITfContext*, WPARAM w, LPARAM, BOOL* e) override {
        ProtoIME::SetFocused(true);
        bool eaten = ProtoIME::OnKeyDown((UINT)w);
        *e = eaten ? TRUE : FALSE;
        write_log("Proto: OnKeyDown vk=" + vk_name((UINT)w) + " eaten=" + (eaten ? "YES" : "no"), DEBUG);
        return S_OK;
    }
    STDMETHODIMP OnKeyUp(ITfContext*, WPARAM w, LPARAM, BOOL* e) override {
        bool eaten = ProtoIME::OnKeyUp((UINT)w);
        *e = eaten ? TRUE : FALSE;
        if (eaten) write_log("Proto: OnKeyUp vk=" + vk_name((UINT)w) + " eaten=YES (Shift tap)", DEBUG);
        return S_OK;
    }
    STDMETHODIMP OnTestKeyUp(ITfContext*, WPARAM w, LPARAM, BOOL* e) override {
        UINT vk = (UINT)w;
        if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)
            { *e = TRUE; return S_OK; }  // claim Shift release for tap detection
        *e = FALSE; return S_OK;
    }
    STDMETHODIMP OnSetFocus(BOOL fForeground) override {
        write_log("Proto: ITfKeyEventSink::OnSetFocus fg=" + std::to_string(fForeground), DEBUG);
        ProtoIME::SetFocused(fForeground != FALSE);
        return S_OK;
    }
    STDMETHODIMP OnPreservedKey(ITfContext*, REFGUID, BOOL* e) override { *e = FALSE; return S_OK; }

    // --- ITfThreadMgrEventSink ---
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr*) override {
        write_log("Proto: ITfThreadMgrEventSink::OnSetFocus focused=" + std::to_string(pdimFocus != nullptr), DEBUG);
        ProtoIME::SetFocused(pdimFocus != nullptr);
        return S_OK;
    }
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext*) override { return S_OK; }
};

// ========== Class Factory ==========
class ProtoFactory : public IClassFactory { LONG m_ref = 1;
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) { *ppv = (IClassFactory*)this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&m_ref); if (r == 0) delete this; return r; }
    STDMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv) override {
        if (pOuter) return CLASS_E_NOAGGREGATION;
        auto* s = new (std::nothrow) ProtoService(); if (!s) return E_OUTOFMEMORY;
        HRESULT hr = s->QueryInterface(riid, ppv); s->Release(); return hr;
    }
    STDMETHODIMP LockServer(BOOL) override { return S_OK; }
};

// ========== Registration ==========
static BOOL regServer() {
    wchar_t path[MAX_PATH]; GetModuleFileNameW(g_hinst, path, MAX_PATH);
    wchar_t k[256]; wsprintfW(k, L"CLSID\\{F4E5D6C7-B8A9-40BC-ABCD-EF1234567890}");
    HKEY h; if (RegCreateKeyExW(HKEY_CLASSES_ROOT, k, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &h, nullptr) != ERROR_SUCCESS) return FALSE;
    RegSetValueExW(h, nullptr, 0, REG_SZ, (BYTE*)L"Proto IME", 18); RegCloseKey(h);
    wsprintfW(k, L"CLSID\\{F4E5D6C7-B8A9-40BC-ABCD-EF1234567890}\\InprocServer32");
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, k, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &h, nullptr) != ERROR_SUCCESS) return FALSE;
    RegSetValueExW(h, nullptr, 0, REG_SZ, (BYTE*)path, (DWORD)(wcslen(path) + 1) * 2);
    RegSetValueExW(h, L"ThreadingModel", 0, REG_SZ, (BYTE*)L"Apartment", 20); RegCloseKey(h);
    return TRUE;
}

static BOOL regProfiles() {
    ITfInputProcessorProfiles* p = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&p))) return FALSE;
    wchar_t path[MAX_PATH]; GetModuleFileNameW(g_hinst, path, MAX_PATH);
    HRESULT hr = p->Register(CLSID_Proto);
    if (hr == TF_E_ALREADY_EXISTS) hr = S_OK;
    if (SUCCEEDED(hr)) {
        hr = p->AddLanguageProfile(CLSID_Proto, kLang, GUID_ProtoPrf, L"Proto IME", 9, path, (ULONG)wcslen(path), 0);
        if (hr == TF_E_ALREADY_EXISTS) hr = S_OK;
        if (SUCCEEDED(hr)) p->EnableLanguageProfile(CLSID_Proto, kLang, GUID_ProtoPrf, TRUE);
    }
    p->Release(); return SUCCEEDED(hr);
}

static BOOL regCats() {
    ITfCategoryMgr* c = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&c))) return FALSE;
    c->RegisterCategory(CLSID_Proto, GUID_TFCAT_TIP_KEYBOARD, CLSID_Proto);
    c->Release(); return TRUE;
}

// ========== DLL Exports ==========
BOOL APIENTRY DllMain(HINSTANCE h, DWORD r, LPVOID) { if (r == DLL_PROCESS_ATTACH) { g_hinst = h; DisableThreadLibraryCalls(h); } return TRUE; }
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!IsEqualCLSID(rclsid, CLSID_Proto)) return CLASS_E_CLASSNOTAVAILABLE;
    auto* f = new (std::nothrow) ProtoFactory(); if (!f) return E_OUTOFMEMORY;
    HRESULT hr = f->QueryInterface(riid, ppv); f->Release(); return hr;
}
STDAPI DllCanUnloadNow() { return S_FALSE; }
STDAPI DllRegisterServer() {
    HRESULT hr = CoInitialize(nullptr); if (FAILED(hr)) return E_FAIL;
    if (!regServer() || !regProfiles() || !regCats()) { CoUninitialize(); return E_FAIL; }
    CoUninitialize(); return S_OK;
}
STDAPI DllUnregisterServer() {
    HRESULT hr = CoInitialize(nullptr);
    if (SUCCEEDED(hr)) {
        ITfInputProcessorProfiles* p = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&p))) {
            p->EnableLanguageProfile(CLSID_Proto, kLang, GUID_ProtoPrf, FALSE);
            p->RemoveLanguageProfile(CLSID_Proto, kLang, GUID_ProtoPrf);
            p->Unregister(CLSID_Proto); p->Release();
        }
        ITfCategoryMgr* c = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&c))) {
            c->UnregisterCategory(CLSID_Proto, GUID_TFCAT_TIP_KEYBOARD, CLSID_Proto); c->Release();
        }
        CoUninitialize();
    }
    // Remove COM registration
    wchar_t k[256]; wsprintfW(k, L"CLSID\\{F4E5D6C7-B8A9-40BC-ABCD-EF1234567890}");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, k);
    return S_OK;
}
