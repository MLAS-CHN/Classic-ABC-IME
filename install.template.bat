@echo off
setlocal enableextensions

REM Auto-elevate to admin
net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Requesting administrator privileges...
  powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

set "SRCDIR=%~dp0"
set "RUNTIMEDIR=%ProgramData%\ClassicABC"
set "SYS32=%windir%\System32"
set "SYSWOW64=%windir%\SysWOW64"

echo === Installing Classic ABC Input Method ===
echo.

REM --- 0. Deploy runtime files (data/res) shared by IME ---
echo [0/5] Deploying runtime files...

if not exist "%RUNTIMEDIR%" mkdir "%RUNTIMEDIR%"
if not exist "%RUNTIMEDIR%\data" mkdir "%RUNTIMEDIR%\data"
if not exist "%RUNTIMEDIR%\res" mkdir "%RUNTIMEDIR%\res"

if exist "%SRCDIR%data" (
  xcopy /y /e "%SRCDIR%data\*" "%RUNTIMEDIR%\data\" >nul 2>&1
  echo   Copied data directory
)
if exist "%SRCDIR%res" (
  xcopy /y /e "%SRCDIR%res\*" "%RUNTIMEDIR%\res\" >nul 2>&1
  echo   Copied res directory
)

echo.

REM --- 1. Register TSF (Text Services Framework) ---
echo [1/5] Registering TSF DLLs...

if exist "%SRCDIR%abcimex64.dll" (
  regsvr32 /s "%SRCDIR%abcimex64.dll"
  if %errorlevel% neq 0 echo   WARNING: Failed to register abcimex64.dll
  echo   Registered abcimex64.dll ^(64-bit TSF^)
)

if exist "%SRCDIR%abcime.dll" (
  "%SYSWOW64%\regsvr32.exe" /s "%SRCDIR%abcime.dll"
  if %errorlevel% neq 0 echo   WARNING: Failed to register abcime.dll
  echo   Registered abcime.dll ^(32-bit TSF^)
)

echo.

REM --- 2. Install IMM32 IME files to system directories ---
echo [2/5] Installing IME files...

if exist "%SRCDIR%abcimex64.ime" (
  copy /y "%SRCDIR%abcimex64.ime" "%SYS32%\abcime_test.ime" >nul
  echo   Copied abcimex64.ime to System32
)

if exist "%SRCDIR%abcime.ime" (
  copy /y "%SRCDIR%abcime.ime" "%SYSWOW64%\abcime_test.ime" >nul
  echo   Copied abcime.ime to SysWOW64
)

echo.

REM --- 3. Register IMM32 keyboard layout ---
echo [3/5] Registering IMM32 keyboard layout...

reg add "HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\E05E0804" /v "IME File" /t REG_SZ /d "abcime_test.ime" /f >nul
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\E05E0804" /v "Layout File" /t REG_SZ /d "abcime_test.ime" /f >nul
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\E05E0804" /v "Layout Text" /t REG_SZ /d "Classic ABC" /f >nul

echo   Registered layout E05E0804
echo.

REM --- 4. Add to user input method list ---
echo [4/5] Adding to user input methods...

powershell -NoProfile -Command ^
  "$signature = '[DllImport(\"input.dll\", CharSet = CharSet.Unicode)] public static extern bool InstallLayoutOrTip(string psz, uint dwFlags);';" ^
  "$type = Add-Type -MemberDefinition $signature -Name 'InputHelper' -Namespace 'Win32' -PassThru;" ^
  "$tsf = '0804:{3D02CAB6-2B8E-4781-BA20-1C9267529467}';" ^
  "$imm32 = 'E05E0804';" ^
  "$r1 = $type::InstallLayoutOrTip($tsf, 0);" ^
  "$r2 = $type::InstallLayoutOrTip($imm32, 0);" ^
  "Write-Host ('  TSF profile: ' + $(if ($r1) {'OK'} else {'Failed or already exists'}));" ^
  "Write-Host ('  IMM32 layout: ' + $(if ($r2) {'OK'} else {'Failed or already exists'}));"

echo.

REM --- 5. Restart ctfmon ---
echo [5/5] Restarting text input service...

taskkill /f /im ctfmon.exe >nul 2>&1
taskkill /f /im TextInputHost.exe >nul 2>&1

echo.
echo === Installation complete ===
echo Press Win+Space to switch to Classic ABC.
echo.
pause
