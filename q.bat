@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\Projects\github\Cmulator
cmake --build Build --config Debug
