#pragma once

#include "codane/chat_provider.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace codane {

struct ConversationMetadata {
  std::string session_id;
  std::filesystem::path workspace;
  std::string provider_policy{"auto"};
  std::string model_policy{"auto"};
  std::string created_at;
  std::string updated_at;
};

struct ConversationTransition {
  std::string from_provider;
  std::string to_provider;
  std::string reason;
};

struct ConversationError {
  std::string kind;
  std::string message;
};

class ConversationSession {
 public:
  static ConversationSession create(const std::filesystem::path& chats_root,
                                    ConversationMetadata metadata);
  static ConversationSession load(const std::filesystem::path& chats_root,
                                  const std::string& session_id);

  const ConversationMetadata& metadata() const noexcept { return metadata_; }
  const std::vector<ChatMessage>& messages() const noexcept { return messages_; }
  const std::vector<ConversationTransition>& transitions() const noexcept { return transitions_; }
  const std::vector<ConversationError>& errors() const noexcept { return errors_; }

  void append_message(const ChatMessage& message);
  void append_transition(const ConversationTransition& transition);
  void append_error(const ConversationError& error);
  void update_selection(std::string provider_policy, std::string model_policy);

 private:
  ConversationSession(std::filesystem::path directory, ConversationMetadata metadata)
      : directory_(std::move(directory)), metadata_(std::move(metadata)) {}

  void write_snapshot();
  void append_event(const std::string& json);

  std::filesystem::path directory_;
  ConversationMetadata metadata_;
  std::vector<ChatMessage> messages_;
  std::vector<ConversationTransition> transitions_;
  std::vector<ConversationError> errors_;
};

}
