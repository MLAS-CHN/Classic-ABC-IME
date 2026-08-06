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
#include <cstdint>
#include <ctime>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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
    // 内存缓存已全部移除：匹配用 mmap 的 parts 硬盘缓存，
    // findSourceLineNumber 用二分。此函数构建 parts 缓存。
    build_parts_cache();
    write_log("FileIO: build_user_dict_cache (" + std::to_string(g_user_dict_lines.size()) + " lines) took " +
                  std::to_string(now_ms() - t0) + " ms",
              LOG_INFO);
}

// ---- 词库 parts 硬盘缓存（mmap）----
// 二进制格式：
//   [u32 magic=0x50415943][u32 line_count]
//   [offset 表: line_count × u32]      每行数据区起始偏移
//   [数据区: 逐行]  u8 seg_count, 每段: u8 seg_len + seg_len 字节
static const uint32_t kPartsMagic = 0x50415943;  // "PYAC"

static HANDLE g_parts_file = INVALID_HANDLE_VALUE;
static HANDLE g_parts_mapping = nullptr;
static const uint8_t* g_parts_base = nullptr;
static uint32_t g_parts_line_count = 0;

std::string get_parts_cache_path() {
    return join_path(join_path(get_pinyin_data_dir(), "cache"), "user_dict_parts.bin");
}

// 逐段遍历拼音 CSV（如 "a,bc,d" → 依次回调 "a","bc","d"）。
template <typename F>
static void for_each_segment(const std::string& pinyin_csv, F&& fn) {
    size_t pos = 0;
    while (pos <= pinyin_csv.size()) {
        size_t next = pinyin_csv.find(',', pos);
        size_t end = (next == std::string::npos) ? pinyin_csv.size() : next;
        size_t len = end - pos;
        if (len > 0) fn(pinyin_csv.data() + pos, len);
        if (next == std::string::npos) break;
        pos = next + 1;
    }
}

// 生成 parts cache 文件（词库变化时重建）。返回文件路径。
static std::string generate_parts_cache_file() {
    std::string path = get_parts_cache_path();
    std::string cache_dir = join_path(get_pinyin_data_dir(), "cache");
    std::filesystem::create_directories(cache_dir);

    // 每行数据字节数 = 段数(1) + Σ(长度(1) + 内容)。
    auto row_bytes = [](const std::string& line) {
        std::string py = get_pinyin_from_line(line);
        uint64_t size = 1;  // 段数
        for_each_segment(py, [&size](const char* s, size_t n) { size += 1 + n; });
        return size;
    };
    uint64_t exact_size = 0;
    for (const auto& line : g_user_dict_lines) exact_size += row_bytes(line);

    uint64_t header_size = 8 + (uint64_t)g_user_dict_lines.size() * 4;

    // 写入临时文件。
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) return path;

        uint32_t magic = kPartsMagic;
        uint32_t count = (uint32_t)g_user_dict_lines.size();
        f.write((const char*)&magic, 4);
        f.write((const char*)&count, 4);

        // 先写完整 offset 表（占位），再写数据区。
        uint32_t cur_offset = (uint32_t)header_size;
        for (uint32_t i = 0; i < count; ++i) {
            f.write((const char*)&cur_offset, 4);
            cur_offset += (uint32_t)row_bytes(g_user_dict_lines[i]);
        }

        // 写数据区。
        for (uint32_t i = 0; i < count; ++i) {
            const std::string& line = g_user_dict_lines[i];
            std::string py = get_pinyin_from_line(line);
            uint8_t seg_count = 0;
            for_each_segment(py, [&seg_count](const char*, size_t) { ++seg_count; });
            f.write((const char*)&seg_count, 1);
            for_each_segment(py, [&f](const char* s, size_t n) {
                uint8_t slen = (uint8_t)n;
                f.write((const char*)&slen, 1);
                f.write(s, (std::streamsize)n);
            });
        }
        f.close();
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return path;
}

// unmap 并关闭句柄（不删文件）。
static void unmap_parts_cache() {
    if (g_parts_base) {
        UnmapViewOfFile((LPVOID)g_parts_base);
        g_parts_base = nullptr;
    }
    if (g_parts_mapping) {
        CloseHandle(g_parts_mapping);
        g_parts_mapping = nullptr;
    }
    if (g_parts_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_parts_file);
        g_parts_file = INVALID_HANDLE_VALUE;
    }
    g_parts_line_count = 0;
}

// mmap 映射 parts cache 文件；返回是否成功。
static bool map_parts_cache() {
    std::string path = get_parts_cache_path();
    int n = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (n <= 0) return false;
    std::wstring wpath((size_t)(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], n);

    g_parts_file = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_parts_file == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(g_parts_file, &size) || size.QuadPart <= 0) {
        CloseHandle(g_parts_file);
        g_parts_file = INVALID_HANDLE_VALUE;
        return false;
    }

    g_parts_mapping = CreateFileMappingW(g_parts_file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!g_parts_mapping) {
        CloseHandle(g_parts_file);
        g_parts_file = INVALID_HANDLE_VALUE;
        return false;
    }

    g_parts_base = (const uint8_t*)MapViewOfFile(g_parts_mapping, FILE_MAP_READ, 0, 0, 0);
    if (!g_parts_base) {
        CloseHandle(g_parts_mapping);
        CloseHandle(g_parts_file);
        g_parts_mapping = nullptr;
        g_parts_file = INVALID_HANDLE_VALUE;
        return false;
    }

    // 校验头。
    if (size.QuadPart >= 8) {
        uint32_t magic = *(const uint32_t*)g_parts_base;
        g_parts_line_count = *(const uint32_t*)(g_parts_base + 4);
        if (magic != kPartsMagic || g_parts_line_count != g_user_dict_lines.size()) {
            unmap_parts_cache();
            return false;
        }
        return true;
    }
    unmap_parts_cache();
    return false;
}

void build_parts_cache() {
    long long t0 = now_ms();
    std::string path = get_parts_cache_path();

    // 检查现有 cache 是否有效（文件存在 + 行数匹配）。不匹配则重建。
    bool need_rebuild = true;
    {
        std::ifstream f(path, std::ios::binary);
        if (f.is_open()) {
            uint32_t magic = 0, count = 0;
            f.read((char*)&magic, 4);
            f.read((char*)&count, 4);
            if (magic == kPartsMagic && count == (uint32_t)g_user_dict_lines.size()) {
                need_rebuild = false;
            }
        }
    }
    if (need_rebuild) {
        path = generate_parts_cache_file();
    }

    bool ok = map_parts_cache();
    write_log("FileIO: build_parts_cache (" + std::to_string(g_parts_line_count) + " lines, mmap=" +
                  (ok ? "OK" : "FAIL") + ") took " + std::to_string(now_ms() - t0) + " ms",
              LOG_INFO);
}

void invalidate_parts_cache() {
    unmap_parts_cache();
    // cache 失效后，匹配回退按需 split；下次 init_pinyin_data 重建。
}

const char* get_parts_cached_line(int line_index) {
    if (!g_parts_base || line_index < 0 || line_index >= (int)g_parts_line_count) return nullptr;
    const uint32_t* offsets = (const uint32_t*)(g_parts_base + 8);
    return (const char*)(g_parts_base + offsets[line_index]);
}

uint8_t get_parts_cached_seg_count(const char* p) {
    if (!p) return 0;
    return (uint8_t)*p;
}

const char* get_parts_cached_seg(const char* p, int seg_index, int& seg_len) {
    if (!p) { seg_len = 0; return nullptr; }
    uint8_t count = (uint8_t)*p;
    if (seg_index < 0 || seg_index >= (int)count) { seg_len = 0; return nullptr; }
    const uint8_t* cur = (const uint8_t*)p + 1;  // 跳过段数
    for (int i = 0; i < seg_index; ++i) {
        uint8_t len = *cur++;
        cur += len;
    }
    uint8_t len = *cur++;
    seg_len = (int)len;
    return (const char*)cur;
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
        if (&lines == &g_user_dict_lines) invalidate_parts_cache();
        build_index_from_lines(lines, index);
        if (&lines == &g_char_freq_lines) {
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
    if (&lines == &g_user_dict_lines) invalidate_parts_cache();
    build_index_from_lines(lines, index);
    if (&lines == &g_char_freq_lines) {
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

void release_dict_memory() {
    // 窗口失焦：释放词库全部内存（lines/index/parts mmap），文件保留。
    unmap_parts_cache();
    g_user_dict_lines.clear();
    g_user_dict_lines.shrink_to_fit();
    g_user_dict_index.clear();
    g_pinyin_map_lines.clear();
    g_pinyin_map_lines.shrink_to_fit();
    g_pinyin_map_index.clear();
    g_char_freq_lines.clear();
    g_char_freq_lookup.clear();
    g_cache_ready = false;
    write_log("FileIO: release_dict_memory (dict memory freed)", LOG_INFO);
}

void init_pinyin_data_async() {
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    if (g_cache_thread.joinable()) {
        // A build is still running. Wait for it to finish so the reload below
        // replaces the completed (old) cache instead of racing with it.
        if (g_cache_thread.get_id() != std::this_thread::get_id())
            g_cache_thread.join();
    }
    g_cache_ready = false;
    // 全后台：load 文件 + 构建缓存都在后台线程，聚焦时主线程不卡。
    g_cache_thread = std::thread([]() {
        load_file_and_build_index(get_pinyin_map_file_path(), g_pinyin_map_lines, g_pinyin_map_index);
        load_file_and_build_index(get_user_dict_file_path(), g_user_dict_lines, g_user_dict_index);
        load_file_and_build_index(get_char_freq_file_path(), g_char_freq_lines, g_char_freq_index);
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
    invalidate_parts_cache();
    build_index_from_lines(g_user_dict_lines, g_user_dict_index);
    // UserDict 无内存查找表（findSourceLineNumber 用二分），无需增量更新。
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
    // UserDict 无内存查找表（findSourceLineNumber 用二分），无需增量更新。
    if (target_file == DictTargetFile::CharFreq) {
        // CharFreq lookup is keyed the same way; update it in place.
        std::string key = py + " " + text;
        auto it = g_char_freq_lookup.find(key);
        if (it != g_char_freq_lookup.end()) {
            it->second.timestamp = now;
            it->second.count = count;
        }
    }
}
