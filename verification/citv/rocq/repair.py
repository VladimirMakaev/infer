#!/usr/bin/env python3
"""The ONE hand repair the generated Rocq file needs to be accepted by Rocq 9.0.

rocq-of-ocaml emits `Fixpoint abduce_eq` because the OCaml is `let rec`, but the only
recursive call is `abduce_eq a2 a1` - arguments SWAPPED - so there is no structurally
decreasing argument and Rocq rejects it with "Cannot guess decreasing argument of fix".
The OCaml terminates because the swap can only fire once: it fires on (Outside, Between),
and the swapped call is (Between, Outside), which does not recurse.

The repair encodes exactly that argument: a measure of 1 on Outside and 0 on Between for the
FIRST argument, plus naming the wildcards in that one branch so the recursive call is on the
reconstructed values and the obligation is syntactic.  Semantics are unchanged.
"""
import pathlib, sys

if len(sys.argv) != 2:
    sys.exit('usage: repair.py <output-dir>')
p = pathlib.Path(sys.argv[1]) / 'PulseCItv.v'
t = p.read_text()

OLD_HEAD = 'Fixpoint abduce_eq (a1 : t) (a2 : t) : abduction_result :=\n'
NEW_HEAD = '''(* HAND REPAIR 1/2: Fixpoint -> Program Fixpoint with a measure.  See repair.py. *)
Definition eq_measure (a : t) : nat :=
  match a with
  | Unsafe.Outside _ _ => 1
  | Unsafe.Between _ _ => 0
  end.

Program Fixpoint abduce_eq (a1 : t) (a2 : t) {measure (eq_measure a1)}
  : abduction_result :=
'''
assert t.count(OLD_HEAD) == 1
t = t.replace(OLD_HEAD, NEW_HEAD)

OLD_REC = '  | (Unsafe.Outside _ _, Unsafe.Between _ _) => flip_abduced (abduce_eq a2 a1)\n'
NEW_REC = ('  (* HAND REPAIR 2/2: wildcards named so the swapped call is on reconstructed values *)\n'
           '  | (Unsafe.Outside o1 o2, Unsafe.Between b1 b2) =>\n'
           '    flip_abduced (abduce_eq (Unsafe.Between b1 b2) (Unsafe.Outside o1 o2))\n')
assert t.count(OLD_REC) == 1
t = t.replace(OLD_REC, NEW_REC)

# the obligation, right after the body of abduce_eq
ANCHOR = '\nDefinition intersection (a1 : t) (a2 : t)'
assert t.count(ANCHOR) == 1
t = t  # obligation discharged automatically by Program's default tactic

# Program Fixpoint needs these.  Anchored on the LAST import that strip.py adds, so the
# two scripts cannot fight over the same insertion point and reorder the header.
ANCHOR2 = 'Require Import Unop.\n'
assert t.count(ANCHOR2) == 1
t = t.replace(ANCHOR2, ANCHOR2 + 'From Stdlib Require Import Program.Wf.\nFrom Stdlib Require Import Lia.\n')

p.write_text(t)
print('repaired:', p)
