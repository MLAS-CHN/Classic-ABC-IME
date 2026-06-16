#ifndef UTIL_H
#define UTIL_H

#include <cstddef>
#include <string>

#ifdef _WIN32
typedef __int64 ssize_t;
#endif

/**
 * 日志等级
 */
enum LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

/**
 * 通用工具函数接口
 * 包含与终端交互相关的辅助函数
 */

// 获取当前终端的尺寸（行数和列数）
// 通过引用参数 rows 和 cols 返回结果
void get_terminal_size(int &rows, int &cols);

// 写入日志
// 参数：message - 日志内容，level - 日志等级
void write_log(const std::string& message, LogLevel level = INFO);

void init_logger();

/**
 * 将按键序列转换为可读名称
 */
std::string get_key_name(const char* buf, ssize_t len);

/**
 * 计算字符串的显示列宽（粗略估算）。
 * 规则：ASCII 字符按 1 列；非 ASCII 字符按 2 列。
 *
 * @param s UTF-8 字符串。
 * @return 估算后的显示列宽。
 */
int get_display_width(const std::string& s);

/**
 * 按“显示列宽”裁剪 UTF-8 字符串，返回可安全截断的字节下标。
 * 规则同 get_display_width：ASCII=1 列，非 ASCII=2 列。
 *
 * @param s UTF-8 字符串。
 * @param max_width 允许的最大显示列宽。
 * @return 可用于 string::resize 的字节下标（不会截断到 UTF-8 字符中间）。
 */
size_t get_utf8_cut_index_by_width(const std::string& s, int max_width);

#endif // UTIL_H
