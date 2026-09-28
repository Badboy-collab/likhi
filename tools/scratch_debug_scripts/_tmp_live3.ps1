$ProgressPreference = 'SilentlyContinue'
$r = Invoke-WebRequest -Uri 'https://getlikhi.com/' -TimeoutSec 45 -UseBasicParsing
$c = $r.Content
'=== generator metas ==='
[regex]::Matches($c, '<meta name="generator"[^>]*>') | ForEach-Object { $_.Value }
'=== plugin paths (unique) ==='
[regex]::Matches($c, 'wp-content/plugins/([^/"]+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
'=== theme paths (unique) ==='
[regex]::Matches($c, 'wp-content/themes/([^/"]+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
'=== canonical/description/og lines ==='
[regex]::Matches($c, '<(link rel="canonical"[^>]*|meta name="description"[^>]*|meta property="og:[^"]*"[^>]*|meta name="twitter:[^"]*"[^>]*)>') | ForEach-Object { $_.Value }
'=== head scripts ==='
[regex]::Matches($c, 'wp-content/(themes|plugins)/[^"?]*\.(js|css)') | ForEach-Object { $_.Value } | Sort-Object -Unique
