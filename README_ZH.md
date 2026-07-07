经典ABC（ClassicABC）— Windows 拼音输入法

> English Version (README.md)

经典ABC 是一款 Windows 拼音输入法，同时支持现代 TSF（Text Services Framework）和传统 IMM32 双后端。提供完整的拼音输入体验，包括候选窗、中文标点符号、可拖拽设置栏和 9-patch 皮肤系统。

本项目由 AI（Opencode）辅助编程完成。

---

功能特性

- 双后端：TSF COM 文本服务 + 传统 IMM32 IME
- 拼音输入：完整音节切分、字频权重排序、用户词库自动学习
- 中文标点：中文模式下自动转换为全角标点符号
- 候选导航：底部翻页按钮（首页/末页/上页/下页）+ 页码显示
- 设置栏：可拖拽状态栏，显示模式/锁定/标点状态
- 9-patch 皮肤：所有窗口使用可拉伸 PNG 皮肤
- x64 + Win32：同时提供 64 位和 32 位版本

## 截图

![](img/screenshot.png)

---

 编译

环境要求

- Visual Studio 2022 (MSVC v143)
- C++17 支持
- MSBuild 在 PATH 中或安装于 D:\VisualStudio

编译命令

.\build.bat

同时编译 x64 和 Win32 的 Release 版本。输出：

| 架构 | TSF DLL | IMM32 IME |
|---|---|---|
| x64 | output\abcimex64.dll | output\abcimex64.ime |
| Win32 | output\abcime.dll | output\abcime.ime |

---

项目结构
```
├── build.bat                  # 一键编译脚本 (x64 + Win32)
├── abcime.sln                 # VS2022 解决方案 (TSF + IME)
├── abcime.props               # 共享 MSBuild 属性
├── data/                      # 词典数据文件
│   ├── pinyin_map.txt         # 拼音→汉字映射 (203 行)
│   ├── char_freq.txt          # 单字频率表 (1188 行)
│   ├── user_dict.txt          # 用户词库 (4080 行)
│   └── emoji.txt              # Emoji 拼音映射 (750 行)
├── res/                       # PNG/ICO 皮肤图标资源
├── output/                    # 编译输出 + 安装脚本
└── src/
    ├── candidate_item.cpp/.h   # 候选词数据结构
    ├── pinyin_composition.cpp  # 候选词聚合与显示文本构建
    ├── pinyin_data.cpp/.h      # 词典全局缓存
    ├── pinyin_file_io.cpp/.h   # 词典文件读写
    ├── pinyin_matcher.cpp/.h   # 精确/前缀/分段匹配
    ├── pinyin_split.cpp/.h     # 拼音音节切分
    ├── pinyin_virtual_cursor   # 虚拟光标操作
    ├── word_matcher.cpp/.h     # 词组候选收集
    ├── util.cpp/.h             # 日志、UTF-8 宽度计算
    └── win/                    # Windows 适配层
        ├── proto_core.cpp/.h   # ClassicABC 协调器 (门面)
        ├── proto_engine.cpp/.h # 引擎状态机与按键处理
        ├── proto_ui.cpp/.h     # GDI+ UI (候选框 + 设置栏)
        ├── TSF/                # TSF COM 前端
        └── IME/                # IMM32 前端
```
---

架构
```
应用程序 (记事本/控制台/等)
    │
    ├── TSF COM  ───→ abcime.dll
    └── IMM32    ───→ abcime.ime
            │
            ▼
    ┌───────────────┐
    │  ClassicABC    │  ← 统一门面
    ├───────┬───────┤
    ▼       ▼       ▼
  UI    Engine  Keys
  GDI+  状态机  按键分发
        │
        ▼
  ┌──────────────┐
  │ 拼音引擎库    │
  │ 音节切分     │
  │ 字词匹配     │
  │ 候选聚合     │
  │ 词典文件IO   │
  └──────────────┘
```
---

使用

快捷键

| 按键 | 功能 |
|---|---|
| Shift (单击) | 切换中/英文模式 |
| CapsLock | 强制英文模式 |
| 1-9, 0 | 选择候选词 (1-9, 0=第10个) |
| 空格 | 选择第一个候选词 |
| - / = | 上一页 / 下一页 |
| Delete | 切换删除模式 |
| ← / → | 移动拼音缓冲区光标 |
| Backspace | 删除拼音字符 |
| Esc | 清空缓冲 |
| Enter | 直接提交拼音原文 |

中文标点转换

中文模式下（无输入缓冲和候选词），标点键自动转换为全角中文符号：

| 无 Shift | 有 Shift |
|---|---|
|  `  → · | ~ → ～ |
| , → ， | < → 《 |
| . → 。 | > → 》 |
| \ → 、 | `| → ｜` |
| ; → ； | : → ： |
| ' → '' (配对) | " → "" (配对) |
| [ → 【 | { → ｛ |
| ] → 】 | } → ｝ |
| - → － | _ → —— (双破折号) |
| = → ＝ | + → ＋ |
| / → ／ | ? → ？ |

小键盘 *、/、+、- 始终穿透，不做转换。

设置栏

| 按钮 | 图标 | 功能 |
|---|---|---|
| ① | ABC_ICON.png | 锁定/解锁（锁定=游戏模式，强制英文） |
| ② | capital.png/english.png/pinyin.png | 中/英/大写切换 |
| ③ | half.png | — |
| ④ | sign.png/sign_en.png | — |
| ⑤ | keyboard.png | — |

---

运行时配置

| 文件/标记 | 说明 |
|---|---|
| data/ | 放在 DLL 同级目录或 %ProgramData%\ClassicABC\data\ |
| res/ | 放在 DLL 同级目录或 %ProgramData%\ClassicABC\res\ |
| proto_debug_enable.flag | 零字节标记文件，存在时启用调试日志 |
| abcime.log | 日志文件（默认 LOG_INFO 级别） |

---

安装

cd output
install.bat     # 需管理员权限

安装步骤：
1. 复制 data/ 和 res/ 到 %ProgramData%\ClassicABC\
2. regsvr32 注册 TSF DLL（x64 + Win32）
3. 复制 .ime 到 System32/SysWOW64
4. 注册 IMM32 键盘布局 E05E0804
5. 添加到用户语言列表，重启 ctfmon

卸载：

cd output
uninstall.bat

---

技术细节

- 线程安全：TSF/IMM32 为 STA 单线程模型，引擎状态无需加锁
- 字典缓存：init_pinyin_data() 预解析全部词典到内存，g_char_freq_lookup 实现 O(1) 字频查找
- 原子写入：persist_lines_to_file 使用临时文件 + std::filesystem::rename 避免崩溃时词库损坏
- UTF-8：源码 /utf-8 编译，内部 UTF-8 ↔ UTF-16 转换

---

许可证

本项目按原样提供，不做任何保证。

---

致谢

- 基于 Weasel/Rime TSF 样例项目结构
- 本项目由 AI（Opencode）辅助编程完成
</parameter>
<parameter=filePath>
E:\Program\C++\pinyin\README_ZH.md
