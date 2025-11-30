@echo off
echo Starting Test Case #4
echo.
echo Step 1: Initialize and start scheduler
echo initialize | .\build\app.exe
timeout /t 2 /nobreak >nul

echo.
echo Step 2: Start scheduler-test and wait 2 seconds
start /min .\build\app.exe
timeout /t 1 /nobreak >nul
echo scheduler-test > test_pipe.txt
type test_pipe.txt | .\build\app.exe
timeout /t 2 /nobreak >nul

echo.
echo Step 3: Check process-smi
echo process-smi | .\build\app.exe

echo.
echo Step 4: Check screen -ls  
echo screen -ls | .\build\app.exe

echo.
echo Step 5-14: Run vmstat 10 times
for /L %%i in (1,1,10) do (
    echo.
    echo vmstat run %%i
    echo vmstat | .\build\app.exe
    timeout /t 1 /nobreak >nul
)

echo.
echo Test complete!
pause
