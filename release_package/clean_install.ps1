# tools/installer/clean_install.ps1
# Requires Administrator privileges
$ErrorActionPreference = "Continue"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "   LIKHI (লিখি) v1.0.0 — FRESH CLEAN INSTALLATION" -ForegroundColor Cyan
Write-Host "   Removing previous version & installing fresh build..." -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# 1. Close processes that might lock DLLs
Write-Host "`n[1/6] Closing applications that might hold locks..." -ForegroundColor Yellow
$procs = @("WhatsApp", "WhatsApp.Root", "WINWORD", "EXCEL", "POWERPNT", "bangla_settings")
foreach ($p in $procs) {
    Get-Process -Name $p -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Seconds 1

# 2. Unregister previous COM / TSF components
Write-Host "`n[2/6] Unregistering previous installation..." -ForegroundColor Yellow
$oldDll = "C:\Program Files\Likhi\bangla_tsf.dll"
$oldDll32 = "C:\Program Files\Likhi\bangla_tsf32.dll"
if (Test-Path $oldDll) {
    regsvr32.exe /u /s "$oldDll"
}
if (Test-Path $oldDll32) {
    if (Test-Path "$env:WINDIR\SysWOW64\regsvr32.exe") {
        & "$env:WINDIR\SysWOW64\regsvr32.exe" /u /s "$oldDll32"
    }
}

# Clean old registry keys
$clsid = "{B4F1470A-7C69-4C62-972F-6379532856E1}"
Remove-Item -Path "HKLM:\SOFTWARE\Classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "HKCU:\Software\Classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "HKLM:\SOFTWARE\Microsoft\CTF\TIP\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "HKCU:\Software\Microsoft\CTF\TIP\$clsid" -Recurse -Force -ErrorAction SilentlyContinue

# 3. Wipe old files in C:\Program Files\Likhi
Write-Host "`n[3/6] Removing old installation files in C:\Program Files\Likhi..." -ForegroundColor Yellow
$installDir = "C:\Program Files\Likhi"
if (Test-Path $installDir) {
    # If DLLs are locked by active background apps, rename them so new files can be written cleanly
    if (Test-Path "$installDir\bangla_tsf.dll") {
        try {
            Remove-Item -Path "$installDir\bangla_tsf.dll" -Force -ErrorAction Stop
        } catch {
            Move-Item -Path "$installDir\bangla_tsf.dll" -Destination "$installDir\bangla_tsf.dll.old" -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path "$installDir\bangla_tsf32.dll") {
        try {
            Remove-Item -Path "$installDir\bangla_tsf32.dll" -Force -ErrorAction Stop
        } catch {
            Move-Item -Path "$installDir\bangla_tsf32.dll" -Destination "$installDir\bangla_tsf32.dll.old" -Force -ErrorAction SilentlyContinue
        }
    }
    Get-ChildItem -Path $installDir -Exclude "*.old" -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
} else {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

# 4. Copy fresh binaries
Write-Host "`n[4/6] Installing fresh binaries..." -ForegroundColor Yellow
$scriptDir = $PSScriptRoot
$rootDir = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$buildDir = Join-Path $rootDir "build"
if (-not (Test-Path (Join-Path $buildDir "bangla_tsf.dll"))) {
    $buildDir = Join-Path $rootDir "release_package"
}
if (-not (Test-Path (Join-Path $buildDir "bangla_tsf.dll"))) {
    $buildDir = $scriptDir
}

$lexiconPath = Join-Path $rootDir "engine\data\lexicon.bin"
if (-not (Test-Path $lexiconPath)) {
    $lexiconPath = Join-Path $rootDir "release_package\data\lexicon.bin"
}
if (-not (Test-Path $lexiconPath)) {
    $lexiconPath = Join-Path $scriptDir "data\lexicon.bin"
}

$enableTipPath = Join-Path $scriptDir "enable_tip.ps1"
if (-not (Test-Path $enableTipPath)) {
    $enableTipPath = Join-Path $rootDir "tools\installer\enable_tip.ps1"
}

$disableTipPath = Join-Path $scriptDir "disable_tip.ps1"
if (-not (Test-Path $disableTipPath)) {
    $disableTipPath = Join-Path $rootDir "tools\installer\disable_tip.ps1"
}

$dataDir = Join-Path $installDir "data"
$toolsDir = Join-Path $installDir "tools"

New-Item -ItemType Directory -Path $dataDir -Force | Out-Null
New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null

Copy-Item -Path (Join-Path $buildDir "bangla_tsf.dll") -Destination (Join-Path $installDir "bangla_tsf.dll") -Force
Copy-Item -Path (Join-Path $buildDir "bangla_tsf32.dll") -Destination (Join-Path $installDir "bangla_tsf32.dll") -Force
Copy-Item -Path (Join-Path $buildDir "bangla_settings.exe") -Destination (Join-Path $installDir "bangla_settings.exe") -Force
Copy-Item -Path $lexiconPath -Destination (Join-Path $dataDir "lexicon.bin") -Force
Copy-Item -Path $enableTipPath -Destination (Join-Path $toolsDir "enable_tip.ps1") -Force
Copy-Item -Path $disableTipPath -Destination (Join-Path $toolsDir "disable_tip.ps1") -Force

# 5. Grant permissions to ALL APPLICATION PACKAGES for WhatsApp / UWP
Write-Host "`n[5/6] Setting security permissions for WhatsApp & Windows Store apps..." -ForegroundColor Yellow
icacls.exe "$installDir" /grant "*S-1-15-2-1:(OI)(CI)(RX)" /T | Out-Null

# 6. Register COM, TSF, and Language Profiles in HKLM
Write-Host "`n[6/6] Registering TSF components and Language profiles in HKLM..." -ForegroundColor Yellow
regsvr32.exe /s (Join-Path $installDir "bangla_tsf.dll")
if (Test-Path "$env:WINDIR\SysWOW64\regsvr32.exe") {
    & "$env:WINDIR\SysWOW64\regsvr32.exe" /s (Join-Path $installDir "bangla_tsf32.dll")
}

# Configure Categories in HKLM
$keyboard = "{34745C63-B2F0-4784-8B67-5E12C8701A31}"
$immersive = "{13A016DF-560B-46CD-947A-4C3AF1E0E35D}"
$systray = "{25504FB4-7BAB-4BC1-9C69-CF81890F0EF5}"
$r = "HKLM:\SOFTWARE\Microsoft\CTF\TIP\$clsid"

New-Item -Path "$r\Category\Category\$keyboard\$clsid" -Force | Out-Null
New-Item -Path "$r\Category\Category\$immersive\$clsid" -Force | Out-Null
New-Item -Path "$r\Category\Category\$systray\$clsid" -Force | Out-Null

Set-ItemProperty -Path "$r\Category\Item\$clsid" -Name $keyboard -Value 0 -Type DWord -Force
Set-ItemProperty -Path "$r\Category\Item\$clsid" -Name $immersive -Value 0 -Type DWord -Force
Set-ItemProperty -Path "$r\Category\Item\$clsid" -Name $systray -Value 0 -Type DWord -Force

# Enable TIP in language bar
& (Join-Path $toolsDir "enable_tip.ps1")

Write-Host "`n============================================================" -ForegroundColor Green
Write-Host "   [SUCCESS] LIKHI v1.0.0 FRESH INSTALLATION COMPLETE!" -ForegroundColor Green
Write-Host "   WhatsApp, Word, Chrome & Windows Store apps are ready." -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host "1. Press Win + Space to select 'Likhi (লিখি)'." -ForegroundColor White
Write-Host "2. Open WhatsApp or any app and start typing!" -ForegroundColor White
