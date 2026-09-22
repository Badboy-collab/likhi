$ErrorActionPreference = 'Stop'
$dest = "$env:LOCALAPPDATA\Programs\Likhi"
if (!(Test-Path $dest)) {
    New-Item -ItemType Directory -Path $dest -Force | Out-Null
}

function Safe-Copy($src, $dst) {
    if (Test-Path $dst) {
        $old = "$dst.old"
        if (Test-Path $old) {
            Remove-Item $old -Force -ErrorAction SilentlyContinue
        }
        try {
            Move-Item $dst $old -Force -ErrorAction Stop
        } catch {
            # If move fails, try direct copy
        }
    }
    Copy-Item $src -Destination $dst -Force
}

Safe-Copy "build/bangla_tsf.dll" "$dest\bangla_tsf.dll"
Safe-Copy "build/bangla_tsf32.dll" "$dest\bangla_tsf32.dll"
Safe-Copy "build/bangla_settings.exe" "$dest\bangla_settings.exe"
if (Test-Path "build/likhi_virtual_keyboard.exe") {
    Safe-Copy "build/likhi_virtual_keyboard.exe" "$dest\likhi_virtual_keyboard.exe"
}

Safe-Copy "build/bangla_tsf.dll" "release_package/bangla_tsf.dll"
Safe-Copy "build/bangla_tsf32.dll" "release_package/bangla_tsf32.dll"
Safe-Copy "build/bangla_settings.exe" "release_package/bangla_settings.exe"
if (Test-Path "build/likhi_virtual_keyboard.exe") {
    Safe-Copy "build/likhi_virtual_keyboard.exe" "release_package/likhi_virtual_keyboard.exe"
}

Write-Host "All binaries successfully deployed to $dest and release_package!"
