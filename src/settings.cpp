#include "settings.h"
#include "util.h"
#include <fstream>
#include <sstream>

static std::string g_config_dir;
static int g_page_size = 10;
static bool g_log_enabled = false;  // 默认不生成日志（无配置文件时）
static int g_log_level = 0;         // 0=INFO 1=DEBUG

static const int kPageSizeMin = 5;
static const int kPageSizeMax = 10;

static std::string config_path() {
    if (g_config_dir.empty()) return "settings.ini";
    return g_config_dir + "\\settings.ini";
}

void load_settings(const std::string& dir) {
    g_config_dir = dir;
    g_page_size = 10;
    g_log_enabled = false;  // 缺省：每页 10 个、不生成日志
    g_log_level = 0;        // 缺省：INFO

    std::ifstream f(config_path());
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\r')) key.pop_back();
        while (!value.empty() && (value.back() == ' ' || value.back() == '\r')) value.pop_back();

        if (key == "candidate_page_size") {
            try {
                int v = std::stoi(value);
                if (v >= kPageSizeMin && v <= kPageSizeMax) g_page_size = v;
            } catch (...) {}
        } else if (key == "log_enabled") {
            g_log_enabled = (value == "1" || value == "true");
        } else if (key == "log_level") {
            try {
                int v = std::stoi(value);
                if (v == 0 || v == 1) g_log_level = v;
            } catch (...) {}
        }
    }

    // 日志开关与等级立即生效（无需重启）。
    set_log_file_enabled(g_log_enabled);
    set_log_level(g_log_level == 1 ? LOG_DEBUG : LOG_INFO);
}

bool save_settings() {
    std::ofstream f(config_path(), std::ios::trunc);
    if (!f.is_open()) return false;
    f << "candidate_page_size=" << g_page_size << "\n";
    f << "log_enabled=" << (g_log_enabled ? "1" : "0") << "\n";
    f << "log_level=" << g_log_level << "\n";
    return true;
}

int get_page_size() {
    return g_page_size;
}

void set_page_size(int size) {
    if (size < kPageSizeMin) size = kPageSizeMin;
    if (size > kPageSizeMax) size = kPageSizeMax;
    g_page_size = size;
}

bool is_log_enabled() {
    return g_log_enabled;
}

void set_log_enabled(bool enabled) {
    g_log_enabled = enabled;
    set_log_file_enabled(enabled);
    save_settings();
}

int get_log_level() {
    return g_log_level;
}

void set_log_level(int level) {
    if (level != 0 && level != 1) level = 0;
    g_log_level = level;
    set_log_level(level == 1 ? LOG_DEBUG : LOG_INFO);
    save_settings();
}
