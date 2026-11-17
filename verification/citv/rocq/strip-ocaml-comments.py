#!/usr/bin/env python3
"""Blank out OCaml comments and string/char literals, keeping line structure.

A keyword gate that matches its own documentation fires on the comment explaining why the
keyword is there, and the next person deletes the gate rather than the comment.  So the
text the checks below match is the CODE, with every (* ... *) -- nested, as OCaml allows --
and every "..." and '.' replaced by spaces of the same length.  Line numbers and columns
survive so a refusal can still point at the source.
"""
import sys


def strip(text: str) -> str:
    out = list(text)
    i, n, depth = 0, len(text), 0
    while i < n:
        c = text[i]
        if depth > 0:
            if text.startswith("(*", i):
                out[i] = out[i + 1] = " "
                depth += 1
                i += 2
                continue
            if text.startswith("*)", i):
                out[i] = out[i + 1] = " "
                depth -= 1
                i += 2
                continue
            if c != "\n":
                out[i] = " "
            i += 1
            continue
        if text.startswith("(*", i):
            out[i] = out[i + 1] = " "
            depth = 1
            i += 2
            continue
        if c == '"':
            out[i] = " "
            i += 1
            while i < n and text[i] != '"':
                if text[i] == "\\" and i + 1 < n:
                    out[i] = " "
                    i += 1
                if text[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
                i += 1
            continue
        # a char literal, but not the quote of a type variable ('a) or a prime in a name
        if c == "'" and i + 2 < n and text[i + 2] == "'" and text[i + 1] != "\\":
            out[i] = out[i + 1] = out[i + 2] = " "
            i += 3
            continue
        i += 1
    return "".join(out)


def main(argv):
    # strip-ocaml-comments.py FILE              -> the stripped text
    # strip-ocaml-comments.py --grep RE FILE... -> path:lineno:line, over the stripped text
    #
    # The --grep mode exists so that a scan of a whole source tree can still be comment
    # aware.  Stripping every file would cost one process each; the caller narrows the
    # candidates with an ordinary grep first and passes only those here, so the raw pass
    # is a filter and this pass is the verdict.
    if argv and argv[0] == "--grep":
        import re

        rx = re.compile(argv[1])
        hits = 0
        for path in argv[2:]:
            with open(path, errors="replace") as fh:
                for n, line in enumerate(strip(fh.read()).split("\n"), 1):
                    if rx.search(line):
                        print("%s:%d:%s" % (path, n, line.rstrip()))
                        hits += 1
        return 0 if hits else 1
    sys.stdout.write(strip(open(argv[0]).read()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
