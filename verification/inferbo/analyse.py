#!/usr/bin/env python3
"""Turn a grid run into verdicts.

  probe silent + its reach control reports  -> UNSOUND candidate: inferbo pruned a
      guard `v == R` for an R the program really can produce. The reach sibling shares
      the operands, the op and the access shape, so the guard is the only difference.
  probe silent + reach ALSO silent          -> INCONCLUSIVE, not a finding. The whole
      function was skipped, or the access was not seen; says nothing about the op.
  probe reports                             -> sound on this point.
"""
import collections
import json
import sys

manifest, report = json.load(open(sys.argv[1])), json.load(open(sys.argv[2]))
fired = {r['procedure'] for r in report}
unsound, inconclusive, sound = [], [], 0
for fn, m in manifest.items():
    if m['kind'] != 'probe':
        continue
    if fn in fired:
        sound += 1
    elif m['reach'] in fired:
        unsound.append((fn, m))
    else:
        inconclusive.append((fn, m))

reaches = [f for f, m in manifest.items() if m['kind'] == 'reach']
print(f'reach controls: {sum(1 for r in reaches if r in fired)}/{len(reaches)} fired '
      f'(a control that does not fire disarms its probes)')
print(f'probes: {sound} sound, {len(unsound)} UNSOUND, {len(inconclusive)} inconclusive')

by = collections.defaultdict(list)
for fn, m in unsound:
    by[(m['op'], m['impl_defined'])].append((fn, m))
for (op, impl), v in sorted(by.items(), key=lambda kv: -len(kv[1])):
    tag = '  [impl-defined shift of a negative, i.e. the handed-off class]' if impl else ''
    print(f'\n=== {op}: {len(v)} unsound points{tag}')
    for fn, m in sorted(v, key=lambda t: (t[1]['x'], t[1]['y']))[:8]:
        print(f'    x={m["x"]} y={m["y"]}  achievable R={m["r"]}  excluded by inferbo   {fn}')
    if len(v) > 8:
        print(f'    ... and {len(v) - 8} more')
json.dump({f: m for f, m in unsound}, open(sys.argv[3], 'w'), indent=1)
