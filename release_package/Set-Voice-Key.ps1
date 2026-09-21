param([string]$Key)

# Likhi voice typing - API key helper.
# Runs from Set-Voice-Key.cmd (double click). Writes
# %APPDATA%\PC-Bangla-Typing-App\voice.json in a valid state, so the file can
# never be broken by hand-editing.
$ErrorActionPreference = 'Stop'

$cfgDir = Join-Path $env:APPDATA 'PC-Bangla-Typing-App'
$cfg = Join-Path $cfgDir 'voice.json'
New-Item -ItemType Directory -Force -Path $cfgDir | Out-Null

Write-Host ''
Write-Host '=== Likhi voice typing - API key ===' -ForegroundColor Cyan
Write-Host 'Get a FREE key from Google AI Studio:'
Write-Host '    https://aistudio.google.com/apikey'
Write-Host '    (open it -> "Create API key" -> copy the string that starts with AIza)'
Write-Host ''

if (-not $Key) {
    $Key = Read-Host 'Paste the KEY and press Enter (just Enter = keep voice off)'
}

# Clean up the usual paste accidents: spaces, quotes, newlines.
$Key = ($Key -replace '\s', '').Trim('"', "'")

if ([string]::IsNullOrEmpty($Key)) {
    $json = @{
        _help    = 'api_key: put the provider key here (Google AI Studio AIza... or OpenAI sk-...). Empty = voice typing off.'
        provider = 'gemini'
        api_key  = ''
        model    = 'gemini-3.6-flash'
        language = 'bn-BD'
    } | ConvertTo-Json
    [IO.File]::WriteAllText($cfg, $json, (New-Object Text.UTF8Encoding($false)))
    Write-Host 'No key entered - voice typing stays OFF (suggestions still work).' -ForegroundColor Yellow
    exit 0
}

if ($Key -like 'http*') {
    Write-Host 'That is a WEB LINK, not a key.' -ForegroundColor Red
    Write-Host 'Open the link in a browser, click "Create API key", then paste THAT string.'
    exit 1
}

$provider = 'gemini'
$model = 'gemini-3.6-flash'
if ($Key -like 'sk-*') {
    $provider = 'openai'
    $model = 'whisper-1'
}

$json = @{
    _help    = 'api_key: put the provider key here (Google AI Studio AIza... or OpenAI sk-...). Empty = voice typing off.'
    provider = $provider
    api_key  = $Key
    model    = $model
    language = 'bn-BD'
} | ConvertTo-Json

[IO.File]::WriteAllText($cfg, $json, (New-Object Text.UTF8Encoding($false)))

Write-Host ''
Write-Host ("Saved: provider={0}  model={1}  key length={2}" -f $provider, $model, $Key.Length) -ForegroundColor Green
Write-Host ("File: {0}" -f $cfg)
Write-Host ''
Write-Host 'Next: sign out and sign in once, then in any app press Ctrl+Alt+V to talk.' -ForegroundColor Green
