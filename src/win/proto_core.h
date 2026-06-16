// proto_core.h - Thin coordinator: combines Engine (input logic) + UI (candidate window).
// Public API for TSF adapter (proto.cpp).
#pragma once
#include <windows.h>
#include <string>

namespace ProtoIME {

// Re-export skin type
struct NinePatchSkin {
    HBITMAP hBmp = nullptr;
    int srcW = 0, srcH = 0;
    int marginL = 0, marginT = 0, marginR = 0, marginB = 0;
};

bool Initialize(HINSTANCE hInstance);
void Shutdown();
void SetActive(bool active);
bool IsActive();
void SetFocused(bool focused);  // TSF OnSetFocus: current window using Proto?

bool TestKeyDown(UINT vk);
bool OnKeyDown(UINT vk);
bool OnKeyUp(UINT vk);

const std::wstring& GetCompositionString();

// Skin
bool LoadSkinFromFile(const wchar_t* path, NinePatchSkin& skin,
                      int mL, int mT, int mR, int mB);
void FreeSkin(NinePatchSkin& skin);
void SetSkin(const NinePatchSkin* skin);
void SetSettingsSkin(const NinePatchSkin* skin);
void SetBtnSkin(const NinePatchSkin* skin);  // button.png
bool SetBtnIcon(int idx, const wchar_t* path);  // PNG icon for button 0~4

} // namespace ProtoIME
