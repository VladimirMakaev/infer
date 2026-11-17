#!/usr/bin/env python3
"""Compare the unsound points a run observed against the pinned baseline.

Prints JSON {"new": [...], "fixed": [...], "unknown": [...]}; the caller turns each
non-empty list into a failed obligation.

  new      observed and not pinned          -- a regression
  fixed    pinned and not observed          -- a deliberate change whose baseline must
                                               move in the same commit
  unknown  pinned and never generated       -- a pin that matches nothing

THE THREE CATEGORIES ARE DELIBERATELY SEPARATE, and `unknown` is the one that was missing.
Intersecting the pinned set with what the run generated was right while the sample was
blind: a run could not be blamed for cases it never looked at. The watch list now forces
every pinned quadruple to be generated, so a pin that is STILL absent is either a typo or
a pin whose quadruple is missing from always_sample.json. Excusing it silently turns it
into coverage that does not exist -- review added probe_w_shl_9999_0 and the grid passed.

Lives in its own file rather than inline in run.sh so it can be tested without running a
grid: the holes this guards against are cheap to assert and expensive to reproduce.
"""
import json
import os
import sys


def compare(pinned, arms, manifest_of, observed_of):
    """Pure core. `manifest_of`/`observed_of` map an arm to a set of names."""
    new, fixed, unknown = [], [], []
    for arm in arms:
        present = manifest_of(arm)
        observed = observed_of(arm)
        want = set(pinned.get(arm, []))
        unknown += sorted(f'{arm}:{n}' for n in want - present)
        expect = want & present
        new += sorted(observed - expect)
        fixed += sorted(expect - observed)
    return {'new': new, 'fixed': fixed, 'unknown': unknown}


def main():
    pinned_path, out, arms = sys.argv[1], sys.argv[2], sys.argv[3].split()
    if not os.path.exists(pinned_path):
        print(f'{pinned_path} is missing; there is no baseline to compare against',
              file=sys.stderr)
        return 2
    pinned = json.load(open(pinned_path))
    load = lambda p: set(json.load(open(p)))
    print(json.dumps(compare(
        pinned, arms,
        lambda a: load(os.path.join(out, f'manifest_{a}.json')),
        lambda a: load(os.path.join(out, f'unsound_{a}.json')))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
