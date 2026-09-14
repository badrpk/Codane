# Codane C++ Harness

Codane is a highly efficient C++20 orchestration harness for Codex.

Codane must support:

- long-running autonomous loops;
- parallel Codex agents;
- dependency graphs / DAGs;
- fan-out and fan-in;
- machine-verifiable completion;
- bounded retries;
- deadlines;
- cancellation;
- persistent execution history;
- crash-safe checkpoints;
- resume after interruption;
- explicit artifacts;
- bounded dynamic graph growth.

## Core authority model

Codex reasons and performs agent work.

Codane owns:

- scheduling;
- process lifecycle;
- graph state;
- retries;
- verification;
- acceptance;
- persistence;
- cancellation;
- resource limits.

Agent prose never establishes success.

A task becomes successful only through deterministic verification.

## Platform

Primary targets:

- Android Termux
- Linux

Language:

- C++20

Build:

- CMake
- Ninja

Avoid unnecessary runtime dependencies.

Do not require:

- Python
- Node.js
- Docker
- Kubernetes
- Boost
- Qt
- a GUI
- a web server

for the Codane runtime.

## Architecture

Implement focused modules approximately equivalent to:

    include/codane/
        agent.hpp
        codex_provider.hpp
        graph.hpp
        journal.hpp
        loop.hpp
        process.hpp
        runtime.hpp
        verifier.hpp

    src/
        agent.cpp
        codex_provider.cpp
        graph.cpp
        journal.cpp
        loop.cpp
        main.cpp
        process.cpp
        runtime.cpp
        verifier.cpp

    tests/
    scripts/
    examples/

The exact decomposition may improve on this if justified.

## 1. Runtime

Provide:

- RunId
- NodeId
- monotonic deadlines
- bounded retry policy
- exponential backoff
- cancellation
- structured state transitions
- event sequence numbers

Prefer:

- std::chrono
- std::jthread
- std::stop_token
- std::condition_variable
- std::filesystem
- STL containers

## 2. Process supervisor

Implement safe POSIX process execution.

Requirements:

- argv-vector API
- no shell interpolation
- fork/exec or posix_spawn
- stdin pipe
- separate stdout and stderr capture
- process groups
- nonblocking output reads
- poll/ppoll where useful
- timeout
- cancellation
- SIGTERM followed by bounded SIGKILL escalation
- waitpid/reaping
- no zombies
- captured-output byte limits
- explicit truncation flag
- real exit status

Never use arbitrary:

    system()
    popen()
    sh -c

for graph commands.

## 3. Provider interface

Provide:

    AgentProvider

with a deterministic FakeProvider for tests.

Implement:

    CodexProvider

CodexProvider must invoke Codex safely using the process supervisor.

Conceptual command:

    codex exec --json -

Prompt is supplied through stdin.

Support configuration for:

- codex executable
- model
- cwd
- extra argv
- timeout
- output limits

Do not build shell command strings.

## 4. Agent request/result

Agent request should represent:

- id
- name
- prompt
- workspace
- model
- attempt
- timeout
- environment
- metadata

Result should represent:

- status
- exit code
- signal
- stdout
- stderr
- duration
- truncation
- event references

## 5. Verifiers

Implement deterministic verifier types:

- CommandVerifier
- FileExistsVerifier
- FileContentVerifier
- AllOfVerifier

CommandVerifier must use argv arrays.

Agents cannot modify verifier results.

## 6. DAG engine

Node states:

- Pending
- Ready
- Running
- Succeeded
- Failed
- Blocked
- Cancelled
- Skipped

Required:

- dependency validation
- cycle rejection
- dependency ordering
- fan-out
- fan-in
- retries
- failed dependency propagation
- fail-fast mode
- continue-independent mode
- cancellation

Node scheduling must not busy-spin.

## 7. Parallel scheduler

Implement bounded concurrency.

Configuration:

    max_parallel

Hundreds of logical graph nodes must not imply hundreds of persistent
threads.

Use a bounded worker pool or similarly efficient event-driven design.

## 8. Long-running loop engine

Support policies including:

- max_iterations
- max_wall_time
- max_consecutive_failures
- stagnation_limit
- retry/backoff
- cancellation

No accidentally unbounded loop is allowed.

After every accepted iteration, durable progress must be checkpointed.

A resumed loop must not re-run already verified accepted iterations.

## 9. Dynamic graphs

Successful nodes may propose bounded new nodes/edges.

Before mutation, validate:

- unique IDs
- total node maximum
- graph depth maximum
- cycle freedom

Mutation must be atomic.

Invalid dynamic additions must leave the original graph unchanged.

## 10. Journal

Append-only JSON Lines:

    .codane/runs/<run-id>/events.jsonl

Each event must contain at least:

- schema_version
- sequence
- timestamp
- run_id
- event_type
- optional node_id
- payload

Journal behavior:

- append
- flush
- fsync at durable checkpoints
- never silently truncate an existing journal

## 11. Snapshots

Path:

    .codane/runs/<run-id>/snapshot.json

Snapshot must capture sufficient reconstructed state for:

- graph
- node states
- attempts
- loop progress
- retry state
- verified successes

Write snapshots using:

1. temporary file
2. flush/fsync
3. atomic rename

Journal remains authoritative.

## 12. Recovery

Command:

    codane resume <run-id>

Recovery must:

- load snapshot
- replay later events
- preserve verified successes
- treat old Running nodes as interrupted
- never convert interrupted work into success
- continue only unfinished work

Malformed journal content must produce a clear error.

Do not silently ignore corruption.

## 13. Artifacts

Per-node artifact directory:

    .codane/runs/<run-id>/artifacts/<node-id>/

Downstream tasks consume explicitly declared artifact paths.

Do not concatenate all prior stdout into later prompts automatically.

## 14. Resource limits

Support:

- max_parallel
- max_graph_nodes
- max_graph_depth
- max_attempts
- process timeout
- graph timeout
- max stdout bytes
- max stderr bytes

All queues and output buffers must remain bounded.

## 15. Graph format

Use small JSON graph files.

Example:

    {
      "name": "example",
      "max_parallel": 3,
      "nodes": [
        {
          "id": "plan",
          "type": "agent",
          "prompt": "Create a plan."
        },
        {
          "id": "implement",
          "type": "agent",
          "depends_on": ["plan"],
          "prompt": "Implement it."
        },
        {
          "id": "verify",
          "type": "command",
          "depends_on": ["implement"],
          "command": ["ctest", "--test-dir", "build"]
        }
      ]
    }

Keep JSON support focused.

Do not introduce a heavyweight framework only for JSON.

## 16. CLI

Required:

    codane run <graph.json>
    codane resume <run-id>
    codane status <run-id>
    codane inspect <run-id>
    codane cancel <run-id>
    codane validate <graph.json>
    codane version

Provide:

    codane --help

Also support:

    --events-json

In JSON-event mode:

- stdout is machine-readable JSONL
- human diagnostics go to stderr

## 17. Tests

Automated tests must NEVER invoke real Codex.

Use FakeProvider.

At minimum test:

1. cycle detection
2. dependency ordering
3. fan-out
4. fan-in
5. concurrency bound
6. failed dependency propagation
7. retries
8. process timeout
9. process cancellation
10. stdout capture
11. stderr capture
12. nonzero child status
13. output truncation
14. verifier pass
15. verifier failure
16. journal append
17. event ordering
18. snapshot atomic replacement
19. replay
20. interrupted-node recovery
21. stagnation bound
22. loop iteration bound
23. dynamic cycle rejection
24. dynamic node-limit rejection
25. artifact paths
26. CLI graph validation
27. CLI status

Use CTest.

Do not download GoogleTest or Catch2 during configuration.

A small internal C++ test harness is acceptable.

## 18. Smoke tests

Provide:

    scripts/smoke_fake.sh
    scripts/smoke_codex.sh

smoke_fake.sh must be deterministic and free.

smoke_codex.sh must be explicitly invoked by a human.

Never run smoke_codex.sh from:

- CTest
- configure
- build
- smoke_fake
- normal installation

## 19. Examples

Provide:

    examples/simple.json
    examples/parallel.json
    examples/repair_loop.json

## 20. Documentation

README must document:

- what Codane is
- architecture
- Termux build
- Linux build
- Codex adapter
- graph schema
- parallel agents
- long loops
- verification
- persistence
- resume
- artifacts
- structured events
- limitations
- safety model

## Definition of done

Do not claim completion until all are proven:

1. clean CMake configure
2. Clang build succeeds
3. tests compile
4. all CTest tests pass
5. smoke_fake exits 0
6. git diff --check exits 0
7. CLI help works
8. version works
9. valid graph is accepted
10. cycle graph is rejected
11. concurrency is bounded
12. loops are bounded
13. no test invokes real Codex
14. interrupted work cannot become successful after resume
15. verified successes survive resume
16. CodexProvider uses safe argv-based process execution
17. Termux instructions exist
