// proto_core.h - Thin coordinator: Engine + UI. Public API for TSF adapter.
#pragma once
#include <windows.h>
#include <string>

namespace ClassicABC {

enum class BuiltinSkinId {
    Candidate,
    Settings,
    Button
};

struct NinePatchSkin {
    HBITMAP hBmp = nullptr;
    int srcW = 0, srcH = 0;
    int marginL = 0, marginT = 0, marginR = 0, marginB = 0;
};

bool Initialize(HINSTANCE hInstance);
void Shutdown();
void SetActive(bool active);
bool IsActive();
void SetFocused(bool focused);

bool TestKeyDown(UINT vk);
bool OnKeyDown(UINT vk);
bool OnKeyUp(UINT vk);

// Text commit sink: called instead of SendInput when a TSF adapter is
// available. Set to nullptr to fall back to SendInput (IMM32 path).
using CommitTextFn = void (*)(const wchar_t* text, size_t len, void* userdata);
void SetCommitTextFn(CommitTextFn fn, void* userdata);
void ClearCommitTextFn();
void CommitText(const wchar_t* text, size_t len);

const std::wstring& GetCompositionString();
bool IsChineseMode();
bool IsCapsLockActive();

// Candidate access
size_t GetCandidateCount();
std::wstring GetCandidateText(size_t i);
size_t GetCandidatePage();
size_t GetTotalPages();
bool IsDelMode();
void RefreshCandidates();  // recompute candidate list for current buffer

void GoFirstPage();
void GoLastPage();
void GoNextPage();
void GoPrevPage();

// Data dir
void SetDataDir(const wchar_t* dir);

// Skin
bool LoadBuiltinSkin(BuiltinSkinId skin_id, NinePatchSkin& skin);
bool LoadSkinFromFile(const wchar_t* path, NinePatchSkin& skin, int mL, int mT, int mR, int mB);
void FreeSkin(NinePatchSkin& skin);
void SetSkin(const NinePatchSkin* skin);
void SetSettingsSkin(const NinePatchSkin* skin);
void SetBtnSkin(const NinePatchSkin* skin);
bool SetBtnIcon(int idx, const wchar_t* path);
bool SetModeIcon(int idx, const wchar_t* path);
bool SetLockIcon(const wchar_t* path);
bool SetSignEnIcon(const wchar_t* path);
bool SetNavIcon(int idx, const wchar_t* path);
void ToggleMode();
void ToggleLock();
bool IsLocked();
bool FlushPendingAndHideUI();

} // namespace ClassicABC
