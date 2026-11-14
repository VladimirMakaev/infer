# Machine-checked soundness for the real interval code

The oracle two directories up runs `infer/src/pulse/PulseCItv.ml` over a window of endpoints and
compares it with an independent reading. The Lean development one directory up proves the
comparison fragment for all integers, but about a **hand-written model** whose correspondence to
the OCaml is argued in prose and a table.

This is the third corner. It proves soundness theorems about a **mechanical translation of the
OCaml itself**, produced by rocq-of-ocaml from the same `PulseCItv.ml` the analyser ships. Nobody
restates the algorithm: `v/PulseCItv.v` is generated, and the proofs are about it.

That buys the one thing the other two cannot give, and costs a different thing: the proofs are
about a *translation*, and nothing checks that the translation means the module except reading
it. The three are complementary. None of them alone says the OCaml is sound.

## Running it

```
./run.sh            # pins, proofs from clean, freshness attack, drift, mutants
./run.sh --proofs   # stop after the axiom check
```

Opt-in, not standing: it needs a second opam switch and a Rocq install, and is not wired into
any build. Infer does not depend on Rocq and never asks for it. This directory is outside the
dune root (`infer/`), so `make` and `dune build` do not see it; `.vo` files are build output and
are not committed.

### What it needs

A scratch opam root — never `~/.opam`, never Infer's switch. `setup.sh` holds the pins and
refuses the default root. The versions are **asserted**, not merely requested:

| component | pin |
|---|---|
| rocq-of-ocaml | master `1ac69a72` |
| OCaml | 5.4.1 |
| rocq-core | 9.0.1 |
| rocq-stdlib | 9.0.0 |
| zarith | 1.14 |

opam resolves `rocq-core` to 9.3.0/9.2.0 by default, which is **outside** rocq-of-ocaml's window
(`>= 9.0 & < 9.1`). A silently wrong Rocq is exactly the shape of defect this project keeps
paying for, so `run.sh` refuses to start unless every pin matches.

The translator is not vendored here: the generated `.v` and the proofs stand alone and are
reviewable without it. Set `CITV_TRANSLATOR` to a `rocqOfOCaml.exe` built from the pinned commit
to regenerate; `drift.sh` refuses rather than silently skipping if it is absent.

## What is proved

`gamma a x` is the concretisation: the IntLit literals the abstract value `a` admits.
Crucially the bounds are compared with **IntLit's own order**, not with the order on `Z` —
see "The value model" below for why that distinction is the whole point of this round.

| result | statement |
|---|---|
| `abduce_le_unsat_sound` | if `abduce_le` says `x <= y` is impossible, no pair drawn from the two concretisations satisfies it. All four operand shapes |
| `abduce_le_sat_sound` | if it tightens instead, **no solution is lost** — for three of the four shapes; the fourth is excluded by an explicit hypothesis and refuted below |
| `abduce_le_sat_is_unsound_on_outside_lhs` | a machine-checked **counterexample**: with `Outside` on the left and `Between` on the right, the refinement drops a value that satisfies the constraint |
| four `witness_*` results | the regression checks described under "The value model" |

### The arm that is refuted, and why

For `Outside(l1,u1) <= Between(_,u2)` the code refines the left operand to
`x <= min(l1+1, u2)`. Over `Z` that is merely imprecise: `x < l1` gives `x <= l1-1 <= l1+1`.
Over IntLit's order it is **unsound**. `Unsigned(-1)` is in area 3, the top area, and
`Unsigned(0)` is in area 2, so adding one moves the literal *down* and the refinement cuts
below a value that genuinely satisfies the constraint. The witness, computed on the
translated code:

```
l1 = u1 = Unsigned(-1)      a2 = [Unsigned(-2), Unsigned(-2)]      x = y = Unsigned(-2)
abduce_le a1 a2 = Satisfiable (Some (Between MinusInfinity (Int Unsigned(0)))) None
IntLit.leq Unsigned(-2) Unsigned(0) = false
```

This is a statement about the algorithm over its own declared domain. It is **not** a
production bug claim: whether Pulse ever builds an `Outside` interval over negative
unsigned literals is a reachability question about IntLit's producers, and that has not
been investigated here.

### What the tagged order cost

The previous round of this directory also claimed `abduce_lt` (both halves) and
`abduce_binop_constraints` over the four ordered operators. Those theorems were proved
against a model in which `IntLit.t` was plain `Z`, and they are **withdrawn**, not merely
unported. The obstruction is concrete and is the same one as above:

- `abduce_lt` works by shifting the left operand up by one, calling `abduce_le`, and
  shifting the refinement back. Shifting is **not monotone** in IntLit's order:
  `Signed(5) <= Unsigned(-1)` holds, but `Signed(6) <= Unsigned(0)` does not.
- `add_int` can therefore turn a valid interval into an inverted one. In real OCaml
  `between` then raises `Assert_failure`; in the translation that assert is erased (see
  "What is NOT covered"), so the Rocq model silently continues.

`abduce_binop_constraints` rests on `abduce_lt`, so it goes with it. Restoring them means
either scoping them to an all-`Signed` invariant and justifying where in Infer that holds,
or doing the area analysis for the shift. Neither is attempted here, and nothing in this
directory claims them.

## Why the axiom check, and why it has no whitelist

`rocq compile` succeeding means the proofs checked, not that they proved what you think. The
check is `Print Assumptions` on every result, produced by a **freshly compiled** file.

The Lean check one directory up whitelists three axioms the kernel is normally trusted with.
**This one whitelists nothing.** The support library every generated file imports declares

> `Parameter assert : forall (A : Set), bool -> A`

which is flatly inconsistent — instantiate `A` with the empty type and you have `False` in two
lines. There is no sound subset of this library's parameters to allow, so the only acceptable
report is `Closed under the global context`.

Freshness is established positively, not promised: the clean's exit status is checked, the `.vo`
files have to be **gone** afterwards, and `run.sh` attacks the guard by forcing the clean to fail
(`CITV_CLEAN=false`) and requiring the checker to refuse. The theorem list is attested by **set
equality** against `expected/theorems.txt`, in both directions, so a deleted or renamed theorem
fails and a new one cannot quietly go unlisted.

## Drift

Three independent checks, each with its own message (`./drift.sh`):

1. `PulseCItv.ml`'s sha256 against the reviewed revision;
2. the translator's error set against `expected/translator-errors.txt`, failing if a site appears
   **or disappears**;
3. the regenerated `v/*.v` must be **byte-identical** to the committed files.

The third is the one that must not be dropped. A passing proof after a source change proves
nothing about the change: if the edited function is not named by any theorem, every proof still
compiles and the run says exactly what it said before. This was tested by perturbing
`is_non_pointer` — no theorem mentions it — and all three checks fired.

## What is NOT covered

Two limits, both of which a reviewer would otherwise find the hard way.

**The `[@@deriving]` attributes are outside the verified surface.** `ppx_compare` and
`ppx_yojson_conv` expand into the `.cmt`, which is why the file typechecks, but no `compare`,
`equal_t` or `yojson_of_t` appears anywhere in the generated output and the translator reports no
error for them. So "we verified PulseCItv" does **not** cover the derived comparison that
`PulseFormula` actually uses.

**The translator is unverified, and the axiom gate does not cover the asserts.** rocq-of-ocaml is
~27 MB of OCaml with no proof that the `.v` means the `.ml`; the mitigation is only that the
output reads 1:1 against the source, which it genuinely does. On the asserts specifically, the
gate is weaker than it looks, because the two shapes behave oppositely:

- *statement position* (`PulseCItv.ml:129` in `between`, `:134` in `outside`) becomes
  `let '_ := assert unit (...) in ...`, which Rocq's elaborator **discards**. The compiled
  `between` is literally `fun b1 b2 => Between b1 b2`. No axiom is left, so nothing can detect
  it — and the translation is at those two sites **not faithful**: the OCaml raises
  `Assert_failure` on a non-interval pair, the Rocq function returns happily. For the soundness
  direction this makes the theorems *stronger* (they quantify over inputs the OCaml rejects
  outright), so it costs nothing here, but it is a real infidelity.
- *value position* (`PulseCItv.ml:568` in `requires_integer_reasoning`) survives, and any theorem
  touching it reports `Basics.assert` and is refused. `requires_integer_reasoning` is therefore
  unprovable under this gate, which matters for any future target that needs it.

**The `private` abstraction is lost.** `module Unsafe : sig type t = private ... end` has its
signature dropped and the struct included, so in the `.v` anyone can build
`Unsafe.Between PlusInfinity MinusInfinity`. Harmless for these theorems — they quantify over the
whole type and never use the invariant — but any target that *needs* well-formedness must restate
it as an explicit predicate and carry it by hand.

## The value model

`PulseCItv.ml` reaches outside itself only for `IntLit`, `Binop.t`, `Unop.t`, `Option`,
`L.die` and `Format`. `stubs/` holds stand-ins for those.

`stubs/intLit.ml` mirrors `infer/src/IR/IntLit.ml`: the record
`{signedness; i; pointerness}`, `area` (1 for negative `Signed`, 2 for non-negative, 3 for
negative `Unsigned`), `compare_value` as lexicographic `(area, i)`, the comparisons derived
from it, and `lift1`/`lift2` with the `join_signedness` / `join_pointerness` tag joins.
Pointerness is carried and, as in the real module, never ordered on.

It also mirrors `IntLit.ml:172`, `let equal i1 i2 = eq i1 i2`, which **shadows** the `equal`
that `[@@deriving equal]` generates on the record. The exported equality is therefore
area-based, not structural: it ignores pointerness, and it identifies `signed(0)` with
`unsigned(0)`. An earlier version of this directory hand-wrote a structural equality and
claimed it was what the real module exports; `witness_equal_*` now pins all three cases.

**An earlier version of this directory got this wrong**, and the correction is the reason
for this round. It used `type t = int`, which the translator maps to Rocq's unbounded `Z`,
and the README claimed the deviation "is in the OCaml typechecking step only". That was
false. Dropping the tags changes the *order*, and therefore changes the answers:

| | real OCaml | the erased `Z` model |
|---|---|---|
| `IntLit.leq unsigned(-1) signed(0)` | `false` | `true` |
| `abduce_le (equal_to unsigned(-1)) (equal_to signed(0))` | `Unsatisfiable` | `Satisfiable` |
| `abduce_lt (equal_to signed(0)) (equal_to unsigned(-1))` | `Satisfiable` | `Unsatisfiable` |

All of those are now `witness_*` results in `v/Soundness.v`, checked by the pipeline, and
they agree with the real OCaml column.

### What remains a stand-in — the exhaustive list

Earlier versions of this section each claimed a single residual gap and were wrong twice.
This list is meant to be complete; if something is missing it is a defect in this file.

1. **The `i` field is OCaml `int`, not `Z.t`.** The translator renders it as Rocq's
   unbounded `Z`, so the *value* domain is faithful and the machine int exists only in the
   OCaml typechecking step that produces the `.cmt`. No effect on the ordering.
2. **`div` rounds differently.** The real module uses Zarith's `Z.( / )`, which truncates
   toward zero; the translated stub's `/` becomes Rocq's `Z.div`, which floors. They differ
   on negative operands: `div signed(-7) signed(2)` is `-3` in Infer and computes to `-4`
   here. Nothing proved in this directory uses `div` — it is reached only through
   `div_by_literal` and `binop`, outside the comparison fragment — but the divergence is
   real and would matter to anyone extending to the arithmetic.
3. **The stub exposes only the operations `PulseCItv.ml` consumes.** The rest of IntLit's
   API (`of_int64`, `of_string`, `null`, `isnull`, `isnegative`, `mul`, `rem`, the bitwise
   operations, the shifts, hashing, sexp, normalisation) is simply absent rather than
   modelled. That is a restriction of scope, not a difference in behaviour.
4. **Printing and serialisation are stripped**, not modelled: `pp` and `yojson_of_t`. The
   support library has no `Format`, and the real `yojson_of_t` goes through `Z.to_string`,
   which the translator cannot render.
5. **`to_int` has the wrong type.** The real one is `let to_int {i} = z_to_int_opt i`,
   returning `int option` with `None` on overflow; the stub returns the value directly.
   `PulseCItv.ml` never calls it — it uses only `add`, `div`, `eq`, `equal`, `geq`, `gt`,
   `iszero`, `leq`, `lt`, `max`, `min`, `minus_one`, `neg`, `one`, `pp`, `sub`, `zero` —
   so nothing reads this binding, which is why it survived three rounds of review. It
   would be cleaner to delete it than to model it; that is a one-line follow-up and is
   deliberately not folded into a text-only amend.

**Every other binding in the stub matches its counterpart in `infer/src/IR/IntLit.ml`.**
That is deliberately a quantifier and not a list: the previous version of this sentence
enumerated 22 names and silently omitted three of them (`join_signedness`,
`join_pointerness`, `compare_pointerness`), which is exactly how a correspondence claim
rots. The tag behaviour of `lift1` and `lift2` is pinned by mutants rather than by this
paragraph; the rest is a line-by-line reading of the two files, and the five numbered
entries above are the only exceptions.

The design called for driving this from M1's shipped `verification/citv/shims`. That works
for `PulseCItv.v` itself — the two translations differ in 5 cosmetic lines and no function
body — but **not** for the dependencies: M1's `IntLit` is Zarith-backed, Zarith is opaque to
the translator, and the resulting `IntLit.v` is nothing but `op_startypeminuserrorstar`
markers.

## Mutants

A green proof run means nothing unless an unsound module is rejected. `mutate.sh` breaks the
translated module four ways, one per theorem family, and each must **compile** (so that a
rejection means the proof talking, not a broken file) and then be **refused**:

| mutant | what it breaks |
|---|---|
| `abduce_le/weaken-the-unsat-guard` | `Bound.lt` → `Bound.le` in the refutation test |
| `abduce_le/over-tighten-the-lhs` | shifts the left refinement's lower bound up by one |
| `abduce_le/over-tighten-the-rhs` | the same on the right refinement |
| `abduce_le/over-tighten-the-between-outside-arm` | shifts that arm's lower bound one too far |
| `IntLit/lift1-drops-the-tags` | rebuilds a default `Signed`/`NotPointer` record instead of `{x with i}` |
| `IntLit/equal-ignores-the-area` | compares values only, so the two negative tags stop being distinguished |

The first four aim at `abduce_le`, which is what is still proved. The `abduce_lt` and
`flip_abduced` mutants of an earlier round are gone because the theorems they attacked are
withdrawn — no floor was lowered to make a run pass.

The last two aim at `IntLit.v`, which previously had no mutant at all: review found that a
`lift1` which silently dropped the tags compiled and left every proof passing. It does not
any more.
