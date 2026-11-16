# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import json
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

infer = sys.argv[1]
source = Path(__file__).resolve().parent


def run(*args, failed=False):
    result = subprocess.run([infer, *map(str, args)], capture_output=True, text=True)
    assert (result.returncode != 0) == failed, result.stdout + result.stderr


def constructor_defined(directory, name):
    with sqlite3.connect(directory / "capture.db") as db:
        constructors = db.execute(
            "SELECT proc_uid, cfg IS NOT NULL FROM procedures WHERE proc_uid LIKE ?",
            (f"class {name}::{name.split('<')[0]}{{%",),
        ).fetchall()
    assert constructors, "defaulted constructor was not captured"
    # The unused copy/move constructor declarations can be present as well.
    assert sum(defined for _, defined in constructors) <= 1, constructors
    return any(defined for _, defined in constructors)


with tempfile.TemporaryDirectory(prefix="defaulted-ctor-merge-") as temporary:
    root = Path(temporary)
    broken, good = root / "broken", root / "good"
    for unit, output, failed in [("a_broken.cpp", broken, True), ("sibling.cpp", good, False)]:
        run("capture", "--project-root", source, "-o", output,
            "--", "clang++", "-std=c++17", "-c", source / unit,
            "-o", root / (unit + ".o"), failed=failed)
    assert not constructor_defined(broken, "Array<int>"), "recovery constructor must be declared only"
    assert constructor_defined(good, "Array<int>"), "synthesized constructor must remain defined"
    for name in ["Uninitialized", "ExplicitEmpty", "TrivialEmpty"]:
        assert constructor_defined(broken, name), f"legitimate empty {name} constructor was dropped"

    expected = {("failed_defaulted_bad", "PULSE_UNINITIALIZED_VALUE"),
                ("failed_explicit_bad", "PULSE_UNINITIALIZED_VALUE"),
                ("uninitialized_bad", "PULSE_UNINITIALIZED_VALUE")}
    for label, first, second in [("broken-good", broken, good), ("good-broken", good, broken)]:
        merged = root / label
        run("capture", "--project-root", source, "-o", merged,
            "--merge-capture", first, "--merge-capture", second)
        assert constructor_defined(merged, "Array<int>"), label
        run("analyze", "--pulse-only", "--jobs", "1", "--project-root", source, "-o", merged)
        issues = json.loads((merged / "report.json").read_text())
        actual = {(issue["procedure"], issue["bug_type"]) for issue in issues}
        assert len(issues) == len(expected) and actual == expected, (label, issues)
        print(f"PASS {label}: complete definition selected; three genuine uninitialized reads")
