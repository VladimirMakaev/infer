#!/usr/bin/env python3
"""Generate a soundness grid for inferbo's interval arithmetic.

THE DETECTOR, calibrated in calib.c before any of this was generated:

    int64_t x = c ? XL : XU;        /* inferbo joins to the interval [XL,XU] */
    int64_t y = d ? YL : YU;
    int64_t v = x OP y;
    if (v == R) { int a[1]; a[1] = 1; }   /* R is a CONCRETELY ACHIEVABLE result */

  a[1] on int a[1] is an unconditional overrun, so the only question is whether the
  branch is live. inferbo prunes `v == R` to bottom exactly when its interval for v
  excludes R (calib c3 is silent, c2 and c4 report). R is achievable by construction,
  so SILENCE means inferbo proved a safety that does not hold: an unsoundness.

  Every quadruple also gets a reach_ sibling with the same operands and the same op
  but an UNCONDITIONAL access. If reach_ reports and probe_ is silent, the guard is
  the only difference and the verdict cannot be blamed on the function being skipped,
  the op being unsupported, or the access shape.

UNDEFINED BEHAVIOUR IS EXCLUDED, not merely avoided: a quadruple is dropped unless
ALL FOUR operand combinations are well defined in C. A C program with a UB path
entitles the analyser to anything, so a finding on one would be arguable. These are
not. Signed >> of a negative value is implementation-defined rather than undefined
(clang and gcc both document an arithmetic shift), so it is generated but tagged, and
reported separately from the airtight set.
"""
import hashlib
import json
import sys

MIN64, MAX64 = -(2**63), 2**63 - 1
U64 = 2**64


def fits(v):
    return MIN64 <= v <= MAX64


def cdiv(a, b):
    """C truncates toward zero; Python floors. Done in exact integers, not floats."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def cmod(a, b):
    return a - cdiv(a, b) * b


# Each op: name, C spelling, exact evaluator, and the predicate saying when the C
# expression is well defined for a given operand pair.
def _ok_add(a, b):
    return fits(a + b)


def _ok_sub(a, b):
    return fits(a - b)


def _ok_mul(a, b):
    return fits(a * b)


def _ok_divmod(a, b):
    return b != 0 and not (a == MIN64 and b == -1)


def _ok_shl(a, b):
    # UB if the count is out of range, if the value is negative (C99 6.5.7p4), or if
    # the result is not representable.
    return 0 <= b < 64 and a >= 0 and fits(a << b)


def _ok_shr(a, b):
    return 0 <= b < 64


SIGNED_OPS = [
    ('add', '+', lambda a, b: a + b, _ok_add, False),
    ('sub', '-', lambda a, b: a - b, _ok_sub, False),
    ('mul', '*', lambda a, b: a * b, _ok_mul, False),
    ('div', '/', cdiv, _ok_divmod, False),
    ('mod', '%', cmod, _ok_divmod, False),
    ('band', '&', lambda a, b: a & b, lambda a, b: True, False),
    ('bor', '|', lambda a, b: a | b, lambda a, b: True, False),
    ('bxor', '^', lambda a, b: a ^ b, lambda a, b: True, False),
    ('shl', '<<', lambda a, b: a << b, _ok_shl, False),
    # arithmetic shift: Python >> floors, which is what clang/gcc emit.
    ('shr', '>>', lambda a, b: a >> b, _ok_shr, True),
]

# Intervals chosen for sign combinations, zero, +-1, powers of two and the 2^31 /
# 2^32 / 2^63 boundaries where saturation lives.
SIGNED_IVS = [
    (0, 0), (1, 1), (-1, -1), (0, 1), (-1, 0), (-1, 1),
    (2, 3), (-3, -2), (-3, 3), (7, 8), (-8, -7), (8, 8), (64, 64), (63, 65),
    (2**31 - 1, 2**31), (-(2**31), -(2**31) + 1), (2**32 - 1, 2**32),
    (MAX64 - 1, MAX64), (MIN64, MIN64 + 1), (MIN64, MAX64),
    (0, MAX64), (MIN64, 0), (0, 63), (1, 64),
]


def lit_s(v):
    """A C literal for an int64_t. INT64_MIN has no literal form: -9223372036854775808
    parses as a unary minus on an out-of-range constant, so build it by arithmetic."""
    if v == MIN64:
        return '(-9223372036854775807LL - 1)'
    return f'{v}LL'


def lit_u(v):
    return f'{v}ULL'


# UNSIGNED. Every arithmetic op wraps mod 2^64 and that wrapping is DEFINED by the
# standard, so unlike the signed grid nothing has to be dropped near the boundary.
# This is the grid that can actually ask about saturation at 2^63 and 2^64.
def _u(v):
    return v % U64


UNSIGNED_OPS = [
    ('add', '+', lambda a, b: _u(a + b), lambda a, b: True, False),
    ('sub', '-', lambda a, b: _u(a - b), lambda a, b: True, False),
    ('mul', '*', lambda a, b: _u(a * b), lambda a, b: True, False),
    ('div', '/', lambda a, b: a // b, lambda a, b: b != 0, False),
    ('mod', '%', lambda a, b: a % b, lambda a, b: b != 0, False),
    ('band', '&', lambda a, b: a & b, lambda a, b: True, False),
    ('bor', '|', lambda a, b: a | b, lambda a, b: True, False),
    ('bxor', '^', lambda a, b: a ^ b, lambda a, b: True, False),
    ('shl', '<<', lambda a, b: _u(a << b), lambda a, b: b < 64, False),
    ('shr', '>>', lambda a, b: a >> b, lambda a, b: b < 64, False),
]

UNSIGNED_IVS = [
    (0, 0), (1, 1), (0, 1), (2, 3), (7, 8), (8, 8), (64, 64), (63, 65),
    (0, 63), (1, 64), (2**31 - 1, 2**31), (2**32 - 1, 2**32),
    (2**63 - 1, 2**63), (U64 - 2, U64 - 1), (0, U64 - 1), (0, 2**32),
    (2**63, U64 - 1), (U64 - 1, U64 - 1),
]


def keep_in_sample(base, permille, seed, always=()):
    """ALWAYS-INCLUDED CASES COME FIRST, and that is not an optimisation.

    The pinned known-unsound points are precisely the cases most worth watching, so
    letting a random sample decide whether to look at them is backwards. Measured: after
    the baseline was regenerated against the right engine, all 34 remaining pins live in
    the u32 and unsigned arms, and the 2% sample of u32 contained NONE of its 21 -- so the
    pre-push tier's set comparison could neither notice a pin going sound nor be seeded
    with a meaningful violation. Forcing the pinned quadruples in costs 34 cases.
    """
    if base in always:
        return True
    return _keep_in_sample_random(base, permille, seed)


def _keep_in_sample_random(base, permille, seed):
    """Deterministic membership of the pre-push sample.

    Hash-based rather than RNG-based on purpose: the sample must be reproducible from
    (seed, permille) alone, with no dependence on how many quadruples were skipped
    before this one. A random.Random() walked by the loop would reshuffle the whole
    sample whenever an operand set or a UB filter changed, so a pre-push failure could
    not be reproduced from the seed printed in the trailer.
    """
    if permille >= 1000:
        return True
    h = hashlib.sha256(f'{seed}:{base}'.encode()).digest()
    return int.from_bytes(h[:4], 'big') % 1000 < permille


def gen(ops, ivs, path_c, path_manifest, tag, ctype='int64_t', lit=lit_s,
        in_range=fits, permille=1000, seed=0, always=()):
    out = ['#include <stdint.h>', 'int nondet(void);', '']
    manifest = {}
    nq = 0
    for opname, csym, ev, ok, impl_defined in ops:
        for qi, ((xl, xu), (yl, yu)) in enumerate(
            (a, b) for a in ivs for b in ivs
        ):
            combos = [(xl, yl), (xl, yu), (xu, yl), (xu, yu)]
            if not all(ok(a, b) for a, b in combos):
                continue  # a UB path anywhere in the function voids the claim
            results = sorted({ev(a, b) for a, b in combos})
            if not all(in_range(r) for r in results):
                continue
            base = f'{tag}_{opname}_{qi}'
            # Sampling happens AFTER the UB and range filters, so the sample is a subset
            # of the cases the full grid would have emitted and the two tiers cannot
            # disagree about which quadruples are legitimate.
            if not keep_in_sample(base, permille, seed, always):
                continue
            nq += 1
            setup = (f'  {ctype} x = c ? {lit(xl)} : {lit(xu)};\n'
                     f'  {ctype} y = d ? {lit(yl)} : {lit(yu)};\n'
                     f'  {ctype} v = x {csym} y;\n')
            out.append(f'int reach_{base}(int c, int d) {{\n{setup}'
                       f'  int a[1]; a[1] = 1;\n  return a[0] + (int)v;\n}}\n')
            manifest[f'reach_{base}'] = {
                'kind': 'reach', 'op': opname, 'x': [xl, xu], 'y': [yl, yu],
                'impl_defined': impl_defined}
            for ri, r in enumerate(results):
                fn = f'probe_{base}_{ri}'
                out.append(f'int {fn}(int c, int d) {{\n{setup}'
                           f'  if (v == {lit(r)}) {{ int a[1]; a[1] = 1; return a[0]; }}\n'
                           f'  return 0;\n}}\n')
                manifest[fn] = {
                    'kind': 'probe', 'op': opname, 'x': [xl, xu], 'y': [yl, yu],
                    'r': r, 'reach': f'reach_{base}', 'impl_defined': impl_defined}
    open(path_c, 'w').write('\n'.join(out))
    json.dump(manifest, open(path_manifest, 'w'), indent=1)
    probes = sum(1 for m in manifest.values() if m['kind'] == 'probe')
    print(f'{path_c}: {nq} quadruples, {probes} probes, {len(manifest) - probes} reach '
          f'controls, {len(manifest)} functions')


# ---- 32-bit. itv.ml works over unbounded Z and the type width only enters at
# assignment and pruning, so a width-specific defect would show up here and nowhere
# else. The shift threshold in shiftrt is the literal 64 regardless of operand width,
# which is the specific reason to look.
MIN32, MAX32 = -(2**31), 2**31 - 1
U32 = 2**32


def _fits32(v):
    return MIN32 <= v <= MAX32


def _ok32(f):
    """Lift a 64-bit definedness test to 32 bits: same rules, narrower range."""
    return f


S32_OPS = [
    ('add', '+', lambda a, b: a + b, lambda a, b: _fits32(a + b), False),
    ('sub', '-', lambda a, b: a - b, lambda a, b: _fits32(a - b), False),
    ('mul', '*', lambda a, b: a * b, lambda a, b: _fits32(a * b), False),
    ('div', '/', cdiv, lambda a, b: b != 0 and not (a == MIN32 and b == -1), False),
    ('mod', '%', cmod, lambda a, b: b != 0 and not (a == MIN32 and b == -1), False),
    ('band', '&', lambda a, b: a & b, lambda a, b: True, False),
    ('bor', '|', lambda a, b: a | b, lambda a, b: True, False),
    ('bxor', '^', lambda a, b: a ^ b, lambda a, b: True, False),
    ('shl', '<<', lambda a, b: a << b,
     lambda a, b: 0 <= b < 32 and a >= 0 and _fits32(a << b), False),
    ('shr', '>>', lambda a, b: a >> b, lambda a, b: 0 <= b < 32, True),
]

S32_IVS = [
    (0, 0), (1, 1), (-1, -1), (0, 1), (-1, 0), (-1, 1), (2, 3), (-3, -2), (-3, 3),
    (7, 8), (-8, -7), (31, 31), (30, 32), (2**15 - 1, 2**15), (-(2**15), -(2**15) + 1),
    (MAX32 - 1, MAX32), (MIN32, MIN32 + 1), (MIN32, MAX32), (0, MAX32), (MIN32, 0),
    (0, 31),
]

U32_OPS = [
    ('add', '+', lambda a, b: (a + b) % U32, lambda a, b: True, False),
    ('sub', '-', lambda a, b: (a - b) % U32, lambda a, b: True, False),
    ('mul', '*', lambda a, b: (a * b) % U32, lambda a, b: True, False),
    ('div', '/', lambda a, b: a // b, lambda a, b: b != 0, False),
    ('mod', '%', lambda a, b: a % b, lambda a, b: b != 0, False),
    ('band', '&', lambda a, b: a & b, lambda a, b: True, False),
    ('bor', '|', lambda a, b: a | b, lambda a, b: True, False),
    ('bxor', '^', lambda a, b: a ^ b, lambda a, b: True, False),
    ('shl', '<<', lambda a, b: (a << b) % U32, lambda a, b: b < 32, False),
    ('shr', '>>', lambda a, b: a >> b, lambda a, b: b < 32, False),
]

U32_IVS = [
    (0, 0), (1, 1), (0, 1), (2, 3), (7, 8), (31, 31), (30, 32), (0, 31),
    (2**15 - 1, 2**15), (2**31 - 1, 2**31), (U32 - 2, U32 - 1), (0, U32 - 1),
    (2**31, U32 - 1), (U32 - 1, U32 - 1),
]


def lit_s32(v):
    # -2147483648 is unary minus on a constant that does not fit int, so it has type
    # long. Spelling it as an arithmetic expression keeps the literal int-typed and
    # the whole probe in 32-bit arithmetic, which is the point of this grid.
    return '(-2147483647 - 1)' if v == MIN32 else f'({v})'


def lit_u32(v):
    return f'{v}U'


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('which', choices=['signed', 'unsigned', 's32', 'u32'])
    ap.add_argument('--sample', type=int, default=1000,
                    help='per-mille of quadruples to keep (1000 = the full grid)')
    ap.add_argument('--seed', type=int, default=0)
    ap.add_argument('--outdir', default='.')
    ap.add_argument('--always-from', default=None,
                    help='JSON {arm: [probe names]}; those quadruples are always included '
                         'in the sample regardless of the seed')
    a = ap.parse_args()
    import os
    j = lambda p: os.path.join(a.outdir, p)
    always = set()
    if a.always_from:
        # ASKED FOR AND UNREADABLE IS AN ERROR, NOT A SHRUG. Skipping a missing watch list
        # here is how the caller's coverage disappears without anything saying so: the
        # grid still generates, still passes, and simply stops containing the cases it was
        # told to always include. A caller that does not want a watch list omits the flag.
        if not os.path.exists(a.always_from):
            raise SystemExit(
                f'--always-from {a.always_from}: no such file. Refusing rather than '
                f'generating a grid that silently omits the cases it was asked to always '
                f'include.')
        import re as _re
        try:
            doc = json.load(open(a.always_from))
        except (ValueError, OSError) as e:
            raise SystemExit(f'--always-from {a.always_from}: unreadable ({e}). Refusing '
                             f'rather than generating a grid with less coverage than asked.')
        if not isinstance(doc, dict):
            raise SystemExit(f'--always-from {a.always_from}: expected a JSON object '
                             f'{{arm: [names]}}, got {type(doc).__name__}')
        names = doc.get(a.which, [])
        # Accepts either quadruple bases (<tag>_<op>_<qi>, as always_sample.json holds)
        # or full probe names (probe_<base>_<ri>), so the file can be regenerated from
        # either side without a silent mismatch.
        for n in names:
            m = _re.match(r'probe_(.+)_\d+$', n)
            always.add(m.group(1) if m else n)
    kw = dict(permille=a.sample, seed=a.seed, always=always)
    if a.which == 'signed':
        gen(SIGNED_OPS, SIGNED_IVS, j('grid_signed.c'), j('manifest_signed.json'), 's', **kw)
    elif a.which == 'unsigned':
        gen(UNSIGNED_OPS, UNSIGNED_IVS, j('grid_unsigned.c'), j('manifest_unsigned.json'),
            'u', ctype='uint64_t', lit=lit_u, in_range=lambda v: 0 <= v < U64, **kw)
    elif a.which == 's32':
        gen(S32_OPS, S32_IVS, j('grid_s32.c'), j('manifest_s32.json'), 'i',
            ctype='int32_t', lit=lit_s32, in_range=_fits32, **kw)
    elif a.which == 'u32':
        gen(U32_OPS, U32_IVS, j('grid_u32.c'), j('manifest_u32.json'), 'w',
            ctype='uint32_t', lit=lit_u32, in_range=lambda v: 0 <= v < U32, **kw)
