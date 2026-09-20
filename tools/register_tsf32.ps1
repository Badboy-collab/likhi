# tools/register_tsf32.ps1
$ErrorActionPreference = "Continue"

$dll32 = (Resolve-Path (Join-Path $PSScriptRoot "..\release_package\bangla_tsf32.dll")).Path
Write-Host "Registering 32-bit DLL via SysWOW64 regsvr32: $dll32"

$proc = Start-Process -FilePath "C:\Windows\SysWOW64\regsvr32.exe" -ArgumentList "/s `"$dll32`"" -Wait -PassThru
Write-Host "regsvr32 exit code: $($proc.ExitCode)"

# Also let's inspect the registry paths for WOW64
$clsid = "{B4F1470A-7C69-4C62-972F-6379532856E1}"
Write-Host "Checking HKCU WOW6432Node..."
Get-ItemProperty "HKCU:\Software\Classes\WOW6432Node\CLSID\$clsid\InprocServer32" -ErrorAction SilentlyContinue
Get-ItemProperty "HKCU:\Software\Classes\CLSID\$clsid\InprocServer32" -ErrorAction SilentlyContinue
Get-ItemProperty "HKLM:\Software\WOW6432Node\Classes\CLSID\$clsid\InprocServer32" -ErrorAction SilentlyContinue
