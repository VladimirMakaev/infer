#!/usr/bin/env python3
"""Capture the valid _bad/_ok shapes and require complete attribute printing."""
import argparse
import json
from pathlib import Path
import sqlite3
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--infer", required=True, type=Path)
parser.add_argument("--clang", required=True, type=Path)
args = parser.parse_args()
args.infer = args.infer.resolve()
if args.clang.is_absolute() or len(args.clang.parts) > 1:
    args.clang = args.clang.resolve()
source = Path(__file__).resolve().with_name("qualified_name_debug.cpp")
with tempfile.TemporaryDirectory(prefix="qualified-name-debug-") as temp:
    root = Path(temp)
    out = root / "infer-out"
    capture = subprocess.run(
        [str(args.infer), "capture", "--project-root", str(source.parent), "-o", str(out), "--",
         str(args.clang), "-std=c++17", "-c", str(source), "-o", str(root / "fixture.o")],
        text=True, capture_output=True, timeout=120, cwd=root)
    if capture.returncode:
        raise SystemExit(capture.stdout + capture.stderr)
    with sqlite3.connect(out / "capture.db") as db:
        captured = db.execute("SELECT proc_uid FROM procedures ORDER BY proc_uid").fetchall()
    if not any("entry_bad" in row[0] and "Support<" in row[0] for row in captured):
        raise SystemExit("the templated _bad procedure was not captured")
    if not any("entry_ok" in row[0] for row in captured):
        raise SystemExit("the plain _ok procedure was not captured")
    debug = subprocess.run(
        [str(args.infer), "debug", "--procedures", "--procedures-attributes", "-o", str(out)],
        text=True, capture_output=True, timeout=120, cwd=root)
    if debug.returncode:
        raise SystemExit(debug.stdout + debug.stderr)
    if "ParseError" in debug.stdout + debug.stderr:
        raise SystemExit("attribute printing raised ParseError")
    if debug.stdout.count("attributes:") != len(captured):
        raise SystemExit("attribute listing did not cover every captured procedure")
    for needle in ("entry_bad", "entry_ok", "Support<", "translation_unit="):
        if needle not in debug.stdout:
            raise SystemExit("missing required attribute output: " + needle)
    print(json.dumps({"captured": len(captured), "printed": len(captured), "exit": 0}))
