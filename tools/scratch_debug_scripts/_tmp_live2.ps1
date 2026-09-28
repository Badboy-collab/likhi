$ProgressPreference = 'SilentlyContinue'
function Markers($url, $names) {
  try {
    $r = Invoke-WebRequest -Uri $url -TimeoutSec 45 -UseBasicParsing
    '=== {0} -> {1} ({2} bytes) ===' -f $url, $r.StatusCode, $r.RawContentLength
    $c = $r.Content
    foreach ($m in $names) {
      $n = ([regex]::Matches($c, [regex]::Escape($m))).Count
      '  {0,-28} : {1}' -f $m, $n
    }
  } catch { 'ERR {0} {1}' -f ([int]$_.Exception.Response.StatusCode), $url }
}
Markers 'https://getlikhi.com/' @('id="why"', 'skip-link', 'nav-links', 'nav-toggle', 'download-counter-badge', 'demo.js', 'main.js', 'Likhi SEO &amp; Metadata', 'no-js', 'wp-block')
Markers 'https://getlikhi.com/download/' @('btn-final-download', 'download-counter-badge', 'Likhi_Setup_v1.0.0.exe', 'Download Likhi', 'rest_url', 'track-download')
Markers 'https://getlikhi.com/docs/' @('.html', 'page-docs', 'Getting Started')
Markers 'https://getlikhi.com/typing-guide/' @('typing-filter-btn', 'phonetic-pair', 'data-category')
Markers 'https://getlikhi.com/how-it-works/' @('.html', 'How Likhi Works')
Markers 'https://getlikhi.com/faq/' @('faq-item', 'faq-question')
Markers 'https://getlikhi.com/privacy-policy/' @('Privacy')
