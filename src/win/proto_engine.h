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
void ReloadDict();

bool TestKey(UINT vk);
bool ProcessKey(UINT vk);
bool ProcessShiftTap();
void ToggleChineseMode();
bool IsLocked();
void ToggleLock();
bool FlushPending();  // flush buffer text + hide UI; returns true if flushed

const std::wstring& CompStr();
bool HasText();

// Candidate access
size_t GetCandidateCount();
std::wstring GetCandidateText(size_t i);
size_t GetCandidatePage();
size_t GetTotalPages();
bool IsChineseMode();
bool IsDelMode();

void GoFirstPage();
void GoLastPage();
void GoNextPage();
void GoPrevPage();

} // Engine
} // ProtoIME
