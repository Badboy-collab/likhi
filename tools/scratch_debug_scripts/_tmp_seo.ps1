$ProgressPreference = 'SilentlyContinue'
'=== LIVE /robots.txt ==='
(Invoke-WebRequest -Uri 'https://getlikhi.com/robots.txt' -TimeoutSec 30 -UseBasicParsing).Content
'=== LIVE /sitemap.xml ==='
(Invoke-WebRequest -Uri 'https://getlikhi.com/sitemap.xml' -TimeoutSec 30 -UseBasicParsing).Content
'=== LOCAL website/robots.txt ==='
Get-Content 'E:\Pervez\PC Bangla Typing App\website\robots.txt' -Raw
'=== LOCAL website/sitemap.xml (size) ==='
(Get-Item 'E:\Pervez\PC Bangla Typing App\website\sitemap.xml').Length
