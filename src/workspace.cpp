#include "codane/workspace.hpp"

#include <stdexcept>
#include <system_error>

namespace codane {
namespace fs = std::filesystem;

WorkspacePolicy WorkspacePolicy::select(const fs::path& root) {
  std::error_code error;
  const auto canonical = fs::canonical(root, error);
  if (error || !fs::is_directory(canonical, error) || error) {
    throw std::runtime_error("workspace must be an existing directory: " + root.string());
  }
  return WorkspacePolicy(canonical);
}

fs::path WorkspacePolicy::resolve(const fs::path& path) const {
  std::error_code error;
  const auto candidate = path.is_absolute() ? path : root_ / path;
  const auto resolved = fs::weakly_canonical(candidate, error);
  if (error) throw std::runtime_error("cannot resolve workspace path: " + candidate.string());
  return resolved;
}

bool WorkspacePolicy::contains(const fs::path& path) const {
  try {
    const auto resolved = resolve(path);
    auto root_part = root_.begin();
    auto candidate_part = resolved.begin();
    for (; root_part != root_.end(); ++root_part, ++candidate_part) {
      if (candidate_part == resolved.end() || *candidate_part != *root_part) return false;
    }
    return true;
  } catch (const std::runtime_error&) {
    return false;
  }
}

fs::path WorkspacePolicy::require_inside(const fs::path& path) const {
  const auto resolved = resolve(path);
  auto root_part = root_.begin();
  auto candidate_part = resolved.begin();
  for (; root_part != root_.end(); ++root_part, ++candidate_part) {
    if (candidate_part == resolved.end() || *candidate_part != *root_part) {
      throw std::runtime_error("path escapes selected workspace: " + path.string());
    }
  }
  return resolved;
}

}
