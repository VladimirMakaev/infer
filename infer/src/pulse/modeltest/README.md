# Direct library model adapter

This standalone executable invokes actual Pulse model dispatch on synthetic abstract states.
It does not run C++ capture, execute a CFG, or apply callee summaries. It is a test interface;
it does not change library model semantics or run as part of the analyzer executable.

Build with the repository's normal development setup:

```sh
dune build infer/src/pulse/modeltest/model_contract_driver.exe
```

Provide a JSON object on standard input and a fresh results directory:

```sh
model_contract_driver.exe analyze --pulse-only --pulse-test-observability \
  --results-dir model-results < operations.json
```

An example input exercising ownership and a non-owning alias:

```json
{
  "schema_version": 1,
  "case_id": "unique.reset.alias",
  "objects": [{"name": "u", "kind": "unique"}],
  "operations": [
    {"op": "construct", "object": "u"},
    {"op": "allocate", "name": "p"},
    {"op": "adopt", "object": "u", "value": "p"},
    {"op": "alias", "object": "u", "name": "a"},
    {"op": "reset", "object": "u"},
    {"op": "use", "name": "a"}
  ]
}
```

The output preserves all model outcomes and snapshots. Slot IDs are opaque: compare equality
between bindings, rather than numeric IDs across runs. Vector size queries publish feasible
values from zero through eight; they do not imply a global size bound. Capacity is explicitly
unrepresented. The independent test oracle must supply the obligation; this executable never
creates an expected result from its own output. Call locations use one-based operation indices.
Named model rules and actual dispatch routes use the opt-in observation interface.

Scope: vector<int>, unique_ptr<int, default_delete<int>>, an LP64 synthetic source context,
scalar allocation and iterator dereference. Typed element constructors/destructors, custom
deleter callbacks, exceptions, compiler lowering and summary composition require end-to-end
C++ tests. An unavailable model or callback is explicit; initialization failure exits nonzero.
Stopped model states are retained for evidence and do not execute later operations.
