$ProgressPreference = 'SilentlyContinue'
foreach ($u in @(
  'https://getlikhi.com/',
  'https://getlikhi.com/feedback/',
  'https://getlikhi.com/vision-mission/',
  'https://getlikhi.com/download/',
  'https://getlikhi.com/wp-json/likhi/v1/download-stats',
  'https://github.com/Badboy-collab/likhi/raw/master/release_package/Likhi_Setup_v1.0.0.exe',
  'https://github.com/Badboy-collab/likhi/raw/master/release_package/LikhiSetup.exe'
)) {
  try {
    $r = Invoke-WebRequest -Uri $u -MaximumRedirection 5 -TimeoutSec 45 -UseBasicParsing
    '{0}  len={1}  server={2}' -f $r.StatusCode, $r.RawContentLength, $r.Headers['Server']
    '   url: ' + $u
  } catch {
    $code = $null
    if ($_.Exception.Response) { $code = [int]$_.Exception.Response.StatusCode }
    'ERR {0}  {1}' -f $code, $u
  }
}
