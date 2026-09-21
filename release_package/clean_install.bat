@echo off
cd /d "%~dp0"
echo ============================================================
echo   LIKHI (লিখি) v1.0.0 - FRESH CLEAN INSTALL
echo   Deleting Old Installation and Performing Clean Setup...
echo ============================================================
echo.
echo Launching Administrator PowerShell to perform clean install...
powershell -Command "Start-Process powershell -Verb RunAs -ArgumentList '-NoExit -ExecutionPolicy Bypass -File ""%~dp0..\tools\installer\clean_install.ps1""'"
pause
