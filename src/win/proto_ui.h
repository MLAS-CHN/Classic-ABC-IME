// proto_ui.h - Candidate window UI + Settings bar: GDI/GDI+, 9-patch, font, positioning.
// Reads composition string from Engine::CompStr() for painting.
#pragma once
#include <windows.h>

namespace ProtoIME {
namespace UI {

struct NinePatchSkin {
    HBITMAP hBmp = nullptr;
    int srcW = 0, srcH = 0;
    int marginL = 0, marginT = 0, marginR = 0, marginB = 0;
};

bool Init(HINSTANCE hInst, int candW, int candH);
void Shutdown();

// Candidate window
void Show(bool visible);
void Update();  // reposition near caret + repaint

// Settings bar (appears when IME is active, follows no caret)
void ShowSettings(bool visible);
void SetSettingsSkin(const NinePatchSkin* skin);
void SetBtnSkin(const NinePatchSkin* skin);  // button.png for settings bar buttons

// Candidate list (test: shows 10 hardcoded rows when composition length > 1)
void ShowCand(bool visible);
void UpdateCand();  // reposition below input window + repaint

// Settings bar button icons (PNG with alpha, drawn over button 9-patch)
bool SetBtnIcon(int idx, const wchar_t* path);  // idx 0~4
bool SetModeIcon(int idx, const wchar_t* path); // idx 0(capital) 1(english) 2(pinyin)
void RefreshSettings();
bool SetLockIcon(const wchar_t* path);
bool SetSignEnIcon(const wchar_t* path);
bool SetNavIcon(int idx, const wchar_t* path);  // idx 0=first 1=last 2=next 3=prev

// Skin helpers
void SetSkin(const NinePatchSkin* skin);
bool LoadSkin(const wchar_t* path, NinePatchSkin& skin,
              int mL, int mT, int mR, int mB);
void FreeSkin(NinePatchSkin& skin);

} // UI
} // ProtoIME
