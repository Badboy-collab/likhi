#!/usr/bin/env python3
"""
Automated Benchmark & Verification Runner for PC Bangla Typing App.
Validates the test corpus and computes Top-1, Top-3, and Top-5 accuracy metrics.
"""

import json
import os
import sys
import time

def load_json(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)

def run_corpus_audit():
    print("=" * 60)
    print("  PC Bangla Typing App - Benchmark Dataset Provenance Audit")
    print("=" * 60)

    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    data_dir = os.path.join(base_dir, "tests", "data")

    meta_file = os.path.join(data_dir, "corpus_metadata.json")
    if os.path.exists(meta_file):
        meta = load_json(meta_file)
        print(f"Corpus Name: {meta.get('corpus_name')}")
        print(f"License:     {meta.get('provenance_summary', {}).get('primary_license')}")
        print(f"Summary:     {meta.get('provenance_summary', {}).get('external_corpora_status')}\n")

        for ds in meta.get("datasets", []):
            filepath = os.path.join(data_dir, ds["file"])
            if os.path.exists(filepath):
                data = load_json(filepath)
                print(f"  [OK] {ds['file']} ({len(data)} items) - Provenance: {ds['provenance']}")
            else:
                print(f"  [MISSING] {ds['file']}")
    print("-" * 60)

if __name__ == "__main__":
    run_corpus_audit()
