$dir = 'E:\Pervez\PC Bangla Typing App\website\wordpress-theme\likhi'
$style = Get-Content (Join-Path $dir 'style.css') -Raw
foreach ($f in @('main.css','components.css','pages.css')) {
  $p = Join-Path $dir ('assets\css\' + $f)
  $c = Get-Content $p -Raw
  $sels = [regex]::Matches($c, '(?m)^([.#][A-Za-z0-9_\-\[\]=:"''\s>\.:#,]+?)\s*\{') | ForEach-Object { $_.Groups[1].Value.Trim() }
  $sels = $sels | Sort-Object -Unique
  $missing = @()
  foreach ($s in $sels) {
    $first = ($s -split ',')[0].Trim()
    if ($first -and $style -notmatch [regex]::Escape($first)) { $missing += $first }
  }
  '{0}: selectors={1} missing-in-style.css={2}' -f $f, $sels.Count, $missing.Count
  if ($missing.Count) { $missing | Select-Object -First 25 | ForEach-Object { '   MISS ' + $_ } }
}
'=== live-metadata markers in backup vs current functions.php ==='
foreach ($t in @('_theme_backup_20260924_121238','website\wordpress-theme\likhi')) {
  $p = Join-Path 'E:\Pervez\PC Bangla Typing App' ($t + '\functions.php')
  if (Test-Path $p) {
    $x = Get-Content $p -Raw
    '{0}: nositelinkssearchbox={1} twitter:locale={2} og:image={3} Likhi SEO={4}' -f $t,
      ([regex]::Matches($x,'nositelinkssearchbox')).Count,
      ([regex]::Matches($x,'twitter:locale')).Count,
      ([regex]::Matches($x,'og:image')).Count,
      ([regex]::Matches($x,'Likhi SEO')).Count
  }
}
