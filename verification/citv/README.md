# A bounded soundness check for the concrete interval domain

`infer/src/pulse/PulseCItv.ml` decides two things for Pulse: that a comparison is impossible,
which prunes a path, and how far an operand can be tightened when a comparison holds. Both are
only as good as the arithmetic underneath. An interval tightened too far, or a contradiction
announced too eagerly, removes program states that really occur, and with them real reports.

This directory states what each exported function of that module *means*, as a set of integers,
independently of how the module computes it, and then compares the two over every interval that
can be built from a small window of endpoints.

## Running it

```
./verification/citv/run.sh
```

That checks the module as it stands in the working tree and then runs every mutant. Options:

| | |
|---|---|
| `--oracle` | check the module only, skip the mutants |
| `--retired` | replay the retired mutants and check the record below |
| `--adversarial` | attack every guard in here; each edit must be refused |
| `--window W T` | endpoints in `[-W, W]`, integers checked over `[-T, T]` (default `4 8`) |

It needs an OCaml switch with `dune zarith yojson ppx_compare ppx_yojson_conv ppx_enumerate`,
and nothing else.
At the default window the check itself takes a few seconds and the mutants a few minutes.

## What it is not part of

Nothing here is part of Infer's build. The dune root is `infer/`, and this directory is outside
it, so `make` and `dune build` never see these files; no binary, expectation file or test result
changes because they exist. The check has to be asked for by name.

`PulseCItv.ml` and `PulseCItv.mli` are **not** kept here. `run.sh` copies them out of the working
tree into a temporary directory at run time and prints their checksums, so this directory cannot
hold a stale fork of the module, and the bytes that were checked are provably the bytes in the
tree. Everything else — the stand-ins for the handful of Infer modules `PulseCItv` names, the
oracle, and the mutants — is committed here.

## What is checked

One property per exported value. `mem` in `oracle.ml` is a reading of the two constructors and
never calls the module; every property below is phrased against it.

| exported value | property |
|---|---|
| `unknown` | contains every integer |
| `equal_to i` | contains exactly `i` |
| `nonnegative` | contains exactly the integers `>= 0` |
| `is_equal_to_zero` | if it says yes, the set is exactly `{0}` |
| `is_not_equal_to_zero` | if it says yes, the set does not contain `0` |
| `is_non_pointer` | if it says yes, the set does not grow when the window widens |
| `to_singleton` | if it answers `Some i`, the set is exactly `{i}` |
| `intersection` | `Some c` keeps every shared integer; `None` means there is none |
| `binop` | for every operator, a result contains every value the operation really takes |
| `unop` | likewise, for `Neg`, `BNot` and `LNot` |
| `abduce_binop_is_true` | `Unsatisfiable` only when no pair satisfies the comparison, and a tightened operand keeps every value that takes part in a satisfying pair |
| `pp` | total |
| `requires_integer_reasoning` | total and deterministic |

Three of those deserve a word.

`intersection` is containment rather than equality on purpose: the domain has no hole, so
intersecting `[-2,1]` with `<>0` cannot be represented exactly and the module returns the best
interval that still holds everything shared. Keeping extra values is imprecision; losing a shared
one would be unsound.

`requires_integer_reasoning` is a representability heuristic for the rest of `PulseFormula`
rather than a statement about a set, so there is no soundness property to state for it and only
totality is asserted.

`binop` is asked about **every** operator it matches on, not only the four it answers today.
Declining is always sound, so `None` is accepted everywhere; an answer is only sound if it holds
the true result, so every answer is judged. `oracle.ml` carries the concrete reading of `PlusA`,
`MinusA`, `Mult`, `DivI`, `Mod`, the two shifts, the three bitwise operators, the two logical
ones and the six comparisons. Division truncates toward zero and the remainder keeps the sign of
the dividend, as in C; the bitwise and shift operators are read as two's complement on
mathematical integers. Where C leaves the result undefined — division by zero, a negative shift
count — nothing is judged rather than a value being invented. Pointer arithmetic (`PlusPI`,
`MinusPI`, `MinusPP`) and float division have no reading here as a set of integers, so for those
four the property is that the module **declines**. That one is a tripwire rather than a soundness
statement: if the module ever starts answering them the check fails, which is the prompt to
extend this oracle rather than to trust the new answer.

An exception escaping the module counts as a violation. The module is total on every value its
own constructors can build, and in particular may never build an interval whose bounds are
crossed, which its `Unsafe` constructors assert against. Without this rule a defect that trips an
internal assertion before it can produce a wrong answer would look like a pass. The rule applies
to every call into the module, including the constants, the safe constructors that build the
value space, and the printer: a printer that raises has to be reported rather than take the run
down while it is formatting the description of some other check.

## What goes in: the input domains, and how much of each is covered

A property is only worth what the inputs it is asked about are worth, and an input the oracle
never supplies is a defect it can never see — while the run still says PASS. This is the audit
of what the oracle *supplies*. What it does not cover is in the last section, and the two are
worth reading together.

| input domain | what the module accepts | what the oracle supplies | gap |
|---|---|---|---|
| `Binop.t` | 22 constructors, three of which carry an integer-kind option, so 25 shapes | all 25, generated from the type | none; the kind itself is collapsed to `unit`, see below |
| `Unop.t` | `Neg`, `BNot`, `LNot` | all 3, generated from the type | none |
| `t` as an operand | `Between` with non-crossed bounds and `Outside` with ordered literals; the constructors reject anything else | every `Between (Int a, Int b)` and `Outside (a, b)` with endpoints in `[-W, W]`, both half-bounded shapes, the unbounded one | only the window: endpoints outside `[-W, W]` are not built |
| `t option` for `abduce_binop_is_true` | `None` on either side | both sides, `None` and every value above | none |
| `negated` | `true`, `false` | both | none |
| the integers a result is judged at | any | `[-T, T]`, but membership is total, so a result landing outside the window is still judged correctly | a wrong answer that is wrong only outside `[-T, T]` is not seen |
| the literal in `equal_to` | any | `[-T, T]` | the window |
| the divisor in `DivI` and `Mod` | whatever `literal_of` extracts, so any singleton interval | singletons in `[-W, W]`, which includes the `k = 0` and `k = -1` special cases for `W >= 1` | the window |
| `IntLit.t` | signed or unsigned, pointer or not, arbitrary precision | a plain arbitrary-precision integer | **named gap**: the tags are not modelled, see the last section |
| machine width | implied by the signedness tag | none | **named gap**, same place |

### how each domain's completeness is attested

Five rounds of review each found the next domain this oracle did not fully visit. Every one was
invisible in the same way: a sampler that skips a value reports no failure for it, and a run
that reports no failures prints exactly like a run that found none. Counting does not help —
a check that takes its expected size from the thing it is checking accepts every prefix of it.

So each domain is now attested by **set equality**, before anything is judged, against an
expectation computed by a route that shares no code with the sampler. Missing **and** duplicate
both fail, because a sampler that produced one value twice produced some other value never and
the totals would still look right.

| domain | the expectation | the sampler it is checked against |
|---|---|---|
| `Binop.t` shapes | `[@@deriving enumerate]` on the type, so adding a constructor or a payload shape extends it with no one's help | the successor walk in the same shim |
| `Unop.t` shapes | the same | the same |
| operand intervals for `W` | written out as text straight from the integers, never touching the module's constructors | read back out of the constructors of what `values` actually built |
| the absent operand | offered exactly once beside the values | the length of the two lists |
| the literal set `[-T, T]` | characterised — right length, strictly increasing, reaching both ends — rather than re-derived, since re-deriving it would prove nothing | `concrete` |
| the `negated` flag | exactly `[false; true]` | the list the driver folds over |
| shift counts | no separate domain: they are operand elements, so the literal set covers them, and the arithmetic is arbitrary precision, so none is out of range | — |
| the properties | every name in `expected_properties` must have been exercised at least once | the check counters |

The two operator rows are the ones that used to be wrong. They were a hand-written `count`,
`first` and `last`, which were correct and still said nothing, because **nothing made them grow
with the type**: a fourth `Unop` constructor, with every exhaustive match fixed so that it
compiled, passed with three shapes. They are gone. The domain is the type's own `all`, the
driver iterates that, and the successor walk — whose match has no catch-all, so dropping a
shape is still a compile error — is now a second enumeration that must be set-equal to it.

The integer kind inside `PlusA`, `MinusA` and `Mult` is shimmed as `unit`, so `Some` has one
representative rather than one per `Typ.ikind`. `PulseCItv` matches those constructors with a
wildcard and never reads the kind, which is what makes one representative enough; if it ever
starts reading it, the shim has to grow and this row of the table is where that is recorded.

## Every guard, attacked

Three rounds of review each found the next domain this oracle silently did not visit: first the
operators it never passed to the module, then the payload shapes of three of them, then a
cardinality check that took the cardinality from the list it was checking. The pattern is always
the same — something is not looked at, and a run that looks at nothing reports no failures,
which reads exactly like a pass.

So the guards are themselves subjects now. `./run.sh --adversarial` applies each edit below and
requires it to be refused; it fails if one is accepted, and it fails if the table of attacks has
itself been shortened. The attacks are the ones that keep working: truncate at the start, at the
end, in the middle, make it empty, make it repeat, and stop one short of a bound — applied to
every domain in the table above, not only to the one that was last found wanting. They attack
the suppliers of those domains; they do not attack the call sites that consume them, which is
the limit described at the end.

| the attack | what refuses it |
|---|---|
| drop a payload shape from `succ` | the compiler: the match stops covering `unit option` |
| stop deriving the domain from the type | the compiler: there is no `all` to iterate |
| truncate `Binop`'s walk at the end | set equality against the derived domain |
| truncate `Unop`'s walk at the end | the same, in the other shim |
| truncate `Binop`'s walk at the start | the same |
| truncate `Binop`'s walk in the middle | the same |
| enumerate nothing at all | the same |
| visit a shape twice | the duplicate half of set equality |
| truncate the walk **and** hide a wrong answer in the shape that falls off | both: the driver iterates the derived domain, so the wrong answer is now visited and caught on its own merits, and the truncated walk is reported as well |
| drop the first, last or an interior operand interval | set equality on the value domain |
| produce an operand interval twice | the duplicate half of it |
| drop only `Between(W,W)` **and** hide a wrong `{4} + {4}` in it | the same: this is the round-four subject, and it is the value domain's version of the one above |
| judge one integer fewer | the characterisation of the literal set |
| a `Shiftlt` right below a machine word and wrong above it | nothing, until the arithmetic became arbitrary precision; the evaluator used to return no value for shift counts above 16, which is a window inside which a wrong answer is accepted |
| run a property's loop over an empty list | every property named in `expected_properties` must have been exercised at least once |
| empty the value space | set equality on the value domain |

The same reasoning applies to the files this script reads. A loop over an empty `mutants.txt`
runs no iterations and reports no failures, so `run.sh` declares `MIN_MUTANTS`, `MIN_RETIRED` and
`MIN_ADVERSARIAL` and refuses to report a pass on fewer. They are floors, not counts: adding a
mutant needs no edit, removing one does.

## What the run reports

Three numbers, kept apart so that none of them can be mistaken for another:

```
soundness violations: N        every violation, with no cap
  wrong answers:       A       the module answered and the answer does not hold a value it must
  escaping exceptions: B       the module raised on a value its own constructors built
witnesses retained:   M of N   the first M violations, printed; M is capped at 40
```

`A + B = N`. The cap is on what is printed, never on what is counted.

## Mutants

One deliberate defect per helper and per operator family, in `mutants/mutants.txt`. `run.sh`
applies each to a throwaway copy, runs the oracle, and requires the oracle to reject it. A mutant
that survives is a gap in this check rather than a defect in Pulse, so a survivor fails the
script; so does one that fails to compile or takes the process down, since in neither case did
the oracle get to judge it.

At the default window all 33 are killed and the module itself is clean. Each line of the report
says how it died, so a mutant that is only ever caught by an assertion inside the module is
visible as such.

Four of them exist because of the audit above rather than because of a helper. Three put a wrong
answer in the `Some ikind` branch of `PlusA`, `MinusA` and `Mult`, which an earlier driver never
reached; the fourth makes `abduce_binop_constraints` call an arithmetic condition impossible,
which the same earlier driver could not see because it asked `abduce_binop_is_true` only about
the six comparisons, although it accepts every operator.

## Retired mutants

`mutants/retired.txt` holds every mutant that was tried and is not in `mutants.txt`, with the
exact text that was applied and the outcome it produced. A surviving mutant quietly swapped for
one that dies can hide a hole in the oracle, so the retired text is kept rather than the story
about it, and `./run.sh --retired` replays each one and fails if it no longer reproduces its
recorded outcome.

| retired mutant | outcome | why it is not in `mutants.txt` |
|---|---|---|
| `abduce_le/drop-the-upper-cut` | survived | **Sound.** Widening the cut makes the abduction weaker, and a weaker refinement cannot drop a feasible value. Replaced by `declare-unsat-too-eagerly`, which narrows. |
| `rem_by_literal/one-too-wide` | survived | **Sound.** A wider remainder range is an over-approximation. Replaced by `forget-negative-remainders`. |
| `remove_element/cut-the-wrong-end` | survived | **Sound.** It widens by one and fails to remove the element, which is again only a weaker refinement. Replaced by `remove-two-instead-of-one`. |
| `intersection/return-the-left-operand` | did not compile | `Some a1` leaves both names bound by the branch unused. The first account of this one said it survived; replaying it showed it never built, and the record here is the replay. |
| `intersection/ignore-the-right-operand` | survived | **Sound in the context it edits.** It sits in the `Satisfiable` branch, where `abduce_eq` has already shown the two operands overlap, so returning the left one is a superset of the intersection. Replaced by `claim-disjoint-when-untightened`, which reports a non-empty intersection as empty. The unconditional form is a different statement and does die: see `intersection/always-the-left-operand` in `mutants.txt`. |
| `div_by_literal/allow-the-overflowing-divisor` | survived | **Sound, and more precise.** Dropping the `k = -1` guard is not a no-op: `[2,4] / {-1}` goes from `None` to `Some [-4,-2]`, which is the right answer. The guard is there because a machine overflows when the most negative value of a width is negated, and machine widths are outside the fragment this oracle models, so there is nothing here for it to catch. Replaced by `shift-the-image`. |
| `add/lose-the-lower-bound` | did not compile | A mutant must compile. Replaced by `upper-one-too-small`. |
| `remove_element/keep-only-the-upper-side` | did not compile | Dropped: `remove_element` already has a mutant that dies. |
| `minus/off-by-one` | killed | A stand-in used while `minus/forget-to-swap` looked unkillable. Withdrawn once the oracle could see that one. |
| `add_int/shift-one-too-far` | killed | Likewise, for `add_int/shift-one-side-only`. |

`minus/forget-to-swap`, `add_int/shift-one-side-only` and `remove_element/remove-two-instead-of-one`
are **not** retired. All three looked killed and were not: each produced a crossed interval and
tripped the module's own assertion before the oracle could judge the answer. That was a hole in
the oracle, not a reason to swap the mutants, so the rule that an escaping exception is a
violation was added and all three were restored. They are in `mutants.txt` and die on counted
violations.

## The unbounded half

`lean/` restates one branch of the module in Lean and proves it for all integers and all bounds,
infinities included, with an axiom whitelist rather than a green build as the acceptance
criterion. It is a model rather than the code, so its README carries the correspondence table
and says what the two halves each cover. `./verification/citv/lean/run.sh` runs it; it needs the
Lean toolchain pinned there and nothing else, and Infer's build does not depend on Lean.

## What the oracle guarantees, and what it does not

It is worth being exact about where the line falls, because five rounds of review were spent
finding out and the answer is not obvious from the output.

**What is attested.** Every domain in the table above is checked by set equality, before
anything is judged, against an expectation computed by a route that shares no code with the
sampler. So the *supplier* of each domain is complete: the operator list really is every shape
of `Binop.t` and `Unop.t`, the value space really is every interval that can be built from the
window, each exactly once, and the literal set really runs from `-T` to `T`. Dropping one
operator, one payload shape, one interval, or one integer is refused, and
`./run.sh --adversarial` breaks each of those on purpose and requires the refusal.

**What is not attested: how much of a domain each check actually consumes.** The attestation
happens once, at the top of the driver. After that, nothing checks what arguments any
individual check was called with. The per-property counters are printed, but the only thing
required of them is that each is at least one. So an edit that narrowed a call site would pass:

- calling `check_equal_to` at literal `0` only, instead of over the whole literal set;
- skipping the unbounded interval when calling `check_predicates`, instead of every value.

Both still attest the domains, still exercise every property at least once, and still report
no violations: the first drops the `equal_to` counter from 17 to 1 and the run is still a PASS.
The counter shows the narrowing; nothing asserts it. **In the code as shipped neither is the case** — every check iterates the full
attested list, and the counts below are exactly the products of the attested sizes, which is
how a reader can confirm it by hand today:

| property | checks at the default window | product |
|---|---|---|
| `binop` | 297,025 | 109 values × 109 values × 25 operator shapes |
| `abduce_binop_is_true` | 605,000 | 110 operands × 110 operands × 25 shapes × 2 for `negated` |
| `intersection` | 11,881 | 109 × 109 |
| `unop` | 327 | 109 × 3 |
| `equal_to` | 17 | one per literal in `[-8, 8]` |
| the five predicates, and `pp` | 109 each | one per value |

**What a maintainer has to preserve.** Every check must iterate the full attested list rather
than a subset of it: `Binop.all` and `Unop.all` for the operators, `somes` for the values,
`vs` for the operands of `abduce_binop_is_true`, `concrete ~t` for the literals, and both
values of `negated`. A narrowing there is the one defect this check cannot presently see, and
reviewing a change to the driver means reading the call sites, not just running the script.

Closing that gap means deriving each property's expected call count from the attested sizes and
asserting it, rather than asking only for a positive one. That is follow-up work and is not
done here.

## What this does not establish

The window is finite, so a clean run is evidence over the stated bounds and not a proof. The
Lean development above carries the unbounded statement for the ordered-comparison fragment, but
only for that fragment, and about a model of the module rather than the module itself.

The oracle covers the **signed, non-pointer** fragment. The real `IntLit` carries a signedness
tag and a pointerness tag, and its comparison orders a negative signed value and the same bit
pattern read as unsigned differently; the stand-in here is a plain arbitrary-precision integer.
There is no machine width either, so a defect that only shows up when a value overflows one is
invisible. Extending to the tagged, width-aware domain means modelling those orders too, and is
separate work.
