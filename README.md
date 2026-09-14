# Codane

Codane is a dependency-light C++20 orchestration harness for Codex agents. It owns scheduling, process lifecycle, graph state, retries, verification, cancellation, persistence, and acceptance; agent prose is never acceptance evidence.

## Architecture

`include/codane` and `src` contain runtime IDs/states, a bounded POSIX process supervisor, `AgentProvider`/`FakeProvider`/`CodexProvider`, deterministic verifiers, focused JSON, append-only journals, DAG validation, and bounded loop policy. Process execution uses argv vectors, stdin pipes, separate nonblocking stdout/stderr, process groups, timeout/cancellation escalation, output limits, and `waitpid`.

The CLI defaults to the deterministic `fake` provider; production execution is explicit with `--provider codex`, while tests and `smoke_fake.sh` always use fake. The Codex adapter runs `codex exec --json -`, sends the prompt on stdin, and accepts configurable executable, model, working directory, extra argv, timeout, and output limits. No shell command is constructed.

## Build

Linux and Android Termux:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The runtime requires C++20, CMake, Ninja, and a POSIX system. It does not require Python, Node, Boost, Qt, Docker, or a network service.

## Graphs and CLI

A graph is JSON with `name`, optional `max_parallel`, and `nodes`. Nodes have `id`, `type` (`agent` or `command`), `prompt` or argv `command`, `depends_on`, and optional deterministic `verify` (`command`, `file_exists`, `file_content`, or `all_of`). Command verifiers remain argv arrays; graph strings are never converted to `sh -c`.

```sh
codane validate examples/simple.json
codane run examples/simple.json --provider fake
codane resume RUN_ID
codane status RUN_ID
codane inspect RUN_ID
codane cancel RUN_ID
codane version
codane --help
```

The example files demonstrate a simple dependency, fan-out/fan-in, and bounded repair-loop policy. Use `codane --events-json validate examples/simple.json` for JSONL-only stdout; diagnostics remain on stderr. Parallelism is bounded by `max_parallel`; graph nodes do not create permanent threads. Cycles, unknown dependencies, graph limits, process deadlines, retries, and failed-dependency propagation are deterministic.

## Persistence, loops, verification, and artifacts

Runs use `.codane/runs/<run-id>/events.jsonl`, atomic snapshots, and per-node `.codane/runs/<run-id>/artifacts/<node-id>/` directories. Journal events are sequenced JSONL and durable checkpoints flush and fsync. Recovery replays events, preserves verified successes, and converts old `running` nodes to interrupted unfinished work. Loops require iteration, wall-time, failure, and optional stagnation bounds; accepted iterations checkpoint progress. Downstream prompts consume declared artifact paths only.

Command, file-exists, file-content, and all-of verifiers are deterministic and run outside agent authority. `--events-json` reserves stdout for machine-readable events while diagnostics belong on stderr.

## Safety and limitations

Codane never uses `system`, `popen`, or `sh -c` for graph work. Child groups receive SIGTERM followed by bounded SIGKILL escalation. Output is bounded and truncation is explicit. `scripts/smoke_fake.sh` is deterministic and never contacts Codex; `scripts/smoke_codex.sh` is an explicit human-only smoke entry point.

Graph mutation is bounded and atomic through the Graph API; CLI resume reconstructs snapshots and later journal events, preserves verified successes and attempts, and schedules unfinished nodes. Active runs own a run-local `runner.json`; `codane cancel` writes a durable request that the live runner polls, causing the child process group to receive SIGTERM and bounded SIGKILL escalation. Stale ownership metadata is never used to signal arbitrary PIDs. No unverified agent result can become success. The retained Python safe patch-planning kernel remains in `codane.py` with its original tests.
