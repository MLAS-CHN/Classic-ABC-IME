#ifndef PINYIN_FILE_IO_H
#define PINYIN_FILE_IO_H

#include <string>
#include <vector>
#include "pinyin_data.h"

class CandidateItem;

bool set_pinyin_data_dir(const std::string& dir);
std::string get_pinyin_data_dir();
std::string get_pinyin_map_file_path();
std::string get_user_dict_file_path();
std::string get_char_freq_file_path();

void init_pinyin_data();

void load_file_and_build_index(const std::string& file_path,
                               std::vector<std::string>& lines,
                               std::vector<PinyinIndexItem>& index);

void write_and_update_index(const std::string& file_path,
                            const std::string& content,
                            std::vector<std::string>& lines,
                            std::vector<PinyinIndexItem>& index);

void write_and_update_index(const std::string& file_path,
                            const CandidateItem& candidate,
                            std::vector<std::string>& lines,
                            std::vector<PinyinIndexItem>& index);

bool delete_user_dict_line(int line_number);

enum class DictTargetFile {
    UserDict,
    CharFreq
};

void update_timestamp_by_line(DictTargetFile target_file, int line_number);

#endif // PINYIN_FILE_IO_H
