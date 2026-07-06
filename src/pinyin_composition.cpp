/**
 * pinyin_composition.cpp
 * 拼音组合与候选准备实现文件。
 * 负责将拆分结果转为展示文本，并预留候选元素聚合入口。
 */
#include "pinyin_composition.h"
#include "pinyin_split.h"
#include "word_matcher.h"
#include "util.h"
#include "pinyin_matcher.h"
#include "pinyin_data.h"
#include "pinyin_file_io.h"
#include <algorithm>
#include <sstream>

static const int kStatusBarPrefixLen = 4;
static const int kCursorLen = 1;

/**
 * 获取智能拼接短语候选（预留接口）。
 *
 * @param split_options 多套拼音拆分方案。
 * @return 智能拼接得到的候选元素数组（当前留空）。
 */
static std::vector<CandidateItem> getSmartPhraseCandidateElements(
    const std::vector<std::vector<std::string>>& split_options) {
    (void)split_options;
    return {};
}

/**
 * 获取所有合适的字候选（预留接口）。
 *
 * @param split_options 多套拼音拆分方案。
 * @return 合适的单字候选元素数组（当前留空）。
 */
static std::vector<CandidateItem> getAllSuitableCharElements(
    const std::vector<std::vector<std::string>>& split_options) {
    if (split_options.empty() || split_options[0].empty()) {
        return {};
    }

    std::vector<CandidateItem> candidates;
    std::string charPinyin = split_options[0][0];
    std::vector<int> match_lines;

    // 尝试完全匹配
    int exact_match = find_exact_match_char(charPinyin);
    if (exact_match > 0) {
        match_lines.push_back(exact_match);
    } else {
        // 完全匹配失败，降级为前缀匹配
        match_lines = find_prefix_match_char(charPinyin);
    }

    // 解析匹配到的行，构造候选元素
    for (int line_index : match_lines) {
        // 行号转换为数组下标（0-based）
        int array_index = line_index - 1;
        if (array_index >= 0 && array_index < (int)g_pinyin_map_lines.size()) {
            const std::string& line = g_pinyin_map_lines[array_index];
            size_t space_pos = line.find(' ');
            if (space_pos != std::string::npos) {
                std::string pinyin_part = get_pinyin_from_line(line);
                std::string chars_part = line.substr(space_pos + 1);

                // 将后面的字符串按 UTF-8 字符拆分开
                for (size_t i = 0; i < chars_part.length();) {
                    unsigned char c = (unsigned char)chars_part[i];
                    size_t step = 1;
                    if ((c & 0xE0) == 0xC0) step = 2;
                    else if ((c & 0xF0) == 0xE0) step = 3;
                    else if ((c & 0xF8) == 0xF0) step = 4;

                    if (i + step > chars_part.length()) {
                        step = chars_part.length() - i; // 防止越界
                    }

                    std::string single_char = chars_part.substr(i, step);
                    
                    int weight = 1;
                    std::string freq_key = pinyin_part + " " + single_char;
                    auto freq_it = g_char_freq_lookup.find(freq_key);
                    if (freq_it != g_char_freq_lookup.end()) {
                        weight = freq_it->second.weight;
                    }
                    CandidateItem item({pinyin_part}, single_char, weight);
                    candidates.push_back(item);
                    
                    i += step;
                }
            }
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const CandidateItem& a, const CandidateItem& b) {
                         return a.getWeight() > b.getWeight();
                     });
    return candidates;
}

/**
 * 拼接任意数量的候选元素数组并返回一维结果。
 *
 * @param arrays 候选元素数组集合。
 * @return 拼接后的一维候选元素数组。
 */
static std::vector<CandidateItem> concatCandidateElementArrays(
    std::initializer_list<std::vector<CandidateItem>> arrays) {
    std::vector<CandidateItem> merged;
    size_t total_size = 0;
    for (const auto& arr : arrays) total_size += arr.size();
    merged.reserve(total_size);
    for (const auto& arr : arrays) {
        merged.insert(merged.end(), arr.begin(), arr.end());
    }
    return merged;
}

/**
 * 计算 UTF-8 字符串中的字符个数。
 */
static size_t get_utf8_char_count(const std::string& str) {
    size_t count = 0;
    for (size_t i = 0; i < str.length(); ++i) {
        unsigned char c = (unsigned char)str[i];
        if ((c & 0xC0) != 0x80) { // 不是续接字节，说明是一个新字符的开始
            count++;
        }
    }
    return count;
}

/**
 * 获取完整的候选项列表并按页拆分为二维数组。
 * 该函数整合了不同维度的候选结果，并根据 candidate_page_size 进行分页。
 *
 * @param split_options 输入的多种拼音拆分方案。
 * @param candidate_page_size 每页显示的候选词数量。
 * @return 包含所有分页候选词的二维 CandidateItem 数组。
 */
std::vector<std::vector<CandidateItem>> getAllCandidateElements(
    const std::vector<std::vector<std::string>>& split_options,
    size_t candidate_page_size) {
    std::vector<CandidateItem> smart_phrase_candidates =
        getSmartPhraseCandidateElements(split_options);
    std::vector<CandidateItem> suitable_word_candidates =
        getAllSuitableWords(split_options);
    std::vector<CandidateItem> suitable_char_candidates =
        getAllSuitableCharElements(split_options);

    std::vector<CandidateItem> flat_candidates = concatCandidateElementArrays(
        {smart_phrase_candidates, suitable_word_candidates, suitable_char_candidates});

    std::vector<std::vector<CandidateItem>> paged_candidates;
    if (flat_candidates.empty() || candidate_page_size == 0) {
        return paged_candidates;
    }

    size_t current_index = 0;
    while (current_index < flat_candidates.size()) {
        std::vector<CandidateItem> current_page;
        int count_to_take = static_cast<int>(std::min(candidate_page_size, flat_candidates.size() - current_index));
        for (int i = 0; i < count_to_take; ++i) {
            current_page.push_back(flat_candidates[current_index++]);
        }
        paged_candidates.push_back(current_page);
    }

    return paged_candidates;
}

/**
 * 构建用于状态栏显示的最终文本字符串。
 * 格式示例："pinyin|¹拼音 ²便宜"
 * 
 * 主要逻辑包括：
 * 1. 调用保守拆分算法获取拼音的显示形式（带逗号分隔）。
 * 2. 根据原始缓冲区的光标位置，在显示串中插入 "|" 符号表示虚拟光标。
 * 3. 根据当前页码从分页后的候选列表中提取对应的候选词数组进行展示。
 * 4. 为每个候选词配上上标数字序号（1-9 对应 ¹-⁹，10 对应 ⁰）。
 *
 * @param raw_buffer 用户输入的原始拼音字符串（不含分隔符）。
 * @param virtual_cursor 基于 raw_buffer 的光标下标位置。
 * @param paged_candidates 已经分好页的候选词二维数组。
 * @param candidate_page 当前显示的候选词页码（对应二维数组的第一层下标）。
 * @return 格式化后的状态栏显示字符串。
 */
static std::string buildDisplayFromSplitOptions(
    const std::string& raw_buffer,
    size_t virtual_cursor,
    const std::vector<std::vector<CandidateItem>>& paged_candidates,
    size_t candidate_page) {
    // 1. 调用拼音拆分核心算法，获取带逗号分隔符的拼音显示串
    // 例如：输入 "pinyin" -> 返回 "pin,yin"
    std::string display = conservativePinyinSplitMain(raw_buffer);

    // 安全检查：确保光标位置不超出原始输入缓冲区的长度
    if (virtual_cursor > raw_buffer.size()) virtual_cursor = raw_buffer.size();

    // 2. 将“基于原始缓冲区”的光标位置映射到“带逗号分隔符”的显示串中
    // 逻辑：遍历显示串，跳过自动生成的逗号，直到匹配到原始光标所在的字符数
    size_t display_pos = 0;
    size_t raw_count = 0;
    while (display_pos < display.size() && raw_count < virtual_cursor) {
        if (display[display_pos] != ',') {
            ++raw_count;
        }
        ++display_pos;
    }

    // 3. 在映射后的位置插入虚拟光标符号 "|"
    // 例如："pin,yin" 且光标在末尾 -> "pin,yin|"
    std::string pinyin_display = display.substr(0, display_pos) + "|" + display.substr(display_pos);

    // 如果没有候选项，则直接返回拼音部分
    if (paged_candidates.empty()) {
        return pinyin_display;
    }

    // 安全检查：确保页码不越界
    if (candidate_page >= paged_candidates.size()) {
        candidate_page = paged_candidates.size() - 1;
    }

    // 提取当前页需要显示的候选项数组
    const std::vector<CandidateItem>& current_page_candidates = paged_candidates[candidate_page];
    
    // 定义数字上标字符数组 (0-9 对应 ⁰-⁹)
    static const char* superscripts[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string candidate_display = "";

    // 6. 遍历当前页的候选项，构建带上标序号的展示文本
    for (size_t i = 0; i < current_page_candidates.size(); ++i) {
        size_t num = i + 1; // 当前页内的序号 (1, 2, 3...)
        
        // 将序号转换为上标数字显示
        if (num < 10) {
            candidate_display += superscripts[num]; // 1-9 使用对应上标
        } else if (num == 10) {
            candidate_display += superscripts[0];   // 10 使用上标 ⁰
        }
        
        // 拼接候选项文本
        candidate_display += current_page_candidates[i].getText();
        
        // 候选项之间添加空格分隔
        if (i + 1 < current_page_candidates.size()) candidate_display += " ";
    }

    // 7. 合并拼音部分和候选词部分并返回最终结果
    return pinyin_display + candidate_display;
}

/**
 * 输入法预编辑文本构建入口。
 * 流程：
 * 1) 对当前缓冲执行拆分；
 * 2) 拉取所有候选并按页拆分为二维数组；
 * 3) 根据当前页码返回用于状态栏展示的文本。
 *
 * @param pinyin_buffer 当前拼音输入缓冲。
 * @param virtual_cursor 虚拟光标位置（基于原始缓冲，范围 0..length）。
 * @param candidate_page 当前请求显示的页码。
 * @param candidate_page_size 每页候选数量。
 * @param out_paged_candidates 若非空，则通过此参数传回全部分页后的候选。
 * @return 状态栏展示文本。
 */
std::string buildComposingDisplayText(const std::string& pinyin_buffer,
                                      size_t virtual_cursor,
                                      size_t candidate_page,
                                      size_t candidate_page_size,
                                      std::vector<std::vector<CandidateItem>>* out_paged_candidates) {
    if (pinyin_buffer.empty()) return "";
    
    // 1. 获取拆分方案
    std::vector<std::vector<std::string>> split_options = splitConservativePinyin(pinyin_buffer);
    
    // 2. 获取所有候选项并按页拆分
    std::vector<std::vector<CandidateItem>> paged_candidates = getAllCandidateElements(split_options, candidate_page_size);
    
    // 3. 如果需要，返回分页后的候选列表给调用方（用于翻页逻辑控制）
    if (out_paged_candidates) {
        *out_paged_candidates = paged_candidates;
    }
    
    // 4. 构建并返回展示文本
    return buildDisplayFromSplitOptions(pinyin_buffer, virtual_cursor, paged_candidates, candidate_page);
}
