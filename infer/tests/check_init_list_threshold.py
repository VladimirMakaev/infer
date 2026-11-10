# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

"""Keep the exporter and translator's array-elision thresholds in agreement."""

import re
import sys
from pathlib import Path


def threshold(path, pattern):
    matches = re.findall(pattern, path.read_text(), re.MULTILINE)
    if len(matches) != 1:
        raise ValueError(f"expected one literal threshold in {path}, found {len(matches)}")
    return int(matches[0].replace("_", ""))


def main():
    root = Path(__file__).resolve().parents[2]
    try:
        translator = threshold(
            root / "infer/src/clang/cTrans.ml",
            r"^let max_unrolled_init_list_elements\s*=\s*([0-9][0-9_]*)[ \t]*$",
        )
        exporter = threshold(
            root / "facebook-clang-plugins/libtooling/ASTExporter.h",
            r"E->getNumInits\(\)\s*>\s*([0-9]+)\s*&&",
        )
        if translator != exporter:
            raise ValueError(
                f"array-elision thresholds differ: translator={translator}, exporter={exporter}"
            )
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    print(f"array-elision thresholds agree: {translator}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
