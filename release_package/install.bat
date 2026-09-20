@echo off
setlocal
echo =========================================================
echo   Likhi - Windows Installation and Setup
echo   "Bangla Likhoon, Sohojei."
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

set SETTINGS_EXE=%SCRIPT_DIR%bangla_settings.exe
if not exist "%SETTINGS_EXE%" set SETTINGS_EXE=%SCRIPT_DIR%..\..\build\bangla_settings.exe
if not exist "%SETTINGS_EXE%" set SETTINGS_EXE=%SCRIPT_DIR%build\bangla_settings.exe

set LEX_SRC=%SCRIPT_DIR%data\lexicon.bin
if not exist "%LEX_SRC%" set LEX_SRC=%SCRIPT_DIR%..\..\engine\data\lexicon.bin
if not exist "%LEX_SRC%" set LEX_SRC=%SCRIPT_DIR%engine\data\lexicon.bin

if not exist "%DLL_PATH%" (
    echo [ERROR] Could not locate bangla_tsf.dll!
    pause
    exit /b 1
)

set APP_DATA_DIR=%APPDATA%\PC-Bangla-Typing-App
if not exist "%APP_DATA_DIR%" mkdir "%APP_DATA_DIR%"

set DLL32_PATH=%SCRIPT_DIR%bangla_tsf32.dll
if not exist "%DLL32_PATH%" set DLL32_PATH=%SCRIPT_DIR%..\..\build\bangla_tsf32.dll
if not exist "%DLL32_PATH%" set DLL32_PATH=%SCRIPT_DIR%build\bangla_tsf32.dll

if exist "%LEX_SRC%" (
    echo [1/4] Deploying dictionary data to %APP_DATA_DIR%\lexicon.bin...
    copy /Y "%LEX_SRC%" "%APP_DATA_DIR%\lexicon.bin" >nul
)

echo [2/4] Registering Likhi with Windows Text Services Framework...
regsvr32.exe /u /s "%DLL_PATH%"
regsvr32.exe /s "%DLL_PATH%"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] TSF COM Registration failed with error %ERRORLEVEL%.
    pause
    exit /b 1
)

if exist "%WINDIR%\SysWOW64\regsvr32.exe" if exist "%DLL32_PATH%" (
    echo Registering 32-bit Likhi TSF components for 32-bit apps (MS Office)...
    %WINDIR%\SysWOW64\regsvr32.exe /u /s "%DLL32_PATH%"
    %WINDIR%\SysWOW64\regsvr32.exe /s "%DLL32_PATH%"
)

echo [3/4] Creating Start Menu shortcuts...
set SHORTCUT_DIR=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Likhi
if not exist "%SHORTCUT_DIR%" mkdir "%SHORTCUT_DIR%"

powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%SHORTCUT_DIR%\Likhi Settings.lnk'); $s.TargetPath = '%SETTINGS_EXE%'; $s.Description = 'Likhi Settings'; $s.Save()" >nul 2>&1
powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%SHORTCUT_DIR%\Uninstall Likhi.lnk'); $s.TargetPath = '%SCRIPT_DIR%uninstall.bat'; $s.Description = 'Uninstall Likhi'; $s.Save()" >nul 2>&1

echo [4/4] Registering with Windows Installed Apps...
set REG_KEY=HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Likhi
reg add "%REG_KEY%" /v "DisplayName" /d "Likhi (Likhi) - PC Bangla Typing App" /f >nul 2>&1
reg add "%REG_KEY%" /v "DisplayVersion" /d "1.0.0" /f >nul 2>&1
reg add "%REG_KEY%" /v "Publisher" /d "Likhi" /f >nul 2>&1
reg add "%REG_KEY%" /v "UninstallString" /d "\"%SCRIPT_DIR%uninstall.bat\"" /f >nul 2>&1
reg add "%REG_KEY%" /v "InstallLocation" /d "\"%SCRIPT_DIR%\"" /f >nul 2>&1
reg add "%REG_KEY%" /v "Comments" /d "Bangla Likhoon, Sohojei." /f >nul 2>&1

echo.
echo =========================================================
echo   [SUCCESS] Likhi successfully installed and registered!
echo =========================================================
echo.
echo Next Steps:
echo 1. Press Win + Space on your keyboard.
echo 2. Select 'Bangla (Bangladesh) - Likhi'.
echo 3. Open Notepad or any app and type!
echo 4. Search for 'Likhi Settings' in Start Menu.
echo.