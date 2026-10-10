#!/usr/bin/env bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set -euo pipefail
infer_bin=$(realpath "$1")
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
bin_dir=$(dirname "$infer_bin")
compiler=$(realpath "$bin_dir/../../facebook-clang-plugins/clang/install/bin/clang")
plugin=$(realpath "$bin_dir/../../facebook-clang-plugins/libtooling/build/FacebookClangPlugin.dylib")
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/infer-ivar-bitfields.XXXXXX")
last_log=""
cleanup() {
  local code=$?
  if [[ $code != 0 && -f $last_log ]]; then sed -n '1,160p' "$last_log"; fi
  rm -rf -- "$test_dir"
}
trap cleanup EXIT
cp "$source_dir/ivar_bitfields.m" "$test_dir/ivars.m"

for language in objective-c objective-c++; do
  options=(-x "$language" --target=x86_64-unknown-linux-gnu -fobjc-runtime=gnustep-2.0)
  if [[ $language == objective-c++ ]]; then options+=(-std=c++11); fi
  last_log="$test_dir/$language-raw.log"
  "$compiler" "${options[@]}" -fsyntax-only "$test_dir/ivars.m" > "$last_log" 2>&1
  last_log="$test_dir/$language-capture.log"
  "$infer_bin" run --pulse-only --jobs 1 --no-progress-bar --project-root "$test_dir" \
    --results-dir "$test_dir/$language" -- "$compiler" "${options[@]}" \
    -fsyntax-only "$test_dir/ivars.m" > "$last_log" 2>&1
  last_log="$test_dir/$language-export.log"
  "$compiler" "${options[@]}" -fsyntax-only \
    -Xclang -load -Xclang "$plugin" -Xclang -plugin -Xclang JsonASTExporter \
    -Xclang -plugin-arg-JsonASTExporter -Xclang - "$test_dir/ivars.m" \
    > "$test_dir/$language.json" 2> "$last_log"
done

python3 - "$test_dir" <<'PY'
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
for language in ('objective-c', 'objective-c++'):
    reports = json.loads((root / language / 'report.json').read_text())
    assert len(reports) == 1, reports
    assert reports[0]['bug_type'] == 'NULLPTR_DEREFERENCE', reports
    assert reports[0]['procedure'].startswith('objc_bitfields_null_bad'), reports
    ivars = {}

    def visit(node):
        if isinstance(node, list):
            if len(node) == 2 and node[0] == 'ObjCIvarDecl':
                fields = node[1]
                assert len(fields) == 5, fields
                assert set(fields[1]) == {'name', 'qual_name'}, fields[1]
                ivars[fields[0]['pointer']] = fields
            for child in node:
                visit(child)
        elif isinstance(node, dict):
            for child in node.values():
                visit(child)

    visit(json.loads((root / (language + '.json')).read_text()))
    names = [fields[1]['name'] for fields in ivars.values()]
    assert len(names) >= 10, names
    assert len(names) == len(set(names)), names
    assert all(names), names
    anonymous = [fields for fields in ivars.values()
                 if fields[1]['name'].startswith('__anon_ivar_')]
    assert len(anonymous) >= 5, names
    assert all('bit_width_expr' in fields[3] for fields in anonymous), anonymous
    print('PASS: ' + language + ' ivar identity/schema and genuine null report')
PY
