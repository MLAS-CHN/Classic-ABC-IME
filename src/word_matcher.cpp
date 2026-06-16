/**
 * word_matcher.cpp
 * 词语匹配模块实现文件。
 */
#include "word_matcher.h"
#include "pinyin_matcher.h"
#include <algorithm>
#include <unordered_set>

/**
 * 获取一套拼音分段对应的候选词数组。
 * 处理方式是从完整分段开始逐步减小末尾字量，并把每轮命中行号转换为候选对象。
 * 计划用途：
 * 1) 基于词库索引缩小检索范围；
 * 2) 找出与当前拼音分段匹配的词条行号；
 * 3) 将词条行号组装为候选对象并按权重排序。
 *
 * @param pinyin_parts 一套拼音分段（如 ["ni", "hao"]）。
 * @return 一维候选元素对象数组。
 */
static std::vector<CandidateItem> getSuitableWordLineIndexes(const std::vector<std::string>& pinyin_parts) {
    std::vector<std::vector<int>> grouped_match_lines;

    // 从完整分段开始逐步缩短（仅处理词：至少两段拼音）
    for (size_t len = pinyin_parts.size(); len > 1; --len) {
        std::vector<std::string> current_parts(
            pinyin_parts.begin(), pinyin_parts.begin() + static_cast<std::ptrdiff_t>(len));
        std::vector<int> match_lines = match_segmented_word_pinyin(current_parts);
        grouped_match_lines.push_back(match_lines);
    }

    // 清理二维数组中的空数组
    grouped_match_lines.erase(
        std::remove_if(grouped_match_lines.begin(), grouped_match_lines.end(),
                       [](const std::vector<int>& item) { return item.empty(); }),
        grouped_match_lines.end());

    // 将二维行号数组转换为二维候选元素对象数组
    // grouped_candidate_items: 外层代表“不同截断长度的一组结果”，内层是该组的候选对象。
    std::vector<std::vector<CandidateItem>> grouped_candidate_items;
    // 预留外层容量，避免 push_back 过程中频繁扩容。
    grouped_candidate_items.reserve(grouped_match_lines.size());
    // 逐组处理行号（每个 line_group 对应一次截断长度下的命中结果）。
    for (const auto& line_group : grouped_match_lines) {
        // 当前组的候选对象容器。
        std::vector<CandidateItem> candidate_group;
        // 预留内层容量，长度与当前组行号数量一致。
        candidate_group.reserve(line_group.size());
        // 把每个词库行号转换成 CandidateItem 对象。
        for (int line_number : line_group) {
            candidate_group.push_back(CandidateItem::fromWordDictLineNumber(line_number));
        }
        // 在组内按权重降序排序（高权重优先）。
        // CandidateItem::quickSortByWeightDesc(candidate_group);
        // 将处理完（已排序）的组放入二维候选数组。
        grouped_candidate_items.push_back(candidate_group);
    }

    // 当前下游先用一维结构，这里把二维候选扁平化。
    std::vector<CandidateItem> flattened_candidates;
    // 先遍历每一组候选。
    for (const auto& group : grouped_candidate_items) {
        // 再把组内每个候选按当前顺序追加到一维数组末尾。
        for (const auto& item : group) {
            flattened_candidates.push_back(item);
        }
    }
    // 返回扁平化后的候选数组（保留了“组内已排序”的相对顺序）。
    return flattened_candidates;
}

//获取所有合适的词语，不包括智能拼出来的，也不包括字。这个函数的主要目的也就只是把二维数组拆成一维数组罢了
std::vector<CandidateItem> getAllSuitableWords(
    const std::vector<std::vector<std::string>>& split_options) {
    std::vector<std::vector<CandidateItem>> grouped_matched_candidates;
    for (const auto& pinyin_parts : split_options) {
        std::vector<CandidateItem> matched_candidates = getSuitableWordLineIndexes(pinyin_parts);
        grouped_matched_candidates.push_back(matched_candidates);
    }

    // 先扁平化所有候选
    std::vector<CandidateItem> flattened_candidates;
    for (const auto& group : grouped_matched_candidates) {
        for (const auto& item : group) {
            flattened_candidates.push_back(item);
        }
    }

    // 扁平化后去重：判定依据为“拼音数组 + 文本 + 权重”三者完全一致
    std::vector<CandidateItem> deduped_candidates;
    deduped_candidates.reserve(flattened_candidates.size());
    std::unordered_set<std::string> seen_keys;
    for (const auto& item : flattened_candidates) {
        std::string key;
        const auto& pinyin_parts = item.getPinyinParts();
        for (size_t i = 0; i < pinyin_parts.size(); ++i) {
            key += pinyin_parts[i];
            if (i < pinyin_parts.size() - 1) key += ",";
        }
        key += "||";
        key += item.getText();
        key += "||";
        key += std::to_string(item.getWeight());

        if (seen_keys.insert(key).second) {
            deduped_candidates.push_back(item);
        }
    }
    flattened_candidates = deduped_candidates;

    // 再按拼音长度重建二维数组：
    // 1) 组内长度一致；2) 组间按长度降序。
    std::vector<size_t> unique_lengths;
    for (const auto& item : flattened_candidates) {
        size_t len = item.getPinyinLength();
        if (std::find(unique_lengths.begin(), unique_lengths.end(), len) == unique_lengths.end()) {
            unique_lengths.push_back(len);
        }
    }
    std::sort(unique_lengths.begin(), unique_lengths.end(), std::greater<size_t>());

    std::vector<std::vector<CandidateItem>> regrouped_by_pinyin_length;
    regrouped_by_pinyin_length.reserve(unique_lengths.size());
    for (size_t len : unique_lengths) {
        std::vector<CandidateItem> same_length_group;
        for (const auto& item : flattened_candidates) {
            if (item.getPinyinLength() == len) {
                same_length_group.push_back(item);
            }
        }
        regrouped_by_pinyin_length.push_back(same_length_group);
    }
    grouped_matched_candidates = regrouped_by_pinyin_length;

    // 重分组后，对每个长度组再按权重降序排序
    for (auto& group : grouped_matched_candidates) {
        CandidateItem::quickSortByWeightDesc(group);
    }

    // 扁平化并返回最终候选数组
    std::vector<CandidateItem> final_candidates;
    for (const auto& group : grouped_matched_candidates) {
        final_candidates.insert(final_candidates.end(), group.begin(), group.end());
    }
    return final_candidates;
}
