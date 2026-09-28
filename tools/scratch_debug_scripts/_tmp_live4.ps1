$ProgressPreference = 'SilentlyContinue'
$r = Invoke-WebRequest -Uri 'https://getlikhi.com/' -TimeoutSec 45 -UseBasicParsing
$c = $r.Content
foreach ($m in @('Yoast','Rank Math','rank-math','All in One SEO','aioseo','seopress','SEOPress','This site is optimized','wpseo','schema','@context')) {
  '  {0,-22} : {1}' -f $m, ([regex]::Matches($c, [regex]::Escape($m))).Count
}
'=== comments with "SEO" or "optimized" ==='
[regex]::Matches($c, '<!--[^>]{0,200}-->') | ForEach-Object { $_.Value } | Where-Object { $_ -match 'SEO|optim|Yoast|Rank' } | Select-Object -First 10
'=== all wp-content/plugins occurrences (raw) ==='
[regex]::Matches($c, 'wp-content/plugins/[A-Za-z0-9_\-]+') | ForEach-Object { $_.Value } | Sort-Object -Unique
'=== body classes ==='
if ($c -match '<body[^>]*class="([^"]*)"') { $matches[1] }
