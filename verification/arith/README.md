# A symbolic oracle for interval and formula reasoning

`expected/obligations.json` pins the obligation categories a symbolic arithmetic oracle must
cover: an SMT solver confirming, or refuting, what the buffer-overrun interval domain and the
Pulse formula engine concluded. Joins over-approximate, widening is inclusive, pruning is sound
and deflationary, and a formula the engine calls unsatisfiable really is.

## Why there is a pin and no runner

**This directory deliberately contains a specification and nothing that reads it.** A reader
finding one lonely JSON file should not have to guess whether it was forgotten.

The oracle existed, covered these 18 categories and was mutation-tested, but it was never
committed: it lived in unversioned scratch space and was lost twice. What survived is its
external contract — the result schema, the category counts, recorded request and response
pairs, and the protocol controls — and that recording is kept privately rather than here. The
pin is committed on its own so that a rebuild has something to be correct against, which is
what stops a third loss.

So the file is the specification, written before the thing it specifies. The runner that will
read it is `verification/arith/run.sh`, which does not exist yet.

## What a rebuilt runner must satisfy

- Emit a JSON result whose `counts` keys are **exactly** the pinned categories. Not a subset:
  a category that stops being generated is a silent loss of coverage. A category that appears
  without being pinned means the oracle grew, and this file grows with it in the same commit.
  Both directions are failures.
- Keep soundness and completeness apart. A formula the solver proves unsatisfiable while the
  engine did not refute it is imprecision, not unsoundness. Adding the two together either
  hides a real bug or reddens the check for a known cost.
- Honour the nine protocol controls. Eight of them feed the driver something it cannot process
  and must be refused — the check could not be carried out, so nothing is claimed. The ninth,
  `zero-coverage`, is different: the run completed and covered nothing, which is a failed
  obligation and a statement about the tree, not a broken environment. Reporting it as a
  refusal would waste a genuine finding.
- Expect an **instrumented** build. The oracle reads bound and interval representations through
  test-only hooks, so a stock install cannot run it. That makes it a nightly check; there is no
  pre-push tier for it, and adding one would put an instrumented build on the push path.

## Toolchain

It needs z3 (the pinned version is in `solver_version`), installed by
`verification/citv/install-toolchain.sh`. That script puts the Python bindings somewhere that
is deliberately *not* on `PYTHONPATH`, and exports `CITV_Z3_PYTHONPATH` pointing at them, so
that a 67 MB site-packages tree does not sit in front of every `python3` in a sourcing shell.
Note that `python3 -I` implies `-E` and therefore ignores `PYTHONPATH` anyway: add the bindings
to `sys.path` explicitly, or you get a `ModuleNotFoundError` for a module that is installed.
