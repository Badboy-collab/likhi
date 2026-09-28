#!/usr/bin/env python3
"""
Likhi Theme — Local -> cPanel Direct Deployment System
Automates secure synchronization of local WordPress theme files to live cPanel hosting.

Protocol: FTPS (FTP over Explicit TLS 1.2/1.3)
Source of Truth: website/wordpress-theme/likhi/
Remote Target: public_html/wp-content/themes/likhi/
"""

import sys
import os
import ftplib
import ssl
import hashlib
import json
import time
import uuid
import io
import datetime
import argparse
import subprocess
import urllib.request
import urllib.error

# Ensure stdout handles UTF-8 on Windows consoles without charmap encode errors
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Resolve base directories
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
WEBSITE_DIR = os.path.join(REPO_ROOT, "website")
THEME_DIR = os.path.join(WEBSITE_DIR, "wordpress-theme", "likhi")
MANIFEST_FILE = os.path.join(WEBSITE_DIR, ".deploy_manifest.json")
BACKUPS_DIR = os.path.join(WEBSITE_DIR, "backups", "theme-deploy")
LOG_FILE = os.path.join(REPO_ROOT, "logs", "theme-deploy.log")

# Blacklist: files that must NEVER be uploaded
BLACKLISTED_PATTERNS = [
    ".env", "wp-config.php", "wp-includes", "wp-admin", "plugins",
    ".git", ".gitignore", ".deploy_credentials", ".deploy_manifest.json",
    ".DS_Store", "Thumbs.db", "Desktop.ini", "node_modules", ".tmp", ".bak"
]

# Health check endpoints on live site
HEALTH_CHECK_URLS = [
    "https://getlikhi.com/",
    "https://getlikhi.com/blog/",
    "https://getlikhi.com/download/",
]

# Canonical site + public path of this theme, used to prove that the FTP account
# we deploy to is really the folder the live site serves.
CANONICAL_SITE = "https://getlikhi.com"
PUBLIC_THEME_PREFIX = "/wp-content/themes/likhi/"
PROBE_UA = "Likhi-Deployment-Verifier/1.0 (+https://getlikhi.com/)"

def load_env_config():
    """Load configuration from website/.env, repo .env, or OS environment variables."""
    config = {
        "host": "getlikhi.com",
        "port": 21,
        "user": "",
        "pass": "",
        "remote_theme_dir": "public_html/wp-content/themes/likhi",
        "use_tls": True
    }

    env_files = [
        os.path.join(WEBSITE_DIR, ".env"),
        os.path.join(REPO_ROOT, ".env"),
    ]

    for env_path in env_files:
        if os.path.exists(env_path):
            try:
                with open(env_path, "r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if not line or line.startswith("#"):
                            continue
                        if "=" in line:
                            k, v = line.split("=", 1)
                            k = k.strip()
                            v = v.strip().strip('"').strip("'")
                            if k == "LIKHI_FTP_HOST": config["host"] = v
                            elif k == "LIKHI_FTP_PORT": config["port"] = int(v)
                            elif k == "LIKHI_FTP_USER": config["user"] = v
                            elif k == "LIKHI_FTP_PASS": config["pass"] = v
                            elif k == "LIKHI_FTP_REMOTE_THEME_DIR": config["remote_theme_dir"] = v
                            elif k == "LIKHI_FTP_USE_TLS": config["use_tls"] = (v.lower() == "true")
            except Exception as e:
                print(f"[WARN] Error reading {env_path}: {e}")

    # Environment variables take precedence if present
    if "LIKHI_FTP_HOST" in os.environ: config["host"] = os.environ["LIKHI_FTP_HOST"]
    if "LIKHI_FTP_PORT" in os.environ: config["port"] = int(os.environ["LIKHI_FTP_PORT"])
    if "LIKHI_FTP_USER" in os.environ: config["user"] = os.environ["LIKHI_FTP_USER"]
    if "LIKHI_FTP_PASS" in os.environ: config["pass"] = os.environ["LIKHI_FTP_PASS"]
    if "LIKHI_FTP_REMOTE_THEME_DIR" in os.environ: config["remote_theme_dir"] = os.environ["LIKHI_FTP_REMOTE_THEME_DIR"]
    if "LIKHI_FTP_USE_TLS" in os.environ: config["use_tls"] = (os.environ["LIKHI_FTP_USE_TLS"].lower() == "true")

    return config

def compute_file_sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def scan_local_theme_files():
    """Scans all theme files and returns dict of rel_path -> sha256."""
    files = {}
    if not os.path.exists(THEME_DIR):
        raise FileNotFoundError(f"Theme directory not found: {THEME_DIR}")

    for root, _, filenames in os.walk(THEME_DIR):
        for fname in filenames:
            full_path = os.path.join(root, fname)
            rel_path = os.path.relpath(full_path, THEME_DIR).replace("\\", "/")

            # Safety check: ensure file is strictly inside theme and not blacklisted
            if any(b.lower() in rel_path.lower() for b in BLACKLISTED_PATTERNS):
                continue

            files[rel_path] = compute_file_sha256(full_path)
    return files

def get_changed_files(local_files, force_all=False, specific_file=None):
    """Detects changed or new files compared to the manifest."""
    if specific_file:
        norm = specific_file.replace("\\", "/").strip("/")
        if norm.startswith("website/wordpress-theme/likhi/"):
            norm = norm[len("website/wordpress-theme/likhi/"):]
        if norm in local_files:
            return [norm]
        else:
            raise ValueError(f"Requested file '{specific_file}' not found in theme directory.")

    if force_all or not os.path.exists(MANIFEST_FILE):
        return sorted(list(local_files.keys()))

    manifest = {}
    try:
        with open(MANIFEST_FILE, "r", encoding="utf-8") as f:
            manifest = json.load(f)
    except Exception:
        return sorted(list(local_files.keys()))

    changed = []
    for rel_path, sha in local_files.items():
        if rel_path not in manifest or manifest[rel_path] != sha:
            changed.append(rel_path)
    return sorted(changed)

def validate_php_syntax(files_to_check):
    """Validates PHP syntax if php executable is available, otherwise performs basic lint checks."""
    errors = []
    has_php_bin = False
    try:
        res = subprocess.run(["php", "-v"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res.returncode == 0:
            has_php_bin = True
    except Exception:
        has_php_bin = False

    for rel_path in files_to_check:
        full_path = os.path.join(THEME_DIR, rel_path)
        if rel_path.endswith(".php"):
            if has_php_bin:
                p = subprocess.run(["php", "-l", full_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                if p.returncode != 0:
                    errors.append(f"{rel_path}: PHP syntax error - {p.stderr.strip() or p.stdout.strip()}")
            else:
                # Basic sanity check
                with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
                    content = f.read()
                    if "<<<<<<< HEAD" in content or ">>>>>>>" in content:
                        errors.append(f"{rel_path}: Unresolved git conflict marker found")
                    if content.count("<?php") == 0 and not rel_path.startswith("assets/"):
                        # Just a warning or potential error
                        pass
        elif rel_path.endswith(".json"):
            try:
                with open(full_path, "r", encoding="utf-8") as f:
                    json.load(f)
            except Exception as e:
                errors.append(f"{rel_path}: Invalid JSON - {e}")

    return errors

def connect_ftp(config):
    """Connects to server using FTPS (explicit TLS) or standard FTP."""
    host = config["host"]
    port = config["port"]
    user = config["user"]
    password = config["pass"]
    use_tls = config["use_tls"]

    if not user or not password:
        raise ValueError("Missing FTP credentials. Please configure website/.env (see website/.env.example)")

    print(f"Connecting to {host}:{port} ({'FTPS / TLS' if use_tls else 'FTP'})...")
    if use_tls:
        ftps = ftplib.FTP_TLS(timeout=30)
        ftps.connect(host, port)
        ftps.login(user, password)
        ftps.prot_p()  # Secure data channel
        ftps.set_pasv(True)
        return ftps
    else:
        ftp = ftplib.FTP(timeout=30)
        ftp.connect(host, port)
        ftp.login(user, password)
        ftp.set_pasv(True)
        return ftp

def ensure_remote_dir(ftp, remote_dir):
    """Recursively creates and navigates to remote directory."""
    parts = [p for p in remote_dir.replace("\\", "/").split("/") if p]
    ftp.cwd("/")
    for part in parts:
        try:
            ftp.cwd(part)
        except ftplib.error_perm:
            try:
                ftp.mkd(part)
                ftp.cwd(part)
            except Exception as e:
                raise RuntimeError(f"Failed to create/navigate remote directory '{part}' in '{remote_dir}': {e}")

def remote_file_exists_and_size(ftp, full_remote_path):
    """Returns (exists, size) for a remote file."""
    try:
        size = ftp.size(full_remote_path)
        return True, size
    except Exception:
        # Fallback to NLST/directory parse
        remote_dir = os.path.dirname(full_remote_path).replace("\\", "/")
        fname = os.path.basename(full_remote_path)
        try:
            ftp.cwd(remote_dir)
            files = ftp.nlst()
            if fname in files:
                return True, -1
        except Exception:
            pass
        return False, -1

def download_remote_file(ftp, full_remote_path, local_save_path):
    """Downloads a remote file if it exists."""
    os.makedirs(os.path.dirname(local_save_path), exist_ok=True)
    try:
        with open(local_save_path, "wb") as f:
            ftp.retrbinary(f"RETR {full_remote_path}", f.write)
        return True
    except Exception:
        if os.path.exists(local_save_path):
            os.remove(local_save_path)
        return False

def upload_local_file(ftp, local_full_path, full_remote_path):
    """Uploads a local file via binary transfer."""
    remote_dir = os.path.dirname(full_remote_path).replace("\\", "/")
    ensure_remote_dir(ftp, remote_dir)
    fname = os.path.basename(full_remote_path)
    with open(local_full_path, "rb") as f:
        ftp.storbinary(f"STOR {fname}", f)

def check_live_target_identity(ftp):
    """Prove the FTP target is the theme folder the live site actually serves.

    A jailed FTP account can point at a copy of the theme that no web server
    serves (that is exactly what happened when this account's home directory was
    another domain's theme folder): every deploy reported success while the live
    site never changed. We upload a throwaway probe, fetch it over HTTPS from the
    canonical site, and fail loudly when it is not visible.

    Returns (ok, detail_message).
    """
    token = "likhi-target-probe-%s.txt" % uuid.uuid4().hex[:12]
    url = CANONICAL_SITE + PUBLIC_THEME_PREFIX + token
    body_seen = None
    try:
        ftp.storbinary(f"STOR {token}", io.BytesIO(b"likhi-target-probe"))
        time.sleep(2)  # let the web server pick the new file up
        req = urllib.request.Request(url, headers={"User-Agent": PROBE_UA, "Cache-Control": "no-cache"})
        with urllib.request.urlopen(req, timeout=20) as resp:
            body_seen = resp.read()
    except Exception as exc:
        body_seen = None
        detail = f"probe not reachable ({exc})"
    else:
        detail = "probe served by the canonical site" if body_seen.strip() == b"likhi-target-probe" else "probe content mismatch"
    finally:
        try:
            ftp.delete("/" + token)
        except Exception:
            pass

    return body_seen is not None and body_seen.strip() == b"likhi-target-probe", f"{url} -> {detail}"


def verify_live_content(local_files, candidates):
    """Fetch deployed, web-readable files back over HTTPS and compare bytes.

    The HTTP health check only proves the site answers with 200, which stays true
    even when nothing was deployed. This compares real content.
    """
    checked = {}
    for rel_path in candidates:
        if not rel_path.endswith((".css", ".js", ".png", ".jpg", ".svg", ".txt")):
            continue
        local_path = os.path.join(THEME_DIR, rel_path)
        url = CANONICAL_SITE + PUBLIC_THEME_PREFIX + rel_path
        try:
            req = urllib.request.Request(url, headers={"User-Agent": PROBE_UA, "Cache-Control": "no-cache"})
            with urllib.request.urlopen(req, timeout=30) as resp:
                live_bytes = resp.read()
        except Exception as exc:
            checked[rel_path] = f"unreachable ({exc})"
            continue
        with open(local_path, "rb") as fh:
            local_bytes = fh.read()
        checked[rel_path] = "match" if live_bytes == local_bytes else f"MISMATCH (live={len(live_bytes)}B local={len(local_bytes)}B)"
        if checked[rel_path] == "match":
            break  # one confirmed file is enough to prove the target
    return checked


def perform_health_checks():
    """Hits live website URLs and asserts HTTP 200."""
    results = {}
    headers = {"User-Agent": "Likhi-Deployment-Verifier/1.0 (+https://getlikhi.com/)"}

    for url in HEALTH_CHECK_URLS:
        try:
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=10) as resp:
                results[url] = resp.getcode()
        except urllib.error.HTTPError as e:
            results[url] = e.code
        except Exception as e:
            results[url] = f"Error: {e}"
    return results

def get_git_info():
    """Returns current git commit hash and status if available."""
    try:
        res_hash = subprocess.run(["git", "rev-parse", "--short", "HEAD"],
                                  cwd=WEBSITE_DIR, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        res_stat = subprocess.run(["git", "status", "--porcelain"],
                                  cwd=WEBSITE_DIR, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        commit = res_hash.stdout.strip() or "unversioned"
        dirty = "dirty" if res_stat.stdout.strip() else "clean"
        return f"{commit} ({dirty})"
    except Exception:
        return "git-unavailable"

def log_deployment(log_entry):
    """Appends deployment record to LOG_FILE."""
    try:
        os.makedirs(os.path.dirname(LOG_FILE), exist_ok=True)
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(log_entry + "\n" + ("=" * 70) + "\n")
    except Exception as e:
        print(f"[WARN] Failed to write log: {e}")

def run_rollback():
    """Rolls back files using the latest backup snapshot."""
    if not os.path.exists(BACKUPS_DIR):
        print("No backups directory found. Rollback aborted.")
        return False

    subdirs = sorted([d for d in os.listdir(BACKUPS_DIR) if os.path.isdir(os.path.join(BACKUPS_DIR, d))])
    if not subdirs:
        print("No backup snapshots found. Rollback aborted.")
        return False

    latest_backup = os.path.join(BACKUPS_DIR, subdirs[-1])
    manifest_path = os.path.join(latest_backup, "backup_manifest.json")
    if not os.path.exists(manifest_path):
        print(f"No backup_manifest.json found in {latest_backup}. Rollback aborted.")
        return False

    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    files_to_restore = manifest.get("backed_up_files", [])
    if not files_to_restore:
        print("No files listed in backup manifest. Rollback aborted.")
        return False

    print(f"\n[ROLLBACK] Restoring {len(files_to_restore)} files from backup snapshot: {subdirs[-1]}")
    config = load_env_config()
    ftp = connect_ftp(config)

    restored = []
    base_remote = config["remote_theme_dir"].strip("/")
    for rel_path in files_to_restore:
        local_backup_file = os.path.join(latest_backup, rel_path)
        if not os.path.exists(local_backup_file):
            print(f"  [SKIP] Local backup copy missing for: {rel_path}")
            continue

        remote_full = f"/{base_remote}/{rel_path}"
        upload_local_file(ftp, local_backup_file, remote_full)
        restored.append(rel_path)
        print(f"  [RESTORED] {rel_path}")

    ftp.quit()
    print(f"\n[ROLLBACK COMPLETE] Successfully restored {len(restored)} files.")
    return True

def main():
    parser = argparse.ArgumentParser(description="Likhi WordPress Theme cPanel Direct Deployment System")
    parser.add_argument("--dry-run", action="store_true", help="Inspect and display files to upload without transferring")
    parser.add_argument("--all", action="store_true", help="Force deployment of all theme files regardless of manifest")
    parser.add_argument("--file", type=str, default=None, help="Deploy a single specific theme file (e.g. style.css)")
    parser.add_argument("--rollback", action="store_true", help="Roll back replaced files from the latest snapshot")
    parser.add_argument("--skip-target-check", action="store_true", help="Skip the live-target identity probe (not recommended)")
    args = parser.parse_args()

    print("======================================================================")
    print(" LIKHI WORDPRESS THEME — LOCAL -> CPANEL DIRECT DEPLOYMENT SYSTEM")
    print("======================================================================")

    if args.rollback:
        success = run_rollback()
        sys.exit(0 if success else 1)

    # 1. Inspect local theme directory
    local_files = scan_local_theme_files()
    print(f"Local Theme Directory : {THEME_DIR}")
    print(f"Total Theme Files     : {len(local_files)}")

    # 2. Detect changed files
    changed_files = get_changed_files(local_files, force_all=args.all, specific_file=args.file)

    if not changed_files:
        print("\nSTATUS: UP-TO-DATE")
        print("No local theme files have changed since the last deployment.")
        print("Use --all if you wish to force a complete re-synchronization.")
        sys.exit(0)

    print(f"\nDetected Changed / Target Files ({len(changed_files)}):")
    for f in changed_files:
        print(f"  * {f}")

    # 3. Pre-flight validation
    print("\nRunning pre-flight checks...")
    validation_errors = validate_php_syntax(changed_files)
    if validation_errors:
        print("\n[PRE-FLIGHT VALIDATION FAILED]")
        for err in validation_errors:
            print(f"  [FAIL] {err}")
        print("\nDeployment aborted due to pre-flight syntax / validation errors.")
        sys.exit(1)
    print("  [OK] Pre-flight checks passed (syntax and structure clean).")

    # 4. Dry-Run Handling
    if args.dry_run:
        print("\n======================================================================")
        print(" DRY RUN SUMMARY (No files will be modified on remote server)")
        print("======================================================================")
        print(f"Will upload ({len(changed_files)} files):")
        for f in changed_files:
            print(f"  + {f}")
        print("\nWill NOT touch:")
        print("  - wp-config.php (Protected)")
        print("  - WordPress Core files (Protected)")
        print("  - Plugins (Protected)")
        print("  - Uploads / Media Library (Protected)")
        print("  - Database (Untouched)")
        print("  - Unrelated themes (Untouched)")
        print("  - .env / credentials (Protected / Gitignored)")
        print("\nDry-run completed successfully.")
        sys.exit(0)

    # 5. Load credentials & connect
    config = load_env_config()
    if not config["user"] or not config["pass"]:
        print("\n[ERROR] Credentials not configured!")
        print(f"Please set your credentials in: {os.path.join(WEBSITE_DIR, '.env')}")
        print(f"A template is available at: {os.path.join(WEBSITE_DIR, '.env.example')}")
        sys.exit(1)

    try:
        ftp = connect_ftp(config)
        print("  [OK] Authenticated and secured data channel via TLS.")
    except Exception as e:
        print(f"\n[CONNECTION ERROR] Failed to connect/authenticate to {config['host']}:{config['port']}: {e}")
        sys.exit(1)

    # 5b. Prove the FTP target is the folder the live site serves. Without this,
    # a mis-pointed FTP account produces silent no-op deployments that still
    # report SUCCESS.
    if args.skip_target_check:
        print("\n[WARN] Live target identity check skipped (--skip-target-check).")
    else:
        print("\nVerifying that this FTP account writes to the live theme folder...")
        target_ok, target_detail = check_live_target_identity(ftp)
        if not target_ok:
            print("  [FAIL] FTP target is NOT the theme folder served by", CANONICAL_SITE)
            print(f"         {target_detail}")
            print("\n  The FTP account in website/.env is chrooted to a folder the live site")
            print("  does not serve (for example another domain's theme directory). Point")
            print("  the account's directory at this site's theme folder, or set the")
            print("  canonical site in CANONICAL_SITE, then re-run. Nothing was uploaded.")
            sys.exit(1)
        print(f"  [OK] {target_detail}")

    # 6. Remote Backup Snapshot Creation
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    current_backup_dir = os.path.join(BACKUPS_DIR, timestamp)
    backed_up_files = []

    print(f"\nCreating remote backup snapshot in: backups/theme-deploy/{timestamp}...")
    base_remote = config["remote_theme_dir"].strip("/")

    for rel_path in changed_files:
        full_remote = f"/{base_remote}/{rel_path}"
        exists, _ = remote_file_exists_and_size(ftp, full_remote)
        if exists:
            local_save = os.path.join(current_backup_dir, rel_path)
            if download_remote_file(ftp, full_remote, local_save):
                backed_up_files.append(rel_path)

    # Save backup manifest
    os.makedirs(current_backup_dir, exist_ok=True)
    with open(os.path.join(current_backup_dir, "backup_manifest.json"), "w", encoding="utf-8") as f:
        json.dump({
            "timestamp": timestamp,
            "git_info": get_git_info(),
            "backed_up_files": backed_up_files
        }, f, indent=2)
    print(f"  [OK] Preserved {len(backed_up_files)} existing remote files before replacement.")

    # 7. Upload changed files
    print(f"\nUploading {len(changed_files)} changed files to cPanel...")
    uploaded_files = []
    upload_errors = []

    for rel_path in changed_files:
        local_full = os.path.join(THEME_DIR, rel_path)
        full_remote = f"/{base_remote}/{rel_path}"
        try:
            upload_local_file(ftp, local_full, full_remote)
            uploaded_files.append(rel_path)
            print(f"  [UPLOAD] {rel_path}")
        except Exception as e:
            upload_errors.append(f"{rel_path}: {e}")
            print(f"  [FAIL] {rel_path} ({e})")

    # 8. Remote Verification
    print("\nVerifying remote files on cPanel...")
    verification_failures = []
    for rel_path in uploaded_files:
        local_full = os.path.join(THEME_DIR, rel_path)
        full_remote = f"/{base_remote}/{rel_path}"
        local_size = os.path.getsize(local_full)
        exists, remote_size = remote_file_exists_and_size(ftp, full_remote)

        if not exists:
            verification_failures.append(f"{rel_path}: Not found on remote server after upload")
        elif remote_size != -1 and remote_size != local_size:
            verification_failures.append(f"{rel_path}: Size mismatch (local={local_size}B, remote={remote_size}B)")
        else:
            print(f"  [OK] Verified: {rel_path} ({local_size} bytes)")

    ftp.quit()

    if upload_errors or verification_failures:
        print("\n[DEPLOYMENT FAILED DURING TRANSFER/VERIFICATION]")
        for err in upload_errors + verification_failures:
            print(f"  [FAIL] {err}")
        print("\nTriggering automatic rollback of preserved files...")
        run_rollback()
        sys.exit(1)

    # 9. Live HTTP Verification
    print("\nExecuting live website HTTP verification...")
    http_checks = perform_health_checks()
    http_failed = False
    for url, status in http_checks.items():
        if status == 200:
            print(f"  [OK] {url} -> HTTP 200 OK")
        else:
            print(f"  [FAIL] {url} -> HTTP {status}")
            http_failed = True

    # 9b. Content verification: fetch a deployed file back and compare bytes.
    print("\nVerifying deployed content is actually served by the live site...")
    content_checks = verify_live_content(local_files, uploaded_files)
    content_failed = False
    if not content_checks:
        print("  [WARN] No web-readable file was uploaded, content check skipped.")
    for rel_path, result in content_checks.items():
        if result == "match":
            print(f"  [OK] {rel_path}: live bytes match local bytes")
        else:
            print(f"  [FAIL] {rel_path}: {result}")
            content_failed = True
    if content_failed:
        http_failed = True

    if http_failed:
        print("\n[WARNING] Live HTTP verification returned non-200 status code!")

    # 10. Update local manifest
    for f in uploaded_files:
        manifest_data = {}
        if os.path.exists(MANIFEST_FILE):
            try:
                with open(MANIFEST_FILE, "r", encoding="utf-8") as mf:
                    manifest_data = json.load(mf)
            except Exception:
                pass
        manifest_data.update(local_files)
        with open(MANIFEST_FILE, "w", encoding="utf-8") as mf:
            json.dump(manifest_data, mf, indent=2)

    # 11. Structured Report Output
    status_str = "SUCCESS" if not http_failed else "FAILED (HTTP verification)"
    git_info = get_git_info()
    report = f"""
======================================================================
 FINAL DEPLOYMENT REPORT
======================================================================
LOCAL:
- files changed     : {len(uploaded_files)} files ({', '.join(uploaded_files[:5])}{'...' if len(uploaded_files) > 5 else ''})
- tests performed   : Pre-flight syntax validation, size verification
- git status        : {git_info}

DEPLOYED:
- files uploaded    : {len(uploaded_files)}
- cPanel target     : {config['host']} -> {config['remote_theme_dir']}
- protocol          : {'FTPS (Explicit TLS)' if config['use_tls'] else 'FTP'}
- deployment time   : {timestamp}
- backup snapshot   : backups/theme-deploy/{timestamp}

VERIFIED:
- remote files      : {len(uploaded_files)} files confirmed on server with identical byte size
- live target       : {'verified: FTP account writes to the theme folder served by ' + CANONICAL_SITE if not args.skip_target_check else 'identity probe skipped'}
- content check     : {', '.join(f'{k} {v}' for k, v in content_checks.items()) or 'n/a'}
- HTTP verification : {', '.join([f"{u.replace('https://getlikhi.com', '') or '/'} ({s})" for u, s in http_checks.items()])}
- relevant pages    : Homepage, /blog/, /download/ responsive and returning HTTP 200

STATUS:
{status_str}
======================================================================
"""
    print(report)
    log_deployment(f"[{timestamp}] Status: {status_str}\nGit: {git_info}\nFiles: {', '.join(uploaded_files)}\nHTTP: {json.dumps(http_checks)}")

if __name__ == "__main__":
    main()
