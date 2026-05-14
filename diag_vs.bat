@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"
echo VSINSTALLDIR=%VSINSTALLDIR%
echo VCToolsVersion=%VCToolsVersion%
echo VCToolsInstallDir=%VCToolsInstallDir%
where cl.exe
cl.exe 2>&1
