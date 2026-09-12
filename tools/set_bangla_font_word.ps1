# ============================================================
# Likhi — Bangla font as the default for MS Word (and Office)
#
# WHY: Word types complex-script (Bengali) text with the theme's
# "complex script" font of the current style. Fresh documents use the
# Office theme fonts, so Bangla lines fall back to whatever font
# Windows font-linking picks — that is why the text "changes font"
# when you type Bangla in Office. A TSF IME cannot force an app font;
# this script changes Word's OWN default instead:
#
#   1. Verifies the chosen Unicode Bangla font is installed
#      (Kalpurush is already present on this PC; Siyam Rupali is not
#      installed by default but can be added with -FontName).
#   2. Backs up your Word template (Normal.dotm).
#   3. Patches the template theme so the complex-script default of
#      both major (headings) and minor (body) fonts = Kalpurush.
#      All NEW documents you create will render Bengali in Kalpurush.
#   4. Restore any time:  -Restore
#
# NOTE: The legacy "ANSI" variants (Siyam Rupali ANSI / Kalpurush ANSI)
# are OLD 8-bit-encoded fonts — they do NOT render modern Unicode
# Bangla correctly and must never be used for typing. We set the
# Unicode "Kalpurush" (no suffix).
#
# Usage (Word must be CLOSED):
#   powershell -ExecutionPolicy Bypass -File tools\set_bangla_font_word.ps1
#   powershell -ExecutionPolicy Bypass -File tools\set_bangla_font_word.ps1 -Restore
# ============================================================
param(
    [string]$FontName = "Kalpurush",
    [switch]$Restore
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$fontsDir = Join-Path $env:WINDIR "Fonts"
$templates = Join-Path $env:APPDATA "Microsoft\Templates"
$dotm = Join-Path $templates "Normal.dotm"

function Get-InstalledFontExact {
    param([string]$Name)
    # Registry font values (HKLM machine + HKCU per-user)
    $roots = @(
        "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts",
        "HKCU:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts"
    )
    foreach ($r in $roots) {
        if (Test-Path $r) {
            $props = Get-ItemProperty $r
            foreach ($p in $props.PSObject.Properties) {
                if ($p.Name -like "$Name*" -and $p.Name -notlike "*ANSI*") { return $true }
            }
        }
    }
    # Direct file fallback
    if (Get-ChildItem $fontsDir -Filter *.tt? | Where-Object { $_.BaseName -eq $Name -or $_.BaseName -like "$Name*" -and $_.BaseName -notlike "*ANSI*" }) {
        return $true
    }
    return $false
}

if ($Restore) {
    $bak = Get-ChildItem "$dotm.bak-*" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $bak) { Write-Host "No backup found to restore." -ForegroundColor Yellow; exit 1 }
    if (Get-Process winword -ErrorAction SilentlyContinue) { Write-Host "Please CLOSE Microsoft Word first." -ForegroundColor Red; exit 1 }
    Copy-Item $bak.FullName $dotm -Force
    Write-Host "Restored Normal.dotm from $($bak.Name)" -ForegroundColor Green
    exit 0
}

Write-Host "== Likhi: default Bangla font for Word =="
Write-Host "Checking font: $FontName (Unicode)..."
if (-not (Get-InstalledFontExact $FontName)) {
    Write-Host "FONT NOT FOUND: '$FontName'. Install it first (e.g. run the official"
    Write-Host "font installer), then re-run this script." -ForegroundColor Red
    exit 1
}
Write-Host "Font OK. (Skipping legacy *ANSI* variants on purpose.)"

if (-not (Test-Path $dotm)) {
    Write-Host "Normal.dotm not found yet. Open Microsoft Word once (create a blank"
    Write-Host "document and close it), then re-run this script." -ForegroundColor Yellow
    exit 1
}

if (Get-Process winword -ErrorAction SilentlyContinue) {
    Write-Host "Please CLOSE Microsoft Word first, then re-run." -ForegroundColor Red
    exit 1
}

# Backup
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$bak = "$dotm.bak-$stamp"
Copy-Item $dotm $bak -Force
Write-Host "Backup created: $bak"

# Patch theme1.xml inside the dotm (an OPC zip)
$tmp = Join-Path $env:TEMP "normal_dotm_patch"
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
[System.IO.Compression.ZipFile]::ExtractToDirectory($dotm, $tmp)
$theme = Join-Path $tmp "word\theme\theme1.xml"
if (-not (Test-Path $theme)) {
    Write-Host "Theme entry not found in Normal.dotm — template structure differs;"
    Write-Host "backup kept. Nothing changed." -ForegroundColor Yellow
    Remove-Item $tmp -Recurse -Force
    exit 1
}

$xml = Get-Content $theme -Raw -Encoding UTF8
$changed = $false
foreach ($font in @('a:majorFont', 'a:minorFont')) {
    $open = "<$font>"
    $openIdx = $xml.IndexOf($open)
    if ($openIdx -lt 0) { continue }
    # only insert into the font element that has latin/ea/cs children
    $closeTag = "</$font>"
    $closeIdx = $xml.IndexOf($closeTag, $openIdx)
    if ($closeIdx -lt 0) { continue }
    $segment = $xml.Substring($openIdx, $closeIdx - $openIdx + $closeTag.Length)
    if ($segment -match '<a:cs\s+typeface="[^"]*"') {
        # already has a cs typeface — replace it
        $newSeg = [regex]::Replace($segment, '<a:cs\s+typeface="[^"]*"', "<a:cs typeface=`"$FontName`"")
        if ($newSeg -ne $segment) { $xml = $xml.Replace($segment, $newSeg); $changed = $true }
    } elseif ($segment -match '<a:ea\s+typeface="[^"]*"') {
        # insert <a:cs .../> right after <a:ea .../> so ordering stays schema-valid
        $newSeg = [regex]::Replace($segment, '(<a:ea\s+typeface="[^"]*"\s*/>)', "`$1<a:cs typeface=`"$FontName`"/>", 1)
        if ($newSeg -ne $segment) { $xml = $xml.Replace($segment, $newSeg); $changed = $true }
    }
}

if (-not $changed) {
    Write-Host "Theme already had no patchable font slots — check $bak and the theme XML."
    Write-Host "Backup kept; nothing changed." -ForegroundColor Yellow
} else {
    # rewrite zip: easiest reliable path is re-zip into place
    $content = [System.Text.Encoding]::UTF8.GetBytes($xml)
    [System.IO.File]::WriteAllBytes($theme, $content)

    if (Test-Path $dotm) { Remove-Item $dotm -Force }
    [System.IO.Compression.ZipFile]::CreateFromDirectory($tmp, $dotm)
    Write-Host "Normal.dotm patched: Bengali in Word now defaults to '$FontName'." -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:"
    Write-Host "  1. Open Word -> File -> Options -> Save and tick 'Embed fonts in the file'"
    Write-Host "     (optional, for sharing files that keep the font)."
    Write-Host "  2. Existing documents keep their old theme — apply the font once"
    Write-Host "     (Home > Font > $FontName) or re-type in a new document."
    Write-Host "To undo: run this script again with -Restore"
}
Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
