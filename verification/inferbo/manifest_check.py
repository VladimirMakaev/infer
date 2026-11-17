#!/usr/bin/env python3
"""Compare a regenerated manifest against expected/counts.json for one arm.

Prints an empty line when the arm matches, and a short description of what moved
otherwise. Exit status is always 0: the caller decides what a mismatch means.

WHY BOTH A HASH AND COUNTS. The sha256 pins the manifest exactly, so the counts add no
detection power -- they add a READABLE FAILURE. "s32: probes 7558 -> 7000" tells you what
changed; "manifest hash moved" tells you only that something did, and the manifest is
megabytes of generated JSON that nobody is going to diff. They are also the reason the
counts are not dead weight: a baseline field that nothing reads looks like coverage
without being any, which is exactly the defect this grid exists to catch elsewhere.
"""
import hashlib
import json
import sys


def main():
    expected_path, arm, manifest_path = sys.argv[1], sys.argv[2], sys.argv[3]
    want = json.load(open(expected_path)).get(arm)
    if want is None:
        print(f'no baseline entry for arm {arm}')
        return 0
    raw = open(manifest_path, 'rb').read()
    man = json.loads(raw)
    got = {
        'manifest_sha256': hashlib.sha256(raw).hexdigest(),
        'probes': sum(1 for m in man.values() if m['kind'] == 'probe'),
        'reaches': sum(1 for m in man.values() if m['kind'] == 'reach'),
    }
    msgs = []
    if want.get('manifest_sha256') != got['manifest_sha256']:
        msgs.append('manifest hash moved')
    for key in ('probes', 'reaches'):
        if want.get(key) != got[key]:
            msgs.append(f'{key} {want.get(key)} -> {got[key]}')
    print('; '.join(msgs))
    return 0


if __name__ == '__main__':
    sys.exit(main())
