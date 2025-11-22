@echo off
echo Compiling Zero_Ground server...
cd Zero_Ground
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Zero_Ground.vcxproj /p:Configuration=Debug /p:Platform=x64
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b %errorlevel%
)
echo Compilation successful!
pause
