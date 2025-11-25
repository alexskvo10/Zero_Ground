@echo off
echo Compiling Zero_Ground client...
cd Zero_Ground_client
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Zero_Ground_client.vcxproj /p:Configuration=Debug /p:Platform=Win32
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b %errorlevel%
)
echo Compilation successful!
pause
