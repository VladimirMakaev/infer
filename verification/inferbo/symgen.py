#!/usr/bin/env python3
"""Symbolic grid: buffer sizes and indices that stay SYMBOLIC in the parameter.

Everything probed so far had concrete operands, so every Bound was a constant and the
symbol machinery -- Bound.plus_l / plus_u on symbolic terms, and the min/max pairs over
symbols -- never ran. That machinery is the ranked #1 target. Here the size and the
index are both expressions in a parameter n:

    void f(int64_t n) {
      if (n < LO || n > HI) return;       /* n is now a bounded symbol */
      char *buf = malloc(SIZE(n));
      buf[IDX(n)] = 1;
    }

GROUND TRUTH IS ENUMERATED, not estimated. n is guarded to a small range, so for each
configuration every admissible n is tried and the access is classified exactly: if some
admissible n makes IDX(n) negative or >= SIZE(n), the program really can go out of
bounds and inferbo must report. Silence on such a configuration is a missed buffer
overrun. If no admissible n does, a report is a false positive -- counted, but never
presented as a bug.

Each configuration gets a reach_ sibling whose access is unconditionally out of bounds
(buf[-1] on a non-empty buffer), so a probe's silence cannot be the function being
skipped or malloc not being modelled.
"""
import json

# (label, C text, python function). SIZE must be positive for every admissible n.
SIZES = [
    ('n', 'n', lambda n: n),
    ('n+1', 'n + 1', lambda n: n + 1),
    ('n-1', 'n - 1', lambda n: n - 1),
    ('2n', '2 * n', lambda n: 2 * n),
    ('n/2', 'n / 2', lambda n: n // 2),
    ('n+8', 'n + 8', lambda n: n + 8),
]
IDXS = [
    ('0', '0', lambda n: 0),
    ('n-1', 'n - 1', lambda n: n - 1),
    ('n', 'n', lambda n: n),
    ('n+1', 'n + 1', lambda n: n + 1),
    ('2n-1', '2 * n - 1', lambda n: 2 * n - 1),
    ('n/2', 'n / 2', lambda n: n // 2),
]
# (label, lo, hi) -- n is pruned into [lo,hi], small enough to enumerate exhaustively.
GUARDS = [('1_64', 1, 64), ('2_8', 2, 8), ('16_16', 16, 16)]


def main():
    out = ['#include <stdint.h>', '#include <stdlib.h>', '']
    man = {}
    qi = 0
    for gl, lo, hi in GUARDS:
        for sl, sc, sf in SIZES:
            ns = list(range(lo, hi + 1))
            if any(sf(n) <= 0 for n in ns):
                continue  # a non-positive malloc is a different question; skip
            for il, ic, idxf in IDXS:
                qi += 1
                base = f'S{qi}'
                # exhaustive ground truth over every admissible n
                bad = [n for n in ns if idxf(n) < 0 or idxf(n) >= sf(n)]
                guard = (f'  if (n < {lo} || n > {hi}) return 0;\n'
                         f'  char *buf = (char *)malloc({sc});\n'
                         f'  if (!buf) return 0;\n')
                out.append(f'/* guard n in [{lo},{hi}], size={sl}, idx={il}: '
                           + (f'OUT OF BOUNDS for n in {bad[:6]}'
                              f'{"..." if len(bad) > 6 else ""} */'
                              if bad else 'always in bounds */'))
                out.append(f'int probe_{base}(int64_t n) {{\n{guard}'
                           f'  buf[{ic}] = 1;\n  int r = buf[0];\n  free(buf);\n'
                           f'  return r;\n}}\n')
                out.append(f'int reach_{base}(int64_t n) {{\n{guard}'
                           f'  buf[-1] = 1;\n  int r = buf[0];\n  free(buf);\n'
                           f'  return r;\n}}\n')
                man[f'probe_{base}'] = {
                    'kind': 'probe', 'guard': gl, 'lo': lo, 'hi': hi, 'size': sl,
                    'idx': il, 'oob': bool(bad), 'bad_n': bad[:8],
                    'reach': f'reach_{base}'}
                man[f'reach_{base}'] = {'kind': 'reach'}
    open('grid_sym.c', 'w').write('\n'.join(out))
    json.dump(man, open('manifest_sym.json', 'w'), indent=1)
    p = [m for m in man.values() if m['kind'] == 'probe']
    print(f'grid_sym.c: {len(p)} configurations '
          f'({sum(1 for m in p if m["oob"])} reachably out of bounds, '
          f'{sum(1 for m in p if not m["oob"])} always in bounds)')


if __name__ == '__main__':
    main()
