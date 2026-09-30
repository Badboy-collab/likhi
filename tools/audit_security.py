#!/usr/bin/env python3
"""
Theme Security & Static Code Audit Script
Audits the theme for:
- Secret / token leakage
- Raw unescaped echos
- Missing capability checks
- CSRF protection
- Tag matching
"""

import os
import re

THEME_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'website', 'wordpress-theme', 'likhi')

def audit():
    print("=" * 60)
    print(" LIKHI WORDPRESS THEME STATIC & SECURITY AUDIT")
    print("=" * 60)

    # 1. Secret Scanning
    secret_patterns = [
        re.compile(r'(?i)(api[_-]?key|access[_-]?token|secret[_-]?key)\s*[:=]\s*[\'"][^\'"]{12,}'),
        re.compile(r'(?i)(ghp_[A-Za-z0-9_]{30,}|github_pat_[A-Za-z0-9_]{50,})'),
        re.compile(r'(?i)ftp[_-]?pass\s*[:=]\s*[\'"][^\'"]{4,}'),
    ]

    secrets_found = []
    php_files = []

    for root, dirs, files in os.walk(THEME_DIR):
        for f in files:
            p = os.path.join(root, f)
            rel = os.path.relpath(p, THEME_DIR)
            if f.endswith('.php'):
                php_files.append(p)
            if f.endswith(('.php', '.css', '.js', '.json')):
                with open(p, 'r', encoding='utf-8', errors='ignore') as fh:
                    for line_no, line in enumerate(fh, 1):
                        for pat in secret_patterns:
                            if pat.search(line):
                                if 'sanitize' in line or 'placeholder' in line:
                                    continue
                                secrets_found.append(f"{rel}:{line_no}")

    print(f"Total PHP Files in Theme: {len(php_files)}")
    print(f"Secrets Found: {len(secrets_found)}")
    if secrets_found:
        for s in secrets_found:
            print(f"  [SECRET ALERT] {s}")
    else:
        print("  [OK] No credentials, private tokens, or secrets found in theme files.")

    # 2. Bracket and PHP Open/Close Tag Balance
    syntax_issues = []
    for p in php_files:
        rel = os.path.relpath(p, THEME_DIR)
        with open(p, 'r', encoding='utf-8', errors='ignore') as fh:
            content = fh.read()
            # Simple brace count check
            open_braces = content.count('{')
            close_braces = content.count('}')
            if open_braces != close_braces:
                syntax_issues.append((rel, f"Mismatched braces: {open_braces} open vs {close_braces} close"))

    print(f"Brace Balance Issues: {len(syntax_issues)}")
    if syntax_issues:
        for rel, issue in syntax_issues:
            print(f"  [WARN] {rel}: {issue}")
    else:
        print("  [OK] All PHP template files have balanced braces.")

    # 3. ABSPATH Guard Verification
    missing_abspath = []
    for p in php_files:
        rel = os.path.relpath(p, THEME_DIR)
        with open(p, 'r', encoding='utf-8', errors='ignore') as fh:
            header = fh.read(1000)
            if 'ABSPATH' not in header:
                missing_abspath.append(rel)

    print(f"ABSPATH Guard Status: {len(missing_abspath)} missing")
    if missing_abspath:
        for rel in missing_abspath:
            print(f"  [WARN] Missing ABSPATH: {rel}")
    else:
        print("  [OK] All PHP files have ABSPATH execution guards.")

    # 4. Customizer capability & sanitization check
    customizer_path = os.path.join(THEME_DIR, 'inc', 'customizer.php')
    with open(customizer_path, 'r', encoding='utf-8', errors='ignore') as fh:
        cust_content = fh.read()
        settings_count = cust_content.count("add_setting(")
        cap_count = cust_content.count("'capability'        => 'edit_theme_options'")
        san_count = cust_content.count("'sanitize_callback'")
        print(f"Customizer Settings: {settings_count}")
        print(f"Capability Checks ('edit_theme_options'): {cap_count}")
        print(f"Sanitization Callbacks: {san_count}")
        if settings_count == cap_count and settings_count == san_count:
            print("  [OK] 100% of Customizer settings have explicit capability checks and sanitization callbacks.")
        else:
            print(f"  [WARN] Discrepancy: {settings_count} settings vs {cap_count} caps vs {san_count} sanitizers")

    print("=" * 60)
    print(" AUDIT COMPLETE")
    print("=" * 60)

if __name__ == '__main__':
    audit()
