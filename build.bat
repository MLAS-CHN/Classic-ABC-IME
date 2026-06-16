@echo off
cd /d "%~dp0src\win"
if exist build rmdir /S /Q build
mkdir build
xcopy /E /I /Y ..\..\res build\res >nul
xcopy /E /I /Y ..\..\data build\data >nul

call "D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat"

set CL=/I.. /I. /EHsc /O2 /std:c++17 /utf-8
set ENGINE=..\candidate_item.cpp ..\pinyin_composition.cpp ..\pinyin_data.cpp ..\pinyin_file_io.cpp ..\pinyin_matcher.cpp ..\pinyin_split.cpp ..\pinyin_virtual_cursor.cpp ..\word_matcher.cpp ..\util.cpp
set LNK=gdiplus.lib ole32.lib user32.lib gdi32.lib advapi32.lib oleaut32.lib

cl /LD %ENGINE% proto.cpp proto_core.cpp proto_engine.cpp proto_ui.cpp /Fe:build\proto.dll /link /DEF:proto.def %LNK%
if %errorlevel% neq 0 exit /b %errorlevel%

cl install_proto.cpp /Fe:build\install_proto.exe /link ole32.lib user32.lib /SUBSYSTEM:WINDOWS
cl uninstall_proto.cpp /Fe:build\uninstall_proto.exe /link ole32.lib user32.lib /SUBSYSTEM:WINDOWS

echo.
echo ==== Build complete ====
dir build\*.dll build\*.exe
