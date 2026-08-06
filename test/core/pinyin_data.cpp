#include "pinyin_data.h"

std::vector<PinyinIndexItem> g_pinyin_map_index;
std::vector<PinyinIndexItem> g_user_dict_index;
std::vector<PinyinIndexItem> g_char_freq_index;

std::vector<std::string> g_pinyin_map_lines;
std::vector<std::string> g_user_dict_lines;
std::vector<std::string> g_char_freq_lines;

std::unordered_map<std::string, CharFreqLookupEntry> g_char_freq_lookup;
