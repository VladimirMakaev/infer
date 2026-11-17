#!/usr/bin/env python3
"""Loop grid: does inferbo MISS a real out-of-bounds access inside a loop?

The concrete grid cannot see this. It has no loop, so Bound.widen_l / widen_u and the
widening thresholds never run, and those are the ranked #2 target. Here the detector is
not a `== R` guard but the access itself:

    int a[SIZE];
    for (int k = START; k < LIMIT; k += STEP) a[k] = 1;

  The exact set of indices the loop touches is decided by integer arithmetic, so whether
  the program is out of bounds is a fact, not an estimate. A configuration that really
  does go out of bounds and draws NO report is a missed buffer overrun -- the same class
  of result as finding 3, reached through widening instead of through a transfer function.

  Configurations that stay in bounds are kept too, but a report on one of those is a
  FALSE POSITIVE, which is precision rather than soundness. They are counted and reported
  separately and never presented as a bug.

Each configuration gets a reach_ sibling: the same loop followed by an unconditional
a[SIZE] write. That must report, otherwise the function or the array was not visible and
the probe's silence says nothing about widening.
"""
import json

SIZES = [1, 2, 7, 10, 64]
STARTS = [0, 1, -1, -3]
STEPS = [1, 2, 3]


def touched(start, limit, step):
    """The indices the loop actually visits. Pure arithmetic, no execution needed, and
    cross-checked against a run of the real loop by loop_oracle.c."""
    if step <= 0 or start >= limit:
        return []
    n = (limit - start + step - 1) // step
    return [start + i * step for i in range(n)]


def main():
    out = ['#include <stdint.h>', '']
    man = {}
    qi = 0
    for size in SIZES:
        for start in STARTS:
            for step in STEPS:
                for limit in sorted({size - 1, size, size + 1, size + 3, 2 * size}):
                    idx = touched(start, limit, step)
                    if not idx:
                        continue  # the body never runs; nothing to say
                    lo, hi = min(idx), max(idx)
                    oob = lo < 0 or hi >= size
                    qi += 1
                    base = f'L{qi}'
                    loop = (f'  int a[{size}];\n'
                            f'  for (int k = {start}; k < {limit}; k += {step}) a[k] = 1;\n')
                    out.append(f'/* size={size} start={start} limit={limit} step={step}: '
                               f'touches [{lo},{hi}] -> {"OUT OF BOUNDS" if oob else "in bounds"} */')
                    out.append(f'int probe_{base}(void) {{\n{loop}  return a[0];\n}}\n')
                    out.append(f'int reach_{base}(void) {{\n{loop}  a[{size}] = 1;\n'
                               f'  return a[0];\n}}\n')
                    man[f'probe_{base}'] = {
                        'kind': 'probe', 'size': size, 'start': start, 'limit': limit,
                        'step': step, 'lo': lo, 'hi': hi, 'oob': oob,
                        'reach': f'reach_{base}'}
                    man[f'reach_{base}'] = {'kind': 'reach'}
    open('grid_loop.c', 'w').write('\n'.join(out))
    json.dump(man, open('manifest_loop.json', 'w'), indent=1)
    probes = [m for m in man.values() if m['kind'] == 'probe']
    print(f'grid_loop.c: {len(probes)} configurations '
          f'({sum(1 for m in probes if m["oob"])} genuinely out of bounds, '
          f'{sum(1 for m in probes if not m["oob"])} in bounds)')

    # The oracle runs each loop for real over a generously oversized buffer and records
    # the indices actually visited, so the index range above is checked by execution
    # rather than trusted from my arithmetic.
    o = ['#include <stdio.h>', 'static long bad = 0, n = 0;',
         'static void chk(const char *q, int lo, int hi, int glo, int ghi) {',
         '  n++;',
         '  if (lo != glo || hi != ghi) { bad++;',
         '    printf("MISMATCH %s: ran [%d,%d], expected [%d,%d]\\n", q, glo, ghi, lo, hi); }',
         '}', 'int main(void) {']
    for fn, m in man.items():
        if m['kind'] != 'probe':
            continue
        o.append('  {')
        o.append(f'    int glo = 1 << 30, ghi = -(1 << 30);')
        o.append(f'    for (int k = {m["start"]}; k < {m["limit"]}; k += {m["step"]}) '
                 f'{{ if (k < glo) glo = k; if (k > ghi) ghi = k; }}')
        o.append(f'    chk("{fn}", {m["lo"]}, {m["hi"]}, glo, ghi);')
        o.append('  }')
    o += ['  printf("checked %ld loops, %ld mismatches\\n", n, bad);', '  return bad != 0;', '}']
    open('loop_oracle.c', 'w').write('\n'.join(o) + '\n')


if __name__ == '__main__':
    main()
