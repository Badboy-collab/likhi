import os, re, sys, collections

ROOT = r'E:\Pervez\PC Bangla Typing App\website\wordpress-theme\likhi'

# slug -> template file (from functions.php routing map + page files)
route_map = {
    'download': 'page-download.php', 'how-it-works': 'page-how-it-works.php',
    'typing-guide': 'page-typing-guide.php', 'faq': 'page-faq.php',
    'changelog': 'page-changelog.php', 'about': 'page-about.php', 'about-us': 'page-about.php',
    'vision-mission': 'page-vision-mission.php', 'feedback': 'page-feedback.php',
    'contact': 'page-contact.php', 'contact-us': 'page-contact.php',
    'privacy-policy': 'page-privacy-policy.php', 'privacy': 'page-privacy-policy.php',
    'terms': 'page-terms.php', 'terms-conditions': 'page-terms.php',
    'disclaimer': 'page-disclaimer.php', 'cookie-policy': 'page-cookie-policy.php',
    'dmca': 'page-dmca.php', 'copyright': 'page-dmca.php', 'sitemap': 'page-sitemap.php',
    'docs': 'page-docs.php',
}

php_files = sorted(f for f in os.listdir(ROOT) if f.endswith('.php'))

ids = collections.defaultdict(set)
for f in php_files:
    txt = open(os.path.join(ROOT, f), encoding='utf-8', errors='replace').read()
    ids[f] = set(re.findall(r'id="([A-Za-z0-9_\-]+)"', txt))

problems = []
all_home_url_paths = collections.Counter()

for f in php_files:
    txt = open(os.path.join(ROOT, f), encoding='utf-8', errors='replace').read()
    # home_url('...') style links
    for m in re.finditer(r"home_url\('([^']*)'\)", txt):
        raw = m.group(1)
        all_home_url_paths[raw] += 1
    # bare anchors
    for m in re.finditer(r'href="(#[^"]+)"', txt):
        anchor = m.group(1)[1:]
        if anchor and anchor not in ids[f]:
            problems.append((f, 'BARE-ANCHOR missing target: ' + m.group(1)))
    # relative .html or hard links
    for m in re.finditer(r'href="([^"?#]*\.html[^"]*)"', txt):
        problems.append((f, 'HTML-LINK: ' + m.group(1)))
    # https://getlikhi.com hardcoded
    for m in re.finditer(r'(https?://getlikhi\.com[^"\'<> ]*)', txt):
        problems.append((f, 'HARDCODED-DOMAIN: ' + m.group(1)))

print('=== home_url() targets used ===')
for p, n in sorted(all_home_url_paths.items()):
    path = p.split('#')[0].strip('/')
    anchor = p.split('#')[1] if '#' in p else None
    note = ''
    if path == '':
        note = 'front page'
    elif path in route_map:
        tpl = route_map[path]
        if anchor and anchor not in ids.get(tpl, set()):
            note = '!! ANCHOR #{0} NOT FOUND in {1}'.format(anchor, tpl)
            problems.append(('<route>', '{0} -> #{1} missing in {2}'.format(p, anchor, tpl)))
        elif anchor:
            note = 'anchor ok in ' + tpl
        else:
            note = '-> ' + tpl
    else:
        note = '!! UNKNOWN SLUG (not in routing map)'
        problems.append(('<route>', 'unknown slug: ' + p))
    print('  {0:28} x{1:<3} {2}'.format(p, n, note))

print()
print('=== problems ===')
if not problems:
    print('  none')
for f, p in problems:
    print('  [{0}] {1}'.format(f, p))
print()
print('=== files with no main id ===')
for f in php_files:
    if 'main' not in ids[f] and f not in ('functions.php', 'header.php', 'footer.php'):
        print('  ' + f)
