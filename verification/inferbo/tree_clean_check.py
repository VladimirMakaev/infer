#!/usr/bin/env python3
"""Report build artefacts left behind in a source directory.

Prints one offending path per line and exits 1 if any are found, 0 if the directory is
clean, 2 if it cannot look (missing directory).  The caller turns a non-empty list into a
failed obligation.

WHY THIS IS NOT `git status`.  The obvious spelling of this check -- run
`git status --porcelain` and require it to be empty -- CANNOT FAIL HERE.  The artefact
this guards against is `calib.o`, and the repository root carries `*.o` at .gitignore
line 6, so git is silent about exactly the file we are looking for.  A git-based version
would have passed on a dirty tree from the day it landed and reported coverage it did not
have.  So this stats the filesystem instead, and the test suite keeps a positive control
that puts a real .o on disk and requires a non-zero exit.

WHY IT EXISTS AT ALL.  run.sh analyses the committed fixtures by cd-ing to their directory
and running `clang -c` there, so until the `-o` fix the object file was written straight
into the checked-out source tree.  Nothing failed: the grid's verdict was unaffected and
git hid the evidence, which is precisely the kind of leak that is only ever found by
someone noticing a stray file by eye.

Lives in its own file rather than inline in run.sh so it can be tested without running a
grid, the same reason manifest_check.py and unsound_compare.py do.
"""
import os
import sys

# Extensions a compiler or an analysis leaves next to a source file.  Deliberately a
# denylist of what the toolchain emits rather than an allowlist of what belongs: a new
# FIXTURE must not have to be registered here to be allowed to exist, but a new artefact
# KIND should be noticed and added.
ARTEFACT_SUFFIXES = ('.o', '.obj', '.gch', '.pch', '.d', '.bc', '.gcno', '.gcda', '.dSYM')
ARTEFACT_NAMES = ('a.out',)


def offenders(directory):
    """Pure core: the artefact paths directly inside `directory`, sorted."""
    found = []
    for entry in os.listdir(directory):
        if entry in ARTEFACT_NAMES or entry.endswith(ARTEFACT_SUFFIXES):
            found.append(os.path.join(directory, entry))
    return sorted(found)


def main():
    if len(sys.argv) < 2:
        print('usage: tree_clean_check.py DIR [DIR...]', file=sys.stderr)
        return 2
    bad = []
    for directory in sys.argv[1:]:
        if not os.path.isdir(directory):
            print(f'{directory} is not a directory; cannot check it for artefacts',
                  file=sys.stderr)
            return 2
        bad += offenders(directory)
    for path in bad:
        print(path)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
