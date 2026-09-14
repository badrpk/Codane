# Codane Chat-First Multi-Provider Implementation Plan

## Goal

Turn Codane into a native C++20 conversational coding environment.

Running:

    codane

in an interactive terminal must immediately provide a natural-language
prompt. A normal user must not need graph JSON files.

Existing graph/DAG execution remains fully supported for advanced use.

## Fundamental UX

Example:

    CODANE
    Workspace: ~/vivo/myapp
    Provider: Auto
    Model: Auto

    You > make a modern calculator website

    Codane > Using Codex CLI
             ...

    You > add dark mode

Normal text is an AI coding instruction.

Slash commands:

    /workspace
    /folder
    /provider
    /providers
    /model
    /models
    /new
    /history
    /status
    /config
    /help
    /exit

## C++ only

Runtime implementation must remain C++20.

Do not introduce Python, Node, Qt, ncurses, Docker, web server,
or another application runtime.

POSIX tools/programs may be invoked safely through Codane's existing
argv-vector process supervisor.

Never use arbitrary shell command construction.

## Architecture

Add these focused components beside the existing graph engine:

    ChatProvider
    ProviderRegistry
    ProviderRouter
    HarnessProvider
    HttpTransport
    ApiProvider
    ModelCatalog
    WorkspacePolicy
    ConversationSession
    ChatTui
    Config

Do not force conversational providers through graph-specific acceptance
semantics.

The existing AgentProvider and graph execution remain compatible.

## ChatProvider

Provide types conceptually equivalent to:

    enum class ProviderKind { Harness, Api };

    enum class ChatFailureKind {
        None,
        NotFound,
        NotConfigured,
        Auth,
        RateLimited,
        Unavailable,
        Timeout,
        Cancelled,
        AmbiguousExecution,
        InvalidResponse,
        Other
    };

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
        ChatFailureKind failure;
        bool workspace_may_have_changed;
    };

    class ChatProvider {
    public:
        virtual ~ChatProvider() = default;
        virtual ProviderInfo info() const = 0;
        virtual std::vector<ModelInfo> models() const = 0;
        virtual ChatResult send(
            const ChatRequest&,
            std::stop_token
        ) = 0;
    };

## Harness providers

First-class configurable harnesses:

    Codex CLI
    Claude Code
    Grok CLI
    Gemini CLI
    Pi
    Custom CLI

Use one generic HarnessProvider implementation where possible and
small vendor presets.

Harness executable names and argv must be configurable.

Codex adapter must support the existing proven form:

    codex exec --json -

Prompt is supplied safely without constructing a shell command.

Provider detection must check executable availability.

Never invoke real remote harnesses from CTest.

## APIs

Support:

    OpenRouter
    OpenAI
    Anthropic
    xAI
    Google Gemini
    generic OpenAI-compatible endpoint

HTTP must be behind an injectable HttpTransport.

Automated tests use FakeHttpTransport only.

Production API transport may safely invoke the curl executable using the
existing argv-vector process supervisor.

No API key may appear in:

    conversation history
    events.jsonl
    snapshot JSON
    terminal status
    errors
    debug output

Configuration stores environment-variable names, not secret values,
unless explicit secure local secret storage is implemented.

## Auto routing

Default order:

    1. Codex CLI
    2. Claude Code
    3. Grok CLI
    4. Gemini CLI
    5. explicitly enabled Pi/custom harnesses
    6. OpenRouter
    7. configured direct APIs

Safe fallback rule is mandatory.

Fallback may occur for failures such as:

    executable missing
    provider not configured
    authentication unavailable
    quota/rate limit
    provider unavailable

ONLY when Codane can establish that the workspace was not mutated.

If:

    workspace_may_have_changed == true

then automatic fallback MUST STOP.

Never run the same mutation-capable request through another model after
an ambiguous first execution.

Explicit provider choice must not silently switch providers unless the
user explicitly enables fallback.

## Model system

Implement a refreshable model catalog.

Commands:

    /model
    /models
    /models recommended
    /models coding
    /models reasoning
    /models fast
    /models cheap
    /models open
    /models all

Ship at least ten useful representative model/family entries.

Do NOT describe the built-in list as an objectively permanent ranking.

Call the main list:

    Codane Recommended

and limit its default display to 10.

Provider-discovered model IDs override stale built-in IDs.

OpenRouter should be able to expose its broad model catalog when
configured.

Model aliases must be provider-specific.

## Workspace

Current directory is the initial workspace.

Users can change it using:

    /workspace
    /folder

Provide native C++ folder navigation:

    Up/Down
    Enter
    Backspace/Left = parent
    N = create folder
    select current folder
    Q = cancel

No path typing should be necessary for normal operation.

Canonicalize the workspace.

Never silently expand authority outside it.

## Conversation persistence

Store chats separately from graph runs under:

    ~/.local/state/codane/chats/<session-id>/

Use:

    session.json
    events.jsonl

Persist:

    session id
    workspace
    provider policy
    model policy
    timestamps
    user messages
    assistant messages
    provider transitions
    normalized provider errors

Never persist credentials or raw environment dumps.

Use crash-safe temp + fsync + atomic rename semantics.

Commands:

    /new
    /history

must work.

## Configuration

Precedence:

    explicit current-session selection
    workspace .codane/config.json
    ~/.config/codane/config.json
    environment
    defaults

Default:

    provider = auto
    model = auto

Support configurable:

    harness executable
    harness extra argv
    API base URL
    API-key environment variable name
    provider enable/disable
    fallback
    timeout
    output limits

## Chat TUI

Normal user typing must use line-oriented input.

Android keyboard input must work normally.

Raw terminal mode should only be used for selectors/browser screens.

Bare interactive:

    codane

opens chat.

Bare NON-interactive invocation must never block waiting for input.

Existing commands remain:

    codane run
    codane resume
    codane status
    codane inspect
    codane cancel
    codane validate
    codane version
    codane --help
    codane --events-json ...

## Existing graph engine

Preserve:

    DAG execution
    retries
    persistence
    cancellation
    verification
    artifacts
    recovery
    deterministic success semantics

Provider prose still cannot establish graph success.

## Required tests

All automated tests are offline.

Test at least:

    ChatProvider contract
    harness argv safety
    Codex argv shape
    workspace cwd
    executable missing
    provider discovery
    explicit provider selection
    Auto provider ordering
    safe fallback
    fallback blocked after possible mutation
    HTTP fake transport
    API auth errors
    API rate-limit errors
    API unavailable errors
    invalid API responses
    secret redaction
    model catalog >= 10 entries
    recommended view <= 10
    categories
    provider-specific aliases
    workspace canonicalization
    workspace escape prevention
    session persistence
    session recovery
    slash command parsing
    /workspace
    /provider
    /model
    /models
    /new
    /history
    /status
    /config
    /help
    /exit
    non-interactive bare invocation does not hang
    all legacy graph tests

Tests MUST NOT call:

    real Codex
    real Claude
    real Grok
    real Gemini
    real Pi
    OpenRouter
    OpenAI
    Anthropic
    xAI
    Google APIs

## Definition of done

Required mechanical proof:

    git diff --check

    rm -rf build
    cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++

    cmake --build build -j2

    ctest --test-dir build --output-on-failure

    bash scripts/smoke_fake.sh
    bash scripts/smoke_chat_fake.sh

    ./build/codane --help
    ./build/codane version
    ./build/codane validate examples/simple.json

Do not claim completion without actual execution evidence.
