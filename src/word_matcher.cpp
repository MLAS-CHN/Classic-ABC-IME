/**
 * word_matcher.cpp
 * 词语匹配模块实现文件。
 */
#include "word_matcher.h"
#include "pinyin_matcher.h"
#include "util.h"
#include <algorithm>
#include <unordered_set>

/**
 * 获取一套拼音分段对应的候选词数组。
 * 从完整分段开始逐步缩短，将命中行号转为候选对象并扁平化。
 */
static std::vector<CandidateItem> getSuitableWordLineIndexes(const std::vector<std::string>& pinyin_parts) {
    std::vector<CandidateItem> result;

    for (size_t len = pinyin_parts.size(); len > 1; --len) {
        std::vector<std::string> current_parts(
            pinyin_parts.begin(), pinyin_parts.begin() + static_cast<std::ptrdiff_t>(len));
        std::vector<int> match_lines = match_segmented_word_pinyin(current_parts);
        for (int line_number : match_lines) {
            result.push_back(CandidateItem::fromWordDictLineNumber(line_number));
        }
    }

    return result;
}

/**
 * 构建候选去重键：拼音CSV + 文本 + 权重。
 */
static std::string candidate_key(const CandidateItem& item) {
    std::string key = join_csv(item.getPinyinParts());
    key += "||";
    key += item.getText();
    key += "||";
    key += std::to_string(item.getWeight());
    return key;
}

/**
 * 获取所有合适的词语。
 * 流程：收集 → 去重 → 按(拼音长度降序, 权重降序)排序 → 返回。
 */
std::vector<CandidateItem> getAllSuitableWords(
    const std::vector<std::vector<std::string>>& split_options) {

    std::vector<CandidateItem> all;
    for (const auto& pinyin_parts : split_options) {
        auto matched = getSuitableWordLineIndexes(pinyin_parts);
        all.insert(all.end(), matched.begin(), matched.end());
    }

    std::vector<CandidateItem> deduped;
    deduped.reserve(all.size());
    std::unordered_set<std::string> seen;
    for (const auto& item : all) {
        if (seen.insert(candidate_key(item)).second) {
            deduped.push_back(item);
        }
    }

    std::sort(deduped.begin(), deduped.end(),
        [](const CandidateItem& a, const CandidateItem& b) {
            if (a.getPinyinLength() != b.getPinyinLength())
                return a.getPinyinLength() > b.getPinyinLength();
            return a.getWeight() > b.getWeight();
        });

    return deduped;
}
