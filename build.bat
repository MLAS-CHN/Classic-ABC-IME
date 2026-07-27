@echo off
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
  set "MSBUILD=D:\VisualStudio\MSBuild\Current\Bin\MSBuild.exe"
) else (
  set "MSBUILD=msbuild"
)
set "SOLUTION=%~dp0abcime.sln"
set "OUTPUTDIR=%~dp0output"
set "INSTALL_TEMPLATE=%~dp0install.template.bat"
set "UNINSTALL_TEMPLATE=%~dp0uninstall.template.bat"

echo === Building ????ABC (x64) ===
"%MSBUILD%" "%SOLUTION%" /p:Configuration=Release /p:Platform=x64 /m /t:Rebuild
if %errorlevel% neq 0 goto :error

echo.
echo === Building ????ABC (Win32) ===
"%MSBUILD%" "%SOLUTION%" /p:Configuration=Release /p:Platform=Win32 /m /t:Rebuild
if %errorlevel% neq 0 goto :error

if not exist "%OUTPUTDIR%" mkdir "%OUTPUTDIR%"

copy /y "%INSTALL_TEMPLATE%" "%OUTPUTDIR%\install.bat" >nul
if %errorlevel% neq 0 goto :error

copy /y "%UNINSTALL_TEMPLATE%" "%OUTPUTDIR%\uninstall.bat" >nul
if %errorlevel% neq 0 goto :error

for %%F in ("%OUTPUTDIR%\*.exp" "%OUTPUTDIR%\*.pdb" "%OUTPUTDIR%\*.lib") do (
  if exist "%%~F" del /q "%%~F"
)

echo.
echo === Build complete ===
echo Output: %OUTPUTDIR%
pause
exit /b 0

:error
echo.
echo === Build FAILED ===
pause
exit /b 1
