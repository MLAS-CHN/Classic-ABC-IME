// pinyin_bench.cpp - 最小化验证：输入拼音，显示拆分方案与候选词。
// 独立项目：算法核心在 test/core/（从 src/ 复制，可独立修改验证）。
//
// 构建（在 test 目录）：
//   cl /nologo /EHsc /std:c++17 /utf-8 /I core \
//       pinyin_bench.cpp \
//       core/pinyin_composition.cpp core/pinyin_split.cpp core/word_matcher.cpp \
//       core/pinyin_matcher.cpp core/pinyin_data.cpp core/pinyin_file_io.cpp \
//       core/candidate_item.cpp core/util.cpp \
//       /Fe:pinyin_bench.exe
//
// 使用：
//   pinyin_bench [data目录]   // 默认用 ./data
//   逐字输入实时刷新：字母=输入，退格=删除，Esc=清空，回车=退出。

#include "core/pinyin_composition.h"
#include "core/pinyin_split.h"
#include "core/pinyin_file_io.h"
#include "core/pinyin_data.h"
#include "core/candidate_item.h"

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <conio.h>
#include <cstdlib>
#include <windows.h>

static long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static std::string join_csv(const std::vector<std::string>& parts) {
    std::string r;
    for (size_t i = 0; i < parts.size(); ++i) {
        r += parts[i];
        if (i + 1 < parts.size()) r += ",";
    }
    return r;
}

// 对单个拼音串执行拆分+候选并输出。
static void run_one(const std::string& input, bool show_header) {
    if (show_header) {
        std::cout << "== pinyin bench ==" << std::endl;
        std::cout << "data dir: " << get_pinyin_data_dir() << std::endl;
        std::cout << "user_dict lines: " << g_user_dict_lines.size() << std::endl;
    }
    std::cout << "输入: [" << input << "]" << std::endl;
    if (input.empty()) return;

    long long t0 = now_ms();

    // 1. 拆分（含原始激进方案下标）
    auto split = splitConservativePinyinEx(input);

    // 2. 候选
    std::vector<std::vector<CandidateItem>> pages =
        getAllCandidateElements(split.options, 10, split.raw_aggressive_index);

    long long total_ms = now_ms() - t0;

    // 3. 显示拆分方案
    std::cout << "拆分方案 (" << split.options.size() << "):" << std::endl;
    for (size_t i = 0; i < split.options.size(); ++i) {
        std::cout << "  [" << i << "] " << join_csv(split.options[i]) << std::endl;
    }

    // 4. 显示候选：单行空格分隔，最多 30 个（带拼音段数便于排查顺序）
    std::cout << "候选 (" << pages.size() << " 页): ";
    size_t shown = 0;
    for (size_t p = 0; p < pages.size() && shown < 30; ++p) {
        for (const auto& item : pages[p]) {
            if (shown >= 30) break;
            std::cout << item.getText() << "(" << item.getPinyinLength() << ")";
            shown++;
            if (shown < 30) std::cout << " ";
        }
    }
    std::cout << std::endl;
    std::cout << "耗时: " << total_ms << " ms" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // 控制台 UTF-8 输出，避免中文乱码。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 数据目录：默认 ./data；-p 参数为单次模式（第二个参数是拼音，跑一次输出退出）
    std::string data_dir = "data";
    std::string one_shot;
    int i = 1;
    while (i < argc) {
        std::string a = argv[i];
        if (a == "-p" && i + 1 < argc) {
            one_shot = argv[i + 1];
            i += 2;
        } else {
            data_dir = a;
            i += 1;
        }
    }
    set_pinyin_data_dir(data_dir);
    init_pinyin_data();

    // 单次模式：跑一个拼音串就退出
    if (!one_shot.empty()) {
        run_one(one_shot, true);
        return 0;
    }

    std::cout << "== pinyin bench ==" << std::endl;
    std::cout << "data dir: " << get_pinyin_data_dir() << std::endl;
    std::cout << "user_dict lines: " << g_user_dict_lines.size() << std::endl;
    std::cout << "逐字输入实时刷新：字母=输入，退格=删除，Esc=清空，回车=退出。" << std::endl;
    std::cout << std::endl;

    std::string input;
    while (true) {
        // 逐字符读取（无需回车）
        int ch = _getch();
        if (ch == '\r' || ch == '\n') break;              // 回车退出
        if (ch == 27) { input.clear(); }                  // Esc 清空
        else if (ch == 8) {                               // 退格
            if (!input.empty()) input.pop_back();
        } else if (ch >= 'a' && ch <= 'z') {              // 字母输入
            input.push_back((char)ch);
        } else if (ch >= 'A' && ch <= 'Z') {              // 大写转小写
            input.push_back((char)(ch - 'A' + 'a'));
        } else {
            continue;
        }

        // 清屏重绘
        system("cls");
        run_one(input, true);
    }
    return 0;
}
