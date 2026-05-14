@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\Projects\github\Cmulator
rmdir /s /q Build\_deps\peparse-src 2>nul
rmdir /s /q Build\_deps\peparse-build 2>nul
rmdir /s /q Build\_deps\peparse-subbuild 2>nul
cmake -B Build -S . -G "NMake Makefiles" -DUSE_VCPKG=OFF -DCMAKE_BUILD_TYPE=Debug
if %ERRORLEVEL% equ 0 cmake --build Build
if %ERRORLEVEL% equ 0 Build\test_deps.exe
