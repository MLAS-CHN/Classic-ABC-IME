// proto_core.cpp - Thin coordinator: wires Engine + UI together.
#include "proto_core.h"
#include "proto_engine.h"
#include "proto_ui.h"
#include "../pinyin_file_io.h"
#include "../util.h"

static void on_dict_ready();

bool ClassicABC::Initialize(HINSTANCE h) {
    ClassicABC::Engine::Init();
    set_dict_ready_callback(&on_dict_ready);
    return ClassicABC::UI::Init(h, 163, 26);
}
void ClassicABC::Shutdown() {
    wait_dict_cache_ready();  // ensure background cache build has finished
    ClassicABC::UI::Shutdown();
}

void ClassicABC::SetActive(bool a) {
    write_log("ProtoCore: SetActive(" + std::string(a ? "true" : "false") + ")", LOG_DEBUG);
    ClassicABC::Engine::SetActive(a);
    if (!a) {
        ClassicABC::UI::Show(false);
        ClassicABC::UI::ShowSettings(false);
        ClassicABC::UI::ShowCand(false);
    }
}

bool ClassicABC::IsActive() { return ClassicABC::Engine::IsActive(); }

void ClassicABC::SetFocused(bool f) {
    write_log("ProtoCore: SetFocused(" + std::string(f ? "true" : "false") + ") active=" + std::to_string(ClassicABC::Engine::IsActive()), LOG_DEBUG);
    if (f) {
        if (!ClassicABC::Engine::IsActive())
            ClassicABC::Engine::SetActive(true);
        ClassicABC::UI::ShowSettings(true);
    } else {
        ClassicABC::UI::ShowSettings(false);
        ClassicABC::Engine::SetActive(false);
        ClassicABC::UI::Show(false);
        ClassicABC::UI::ShowCand(false);
    }
}

bool ClassicABC::TestKeyDown(UINT vk) { return ClassicABC::Engine::TestKey(vk); }

bool ClassicABC::OnKeyDown(UINT vk) {
    bool eaten = ClassicABC::Engine::ProcessKey(vk);
    if (eaten) {
        ClassicABC::UI::Update();
        ClassicABC::UI::UpdateCand();
    }
    // Also hide windows if buffer was flushed (CapsLock, etc.)
    if (!eaten && ClassicABC::Engine::CompStr().empty())
        { ClassicABC::UI::Show(false); ClassicABC::UI::ShowCand(false); }
    ClassicABC::UI::RefreshSettings();
    return eaten;
}

bool ClassicABC::OnKeyUp(UINT vk) {
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        bool eaten = ClassicABC::Engine::ProcessShiftTap();
        ClassicABC::UI::Update();
        ClassicABC::UI::UpdateCand();
        ClassicABC::UI::RefreshSettings();
        return eaten;
    }
    return false;
}

// ---- text commit sink (TSF adapter) ----
static ClassicABC::CommitTextFn g_commit_fn = nullptr;
static void* g_commit_userdata = nullptr;

// Dictionary cache finished building on a background thread: notify the UI
// thread (candidate window) so the current candidate list can be refreshed.
static void on_dict_ready() {
    HWND hwnd = ClassicABC::UI::GetCandidateWindow();
    write_log("ProtoCore: on_dict_ready hwnd=" + std::to_string((uintptr_t)hwnd), LOG_INFO);
    if (hwnd) PostMessageW(hwnd, WM_USER + 5, 0, 0);
}

void ClassicABC::SetCommitTextFn(CommitTextFn fn, void* userdata) {
    g_commit_fn = fn;
    g_commit_userdata = userdata;
}

void ClassicABC::ClearCommitTextFn() {
    g_commit_fn = nullptr;
    g_commit_userdata = nullptr;
}

void ClassicABC::CommitText(const wchar_t* text, size_t len) {
    if (g_commit_fn) {
        g_commit_fn(text, len, g_commit_userdata);
        return;
    }
    ClassicABC::Engine::SendTextFallback(text, len);
}

const std::wstring& ClassicABC::GetCompositionString() { return ClassicABC::Engine::CompStr(); }
bool ClassicABC::IsChineseMode() { return ClassicABC::Engine::IsChineseMode(); }
bool ClassicABC::IsCapsLockActive() { return ClassicABC::Engine::IsCapsLockActive(); }

size_t      ClassicABC::GetCandidateCount()     { return ClassicABC::Engine::GetCandidateCount(); }
std::wstring ClassicABC::GetCandidateText(size_t i) { return ClassicABC::Engine::GetCandidateText(i); }
size_t      ClassicABC::GetCandidatePage()      { return ClassicABC::Engine::GetCandidatePage(); }
size_t      ClassicABC::GetTotalPages()         { return ClassicABC::Engine::GetTotalPages(); }
bool        ClassicABC::IsDelMode()            { return ClassicABC::Engine::IsDelMode(); }

void ClassicABC::GoFirstPage() { ClassicABC::Engine::GoFirstPage(); ClassicABC::UI::UpdateCand(); }
void ClassicABC::GoLastPage()  { ClassicABC::Engine::GoLastPage();  ClassicABC::UI::UpdateCand(); }
void ClassicABC::GoNextPage()  { ClassicABC::Engine::GoNextPage();  ClassicABC::UI::UpdateCand(); }
void ClassicABC::GoPrevPage()  { ClassicABC::Engine::GoPrevPage();  ClassicABC::UI::UpdateCand(); }
void ClassicABC::RefreshCandidates() {
    ClassicABC::Engine::RebuildCandidates();
    ClassicABC::UI::UpdateCand();
}

void ClassicABC::SetDataDir(const wchar_t* dir) {
    int n = WideCharToMultiByte(CP_UTF8, 0, dir, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return;
    std::string s((size_t)(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, dir, -1, &s[0], n, nullptr, nullptr);
    set_pinyin_data_dir(s);
}

// --- Skin wrappers ---
static ClassicABC::UI::BuiltinSkinId to_ui_skin_id(ClassicABC::BuiltinSkinId skin_id) {
    switch (skin_id) {
        case ClassicABC::BuiltinSkinId::Candidate:
            return ClassicABC::UI::BuiltinSkinId::Candidate;
        case ClassicABC::BuiltinSkinId::Settings:
            return ClassicABC::UI::BuiltinSkinId::Settings;
        case ClassicABC::BuiltinSkinId::Button:
            return ClassicABC::UI::BuiltinSkinId::Button;
    }
    return ClassicABC::UI::BuiltinSkinId::Candidate;
}

bool ClassicABC::LoadBuiltinSkin(BuiltinSkinId skin_id, NinePatchSkin& sk) {
    ClassicABC::UI::NinePatchSkin ui;
    if (!ClassicABC::UI::LoadBuiltinSkin(to_ui_skin_id(skin_id), ui)) return false;
    sk.hBmp = ui.hBmp; sk.srcW = ui.srcW; sk.srcH = ui.srcH;
    sk.marginL = ui.marginL; sk.marginT = ui.marginT;
    sk.marginR = ui.marginR; sk.marginB = ui.marginB;
    return true;
}

bool ClassicABC::LoadSkinFromFile(const wchar_t* path, NinePatchSkin& sk, int mL, int mT, int mR, int mB) {
    ClassicABC::UI::NinePatchSkin ui;
    if (!ClassicABC::UI::LoadSkin(path, ui, mL, mT, mR, mB)) return false;
    sk.hBmp = ui.hBmp; sk.srcW = ui.srcW; sk.srcH = ui.srcH;
    sk.marginL = ui.marginL; sk.marginT = ui.marginT;
    sk.marginR = ui.marginR; sk.marginB = ui.marginB;
    return true;
}

void ClassicABC::FreeSkin(NinePatchSkin& sk) {
    ClassicABC::UI::NinePatchSkin ui; ui.hBmp = sk.hBmp;
    ClassicABC::UI::FreeSkin(ui); sk.hBmp = nullptr;
}

static ClassicABC::UI::NinePatchSkin g_wrapSkin;
static ClassicABC::UI::NinePatchSkin g_wrapSettingsSkin;
static ClassicABC::UI::NinePatchSkin g_wrapBtnSkin;

static void copy_skin(ClassicABC::UI::NinePatchSkin& dst, const ClassicABC::NinePatchSkin& src) {
    dst.hBmp = src.hBmp; dst.srcW = src.srcW; dst.srcH = src.srcH;
    dst.marginL = src.marginL; dst.marginT = src.marginT;
    dst.marginR = src.marginR; dst.marginB = src.marginB;
}

void ClassicABC::SetSkin(const NinePatchSkin* sk) {
    if (sk) { copy_skin(g_wrapSkin, *sk); ClassicABC::UI::SetSkin(&g_wrapSkin); }
    else ClassicABC::UI::SetSkin(nullptr);
}

void ClassicABC::SetSettingsSkin(const NinePatchSkin* sk) {
    if (sk) { copy_skin(g_wrapSettingsSkin, *sk); ClassicABC::UI::SetSettingsSkin(&g_wrapSettingsSkin); }
    else ClassicABC::UI::SetSettingsSkin(nullptr);
}

void ClassicABC::SetBtnSkin(const NinePatchSkin* sk) {
    if (sk) { copy_skin(g_wrapBtnSkin, *sk); ClassicABC::UI::SetBtnSkin(&g_wrapBtnSkin); }
    else ClassicABC::UI::SetBtnSkin(nullptr);
}

bool ClassicABC::SetBtnIcon(int idx, const wchar_t* path) { return ClassicABC::UI::SetBtnIcon(idx, path); }

bool ClassicABC::SetModeIcon(int idx, const wchar_t* path) { return ClassicABC::UI::SetModeIcon(idx, path); }

bool ClassicABC::SetLockIcon(const wchar_t* path) { return ClassicABC::UI::SetLockIcon(path); }
bool ClassicABC::SetSignEnIcon(const wchar_t* path) { return ClassicABC::UI::SetSignEnIcon(path); }
bool ClassicABC::SetNavIcon(int idx, const wchar_t* path) { return ClassicABC::UI::SetNavIcon(idx, path); }

void ClassicABC::ToggleMode() {
    ClassicABC::Engine::ToggleChineseMode();
    ClassicABC::UI::Update();
    ClassicABC::UI::UpdateCand();
    ClassicABC::UI::RefreshSettings();
}

void ClassicABC::ToggleLock() {
    ClassicABC::Engine::ToggleLock();
    ClassicABC::UI::Update();
    ClassicABC::UI::UpdateCand();
    ClassicABC::UI::RefreshSettings();
}

bool ClassicABC::IsLocked() { return ClassicABC::Engine::IsLocked(); }

bool ClassicABC::FlushPendingAndHideUI() {
    bool flushed = ClassicABC::Engine::FlushPending();
    if (flushed) { ClassicABC::UI::Show(false); ClassicABC::UI::ShowCand(false); }
    return flushed;
}
