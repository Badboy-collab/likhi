# Likhi (লিখি) - PowerShell TSF Registration (regsvr32-independent)
# Calls DllRegisterServer/DllUnregisterServer DIRECTLY via register_selfcheck.exe so
# success is deterministic and the real registry result is printed.
# NOTE: must run from an ELEVATED (Administrator) PowerShell for the machine-wide
#       TSF profile (HKLM\SOFTWARE\Microsoft\CTF\TIP) to register.
param (
    [switch]$Unregister
)

$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
$dll = Join-Path $root 'bangla_tsf.dll'
$harness = Join-Path $root 'register_selfcheck.exe'

if (-not (Test-Path $dll)) {
    Write-Error "bangla_tsf.dll not found next to this script: $dll"
    exit 1
}
if (-not (Test-Path $harness)) {
    Write-Error "register_selfcheck.exe not found next to this script: $harness"
    exit 1
}

# Deploy lexicon data for the engine
$appDataDir = Join-Path $env:APPDATA 'PC-Bangla-Typing-App'
if (-not (Test-Path $appDataDir)) { New-Item -ItemType Directory -Path $appDataDir -Force | Out-Null }
$lexSrc = Join-Path $root 'data\lexicon.bin'
if (-not (Test-Path $lexSrc)) { $lexSrc = Join-Path $root '..\..\engine\data\lexicon.bin' }
if (Test-Path $lexSrc) { Copy-Item -Path $lexSrc -Destination (Join-Path $appDataDir 'lexicon.bin') -Force }

if ($Unregister) {
    Write-Host "[1/1] Unregistering $dll ..." -ForegroundColor Yellow
    & $harness $dll unreg
    Write-Host "Done. Full cleanup: HKLM/HKCU CLSID + CTF\TIP keys still containing Likhi should be removed;"
    Write-Host "run the elevated cleanup in the project's build\do_register.ps1 or regedit if stale keys remain."
    exit 0
}

Write-Host "[1/2] Unregistering any previous Likhi registration ..." -ForegroundColor Yellow
& $harness $dll unreg

Write-Host "[2/2] Registering $dll ..." -ForegroundColor Cyan
& $harness $dll

Write-Host ""
Write-Host "=========================================================" -ForegroundColor Green
Write-Host " Verify below: 'CLSID registered ->' must show THIS dll path," -ForegroundColor Green
Write-Host " and HKLM\SOFTWARE\Microsoft\CTF\TIP\...\LanguageProfile\0x00000445" -ForegroundColor Green
Write-Host " must exist (Bengali-Bangladesh). If DllRegisterServer printed FAILURE," -ForegroundColor Green
Write-Host " re-run this script from an elevated (Run as administrator) PowerShell." -ForegroundColor Green
Write-Host "=========================================================" -ForegroundColor Green
Write-Host "Next: 1) Reboot or sign out/in once so TSF reloads."
Write-Host "      2) Win + Space -> select 'Bangla (Bangladesh) - Likhi'."
Write-Host "      3) Test in Notepad: ami + Space -> 'আমি ' (one space)."
