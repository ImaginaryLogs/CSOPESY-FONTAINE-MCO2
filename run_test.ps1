# Test Case 4 - Send commands with delays
Write-Host "Starting app..." -ForegroundColor Green

$app = Start-Process powershell -ArgumentList "-NoExit", "-Command", "cd '$PWD'; .\build\app.exe" -PassThru

Start-Sleep -Milliseconds 800

Write-Host "Sending: initialize" -ForegroundColor Yellow
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.SendKeys]::SendWait("initialize{ENTER}")

Start-Sleep -Milliseconds 500

Write-Host "Sending: scheduler-test" -ForegroundColor Yellow  
[System.Windows.Forms.SendKeys]::SendWait("scheduler-test{ENTER}")

Write-Host "Waiting 3 seconds for processes..." -ForegroundColor Cyan
Start-Sleep -Seconds 3

Write-Host "Sending: process-smi" -ForegroundColor Yellow
[System.Windows.Forms.SendKeys]::SendWait("process-smi{ENTER}")

Start-Sleep -Milliseconds 800

Write-Host "Sending: screen -ls" -ForegroundColor Yellow
[System.Windows.Forms.SendKeys]::SendWait("screen -ls{ENTER}")

Start-Sleep -Milliseconds 800

Write-Host "Sending vmstat 10 times..." -ForegroundColor Yellow
for ($i = 1; $i -le 10; $i++) {
    [System.Windows.Forms.SendKeys]::SendWait("vmstat{ENTER}")
    Start-Sleep -Milliseconds 1500
}

Write-Host "Test complete! Check the app window for output." -ForegroundColor Green
Write-Host "Press Enter to close app..." -ForegroundColor Yellow
Read-Host

[System.Windows.Forms.SendKeys]::SendWait("exit{ENTER}")
Start-Sleep -Seconds 1
Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
