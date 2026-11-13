# An unbounded proof for the ordered-comparison fragment

The check one directory up runs `infer/src/pulse/PulseCItv.ml` itself over a window of endpoints
and compares it with an independent reading. A clean run there is evidence over stated bounds,
not a proof: a defect that only shows up at an endpoint outside the window, or only at an
infinity, survives it.

This is the other half. It restates one branch of that module in Lean and proves it sound for
**all** integers and **all** bounds, infinities included. The price is that it is a model rather
than the code: nothing here reads `PulseCItv.ml`, so the table below is the load-bearing part
and is what a reviewer has to check.

## Running it

```
./verification/citv/lean/run.sh
```

That builds the proofs from clean, checks what they rest on, attacks the freshness guard, and
then breaks the development six ways and requires each break to be refused. `--proofs` stops
after the freshness check.

`./verification/citv/lean/check-axioms.sh` is the acceptance criterion on its own.

### What it needs

Lean, at the version pinned in `lean-toolchain`, installed by `elan` into the user's own home —
no root:

```
curl -sSf https://elan.lean-lang.org/elan-init.sh | sh
elan toolchain install "$(cat verification/citv/lean/lean-toolchain)"
```

The build is offline after that: `lakefile.toml` has no `require`, `lake-manifest.json` lists
`"packages": []`, and the proofs use the Lean standard library only. There is no Mathlib.

Infer's build does not depend on Lean, and never asks for it. This directory is outside the dune
root (`infer/`), so `make` and `dune build` do not see it; `.lake/` is build output and is not
committed.

## Why the axiom check and not just a green build

`lake build` succeeding means every proof checked, but it does not mean every proof proved what
you think. A proof finished with `sorry` builds, with a warning that is easy to lose in output.
A proof that reaches for an extra assumption builds too.

`Citv/Axioms.lean` asks `#print axioms` of every result in the development, and
`check-axioms.sh` reads the answers back and fails on anything outside

> `propext`, `Quot.sound`, `Classical.choice`

which are the three the Lean kernel is normally trusted with. It also fails on any `sorry`.

`memBetween_succ/finish-with-sorry` in `mutants.txt` is that path, deliberately taken: it builds
green and the axiom check is what catches it.

### and why counting the reports is not enough either

Lake **replays** the reports of a module it has not had to rebuild. An incremental build of this
project prints `Replayed Citv.Axioms` and then every one of the reports again, out of the cache.
So seeing a report for each theorem says nothing at all about whether any theorem was checked
just now, and a checker that cleaned first but never looked at whether the clean worked would
report on a stale tree in exactly the same words it uses for a fresh one.

Three separate things establish freshness, and all three have to hold:

1. `lake clean` has to exit zero — its status is checked, not assumed;
2. the compiled `Citv/Axioms.olean` has to be **gone** afterwards, which is evidence rather than
   a promise;
3. the build has to report `Built Citv.Axioms`, and is refused if it says `Replayed`.

`run.sh` attacks that guard before it runs any mutant: it warms the build, forces the clean to
fail (`CITV_CLEAN=false`, which is what the override exists for), and requires the checker to
refuse. The count of reports against the number of `#print axioms` commands is still checked,
but as a check that nothing was dropped from the list — not as evidence of freshness.

## Correspondence with the OCaml

This is the part that cannot be checked by a machine. Line numbers are against
`infer/src/pulse/PulseCItv.ml` as of the commit that adds this directory.

| OCaml | line | Lean | kept the same |
|---|---|---|---|
| `Bound.t` | 13 | `Citv.Bound` | the three shapes, in order |
| `Bound.le` | 34 | `Bound.le` | the arm order, so `le MinusInfinity MinusInfinity` and `le PlusInfinity PlusInfinity` are both true |
| `Bound.lt` | 44 | `Bound.lt` | likewise; `lt b b` is true at both infinities, which is not what one would write from scratch |
| `Bound.min`, `Bound.max` | 74, 76 | `Bound.min`, `Bound.max` | both defined through `le`, so they inherit its behaviour at the infinities |
| `Bound.add_int` | 78 | `Bound.addInt` | the infinities are left alone |
| `Between (lo, hi)` | 115 | `memBetween lo hi` | read as a set: `allowsBelow lo x ∧ allowsAbove hi x` |
| `abduction_result` | 212 | `Abduced` | the refinements are a pair of bounds, not `t option`; see the note below |
| `abduce_le`, `Between`/`Between` branch | 352 | `abduceLe` | the guard and the refinement, with the two `let`s written out so that both uses are visibly the same expression |
| `abduce_lt` | 399 | `abduceLt`, `Abduced.shiftLeftBack` | shift the left operand up by one, call `abduce_le`, shift its left refinement back |
| `flip_abduced` | 215 | `Abduced.flip` | the two refinements change places; an impossibility stays one |
| `abduce_binop_constraints`, ordered rows | 407 | `abduceCmp` | `Ge` and `Gt` swap the operands **and flip the answer back**, as there |

Three deliberate differences, each of which narrows what is claimed rather than widening it:

- **`IntLit.t` is `Int`.** The real one carries a signedness tag and a pointerness tag and
  orders a negative signed value and the same bits read as unsigned differently. The proofs are
  about mathematical integers, so they say nothing about the tagged domain, and nothing about
  machine width. This is the same fragment the bounded check covers, stated in the same place.
- **`Ge` and `Gt` are not simply the swapped helper.** The source swaps the operands and then
  passes the answer through `flip_abduced`, so the refinements come back in the caller's order.
  Leaving that step out keeps every unsoundness theorem true, because flipping is the identity
  on an impossibility, and still gets `[2,2] >= [1,1]` backwards. It is modelled, and
  `abduceCmp_ge_puts_the_refinements_in_the_callers_order` is the witness that says so;
  `abduceCmp/forget-to-flip-the-pair-back` in `mutants.txt` is the same omission as a mutant.
- **The refinements are bounds, not `t option`.** `abduce_le`'s `Between`/`Between` branch
  always answers `Unsatisfiable` or `Satisfiable (Some _, Some _)`, so the `None` refinements
  and `abduce_lt`'s `| result -> result` arm are unreachable in this fragment. `Abduced` has
  only the two cases that occur.
- **`Outside`, `abduce_eq`, `abduce_ne`, `negated` and the arithmetic are not here.** They are
  excluded by the hypotheses of the theorems, which are stated for `Between` operands and the
  four ordered operators, rather than assumed away. The bounded check covers all of them.

## What is proved

| theorem | statement |
|---|---|
| `abduceLe_unsat_sound` | if `abduce_le` says `x ≤ y` is impossible, no pair drawn from the two intervals satisfies it |
| `abduceLe_sat_sound` | if it tightens instead, every pair that satisfies `x ≤ y` is still inside both tightened intervals |
| `abduceLt_unsat_sound`, `abduceLt_sat_sound` | the same two for `<` |
| `abduceCmp_unsat_sound` | and for the dispatch, over `Le`, `Lt`, `Ge` and `Gt` |
| `abduceCmp_sat_sound` | and the tightening for all four, which is the statement the flip is needed for |
| `abduceCmp_ge_puts_the_refinements_in_the_callers_order` | the concrete witness: `[2,2] >= [1,1]` tightens the left operand to `{2}` and the right to `{1}`, in that order, which is what the source answers |
| `abduceLeEager_not_unsat_sound` | the **negation** of soundness for the variant that uses `le` where the module uses `lt`, so the development breaks if that mutant ever becomes equivalent to the module |

The first four rest on three small facts, which are where the content actually is:
`allowsAbove_mono` and `allowsBelow_mono` move a fact across `x ≤ y`, and `lt_excludes` says
that if `b1 < b2` then no integer is admitted from above by `b1` and from below by `b2` at once
— which is the whole of the impossibility test, and is the statement that has to be checked at
the infinities by hand rather than by `omega`.
