@echo off
setlocal
cd /d "%~dp0"
echo ============================================================
echo   LIKHI (লিখি) v1.0.0 - FRESH CLEAN INSTALLATION
echo   Cleaning Old Files and Installing Fresh Build...
echo ============================================================
echo.

echo [1/2] Requesting Administrator privileges to clean and install...
powershell -Command "Start-Process powershell -Verb RunAs -Wait -ArgumentList '-ExecutionPolicy Bypass -File ""%~dp0clean_install.ps1""'"

echo.
echo [2/2] Configuring Windows Language Switcher (Win + Space) for current user...
powershell -ExecutionPolicy Bypass -File "%~dp0enable_tip.ps1"

echo.
echo ============================================================
echo   [SUCCESS] FRESH INSTALLATION COMPLETE!
echo   1. Press Win + Space to select Likhi (লিখি).
echo   2. Test typing in WhatsApp, Word, or Chrome.
echo ============================================================
echo.
pause
