# 智能ABC拼音输入法 — 项目概览

> **版本**: 0.17.4  
> **语言**: C++17 (MSVC v143 / VS2022)  
> **平台**: Windows x64 + Win32  
> **编码**: 源文件 UTF-8 (`/utf-8` 编译选项)  

## 1. 项目简介

基于 Weasel/Rime TSF 样例结构重构的 Windows 拼音输入法。项目从早期的 TTY 终端输入法移植而来，现已完全转为 Windows 图形界面，终端代码已删除。

项目编译为**两个并行后端 DLL**，共享同一套拼音引擎和 UI 层：

| 后端 | 产物 | 用途 |
|------|------|------|
| **TSF** | `abcime.dll` / `abcimex64.dll` | 现代 COM 文本服务框架 |
| **IMM32** | `abcime.ime` / `abcimex64.ime` | 传统输入法管理器 |

## 2. 目录结构

```
pinyin/
├── build.bat                 # 一键编译脚本 (调用 MSBuild)
├── abcime.sln                # VS2022 解决方案 (TSF + IME 两个项目)
├── abcime.props              # 共享 MSBuild 属性表
├── env.bat                   # 设置 ABCIME_ROOT + PLATFORM_TOOLSET=v143
├── detail.md                 # 本文件
│
├── data/                     # 词典数据文件
│   ├── pinyin_map.txt        # 拼音→汉字映射 (203 行)
│   ├── char_freq.txt         # 单字频率表 (1188 行)
│   ├── user_dict.txt         # 用户词库 (4080 行)
│   └── emoji.txt             # Emoji 拼音映射 (750 行)
│
├── res/                      # 皮肤/图标资源 (PNG + ICO)
│   ├── shadow.png            # 候选框 9-patch 皮肤 (4px 边距)
│   ├── common.png            # 设置栏 9-patch 皮肤 (2px 边距)
│   ├── button.png            # 设置栏按钮 9-patch 皮肤 (2px 边距)
│   ├── ABC_ICON.png          # 按钮0: 中文模式图标
│   ├── ABC_ICON_GRAY.png     # 按钮0: 锁定状态图标
│   ├── capital.png           # 按钮1: 大写模式图标
│   ├── english.png           # 按钮1: 英文模式图标
│   ├── pinyin.png            # 按钮1: 拼音模式图标
│   ├── half.png              # 按钮2: 半角图标
│   ├── sign.png              # 按钮3: 中文标点图标
│   ├── sign_en.png           # 按钮3: 英文标点图标
│   ├── keyboard.png          # 按钮4: 软键盘图标
│   ├── first_page.png        # 候选框导航: 首页
│   ├── last_page.png         # 候选框导航: 末页
│   ├── next_page.png         # 候选框导航: 下页
│   ├── prev_page.png         # 候选框导航: 上页
│   ├── full.png, lock.png    # 未使用的资源
│   └── ime.ico               # 主图标
│
├── output/                   # 编译输出 (gitignored)
│   ├── abcime.dll/ime        # Win32 TSF/IMM32
│   ├── abcimex64.dll/ime     # x64 TSF/IMM32
│   ├── install.bat           # 安装脚本
│   ├── uninstall.bat         # 卸载脚本
│   ├── data/                 # data/ 的副本
│   └── res/                  # res/ 的副本
│
├── example/                  # Weasel 样例参考 (独立, 不参与编译)
│
└── src/                      # 源代码根目录
    ├── candidate_item.cpp/.h
    ├── pinyin_composition.cpp/.h
    ├── pinyin_data.cpp/.h
    ├── pinyin_file_io.cpp/.h
    ├── pinyin_matcher.cpp/.h
    ├── pinyin_split.cpp/.h
    ├── pinyin_virtual_cursor.cpp/.h
    ├── word_matcher.cpp/.h
    ├── util.cpp/.h
    │
    └── win/                  # Windows 适配层
        ├── proto_core.cpp/.h     # ProtoIME 协调器 (Engine + UI 门面)
        ├── proto_engine.cpp/.h   # ProtoIME::Engine 引擎状态机
        ├── proto_ui.cpp/.h       # ProtoIME::UI GDI+ 窗口绘制
        ├── include/
        │   ├── KeyEvent.h        # KeyInfo 位域结构
        │   ├── ImeUtility.h      # IME 名称本地化
        │   └── resource.h        # 图标 ID 定义
        ├── resource/weasel.ico
        ├── TSF/                  # TSF 前端项目
        │   ├── TSF.cpp/.h        # TSF 文本服务主类
        │   ├── KeyEventSink.cpp  # 按键事件分发
        │   ├── Server.cpp        # COM 类工厂 + DLL 导出
        │   ├── Register.cpp/.h   # COM 注册
        │   ├── Globals.cpp/.h    # CLSID/Profile GUID
        │   ├── Compartment.cpp/.h
        │   ├── TextEditSink.cpp
        │   ├── ThreadMgrEventSink.cpp
        │   ├── dllmain.cpp
        │   ├── stdafx.h/.cpp
        │   ├── TSF.def           # DLL 导出定义
        │   └── TSF.rc
        └── IME/                  # IMM32 前端项目
            ├── ime.cpp           # IMM32 导出函数
            ├── IMEimpl.cpp       # IME 类实现
            ├── IME.h
            ├── dllmain.cpp
            ├── stdafx.h/.cpp
            ├── immdev.h          # IMM32 设备头文件
            ├── IME.def           # DLL 导出定义
            ├── IME.rc
            └── resource.h
```

## 3. 架构分层

```
应用程序 (记事本/控制台/等)
    │
    ├──────────────────┬──────────────────────┐
    │ TSF COM 接口     │ IMM32 接口            │
    ▼                  ▼                      │
┌────────────┐   ┌────────────┐              │
│ TSF 前端    │   │ IME 前端    │              │
│ abcime.dll │   │ abcime.ime │              │
└─────┬──────┘   └─────┬──────┘              │
      │                │                      │
      └────────┬───────┘                      │
               ▼                               │
    ┌─────────────────────┐                   │
    │ ProtoIME (core)     │ ← 统一门面         │
    │ proto_core.cpp/h    │                   │
    ├─────────┬───────────┤                   │
    ▼         ▼           ▼                   │
┌────────┐ ┌──────────────┐                   │
│ UI     │ │ Engine       │                   │
│ proto_ │ │ proto_       │                   │
│ ui.cpp │ │ engine.cpp   │                   │
│ GDI+   │ │ 状态机/按键   │                   │
│ 窗口   │ │ SendInput    │                   │
└────────┘ └──────┬───────┘                   │
                    ▼                           │
          ┌─────────────────┐                  │
          │ 拼音引擎库 (src/)│                  │
          │  composition    │                  │
          │   ├ split       │                  │
          │   ├ matcher     │                  │
          │   ├ word_matcher│                  │
          │   └ virtual_cur │                  │
          │  file_io + data │                  │
          │  data/*.txt 词典│                  │
          └─────────────────┘                  │
```

## 4. 源文件详解

### 4.1 拼音引擎库 (`src/`)

| 文件 | 职责 | 关键函数 |
|------|------|----------|
| `util.cpp/h` | 日志、UTF-8 宽度计算、CSV 工具 | `write_log`, `init_logger`, `get_display_width`, `split_csv`, `join_csv` |
| `candidate_item.cpp/h` | 候选词数据结构 (拼音分段 + 文本 + 权重) | `CandidateItem`, `quickSortByWeightDesc`, `mergeCandidateItems` |
| `pinyin_data.cpp/h` | 全局词典缓存和索引声明/定义 | `g_pinyin_map_lines`, `g_user_dict_lines`, `g_char_freq_lookup`, `g_user_dict_parts` |
| `pinyin_file_io.cpp/h` | 词典文件加载/写入/权重更新 | `init_pinyin_data`, `load_file_and_build_index`, `write_and_update_index`, `increment_weight_by_line` |
| `pinyin_split.cpp/h` | 拼音音节切分 (激进+保守策略) | `aggressivePinyinSplitMain`, `conservativePinyinSplitMain`, `splitConservativePinyin` |
| `pinyin_matcher.cpp/h` | 精确/前缀匹配查字 | `find_exact_match_char`, `find_prefix_match_char`, `match_segmented_word_pinyin` |
| `word_matcher.cpp/h` | 词组候选收集/去重/排序 | `getAllSuitableWords` |
| `pinyin_composition.cpp/h` | 候选聚合 + 拼音栏显示文本构建 | `getAllCandidateElements`, `buildComposingDisplayText` |
| `pinyin_virtual_cursor.cpp/h` | 拼音缓冲区虚拟光标操作 | `insert_at_virtual_cursor`, `backspace_at_virtual_cursor`, `move_virtual_cursor_left/right` |

### 4.2 Windows 适配层 (`src/win/`)

| 文件 | 职责 | 关键函数/类 |
|------|------|-------------|
| `proto_core.cpp/h` | `ProtoIME` 命名空间门面, 连接 Engine + UI | `Initialize`, `OnKeyDown`, `SetSkin`, `ToggleMode`, `ToggleLock` |
| `proto_engine.cpp/h` | `ProtoIME::Engine` 引擎状态机和按键处理 | `TestKey`, `ProcessKey`, `handlePunct`, `handleLetter`, `rebuild`, `send_u8` |
| `proto_ui.cpp/h` | `ProtoIME::UI` GDI+ 候选框+设置栏窗口 | `Update`, `UpdateCand`, `ShowSettings`, `SetBtnIcon`, `Shutdown` |

### 4.3 TSF 前端 (`src/win/TSF/`)

| 文件 | 职责 |
|------|------|
| `TSF.cpp/h` | TSF 文本服务主类, 实现 `ITfTextInputProcessorEx` 等接口, 加载皮肤/图标 |
| `KeyEventSink.cpp` | 按键事件分发: `OnTestKeyDown`/`OnKeyDown`/`OnKeyUp`, 桥接 `ProtoIME::TestKeyDown/OnKeyDown` |
| `Server.cpp` | COM 类工厂 + DLL 导出 (`DllGetClassObject` 等) |
| `Register.cpp/h` | COM 注册: 注册 TSF Profile/Categories/CLSID |
| `Globals.cpp/h` | 全局 CLSID/Profile GUID, DLL 引用计数 |
| `Compartment.cpp/h` | 键盘开关 Compartment 事件监听 |
| `TextEditSink.cpp` | `ITfTextEditSink` (桩) |
| `ThreadMgrEventSink.cpp` | `ITfThreadMgrEventSink` (焦点变化时重建 EditSink) |

### 4.4 IMM32 前端 (`src/win/IME/`)

| 文件 | 职责 |
|------|------|
| `ime.cpp` | IMM32 导出函数: `ImeInquire`, `ImeProcessKey`, `ImeSelect`, `ImeToAsciiEx` 等 |
| `IMEimpl.cpp` | `IME` 类: UI 窗口注册, 按键处理, 引擎初始化/关闭 |
| `IME.h` | `IME` 类声明, `CompositionInfo` 结构, `HIMCMap` (HIMC→IME 实例映射) |
| `immdev.h` | 本地最小 IMM32 设备头文件 |

## 5. 构建系统

### 编译命令

```batch
.\build.bat
```

调用 MSBuild 对 `abcime.sln` 执行 `Rebuild`，先 x64 后 Win32，均用 `Release` 配置。`build.bat` 会优先使用 PATH 中的 `msbuild`，找不到时回退到 `D:\VisualStudio\...`。

### 产物

| 架构 | TSF DLL | IMM32 IME |
|------|---------|-----------|
| Win32 | `output\abcime.dll` | `output\abcime.ime` |
| x64 | `output\abcimex64.dll` | `output\abcimex64.ime` |

### 编译选项

- C++17 (`/std:c++17`)
- `/utf-8` 源文件编码
- `/MT` 静态链接 CRT
- PCH (`stdafx.h`)
- `_CRT_SECURE_NO_WARNINGS`
- PerMonitorHighDPIAware manifest

### 链接库

- TSF: `Usp10.lib`, `ole32`, `oleaut32`, `uuid`, `gdiplus`, `shlwapi`
- IME: `imm32.lib`, `gdiplus`, `shlwapi`

## 6. 按键处理流程

### TSF 路径

```
应用程序按键
    ↓
TSF::OnTestKeyDown → ProtoIME::TestKeyDown(vk) → *pfEaten
    ↓ (pfEaten=TRUE)
TSF::OnKeyDown → ProtoIME::OnKeyDown(vk)
    → Engine::ProcessKey(vk)
        → handlePunct        (中文符号转换)
        → handleCandidateSelect (数字键选词)
        → handlePage          (翻页 +/-/=)
        → handleDeleteMode    (Delete 切换删除模式)
        → handleCursorMove    (光标移动)
        → Backspace/Escape/Return
        → handleLetter        (拼音字母输入)
    → UI::Update() + UI::UpdateCand()
    → *pfEaten = 返回值
```

### IMM32 路径

```
ImeProcessKey → IME::ProcessKeyEvent
    → ProtoIME::TestKeyDown(vk) → TRUE?
        → ProtoIME::OnKeyDown(vk) (返回值被忽略, 始终返回 TRUE)
        → ImeToAsciiEx 返回 0 (不产生字符, 由 SendInput 提交)
```

### 关键: TSF vs IMM32 的差异

- **TSF**: `OnKeyDown` 的返回值用于设置 `*pfEaten`，未吃掉的键会穿透到应用
- **IMM32**: `ProcessKeyEvent` 始终返回 `TRUE`（只要 `TestKeyDown` 通过），`ImeToAsciiEx` 返回 0，字符由 `SendInput` 提交

### 中文符号转换 (`handlePunct`)

当输入法处于中文模式且无拼音缓冲/候选词时，标点键转换为全角中文符号：

| 按键 | 无 Shift | 有 Shift |
|------|----------|----------|
| `` ` `` | `·` | `～` |
| `1` | 数字 | `！` |
| `2` | 数字 | `＠` |
| `3` | 数字 | `＃` |
| `4` | 数字 | `￥` |
| `5` | 数字 | `％` |
| `6` | 数字 | `……` (双省略号) |
| `7` | 数字 | `＆` |
| `8` | 数字 | `＊` |
| `9` | 数字 | `（` |
| `0` | 数字 | `）` |
| `-` | `－` | `——` (双破折号) |
| `=` | `＝` | `＋` |
| `[` | `【` | `｛` |
| `]` | `】` | `｝` |
| `\` | `、` | `｜` |
| `;` | `；` | `：` |
| `'` | `‘’` (配对) | `“”` (配对) |
| `,` | `，` | `《` |
| `.` | `。` | `》` |
| `/` | `／` | `？` |

**小键盘** `VK_MULTIPLY/DIVIDE/ADD/SUBTRACT` 始终穿透，不做转换。

## 7. 候选词引擎流程

```
用户输入字母 → handleLetter → g.buf += 字母 → rebuild()
    ↓
rebuild() 调用:
    1. splitConservativePinyin(g.buf)  → 多种切分方案
    2. getAllCandidateElements(方案, 10) → 聚合候选词到分页
    3. buildComposingDisplayText() → 构建拼音栏显示文本
    ↓
用户按数字键 → handleCandidateSelect
    → pick(page, idx) 取候选词
    → persist() 更新权重/写入用户词库
    → consume_buf() 消耗已匹配的拼音前缀
    → send_u8(候选词) 通过 SendInput 提交
    → 如果 buf 还有剩余 → rebuild() (连续输入)
    → 如果 buf 空 → 清除候选/隐藏 UI
```

### 词典数据格式

| 文件 | 格式 | 示例 |
|------|------|------|
| `pinyin_map.txt` | `拼音<TAB>汉字...` | `a 阿啊腌嗄锕` |
| `char_freq.txt` | `拼音<TAB>字<TAB>权重` | `a 啊 121` |
| `user_dict.txt` | `拼音CSV<TAB>词<TAB>[权重]` | `ai,hao 爱好 1` |
| `emoji.txt` | `拼音<TAB>emoji` | `aiqingxin 💌` |

### 词典缓存

- `g_pinyin_map_lines` / `g_pinyin_map_index` — 按首字母索引
- `g_user_dict_lines` / `g_user_dict_parts` / `g_user_dict_segcount_map` / `g_user_dict_lookup` — 预解析缓存
- `g_char_freq_lines` / `g_char_freq_lookup` — O(1) 字频查表
- 文件变化时 `ReloadDict()` 重建全部缓存
- `init_pinyin_data()` 及所有写操作后自动重建缓存
- `persist_lines_to_file` 使用临时文件 + `std::filesystem::rename` 原子写入

## 8. UI 窗口

### 拼音栏 (`g_wnd`)

- 固定尺寸 `163×26` 像素
- 9-patch 皮肤: `shadow.png` (4px 边距)
- 位置: 光标下方 `cp.y + g_fh + 4`，空间不足时上方
- 显示拼音输入缓冲 + 虚拟光标 `|`

### 候选框 (`g_candWnd`)

- 宽度 `120px`，高度动态: `6 + count * g_fh + 13(导航栏) + 6`
- 默认在拼音栏右侧，空间不足时左侧
- 下方放不下时上方展开（同步移动拼音栏）
- 底部导航栏: 4 个按钮 (13×13px) + 居中页码 `cur/total`
  - 页码蓝色 `RGB(0,0,255)`
  - 删除模式候选词红色 `RGB(255,0,0)`
  - 候选词格式: `1:字` (x=4, 冒号英文无空格)

### 设置栏 (`g_settingsWnd`)

- 尺寸 `127×26` 像素
- 9-patch 皮肤: `common.png` (背景) + `button.png` (按钮)
- 5 个按钮 (从左到右):

| # | 宽度 | 图标 | 功能 | 光标 |
|---|------|------|------|------|
| 0 | 20 | `ABC_ICON.png` / `ABC_ICON_GRAY.png`(锁定) | 锁定/解锁 | 手型 |
| 1 | 40 | `capital.png`/`english.png`/`pinyin.png` | 中/英/大写切换 | 手型 |
| 2 | 20 | `half.png` | (未实现) | 手型 |
| 3 | 20 | `sign.png`/`sign_en.png` | (不可点击) | 箭头 |
| 4 | 20 | `keyboard.png` | (未实现) | 手型 |

- 按钮外区域可拖拽 (光标 `IDC_SIZEALL`)
- 模式判断优先级: CapsLock 开 → 大写; `!IsChineseMode()` → 英文; 否则 → 拼音
- 可拖拽, 位置保存在 `g_settingsX/g_settingsY`

## 9. 模式切换

| 操作 | 效果 |
|------|------|
| Shift 单击 (tap) | 切换中/英文模式 |
| CapsLock 按下 | 强制英文模式, 刷新缓冲 |
| 按钮 0 点击 | 锁定/解锁 (锁定=游戏模式, 强制英文且 Shift 不切换) |
| 按钮 1 点击 | 切换中/英文模式 |

## 10. CLSID 和注册

| 标识 | 值 |
|------|----|
| Text Service CLSID | `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` |
| Profile GUID | `{3D02CAB6-2B8E-4781-BA20-1C9267529467}` |
| IMM32 键盘布局 | `E05E0804` (中文 PRC `0804` + IME 前缀 `E0`) |
| 线程模型 | `Apartment` (STA) |
| 注册的 LANGID | `0x0804`(Hans), `0x0404`(Hant), `0x0c04`(HK), `0x1404`(Macau), `0x1004`(SG) |

## 11. 运行时配置

| 配置 | 说明 |
|------|------|
| `proto_debug_enable.flag` | 零字节标记文件, 存在时启用 `LOG_DEBUG` 级别日志 |
| `TEXTSERVICE_PROFILE` | 环境变量, 控制 `hans`/`hant` 注册 |
| `%ProgramData%\ProtoIME` | 数据/资源回退目录 (DLL 目录找不到 `data\` 时使用) |
| `abcime.log` | 日志文件, 默认 `LOG_INFO` 级别 |

## 12. 安装/卸载

```batch
# 安装 (需管理员权限)
cd output && install.bat

# 卸载
cd output && uninstall.bat
```

安装步骤:
1. 复制 `data\` 和 `res\` 到 `%ProgramData%\ProtoIME\`
2. `regsvr32` 注册 TSF DLL (x64 原地, Win32 用 `%SYSWOW64%\regsvr32.exe`)
3. 复制 `.ime` 到 `System32`/`SysWOW64` 命名为 `abcime_test.ime`
4. 注册 IMM32 键盘布局 `E05E0804`
5. 添加到用户语言列表, 重启 `ctfmon`

## 13. 线程安全

- TSF/IMM32 是 **STA 单线程模型**, `g_comp`/`g`(引擎状态)/字典缓存无需锁
- `TSF::AddRef`/`Release` 使用 `InterlockedIncrement`/`InterlockedDecrement`
- `Compartment` 的 `_refCount` 使用原子操作
- `BuildGlobalObjects` 有 try/catch 异常保护
- `IME` 的 `HIMCMap` 使用 mutex 保护多 HIMC 实例

## 14. 已知问题 / TODO

- `emoji.txt` 存在但未接入引擎
- 设置栏按钮 2 (half) 和 4 (keyboard) 未实现功能
- `full.png` 和 `lock.png` 资源未被引用
- `example/` 是 Weasel 样例参考, 不参与编译, 可能是嵌套 git 仓库
