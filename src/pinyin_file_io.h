/**
 * pinyin_file_io.h
 * 拼音数据文件读写接口声明。
 * 提供初始化加载、通用读取建索引、写入并刷新索引的能力。
 */
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

/**
 * @brief 初始化并加载所有数据和索引
 */
void init_pinyin_data();

/**
 * @brief 从文件读取数据并更新缓存和索引
 * @param file_path 文件路径
 * @param lines 存储文件内容的 vector
 * @param index 存储索引的 vector
 */
void load_file_and_build_index(const std::string& file_path, 
                               std::vector<std::string>& lines, 
                               std::vector<PinyinIndexItem>& index);

/**
 * @brief 向文件写入一行数据并触发索引更新
 * @param file_path 文件路径
 * @param content 要写入的内容
 * @param lines 对应的缓存 vector
 * @param index 对应的索引 vector
 */
void write_and_update_index(const std::string& file_path, 
                            const std::string& content,
                            std::vector<std::string>& lines, 
                            std::vector<PinyinIndexItem>& index);

void write_and_update_index(const std::string& file_path,
                            const CandidateItem& candidate,
                            std::vector<std::string>& lines,
                            std::vector<PinyinIndexItem>& index);

bool delete_user_dict_line(int line_number);

enum class WeightTargetFile {
    UserDict,
    CharFreq
};

/**
 * @brief 增加指定行的权重（使用次数）。
 * 目标文件可选：
 * - data/user_dict.txt
 * - data/char_freq.txt
 *
 * 对应行格式支持：
 * - pinyin_csv text
 * - pinyin_csv text weight
 * - pinyin text weight
 *
 * 若当前行无 weight，则追加 " 1"；
 * 若已有 weight，则将其加 1 并写回。
 *
 * @param target_file 目标文件类型。
 * @param line_number 词库真实行号（1-based）。
 */
void increment_weight_by_line(WeightTargetFile target_file, int line_number);

int get_weight_by_line(WeightTargetFile target_file, int line_number);

#endif // PINYIN_FILE_IO_H
