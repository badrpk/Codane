#include "codane/conversation.hpp"
#include "codane/workspace.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace codane;

namespace {
int failures = 0;

void check(std::string name, bool value) {
  if (!value) {
    std::cerr << "FAIL: " << name << '\n';
    ++failures;
  }
}

template <class F> void check_throws(std::string name, F fn) {
  try {
    fn();
    check(std::move(name), false);
  } catch (const std::runtime_error&) {
    check(std::move(name), true);
  }
}

struct TempDir {
  fs::path path;
  TempDir() {
    auto pattern = (fs::temp_directory_path() / "codane-conversation-XXXXXX").string();
    pattern.push_back('\0');
    path = ::mkdtemp(pattern.data());
  }
  ~TempDir() { std::error_code error; fs::remove_all(path, error); }
};
}

int main() {
  TempDir temp;
  const auto root = temp.path / "workspace";
  fs::create_directories(root / "src");
  fs::create_directory_symlink(temp.path, root / "outside-link");

  const auto policy = WorkspacePolicy::select(root / "src" / "..");
  check("workspace root is canonical", policy.root() == fs::canonical(root));
  check("dot-dot path inside workspace is allowed",
        policy.require_inside(root / "src" / ".." / "src") == fs::canonical(root / "src"));
  check("lexical escape is rejected", !policy.contains(root / ".." / "elsewhere"));
  check("symlink escape is rejected", !policy.contains(root / "outside-link" / "file"));
  check_throws("require_inside reports workspace escape", [&] {
    policy.require_inside(root / "outside-link" / "file");
  });
  check_throws("workspace selection requires existing directory", [&] {
    WorkspacePolicy::select(temp.path / "missing");
  });

  ConversationMetadata metadata;
  metadata.session_id = "chat-1";
  metadata.workspace = fs::canonical(root);
  metadata.provider_policy = "auto";
  metadata.model_policy = "auto";
  auto session = ConversationSession::create(temp.path / "chats", metadata);
  session.append_message({"user", "fix the parser"});
  session.append_message({"assistant", "I fixed it"});
  session.append_transition({"codex", "claude", "codex unavailable"});
  session.append_error({"unavailable", "provider temporarily unavailable"});
  session.update_selection("claude", "sonnet");

  auto resumed = ConversationSession::load(temp.path / "chats", "chat-1");
  check("session metadata survives restart",
        resumed.metadata().session_id == "chat-1" &&
            resumed.metadata().workspace == fs::canonical(root) &&
            resumed.metadata().provider_policy == "claude" &&
            resumed.metadata().model_policy == "sonnet" &&
            !resumed.metadata().created_at.empty() && !resumed.metadata().updated_at.empty());
  check("conversation messages survive restart",
        resumed.messages().size() == 2 && resumed.messages()[0].role == "user" &&
            resumed.messages()[0].content == "fix the parser" &&
            resumed.messages()[1].role == "assistant");
  check("provider transitions survive restart",
        resumed.transitions().size() == 1 && resumed.transitions()[0].to_provider == "claude");
  check("normalized errors survive restart",
        resumed.errors().size() == 1 && resumed.errors()[0].kind == "unavailable");

  const auto event_path = temp.path / "chats" / "chat-1" / "events.jsonl";
  {
    std::ofstream interrupted(event_path, std::ios::app | std::ios::binary);
    interrupted << "{\"type\":\"message\",\"role\":\"assistant\"";
  }
  resumed = ConversationSession::load(temp.path / "chats", "chat-1");
  check("interrupted final append is ignored", resumed.messages().size() == 2);
  resumed.append_message({"user", "continue"});
  resumed = ConversationSession::load(temp.path / "chats", "chat-1");
  check("append after interrupted record remains recoverable",
        resumed.messages().size() == 3 && resumed.messages().back().content == "continue");

  {
    std::ofstream corrupt(event_path, std::ios::app | std::ios::binary);
    corrupt << "not-json\n";
  }
  check_throws("complete corrupt journal record is reported", [&] {
    ConversationSession::load(temp.path / "chats", "chat-1");
  });

  const auto snapshot = temp.path / "chats" / "chat-1" / "session.json";
  {
    std::ofstream corrupt(snapshot, std::ios::binary | std::ios::trunc);
    corrupt << "{";
  }
  check_throws("corrupt session metadata is reported", [&] {
    ConversationSession::load(temp.path / "chats", "chat-1");
  });

  return failures == 0 ? 0 : 1;
}
