$root = 'E:\Pervez\PC Bangla Typing App'
$cut = (Get-Date).Date
Get-ChildItem -Path $root -Recurse -File -ErrorAction SilentlyContinue |
  Where-Object { $_.LastWriteTime -gt $cut } |
  Where-Object { $_.FullName -notmatch '\\build\\|\\\.git\\|_backup|website_backup|\\dist\\' } |
  Sort-Object LastWriteTime |
  ForEach-Object { '{0}  {1}' -f $_.LastWriteTime.ToString('MM-dd HH:mm:ss'), $_.FullName }
