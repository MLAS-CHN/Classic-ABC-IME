/**
 * pinyin_data.h
 * 拼音数据全局结构与变量声明。
 * 包含索引结构定义，以及字库/词库缓存与索引的 extern 声明。
 */
#ifndef PINYIN_DATA_H
#define PINYIN_DATA_H

#include <vector>
#include <string>
#include <unordered_map>

// 索引项结构：[起始字符(转为int), 起始行, 结束行]
struct PinyinIndexItem {
    int start_char; // 比如 'a'
    int start_line;
    int end_line;
};

// 全局索引，对应 pinyin_map.txt
extern std::vector<PinyinIndexItem> g_pinyin_map_index;

// 全局索引，对应 user_dict.txt
extern std::vector<PinyinIndexItem> g_user_dict_index;

// 全局索引，对应 char_freq.txt
extern std::vector<PinyinIndexItem> g_char_freq_index;

// 缓存文件内容，避免频繁 IO
extern std::vector<std::string> g_pinyin_map_lines;
extern std::vector<std::string> g_user_dict_lines;
extern std::vector<std::string> g_char_freq_lines;

// 预解析缓存：user_dict 每行的拼音分段（避免每键重复 split_csv）
extern std::vector<std::vector<std::string>> g_user_dict_parts;
// user_dict 按分段数量分组索引（分段数 -> 行号列表，1-based）
extern std::unordered_map<size_t, std::vector<int>> g_user_dict_segcount_map;
// user_dict 查找表："pinyin_csv text" -> 行号（1-based）
extern std::unordered_map<std::string, int> g_user_dict_lookup;

// char_freq 查找表："pinyin text" -> {行号(1-based), 权重}
struct CharFreqLookupEntry {
    int line_number;
    int weight;
};
extern std::unordered_map<std::string, CharFreqLookupEntry> g_char_freq_lookup;

#endif // PINYIN_DATA_H
