// proto_engine.h - Real pinyin engine for Windows TSF.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace ProtoIME {
namespace Engine {

struct Candidate { std::wstring text; int weight; };

void Init();
void SetActive(bool active);
bool IsActive();

bool TestKey(UINT vk);
bool ProcessKey(UINT vk);
bool ProcessShiftTap();
void ToggleChineseMode();
bool IsLocked();
void ToggleLock();

const std::wstring& CompStr();
bool HasText();

// Candidate access
size_t GetCandidateCount();
std::wstring GetCandidateText(size_t i);
size_t GetCandidatePage();
size_t GetTotalPages();
bool IsChineseMode();
bool IsDelMode();

} // Engine
} // ProtoIME
