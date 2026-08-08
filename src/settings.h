#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

// 配置模块：读写 DLL 同目录的 settings.ini。
//   candidate_page_size=10   # 候选词每页数量 (5~10)
//   log_enabled=0            # 是否生成日志文件
// 缺省值：10 / 不生成日志。文件不存在时使用缺省值，保存时才创建。

void load_settings(const std::string& dir);  // dir = DLL 目录
bool save_settings();

int  get_page_size();
void set_page_size(int size);                // 钳制到 5~10，仅改内存（点保存才写盘）

bool is_log_enabled();
void set_log_enabled(bool enabled);          // 立即生效：同步日志开关 + 写盘

#endif // SETTINGS_H
