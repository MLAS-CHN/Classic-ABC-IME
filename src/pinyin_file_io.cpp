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
#include <ctime>

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
    std::string tmp_path = file_path + ".tmp";
    std::ofstream file(tmp_path, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << tmp_path << '\n';
        return;
    }
    for (const auto& line : lines) {
        file << line << "\n";
    }
    file.close();
    std::filesystem::rename(tmp_path, file_path);
}

static void build_user_dict_cache() {
    g_user_dict_parts.clear();
    g_user_dict_segcount_map.clear();
    g_user_dict_lookup.clear();

    g_user_dict_parts.reserve(g_user_dict_lines.size());
    for (int i = 0; i < (int)g_user_dict_lines.size(); ++i) {
        const std::string& line = g_user_dict_lines[i];
        std::string pinyin_csv = get_pinyin_from_line(line);
        std::vector<std::string> parts = split_csv(pinyin_csv);
        g_user_dict_parts.push_back(parts);
        g_user_dict_segcount_map[parts.size()].push_back(i + 1);

        std::istringstream iss(line);
        std::string py, text, ts_str, count_str;
        if (iss >> py >> text) {
            std::string key = py + " " + text;
            long long timestamp = 0;
            int count = 1;
            if (iss >> ts_str && is_all_digits(ts_str)) {
                timestamp = std::stoll(ts_str);
            }
            if (iss >> count_str && is_all_digits(count_str)) {
                count = std::stoi(count_str);
            }
            g_user_dict_lookup[key] = {i + 1, timestamp, count};
        }
    }
}

static void build_char_freq_cache() {
    g_char_freq_lookup.clear();
    for (int i = 0; i < (int)g_char_freq_lines.size(); ++i) {
        const std::string& line = g_char_freq_lines[i];
        std::istringstream iss(line);
        std::string py, text, ts_str, count_str;
        if (!(iss >> py >> text)) continue;
        long long timestamp = 0;
        int count = 1;
        if (iss >> ts_str && is_all_digits(ts_str)) {
            timestamp = std::stoll(ts_str);
        }
        if (iss >> count_str && is_all_digits(count_str)) {
            count = std::stoi(count_str);
        }
        std::string key = py + " " + text;
        g_char_freq_lookup[key] = {i + 1, timestamp, count};
    }
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
    if (&lines == &g_user_dict_lines) build_user_dict_cache();
    if (&lines == &g_char_freq_lines) build_char_freq_cache();
}

void init_pinyin_data() {
    load_file_and_build_index(get_pinyin_map_file_path(), g_pinyin_map_lines, g_pinyin_map_index);
    load_file_and_build_index(get_user_dict_file_path(), g_user_dict_lines, g_user_dict_index);
    load_file_and_build_index(get_char_freq_file_path(), g_char_freq_lines, g_char_freq_index);
    build_user_dict_cache();
    build_char_freq_cache();
}

void load_file_and_build_index(const std::string& file_path,
                               std::vector<std::string>& lines,
                               std::vector<PinyinIndexItem>& index) {
    lines.clear();
    index.clear();

    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << '\n';
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        lines.push_back(line);
    }

    build_index_from_lines(lines, index);
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
    build_user_dict_cache();
    return true;
}

void update_timestamp_by_line(DictTargetFile target_file, int line_number) {
    if (line_number <= 0) return;
    std::string file_path = (target_file == DictTargetFile::CharFreq) ? get_char_freq_file_path()
                                                                      : get_user_dict_file_path();
    std::vector<std::string>* lines = (target_file == DictTargetFile::CharFreq) ? &g_char_freq_lines
                                                                                : &g_user_dict_lines;
    std::vector<PinyinIndexItem>* index = (target_file == DictTargetFile::CharFreq) ? &g_char_freq_index
                                                                                    : &g_user_dict_index;

    int idx = line_number - 1;
    if (idx < 0 || idx >= (int)lines->size()) return;

    long long now = (long long)time(nullptr);

    const std::string& raw_line = (*lines)[idx];
    std::istringstream iss(raw_line);
    std::string py, text, ts_str, count_str;
    if (!(iss >> py >> text)) return;

    int count = 1;
    if (iss >> ts_str && is_all_digits(ts_str)) {
        if (iss >> count_str && is_all_digits(count_str)) {
            count = std::stoi(count_str) + 1;
        } else {
            count = 2;
        }
    }

    std::string new_line = py + " " + text + " " + std::to_string(now) + " " + std::to_string(count);

    (*lines)[idx] = new_line;
    persist_lines_to_file(file_path, *lines);
    build_index_from_lines(*lines, *index);
    if (target_file == DictTargetFile::UserDict) build_user_dict_cache();
    if (target_file == DictTargetFile::CharFreq) build_char_freq_cache();
}
