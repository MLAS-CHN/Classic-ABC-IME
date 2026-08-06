#ifndef PINYIN_DATA_H
#define PINYIN_DATA_H

#include <vector>
#include <string>
#include <unordered_map>

struct PinyinIndexItem {
    int start_char;
    int start_line;
    int end_line;
};

extern std::vector<PinyinIndexItem> g_pinyin_map_index;
extern std::vector<PinyinIndexItem> g_user_dict_index;
extern std::vector<PinyinIndexItem> g_char_freq_index;

extern std::vector<std::string> g_pinyin_map_lines;
extern std::vector<std::string> g_user_dict_lines;
extern std::vector<std::string> g_char_freq_lines;

struct CharFreqLookupEntry {
    int line_number;
    long long timestamp;
    int count;
};
extern std::unordered_map<std::string, CharFreqLookupEntry> g_char_freq_lookup;

#endif // PINYIN_DATA_H
