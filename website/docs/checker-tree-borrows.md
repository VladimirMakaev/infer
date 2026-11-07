---
title: "Tree Borrows"
description: "A Rust-only aliasing checker that reports undefined behaviour."
---

A Rust-only aliasing checker that reports undefined behaviour.

Activate with `--tree-borrows`.

Supported languages:
- C/C++/ObjC: No
- C#/.Net: No
- Erlang: No
- Hack: No
- Java: No
- Python: No
- Rust: Experimental
- Swift: No

Detects undefined behaviour under Rust's Tree Borrows aliasing discipline, as a sub-domain of Pulse. Prototype, Rust-only, off by default.

## List of Issue Types

The following issue types are reported by this checker:
- [TREE_BORROWS_UB](/docs/next/all-issue-types#tree_borrows_ub)
