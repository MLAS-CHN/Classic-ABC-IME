// proto_core.cpp - Thin coordinator: wires Engine + UI together.
#include "proto_core.h"
#include "proto_engine.h"
#include "proto_ui.h"
#include "../pinyin_file_io.h"
#include "../util.h"

bool ProtoIME::Initialize(HINSTANCE h) {
    ProtoIME::Engine::Init();
    return ProtoIME::UI::Init(h, 163, 26);
}

void ProtoIME::Shutdown() {
    ProtoIME::UI::Shutdown();
}

void ProtoIME::SetActive(bool a) {
    write_log("ProtoCore: SetActive(" + std::string(a ? "true" : "false") + ")", LOG_DEBUG);
    ProtoIME::Engine::SetActive(a);
    if (!a) {
        ProtoIME::UI::Show(false);
        ProtoIME::UI::ShowSettings(false);
        ProtoIME::UI::ShowCand(false);
    }
}

bool ProtoIME::IsActive() { return ProtoIME::Engine::IsActive(); }

void ProtoIME::SetFocused(bool f) {
    write_log("ProtoCore: SetFocused(" + std::string(f ? "true" : "false") + ") active=" + std::to_string(ProtoIME::Engine::IsActive()), LOG_DEBUG);
    if (f) {
        if (!ProtoIME::Engine::IsActive())
            ProtoIME::Engine::SetActive(true);
        ProtoIME::UI::ShowSettings(true);
    } else {
        ProtoIME::UI::ShowSettings(false);
        ProtoIME::Engine::SetActive(false);
        ProtoIME::UI::Show(false);
        ProtoIME::UI::ShowCand(false);
    }
}

bool ProtoIME::TestKeyDown(UINT vk) { return ProtoIME::Engine::TestKey(vk); }

bool ProtoIME::OnKeyDown(UINT vk) {
    bool eaten = ProtoIME::Engine::ProcessKey(vk);
    if (eaten) {
        ProtoIME::UI::Update();
        ProtoIME::UI::UpdateCand();
    }
    // Also hide windows if buffer was flushed (CapsLock, etc.)
    if (!eaten && ProtoIME::Engine::CompStr().empty())
        { ProtoIME::UI::Show(false); ProtoIME::UI::ShowCand(false); }
    ProtoIME::UI::RefreshSettings();
    return eaten;
}

bool ProtoIME::OnKeyUp(UINT vk) {
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        bool eaten = ProtoIME::Engine::ProcessShiftTap();
        ProtoIME::UI::Update();
        ProtoIME::UI::UpdateCand();
        ProtoIME::UI::RefreshSettings();
        return eaten;
    }
    return false;
}

const std::wstring& ProtoIME::GetCompositionString() { return ProtoIME::Engine::CompStr(); }

size_t      ProtoIME::GetCandidateCount()     { return ProtoIME::Engine::GetCandidateCount(); }
std::wstring ProtoIME::GetCandidateText(size_t i) { return ProtoIME::Engine::GetCandidateText(i); }
size_t      ProtoIME::GetCandidatePage()      { return ProtoIME::Engine::GetCandidatePage(); }
size_t      ProtoIME::GetTotalPages()         { return ProtoIME::Engine::GetTotalPages(); }
bool        ProtoIME::IsDelMode()            { return ProtoIME::Engine::IsDelMode(); }

void ProtoIME::GoFirstPage() { ProtoIME::Engine::GoFirstPage(); ProtoIME::UI::UpdateCand(); }
void ProtoIME::GoLastPage()  { ProtoIME::Engine::GoLastPage();  ProtoIME::UI::UpdateCand(); }
void ProtoIME::GoNextPage()  { ProtoIME::Engine::GoNextPage();  ProtoIME::UI::UpdateCand(); }
void ProtoIME::GoPrevPage()  { ProtoIME::Engine::GoPrevPage();  ProtoIME::UI::UpdateCand(); }

void ProtoIME::SetDataDir(const wchar_t* dir) {
    int n = WideCharToMultiByte(CP_UTF8, 0, dir, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return;
    std::string s((size_t)(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, dir, -1, &s[0], n, nullptr, nullptr);
    set_pinyin_data_dir(s);
}

// --- Skin wrappers ---
bool ProtoIME::LoadSkinFromFile(const wchar_t* path, NinePatchSkin& sk, int mL, int mT, int mR, int mB) {
    ProtoIME::UI::NinePatchSkin ui;
    if (!ProtoIME::UI::LoadSkin(path, ui, mL, mT, mR, mB)) return false;
    sk.hBmp = ui.hBmp; sk.srcW = ui.srcW; sk.srcH = ui.srcH;
    sk.marginL = ui.marginL; sk.marginT = ui.marginT;
    sk.marginR = ui.marginR; sk.marginB = ui.marginB;
    return true;
}

void ProtoIME::FreeSkin(NinePatchSkin& sk) {
    ProtoIME::UI::NinePatchSkin ui; ui.hBmp = sk.hBmp;
    ProtoIME::UI::FreeSkin(ui); sk.hBmp = nullptr;
}

static ProtoIME::UI::NinePatchSkin g_wrapSkin;
static ProtoIME::UI::NinePatchSkin g_wrapSettingsSkin;
static ProtoIME::UI::NinePatchSkin g_wrapBtnSkin;

static void copy_skin(ProtoIME::UI::NinePatchSkin& dst, const ProtoIME::NinePatchSkin& src) {
    dst.hBmp = src.hBmp; dst.srcW = src.srcW; dst.srcH = src.srcH;
    dst.marginL = src.marginL; dst.marginT = src.marginT;
    dst.marginR = src.marginR; dst.marginB = src.marginB;
}

void ProtoIME::SetSkin(const NinePatchSkin* sk) {
    if (sk) { copy_skin(g_wrapSkin, *sk); ProtoIME::UI::SetSkin(&g_wrapSkin); }
    else ProtoIME::UI::SetSkin(nullptr);
}

void ProtoIME::SetSettingsSkin(const NinePatchSkin* sk) {
    if (sk) { copy_skin(g_wrapSettingsSkin, *sk); ProtoIME::UI::SetSettingsSkin(&g_wrapSettingsSkin); }
    else ProtoIME::UI::SetSettingsSkin(nullptr);
}

void ProtoIME::SetBtnSkin(const NinePatchSkin* sk) {
    if (sk) { copy_skin(g_wrapBtnSkin, *sk); ProtoIME::UI::SetBtnSkin(&g_wrapBtnSkin); }
    else ProtoIME::UI::SetBtnSkin(nullptr);
}

bool ProtoIME::SetBtnIcon(int idx, const wchar_t* path) { return ProtoIME::UI::SetBtnIcon(idx, path); }

bool ProtoIME::SetModeIcon(int idx, const wchar_t* path) { return ProtoIME::UI::SetModeIcon(idx, path); }

bool ProtoIME::SetLockIcon(const wchar_t* path) { return ProtoIME::UI::SetLockIcon(path); }
bool ProtoIME::SetSignEnIcon(const wchar_t* path) { return ProtoIME::UI::SetSignEnIcon(path); }
bool ProtoIME::SetNavIcon(int idx, const wchar_t* path) { return ProtoIME::UI::SetNavIcon(idx, path); }

void ProtoIME::ToggleMode() {
    ProtoIME::Engine::ToggleChineseMode();
    ProtoIME::UI::Update();
    ProtoIME::UI::UpdateCand();
    ProtoIME::UI::RefreshSettings();
}

void ProtoIME::ToggleLock() {
    ProtoIME::Engine::ToggleLock();
    ProtoIME::UI::Update();
    ProtoIME::UI::UpdateCand();
    ProtoIME::UI::RefreshSettings();
}

bool ProtoIME::IsLocked() { return ProtoIME::Engine::IsLocked(); }

bool ProtoIME::FlushPendingAndHideUI() {
    bool flushed = ProtoIME::Engine::FlushPending();
    if (flushed) { ProtoIME::UI::Show(false); ProtoIME::UI::ShowCand(false); }
    return flushed;
}
