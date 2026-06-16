/**
 * pinyin_file_io.cpp
 * 拼音数据文件读写实现文件。
 * 负责加载字库/词库文本、构建首字母索引，并在写入后刷新缓存。
 */
#include "pinyin_file_io.h"
#include "candidate_item.h"
#include "util.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <cctype>

static std::string g_data_dir = "";

static std::string join_path(const std::string& dir, const std::string& file) {
    if (dir.empty()) return file;
#ifdef _WIN32
    if (dir.back() == '\\' || dir.back() == '/') return dir + file;
    return dir + "\\" + file;
#else
    if (dir.back() == '/') return dir + file;
    return dir + "/" + file;
#endif
}

static bool file_exists(const std::string& file_path) {
    std::ifstream f(file_path);
    return f.is_open();
}

static bool data_dir_valid(const std::string& dir) {
    return file_exists(join_path(dir, "pinyin_map.txt")) &&
           file_exists(join_path(dir, "user_dict.txt")) &&
           file_exists(join_path(dir, "char_freq.txt"));
}

static std::string resolve_default_data_dir() {
    std::vector<std::string> candidates;

#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    if (appdata && *appdata)
        candidates.push_back(std::string(appdata) + "\\lite-tty-ime\\data");
    const char* localappdata = getenv("LOCALAPPDATA");
    if (localappdata && *localappdata)
        candidates.push_back(std::string(localappdata) + "\\lite-tty-ime\\data");
    const char* userprofile = getenv("USERPROFILE");
    if (userprofile && *userprofile)
        candidates.push_back(std::string(userprofile) + "\\.lite-tty-ime\\data");
#else
    candidates.push_back("/usr/share/lite-tty-ime/data");
    const char* home = getenv("HOME");
    if (home && *home) candidates.push_back(std::string(home) + "/.lite-tty-ime/data");
#endif

    candidates.push_back("data");

    for (const auto& dir : candidates) {
        if (data_dir_valid(dir)) return dir;
    }
    return "data";
}

static void ensure_data_dir_resolved() {
    if (!g_data_dir.empty()) return;
    g_data_dir = resolve_default_data_dir();
}

bool set_pinyin_data_dir(const std::string& dir) {
    if (dir.empty()) return false;
    if (!data_dir_valid(dir)) return false;
    g_data_dir = dir;
    return true;
}

std::string get_pinyin_data_dir() {
    ensure_data_dir_resolved();
    return g_data_dir;
}

std::string get_pinyin_map_file_path() {
    ensure_data_dir_resolved();
    return join_path(g_data_dir, "pinyin_map.txt");
}

std::string get_user_dict_file_path() {
    ensure_data_dir_resolved();
    return join_path(g_data_dir, "user_dict.txt");
}

std::string get_char_freq_file_path() {
    ensure_data_dir_resolved();
    return join_path(g_data_dir, "char_freq.txt");
}

static bool is_all_digits(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

static void build_index_from_lines(const std::vector<std::string>& lines,
                                   std::vector<PinyinIndexItem>& index) {
    index.clear();
    if (lines.empty()) return;

    int current_line_num = 1;
    char current_char = '\0';
    int start_line = -1;

    for (const auto& line : lines) {
        if (line.empty()) {
            ++current_line_num;
            continue;
        }

        char first_char = line[0];
        if (first_char != current_char) {
            if (current_char != '\0') {
                index.push_back({(int)current_char, start_line, current_line_num - 1});
            }
            current_char = first_char;
            start_line = current_line_num;
        }
        ++current_line_num;
    }

    if (current_char != '\0') {
        index.push_back({(int)current_char, start_line, current_line_num - 1});
    }
}

static void persist_lines_to_file(const std::string& file_path, const std::vector<std::string>& lines) {
    std::ofstream file(file_path, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << file_path << std::endl;
        return;
    }
    for (const auto& line : lines) {
        file << line << "\n";
    }
    file.close();
}

static void insert_line_keep_ascii_sorted(const std::string& file_path,
                                          const std::string& new_line,
                                          std::vector<std::string>& lines,
                                          std::vector<PinyinIndexItem>& index) {
    if (new_line.empty()) return;

    if (lines.empty()) {
        lines.push_back(new_line);
        write_log("Insert line into " + file_path + " at line 1: " + new_line, LOG_INFO);
        persist_lines_to_file(file_path, lines);
        build_index_from_lines(lines, index);
        return;
    }

    char first_char = new_line[0];
    auto index_it = std::lower_bound(
        index.begin(), index.end(), (int)first_char,
        [](const PinyinIndexItem& item, int c) { return item.start_char < c; });

    size_t insert_pos = 0;
    if (index_it != index.end() && index_it->start_char == (int)first_char) {
        size_t start_idx = (size_t)std::max(0, index_it->start_line - 1);
        size_t end_exclusive = (size_t)std::max(0, index_it->end_line);
        if (start_idx > lines.size()) start_idx = lines.size();
        if (end_exclusive > lines.size()) end_exclusive = lines.size();

        auto it = std::lower_bound(lines.begin() + (std::ptrdiff_t)start_idx,
                                   lines.begin() + (std::ptrdiff_t)end_exclusive,
                                   new_line);
        insert_pos = (size_t)std::distance(lines.begin(), it);
    } else {
        if (index_it == index.end()) {
            insert_pos = lines.size();
        } else {
            insert_pos = (size_t)std::max(0, index_it->start_line - 1);
            if (insert_pos > lines.size()) insert_pos = lines.size();
        }
    }

    lines.insert(lines.begin() + (std::ptrdiff_t)insert_pos, new_line);
    write_log("Insert line into " + file_path + " at line " + std::to_string(insert_pos + 1) + ": " + new_line,
              LOG_INFO);
    persist_lines_to_file(file_path, lines);
    build_index_from_lines(lines, index);
}

/**
 * 初始化拼音相关数据缓存。
 * 启动时一次性加载：
 * - 单字库及其首字母索引；
 * - 用户词库及其首字母索引。
 */
void init_pinyin_data() {
    load_file_and_build_index(get_pinyin_map_file_path(), g_pinyin_map_lines, g_pinyin_map_index);
    load_file_and_build_index(get_user_dict_file_path(), g_user_dict_lines, g_user_dict_index);
    load_file_and_build_index(get_char_freq_file_path(), g_char_freq_lines, g_char_freq_index);
}

void load_file_and_build_index(const std::string& file_path, 
                               std::vector<std::string>& lines, 
                               std::vector<PinyinIndexItem>& index) {
    /**
     * 通用“加载文件 + 构建首字母索引”流程：
     * 1) 读取非空行到 lines；
     * 2) 按行首字符分段记录 start_line/end_line 到 index。
     */
    lines.clear();
    index.clear();

    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return;
    }

    std::string line;
    int current_line_num = 1;
    char current_char = '\0';
    int start_line = -1;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        lines.push_back(line);

        char first_char = line[0];
        if (first_char != current_char) {
            // 如果不是第一个发现的字符，关闭上一个字符的索引范围
            if (current_char != '\0') {
                index.push_back({(int)current_char, start_line, current_line_num - 1});
            }
            current_char = first_char;
            start_line = current_line_num;
        }
        current_line_num++;
    }

    // 处理最后一个字符
    if (current_char != '\0') {
        index.push_back({(int)current_char, start_line, current_line_num - 1});
    }

    file.close();
}

void write_and_update_index(const std::string& file_path, 
                            const std::string& content,
                            std::vector<std::string>& lines, 
                            std::vector<PinyinIndexItem>& index) {
    insert_line_keep_ascii_sorted(file_path, content, lines, index);
}

void write_and_update_index(const std::string& file_path,
                            const CandidateItem& candidate,
                            std::vector<std::string>& lines,
                            std::vector<PinyinIndexItem>& index) {
    insert_line_keep_ascii_sorted(file_path, candidate.toString(), lines, index);
}

bool delete_user_dict_line(int line_number) {
    if (line_number <= 0) return false;
    int idx = line_number - 1;
    if (idx < 0 || idx >= (int)g_user_dict_lines.size()) return false;

    g_user_dict_lines.erase(g_user_dict_lines.begin() + idx);
    persist_lines_to_file(get_user_dict_file_path(), g_user_dict_lines);
    build_index_from_lines(g_user_dict_lines, g_user_dict_index);
    return true;
}

int get_weight_by_line(WeightTargetFile target_file, int line_number) {
    if (line_number <= 0) return 0;
    const std::vector<std::string>* lines = (target_file == WeightTargetFile::CharFreq) ? &g_char_freq_lines
                                                                                        : &g_user_dict_lines;
    int idx = line_number - 1;
    if (idx < 0 || idx >= (int)lines->size()) return 0;

    const std::string& raw_line = (*lines)[idx];
    std::istringstream iss(raw_line);
    std::vector<std::string> parts;
    std::string token;
    while (iss >> token) {
        parts.push_back(token);
    }
    if (parts.size() < 2) return 0;

    if (parts.size() >= 3 && is_all_digits(parts.back())) {
        return std::stoi(parts.back());
    }
    return 1;
}

void increment_weight_by_line(WeightTargetFile target_file, int line_number) {
    if (line_number <= 0) return;
    std::string file_path = (target_file == WeightTargetFile::CharFreq) ? get_char_freq_file_path()
                                                                        : get_user_dict_file_path();
    std::vector<std::string>* lines = (target_file == WeightTargetFile::CharFreq) ? &g_char_freq_lines
                                                                                  : &g_user_dict_lines;
    std::vector<PinyinIndexItem>* index = (target_file == WeightTargetFile::CharFreq) ? &g_char_freq_index
                                                                                      : &g_user_dict_index;

    int idx = line_number - 1;
    if (idx < 0 || idx >= (int)lines->size()) return;

    const std::string& raw_line = (*lines)[idx];
    std::istringstream iss(raw_line);
    std::vector<std::string> parts;
    std::string token;
    while (iss >> token) {
        parts.push_back(token);
    }
    if (parts.size() < 2) return;

    std::string new_line;
    if (parts.size() >= 3 && is_all_digits(parts.back())) {
        int weight = std::stoi(parts.back());
        weight += 1;
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            if (!new_line.empty()) new_line += " ";
            new_line += parts[i];
        }
        new_line += " " + std::to_string(weight);
    } else {
        new_line = raw_line + " 1";
    }

    (*lines)[idx] = new_line;

    persist_lines_to_file(file_path, *lines);
    build_index_from_lines(*lines, *index);
}
