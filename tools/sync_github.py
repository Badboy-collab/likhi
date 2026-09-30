#!/usr/bin/env python3
"""
Likhi v1.0.1 — GitHub Synchronization & Release Publishing Script.
Pushes branch master to Badboy-collab/likhi and creates/updates v1.0.1 release asset.
Supports:
  1. Personal Access Token: python tools/sync_github.py --token <TOKEN>
  2. Device OAuth Flow: python tools/sync_github.py (interactive)
"""

import os
import sys
import json
import time
import base64
import hashlib
import argparse
import subprocess
import urllib.request
import urllib.parse
import urllib.error

CLIENT_ID = '01ab8ac9400c4e429b23'
REPO = 'Badboy-collab/likhi'
TAG = 'v1.0.1'
RELEASE_NAME = 'Likhi v1.0.1 - One-Click Installer (Windows)'
RELEASE_BODY = """## Likhi v1.0.1 Release (Windows)

### Highlights & Fixes:
- **'Why Likhi' (কেন লিখি) Product & Philosophy Page**: Full interactive product information, architecture, and vision page inside Settings with 6-step workflow, 4 problem-solution cards, 2-column natural Banglish mapping, and direct settings navigation.
- **Silent Update & User Control**: Added auto-update check toggle in Settings and silenced background update notifications so user typing is never interrupted.
- **Settings & Dashboard UI Polish**: Eliminated Bengali typography clipping (WS_CLIPCHILDREN, increased diacritic padding), added modern fluent styled buttons, and fixed dark border artifacts.
- **In-App Auto-Update Pipeline**: Fully integrated download and installation flow directly in Settings app with live progress, CryptoAPI SHA-256 integrity verification, and silent/interactive update launch.
- **Conversational Bigrams**: Enhanced context-aware candidate ranking for everyday conversational phrases.
- **Zero-Flicker Candidate Popup**: Eliminated 1-frame background erase flashes during rapid typing.
- **Personal Learning Safety**: Added confirmation dialog before resetting personal typing habits; personal dictionary remains 100% intact.
- **Privacy First**: Transparent wording for optional online suggestions (strictly zero keylogging, zero telemetry).
- **80,289-Word Lexicon**: Full expanded high-performance lexicon preserved.

### Verification:
- **File**: `LikhiSetup.exe`
- **Size**: 18,139,995 bytes
- **SHA-256**: `21ae667deffcced3efd6caff59dd3fe35f76fff05ede4e9eca6c355516eccb3c`
- **Regression Tests**: 983 / 983 Passing (100%)
"""
ASSET_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'release_package', 'LikhiSetup.exe')
EXPECTED_SIZE = 18139995
EXPECTED_SHA256 = '21ae667deffcced3efd6caff59dd3fe35f76fff05ede4e9eca6c355516eccb3c'

def compute_sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def get_device_code():
    data = urllib.parse.urlencode({'client_id': CLIENT_ID, 'scope': 'repo'}).encode()
    req = urllib.request.Request('https://github.com/login/device/code', data=data, headers={'Accept': 'application/json', 'User-Agent': 'likhi-publish'})
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read().decode())

def poll_device_token(device_code, interval=5, expires_in=900):
    start = time.time()
    while time.time() - start < expires_in:
        data = urllib.parse.urlencode({
            'client_id': CLIENT_ID,
            'device_code': device_code,
            'grant_type': 'urn:ietf:params:oauth:grant-type:device_code'
        }).encode()
        req = urllib.request.Request('https://github.com/login/oauth/access_token', data=data, headers={'Accept': 'application/json', 'User-Agent': 'likhi-publish'})
        try:
            with urllib.request.urlopen(req) as resp:
                res = json.loads(resp.read().decode())
                if 'access_token' in res:
                    return res['access_token']
                err = res.get('error')
                if err not in ('authorization_pending', 'slow_down'):
                    raise RuntimeError(f"Device authorization error: {res}")
        except urllib.error.HTTPError as e:
            pass
        time.sleep(interval)
    raise TimeoutError("Timed out waiting for device authorization.")

def push_master(token):
    print(">>> Pushing master branch to GitHub...")
    auth_b64 = base64.b64encode(f"x-access-token:{token}".encode('ascii')).decode('ascii')
    header_arg = f"AUTHORIZATION: basic {auth_b64}"
    res = subprocess.run([
        'git', '-c', 'credential.helper=',
        '-c', f'http.extraheader={header_arg}',
        'push', 'origin', 'master'
    ], capture_output=True, text=True)
    print(res.stdout)
    if res.returncode != 0:
        print(res.stderr)
        raise RuntimeError(f"Git push failed with exit code {res.returncode}")
    print(">>> Git push master: SUCCESS")

def create_or_update_release(token):
    headers = {
        'Authorization': f'token {token}',
        'Accept': 'application/vnd.github.v3+json',
        'User-Agent': 'likhi-publish'
    }
    
    # 1. Check if release exists
    release_url = f"https://api.github.com/repos/{REPO}/releases/tags/{TAG}"
    req = urllib.request.Request(release_url, headers=headers)
    release = None
    try:
        with urllib.request.urlopen(req) as resp:
            release = json.loads(resp.read().decode())
            print(f">>> Found existing release {TAG} (ID: {release['id']})")
    except urllib.error.HTTPError as e:
        if e.code == 404:
            print(f">>> Creating new release {TAG}...")
            create_payload = json.dumps({
                'tag_name': TAG,
                'target_commitish': 'master',
                'name': RELEASE_NAME,
                'body': RELEASE_BODY,
                'draft': False,
                'prerelease': False
            }).encode('utf-8')
            create_req = urllib.request.Request(
                f"https://api.github.com/repos/{REPO}/releases",
                data=create_payload,
                headers={**headers, 'Content-Type': 'application/json'}
            )
            with urllib.request.urlopen(create_req) as resp:
                release = json.loads(resp.read().decode())
                print(f">>> Created release {TAG} (ID: {release['id']})")
        else:
            raise

    # 2. Upload or replace asset
    release_id = release['id']
    asset_name = os.path.basename(ASSET_PATH)
    
    # Remove existing asset if present
    for asset in release.get('assets', []):
        if asset['name'] == asset_name:
            print(f">>> Deleting existing asset {asset_name} (ID: {asset['id']})...")
            del_req = urllib.request.Request(
                f"https://api.github.com/repos/{REPO}/releases/assets/{asset['id']}",
                headers=headers,
                method='DELETE'
            )
            with urllib.request.urlopen(del_req) as resp:
                pass
            break

    print(f">>> Uploading {asset_name} ({EXPECTED_SIZE:,} bytes)...")
    upload_url = f"https://uploads.github.com/repos/{REPO}/releases/{release_id}/assets?name={asset_name}"
    with open(ASSET_PATH, 'rb') as f:
        asset_data = f.read()
    
    upload_req = urllib.request.Request(
        upload_url,
        data=asset_data,
        headers={**headers, 'Content-Type': 'application/octet-stream'}
    )
    with urllib.request.urlopen(upload_req) as resp:
        uploaded_asset = json.loads(resp.read().decode())
        print(f">>> Asset uploaded successfully: {uploaded_asset.get('browser_download_url')}")
        print(f">>> Asset size: {uploaded_asset.get('size')} bytes")

    return release['html_url']

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--token', default=os.environ.get('GITHUB_TOKEN', ''), help='GitHub Personal Access Token')
    parser.add_argument('--request-code-only', action='store_true', help='Only request and print device code')
    args = parser.parse_args()

    # Pre-verification
    if not os.path.exists(ASSET_PATH):
        print(f"ERROR: {ASSET_PATH} does not exist!")
        sys.exit(1)
    
    size = os.path.getsize(ASSET_PATH)
    sha = compute_sha256(ASSET_PATH)
    print("=" * 60)
    print(" LIKHI v1.0.1 — GITHUB SYNCHRONIZATION")
    print("=" * 60)
    print(f"Asset:  {ASSET_PATH}")
    print(f"Size:   {size:,} bytes (expected {EXPECTED_SIZE:,})")
    print(f"SHA256: {sha}")
    if size != EXPECTED_SIZE or sha.lower() != EXPECTED_SHA256.lower():
        print("ERROR: Asset size or SHA256 does not match verified v1.0.1 build!")
        sys.exit(1)

    token = args.token
    if not token:
        dc = get_device_code()
        print("\n" + "=" * 60, flush=True)
        print(" GITHUB DEVICE AUTHORIZATION REQUIRED", flush=True)
        print("=" * 60, flush=True)
        print(f" 1. Go to:    {dc['verification_uri']}", flush=True)
        print(f" 2. Enter:    {dc['user_code']}", flush=True)
        print("=" * 60 + "\n", flush=True)
        
        if args.request_code_only:
            return 0
            
        print("Waiting for authorization in browser...", flush=True)
        token = poll_device_token(dc['device_code'], interval=dc.get('interval', 5), expires_in=dc.get('expires_in', 900))
        print(">>> Authorization successful!", flush=True)

    push_master(token)
    release_url = create_or_update_release(token)
    print("\n" + "=" * 60)
    print(" GITHUB SYNCHRONIZATION COMPLETE")
    print("=" * 60)
    print(f"Release URL: {release_url}")
    print("Status: SUCCESS")

if __name__ == '__main__':
    main()
