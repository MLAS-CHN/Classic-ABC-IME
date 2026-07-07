// proto_engine.cpp - Pinyin engine adapter for Windows TSF.
#include "proto_engine.h"

// Pinyin engine core
#include "../candidate_item.h"
#include "../pinyin_composition.h"
#include "../pinyin_data.h"
#include "../pinyin_file_io.h"
#include "../pinyin_split.h"
#include "../pinyin_virtual_cursor.h"
#include "../util.h"
#include <vector>
#include <algorithm>

static const size_t kPageSize = 10;

struct State {
    bool   active = false;  // engine activation state
    bool   chinese = true;
    bool   locked = false;  // gaming lock mode (Shift-proof English)
    bool   delmode = false;
    std::string buf;           // pinyin buffer (UTF-8)
    size_t cur = 0;
    std::vector<std::vector<CandidateItem>> pages;
    size_t page = 0;
    std::vector<CandidateItem> cont;      // continuous input
    bool   contmode = false;
};
// NOTE: TSF/IMM is STA (single-threaded apartment) — all key handling,
// rebuild(), WM_PAINT, and dict cache access run on the same thread.
// No lock needed for g_comp / g / caches under this threading model.
static State g;
static std::wstring g_comp;
static bool g_shift_pending = false;  // true when Shift was pressed alone (potential tap)

// ---- UTF helpers ----
static std::wstring u8to16(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::string u16to8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// ---- commit ----
static void send_u8(const std::string& u8) {
    if (u8.empty()) return;
    std::wstring w = u8to16(u8);
    if (w.empty()) return;
    std::vector<INPUT> in; in.reserve(w.size() * 2);
    for (wchar_t ch : w) {
        INPUT d = {}; d.type = INPUT_KEYBOARD; d.ki.wScan = ch; d.ki.dwFlags = KEYEVENTF_UNICODE; in.push_back(d);
        INPUT u = d;   u.ki.dwFlags |= KEYEVENTF_KEYUP;           in.push_back(u);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

static void send_w(const std::wstring& w) {
    if (w.empty()) return;
    std::vector<INPUT> in; in.reserve(w.size() * 2);
    for (wchar_t ch : w) {
        INPUT d = {}; d.type = INPUT_KEYBOARD; d.ki.wScan = ch; d.ki.dwFlags = KEYEVENTF_UNICODE; in.push_back(d);
        INPUT u = d;   u.ki.dwFlags |= KEYEVENTF_KEYUP;           in.push_back(u);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

// ---- rebuild display and candidates ----
static void rebuild() {
    if (g.buf.empty()) { g.pages.clear(); g.page = 0; g_comp.clear(); return; }
    if (g.page >= g.pages.size() && !g.pages.empty()) g.page = g.pages.size() - 1;
    if (g.pages.empty()) g.page = 0;
    std::string d = buildComposingDisplayText(g.buf, g.cur, g.page, kPageSize, &g.pages);
    if (g.page >= g.pages.size() && !g.pages.empty()) g.page = g.pages.size() - 1;
    size_t pos = d.find(u8"\u00B9");
    if (pos == std::string::npos) pos = d.find(u8"\u00B2");
    if (pos == std::string::npos) pos = d.find(u8"\u2070");
    g_comp = u8to16(pos != std::string::npos ? d.substr(0, pos) : d);
}

// ---- consume pinyin buffer prefix ----
static void consume_buf(std::vector<std::string> parts, std::string& buf) {
    while (!parts.empty() && !buf.empty()) {
        if (buf[0] == '\'') { buf.erase(0, 1); continue; }
        if (parts[0].empty()) { parts.erase(parts.begin()); continue; }
        if (parts[0][0] == buf[0]) { parts[0].erase(0, 1); buf.erase(0, 1); continue; }
        parts.erase(parts.begin());
    }
}

// ---- persist weight ----
static void persist(const CandidateItem& ci) {
    auto tf = ci.getPinyinParts().size() == 1 ? WeightTargetFile::CharFreq : WeightTargetFile::UserDict;
    int ln = ci.findSourceLineNumber();
    if (ln > 0) { increment_weight_by_line(tf, ln); return; }
    if (tf == WeightTargetFile::CharFreq)
        write_and_update_index(get_char_freq_file_path(), ci, g_char_freq_lines, g_char_freq_index);
    else
        write_and_update_index(get_user_dict_file_path(), ci, g_user_dict_lines, g_user_dict_index);
}

// ---- select candidate ----
static std::string pick(size_t pi, size_t ci) {
    if (g.pages.empty() || pi >= g.pages.size()) return {};
    const auto& pg = g.pages[pi];
    if (ci >= pg.size()) return {};
    CandidateItem sel = pg[ci];

    if (g.delmode) {
        if (sel.getPinyinParts().size() > 1) {
            int ln = sel.findSourceLineNumber();
            if (ln > 0) delete_user_dict_line(ln);
            consume_buf(sel.getPinyinParts(), g.buf);
            g.delmode = false;
            if (g.cur > g.buf.size()) g.cur = g.buf.size();
            rebuild();
            return {};
        }
        g.delmode = false;
    }

    std::string ret = sel.getText();
    persist(sel);
    consume_buf(sel.getPinyinParts(), g.buf);

    if (g.buf.empty()) {
        g.delmode = false; g.cur = 0; g.pages.clear(); g.page = 0;
        if (g.contmode) {
            g.cont.push_back(sel);
            auto m = CandidateItem::mergeCandidateItems(g.cont);
            if (!m.getText().empty() && !m.getPinyinParts().empty())
                write_and_update_index(get_user_dict_file_path(), m, g_user_dict_lines, g_user_dict_index);
            g.cont.clear(); g.contmode = false;
        }
        g_comp.clear();
    } else {
        g.cur = g.buf.size(); g.page = 0;
        g.cont.push_back(sel); g.contmode = true;
        rebuild();
    }
    return ret;
}

// ========== public API ==========
void ProtoIME::Engine::Init() {
    init_pinyin_data();
    g = State{};
    g_comp.clear();
}
void ProtoIME::Engine::SetActive(bool a) {
    g.active = a;
    if (a) {
        // Reload dictionaries to pick up changes from other processes
        init_pinyin_data();
    } else {
        bool wasChinese = g.chinese;
        g = State{};
        g.chinese = wasChinese;
        g_comp.clear();
    }
    write_log("Engine: SetActive(" + std::string(a ? "true" : "false") + ") chinese=" + std::to_string(g.chinese) + " locked=" + std::to_string(g.locked), LOG_DEBUG);
}
bool ProtoIME::Engine::IsActive() { return g.active; }

void ProtoIME::Engine::ReloadDict() {
    static FILETIME lastWrite = {};
    WIN32_FILE_ATTRIBUTE_DATA attr;
    std::string path = get_user_dict_file_path();
    int n = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (n <= 0) return;
    std::wstring wpath((size_t)(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], n);
    if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &attr)) return;
    if (memcmp(&attr.ftLastWriteTime, &lastWrite, sizeof(FILETIME)) == 0) return;  // unchanged
    lastWrite = attr.ftLastWriteTime;
    init_pinyin_data();
    write_log("Engine: ReloadDict reloaded (file changed)", LOG_DEBUG);
}

bool ProtoIME::Engine::TestKey(UINT vk) {
    if (!g.active) return false;

    // Locked mode: pass through everything (gaming)
    if (g.locked) return false;

    // Always claim Shift/CapsLock keys for tap/mode detection
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_CAPITAL) return true;

    // Any non-Shift key: clear pending tap
    g_shift_pending = false;

    // CapsLock on: English mode — pass through
    if (GetKeyState(VK_CAPITAL) & 0x0001) return false;

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    // Ctrl held: don't intercept (shortcuts)
    if (ctrl) return false;

    // English mode: pass through all keys
    if (!g.chinese) return false;

    // Chinese punctuation with Shift (only when no pinyin buffer and no candidates)
    if (shift && g.buf.empty() && g.pages.empty()) {
        if (vk >= '0' && vk <= '9') return true;
        if (vk == VK_OEM_3 || vk == VK_OEM_4 || vk == VK_OEM_5 || vk == VK_OEM_6 ||
            vk == VK_OEM_1 || vk == VK_OEM_2 || vk == VK_OEM_7 ||
            vk == VK_OEM_PLUS || vk == VK_OEM_MINUS ||
            vk == VK_OEM_COMMA || vk == VK_OEM_PERIOD || vk == VK_OEM_102)
            return true;
    }

    // Shift held (non-punctuation context): don't intercept
    if (shift) return false;

    // Chinese mode: intercept pinyin keys
    if (vk >= 'A' && vk <= 'Z') return true;
    if (vk == VK_BACK && !g.buf.empty()) return true;
    if (!g.buf.empty()) {
        if (vk == VK_OEM_7) return true;
        if (vk == VK_LEFT || vk == VK_RIGHT) return true;
        if (vk == VK_RETURN || vk == VK_ESCAPE) return true;
    }
    if (!g.pages.empty()) {
        if (vk == VK_SPACE) return true;
        if (vk >= '0' && vk <= '9') return true;
        if (vk == VK_OEM_PLUS || vk == VK_OEM_MINUS) return true;
        if (vk == VK_DELETE) return true;
    }
    // Chinese punctuation without Shift (only when no pinyin buffer and no candidates)
    if (g.buf.empty() && g.pages.empty()) {
        if (vk == VK_OEM_3 || vk == VK_OEM_4 || vk == VK_OEM_5 || vk == VK_OEM_6 ||
            vk == VK_OEM_1 || vk == VK_OEM_2 || vk == VK_OEM_7 ||
            vk == VK_OEM_COMMA || vk == VK_OEM_PERIOD || vk == VK_OEM_102)
            return true;
        // VK_OEM_PLUS/MINUS without shift: + and - → fullwidth + and -
        if (vk == VK_OEM_PLUS || vk == VK_OEM_MINUS) return true;
    }
    return false;
}

// ---- key action helpers ----

// Chinese fullwidth punctuation mapping.
// shift=false: unshifted symbol; shift=true: shifted symbol.
// Returns nullptr if the key is not a punctuation key.
static const char* punct_fullwidth(UINT vk, bool shift) {
    // Number row (VK '0'-'9'): shifted symbols are punctuation
    if (vk >= '0' && vk <= '9') {
        if (!shift) return nullptr; // unshifted digits are not punctuation
        static const char* numShift[] = {
            u8"）",  // 0 → )
            u8"！",  // 1 → !
            u8"＠",  // 2 → @
            u8"＃",  // 3 → #
            u8"￥",  // 4 → ¥
            u8"％",  // 5 → %
            u8"……", // 6 → double ellipsis
            u8"＆",  // 7 → &
            u8"＊",  // 8 → *
            u8"（",  // 9 → (
        };
        return numShift[vk - '0'];
    }
    // OEM keys
    if (shift) {
        switch (vk) {
            case VK_OEM_3:      return u8"～";   // ~ → fullwidth tilde
            case VK_OEM_4:      return u8"｛";   // { → fullwidth {
            case VK_OEM_5:      return u8"｜";   // | → fullwidth |
            case VK_OEM_6:      return u8"｝";   // } → fullwidth }
            case VK_OEM_1:      return u8"：";   // : → ：
            case VK_OEM_2:      return u8"？";   // ? → ？
            case VK_OEM_7:      return u8"“”";  // " → paired double quotes
            case VK_OEM_PLUS:   return u8"＋";   // + → fullwidth +
            case VK_OEM_MINUS:  return u8"——";  // _ → double em-dash
            case VK_OEM_COMMA:  return u8"《";   // < → 《
            case VK_OEM_PERIOD: return u8"》";   // > → 》
            case VK_OEM_102:    return u8"｜";   // | → fullwidth |
        }
    } else {
        switch (vk) {
            case VK_OEM_3:      return u8"·";    // ` → ·
            case VK_OEM_4:      return u8"【";   // [ → 【
            case VK_OEM_5:      return u8"、";   // \ → 、
            case VK_OEM_6:      return u8"】";   // ] → 】
            case VK_OEM_1:      return u8"；";   // ; → ；
            case VK_OEM_2:      return u8"／";   // / → fullwidth /
            case VK_OEM_7:      return u8"‘’";   // ' → paired single quotes
            case VK_OEM_PLUS:   return u8"＝";   // = → fullwidth =
            case VK_OEM_MINUS:  return u8"－";   // - → fullwidth -
            case VK_OEM_COMMA:  return u8"，";   // , → ，
            case VK_OEM_PERIOD: return u8"。";   // . → 。
            case VK_OEM_102:    return u8"、";   // <> key → 、
        }
    }
    return nullptr;
}

static bool handlePunct(UINT vk) {
    if (!g.buf.empty() || !g.pages.empty()) return false;
    // Numpad keys always pass through (never convert)
    if (vk == VK_MULTIPLY || vk == VK_DIVIDE || vk == VK_ADD || vk == VK_SUBTRACT)
        return false;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const char* text = punct_fullwidth(vk, shift);
    if (!text) return false;
    send_u8(text);
    return true;
}

static bool handleCandidateSelect(UINT vk) {
    if (g.pages.empty()) return false;
    if (vk == VK_SPACE) { send_u8(pick(g.page, 0)); return true; }
    if (vk >= '0' && vk <= '9') {
        size_t idx = vk == '0' ? 9 : (size_t)(vk - '1');
        send_u8(pick(g.page, idx)); return true;
    }
    return false;
}

static bool handlePage(UINT vk) {
    if (g.pages.empty()) return false;
    if (vk == VK_OEM_PLUS) {
        if (g.page + 1 < g.pages.size()) { g.page++; rebuild(); }
        return true;
    }
    if (vk == VK_OEM_MINUS) {
        if (g.page > 0) { g.page--; rebuild(); }
        return true;
    }
    return false;
}

static bool handleLetter(UINT vk) {
    if (vk < 'A' || vk > 'Z') return false;
    char c = (char)(vk - 'A' + 'a');
    insert_at_virtual_cursor(g.buf, g.cur, c);
    if (g.buf.size() == 1) reset_virtual_cursor_to_end(g.buf, g.cur);
    rebuild();
    return true;
}

static bool handleDeleteMode(UINT vk) {
    if (vk != VK_DELETE || g.pages.empty()) return false;
    g.delmode = !g.delmode; rebuild(); return true;
}

static bool handleCursorMove(UINT vk) {
    if (g.buf.empty()) return false;
    if (vk == VK_LEFT)  { move_virtual_cursor_left(g.cur); rebuild(); return true; }
    if (vk == VK_RIGHT) { move_virtual_cursor_right(g.cur, g.buf); rebuild(); return true; }
    return false;
}

bool ProtoIME::Engine::ProcessKey(UINT vk) {
    if (!g.active) return false;

    // Locked mode: pass through everything (gaming)
    if (g.locked) return false;

    // Shift press tracking for tap detection
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        g_shift_pending = true;
        return false;  // not eaten, let system see Shift
    }

    // Any non-Shift key clears pending tap
    g_shift_pending = false;

    // CapsLock press: force English mode, flush + reset pinyin state
    if (vk == VK_CAPITAL) {
        FlushPending();
        g.chinese = false;
        return false;  // not eaten, let system toggle CapsLock
    }

    // CapsLock on: English mode — pass through
    if (GetKeyState(VK_CAPITAL) & 0x0001) return false;

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    // Ctrl held: pass through (shortcuts)
    if (ctrl) return false;

    // English mode: pass through all keys
    if (!g.chinese) return false;

    // Chinese punctuation (Shift+symbol keys, when no pinyin buffer and no candidates)
    if (shift) {
        if (handlePunct(vk)) return true;
        return false; // other Shift+key combinations pass through
    }

    // priority: functional keys > pinyin input
    if (handlePunct(vk))        return true;
    if (handleCandidateSelect(vk)) return true;
    if (handlePage(vk))           return true;
    if (handleDeleteMode(vk))     return true;
    if (handleCursorMove(vk))     return true;

    // editing keys
    if (vk == VK_BACK && !g.buf.empty()) {
        if (backspace_at_virtual_cursor(g.buf, g.cur)) rebuild();
        return true;
    }
    if (vk == VK_ESCAPE && !g.buf.empty()) {
        g.buf.clear(); g.cur = 0; g.delmode = false;
        g.pages.clear(); g.page = 0; g.cont.clear(); g.contmode = false;
        g_comp.clear(); return true;
    }
    if (vk == VK_RETURN && !g.buf.empty()) {
        send_w(u8to16(g.buf));
        g.buf.clear(); g.cur = 0; g.delmode = false;
        g.pages.clear(); g.page = 0; g_comp.clear();
        return true;
    }

    // word separator '
    if (vk == VK_OEM_7 && !g.buf.empty()) {
        if (can_insert_word_separator_at_virtual_cursor(g.buf, g.cur)) {
            insert_at_virtual_cursor(g.buf, g.cur, '\''); rebuild();
        }
        return true;
    }

    // pinyin letter input (must be last - fallback)
    return handleLetter(vk);
}

bool ProtoIME::Engine::ProcessShiftTap() {
    if (g_shift_pending) {
        g_shift_pending = false;
        ProtoIME::Engine::ToggleChineseMode();
        return true;  // eat the key-up to prevent system handling
    }
    g_shift_pending = false;
    return false;
}

void ProtoIME::Engine::ToggleChineseMode() {
    g.chinese = !g.chinese;
    write_log("Engine: ToggleChineseMode -> chinese=" + std::to_string(g.chinese), LOG_DEBUG);
    if (!g.chinese) FlushPending();
}

bool ProtoIME::Engine::IsLocked() { return g.locked; }

void ProtoIME::Engine::ToggleLock() {
    g.locked = !g.locked;
    write_log("Engine: ToggleLock -> locked=" + std::to_string(g.locked), LOG_INFO);
    if (g.locked) { FlushPending(); g.chinese = false; }
}

bool ProtoIME::Engine::FlushPending() {
    if (g.buf.empty()) return false;
    send_w(u8to16(g.buf));
    g.buf.clear(); g.cur = 0; g.delmode = false;
    g.pages.clear(); g.page = 0; g.cont.clear(); g.contmode = false;
    g_comp.clear();
    write_log("Engine: FlushPending flushed buffer", LOG_DEBUG);
    return true;
}

const std::wstring& ProtoIME::Engine::CompStr() { return g_comp; }
bool ProtoIME::Engine::HasText() { return !g.buf.empty(); }

size_t ProtoIME::Engine::GetCandidateCount() {
    if (g.pages.empty() || g.page >= g.pages.size()) return 0;
    return g.pages[g.page].size();
}
std::wstring ProtoIME::Engine::GetCandidateText(size_t i) {
    if (g.pages.empty() || g.page >= g.pages.size()) return L"";
    const auto& pg = g.pages[g.page];
    return i < pg.size() ? u8to16(pg[i].getText()) : L"";
}
size_t ProtoIME::Engine::GetCandidatePage()  { return g.page; }
size_t ProtoIME::Engine::GetTotalPages()     { return g.pages.size(); }
bool   ProtoIME::Engine::IsChineseMode()     { return g.chinese; }
bool   ProtoIME::Engine::IsDelMode()         { return g.delmode; }

void ProtoIME::Engine::GoFirstPage() {
    if (g.pages.empty()) return;
    g.page = 0; rebuild();
}
void ProtoIME::Engine::GoLastPage() {
    if (g.pages.empty()) return;
    g.page = g.pages.size() - 1; rebuild();
}
void ProtoIME::Engine::GoNextPage() {
    if (g.pages.empty()) return;
    if (g.page + 1 < g.pages.size()) { g.page++; rebuild(); }
}
void ProtoIME::Engine::GoPrevPage() {
    if (g.pages.empty()) return;
    if (g.page > 0) { g.page--; rebuild(); }
}
