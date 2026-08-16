from __future__ import annotations

from dataclasses import dataclass, field, asdict
from hashlib import sha256
import json
from pathlib import PurePosixPath
from typing import Iterable


class CodaneError(ValueError):
    pass


@dataclass(frozen=True)
class FileChange:
    path: str
    action: str
    rationale: str
    content_sha256: str | None = None

    def __post_init__(self) -> None:
        action = self.action.lower().strip()
        if action not in {"create", "update", "delete"}:
            raise CodaneError(f"unsupported action: {self.action}")
        object.__setattr__(self, "action", action)
        _validate_repo_path(self.path)
        if not self.rationale.strip():
            raise CodaneError("rationale is required")
        if action in {"create", "update"} and not self.content_sha256:
            raise CodaneError("content_sha256 is required for create/update")


@dataclass(frozen=True)
class ValidationGate:
    name: str
    command: str
    required: bool = True

    def __post_init__(self) -> None:
        if not self.name.strip():
            raise CodaneError("gate name is required")
        if not self.command.strip():
            raise CodaneError("gate command is required")


@dataclass
class PatchPlan:
    goal: str
    changes: list[FileChange] = field(default_factory=list)
    gates: list[ValidationGate] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)

    def add_change(self, change: FileChange) -> None:
        if any(c.path == change.path for c in self.changes):
            raise CodaneError(f"duplicate path in plan: {change.path}")
        self.changes.append(change)

    def add_gate(self, gate: ValidationGate) -> None:
        if any(g.name == gate.name for g in self.gates):
            raise CodaneError(f"duplicate gate: {gate.name}")
        self.gates.append(gate)

    def canonical(self) -> dict:
        return {
            "goal": self.goal.strip(),
            "changes": [asdict(c) for c in sorted(self.changes, key=lambda x: x.path)],
            "gates": [asdict(g) for g in sorted(self.gates, key=lambda x: x.name)],
            "notes": sorted(n.strip() for n in self.notes if n.strip()),
        }

    def evidence_hash(self) -> str:
        payload = json.dumps(self.canonical(), sort_keys=True, separators=(",", ":")).encode()
        return sha256(payload).hexdigest()


@dataclass(frozen=True)
class PathPolicy:
    allowed_prefixes: tuple[str, ...] = ("src/", "tests/", "docs/", ".github/")
    denied_names: tuple[str, ...] = (".env", "id_rsa", "id_ed25519")
    denied_suffixes: tuple[str, ...] = (".pem", ".key", ".p12", ".pfx")

    def allows(self, path: str) -> bool:
        _validate_repo_path(path)
        p = PurePosixPath(path)
        if any(part in self.denied_names for part in p.parts):
            return False
        if any(path.endswith(s) for s in self.denied_suffixes):
            return False
        return any(path == prefix.rstrip("/") or path.startswith(prefix) for prefix in self.allowed_prefixes)


def _validate_repo_path(path: str) -> None:
    if not path or path.startswith("/"):
        raise CodaneError("path must be repository-relative")
    p = PurePosixPath(path)
    if ".." in p.parts:
        raise CodaneError("path traversal is not allowed")
    if "\\" in path:
        raise CodaneError("use POSIX repository paths")


def digest_text(content: str) -> str:
    return sha256(content.encode("utf-8")).hexdigest()


def validate_plan(
    plan: PatchPlan,
    *,
    policy: PathPolicy | None = None,
    require_tests_for_source: bool = True,
) -> list[str]:
    policy = policy or PathPolicy()
    errors: list[str] = []

    if not plan.goal.strip():
        errors.append("goal is required")
    if not plan.changes:
        errors.append("at least one file change is required")

    for change in plan.changes:
        if not policy.allows(change.path):
            errors.append(f"path rejected by policy: {change.path}")

    source_changed = any(c.path.startswith("src/") for c in plan.changes)
    tests_changed = any(c.path.startswith("tests/") for c in plan.changes)
    if require_tests_for_source and source_changed and not tests_changed:
        errors.append("source changes require a tests/ change")

    required_gate_names = {g.name for g in plan.gates if g.required}
    if source_changed and "tests" not in required_gate_names:
        errors.append("source changes require a required 'tests' validation gate")

    return sorted(set(errors))


def build_plan(
    goal: str,
    changes: Iterable[FileChange],
    gates: Iterable[ValidationGate] = (),
    notes: Iterable[str] = (),
) -> PatchPlan:
    plan = PatchPlan(goal=goal, notes=list(notes))
    for change in changes:
        plan.add_change(change)
    for gate in gates:
        plan.add_gate(gate)
    return plan


__all__ = [
    "CodaneError",
    "FileChange",
    "ValidationGate",
    "PatchPlan",
    "PathPolicy",
    "digest_text",
    "validate_plan",
    "build_plan",
]
