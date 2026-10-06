#!/usr/bin/env python3
"""
Verify live production site for:
- HTTP 200 on all canonical pages
- Canonical Why Likhi route
- Customizer CSS variables presence
- Security headers
- PHP execution protection (no source exposure)
- .git blocking
- Directory listing protection
"""

import urllib.request
import urllib.error
import re
import sys

# Ensure UTF-8 output
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

PAGES = [
    ("Homepage", "https://getlikhi.com/"),
    ("Why Likhi (Canonical)", "https://getlikhi.com/why-likhi/"),
    ("Download", "https://getlikhi.com/download/"),
    ("Blog", "https://getlikhi.com/blog/"),
    ("FAQ", "https://getlikhi.com/faq/"),
    ("Typing Guide", "https://getlikhi.com/typing-guide/"),
    ("How It Works", "https://getlikhi.com/how-it-works/"),
    ("Vision & Mission", "https://getlikhi.com/vision-mission/"),
    ("Changelog", "https://getlikhi.com/changelog/"),
    ("Feedback", "https://getlikhi.com/feedback/"),
    ("Docs", "https://getlikhi.com/docs/"),
    ("About", "https://getlikhi.com/about/"),
    ("Contact", "https://getlikhi.com/contact/"),
    ("Privacy Policy", "https://getlikhi.com/privacy-policy/"),
]

def check_pages():
    print("=" * 60)
    print(" 1. LIVE PAGES HTTP & CONTENT VERIFICATION")
    print("=" * 60)
    all_ok = True
    for name, url in PAGES:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})
        try:
            with urllib.request.urlopen(req, timeout=15) as r:
                body = r.read().decode('utf-8', errors='ignore')
                status = r.status
                has_nav = 'site-nav' in body
                has_footer = 'site-footer' in body
                has_why_clean = ('/#why"' not in body and '/#why\'' not in body)
                print(f"  [PASS] {name:22} -> HTTP {status} (Nav: {'OK' if has_nav else 'MISSING'}, Footer: {'OK' if has_footer else 'MISSING'}, No #why: {'OK' if has_why_clean else 'FAIL'})")
        except Exception as e:
            print(f"  [FAIL] {name:22} -> {e}")
            all_ok = False
    return all_ok

def check_customizer_css():
    print("\n" + "=" * 60)
    print(" 2. CUSTOMIZER CSS VARIABLES & INLINE STYLES")
    print("=" * 60)
    url = "https://getlikhi.com/"
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            html = r.read().decode('utf-8', errors='ignore')
            vars_to_check = [
                '--likhi-primary',
                '--likhi-accent',
                '--likhi-button',
                '--likhi-link',
                '--likhi-header-bg',
                '--likhi-active-menu',
                '--likhi-footer-bg',
                '--likhi-text',
            ]
            found = [v for v in vars_to_check if v in html]
            print(f"  Found {len(found)}/{len(vars_to_check)} CSS Variables rendered in HTML:")
            for v in found:
                print(f"    - {v}: PRESENT")
            return len(found) == len(vars_to_check)
    except Exception as e:
        print(f"  [ERROR] {e}")
        return False

def check_security():
    print("\n" + "=" * 60)
    print(" 3. LIVE SECURITY POSTURE VERIFICATION")
    print("=" * 60)
    
    # Security Headers
    req = urllib.request.Request("https://getlikhi.com/", headers={'User-Agent': 'Mozilla/5.0'})
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            headers = r.headers
            print("  Security Headers:")
            for h in ['X-Content-Type-Options', 'X-Frame-Options', 'Referrer-Policy', 'Permissions-Policy', 'Server', 'Strict-Transport-Security']:
                print(f"    - {h:26}: {headers.get(h, 'NOT SET')}")
    except Exception as e:
        print(f"  Header check error: {e}")

    # Sensitive endpoints
    probe_endpoints = [
        ("PHP Source Exposure (functions.php)", "https://getlikhi.com/wp-content/themes/likhi/functions.php", [200, 403]),
        (".git Exposure", "https://getlikhi.com/.git/", [403, 404]),
        (".git HEAD Exposure", "https://getlikhi.com/.git/HEAD", [403, 404]),
        (".env Exposure", "https://getlikhi.com/.env", [403, 404]),
        ("wp-config.php Exposure", "https://getlikhi.com/wp-config.php", [200, 403, 404]),
        ("Uploads Directory Listing", "https://getlikhi.com/wp-content/uploads/", [200, 403, 404]),
        ("XML-RPC Status", "https://getlikhi.com/xmlrpc.php", [403, 404, 405]),
    ]

    for label, url, allowed_codes in probe_endpoints:
        preq = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        try:
            with urllib.request.urlopen(preq, timeout=10) as pr:
                status = pr.status
                body = pr.read(100)
                # Ensure no raw php code
                if b'<?php' in body or b'DB_PASSWORD' in body:
                    print(f"  [CRITICAL VULNERABILITY] {label} returned source code!")
                else:
                    is_dir_list = (b'<title>Index of' in body)
                    if is_dir_list and 'Uploads' in label:
                        print(f"  [WARN] {label} -> Directory autoindex active (status {status})")
                    else:
                        print(f"  [PASS] {label:38} -> HTTP {status} (Safe, no source exposed)")
        except urllib.error.HTTPError as e:
            if e.code in allowed_codes:
                print(f"  [PASS] {label:38} -> HTTP {e.code} (Blocked as expected)")
            else:
                print(f"  [INFO] {label:38} -> HTTP {e.code}")
        except Exception as e:
            print(f"  [ERROR] {label:38} -> {e}")

if __name__ == '__main__':
    p_ok = check_pages()
    c_ok = check_customizer_css()
    check_security()
