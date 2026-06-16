// proto_core.cpp - Thin coordinator: wires Engine + UI together.
#include "proto_core.h"
#include "proto_engine.h"
#include "proto_ui.h"

bool ProtoIME::Initialize(HINSTANCE h) {
    ProtoIME::Engine::Init();
    return ProtoIME::UI::Init(h, 163, 26);
}

void ProtoIME::Shutdown() {
    ProtoIME::UI::Shutdown();
    ProtoIME::Engine::Init();  // reset
}

void ProtoIME::SetActive(bool a) {
    ProtoIME::Engine::SetActive(a);
    if (!a) {
        ProtoIME::UI::Show(false);
        ProtoIME::UI::ShowSettings(false);
        ProtoIME::UI::ShowCand(false);
    }
}

bool ProtoIME::IsActive() { return ProtoIME::Engine::IsActive(); }

void ProtoIME::SetFocused(bool focused) {
    if (focused) {
        // Re-activate engine if it was shut down by a previous unfocus
        if (!ProtoIME::Engine::IsActive())
            ProtoIME::Engine::SetActive(true);
        ProtoIME::UI::ShowSettings(true);
    } else {
        ProtoIME::UI::ShowSettings(false);
        // When settings bar disappears, the "program" has closed.
        // Clear engine state and hide related windows.
        ProtoIME::Engine::SetActive(false);
        ProtoIME::UI::Show(false);
        ProtoIME::UI::ShowCand(false);
    }
}

bool ProtoIME::TestKeyDown(UINT vk) { return ProtoIME::Engine::TestKey(vk); }

bool ProtoIME::OnKeyDown(UINT vk) {
    if (ProtoIME::Engine::ProcessKey(vk)) {
        ProtoIME::UI::Update();
        ProtoIME::UI::UpdateCand();
        return true;
    }
    return false;
}

bool ProtoIME::OnKeyUp(UINT vk) { (void)vk; return false; }

const std::wstring& ProtoIME::GetCompositionString() { return ProtoIME::Engine::CompStr(); }

bool ProtoIME::LoadSkinFromFile(const wchar_t* path, ProtoIME::NinePatchSkin& skin,
                                 int mL, int mT, int mR, int mB) {
    ProtoIME::UI::NinePatchSkin uiSkin;
    if (!ProtoIME::UI::LoadSkin(path, uiSkin, mL, mT, mR, mB)) return false;
    skin.hBmp = uiSkin.hBmp;
    skin.srcW = uiSkin.srcW;
    skin.srcH = uiSkin.srcH;
    skin.marginL = uiSkin.marginL;
    skin.marginT = uiSkin.marginT;
    skin.marginR = uiSkin.marginR;
    skin.marginB = uiSkin.marginB;
    return true;
}

void ProtoIME::FreeSkin(ProtoIME::NinePatchSkin& skin) {
    ProtoIME::UI::NinePatchSkin uiSkin;
    uiSkin.hBmp = skin.hBmp;
    ProtoIME::UI::FreeSkin(uiSkin);
    skin.hBmp = nullptr;
}

void ProtoIME::SetSkin(const ProtoIME::NinePatchSkin* skin) {
    if (skin) {
        static ProtoIME::UI::NinePatchSkin uiSkin;
        uiSkin.hBmp = skin->hBmp;
        uiSkin.srcW = skin->srcW;
        uiSkin.srcH = skin->srcH;
        uiSkin.marginL = skin->marginL;
        uiSkin.marginT = skin->marginT;
        uiSkin.marginR = skin->marginR;
        uiSkin.marginB = skin->marginB;
        ProtoIME::UI::SetSkin(&uiSkin);
    } else {
        ProtoIME::UI::SetSkin(nullptr);
    }
}

void ProtoIME::SetSettingsSkin(const ProtoIME::NinePatchSkin* skin) {
    if (skin) {
        static ProtoIME::UI::NinePatchSkin uiSkin;
        uiSkin.hBmp = skin->hBmp;
        uiSkin.srcW = skin->srcW;
        uiSkin.srcH = skin->srcH;
        uiSkin.marginL = skin->marginL;
        uiSkin.marginT = skin->marginT;
        uiSkin.marginR = skin->marginR;
        uiSkin.marginB = skin->marginB;
        ProtoIME::UI::SetSettingsSkin(&uiSkin);
    } else {
        ProtoIME::UI::SetSettingsSkin(nullptr);
    }
}

void ProtoIME::SetBtnSkin(const ProtoIME::NinePatchSkin* skin) {
    if (skin) {
        static ProtoIME::UI::NinePatchSkin uiSkin;
        uiSkin.hBmp = skin->hBmp;
        uiSkin.srcW = skin->srcW;
        uiSkin.srcH = skin->srcH;
        uiSkin.marginL = skin->marginL;
        uiSkin.marginT = skin->marginT;
        uiSkin.marginR = skin->marginR;
        uiSkin.marginB = skin->marginB;
        ProtoIME::UI::SetBtnSkin(&uiSkin);
    } else {
        ProtoIME::UI::SetBtnSkin(nullptr);
    }
}

bool ProtoIME::SetBtnIcon(int idx, const wchar_t* path) {
    return ProtoIME::UI::SetBtnIcon(idx, path);
}
