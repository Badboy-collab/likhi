@echo off
setlocal
echo =========================================================
echo   Likhi - Uninstallation Script
echo =========================================================
echo.

net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Administrator privileges are required!
    echo Please right-click this script and select 'Run as administrator'.
    echo.
    pause
    exit /b 1
)

set SCRIPT_DIR=%~dp0
set DLL_PATH=%SCRIPT_DIR%bangla_tsf.dll
if not exist "%DLL_PATH%" set DLL_PATH=%SCRIPT_DIR%..\..\build\bangla_tsf.dll
if not exist "%DLL_PATH%" set DLL_PATH=%SCRIPT_DIR%build\bangla_tsf.dll

echo [1/3] Unregistering Likhi from Windows TSF and COM...
if exist "%DLL_PATH%" (
    regsvr32.exe /u /s "%DLL_PATH%"
) else (
    regsvr32.exe /u /s bangla_tsf.dll
)

echo [2/3] Removing Start Menu shortcuts...
set SHORTCUT_DIR=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Likhi
if exist "%SHORTCUT_DIR%" (
    rmdir /S /Q "%SHORTCUT_DIR%" >nul 2>&1
)

echo [3/3] Removing Windows Uninstall registry entries...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Likhi" /f >nul 2>&1

echo.
echo =========================================================
echo   [SUCCESS] Likhi successfully uninstalled!
echo =========================================================
echo.
pause