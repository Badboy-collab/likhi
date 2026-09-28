$here = 'E:\Pervez\PC Bangla Typing App'
function Scan($path, $needles) {
  if (-not (Test-Path $path)) { "MISSING $path"; return }
  $bytes = [System.IO.File]::ReadAllBytes($path)
  $txt = [System.Text.Encoding]::ASCII.GetString($bytes)
  '--- {0} ({1} bytes) ---' -f (Split-Path $path -Leaf), $bytes.Length
  foreach ($n in $needles) {
    $c = ([regex]::Matches($txt, [regex]::Escape($n))).Count
    '   {0,-22} : {1}' -f $n, $c
  }
}
Scan (Join-Path $here 'release_package\bangla_tsf.dll') @('WINHTTP.dll','WinHttpOpen','winhttp','inputtools.google.com','cloud_translit','LIKHI_CLOUD_TRANSLIT_OFF')
$exe = Join-Path $env:TEMP 'Likhi_Setup_v1.0.0.exe'
Scan $exe @('WINHTTP.dll','WinHttpOpen','inputtools','bangla_tsf.dll','lexicon.bin','Inno Setup','Likhi')
