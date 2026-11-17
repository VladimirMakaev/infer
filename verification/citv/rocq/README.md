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
./run.sh                   # pins, proofs from clean, freshness attack, drift, mutants
./run.sh --proofs          # stop after the axiom check
./concurrency-test.sh      # this checkout and a peer copy, at the same time, both green
```

Every script here keeps its scratch in a per-run `mktemp -d` with a cleanup trap, so a run in
one checkout cannot disturb a run in another. It used to keep it at fixed paths —
`/tmp/citv-expected.txt`, `/tmp/citv-axioms.log`, a whole mutant tree at `/tmp/citv-mut` — and
a reviewer running the proofs while the standing check fired had both runs writing the same
files. One run would delete and repopulate the mutant tree while the other was compiling
inside it, and the second run then reported *the mutant does not compile*: a statement about
interference, printed as a statement about the mutant. `CITV_KEEP_TMP=1` keeps the directory
when you need to look inside it.

Two runs in the **same** directory are still unsafe, and no temp path can change that: the
proofs are compiled in `v/`, which `check-axioms.sh` cleans first, so one checkout cannot host
two runs. `concurrency-test.sh` therefore pits this directory against an independent copy,
which is the case that actually arose.

Opt-in, not standing: it needs a second opam switch and a Rocq install, and is not wired into
any build. Infer does not depend on Rocq and never asks for it. This directory is outside the
dune root (`infer/`), so `make` and `dune build` do not see it; `.vo` files are build output and
are not committed.

### What it needs

A dedicated opam root — never `~/.opam`, never Infer's switch. `setup.sh` holds the pins and
the layout, and refuses the default root. Build the whole toolchain with one command:

```
../install-toolchain.sh            # builds whatever is missing, resumable
../install-toolchain.sh --check    # reports what is present, builds nothing
```

It lands in `$CITV_HOME`, default `~/.citv-rocq`, and that default matters more than it
looks. The toolchain first lived in the scratch directory of whoever built it, which exists
on exactly one machine; everywhere else the standing check reported "toolchain not found",
and that is *exit 2, a check that could not be carried out* — a result nobody can act on and
that a reader learns to ignore. Every path below is derived from `CITV_HOME`, so a second
install (a different Rocq, a bisect) is one variable away and needs no edit to any script.

| under `$CITV_HOME` | what it is |
|---|---|
| `bin/opam` | opam 2.3.0, sha256-pinned. The home carries its own so a shell with no FV environment can still run the check; `setup.sh` prepends `bin/` to `PATH` |
| `prefix/` | GNU MP 6.3.0, built user-locally. Only needed where the host has `libgmp.so` but no `gmp.h`, which is the usual state of a box without `gmp-devel`. zarith needs both, and `--no-depexts` is correct precisely because this supplies what `gmp-devel` would |
| `opamroot/coq541` | OCaml 5.4.1, Rocq, zarith, and the translator's own dependencies |
| `opamroot/fv` | OCaml 5.2.0 and exactly what the oracle's dune files ask for |
| `src/rocq-of-ocaml` | the translator's source at the pinned commit |
| `bin/rocqOfOCaml.exe` | built from it, in `coq541` |

Roughly an hour and 2.5 GB from nothing. `CITV_SEED_ROOT=<an existing opam root>` copies that
root's download-cache in first and turns most of the downloading into a file copy; the cache
is content-addressed tarballs, so seeding cannot change what gets installed.

Two things not to do with it. **Do not source `setup.sh` into a shell you then build Infer
in**: it prepends to `PATH`, `LD_LIBRARY_PATH`, `LIBRARY_PATH` and `C_INCLUDE_PATH`, and the
build harness hashes all four into the context digest that decides whether a cached build may
be reused — a shell carrying them silently rebuilds everything and turns a gate comparison
into a fresh baseline. Every script here runs it in a subprocess, so this only bites by hand.

**Do not copy an existing opam root to a new location** instead of building one. It appears to
work and is wrong: `ocamlc -where` reports the path the compiler was *configured* with, so the
copy keeps reading the original tree, and the dependency this is meant to remove survives
until the original is deleted — at which point everything breaks at once, far from the cause.

The versions are **asserted**, not merely requested:

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
reviewable without it. `CITV_TRANSLATOR` defaults to `$CITV_HOME/bin/rocqOfOCaml.exe`; point it
elsewhere to use another build. `drift.sh` refuses rather than silently skipping if it is
absent, and distinguishes the two cases — a path that does not resolve is
`REFUSED: toolchain` (the check could not be carried out), while an *explicitly emptied*
`CITV_TRANSLATOR` is the documented way to ask for a deliberately weaker run and says so. That
is why the default is written `${CITV_TRANSLATOR=...}` and not `${CITV_TRANSLATOR:=...}`: the
colon form would swallow the empty value and turn a request for a weaker run into a full one.

## What is proved

`gamma a x` is the concretisation: the IntLit literals the abstract value `a` admits.
Crucially the bounds are compared with **IntLit's own order**, not with the order on `Z` —
see "The value model" below for why that distinction is the whole point of this round.

| result | statement |
|---|---|
| `abduce_le_unsat_sound` | if `abduce_le` says `x <= y` is impossible, no pair drawn from the two concretisations satisfies it. All four operand shapes |
| `abduce_le_sat_sound` | if it tightens instead, **no solution is lost**. All four operand shapes: the `Outside`/`Between` arm used to be excluded by a hypothesis and refuted, and the arm was then repaired |
| `abduce_le_sat_sound_on_the_formerly_refuted_case` | the instance that broke that arm, kept as a pinned regression: if the arithmetic ever comes back into the bound, this fails |
| `abduce_lt_unsat_sound` | if `abduce_lt` says `x < y` is impossible, no pair satisfies it. All four shapes, under `nnu` on the left operand |
| `abduce_lt_sat_sound` | and if it tightens, no solution is lost — including across the shift back down, which is where the tagged order bites |
| `flip_abduced_unsat_sound`, `flip_abduced_sat_sound` | the flip preserves both halves, so the dispatch's swapped calls come back in the caller's order |
| `abduce_binop_constraints_unsat_sound`, `abduce_binop_constraints_sat_sound` | the dispatch over `Le`/`Lt`/`Ge`/`Gt` is sound in both halves, for **both** values of `negated` — all eight rows of the table that reach the ordered helpers |
| `witness_ge_returns_the_refinements_in_the_callers_order` | the computation that distinguishes the flip from an identity |
| four `sat_*_add_*_iff` results, `gamma_between_add_one_iff` | a bound and a value shifted by one together, in both directions, on the `nnu` fragment |
| `witness_the_shift_down_needs_more_than_nnu` | why those four do not take the tidier hypothesis: a counterexample, failing in opposite directions on the two halves of membership |
| `signed_literals_are_closed` | the half of "every reachable literal is `Signed`" that Rocq can see. The other half is `intlit-signed-invariant.sh` |
| four `witness_*` results | the regression checks described under "The value model" |
| `add_sound` | if `add` returns an interval, the real sum of any two admitted values is in it |
| `minus_sound` | the same for negation, for **both** arms — `Between`, where the constructor swaps the endpoints, and `Outside`, where it swaps which disjunct holds |
| `unop_neg_sound` | `unop Neg` is `minus`, so it inherits that |
| `binop_PlusA_sound`, `binop_MinusA_sound` | the two operators `binop` interprets arithmetically. `MinusA` is implemented as `minus a_rhs >>= add a_lhs` while the concrete operation is `IntLit.sub`, so the proof needs `sub_is_add_neg` to close the gap |
| `binop_unsupported_is_none`, `unop_bnot_is_none`, `unop_lnot_is_none` | the other eighteen binary operators and the other two unary ones decline. That is most of `binop`'s surface and is stated rather than skipped: "returns `None`" is a soundness obligation too, just a trivial one |
| `bound_add_die_arm_is_unreachable` | the arm where the real code dies cannot be selected once both operands have a concrete value |
| four `*_off_the_signed_fragment` results | machine-checked **counterexamples** showing the `Signed` hypothesis on the five results above cannot be dropped |

### The arithmetic, and the hypothesis it carries

Every arithmetic result above assumes its literals are `Signed`. Two separate things make
that the right statement.

**It cannot be dropped.** `IntLit` orders lexicographically on `(area, i)`, and `lift1` and
`lift2` join the signedness tag, so an operation can move a literal between *areas* and
reorder it without the arithmetic having moved it. `Signed 1 <= Unsigned (-1)`, but adding
`Unsigned 2` to each gives `Unsigned 3` and `Unsigned 1` — the smaller operand now has the
larger sum. Negation is not antitone for the same reason. Both failures reach the interval
level: `add_is_unsound_off_the_signed_fragment` exhibits a value in the operand whose real
sum is outside the returned interval, and `minus_is_unsound_off_the_signed_fragment` does
the same for negation.

**It costs nothing, because every `IntLit` the analyser can build is `Signed.`** That is an
invariant of the OCaml module, not of anything here, and it rests on four facts about
`infer/src/IR/IntLit.ml` and its interface: the record is built in exactly two places and
both write `Signed`; every other producer goes through `lift1`, which preserves the tag, or
`lift2`, which joins two, and `Signed` joined with `Signed` is `Signed`; `type t` is
abstract and the interface never names the field or the `Unsigned` constructor; and the
derived `t_of_sexp` has no callers.

The **closure** half of that is machine-checked here — `sgn_add`, `sgn_neg` and
`sgn_itv_minus` are exactly "the operations stay in the fragment". The base case is the
inspection above and is *not* formalised, because this development sees the translated
`PulseCItv`, not the whole of `IntLit.ml`. So the four refutations describe values the
analyser cannot currently build, and they are kept as **regression checks on that
invariant**: if a new constructor, an exposed field or a deserialiser with a caller ever
produces an `Unsigned` literal, they stop being hypothetical and the arithmetic results
stop covering the reachable inputs.


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

### What the tagged order cost, and how it was paid

An earlier round of this directory claimed `abduce_lt` (both halves) and
`abduce_binop_constraints` over the four ordered operators, proved against a model in which
`IntLit.t` was plain `Z`. Those proofs were **withdrawn**, because the obstruction is real:

- `abduce_lt` works by shifting the left operand up by one, calling `abduce_le`, and
  shifting the refinement back. Shifting is **not monotone** in IntLit's order:
  `Signed(5) <= Unsigned(-1)` holds, but `Signed(6) <= Unsigned(0)` does not.
- `add_int` can therefore turn a valid interval into an inverted one. In real OCaml
  `between` then raises `Assert_failure`; in the translation that assert is erased (see
  "What is NOT covered"), so the Rocq model silently continues.

**They are back, and not by assuming the problem away.** The note that withdrew them said
restoring them meant "either scoping them to an all-`Signed` invariant and justifying where
in Infer that holds, or doing the area analysis for the shift". What is in `Soundness.v`
now is the second, scoped by the first:

- `nnu x` — *not negative unsigned*, i.e. `area x <> 3`, equivalently "`x` is `Signed` **or**
  its value is non-negative" — is the exact fragment on which `area` is decided by the sign
  of the value, so the lexicographic `(area, i)` order collapses to the order on `i`. The
  area analysis is `nnu_lex`, `nnu_lexlt`, `nnu_ile`, `nnu_ilt`, and `sgn_lex` and friends
  are now derived from them rather than proved separately. The inclusion is strict:
  `Unsigned 0` is `nnu` and is not `sgn`, so the hypothesis the ported theorems carry is
  **weaker** than the one the arithmetic already carried.
- `nnu` is closed under the shift **up** and not under the shift **down**, and that is not a
  detail of the proof: `witness_the_shift_down_needs_more_than_nnu` is the computed
  counterexample, and it fails in *opposite directions* on the two halves of membership — an
  area-3 bound admits nothing as a lower bound and everything as an upper one. So
  `sat_upper_shift_down` needs no side condition and there is deliberately no
  `sat_lower_shift_down`.
- The lower side is settled structurally instead, by `abduce_le_lhs_shape`: in the only two
  arms where `abduce_le` returns a left refinement, its lower bound is either
  `MinusInfinity` or the left operand's own lower bound **unchanged**. So the bound
  `abduce_lt` shifts back down is one this very function had just shifted up, and
  `add_one_then_minus_one` returns it exactly, in all three fields.
- The premise that the hypothesis is free — every `IntLit` a Pulse analysis can build is
  `Signed` — has a closure half that is machine-checked (`signed_literals_are_closed`) and a
  base case that Rocq cannot see, because this development compiles a stand-in `IntLit`.
  The base case is pinned by `intlit-signed-invariant.sh`; see "The IntLit premise" below.

The ported statements are **stronger than the Lean ones they replace** on three axes, the
same three on which the `abduce_le` theorems already were: they are about the translated
real module rather than a hand copy, about `IntLit.t` under the module's own order rather
than `Z`, and about all four operand shapes rather than `Between`/`Between` only. The
dispatch theorems additionally cover both values of `negated`, so all eight rows of
`abduce_binop_constraints` that reach the ordered helpers are covered rather than four.

#### The mapping, Lean theorem to Rocq theorem

`verification/citv/lean/Citv/Comparison.lean` proved twenty theorems; eight were already
covered here and these are the other twelve.

| Lean (`Comparison.lean`) | Rocq (`v/Soundness.v`) | hypothesis the port adds |
|---|---|---|
| `Bound.allowsBelow_succ` :125 | `sat_lower_add_one_iff` | `nnu` bound, `nnu` value |
| `Bound.allowsAbove_succ` :129 | `sat_upper_add_one_iff` | `nnu` bound, `nnu` value |
| `Bound.allowsBelow_pred` :133 | `sat_lower_add_minus_one_iff` | `nnu` of the **shifted** bound, `nnu` value |
| `Bound.allowsAbove_pred` :137 | `sat_upper_add_minus_one_iff` | `nnu` of the **shifted** bound, `nnu` value |
| `memBetween_succ` :226 | `gamma_between_add_one_iff` | `nnu` on both bounds and the value |
| `abduceLt_unsat_sound` :232 | `abduce_lt_unsat_sound` | `nnu_itv` on the left operand, `nnu x` |
| `abduceLt_sat_sound` :245 | `abduce_lt_sat_sound` | `nnu_itv` on the left operand, `nnu x` |
| `Abduced.flip_unsat` :290 | `flip_abduced_unsat_sound` | none |
| `Abduced.flip_sat` :293 | `flip_abduced_sat_sound` | none |
| `abduceCmp_unsat_sound` :314 | `abduce_binop_constraints_unsat_sound` | `nnu_itv` on both operands, `nnu` on both values |
| `abduceCmp_sat_sound` :328 | `abduce_binop_constraints_sat_sound` | `nnu_itv` on both operands, `nnu` on both values |
| `abduceCmp_ge_puts_the_refinements_in_the_callers_order` :344 | `witness_ge_returns_the_refinements_in_the_callers_order` | none (a computation) |

`sgn_itv_nnu_itv` and `sgn_nnu` discharge every one of those hypotheses on the `Signed`
fragment, which is the fragment the analyser runs in.

### The IntLit premise

Every ported theorem is conditional on `nnu`. `intlit-signed-invariant.sh`, which `run.sh`
executes, is what entitles anyone to call that hypothesis free. **Its load-bearing argument
is not a grep:** `IntLit.mli` declares `type t` abstract and never mentions the type
`signedness`, so no other module can even *name* the constructor `Unsigned` — that is the
OCaml type system, not a pattern match on text. The five checks pin the premises of that
argument: the places `IntLit.ml` mentions `Unsigned` (set equality, both directions), the
record constructions that write the field (likewise — the only two are `of_z` and `null`,
and both write the literal `Signed`), the interface staying abstract, the derived
`t_of_sexp` — the one back door the interface does open — having no caller and no new
derived chain reaching it, and a redundant canary over the rest of `infer/src`.

Two things about how it is written are deliberate. Comments and string literals are
**stripped before matching**, because a keyword gate that matches its own documentation
fires on the comment explaining the keyword and then gets deleted instead of the comment;
the scan over `infer/src` is two-stage for the same reason, an ordinary grep to narrow the
candidates and the stripper to decide. And it carries `--self-test`, which plants one
violation per check in a copy outside the tree and requires the refusal to **name** that
check — plus two plants that put the keywords in a *comment* and require the checks to
still pass. The first version of the script failed that second kind, which is how the raw
grep in two of the checks was found.

It is **not** a `sha256` pin like `expected/source.sha256`. `IntLit.ml` is edited for
reasons that have nothing to do with signedness, and a check that refuses on every
unrelated change is relaxed and then removed.

### The arm the real code dies in

`Bound.add` has a case the real code answers with `L.die`, which never returns. A Rocq
match must be total, so the arm needs an inhabitant, and it used to be the support
library's `axiom`. That was wrong in a way worth recording: `Print Assumptions` follows the
*constants a theorem mentions*, not the branches its proof visits, so every theorem about
`PulseCItv.add` inherited `Basics.axiom` — which is `forall A : Set, A`, i.e. inconsistent
— even though no proof could reach the arm. The axiom check refused four R4 theorems for
exactly that, which is the gate doing its job.

It is now a concrete bound. The choice is arbitrary, so it had better be inert, and
`bound_add_die_arm_is_unreachable` shows it is: the arm is selected only by `MinusInfinity`
together with `PlusInfinity`, and whichever way round they come one of them contradicts the
concretisation, because `sat_lower PlusInfinity` and `sat_upper MinusInfinity` are both
`False`.

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
translated module sixteen ways, at least one per theorem family, and each must **compile**
(so that a rejection means the proof talking, not a broken file) and then be **refused**.
`MIN_MUTANTS` is 16; the floor has only ever gone up, 4 → 6 → 12 → 16.

This paragraph said *eleven* while `mutants.txt` already held twelve — the twelfth,
`abduce_le/outside-arm-adds-one-again`, was added with the fix to that arm and the prose
was not moved with it. A table that can drift from the file it describes is worse than no
table, so `mutate.sh` now **asserts** that the names in the table below are exactly the
names in `mutants.txt` and that the count in the sentence above is `MIN_MUTANTS`. The
drift that produced this note would now be a failing run.

| mutant | what it breaks |
|---|---|
| `abduce_le/weaken-the-unsat-guard` | `Bound.lt` → `Bound.le` in the refutation test |
| `abduce_le/over-tighten-the-lhs` | shifts the left refinement's lower bound up by one |
| `abduce_le/over-tighten-the-rhs` | the same on the right refinement |
| `abduce_le/over-tighten-the-between-outside-arm` | shifts that arm's lower bound one too far |
| `IntLit/lift1-drops-the-tags` | rebuilds a default `Signed`/`NotPointer` record instead of `{x with i}` |
| `IntLit/equal-ignores-the-area` | compares values only, so the two negative tags stop being distinguished |
| `add/drop-the-second-upper-bound` | returns `upper1` instead of `upper1 + upper2`, so a real sum can exceed the result |
| `minus/forget-to-swap-the-between-arm` | negates the endpoints without exchanging them |
| `minus/forget-to-swap-the-outside-arm` | the same on the `Outside` arm, which the Between mutant does not touch |
| `binop/MinusA-forgets-the-negation` | computes `a_lhs + a_rhs` where the concrete operation is subtraction |
| `binop/PlusA-subtracts-instead` | the mirror image, so neither operator can be satisfied by the other's implementation |
| `abduce_le/outside-arm-adds-one-again` | puts the `+1` back into the `Outside`/`Between` arm's upper bound |
| `abduce_lt/forget-the-shift-back` | returns the refinement still shifted up, so it excludes `x = lo1` |
| `abduce_lt/shift-the-left-operand-twice` | decides `x + 2 <= y`, so `[0,0] < [1,1]` is called impossible |
| `flip_abduced/make-it-the-identity` | returns the two refinements in the callee's order, not the caller's |
| `abduce_binop_constraints/ge-without-the-swap` | `Ge` abduces `x <= y` instead of `y <= x` |

Five aim at `abduce_le` — the first four plus `outside-arm-adds-one-again`, which guards
the arm that was repaired after a counterexample.

Two aim at `IntLit.v`, which previously had no mutant at all: review found that a `lift1`
which silently dropped the tags compiled and left every proof passing. It does not any
more.

Five are R4's. Two of them exist as a pair on purpose: `MinusA-forgets-the-negation`
and `PlusA-subtracts-instead` are mirror images, so neither operator's theorem can be
satisfied by the other operator's implementation. `minus` gets one mutant per arm, because
a mutant of the `Between` arm leaves the `Outside` arm untested and that arm is where the
negation has to swap which disjunct holds.

The last four are the comparison fragment ported from Lean, and they *return* mutants an
earlier round removed when those theorems were withdrawn. Each was checked to die in the
theorem that attacks it rather than merely somewhere: `forget-the-shift-back` in
`abduce_lt_sat_sound`, `shift-the-left-operand-twice` in `abduce_lt_unsat_sound`,
`make-it-the-identity` in `flip_abduced_sat_sound`, and `ge-without-the-swap` in
`abduce_binop_constraints_unsat_sound`. The witness
`witness_ge_returns_the_refinements_in_the_callers_order` was separately run **on its own**
against the flip mutant and rejects it there too, because the claim in its comment — that
only a computation can tell a flip from an identity — deserved a run rather than an
assertion.

Two obvious mutants are deliberately **absent**, and the reason is about the code rather
than the tests. Replacing `abduce_lt` by `abduce_le`, and dispatching `Lt` to `abduce_le`,
are both *sound* — merely imprecise — so they would survive. A surviving mutant that is
really an equivalent mutant teaches nothing and invites someone to weaken a theorem until
it dies. A mutant belongs in `mutants.txt` only if the statement it attacks becomes false.
