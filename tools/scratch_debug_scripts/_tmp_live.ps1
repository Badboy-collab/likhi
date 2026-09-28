$ProgressPreference = 'SilentlyContinue'
$tmp = Join-Path $env:TEMP 'likhi_live_home.html'
try {
  Invoke-WebRequest -Uri 'https://getlikhi.com/' -TimeoutSec 45 -UseBasicParsing -OutFile $tmp
  'downloaded: ' + (Get-Item $tmp).Length
  $c = Get-Content $tmp -Raw
  foreach ($m in @('wp-content', 'wp-includes', 'wp-json', 'generator', 'blogger', 'Blogger', 'blogspot', 'static', 'preload')) {
    $n = ([regex]::Matches($c, [regex]::Escape($m))).Count
    'marker {0,-12} : {1}' -f $m, $n
  }
  '--- title ---'
  if ($c -match '(?s)<title>(.*?)</title>') { $matches[1] }
  '--- head (first 1200 chars) ---'
  $c.Substring(0, [Math]::Min(1200, $c.Length))
} catch { 'ERR ' + $_.Exception.Message }

'=== installer hash check ==='
$exe = Join-Path $env:TEMP 'Likhi_Setup_v1.0.0.exe'
try {
  Invoke-WebRequest -Uri 'https://github.com/Badboy-collab/likhi/raw/master/release_package/Likhi_Setup_v1.0.0.exe' -TimeoutSec 120 -UseBasicParsing -OutFile $exe
  'size   : ' + (Get-Item $exe).Length
  'sha256 : ' + (Get-FileHash $exe -Algorithm SHA256).Hash
  'claimed: 0610c80138adb89475ccfc68c052b7d171c34f8e8b9410642cbb266d75fbd208'
} catch { 'ERR ' + $_.Exception.Message }
