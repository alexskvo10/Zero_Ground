@echo off
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cl /EHsc /std:c++17 run_weapon_tests.cpp /Fe:run_weapon_tests.exe
if %ERRORLEVEL% EQU 0 (
    echo Compilation successful!
    run_weapon_tests.exe
) else (
    echo Compilation failed!
)
