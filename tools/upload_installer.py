#!/usr/bin/env python3
"""
Likhi Installer — upload the standalone installer to getlikhi.com.

The getlikhi.com document root is /home/shohojba/getlikhi.com, so the dedicated
installer FTP account must be jailed to
/home/shohojba/getlikhi.com/downloads. Its FTP-visible remote root is then '.'.
Never use the WordPress theme FTP account. FTP PWD alone cannot prove the cPanel
filesystem mapping; confirm the account directory in cPanel before enabling
uploads with LIKHI_INSTALLER_FTP_TARGET_CONFIRMED=true.

Usage:
    python tools/upload_installer.py                # upload + verify
    python tools/upload_installer.py --dry-run      # show what would happen
"""

import argparse
import hashlib
import os
import posixpath
import sys
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import deploy_theme as dt  # reuse the proven FTPS config/connection helpers

DEFAULT_LOCAL = os.path.join(dt.REPO_ROOT, "release_package", "LikhiSetup.exe")
EXPECTED_SIZE = 18_074_041
EXPECTED_SHA256 = "2082cbf776ac4ce429913738b4c523491dfd56a3e8d1f41ebe445b091eee30b8"
EXPECTED_DOCUMENT_ROOT = "/home/shohojba/getlikhi.com"
EXPECTED_INSTALLER_DIRECTORY = posixpath.join(EXPECTED_DOCUMENT_ROOT, "downloads")
REMOTE_PATH = "/LikhiSetup.exe"
PUBLIC_URL = dt.CANONICAL_SITE + "/downloads/LikhiSetup.exe"


def load_installer_config():
    """Load only the dedicated installer FTPS settings; never fall back to theme credentials."""
    names = {
        "LIKHI_INSTALLER_FTP_HOST": ("host", str),
        "LIKHI_INSTALLER_FTP_PORT": ("port", int),
        "LIKHI_INSTALLER_FTP_USER": ("user", str),
        "LIKHI_INSTALLER_FTP_PASS": ("pass", str),
        "LIKHI_INSTALLER_FTP_REMOTE_DIR": ("remote_dir", str),
        "LIKHI_INSTALLER_FTP_USE_TLS": ("use_tls", lambda value: value.lower() == "true"),
        "LIKHI_INSTALLER_FTP_EXPECTED_DIR": ("expected_dir", str),
        "LIKHI_INSTALLER_FTP_TARGET_CONFIRMED": ("target_confirmed", lambda value: value.lower() == "true"),
    }
    config = {
        "host": "", "port": 21, "user": "", "pass": "", "remote_dir": ".", "use_tls": False,
        "expected_dir": EXPECTED_INSTALLER_DIRECTORY, "target_confirmed": False,
    }
    env_files = [os.path.join(dt.WEBSITE_DIR, ".env"), os.path.join(dt.REPO_ROOT, ".env")]

    for env_path in env_files:
        if not os.path.exists(env_path):
            continue
        with open(env_path, "r", encoding="utf-8") as env_file:
            for line in env_file:
                key, separator, value = line.strip().partition("=")
                if separator and key in names:
                    config_key, convert = names[key]
                    config[config_key] = convert(value.strip().strip('"').strip("'"))

    for name, (config_key, convert) in names.items():
        if name in os.environ:
            config[config_key] = convert(os.environ[name])

    if config["host"].lower() != "ftp.islamiceboi.com" or config["port"] != 21:
        raise ValueError("Installer FTP host/port do not match the approved endpoint.")
    if config["user"].lower() != "likhidownload@getlikhi.com":
        raise ValueError("The dedicated installer FTP username is not configured.")
    if config["user"].lower() == "likhideploy@getlikhi.com":
        raise ValueError("The theme FTP account is prohibited for installer uploads.")
    if not config["pass"]:
        raise ValueError("Dedicated installer FTP password is missing from local configuration.")
    if config["remote_dir"] not in ("", "."):
        raise ValueError("Installer FTP remote root must be '.'.")
    if config["expected_dir"] != EXPECTED_INSTALLER_DIRECTORY:
        raise ValueError("Expected cPanel installer directory must be '" + EXPECTED_INSTALLER_DIRECTORY + "'.")
    if not config["target_confirmed"]:
        raise ValueError("Confirm the dedicated FTP account directory in cPanel before enabling uploads.")
    if not config["use_tls"]:
        raise ValueError("Installer FTP must use explicit FTPS.")
    return config


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(1 << 16)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description="Upload the Likhi installer to getlikhi.com")
    parser.add_argument("--file", default=DEFAULT_LOCAL, help="Local installer file")
    parser.add_argument("--no-verify-http", action="store_true", help="Skip the public HTTPS verification")
    parser.add_argument("--dry-run", action="store_true", help="Print the plan without uploading")
    args = parser.parse_args()

    if not os.path.exists(args.file):
        print(f"[ERROR] Local installer not found: {args.file}")
        return 1
    if os.path.abspath(args.file) != os.path.abspath(DEFAULT_LOCAL):
        print("[ERROR] Only the approved release_package/LikhiSetup.exe may be uploaded.")
        return 1

    remote_name = "LikhiSetup.exe"
    remote_path = posixpath.normpath(REMOTE_PATH)
    public_url = PUBLIC_URL
    local_size = os.path.getsize(args.file)
    local_sha = sha256_of(args.file)

    if local_size != EXPECTED_SIZE or local_sha.lower() != EXPECTED_SHA256:
        print("[ERROR] Local installer does not match the approved size and SHA-256.")
        print(f"Expected: {EXPECTED_SIZE} bytes / {EXPECTED_SHA256}")
        print(f"Found   : {local_size} bytes / {local_sha}")
        return 1

    print("=" * 70)
    print(" LIKHI INSTALLER — UPLOAD TO GETLIKHI.COM")
    print("=" * 70)
    print(f"Local file   : {args.file}")
    print(f"Size / SHA256: {local_size:,} B / {local_sha}")
    print(f"Remote target: {remote_path}")
    print(f"Expected cPanel location: {EXPECTED_INSTALLER_DIRECTORY}/{remote_name}")
    print(f"Public URL   : {public_url}")

    if args.dry_run:
        print("\n[DRY RUN] No upload performed.")
        return 0

    try:
        config = load_installer_config()
    except Exception as exc:
        print(f"\n[ERROR] Installer FTP configuration: {exc}")
        return 1

    ftp = None
    try:
        ftp = dt.connect_ftp(config)
        print("\n  [OK] Connected (FTPS).")
        dt.ensure_remote_dir(ftp, config["remote_dir"])
        with open(args.file, "rb") as f:
            ftp.storbinary(f"STOR {remote_name}", f)
        print(f"  [OK] Uploaded {remote_name}")

        exists, remote_size = dt.remote_file_exists_and_size(ftp, remote_path)
        if not exists:
            print("  [FAIL] File not found on server after upload.")
            return 1
        if remote_size not in (-1, local_size):
            print(f"  [FAIL] Size mismatch (local={local_size} B, remote={remote_size} B).")
            return 1
        print(f"  [OK] Remote size verified: {local_size:,} B")

        remote_digest = hashlib.sha256()
        ftp.retrbinary(f"RETR {remote_path}", remote_digest.update)
        remote_sha = remote_digest.hexdigest()
        print(f"  Remote SHA-256: {remote_sha}")
        if remote_sha != EXPECTED_SHA256:
            print("  [FAIL] Remote SHA-256 does not match the approved installer.")
            return 1
    except Exception as exc:
        print(f"\n[FAIL] Upload error: {exc}")
        return 1
    finally:
        if ftp is not None:
            try:
                ftp.quit()
            except Exception:
                pass

    if args.no_verify_http:
        print("\n[OK] Upload complete (public HTTPS verification skipped).")
        return 0

    print("\nVerifying the public HTTPS URL...")
    req = urllib.request.Request(public_url, headers={"User-Agent": "Likhi-Installer-Verifier/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            status = resp.getcode()
            body = resp.read()
    except urllib.error.HTTPError as exc:
        print(f"  [FAIL] {public_url} -> HTTP {exc.code}")
        return 1
    except Exception as exc:
        print(f"  [FAIL] {public_url} -> {exc}")
        return 1

    public_sha = hashlib.sha256(body).hexdigest()
    print(f"  HTTP status : {status}")
    print(f"  Bytes served: {len(body):,}")
    print(f"  SHA-256     : {public_sha}")

    if status != 200 or len(body) != local_size or public_sha != local_sha:
        print("\n[FAIL] Public file does not match the local installer.")
        return 1

    print("\n[OK] Public download verified — status, size and SHA-256 all match.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
