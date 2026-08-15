#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

// 配置模块：读写 DLL 同目录的 settings.ini。
//   candidate_page_size=10   # 候选词每页数量 (5~10)
//   log_enabled=0            # 是否生成日志文件
//   log_level=1              # 日志等级：0=INFO 1=DEBUG
//   candidate_font=Segoe UI  # 候选词主字体
//   candidate_font_size=12   # 主字体字号 (px)
//   fallback_font=Microsoft YaHei  # 候选词备选字体（主字体缺字时）
//   fallback_font_size=12    # 备选字体字号 (px)
// 缺省值：10 / 不生成日志 / INFO / Segoe UI 12px / YaHei 12px。
// 文件不存在时使用缺省值，保存时才创建。

void load_settings(const std::string& dir);  // dir = DLL 目录
bool save_settings();

int  get_page_size();
void set_page_size(int size);                // 钳制到 5~10，仅改内存（点保存才写盘）

bool is_log_enabled();
void set_log_enabled(bool enabled);          // 立即生效：同步日志开关 + 写盘

int  get_log_level();                        // 0=INFO 1=DEBUG
void set_log_level(int level);               // 立即生效：同步日志级别 + 写盘

std::string get_candidate_font();
void set_candidate_font(const std::string& name);  // 立即生效 + 写盘

int  get_candidate_font_size();
void set_candidate_font_size(int px);              // 立即生效 + 写盘

std::string get_fallback_font();
void set_fallback_font(const std::string& name);   // 立即生效 + 写盘

int  get_fallback_font_size();
void set_fallback_font_size(int px);               // 立即生效 + 写盘

#endif // SETTINGS_H
