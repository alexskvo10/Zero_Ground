@echo off
echo ========================================
echo Zero Ground Performance Testing Script
echo ========================================
echo.
echo This script will help you run performance tests on the Zero Ground server.
echo.
echo INSTRUCTIONS:
echo 1. Make sure the server is compiled (Debug or Release configuration)
echo 2. This script will start the server
echo 3. Monitor the console output for performance metrics
echo 4. Performance metrics are logged every second
echo.
echo WHAT TO LOOK FOR:
echo - Map Generation Time: Should be less than 100ms
echo - FPS: Should be 55+ (target 60)
echo - Collision Detection: Should be less than 1ms per frame
echo - CPU Usage: Should be less than 40%% with 2 players
echo - Network Bandwidth: Monitor bytes/sec sent and received
echo.
echo Press any key to start the server...
pause > nul

echo.
echo Starting Zero Ground Server...
echo Performance metrics will be displayed in the console.
echo.
echo To test with a client:
echo 1. Open another terminal
echo 2. Run: Zero_Ground_client\x64\Debug\Zero_Ground_client.exe
echo 3. Connect to the server IP shown
echo 4. Click READY, then PLAY on server
echo.

cd ..
if exist "Zero_Ground\x64\Debug\Zero_Ground.exe" (
    echo Running Debug build...
    Zero_Ground\x64\Debug\Zero_Ground.exe
) else if exist "Zero_Ground\x64\Release\Zero_Ground.exe" (
    echo Running Release build...
    Zero_Ground\x64\Release\Zero_Ground.exe
) else (
    echo ERROR: Server executable not found!
    echo Please compile the project first using Visual Studio.
    echo.
    pause
    exit /b 1
)

echo.
echo Server closed.
pause
