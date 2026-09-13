# ===========================================================================
# Likhi — publish a test build to GitHub (device flow, no secrets stored)
#
#   powershell -ExecutionPolicy Bypass -File tools\publish_release.ps1
#
# 1. asks GitHub for a device code and prints it,
# 2. waits until you authorise it at https://github.com/login/device,
# 3. pushes the current branch, then creates/updates the release and uploads
#    the installer asset.
#
# The token lives only in this process's memory — nothing is written to disk
# and the git remote stays a plain https URL.
# ===========================================================================
[CmdletBinding()]
param(
    [string]$Repo    = 'Badboy-collab/likhi',
    [string]$Tag     = 'v1.0.0-test2',
    [string]$Asset   = 'release_package\LikhiSetup.exe',
    [string]$Name    = 'Likhi v1.0.0 test 2 - one-click installer',
    [string]$Notes   = 'One-click installer: double-click LikhiSetup.exe, accept the UAC prompt, sign out/in once. Win+Space then shows only your normal keyboard and "Likhi (লিখি)".',
    [string]$Branch  = 'master'
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$ClientId = '01ab8ac9400c4e429b23'   # GitHub CLI's public OAuth app (device flow)
$Api = 'https://api.github.com'

Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    # --- 1. device code -----------------------------------------------------
    $dc = Invoke-RestMethod -Method Post -Uri 'https://github.com/login/device/code' `
              -Headers @{ Accept = 'application/json' } `
              -Body @{ client_id = $ClientId; scope = 'repo' }
    Write-Host ""
    Write-Host "  1) open  : $($dc.verification_uri)" -ForegroundColor Cyan
    Write-Host "  2) enter : $($dc.user_code)" -ForegroundColor Yellow
    Write-Host "  (waiting up to $([int]($dc.expires_in/60)) min)" -ForegroundColor DarkGray
    Write-Host ""

    # --- 2. poll for the token ---------------------------------------------
    $token = $null
    for ($i = 0; $i -lt [int]($dc.expires_in / 5); $i++) {
        try {
            $r = Invoke-RestMethod -Method Post -Uri 'https://github.com/login/oauth/access_token' `
                     -Headers @{ Accept = 'application/json'; 'User-Agent' = 'likhi-publish' } `
                     -Body @{ client_id = $ClientId; device_code = $dc.device_code
                              grant_type = 'urn:ietf:params:oauth:grant-type:device_code' }
            if ($r.access_token) { $token = $r.access_token; break }
            if ($r.error -ne 'authorization_pending' -and $r.error -ne 'slow_down') {
                throw "device flow: $($r.error) - $($r.error_description)"
            }
        } catch { if ($_.Exception.Message -like 'device flow:*') { throw } }
        Start-Sleep -Seconds 5
    }
    if (-not $token) { throw 'timed out waiting for authorisation' }
    Write-Host "  authorised." -ForegroundColor Green

    $authHeader = @{ Authorization = "token $token"; 'User-Agent' = 'likhi-publish' }
    $gitB64 = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("x-access-token:$token"))

    # --- 3. push ------------------------------------------------------------
    Write-Host "  pushing $Branch ..." -ForegroundColor Cyan
    git -c credential.helper= -c http.extraheader="AUTHORIZATION: basic $gitB64" push origin $Branch
    if ($LASTEXITCODE -ne 0) { throw "git push failed ($LASTEXITCODE)" }

    # --- 4. release + asset -------------------------------------------------
    $release = $null
    try {
        $release = Invoke-RestMethod -Uri "$Api/repos/$Repo/releases/tags/$Tag" -Headers $authHeader
    } catch {
        $release = Invoke-RestMethod -Method Post -Uri "$Api/repos/$Repo/releases" -Headers $authHeader `
                   -ContentType 'application/json' -Body (@{
                        tag_name   = $Tag
                        target_commitish = $Branch
                        name       = $Name
                        body       = $Notes
                        prerelease = $true
                   } | ConvertTo-Json -Depth 3)
    }

    $assetName = Split-Path -Leaf $Asset
    $existing = @($release.assets | Where-Object { $_.name -eq $assetName })
    foreach ($a in $existing) {
        Invoke-RestMethod -Method Delete -Uri "$Api/repos/$Repo/releases/assets/$($a.id)" -Headers $authHeader | Out-Null
    }
    Write-Host "  uploading $assetName ..." -ForegroundColor Cyan
    $uploaded = Invoke-RestMethod -Method Post `
        -Uri "https://uploads.github.com/repos/$Repo/releases/$($release.id)/assets?name=$assetName" `
        -Headers $authHeader -ContentType 'application/octet-stream' -InFile $Asset

    Write-Host ""
    Write-Host "  done: $($uploaded.browser_download_url)" -ForegroundColor Green
    Write-Host "  release page: $($release.html_url)" -ForegroundColor Green
} finally {
    Pop-Location
    $token = $null
}
