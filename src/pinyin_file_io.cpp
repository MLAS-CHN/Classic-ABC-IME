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
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

static long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

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
    long long t0 = now_ms();
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
    write_log("FileIO: persist " + file_path + " (" + std::to_string(lines.size()) + " lines) took " +
                  std::to_string(now_ms() - t0) + " ms",
              LOG_INFO);
}

// ---- deferred write support ----
// Cross-process visibility requires the dict to hit disk promptly, but a full
// 64MB rewrite must not block the UI thread. We mark dirty on every change and
// schedule a debounced background flush (~400ms after the last change).
static std::mutex g_lines_mutex;  // guards g_user_dict_lines / g_char_freq_lines
static std::atomic<bool> g_user_dict_dirty{false};
static std::atomic<bool> g_char_freq_dirty{false};
static std::atomic<bool> g_flush_thread_running{false};

void mark_dict_dirty(DictTargetFile target_file) {
    if (target_file == DictTargetFile::UserDict) g_user_dict_dirty = true;
    else g_char_freq_dirty = true;

    bool expected = false;
    if (g_flush_thread_running.compare_exchange_strong(expected, true)) {
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            flush_dirty_dicts();
            // Flush again if changes arrived while we were writing; this
            // closes the window where a dirty flag set just before the thread
            // exits would otherwise be deferred until the next event.
            flush_dirty_dicts();
            g_flush_thread_running = false;
        }).detach();
    }
}

void flush_dirty_dicts() {
    if (g_user_dict_dirty.exchange(false)) {
        // Copy under a lock so a concurrent insert/delete/update on the main
        // thread cannot race with the background write.
        std::vector<std::string> snapshot;
        {
            std::lock_guard<std::mutex> lock(g_lines_mutex);
            snapshot = g_user_dict_lines;
        }
        persist_lines_to_file(get_user_dict_file_path(), snapshot);
    }
    if (g_char_freq_dirty.exchange(false)) {
        std::vector<std::string> snapshot;
        {
            std::lock_guard<std::mutex> lock(g_lines_mutex);
            snapshot = g_char_freq_lines;
        }
        persist_lines_to_file(get_char_freq_file_path(), snapshot);
    }
}

static void build_user_dict_cache() {
    long long t0 = now_ms();
    g_user_dict_parts.clear();
    g_user_dict_segcount_map.clear();
    g_user_dict_lookup.clear();

    g_user_dict_parts.reserve(g_user_dict_lines.size());
    g_user_dict_segcount_map.reserve(g_user_dict_lines.size() / 16);
    g_user_dict_lookup.reserve(g_user_dict_lines.size());
    for (int i = 0; i < (int)g_user_dict_lines.size(); ++i) {
        const std::string& line = g_user_dict_lines[i];
        std::string pinyin_csv = get_pinyin_from_line(line);
        std::vector<std::string> parts = split_csv(pinyin_csv);
        g_user_dict_parts.push_back(parts);
        g_user_dict_segcount_map[parts.size()].push_back(i + 1);

        // Manual field split (avoids istringstream per line).
        size_t sp1 = line.find(' ');
        if (sp1 == std::string::npos) continue;
        std::string py = line.substr(0, sp1);
        size_t sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) continue;
        std::string text = line.substr(sp1 + 1, sp2 - sp1 - 1);

        long long timestamp = 0;
        int count = 1;
        size_t sp3 = line.find(' ', sp2 + 1);
        if (sp3 != std::string::npos) {
            std::string ts_str = line.substr(sp2 + 1, sp3 - sp2 - 1);
            if (is_all_digits(ts_str)) timestamp = std::stoll(ts_str);
            size_t sp4 = line.find(' ', sp3 + 1);
            if (sp4 != std::string::npos) {
                std::string count_str = line.substr(sp3 + 1, sp4 - sp3 - 1);
                if (is_all_digits(count_str)) count = std::stoi(count_str);
            }
        }

        g_user_dict_lookup[py + " " + text] = {i + 1, timestamp, count};
    }
    write_log("FileIO: build_user_dict_cache (" + std::to_string(g_user_dict_lines.size()) + " lines) took " +
                  std::to_string(now_ms() - t0) + " ms",
              LOG_INFO);
}

static void build_char_freq_cache() {
    long long t0 = now_ms();
    g_char_freq_lookup.clear();
    g_char_freq_lookup.reserve(g_char_freq_lines.size());
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
    write_log("FileIO: build_char_freq_cache (" + std::to_string(g_char_freq_lines.size()) + " lines) took " +
                  std::to_string(now_ms() - t0) + " ms",
              LOG_INFO);
}

static void insert_line_keep_ascii_sorted(const std::string& file_path,
                                          const std::string& new_line,
                                          std::vector<std::string>& lines,
                                          std::vector<PinyinIndexItem>& index) {
    long long t0 = now_ms();
    if (new_line.empty()) return;
    std::lock_guard<std::mutex> lock(g_lines_mutex);  // guard lines & caches vs background flush

    if (lines.empty()) {
        lines.push_back(new_line);
        write_log("Insert line into " + file_path + " at line 1: " + new_line, LOG_INFO);
        mark_dict_dirty(&lines == &g_user_dict_lines ? DictTargetFile::UserDict : DictTargetFile::CharFreq);
        build_index_from_lines(lines, index);
        if (&lines == &g_user_dict_lines) {
            std::istringstream iss(new_line);
            std::string py, text, ts_str, count_str;
            if (iss >> py >> text) {
                std::vector<std::string> parts = split_csv(py);
                g_user_dict_parts.push_back(parts);
                g_user_dict_segcount_map[parts.size()].push_back(1);
                long long timestamp = 0;
                int count = 1;
                if (iss >> ts_str && is_all_digits(ts_str)) timestamp = std::stoll(ts_str);
                if (iss >> count_str && is_all_digits(count_str)) count = std::stoi(count_str);
                g_user_dict_lookup[py + " " + text] = {1, timestamp, count};
            }
        } else if (&lines == &g_char_freq_lines) {
            std::istringstream iss(new_line);
            std::string py, text, ts_str, count_str;
            if (iss >> py >> text) {
                long long timestamp = 0;
                int count = 1;
                if (iss >> ts_str && is_all_digits(ts_str)) timestamp = std::stoll(ts_str);
                if (iss >> count_str && is_all_digits(count_str)) count = std::stoi(count_str);
                g_char_freq_lookup[py + " " + text] = {1, timestamp, count};
            }
        }
        write_log("FileIO: insert(empty) " + file_path + " took " + std::to_string(now_ms() - t0) + " ms", LOG_INFO);
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
    mark_dict_dirty(&lines == &g_user_dict_lines ? DictTargetFile::UserDict : DictTargetFile::CharFreq);
    build_index_from_lines(lines, index);
    if (&lines == &g_user_dict_lines) {
        // Incremental cache update: insert into parts/segcount/lookup and
        // bump line numbers of entries after the insertion point.
        std::istringstream iss(new_line);
        std::string py, text, ts_str, count_str;
        long long timestamp = 0;
        int count = 1;
        if (iss >> py >> text) {
            if (iss >> ts_str && is_all_digits(ts_str)) timestamp = std::stoll(ts_str);
            if (iss >> count_str && is_all_digits(count_str)) count = std::stoi(count_str);
            std::vector<std::string> parts = split_csv(py);
            size_t pos = insert_pos;
            if (pos > g_user_dict_parts.size()) pos = g_user_dict_parts.size();
            g_user_dict_parts.insert(g_user_dict_parts.begin() + (std::ptrdiff_t)pos, parts);
            size_t seg_count = parts.size();
            // All line numbers after the insertion point shift by one in every
            // segment-count group (not just the new entry's group).
            for (auto& kv : g_user_dict_segcount_map) {
                for (auto& ln : kv.second)
                    if (ln > (int)insert_pos) ++ln;
            }
            g_user_dict_segcount_map[seg_count].push_back((int)insert_pos + 1);
            for (auto& kv : g_user_dict_lookup)
                if (kv.second.line_number > (int)insert_pos) ++kv.second.line_number;
            g_user_dict_lookup[py + " " + text] = {(int)insert_pos + 1, timestamp, count};
        }
    } else if (&lines == &g_char_freq_lines) {
        std::istringstream iss(new_line);
        std::string py, text, ts_str, count_str;
        long long timestamp = 0;
        int count = 1;
        if (iss >> py >> text) {
            if (iss >> ts_str && is_all_digits(ts_str)) timestamp = std::stoll(ts_str);
            if (iss >> count_str && is_all_digits(count_str)) count = std::stoi(count_str);
            for (auto& kv : g_char_freq_lookup)
                if (kv.second.line_number > (int)insert_pos) ++kv.second.line_number;
            g_char_freq_lookup[py + " " + text] = {(int)insert_pos + 1, timestamp, count};
        }
    }
    write_log("FileIO: insert " + file_path + " at line " + std::to_string(insert_pos + 1) + " took " +
                  std::to_string(now_ms() - t0) + " ms",
              LOG_INFO);
}

// ---- async cache build ----
static std::atomic<bool> g_cache_ready{false};
static std::thread g_cache_thread;
static std::mutex g_cache_mutex;
static DictReadyCallback g_ready_cb = nullptr;

void set_dict_ready_callback(DictReadyCallback cb) {
    g_ready_cb = cb;
}

void init_pinyin_data() {
    long long t0 = now_ms();
    load_file_and_build_index(get_pinyin_map_file_path(), g_pinyin_map_lines, g_pinyin_map_index);
    load_file_and_build_index(get_user_dict_file_path(), g_user_dict_lines, g_user_dict_index);
    load_file_and_build_index(get_char_freq_file_path(), g_char_freq_lines, g_char_freq_index);
    build_user_dict_cache();
    build_char_freq_cache();
    g_cache_ready = true;
    write_log("FileIO: init_pinyin_data total took " + std::to_string(now_ms() - t0) + " ms", LOG_INFO);
}

void init_pinyin_data_async() {
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    if (g_cache_thread.joinable()) {
        // A build is still running. Wait for it to finish so the reload below
        // replaces the completed (old) cache instead of racing with it.
        if (g_cache_thread.get_id() != std::this_thread::get_id())
            g_cache_thread.join();
    }
    load_file_and_build_index(get_pinyin_map_file_path(), g_pinyin_map_lines, g_pinyin_map_index);
    load_file_and_build_index(get_user_dict_file_path(), g_user_dict_lines, g_user_dict_index);
    load_file_and_build_index(get_char_freq_file_path(), g_char_freq_lines, g_char_freq_index);
    g_cache_ready = false;
    g_cache_thread = std::thread([]() {
        build_user_dict_cache();
        build_char_freq_cache();
        g_cache_ready = true;
        write_log("FileIO: async cache build finished", LOG_INFO);
        if (g_ready_cb) g_ready_cb();
    });
    g_cache_thread.detach();  // finished thread no longer holds joinable state
}

bool is_dict_cache_ready() {
    return g_cache_ready.load(std::memory_order_acquire);
}

void wait_dict_cache_ready() {
    // The build thread is detached; poll the ready flag (used at shutdown).
    for (int i = 0; i < 500 && !g_cache_ready.load(std::memory_order_acquire); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void load_file_and_build_index(const std::string& file_path,
                               std::vector<std::string>& lines,
                               std::vector<PinyinIndexItem>& index) {
    long long t0 = now_ms();
    std::lock_guard<std::mutex> lock(g_lines_mutex);
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
    write_log("FileIO: load " + file_path + " (" + std::to_string(lines.size()) + " lines) took " +
                  std::to_string(now_ms() - t0) + " ms",
              LOG_INFO);
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
    long long t0 = now_ms();
    if (line_number <= 0) return false;
    int idx = line_number - 1;
    if (idx < 0 || idx >= (int)g_user_dict_lines.size()) return false;
    std::lock_guard<std::mutex> lock(g_lines_mutex);

    std::string removed = g_user_dict_lines[idx];
    g_user_dict_lines.erase(g_user_dict_lines.begin() + idx);
    mark_dict_dirty(DictTargetFile::UserDict);
    build_index_from_lines(g_user_dict_lines, g_user_dict_index);

    // Incremental cache update: remove the entry, bump line numbers after it.
    std::istringstream iss(removed);
    std::string py, text, ts_str, count_str;
    if (iss >> py >> text) {
        g_user_dict_lookup.erase(py + " " + text);
    }
    for (auto it = g_user_dict_segcount_map.begin(); it != g_user_dict_segcount_map.end(); ++it) {
        auto& v = it->second;
        for (auto vit = v.begin(); vit != v.end();) {
            if (*vit == line_number) {
                vit = v.erase(vit);
            } else {
                if (*vit > line_number) --(*vit);
                ++vit;
            }
        }
    }
    if (idx < (int)g_user_dict_parts.size()) {
        g_user_dict_parts.erase(g_user_dict_parts.begin() + idx);
    }
    for (auto& kv : g_user_dict_lookup)
        if (kv.second.line_number > line_number) --kv.second.line_number;
    write_log("FileIO: delete line " + std::to_string(line_number) + " took " + std::to_string(now_ms() - t0) + " ms",
              LOG_INFO);
    return true;
}

void update_timestamp_by_line(DictTargetFile target_file, int line_number) {
    if (line_number <= 0) return;
    std::lock_guard<std::mutex> lock(g_lines_mutex);
    std::vector<std::string>* lines = (target_file == DictTargetFile::CharFreq) ? &g_char_freq_lines
                                                                                : &g_user_dict_lines;

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
    mark_dict_dirty(target_file);
    // No index rebuild needed: line content (ts/count) changed but the
    // leading pinyin character (the only thing the index keys on) is intact.
    if (target_file == DictTargetFile::UserDict) {
        // Incremental cache update: only touch the affected lookup entry.
        std::string key = py + " " + text;
        auto it = g_user_dict_lookup.find(key);
        if (it != g_user_dict_lookup.end()) {
            it->second.timestamp = now;
            it->second.count = count;
        }
    } else {
        // CharFreq lookup is keyed the same way; update it in place.
        std::string key = py + " " + text;
        auto it = g_char_freq_lookup.find(key);
        if (it != g_char_freq_lookup.end()) {
            it->second.timestamp = now;
            it->second.count = count;
        }
    }
}
