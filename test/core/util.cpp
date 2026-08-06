#include "util.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <cstdint>
#include <sstream>

static std::string g_log_dir;
static std::string g_log_file_path = "abcime.log";
static LogLevel g_min_log_level = LOG_INFO;

/**
 * 辅助函数实现
 */
void init_logger() {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
#ifdef _WIN32
    localtime_s(&tm_now, &now_time);
#else
    localtime_r(&now_time, &tm_now);
#endif

    std::ostringstream oss;
    if (!g_log_dir.empty())
        oss << g_log_dir << "/";
    oss << "lite-tty-ime_" << std::put_time(&tm_now, "%Y%m%d_%H%M%S") << ".log";
    g_log_file_path = oss.str();

    std::ofstream ofs(g_log_file_path, std::ios::trunc);
    (void)ofs;
}

void init_logger_with_dir(const std::string& dir) {
    g_log_dir = dir;
    init_logger();
}

void set_log_level(LogLevel level) {
    g_min_log_level = level;
}

/**
 * 统一日志写入入口。
 * 以追加模式写入 `lite-tty-ime.log`，并带时间戳与日志等级。
 *
 * @param message 日志正文。
 * @param level 日志等级。
 */
void write_log(const std::string& message, LogLevel level) {
    if (level < g_min_log_level) return;
    // 打开文件（追加模式）
    // 直接使用字符串字面量，避免 static string 在 atexit 时可能已析构的问题
    std::ofstream ofs(g_log_file_path.c_str(), std::ios::app);
    if (!ofs.is_open()) return;

    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // 格式化日志等级
    std::string level_str;
    switch (level) {
        case LOG_DEBUG: level_str = "DEBUG"; break;
        case LOG_INFO:  level_str = "INFO "; break;
        case LOG_WARN:  level_str = "WARN "; break;
        case LOG_ERROR: level_str = "ERROR"; break;
    }

    // 写入日志：[2023-04-25 10:00:00.123] [INFO ] Message
    std::tm tm_log;
#ifdef _WIN32
    localtime_s(&tm_log, &now_time);
#else
    localtime_r(&now_time, &tm_log);
#endif
    ofs << "[" << std::put_time(&tm_log, "%Y-%m-%d %H:%M:%S") 
        << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
        << "[" << level_str << "] " 
         << message << '\n';
}

static bool decode_utf8_codepoint(const std::string& s, size_t offset, uint32_t& codepoint, size_t& step) {
    if (offset >= s.size()) return false;
    unsigned char c0 = (unsigned char)s[offset];
    if ((c0 & 0x80) == 0) {
        codepoint = c0;
        step = 1;
        return true;
    }
    if ((c0 & 0xE0) == 0xC0) {
        if (offset + 2 > s.size()) return false;
        unsigned char c1 = (unsigned char)s[offset + 1];
        if ((c1 & 0xC0) != 0x80) return false;
        codepoint = ((uint32_t)(c0 & 0x1F) << 6) | (uint32_t)(c1 & 0x3F);
        step = 2;
        return true;
    }
    if ((c0 & 0xF0) == 0xE0) {
        if (offset + 3 > s.size()) return false;
        unsigned char c1 = (unsigned char)s[offset + 1];
        unsigned char c2 = (unsigned char)s[offset + 2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return false;
        codepoint = ((uint32_t)(c0 & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) | (uint32_t)(c2 & 0x3F);
        step = 3;
        return true;
    }
    if ((c0 & 0xF8) == 0xF0) {
        if (offset + 4 > s.size()) return false;
        unsigned char c1 = (unsigned char)s[offset + 1];
        unsigned char c2 = (unsigned char)s[offset + 2];
        unsigned char c3 = (unsigned char)s[offset + 3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
        codepoint = ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) |
                    ((uint32_t)(c2 & 0x3F) << 6) | (uint32_t)(c3 & 0x3F);
        step = 4;
        return true;
    }
    return false;
}

static bool is_candidate_superscript(uint32_t codepoint) {
    switch (codepoint) {
        case 0x00B9: // ¹
        case 0x00B2: // ²
        case 0x00B3: // ³
        case 0x2070: // ⁰
        case 0x2074: // ⁴
        case 0x2075: // ⁵
        case 0x2076: // ⁶
        case 0x2077: // ⁷
        case 0x2078: // ⁸
        case 0x2079: // ⁹
            return true;
        default:
            return false;
    }
}

/**
 * 计算字符串的显示列宽（粗略估算）。
 * 规则：
 * - 以第一个候选上标数字为分界点（¹/²/³/⁰-⁹）。
 * - 分界点之前：ASCII=1 列，非 ASCII=2 列。
 * - 分界点之后：上标数字与空格=1 列，其余全部按全角=2 列（包括 ASCII）。
 *
 * @param s UTF-8 字符串。
 * @return 估算后的显示列宽。
 */
int get_display_width(const std::string& s) {
    int width = 0;
    bool in_candidates = false;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = 0;
        size_t step = 1;
        if (!decode_utf8_codepoint(s, i, cp, step)) {
            cp = (unsigned char)s[i];
            step = 1;
        }

        if (!in_candidates && is_candidate_superscript(cp)) {
            in_candidates = true;
        }

        if (!in_candidates) {
            width += (cp <= 0x7F) ? 1 : 2;
        } else {
            if (cp == 0x20 || is_candidate_superscript(cp)) {
                width += 1;
            } else {
                width += 2;
            }
        }

        i += step;
    }
    return width;
}

/**
 * 按“显示列宽”裁剪 UTF-8 字符串，返回可安全截断的字节下标。
 * 规则同 get_display_width。
 *
 * @param s UTF-8 字符串。
 * @param max_width 允许的最大显示列宽。
 * @return 可用于 string::resize 的字节下标（不会截断到 UTF-8 字符中间）。
 */
size_t get_utf8_cut_index_by_width(const std::string& s, int max_width) {
    int width = 0;
    size_t i = 0;
    bool in_candidates = false;
    while (i < s.size()) {
        uint32_t cp = 0;
        size_t step = 1;
        if (!decode_utf8_codepoint(s, i, cp, step)) {
            cp = (unsigned char)s[i];
            step = 1;
        }

        if (!in_candidates && is_candidate_superscript(cp)) {
            in_candidates = true;
        }

        int char_width = 0;
        if (!in_candidates) {
            char_width = (cp <= 0x7F) ? 1 : 2;
        } else {
            if (cp == 0x20 || is_candidate_superscript(cp)) {
                char_width = 1;
            } else {
                char_width = 2;
            }
        }

        if (width + char_width > max_width) break;
        width += char_width;
        i += step;
    }
    return i;
}
