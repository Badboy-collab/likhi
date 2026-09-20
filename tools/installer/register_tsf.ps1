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

$candidates32 = @(
    Join-Path $PSScriptRoot "bangla_tsf32.dll",
    Join-Path $PSScriptRoot "..\..\build\bangla_tsf32.dll",
    Join-Path $PSScriptRoot "build\bangla_tsf32.dll"
)

$resolvedDll32 = $null
foreach ($c in $candidates32) {
    if (Test-Path $c) {
        $resolvedDll32 = [System.IO.Path]::GetFullPath($c)
        break
    }
}

if ($Unregister) {
    if ($resolvedDll) {
        Write-Host "Unregistering 64-bit $resolvedDll..." -ForegroundColor Yellow
        regsvr32.exe /u /s "$resolvedDll"
    } else {
        regsvr32.exe /u /s "bangla_tsf.dll"
    }
    if ((Test-Path "$env:WINDIR\SysWOW64\regsvr32.exe") -and $resolvedDll32) {
        Write-Host "Unregistering 32-bit $resolvedDll32..." -ForegroundColor Yellow
        & "$env:WINDIR\SysWOW64\regsvr32.exe" /u /s "$resolvedDll32"
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

    Write-Host "Registering 64-bit $resolvedDll with Windows COM and TSF..." -ForegroundColor Cyan
    regsvr32.exe /s "$resolvedDll"
    $success64 = ($LASTEXITCODE -eq 0)

    $success32 = $true
    if ((Test-Path "$env:WINDIR\SysWOW64\regsvr32.exe") -and $resolvedDll32) {
        Write-Host "Registering 32-bit $resolvedDll32 (for 32-bit MS Office)..." -ForegroundColor Cyan
        & "$env:WINDIR\SysWOW64\regsvr32.exe" /s "$resolvedDll32"
        $success32 = ($LASTEXITCODE -eq 0)
    }

    if ($success64 -and $success32) {
        Write-Host "=========================================================" -ForegroundColor Green
        Write-Host " [SUCCESS] Likhi (লিখি) dual-arch registered successfully!" -ForegroundColor Green
        Write-Host " 'বাংলা লিখুন, সহজেই।'" -ForegroundColor Yellow
        Write-Host "=========================================================" -ForegroundColor Green
        Write-Host "1. Press Win + Space to select 'Likhi (লিখি)'." -ForegroundColor White
        Write-Host "2. Start typing in Notepad, Chrome, Word, or any app!" -ForegroundColor White
    } else {
        Write-Error "Registration failed. Ensure you run this from an elevated Administrator prompt."
    }
}
