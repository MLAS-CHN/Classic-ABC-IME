#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

// 配置模块：读写 DLL 同目录的 settings.ini。
//   candidate_page_size=10   # 候选词每页数量 (5~10)
//   log_enabled=0            # 是否生成日志文件
//   log_level=1              # 日志等级：0=INFO 1=DEBUG
// 缺省值：10 / 不生成日志 / INFO。文件不存在时使用缺省值，保存时才创建。

void load_settings(const std::string& dir);  // dir = DLL 目录
bool save_settings();

int  get_page_size();
void set_page_size(int size);                // 钳制到 5~10，仅改内存（点保存才写盘）

bool is_log_enabled();
void set_log_enabled(bool enabled);          // 立即生效：同步日志开关 + 写盘

int  get_log_level();                        // 0=INFO 1=DEBUG
void set_log_level(int level);               // 立即生效：同步日志级别 + 写盘

#endif // SETTINGS_H
