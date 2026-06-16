// proto_engine.cpp - Input engine implementation.
#include "proto_engine.h"
#include <vector>

static bool       g_on  = false;
static std::wstring g_buf;

void ProtoIME::Engine::Init() {
    g_on = false;
    g_buf.clear();
}

void ProtoIME::Engine::SetActive(bool a) {
    g_on = a;
    if (!a) g_buf.clear();
}

bool ProtoIME::Engine::IsActive() { return g_on; }

bool ProtoIME::Engine::TestKey(UINT vk) {
    if (!g_on) return false;
    if (vk >= 'A' && vk <= 'Z') return true;
    if (vk == VK_SPACE || vk == VK_RETURN) return true;
    if (vk == VK_BACK || vk == VK_ESCAPE) return true;
    return false;
}

bool ProtoIME::Engine::ProcessKey(UINT vk) {
    if (!g_on) return false;
    if (vk >= 'A' && vk <= 'Z') {
        g_buf += (wchar_t)(vk - 'A' + 'a');
        return true;
    }
    if (vk == VK_SPACE && !g_buf.empty())   { Commit(); return true; }
    if (vk == VK_RETURN && !g_buf.empty())  { Commit(); return true; }
    if (vk == VK_BACK && !g_buf.empty())    { g_buf.pop_back(); return true; }
    if (vk == VK_ESCAPE && !g_buf.empty())  { g_buf.clear(); return true; }
    return false;
}

const std::wstring& ProtoIME::Engine::CompStr() { return g_buf; }

bool ProtoIME::Engine::HasText() { return !g_buf.empty(); }

void ProtoIME::Engine::Commit() {
    if (g_buf.empty()) return;
    std::vector<INPUT> in; in.reserve(g_buf.size() * 2);
    for (wchar_t ch : g_buf) {
        INPUT d = {}; d.type = INPUT_KEYBOARD; d.ki.wScan = ch; d.ki.dwFlags = KEYEVENTF_UNICODE; in.push_back(d);
        INPUT u = d;  u.ki.dwFlags |= KEYEVENTF_KEYUP; in.push_back(u);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
    g_buf.clear();
}
