@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"
set LOCALAPPDATA=%USERPROFILE%\AppData\Local
set APPDATA=%USERPROFILE%\AppData\Roaming
cd /d D:\Projects\github\Cmulator
rmdir /s /q Build 2>nul
cmake -B Build -S . -G "Visual Studio 18 2026" -A x64 -DUSE_VCPKG=OFF
if %ERRORLEVEL% equ 0 cmake --build Build --config Debug
