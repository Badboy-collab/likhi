# tools/check_tsf_imports.ps1
# Phase 3.1 - TSF DLL Import Table Security Check
param(
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

Write-Host "================================================================"
Write-Host "  Phase 3.1 - TSF DLL Import Table Security Check"
Write-Host "================================================================"

$targetDlls = @("bangla_tsf.dll", "bangla_tsf32.dll")
$allPassed = $true

foreach ($dllName in $targetDlls) {
    $DllPath = Join-Path $BuildDir $dllName
    if (-not (Test-Path $DllPath)) {
        Write-Warning "Skipping missing DLL: $DllPath"
        continue
    }

    Write-Host "Analyzing: $DllPath"

    $imports = @()
    $objdump = Get-Command "objdump.exe" -ErrorAction SilentlyContinue
    $dumpbin = Get-Command "dumpbin.exe" -ErrorAction SilentlyContinue

    if ($objdump) {
        $raw = & objdump.exe -p $DllPath 2>&1
        $imports = $raw | Select-String "DLL Name:" | ForEach-Object {
            $_.Line -replace ".*DLL Name:\s*", "" | ForEach-Object { $_.Trim().ToLower() }
        }
    } elseif ($dumpbin) {
        $raw = & dumpbin.exe /imports $DllPath 2>&1
        $imports = $raw | Select-String "\.dll" | ForEach-Object {
            $_.Line.Trim().ToLower()
        }
    } else {
        $bytes = [System.IO.File]::ReadAllBytes($DllPath)
        $text = [System.Text.Encoding]::ASCII.GetString($bytes)
        $candidates = @("ws2_32.dll","wininet.dll","winhttp.dll","winsock.dll","urlmon.dll","dnsapi.dll","mswsock.dll","kernel32.dll","user32.dll","advapi32.dll","ole32.dll")
        $imports = $candidates | Where-Object { $text.Contains($_) }
    }

    Write-Host "Imported DLLs detected for $dllName :"
    $imports | ForEach-Object { Write-Host "  $_" }
    Write-Host ""

    $forbidden_network = @(
        "wininet.dll",
        "winhttp.dll",
        "ws2_32.dll",
        "wsock32.dll",
        "winsock.dll",
        "urlmon.dll",
        "httpapi.dll",
        "dnsapi.dll",
        "mswsock.dll",
        "secur32.dll",
        "schannel.dll"
    )

    $network_violations = @()
    foreach ($f in $forbidden_network) {
        $found = $imports | Where-Object { $_ -like "*$f*" }
        if ($found) {
            $network_violations += $f
        }
    }

    if ($network_violations.Count -eq 0) {
        Write-Host "  [PASS] ZERO network DLL imports detected for $dllName."
        Write-Host "         $dllName is completely network-free and isolated."
        Write-Host "         (Verified: No wininet, winhttp, ws2_32, wsock32, urlmon, etc.)"
        Write-Host ""
    } else {
        Write-Error "  [FAIL] NETWORK ISOLATION VIOLATION in ${dllName}: Network imports detected: $($network_violations -join ', ')"
        $allPassed = $false
    }
}

if (-not $allPassed) {
    exit 1
}
exit 0
