# Likhi (লিখি) - PowerShell TSF Registration Script
# "বাংলা লিখুন, সহজেই।"
param (
    [switch]$Unregister
)

$candidates = @(
    Join-Path $PSScriptRoot "bangla_tsf.dll",
    Join-Path $PSScriptRoot "..\..\build\bangla_tsf.dll",
    Join-Path $PSScriptRoot "build\bangla_tsf.dll"
)

$resolvedDll = $null
foreach ($c in $candidates) {
    if (Test-Path $c) {
        $resolvedDll = [System.IO.Path]::GetFullPath($c)
        break
    }
}

if ($Unregister) {
    if ($resolvedDll) {
        Write-Host "Unregistering $resolvedDll..." -ForegroundColor Yellow
        regsvr32.exe /u /s "$resolvedDll"
    } else {
        regsvr32.exe /u /s "bangla_tsf.dll"
    }
    Write-Host "[SUCCESS] Unregistered Likhi (লিখি)." -ForegroundColor Green
} else {
    if (-not $resolvedDll) {
        Write-Error "Could not locate bangla_tsf.dll. Please compile the project first."
        exit 1
    }

    # Ensure %APPDATA%\PC-Bangla-Typing-App exists and copy lexicon
    $appDataDir = Join-Path $env:APPDATA "PC-Bangla-Typing-App"
    if (-not (Test-Path $appDataDir)) {
        New-Item -ItemType Directory -Path $appDataDir -Force | Out-Null
    }

    $lexCandidates = @(
        Join-Path $PSScriptRoot "data\lexicon.bin",
        Join-Path $PSScriptRoot "..\..\engine\data\lexicon.bin",
        Join-Path $PSScriptRoot "engine\data\lexicon.bin"
    )
    foreach ($lex in $lexCandidates) {
        if (Test-Path $lex) {
            Copy-Item -Path $lex -Destination (Join-Path $appDataDir "lexicon.bin") -Force
            break
        }
    }

    Write-Host "Registering $resolvedDll with Windows COM and TSF..." -ForegroundColor Cyan
    regsvr32.exe /s "$resolvedDll"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "=========================================================" -ForegroundColor Green
        Write-Host " [SUCCESS] Likhi (লিখি) registered successfully!" -ForegroundColor Green
        Write-Host " 'বাংলা লিখুন, সহজেই।'" -ForegroundColor Yellow
        Write-Host "=========================================================" -ForegroundColor Green
        Write-Host "1. Press Win + Space to select 'Likhi (লিখি)'." -ForegroundColor White
        Write-Host "2. Start typing in Notepad, Chrome, Word, or any app!" -ForegroundColor White
    } else {
        Write-Error "Registration failed. Ensure you run this from an elevated Administrator prompt."
    }
}
