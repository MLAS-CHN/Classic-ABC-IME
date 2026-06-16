// proto_engine.h - Pure input engine: composition buffer + key dispatch + commit.
// ZERO dependency on GDI, windows, or rendering. Only uses SendInput via windows.h.
#pragma once
#include <windows.h>
#include <string>

namespace ProtoIME {
namespace Engine {

void Init();
void SetActive(bool active);
bool IsActive();

// Non-mutating check: would this virtual key be consumed?
bool TestKey(UINT vk);

// Process key, returns true if eaten. Modifies composition state.
// May call SendInput on commit.
bool ProcessKey(UINT vk);

const std::wstring& CompStr();
bool HasText();

// Commit current composition via SendInput and clear buffer.
void Commit();

} // Engine
} // ProtoIME
