#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

"""Make a capture look like one whose process died before it stored every procedure.

A translation unit's procedures are written one by one after the row naming the file they belong
to, so a capture that is killed in between leaves a file Infer believes it has captured next to a
procedures table missing some of its rows. A caller whose callee is one of the missing rows then
reaches a procedure with no formals at all. Deleting the declaration of std::make_shared
reproduces that without having to kill a capture at the right instant.
"""

import sqlite3
import sys


def main(db_path: str) -> int:
    db = sqlite3.connect(db_path)
    callers = db.execute(
        "SELECT count(*) FROM procedures WHERE proc_uid LIKE '%Manager::Manager%'"
    ).fetchone()[0]
    dropped = db.execute(
        "DELETE FROM procedures WHERE proc_uid LIKE 'std::make_shared%'"
    ).rowcount
    db.commit()
    left = db.execute(
        "SELECT count(*) FROM procedures WHERE proc_uid LIKE 'std::make_shared%'"
    ).fetchone()[0]
    files = db.execute("SELECT count(*) FROM source_files").fetchone()[0]
    db.close()
    # the point of the test is a caller that survives a callee that did not, so say so rather
    # than leaving a silent no-op to pass as a working test
    if dropped < 1 or left != 0 or callers < 1 or files < 1:
        print(
            f"{db_path}: expected to drop every std::make_shared declaration and keep the "
            f"constructor calling it in a recorded file, but dropped {dropped} leaving {left}, "
            f"with {callers} caller(s) and {files} source file(s)",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
