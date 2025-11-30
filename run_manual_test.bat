@echo off
cls
echo =====================================================
echo   Test Case 4 - Manual Execution
echo =====================================================
echo.
echo Config: 4 CPUs, RR, 32KB RAM, 32B frames, 8B/proc
echo.
echo Steps:
echo   1. Type: initialize
echo   2. Type: scheduler-test
echo   3. Wait 2-3 seconds
echo   4. Type: process-smi
echo   5. Type: screen -ls
echo   6. Type: vmstat  (repeat 10 times)
echo   7. Type: exit
echo.
echo =====================================================
echo.
.\build\app.exe
pause
