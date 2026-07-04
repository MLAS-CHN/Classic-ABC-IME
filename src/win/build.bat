@echo off
setlocal
call "%~dp0env.bat"

where msbuild >nul 2>&1
if %errorlevel% neq 0 (
  set "MSBUILD=D:\VisualStudio\MSBuild\Current\Bin\MSBuild.exe"
) else (
  set "MSBUILD=msbuild"
)

echo Building Release x64...
"%MSBUILD%" "%~dp0weasel.sln" /p:Configuration=Release /p:Platform=x64 /m /t:Rebuild /nologo /v:m
if %errorlevel% neq 0 exit /b %errorlevel%

echo Building Release Win32...
"%MSBUILD%" "%~dp0weasel.sln" /p:Configuration=Release /p:Platform=Win32 /m /t:Rebuild /nologo /v:m
if %errorlevel% neq 0 exit /b %errorlevel%

del /q "%~dp0output\*.pdb" "%~dp0output\*.lib" "%~dp0output\*.exp" 2>nul
rmdir /s /q "%~dp0msbuild" 2>nul

echo Done.
dir /b "%~dp0output\*.dll" "%~dp0output\*.ime" 2>nul
