@echo off
cls
echo =====================================================
echo   Test Case 4 - Running Automated Test
echo =====================================================
echo.
echo Executing test sequence...
echo.

(
echo initialize
timeout /t 1 /nobreak >nul
echo scheduler-test
timeout /t 3 /nobreak >nul
echo process-smi
timeout /t 1 /nobreak >nul
echo screen -ls
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo vmstat
timeout /t 1 /nobreak >nul
echo exit
) | .\build\app.exe

echo.
echo =====================================================
echo   Test Complete!
echo =====================================================
pause
