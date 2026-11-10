#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

"""Measure parsing, AST export, AST ingestion and capture separately.

Pass --infer-root for a built Infer installation and --output for a new result
directory. By default this generates a 30M-element array. --source instead
measures an existing translation unit, including its normal relative includes.
Peak RSS is reported in KiB for each timed command; the Clang parse and the
isolated Infer ingestion have separate measurements.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import re
import subprocess


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--infer-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--elements", type=int, default=30_000_000)
    args = parser.parse_args()
    if args.elements <= 0:
        parser.error("--elements must be positive")
    root, output = args.infer_root.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    clang = root / "lib/infer/facebook-clang-plugins/clang/install/bin/clang++"
    plugin = root / "lib/infer/facebook-clang-plugins/libtooling/build/FacebookClangPlugin.dylib"
    infer = root / "bin/infer"
    for tool in (clang, plugin, infer):
        if not tool.is_file():
            parser.error(f"missing installed component: {tool}")
    source = args.source.resolve() if args.source else output / "data.cpp"
    if not args.source:
        rng = random.Random(0)
        with source.open("w") as stream:
            stream.write("static const unsigned char data_raw[] = {\n")
            for i in range(0, args.elements, 12):
                row = (f"0x{rng.randrange(256):02x}" for _ in range(min(12, args.elements - i)))
                stream.write("  " + ", ".join(row) + ",\n")
            stream.write("};\nunsigned int data_raw_len = sizeof(data_raw);\n")
    ast = output / "ast.biniou"
    compile_args = [str(clang), "-std=c++17", "-c", str(source), "-o", str(output / "data.o")]
    capture_args = [str(infer), "capture", "--jobs", "1", "--project-root", str(source.parent)]
    export_args = [
        str(clang),
        "-std=c++17",
        "-fsyntax-only",
        "-Xclang",
        "-load",
        "-Xclang",
        str(plugin),
        "-Xclang",
        "-plugin",
        "-Xclang",
        "BiniouASTExporter",
        "-Xclang",
        "-plugin-arg-BiniouASTExporter",
        "-Xclang",
        str(ast),
        str(source),
    ]
    commands = {
        "parse": [str(clang), "-std=c++17", "-fsyntax-only", str(source)],
        "export": export_args,
        "ingest": capture_args
        + ["--clang-biniou-file", str(ast), "-o", str(output / "ingest")]
        + ["--"]
        + compile_args,
        "capture": capture_args + ["-o", str(output / "capture"), "--"] + compile_args,
    }
    results = {
        "source": str(source),
        "source_sha256": sha256(source),
        "source_bytes": source.stat().st_size,
        "components": {str(tool): sha256(tool) for tool in (clang, plugin, infer)},
        "stages": {},
    }
    (output / "commands.json").write_text(json.dumps(commands, indent=2))
    for stage, command in commands.items():
        timing = output / f"{stage}.time"
        with (output / f"{stage}.log").open("w") as log:
            result = subprocess.run(
                ["/usr/bin/time", "-v", "-o", str(timing)] + command,
                cwd=source.parent,
                stdout=log,
                stderr=subprocess.STDOUT,
                env=dict(os.environ, LC_ALL="C"),
            )
        text = timing.read_text()
        rss = re.search(r"Maximum resident set size \(kbytes\): (\d+)", text)
        wall = re.search(r"Elapsed \(wall clock\) time .*: ([\d:.]+)", text)
        results["stages"][stage] = {
            "exit_code": result.returncode,
            "maxrss_kib": int(rss[1]) if rss else None,
            "wall_time": wall[1] if wall else None,
        }
        if stage == "export":
            results["export_bytes"] = ast.stat().st_size if ast.exists() else 0
        (output / "results.json").write_text(json.dumps(results, indent=2))
        print(stage, results["stages"][stage], flush=True)
        if result.returncode:
            raise SystemExit(result.returncode)
        if stage == "export" and not args.source and args.elements > 10_000:
            # Array dimensions and the initialization marker are bounded in
            # size. Re-exporting element trees must fail this regression probe.
            assert results["export_bytes"] < 65_536, results["export_bytes"]


if __name__ == "__main__":
    main()
