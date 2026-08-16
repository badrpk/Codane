import unittest

from codane import (
    CodaneError,
    FileChange,
    PathPolicy,
    PatchPlan,
    ValidationGate,
    build_plan,
    digest_text,
    validate_plan,
)


class CodaneTests(unittest.TestCase):
    def test_digest_is_deterministic(self):
        self.assertEqual(digest_text("hello"), digest_text("hello"))

    def test_rejects_path_traversal(self):
        with self.assertRaises(CodaneError):
            FileChange("../secret.txt", "delete", "bad")

    def test_policy_rejects_secrets(self):
        policy = PathPolicy(allowed_prefixes=("src/", ".env"))
        self.assertFalse(policy.allows("src/private.key"))
        self.assertFalse(policy.allows(".env"))

    def test_source_change_requires_test_change(self):
        plan = build_plan(
            "change parser",
            [FileChange("src/parser.py", "update", "fix parser", digest_text("x"))],
            [ValidationGate("tests", "python -m unittest")],
        )
        self.assertIn("source changes require a tests/ change", validate_plan(plan))

    def test_source_change_requires_tests_gate(self):
        plan = build_plan(
            "change parser",
            [
                FileChange("src/parser.py", "update", "fix parser", digest_text("x")),
                FileChange("tests/test_parser.py", "update", "cover fix", digest_text("y")),
            ],
        )
        self.assertIn("source changes require a required 'tests' validation gate", validate_plan(plan))

    def test_valid_plan_passes(self):
        plan = build_plan(
            "change parser",
            [
                FileChange("src/parser.py", "update", "fix parser", digest_text("x")),
                FileChange("tests/test_parser.py", "update", "cover fix", digest_text("y")),
            ],
            [ValidationGate("tests", "python -m unittest")],
        )
        self.assertEqual(validate_plan(plan), [])

    def test_evidence_hash_independent_of_insertion_order(self):
        a = FileChange("src/a.py", "create", "a", digest_text("a"))
        b = FileChange("tests/test_a.py", "create", "b", digest_text("b"))
        gate = ValidationGate("tests", "python -m unittest")
        p1 = build_plan("goal", [a, b], [gate], ["note"])
        p2 = build_plan("goal", [b, a], [gate], ["note"])
        self.assertEqual(p1.evidence_hash(), p2.evidence_hash())

    def test_duplicate_path_rejected(self):
        change = FileChange("docs/x.md", "create", "doc", digest_text("x"))
        plan = PatchPlan("docs")
        plan.add_change(change)
        with self.assertRaises(CodaneError):
            plan.add_change(change)

    def test_duplicate_gate_rejected(self):
        plan = PatchPlan("goal")
        gate = ValidationGate("tests", "python -m unittest")
        plan.add_gate(gate)
        with self.assertRaises(CodaneError):
            plan.add_gate(gate)


if __name__ == "__main__":
    unittest.main()
