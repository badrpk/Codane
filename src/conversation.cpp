#include "codane/conversation.hpp"

#include "codane/json.hpp"

#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace codane {
namespace fs = std::filesystem;
namespace {

std::runtime_error io_error(const std::string& action, const fs::path& path) {
  return std::runtime_error(action + ": " + path.string());
}

void write_all(int fd, const std::string& data, const fs::path& path) {
  std::size_t offset = 0;
  while (offset < data.size()) {
    const auto count = ::write(fd, data.data() + offset, data.size() - offset);
    if (count < 0) {
      if (errno == EINTR) continue;
      throw io_error("cannot write conversation file", path);
    }
    offset += static_cast<std::size_t>(count);
  }
}

void sync_directory(const fs::path& directory) {
  const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) throw io_error("cannot open conversation directory", directory);
  const int result = ::fsync(fd);
  const int saved_errno = errno;
  ::close(fd);
  if (result != 0) {
    errno = saved_errno;
    throw io_error("cannot sync conversation directory", directory);
  }
}

void atomic_write(const fs::path& path, const std::string& data) {
  const auto temporary = path.string() + ".tmp." + std::to_string(::getpid());
  const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (fd < 0) throw io_error("cannot create temporary conversation file", temporary);
  try {
    write_all(fd, data, temporary);
    if (::fsync(fd) != 0) throw io_error("cannot sync temporary conversation file", temporary);
    if (::close(fd) != 0) throw io_error("cannot close temporary conversation file", temporary);
    if (::rename(temporary.c_str(), path.c_str()) != 0) {
      throw io_error("cannot replace conversation file", path);
    }
    sync_directory(path.parent_path());
  } catch (...) {
    ::close(fd);
    ::unlink(temporary.c_str());
    throw;
  }
}

std::string now_utc() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm value{};
  gmtime_r(&time, &value);
  std::ostringstream output;
  output << std::put_time(&value, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

void validate_id(const std::string& id) {
  if (id.empty() || id == "." || id == ".." || id.find('/') != std::string::npos ||
      id.find('\\') != std::string::npos) {
    throw std::runtime_error("invalid conversation session id");
  }
}

std::string read_file(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw io_error("cannot read conversation file", path);
  return std::string(std::istreambuf_iterator<char>(input), {});
}

Json parse_object(const std::string& text, const fs::path& path) {
  try {
    auto value = Json::parse(text);
    if (!value.is_object()) throw std::runtime_error("not an object");
    return value;
  } catch (const std::exception&) {
    throw std::runtime_error("corrupt conversation data: " + path.string());
  }
}

std::string required_string(const Json& value, const std::string& key, const fs::path& path) {
  try {
    return value.at(key).str();
  } catch (const std::exception&) {
    throw std::runtime_error("invalid conversation field '" + key + "': " + path.string());
  }
}

void discard_incomplete_tail(const fs::path& path) {
  if (!fs::exists(path)) return;
  const auto data = read_file(path);
  if (data.empty() || data.back() == '\n') return;
  const auto last_newline = data.find_last_of('\n');
  const auto valid_size = last_newline == std::string::npos ? 0 : last_newline + 1;
  std::error_code error;
  fs::resize_file(path, valid_size, error);
  if (error) throw io_error("cannot recover interrupted conversation append", path);
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) throw io_error("cannot open recovered conversation journal", path);
  const int result = ::fsync(fd);
  ::close(fd);
  if (result != 0) throw io_error("cannot sync recovered conversation journal", path);
  sync_directory(path.parent_path());
}
}

ConversationSession ConversationSession::create(const fs::path& chats_root,
                                                ConversationMetadata metadata) {
  validate_id(metadata.session_id);
  if (metadata.workspace.empty()) throw std::runtime_error("conversation workspace is required");
  std::error_code error;
  metadata.workspace = fs::canonical(metadata.workspace, error);
  if (error || !fs::is_directory(metadata.workspace)) {
    throw std::runtime_error("conversation workspace must be an existing directory");
  }
  fs::create_directories(chats_root, error);
  if (error) throw io_error("cannot create chats directory", chats_root);
  const auto directory = chats_root / metadata.session_id;
  if (!fs::create_directory(directory, error) || error) {
    throw std::runtime_error("conversation session already exists or cannot be created: " +
                             metadata.session_id);
  }
  sync_directory(chats_root);
  metadata.created_at = now_utc();
  metadata.updated_at = metadata.created_at;
  ConversationSession session(directory, std::move(metadata));
  session.write_snapshot();
  const auto journal = directory / "events.jsonl";
  const int fd = ::open(journal.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0) throw io_error("cannot create conversation journal", journal);
  if (::fsync(fd) != 0) { ::close(fd); throw io_error("cannot sync conversation journal", journal); }
  ::close(fd);
  sync_directory(directory);
  return session;
}

ConversationSession ConversationSession::load(const fs::path& chats_root,
                                              const std::string& session_id) {
  validate_id(session_id);
  const auto directory = chats_root / session_id;
  const auto snapshot_path = directory / "session.json";
  const auto snapshot = parse_object(read_file(snapshot_path), snapshot_path);
  ConversationMetadata metadata;
  metadata.session_id = required_string(snapshot, "session_id", snapshot_path);
  if (metadata.session_id != session_id) throw std::runtime_error("conversation session id mismatch");
  metadata.workspace = required_string(snapshot, "workspace", snapshot_path);
  metadata.provider_policy = required_string(snapshot, "provider_policy", snapshot_path);
  metadata.model_policy = required_string(snapshot, "model_policy", snapshot_path);
  metadata.created_at = required_string(snapshot, "created_at", snapshot_path);
  metadata.updated_at = required_string(snapshot, "updated_at", snapshot_path);
  ConversationSession session(directory, std::move(metadata));

  const auto journal_path = directory / "events.jsonl";
  discard_incomplete_tail(journal_path);
  std::ifstream journal(journal_path, std::ios::binary);
  if (!journal) throw io_error("cannot read conversation journal", journal_path);
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(journal, line)) {
    ++line_number;
    if (line.empty()) throw std::runtime_error("corrupt conversation journal at line " + std::to_string(line_number));
    const auto event = parse_object(line, journal_path);
    const auto type = required_string(event, "type", journal_path);
    if (type == "message") {
      session.messages_.push_back({required_string(event, "role", journal_path),
                                   required_string(event, "content", journal_path)});
    } else if (type == "transition") {
      session.transitions_.push_back({required_string(event, "from_provider", journal_path),
                                      required_string(event, "to_provider", journal_path),
                                      required_string(event, "reason", journal_path)});
    } else if (type == "error") {
      session.errors_.push_back({required_string(event, "kind", journal_path),
                                 required_string(event, "message", journal_path)});
    } else {
      throw std::runtime_error("unknown conversation event at line " + std::to_string(line_number));
    }
  }
  return session;
}

void ConversationSession::write_snapshot() {
  Json snapshot(Json::object{{"schema_version", 1},
                             {"session_id", metadata_.session_id},
                             {"workspace", metadata_.workspace.string()},
                             {"provider_policy", metadata_.provider_policy},
                             {"model_policy", metadata_.model_policy},
                             {"created_at", metadata_.created_at},
                             {"updated_at", metadata_.updated_at}});
  atomic_write(directory_ / "session.json", snapshot.dump() + "\n");
}

void ConversationSession::append_event(const std::string& json) {
  const auto path = directory_ / "events.jsonl";
  discard_incomplete_tail(path);
  const int fd = ::open(path.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
  if (fd < 0) throw io_error("cannot open conversation journal", path);
  try {
    write_all(fd, json + "\n", path);
    if (::fsync(fd) != 0) throw io_error("cannot sync conversation journal", path);
    if (::close(fd) != 0) throw io_error("cannot close conversation journal", path);
  } catch (...) {
    ::close(fd);
    throw;
  }
}

void ConversationSession::append_message(const ChatMessage& message) {
  if (message.role != "user" && message.role != "assistant") {
    throw std::runtime_error("conversation message role must be user or assistant");
  }
  append_event(Json(Json::object{{"type", "message"}, {"role", message.role},
                                 {"content", message.content}}).dump());
  messages_.push_back(message);
}

void ConversationSession::append_transition(const ConversationTransition& transition) {
  append_event(Json(Json::object{{"type", "transition"},
                                 {"from_provider", transition.from_provider},
                                 {"to_provider", transition.to_provider},
                                 {"reason", transition.reason}}).dump());
  transitions_.push_back(transition);
}

void ConversationSession::append_error(const ConversationError& error) {
  if (error.kind.empty()) throw std::runtime_error("normalized conversation error kind is required");
  append_event(Json(Json::object{{"type", "error"}, {"kind", error.kind},
                                 {"message", error.message}}).dump());
  errors_.push_back(error);
}

void ConversationSession::update_selection(std::string provider_policy, std::string model_policy) {
  metadata_.provider_policy = std::move(provider_policy);
  metadata_.model_policy = std::move(model_policy);
  metadata_.updated_at = now_utc();
  write_snapshot();
}

}
