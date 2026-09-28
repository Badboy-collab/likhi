$ProgressPreference = 'SilentlyContinue'
foreach ($u in @('https://getlikhi.com/sitemap.xml','https://getlikhi.com/wp-sitemap.xml','https://getlikhi.com/robots.txt','https://getlikhi.com/about/','https://getlikhi.com/contact/','https://getlikhi.com/privacy-policy/','https://getlikhi.com/terms/','https://getlikhi.com/disclaimer/','https://getlikhi.com/cookie-policy/','https://getlikhi.com/dmca/','https://getlikhi.com/sitemap/','https://getlikhi.com/about-us/')) {
  try {
    $r = Invoke-WebRequest -Uri $u -TimeoutSec 30 -UseBasicParsing
    '{0,-6} len={1,-8} {2}' -f $r.StatusCode, $r.RawContentLength, $u
  } catch {
    $code = 'ERR'
    if ($_.Exception.Response) { $code = [int]$_.Exception.Response.StatusCode }
    '{0,-6} {1}' -f $code, $u
  }
}
