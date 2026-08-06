/**
 * word_matcher.cpp
 * 词语匹配模块实现文件。
 */
#include "word_matcher.h"
#include "pinyin_matcher.h"
#include "util.h"
#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <ctime>

/**
 * 获取一套拼音分段对应的候选词数组。
 * 从最短分段（2 段）开始逐步加长：
 * 取前 len 段，先宽松匹配（作为前缀）判断是否继续；再严格匹配收集候选。
 * - 若前 len 段连前缀都不存在 ⇒ 更长段组合必然也不存在，停止；
 * - 若前缀存在但当前 len 无严格匹配，继续尝试更长段
 *   （可能有"无短词但有长词"的情况，如只有 西安八桥 没有 西安）。
 * 每层的宽松匹配行号范围打印到控制台，如 [67788,69691]。
 */
static std::vector<CandidateItem> getSuitableWordLineIndexes(const std::vector<std::string>& pinyin_parts) {
    std::vector<CandidateItem> result;

    for (size_t len = 2; len <= pinyin_parts.size(); ++len) {
        std::vector<std::string> current_parts(
            pinyin_parts.begin(), pinyin_parts.begin() + static_cast<std::ptrdiff_t>(len));

        auto range = word_prefix_range(current_parts);  // 宽松匹配行号范围
        std::cout << "  len=" << len << " 前缀范围 [" << range.first << "," << range.second << "]";
        if (range.first == -1) {
            std::cout << "（前缀不存在，停止）" << std::endl;
            break;
        }

        std::vector<int> match_lines = match_segmented_word_pinyin(current_parts);
        std::cout << " 严格命中 " << match_lines.size() << std::endl;
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
    return key;
}

/**
 * 收集并去重一组拆分方案产生的候选词。
 */
static std::vector<CandidateItem> collectAndDedupeWords(
    const std::vector<std::vector<std::string>>& split_options) {

    std::vector<CandidateItem> all;
    for (const auto& pinyin_parts : split_options) {
        auto matched = getSuitableWordLineIndexes(pinyin_parts);
        all.insert(all.end(), matched.begin(), matched.end());
    }

    std::vector<CandidateItem> deduped;
    deduped.reserve(all.size());
    std::unordered_map<std::string, size_t> seen;
    long long now = (long long)time(nullptr);
    for (const auto& item : all) {
        std::string key = candidate_key(item);
        auto it = seen.find(key);
        if (it == seen.end()) {
            seen[key] = deduped.size();
            deduped.push_back(item);
        } else {
            if (item.computeScore(now) > deduped[it->second].computeScore(now)) {
                deduped[it->second] = item;
            }
        }
    }
    return deduped;
}

/**
 * 获取所有合适的词语。
 * 流程：收集 → 去重 → 按(拼音长度降序, 权重降序)排序 → 返回。
 */
std::vector<CandidateItem> getAllSuitableWords(
    const std::vector<std::vector<std::string>>& split_options) {

    std::vector<CandidateItem> deduped = collectAndDedupeWords(split_options);

    long long now = (long long)time(nullptr);
    std::sort(deduped.begin(), deduped.end(),
        [now](const CandidateItem& a, const CandidateItem& b) {
            if (a.getPinyinLength() != b.getPinyinLength())
                return a.getPinyinLength() > b.getPinyinLength();
            long long sa = a.computeScore(now);
            long long sb = b.computeScore(now);
            if (sa != sb) return sa > sb;
            return a.getTimestamp() > b.getTimestamp();
        });

    return deduped;
}
