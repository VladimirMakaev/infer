#!/usr/bin/env python3
"""Emit a C program that recomputes every quadruple natively and checks it against the
grid's expectations.

The whole grid rests on my Python model of C's semantics -- truncating division, the
sign of %, arithmetic >>, two's-complement bitwise. If that model is wrong the "true"
result R is wrong and a "silent" verdict means nothing. So the compiler is asked
instead: operands are volatile, so the four results are computed at run time by the
same machine inferbo is reasoning about, and the distinct sorted set is compared with
the one the generator baked in. A single MISMATCH invalidates that quadruple.
"""
import json
import re
import sys

SYM = {'add': '+', 'sub': '-', 'mul': '*', 'div': '/', 'mod': '%',
       'band': '&', 'bor': '|', 'bxor': '^', 'shl': '<<', 'shr': '>>'}
MIN64 = -(2**63)


def lit(v):
    return '(-9223372036854775807LL - 1)' if v == MIN64 else f'{v}LL'


TYPES = {
    'int64_t': ('PRId64',
                lambda v: '(-9223372036854775807LL - 1)' if v == -(2**63) else f'{v}LL'),
    'uint64_t': ('PRIu64', lambda v: f'{v}ULL'),
    'int32_t': ('PRId32',
                lambda v: '(-2147483647 - 1)' if v == -(2**31) else f'({v})'),
    'uint32_t': ('PRIu32', lambda v: f'{v}U'),
}


def main(manifest_path, out_path, ctype='int64_t'):
    pri, lit = TYPES[ctype]
    man = json.load(open(manifest_path))
    # Rebuild each quadruple's expected distinct result set from the probes that name it.
    quads = {}
    for fn, m in man.items():
        if m['kind'] != 'probe':
            continue
        quads.setdefault(m['reach'], (m['op'], m['x'], m['y'], []))[3].append(m['r'])

    o = ['#include <stdio.h>', '#include <stdint.h>', '#include <inttypes.h>',
         'static long checks = 0, bad = 0;',
         'static void chk(const char *q, int n, const CTYPE *got, const CTYPE *want) {',
         '  checks++;',
         '  /* got is the four runtime results; compare its distinct sorted set to want. */',
         '  CTYPE d[4]; int nd = 0;',
         '  for (int i = 0; i < 4; i++) {',
         '    int seen = 0;',
         '    for (int j = 0; j < nd; j++) if (d[j] == got[i]) seen = 1;',
         '    if (!seen) d[nd++] = got[i];',
         '  }',
         '  for (int i = 0; i < nd; i++)',
         '    for (int j = i + 1; j < nd; j++)',
         '      if (d[j] < d[i]) { CTYPE t = d[i]; d[i] = d[j]; d[j] = t; }',
         '  if (nd != n) { bad++; printf("MISMATCH %s: %d distinct, expected %d\\n", q, nd, n); return; }',
         '  for (int i = 0; i < nd; i++)',
         '    if (d[i] != want[i]) { bad++;',
         '      printf("MISMATCH %s: [%d] = %" PRI ", expected %" PRI "\\n", q, i, d[i], want[i]);',
         '      return; }',
         '}',
         'int main(void) {']
    for reach, (op, (xl, xu), (yl, yu), rs) in sorted(quads.items()):
        rs = sorted(set(rs))
        s = SYM[op]
        o.append('  {')
        o.append(f'    volatile CTYPE xl = {lit(xl)}, xu = {lit(xu)}, '
                 f'yl = {lit(yl)}, yu = {lit(yu)};')
        o.append(f'    CTYPE g[4] = {{ xl {s} yl, xl {s} yu, xu {s} yl, xu {s} yu }};')
        o.append(f'    const CTYPE w[] = {{ {", ".join(lit(r) for r in rs)} }};')
        o.append(f'    chk("{reach}", {len(rs)}, g, w);')
        o.append('  }')
    o += ['  printf("checked %ld quadruples, %ld mismatches\\n", checks, bad);',
          '  return bad != 0;', '}']
    text = '\n'.join(o) + '\n'
    text = re.sub(r'\bPRI\b', pri, text.replace('CTYPE', ctype))
    open(out_path, 'w').write(text)
    print(f'{out_path}: {len(quads)} quadruples')


if __name__ == '__main__':
    main(*sys.argv[1:])
