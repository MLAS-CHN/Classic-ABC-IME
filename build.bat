@echo off
set "MSBUILD=D:\VisualStudio\MSBuild\Current\Bin\MSBuild.exe"
set "SOLUTION=%~dp0abcime.sln"

echo === Building ????ABC (x64) ===
"%MSBUILD%" "%SOLUTION%" /p:Configuration=Release /p:Platform=x64 /m /t:Rebuild
if %errorlevel% neq 0 goto :error

echo.
echo === Building ????ABC (Win32) ===
"%MSBUILD%" "%SOLUTION%" /p:Configuration=Release /p:Platform=Win32 /m /t:Rebuild
if %errorlevel% neq 0 goto :error

echo.
echo === Build complete ===
echo Output: %~dp0output\
pause
exit /b 0

:error
echo.
echo === Build FAILED ===
pause
exit /b 1