@echo off
REM Batch script to compile and run integration tests
REM Requires Visual Studio 2022 or MinGW to be installed

echo ========================================
echo Zero Ground Integration Test Compiler
echo ========================================
echo.

REM Try to find Visual Studio compiler
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALLDIR=%%i"
    )
)

if defined VSINSTALLDIR (
    echo Found Visual Studio at: %VSINSTALLDIR%
    call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
    
    echo.
    echo Compiling tests with MSVC...
    cl /EHsc /std:c++17 run_integration_tests.cpp /Fe:run_integration_tests.exe
    
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo Compilation successful!
        echo.
        echo Running tests...
        echo.
        run_integration_tests.exe
        
        if %ERRORLEVEL% EQU 0 (
            echo.
            echo ========================================
            echo All tests passed successfully!
            echo ========================================
            exit /b 0
        ) else (
            echo.
            echo ========================================
            echo Some tests failed!
            echo ========================================
            exit /b 1
        )
    ) else (
        echo.
        echo Compilation failed!
        exit /b 1
    )
) else (
    echo Visual Studio not found!
    echo.
    echo Trying MinGW g++...
    
    where g++ >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo Found g++ compiler
        echo.
        echo Compiling tests with g++...
        g++ -std=c++17 run_integration_tests.cpp -o run_integration_tests.exe
        
        if %ERRORLEVEL% EQU 0 (
            echo.
            echo Compilation successful!
            echo.
            echo Running tests...
            echo.
            run_integration_tests.exe
            
            if %ERRORLEVEL% EQU 0 (
                echo.
                echo ========================================
                echo All tests passed successfully!
                echo ========================================
                exit /b 0
            ) else (
                echo.
                echo ========================================
                echo Some tests failed!
                echo ========================================
                exit /b 1
            )
        ) else (
            echo.
            echo Compilation failed!
            exit /b 1
        )
    ) else (
        echo.
        echo ERROR: No C++ compiler found!
        echo.
        echo Please install one of the following:
        echo   - Visual Studio 2022 with C++ tools
        echo   - MinGW-w64 with g++
        echo.
        echo Then run this script again.
        exit /b 1
    )
)
