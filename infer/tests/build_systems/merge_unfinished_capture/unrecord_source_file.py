#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

"""Make a capture look like one whose process died before it recorded its source file.

The procedures of a translation unit are stored first and the row naming the file they belong to
last, so a capture that is killed in between leaves exactly this state on disk: procedures that
cannot be attributed to any recorded file. Deleting the row reproduces it without having to kill
a capture at the right instant.
"""

import sqlite3
import sys


def main(db_path: str) -> int:
    db = sqlite3.connect(db_path)
    procedures = db.execute("SELECT count(*) FROM procedures").fetchone()[0]
    db.execute("DELETE FROM source_files")
    db.commit()
    source_files = db.execute("SELECT count(*) FROM source_files").fetchone()[0]
    db.close()
    if procedures == 0 or source_files != 0:
        print(
            f"{db_path}: expected procedures and no source file, got {procedures} and "
            f"{source_files}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
