param([string]$Backup, [string]$Current)
$ErrorActionPreference = 'Stop'
if (-not $Backup) { $Backup = 'E:\Pervez\PC Bangla Typing App\_theme_backup_20260924_121238' }
if (-not $Current) { $Current = 'E:\Pervez\PC Bangla Typing App\website\wordpress-theme\likhi' }
$ha = @{}
Get-ChildItem -Recurse -File -LiteralPath $Backup | ForEach-Object {
  $rel = $_.FullName.Substring($Backup.Length).TrimStart('\')
  $ha[$rel] = (Get-FileHash -LiteralPath $_.FullName -Algorithm MD5).Hash
}
Get-ChildItem -Recurse -File -LiteralPath $Current | ForEach-Object {
  $rel = $_.FullName.Substring($Current.Length).TrimStart('\')
  $h = (Get-FileHash -LiteralPath $_.FullName -Algorithm MD5).Hash
  if ($ha.ContainsKey($rel)) {
    if ($ha[$rel] -ne $h) { Write-Output ("MODIFIED " + $rel) }
    $ha.Remove($rel)
  } else {
    Write-Output ("ADDED    " + $rel)
  }
}
foreach ($k in $ha.Keys) { Write-Output ("DELETED  " + $k) }
