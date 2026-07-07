/**
 * pinyin_virtual_cursor.cpp
 * 拼音缓冲虚拟光标操作实现文件。
 */
#include "pinyin_virtual_cursor.h"
#include <algorithm>
#include <string>

static size_t clamp_cursor(size_t virtual_cursor, const std::string& buffer) {
    return std::min(virtual_cursor, buffer.size());
}

void reset_virtual_cursor_to_end(const std::string& buffer, size_t& virtual_cursor) {
    virtual_cursor = buffer.size();
}

void move_virtual_cursor_left(size_t& virtual_cursor) {
    if (virtual_cursor > 0) --virtual_cursor;
}

void move_virtual_cursor_right(size_t& virtual_cursor, const std::string& buffer) {
    if (virtual_cursor < buffer.size()) ++virtual_cursor;
}

void insert_at_virtual_cursor(std::string& buffer, size_t& virtual_cursor, char ch) {
    virtual_cursor = clamp_cursor(virtual_cursor, buffer);
    buffer.insert(buffer.begin() + static_cast<std::ptrdiff_t>(virtual_cursor), ch);
    ++virtual_cursor;
}

bool can_insert_word_separator_at_virtual_cursor(const std::string& buffer, size_t virtual_cursor) {
    virtual_cursor = clamp_cursor(virtual_cursor, buffer);
    // 不允许在首尾插入单引号（前或后为空）
    if (virtual_cursor == 0 || virtual_cursor >= buffer.size()) return false;

    char left = buffer[virtual_cursor - 1];
    char right = buffer[virtual_cursor];
    auto is_lower_letter = [](char ch) { return ch >= 'a' && ch <= 'z'; };

    // 前后任一不是字母都不允许（包含连续单引号等非字母场景）
    return is_lower_letter(left) && is_lower_letter(right);
}

bool backspace_at_virtual_cursor(std::string& buffer, size_t& virtual_cursor) {
    virtual_cursor = clamp_cursor(virtual_cursor, buffer);
    if (virtual_cursor == 0 || buffer.empty()) return false;
    buffer.erase(buffer.begin() + static_cast<std::ptrdiff_t>(virtual_cursor - 1));
    --virtual_cursor;
    return true;
}
