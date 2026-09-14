#pragma once

#include <filesystem>

namespace codane {

class WorkspacePolicy {
 public:
  static WorkspacePolicy select(const std::filesystem::path& root);

  const std::filesystem::path& root() const noexcept { return root_; }
  std::filesystem::path resolve(const std::filesystem::path& path) const;
  bool contains(const std::filesystem::path& path) const;
  std::filesystem::path require_inside(const std::filesystem::path& path) const;

 private:
  explicit WorkspacePolicy(std::filesystem::path root) : root_(std::move(root)) {}
  std::filesystem::path root_;
};

}
