# Codane Chat-First Multi-Provider Design

## Purpose

Codane becomes a dependency-light C++20 conversational coding shell for Android Termux and Linux. Running `codane` in an interactive terminal opens a chat-first interface where the user can type natural-language instructions immediately. Existing graph/DAG commands remain supported as an advanced execution interface and retain their deterministic verification, persistence, retry, cancellation, and recovery guarantees.

The user should not need to understand JSON graphs, provider command syntax, or model IDs for normal use.

## User experience

Default interactive startup:

```text
CODANE
Workspace: ~/project
Provider: Auto
Model: Auto

You > make a simple calculator website
Codane > Using Codex CLI ...
         working ...

You > now add dark mode
Codane > ...
```

Normal text is sent as a conversational coding request. Slash commands expose controls only when needed:

- `/workspace` or `/folder` — choose/change workspace.
- `/provider` — choose Auto, a harness, or an API backend.
- `/model` and `/models` — choose/list models.
- `/providers` — show configured and detected providers.
- `/new` — start a new conversation.
- `/history` — list/reopen saved conversations.
- `/status` — show active workspace/provider/model/session.
- `/config` — open provider configuration.
- `/help` — concise help.
- `/exit` — leave Codane.

Existing CLI commands (`run`, `resume`, `status`, `inspect`, `cancel`, `validate`, `version`, `--events-json`) stay backward compatible.

## Safety and authority

Codane may let an approved coding harness or API-backed agent modify files and execute ordinary development commands inside the selected workspace without prompting for every edit.

Codane must require confirmation for operations that are outside the selected workspace, destructive beyond ordinary source edits, involve credentials/secrets, or create external side effects. Codane never silently broadens workspace authority.

The existing core authority model remains intact for graph execution: provider prose is not acceptance evidence, and deterministic verifiers remain authoritative for graph success.

## Architecture

The new interactive path is separate from, and layered beside, the graph engine:

```text
Interactive terminal
      |
      v
Chat TUI / line editor
      |
      v
ConversationSession ---- WorkspacePolicy
      |
      v
ProviderRouter ---- ProviderRegistry ---- ModelCatalog
      |
      +---------------- Harness providers
      |                   - Codex CLI
      |                   - Claude Code
      |                   - Grok CLI
      |                   - Gemini CLI
      |                   - Pi/custom harness
      |
      +---------------- API providers
                          - OpenRouter
                          - OpenAI
                          - Anthropic
                          - xAI
                          - Google Gemini
                          - generic OpenAI-compatible endpoint
```

The existing `AgentProvider` contract remains for graph execution. The conversational layer introduces a focused `ChatProvider` abstraction rather than forcing every interactive provider through graph-specific request/result types.

## Chat provider interface

Conceptual public types:

```cpp
enum class ProviderKind { Harness, Api };

struct ModelInfo {
    std::string id;
    std::string display_name;
    std::string vendor;
    std::vector<std::string> tags;
};

struct ProviderInfo {
    std::string id;
    std::string display_name;
    ProviderKind kind;
    bool configured;
    bool available;
};

struct ChatMessage {
    std::string role;
    std::string content;
};

struct ChatRequest {
    std::filesystem::path workspace;
    std::vector<ChatMessage> history;
    std::string user_text;
    std::string model;
};

struct ChatResult {
    bool ok;
    int exit_code;
    std::string text;
    std::string error;
    std::string provider_id;
    std::string model_id;
};

class ChatProvider {
public:
    virtual ~ChatProvider() = default;
    virtual ProviderInfo info() const = 0;
    virtual std::vector<ModelInfo> models() const = 0;
    virtual ChatResult send(const ChatRequest&, std::stop_token) = 0;
};
```

Provider implementations remain small and independently testable.

## Harness providers

Harness providers invoke installed coding-agent CLIs via Codane's existing safe POSIX process supervisor using argv vectors and stdin, never shell command construction.

First-class adapters:

1. `CodexHarnessProvider`
2. `ClaudeCodeHarnessProvider`
3. `GrokHarnessProvider`
4. `GeminiHarnessProvider`
5. `PiHarnessProvider`
6. `CustomHarnessProvider`

Each adapter owns only executable detection, argv construction, prompt/session transport, output parsing, and model discovery/configuration relevant to that harness.

Automated tests use fake executables/scripts or dependency-injected process results and never invoke a real paid or remote harness.

## API providers

API access is implemented without introducing a heavyweight runtime framework. HTTPS transport should be isolated behind a small `HttpTransport` interface so tests use a fake transport. The production implementation may use a platform-appropriate dependency already available on Termux/Linux (for example libcurl) if adding HTTPS from raw sockets would create unnecessary risk and complexity; this is the one justified optional runtime dependency for API mode.

First-class API adapters:

1. `OpenRouterProvider`
2. `OpenAIProvider`
3. `AnthropicProvider`
4. `XaiProvider`
5. `GeminiApiProvider`
6. `OpenAICompatibleProvider`

Secrets are never written into conversation journals. API keys are read from environment variables or a user-owned configuration file with restrictive permissions. The UI displays whether a provider is configured but never prints secret values.

## Provider registry and Auto routing

`ProviderRegistry` contains all harness/API adapters and reports availability/configuration.

Default `Auto` order is harness-first:

1. Codex CLI
2. Claude Code
3. Grok CLI
4. Gemini CLI
5. Pi/custom harnesses explicitly enabled by the user
6. OpenRouter API
7. configured direct APIs

Routing rules:

- Use the first healthy configured provider matching the user's requested model/capability.
- A selected explicit provider never silently switches unless the user enabled fallback for it.
- Auto may fall through on executable-not-found, authentication-unavailable, quota/rate-limit, or provider-unavailable errors.
- It must not fall through after an ambiguous provider result that may already have modified the workspace unless the adapter can establish that no mutation occurred.
- The UI reports the selected provider and any fallback transition succinctly.

## Model catalog

Codane ships with a small curated seed catalog and can refresh catalogs from providers when supported. The UI must not present a hardcoded ranking as permanent truth.

Default categories:

- Recommended
- Coding
- Reasoning
- Fast
- Low cost
- Open-weight
- All

The seed catalog includes at least ten representative current high-end/model-family entries spanning major vendors, but provider-discovered model IDs are authoritative. Model aliases are resolved per provider.

OpenRouter is treated as the easiest broad catalog backend when configured, while direct APIs remain available for users who prefer vendor-specific billing/authentication.

## Configuration

Configuration precedence:

1. explicit command/session selection
2. workspace configuration (`.codane/config.json` inside workspace, non-secret settings only)
3. user configuration (`~/.config/codane/config.json`)
4. environment variables
5. built-in defaults

Secrets are referenced by environment-variable name or stored only in a separate secret file with owner-only permissions if the user explicitly chooses local storage.

Configuration covers:

- default provider (`auto` by default)
- default model (`auto` by default)
- harness executable paths and optional extra argv
- API base URLs
- API-key environment variable names
- enabled/disabled providers
- fallback policy
- per-provider timeouts/output limits

## Conversation persistence

Interactive sessions persist separately from graph runs:

```text
~/.local/state/codane/chats/<session-id>/session.json
~/.local/state/codane/chats/<session-id>/events.jsonl
```

Persist only conversation metadata/messages needed to resume. Never persist API keys or raw environment dumps.

Each session records:

- session ID
- workspace
- selected provider/model policy
- timestamps
- user/assistant messages
- provider transitions/errors

Writes use temp-file + fsync + atomic rename for snapshots, following the existing crash-safety style.

## Workspace selection

The current working directory is the default workspace. `/workspace` invokes a simple native C++ folder browser; users can navigate directories and create folders without typing paths. The existing graph JSON browser remains available only where graph commands need it.

Workspace canonicalization prevents `..`/symlink confusion from silently broadening the approved root.

## Terminal behavior

The chat UI remains dependency-light and usable on small Termux screens. It uses line-oriented input for normal prompts so Android keyboards work naturally. Raw terminal mode is limited to focused selectors (provider/model/folder) and restored reliably on errors/signals.

The interface must not require ncurses, Qt, a GUI, Node, or Python.

## Compatibility with the graph engine

No existing graph capabilities are removed. `CodexProvider` used by graphs remains supported. The new chat adapters may share lower-level process/config helpers, but graph acceptance semantics stay unchanged.

`codane run graph.json --provider codex` continues to work exactly as before.

## Testing

All work follows incremental TDD.

Automated tests must cover:

- bare interactive entry routes to chat TUI when stdin/stdout are terminals
- non-interactive bare invocation does not hang
- slash-command parsing
- workspace canonicalization/boundaries
- session persistence/recovery
- provider detection
- explicit provider selection
- harness-first Auto ordering
- safe fallback on pre-execution/provider-unavailable failures
- no fallback after ambiguous mutation-capable execution
- model catalog categories and alias resolution
- secret redaction/non-persistence
- argv-safe harness invocation
- fake HTTP API request/response parsing
- API error normalization (auth, rate limit, unavailable)
- existing graph/CLI test suite remains green
- no automated test calls a real remote model or paid service

## Delivery phases

The architecture is delivered incrementally so every phase remains usable:

1. Chat shell, workspace selection, sessions, slash commands, Codex CLI adapter.
2. Provider registry and harness adapters (Claude Code, Grok CLI, Gemini CLI, Pi/custom).
3. HTTP transport and OpenRouter/generic OpenAI-compatible APIs.
4. Direct OpenAI/Anthropic/xAI/Gemini API adapters.
5. Model catalog refresh, category UX, robust Auto fallback.

Each phase preserves the graph engine and passes the full CTest suite before continuing.

## Definition of done

The feature is complete when:

1. `codane` opens a natural-language chat in an interactive Termux/Linux terminal.
2. A user can select/create a workspace without typing a path.
3. Chat works through Codex CLI with safe argv-based execution.
4. Codane can detect/configure Codex CLI, Claude Code, Grok CLI, Gemini CLI, and Pi/custom harnesses.
5. Codane supports OpenRouter plus direct OpenAI, Anthropic, xAI, Gemini, and generic OpenAI-compatible APIs.
6. Auto routing is harness-first and follows the mutation-safe fallback rule.
7. Provider/model configuration persists without leaking secrets.
8. At least ten seed model-family entries and refreshable provider model catalogs are available.
9. Conversation history survives restart.
10. Existing graph commands and deterministic verification behavior remain compatible.
11. All automated tests are offline/fake and pass under CTest.
12. Termux and Linux build instructions are updated.
13. `git diff --check`, CMake configure/build, CTest, fake smoke, CLI help/version, and interactive fake-provider smoke all pass.
