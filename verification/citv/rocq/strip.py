#!/usr/bin/env python3
"""Remove the PRINTING-ONLY definitions from the generated Rocq files.

Why: the RocqOfOCaml support library has no Format module at all (proofs/Basics.v defines
Module Stdlib with comparisons, strings, lists - no Format, no CamlinternalFormatBasics), so
every definition that formats output is untypable in Rocq.  None of them is part of the
abstract domain's soundness, so they are removed rather than shimmed.

Every removal is asserted: if the generated text ever stops matching, this fails loudly
instead of silently stripping something else.  It prints the exact line count removed.
"""
import sys, pathlib

if len(sys.argv) != 3:
    sys.exit('usage: strip.py <raw-translation-dir> <output-dir>')
V = pathlib.Path(sys.argv[1])
OUT = pathlib.Path(sys.argv[2])
OUT.mkdir(parents=True, exist_ok=True)

removed = {}


def cut(text, start, end, name, replacement=''):
    """Remove the unique block from the line beginning with `start` up to (not incl.) `end`."""
    i = text.find(start)
    assert i != -1, f'{name}: start not found'
    assert text.find(start, i + 1) == -1, f'{name}: start is not unique'
    if end is None:
        j = len(text)
    else:
        j = text.find(end, i)
        assert j != -1, f'{name}: end not found'
    removed[name] = text[i:j].count('\n')
    return text[:i] + replacement + text[j:]


# ---- PulseCItv.v -------------------------------------------------------------------------
t = (V / 'PulseCItv.v').read_text()
# The translator emits no Require for the modules it references, so Rocq cannot find them.
# Added here rather than by hand so that a regenerated file is byte-identical to the
# committed one.
REQ_ANCHOR = 'Require Import RocqOfOCaml.Settings.\n'
assert t.count(REQ_ANCHOR) == 1, 'PulseCItv: Settings import not unique'
t = t.replace(REQ_ANCHOR,
              REQ_ANCHOR + 'Require Import IStd.\nRequire Import IntLit.\n'
                           'Require Import Binop.\nRequire Import Unop.\n')
t = cut(t, 'Module F := Stdlib.Format.\n\n', 'Module L := Logging.', 'PulseCItv: module alias F')
t = cut(t, 'Module L := Logging.\n\n', 'Module Bound.', 'PulseCItv: module alias L')
t = cut(t, '  Definition pp (fmt : Stdlib.Format.formatter)', '  Definition equal (b1 : t)',
        'PulseCItv: Bound.pp')
t = cut(t, '      L.die IStd.InternalError\n', '    end.\n  \n  Definition minus',
        'PulseCItv: the L.die arm of Bound.add',
        '      (* was: L.die InternalError "cannot add %a and %a" pp b1 pp b2 *)\n'
        '      axiom\n')
t = cut(t, 'Definition pp (fmt : F.formatter)', 'Definition is_equal_to_zero',
        'PulseCItv: top-level pp')
(OUT / 'PulseCItv.v').write_text(t)

# ---- IntLit.v ----------------------------------------------------------------------------
t = (V / 'IntLit.v').read_text()
# Serialisation, like printing: the real IntLit writes yojson_of_t by hand over Z.to_string,
# which the translator cannot render (it emits its type-error marker).  Nothing in the
# abstract domain's soundness goes through it.
t = cut(t, 'Definition yojson_of_t', 'Definition join_signedness', 'IntLit: yojson_of_t')
t = cut(t, 'Definition pp (fmt : Stdlib.Format.formatter)', None, 'IntLit: pp')
# NO hand-added equal any more.  An earlier version of this script appended a STRUCTURAL
# equality here, on the theory that it was what [@@deriving equal] generates.  It is not what
# the real module exports: IntLit.ml:172 shadows the derived one with [let equal = eq], so
# the exported equality is area-based and ignores pointerness.  The stub now writes that
# definition out, so the translator emits it and there is nothing to patch in.
(OUT / 'IntLit.v').write_text(t)

# ---- IStd.v ------------------------------------------------------------------------------
# The two `include` lines in the IStd stub are the translator's two not_supported sites; it
# renders each as an empty module plus an Include of it.  Harmless but noise, and Rocq warns.
t = (V / 'IStd.v').read_text()
EMPTY_INC = ('Module nameless_include.\n\nEnd nameless_include.\n\n'
             'Include nameless_include.\n\n')
n = t.count(EMPTY_INC)
assert n == 2, f'IStd: expected 2 empty nameless_include blocks, found {n}'
t = t.replace(EMPTY_INC, '')
removed['IStd: 2 empty nameless_include blocks'] = 2 * EMPTY_INC.count('\n')
(OUT / 'IStd.v').write_text(t)

# ---- the rest are copied verbatim --------------------------------------------------------
for f in ('Binop.v', 'Unop.v'):
    (OUT / f).write_text((V / f).read_text())

for k, v in removed.items():
    print(f'{v:4d} lines  {k}')
print(f'{sum(removed.values()):4d} lines removed in total')
print('Logging.v dropped entirely (its only definition is die, which needs Format).')
