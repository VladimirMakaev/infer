#!/usr/bin/env python3
"""Turn the property runners' output into a verdict against expected/counts.json.

THE BASELINE IS PER TIER, and it has to be. Violation counts are matched exactly, and they
scale with the number of iterations: the normalisation residuals are 10/11/3/1 at the
pre-push sizes and 77/71/10/16 at ten times those sizes, and the larger run reaches a
shape the smaller one never does. One baseline for both tiers would therefore fail every
nightly run, which is the fastest way to teach everyone to ignore it.

WHY A SUMMARISER AND NOT grep IN THE RUNNER. Three things have to be judged together and
only one of them is "did anything fail":

  violations  exact match, in BOTH directions. A property that stops firing is as much a
              change as one that starts: normtest has accepted residuals (simplify_shallow
              is shallow by contract, and Atom.eval's `x mod 1` shape is absorbed by the
              Phi layer), and if a change makes one of them disappear that is a deliberate
              semantic change whose baseline must move in the same commit.
  reach       floors. A property that never reaches its condition reports a pass for the
              wrong reason. These harnesses have produced four such false clean runs, so
              the floors are part of the verdict rather than a diagnostic.
TWO LEGS, NOT THREE. An earlier version of this docstring listed a third, "controls: the
mutation controls must still be able to fail ... the runner asserts the count it was told
to expect". IT ASSERTS NO SUCH COUNT, and there is no count to assert: this harness ships
no mutants file and no mutate script, unlike both its siblings (verification/citv/rocq has
mutants.txt and mutate.sh, verification/inferbo has oracle.py, calib.c and
unsound_compare.py). The claim credited the check with a guarantee it does not carry, which
is the same class of fault the reach floors below exist to prevent -- a verdict that reads
as broader than the evidence. Raised by review; the sentence is removed rather than
weakened.

WHAT IS AND IS NOT COVERED, stated so nobody has to infer it. The mutants that justified
these properties were run during development, against deliberately broken copies of the
engine sources, and they are NOT REPLAYABLE from this tree: the runner takes the engine
files live and never mutates them, by design, because a mutating runner would have to write
into infer/src. So "these properties can still fail" is something a human re-establishes
when changing them, not something a run of this harness demonstrates. Making it replayable
means adding a mutants file and a mutate script that work on the assembled build directory
rather than on the tree; that is worth doing and is not done here.

Exit 0 = every obligation held, and a JSON summary is printed. Exit 1 = an obligation
failed. Exit 2 = the output could not be parsed, which is not a pass.
"""
import json
import re
import sys


def parse(text):
    """Pull the REACH and RESULTS blocks out of one runner's output."""
    reach, violations = {}, {}
    section = None
    for line in text.splitlines():
        if line.startswith('==== REACH'):
            section = 'reach'
            continue
        if line.startswith('==== RESULTS'):
            section = 'violations'
            continue
        if line.startswith('==== summary'):
            section = 'summary'
            continue
        if section in ('reach', 'violations'):
            m = re.match(r'\s+(\d+)\s+(\S.*?)\s*$', line)
            if m:
                (reach if section == 'reach' else violations)[m.group(2)] = int(m.group(1))
            elif 'no property violations' in line:
                pass
        if section == 'summary':
            m = re.match(r'\s+(\d+)\s+(\S.*?)\s*$', line)
            if m:
                violations[m.group(2)] = int(m.group(1))
    # test.ml prints its two counters as prose rather than in a block.
    m = re.search(r'abstained via NotRepresentable:\s*(\d+)\s*/\s*(\d+)', text)
    if m:
        reach['abstained'] = int(m.group(1))
        reach['iterations'] = int(m.group(2))
    m = re.search(r'surviving forms carrying a capped \(non-rational\) coefficient:\s*(\d+)', text)
    if m:
        violations['poisoned surviving forms'] = int(m.group(1))
    return reach, violations


def main():
    all_expected = json.load(open(sys.argv[1]))
    tier = sys.argv[2]
    if tier not in all_expected:
        print(f'no baseline for tier {tier!r} in {sys.argv[1]}', file=sys.stderr)
        return 2
    expected = {k: v for k, v in all_expected[tier].items() if not k.startswith("_")}
    outputs = {}
    for pair in sys.argv[3:]:
        name, path = pair.split('=', 1)
        outputs[name] = open(path, encoding='utf-8', errors='replace').read()

    failures, summary = [], {}
    for name, spec in expected.items():
        if name not in outputs:
            failures.append(f'{name}: no output was captured, so nothing can be judged')
            continue
        reach, violations = parse(outputs[name])
        if not reach and not violations:
            failures.append(f'{name}: neither a REACH nor a RESULTS block was found; the '
                            f'runner did not reach a verdict, which is not a pass')
            continue
        summary[name] = {'reach': reach, 'violations': violations}

        for key, floor in spec.get('reach_floors', {}).items():
            got = reach.get(key, 0)
            if got < floor:
                failures.append(f'{name}: reach {key!r} is {got}, below the floor {floor}. '
                                f'The property did not reach its condition, so a clean '
                                f'result from it would mean nothing.')

        want = spec.get('violations', {})
        for key, n in want.items():
            got = violations.get(key, 0)
            if got != n:
                failures.append(f'{name}: violations {key!r} is {got}, expected {n}. If this '
                                f'change is intended, update expected/counts.json in the '
                                f'SAME commit.')
        for key, got in violations.items():
            if key not in want and got:
                failures.append(f'{name}: NEW violation {key!r} ({got}) not in the baseline.')

    print(json.dumps(summary, indent=1, sort_keys=True))
    if failures:
        for f in failures:
            print(f'OBLIGATION FAILED: {f}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
