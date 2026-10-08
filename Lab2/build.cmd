@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Installer was not found.
  exit /b 1
)
for /f "tokens=*" %%i in ('"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VSROOT=%%i"
if not defined VSROOT (
  echo Install Desktop development with C++ in Visual Studio Installer.
  exit /b 1
)
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
if not exist "%~dp0build" mkdir "%~dp0build"
pushd "%~dp0build"
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 "%~dp0main.cpp" /Fe:GroovyGilb.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comdlg32.lib
set "BUILD_RESULT=%errorlevel%"
popd
exit /b %BUILD_RESULT%
