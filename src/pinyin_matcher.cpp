/**
 * pinyin_matcher.cpp
 * 拼音匹配模块实现文件。
 * 负责基于拼音键在单字库/词库缓存中执行完整匹配与前缀匹配。
 */
#include "pinyin_matcher.h"
#include "pinyin_data.h"
#include "util.h"
#include <algorithm>

/**
 * 拼音匹配模块实现
 */



/**
 * 最小化智能匹配：
 * - exact_flags[i] == 1: 原拼音必须与目标拼音完全相等；
 * - exact_flags[i] == 0: 原拼音可与目标拼音相等，或为目标拼音前缀。
 *
 * @param source_parts 原拼音分段数组。
 * @param exact_flags 逐位精确匹配标记（1/0）。
 * @param target_parts 目标词条拼音分段数组。
 * @return 匹配是否通过。
 */
static bool minimal_smart_match(const std::vector<std::string>& source_parts,
                                const std::vector<int>& exact_flags,
                                const std::vector<std::string>& target_parts) {
    if (source_parts.size() != exact_flags.size() || source_parts.size() != target_parts.size()) {
        write_log("minimal_smart_match parameter length mismatch", LOG_ERROR);
        return false;
    }

    for (size_t i = 0; i < source_parts.size(); ++i) {
        const std::string& source = source_parts[i];
        const std::string& target = target_parts[i];
        if (exact_flags[i] == 1) {
            if (source != target) return false;
        } else {
            bool equal = (source == target);
            bool is_prefix = target.compare(0, source.size(), source) == 0;
            if (!equal && !is_prefix) return false;
        }
    }
    return true;
}

int find_exact_match_char(const std::string& pinyin, bool reject_iuv_as_initial) {
    /**
     * 在单字库中进行“完整匹配 + 前缀判定”：
     * - 返回 >0：完整命中行号（1-based）
     * - 返回 -1：无完整命中，但存在前缀
     * - 返回 -2：连前缀都不存在
     */
    if (pinyin.empty()) return -2;

    // 1. 查找索引，定位起始字符
    char first_char = pinyin[0];
    if (reject_iuv_as_initial && (first_char == 'i' || first_char == 'u' || first_char == 'v')) {
        return -2;
    }
    int start_line = -1, end_line = -1;

    for (const auto& item : g_pinyin_map_index) {
        if (item.start_char == (int)first_char) {
            start_line = item.start_line;
            end_line = item.end_line;
            break;
        }
    }

    if (start_line == -1) return -2; // 连首字母索引都找不到，肯定返回 -2

    bool has_prefix = false;
    // 2. 在指定行范围内进行搜索
    for (int i = start_line - 1; i < end_line && i < (int)g_pinyin_map_lines.size(); ++i) {
        std::string current_pinyin = get_pinyin_from_line(g_pinyin_map_lines[i]);
        
        // 检查是否完全匹配
        if (current_pinyin == pinyin) {
            return i + 1; // 找到完整匹配
        }
        
        // 如果还没找到完整匹配，检查是否是前缀
        if (!has_prefix && current_pinyin.compare(0, pinyin.length(), pinyin) == 0) {
            has_prefix = true;
        }
    }

    return has_prefix ? -1 : -2;
}

std::vector<int> find_prefix_match_char(const std::string& pinyin) {
    /**
     * 在单字库中执行前缀搜索，返回所有命中的真实行号（1-based）。
     */
    std::vector<int> results;
    if (pinyin.empty()) return results;

    // 1. 查找索引，定位起始字符
    char first_char = pinyin[0];
    int start_line = -1, end_line = -1;

    for (const auto& item : g_pinyin_map_index) {
        if (item.start_char == (int)first_char) {
            start_line = item.start_line;
            end_line = item.end_line;
            break;
        }
    }

    if (start_line == -1) return results;

    // 2. 在指定行范围内进行前缀搜索
    for (int i = start_line - 1; i < end_line && i < (int)g_pinyin_map_lines.size(); ++i) {
        std::string current_pinyin = get_pinyin_from_line(g_pinyin_map_lines[i]);
        // 检查 current_pinyin 是否以输入的 pinyin 开头
        if (current_pinyin.compare(0, pinyin.length(), pinyin) == 0) {
            results.push_back(i + 1); // 返回 1-based 真实行号
        }
    }

    return results;
}

std::vector<int> match_segmented_word_pinyin(const std::vector<std::string>& pinyin_parts) {
    /**
     * 智能词语拼音匹配函数。
     * 接收拼音分段，返回符合该分段词语的所在行数。完整的，不做舍弃的
     */
    if (pinyin_parts.empty() || pinyin_parts[0].empty()) return {};

    std::vector<int> exact_match_flags;
    exact_match_flags.reserve(pinyin_parts.size());

    for (const auto& part : pinyin_parts) {
        int match_result = find_exact_match_char(part, false);
        exact_match_flags.push_back(match_result > 0 ? 1 : 0);
    }

    // 基于词库首字母索引，先缩小到同首字母的行区间
    char first_char = pinyin_parts[0][0];
    int start_line = -1;
    int end_line = -1;
    for (const auto& item : g_user_dict_index) {
        if (item.start_char == (int)first_char) {
            start_line = item.start_line;
            end_line = item.end_line;
            break;
        }
    }

    if (start_line == -1) return {};

    std::vector<int> matched_lines;
    for (int i = start_line - 1; i <= end_line && i < (int)g_user_dict_lines.size(); ++i) {
        const std::string& line = g_user_dict_lines[i];
        std::string target_pinyin_csv = get_pinyin_from_line(line);
        std::vector<std::string> target_parts = split_csv(target_pinyin_csv);

        // 第一次筛选：分段长度不一致直接跳过
        if (target_parts.size() != pinyin_parts.size()) continue;

        // 第二次筛选：最小化智能匹配
        if (!minimal_smart_match(pinyin_parts, exact_match_flags, target_parts)) continue;

        // 保留真实行号（1-based）
        matched_lines.push_back(i + 1);
    }

    return matched_lines;
}
