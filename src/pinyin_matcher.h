/**
 * pinyin_matcher.h
 * 拼音匹配模块接口声明。
 * 对外提供单字/词组的完整匹配与前缀匹配函数。
 */
#ifndef PINYIN_MATCHER_H
#define PINYIN_MATCHER_H

#include <string>
#include <vector>

/**
 * 拼音匹配模块
 * 负责在字库和词库中搜索拼音匹配项。
 */

// --- 单字匹配 (Character Matching) ---

/**
 * 匹配全拼音字（一字不差）
 * 返回值：
 *   > 0 : 字库中的真实行号（1-based）
 *   -1  : 未找到完整匹配，但存在以此为前缀的拼音
 *   -2  : 连前缀都匹配不到
 */
int find_exact_match_char(const std::string& pinyin, bool reject_iuv_as_initial = false);

/**
 * 匹配前缀拼音字
 */
std::vector<int> find_prefix_match_char(const std::string& pinyin);


// --- 词语匹配 (Word Matching) ---

/**
 * 智能词语拼音匹配。
 * 接收一套拼音分段，返回符合该分段的词语所在行号数组。
 */
std::vector<int> match_segmented_word_pinyin(const std::vector<std::string>& pinyin_parts);

#endif // PINYIN_MATCHER_H
