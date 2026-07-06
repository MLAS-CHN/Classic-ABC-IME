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
#include <algorithm>
#include <cstdlib>
#include <utility>



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
    std::string pinyin_csv = join_csv(pinyin_parts_);
    std::string key = pinyin_csv + " " + text_;

    if (pinyin_parts_.size() == 1) {
        auto it = g_char_freq_lookup.find(key);
        if (it != g_char_freq_lookup.end()) return it->second.line_number;
        return -1;
    }
    auto it = g_user_dict_lookup.find(key);
    if (it != g_user_dict_lookup.end()) return it->second;
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
    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateItem& a, const CandidateItem& b) {
                  return a.getWeight() > b.getWeight();
              });
}
