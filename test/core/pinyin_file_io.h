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

// Async variant: loads files synchronously (fast) but builds the in-memory
// caches on a background thread. While the caches are not ready,
// is_dict_cache_ready() returns false and matching returns empty results.
void init_pinyin_data_async();
bool is_dict_cache_ready();
void wait_dict_cache_ready();

// Optional callback invoked (on the background thread) once the async cache
// build finishes. The adapter uses it to notify the UI thread to rebuild the
// candidate list so words appear without user interaction.
using DictReadyCallback = void (*)();
void set_dict_ready_callback(DictReadyCallback cb);

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

// Deferred-write support: mark a dict dirty (no disk I/O), then flush all
// dirty dicts at a safe point (deactivate / idle). Avoids per-candidate
// full-file rewrites on large dictionaries.
void mark_dict_dirty(DictTargetFile target_file);
void flush_dirty_dicts();

#endif // PINYIN_FILE_IO_H
