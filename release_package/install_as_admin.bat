@echo off
cd /d "%~dp0"
echo ============================================================
echo   LIKHI (লিখি) v1.0.0 - SYSTEM-WIDE INSTALLATION
echo   WhatsApp & Windows Store App Support Included
echo ============================================================
echo.
echo Closing WhatsApp to release locked components if running...
taskkill /f /im WhatsApp.Root.exe >nul 2>&1
taskkill /f /im WhatsApp.exe >nul 2>&1
echo.
echo Requesting Administrator permission...
powershell -Command "Start-Process '%~dp0Likhi_Setup_v1.0.0.exe' -Verb RunAs"
pause
