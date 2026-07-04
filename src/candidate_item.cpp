/**
 * candidate_item.cpp
 * 候选元素构建逻辑实现文件。
 */
#include "candidate_item.h"
#include "pinyin_data.h"
#include "pinyin_file_io.h"
#include "util.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <utility>



static int partition_by_weight_desc(std::vector<CandidateItem>& candidates, int low, int high) {
    int pivot_weight = candidates[(size_t)high].getWeight();
    int i = low - 1;
    for (int j = low; j < high; ++j) {
        if (candidates[(size_t)j].getWeight() > pivot_weight) {
            ++i;
            std::swap(candidates[(size_t)i], candidates[(size_t)j]);
        }
    }
    std::swap(candidates[(size_t)(i + 1)], candidates[(size_t)high]);
    return i + 1;
}

static void quick_sort_by_weight_desc_impl(std::vector<CandidateItem>& candidates, int low, int high) {
    if (low >= high) return;
    int pivot_index = partition_by_weight_desc(candidates, low, high);
    quick_sort_by_weight_desc_impl(candidates, low, pivot_index - 1);
    quick_sort_by_weight_desc_impl(candidates, pivot_index + 1, high);
}

std::string CandidateItem::toString() const {
    std::string pinyin_csv = join_csv(pinyin_parts_);
    return pinyin_csv + " " + text_ + " " + std::to_string(weight_);
}

std::string CandidateItem::getSourceFileName() const {
    if (pinyin_parts_.size() == 1) {
        return get_char_freq_file_path();
    }
    return get_user_dict_file_path();
}

int CandidateItem::findSourceLineNumber() const {
    std::string file_path = getSourceFileName();
    std::string pinyin_csv = join_csv(pinyin_parts_);
    std::string target = pinyin_csv + " " + text_;

    std::ifstream file(file_path);
    if (!file.is_open()) return -1;

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        if (line == target) return line_number;
        if (line.rfind(target + " ", 0) == 0) return line_number;
    }
    return -1;
}

CandidateItem CandidateItem::fromCharDictLineNumber(int line_number) {
    // 预留：后续按字库格式解析并构建。
    (void)line_number;
    return CandidateItem();
}

CandidateItem CandidateItem::fromWordDictLineNumber(int line_number) {
    if (line_number <= 0 || line_number > (int)g_user_dict_lines.size()) {
        return CandidateItem();
    }

    const std::string& line = g_user_dict_lines[(size_t)line_number - 1];
    std::istringstream iss(line);

    std::string pinyin_csv;
    std::string text;
    std::string weight_str;
    int weight = 1;

    if (!(iss >> pinyin_csv >> text)) {
        return CandidateItem();
    }

    if (iss >> weight_str) {
        char* end_ptr = nullptr;
        long parsed = std::strtol(weight_str.c_str(), &end_ptr, 10);
        if (end_ptr != weight_str.c_str() && *end_ptr == '\0') {
            weight = (int)parsed;
        }
    }

    std::vector<std::string> pinyin_parts = split_csv(pinyin_csv);
    return CandidateItem(pinyin_parts, text, weight);
}

CandidateItem CandidateItem::mergeCandidateItems(const std::vector<CandidateItem>& items) {
    if (items.empty()) return CandidateItem();

    std::vector<std::string> merged_pinyin_parts;
    std::string merged_text;
    for (const auto& item : items) {
        const auto& parts = item.getPinyinParts();
        merged_pinyin_parts.insert(merged_pinyin_parts.end(), parts.begin(), parts.end());
        merged_text += item.getText();
    }

    if (merged_pinyin_parts.empty() || merged_text.empty()) return CandidateItem();
    return CandidateItem(merged_pinyin_parts, merged_text, 1);
}

void CandidateItem::quickSortByWeightDesc(std::vector<CandidateItem>& candidates) {
    if (candidates.empty()) return;
    quick_sort_by_weight_desc_impl(candidates, 0, (int)candidates.size() - 1);
}
