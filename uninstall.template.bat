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

echo === Uninstalling Classic ABC Input Method ===
echo.

REM --- 1. Remove from user input method list ---
echo [1/5] Removing from user input methods...

powershell -NoProfile -Command ^
  "$signature = '[DllImport(\"input.dll\", CharSet = CharSet.Unicode)] public static extern bool InstallLayoutOrTip(string psz, uint dwFlags);';" ^
  "$type = Add-Type -MemberDefinition $signature -Name 'InputHelper' -Namespace 'Win32' -PassThru;" ^
  "$type::InstallLayoutOrTip('0804:{3D02CAB6-2B8E-4781-BA20-1C9267529467}', 0x2);" ^
  "$type::InstallLayoutOrTip('E05E0804', 0x2);"

echo   Done
echo.

REM --- 2. Unregister TSF DLLs (from output dir) ---
echo [2/5] Unregistering TSF DLLs...

regsvr32 /u /s "%SRCDIR%abcimex64.dll" 2>nul
"%SYSWOW64%\regsvr32.exe" /u /s "%SRCDIR%abcime.dll" 2>nul

echo   Done
echo.

REM --- 3. Remove IMM32 keyboard layout registry ---
echo [3/5] Removing IMM32 registry entries...

reg delete "HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts\E05E0804" /f >nul 2>&1

echo   Done
echo.

REM --- 4. Delete IME files from system directories ---
echo [4/5] Deleting IME files...

if exist "%SYS32%\abcime_test.ime" (
  del /f /q "%SYS32%\abcime_test.ime" >nul 2>&1
  echo   Deleted System32\abcime_test.ime
)

if exist "%SYSWOW64%\abcime_test.ime" (
  del /f /q "%SYSWOW64%\abcime_test.ime" >nul 2>&1
  echo   Deleted SysWOW64\abcime_test.ime
)

echo.

REM --- 5. Delete runtime files (data/res in ProgramData) ---
echo [5/5] Deleting runtime files...

if exist "%RUNTIMEDIR%" (
  rmdir /s /q "%RUNTIMEDIR%" >nul 2>&1
  echo   Deleted %RUNTIMEDIR%
)

echo.

REM --- Restart text input ---
taskkill /f /im ctfmon.exe >nul 2>&1
taskkill /f /im TextInputHost.exe >nul 2>&1

echo.
echo === Uninstallation complete ===
echo Please restart or log off for changes to take effect.
echo.
pause
