# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import json
import re
import sys
from pathlib import Path

procedures = re.split(r"(?m)(?=^\S)", Path(sys.argv[1]).read_text())
native_count = 0
controls = set()
for procedure in procedures:
    name = procedure.splitlines()[0] if procedure else ""
    if "NativeMethods" not in name:
        continue
    is_native = "; is_native= true" in procedure
    if ".native" in name:
        assert is_native, name
        assert "defined: false" in procedure, name
        native_count += 1
    else:
        assert not is_native, name
        for control in ("abstractOk", "ordinaryOk", "callbackOk", "nullControlBad"):
            if f".{control}(" in name:
                controls.add(control)
                expected_defined = control in ("ordinaryOk", "nullControlBad")
                assert ("defined: true" in procedure) == expected_defined, name

assert native_count == 7, native_count
assert controls == {"abstractOk", "ordinaryOk", "callbackOk", "nullControlBad"}, controls
issues = json.loads(Path(sys.argv[2]).read_text())
assert len(issues) == 1, issues
assert issues[0]["bug_type"] == "NULLPTR_DEREFERENCE", issues
assert "nullControlBad" in issues[0]["procedure"], issues
print("PASS: seven native flags, four non-native controls, and null-dereference control")
