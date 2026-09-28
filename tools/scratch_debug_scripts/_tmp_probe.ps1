$here = 'E:\Pervez\PC Bangla Typing App'
'=== lexicon.bin header ==='
$lex = Join-Path $here 'release_package\lexicon.bin'
if (Test-Path $lex) {
  $fs = [System.IO.File]::OpenRead($lex)
  $buf = New-Object byte[] 32
  [void]$fs.Read($buf, 0, 32)
  $fs.Close()
  'magic : ' + (($buf[0..3] | ForEach-Object { [char]$_ }) -join '')
  'ver   : ' + [BitConverter]::ToUInt16($buf, 4)
  'count : ' + [BitConverter]::ToUInt32($buf, 8)
  'size  : ' + (Get-Item $lex).Length
} else { 'MISSING' }

'=== other lexicon copies ==='
foreach ($p in @('engine\data\lexicon.bin','release_package\data\lexicon.bin')) {
  $f = Join-Path $here $p
  if (Test-Path $f) { 'FOUND ' + $p + ' ' + (Get-Item $f).Length } else { 'none  ' + $p }
}

'=== installer sha256 ==='
foreach ($n in @('release_package\LikhiSetup.exe','release_package\Likhi-Test-Windows-x64.zip')) {
  $f = Join-Path $here $n
  if (Test-Path $f) { (Get-FileHash $f -Algorithm SHA256).Hash + '  ' + $n + '  ' + (Get-Item $f).Length }
}
