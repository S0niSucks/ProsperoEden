"""A36 static-review controls for A12; NOT_RUN. A50 offline Docker only.

Imports only the hash-pinned arithmetic gate, never an emulator/backend.
Set A12_QA_INPUT_ROOT=/a12 and TMPDIR to A50's exclusive writable output area.
"""

import contextlib
import copy
import hashlib
import io
import itertools
import json
import os
from pathlib import Path
import tempfile
import types
import unittest


PINS = {
    "nx2_gate.py": "4400b287c78a4a3d6282f34898d58eb9ffd73ff95871bcc540664ff311028389",
    "fixtures/synthetic-exact-boundary.json": "c64df1dbf46a0efde7661eb2792fa8e3fa16989d4dfb9fcd907162dfd707d24b",
    "fixtures/pinned-pc-commit.json": "9a6d6b55cc28055c81a3d214b120faba6c3534da242a2738ec975f9faea057d1",
}


class AdditionalA12GateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(os.environ.get(
            "A12_QA_INPUT_ROOT", str(Path(__file__).resolve().parent)))
        cls.pinned = {}
        for name, expected in PINS.items():
            raw = (cls.root / name).read_bytes()
            actual = hashlib.sha256(raw).hexdigest()
            if actual != expected:
                raise AssertionError("reviewed A12 bytes changed: " + name)
            cls.pinned[name] = raw
        # Execute exactly the checked bytes, not an unchecked cached .pyc or reread.
        cls.gate = types.ModuleType("a36_reviewed_a12_gate")
        exec(compile(cls.pinned["nx2_gate.py"], str(cls.root / "nx2_gate.py"), "exec"),
             cls.gate.__dict__)
        cls.toy = json.loads(cls.pinned["fixtures/synthetic-exact-boundary.json"])

    def setUp(self):
        self.case = copy.deepcopy(self.toy)

    def evaluate(self, case, status):
        result = self.gate.evaluate(case)
        self.assertEqual(result["status"], status)
        self.assertIs(result["switch2_supported"], False)
        self.assertEqual(result["scope"], "research_arithmetic_only")
        return result

    def quantity_paths(self):
        return ([('virtual_bytes', name) for name in ("guest", "gpu")]
                + [('memory', name) for name in (
                    "backing_commit_bytes", "commit_limit_bytes", "app_budget_bytes", "safety_margin_bytes")]
                + [('memory', 'resident_components_bytes', name)
                   for name in self.gate.COMPONENTS])

    @staticmethod
    def assign(case, path, value):
        parent = case
        for name in path[:-1]:
            parent = parent[name]
        parent[path[-1]] = value

    def test_invalid_quantity_types_for_every_byte_field(self):
        invalid = (True, False, -1, 1.0, "20480 MiB", "21474836480",
                   float("inf"), float("nan"), (1 << 64))
        for path in self.quantity_paths():
            for value in invalid:
                with self.subTest(path=path, value=repr(value)):
                    case = copy.deepcopy(self.toy)
                    self.assign(case, path, value)
                    with self.assertRaises(ValueError):
                        self.gate.evaluate(case)

    def test_unknown_physical_quantities_keep_explicit_blockers(self):
        for path in self.quantity_paths():
            if path[0] == "virtual_bytes":
                continue
            with self.subTest(path=path):
                case = copy.deepcopy(self.toy)
                self.assign(case, path, None)
                result = self.evaluate(case, "BLOCKED")
                self.assertIn("missing:" + ".".join(path), result["blockers"])

    def test_user_process_budget_unknown_is_not_hardware_total_commit_or_va(self):
        self.case["memory"]["app_budget_bytes"] = None
        self.case["memory"]["backing_commit_bytes"] = (1 << 64) - 1
        self.case["memory"]["commit_limit_bytes"] = (1 << 64) - 1
        self.case["virtual_bytes"] = {"guest": 16 * (1 << 30), "gpu": 64 * (1 << 30)}
        result = self.evaluate(self.case, "BLOCKED")
        self.assertIn("missing:memory.app_budget_bytes", result["blockers"])
        self.assertEqual(result["resident_peak_bytes"], 75)
        self.assertEqual(result["resident_with_margin_bytes"], 100)
        self.assertIsNone(result["headroom_bytes"])

    def test_positive_margin_sum_exceeds_u64_without_wrapping(self):
        maximum = (1 << 64) - 1
        self.case["memory"]["resident_components_bytes"] = dict.fromkeys(self.gate.COMPONENTS, 0)
        self.case["memory"]["resident_components_bytes"]["guest_backing"] = maximum
        self.case["memory"]["safety_margin_bytes"] = 1
        self.case["memory"]["app_budget_bytes"] = maximum
        result = self.evaluate(self.case, "REJECTED")
        self.assertEqual(result["resident_with_margin_bytes"], 1 << 64)
        self.assertEqual(result["headroom_bytes"], -1)
        self.assertEqual(result["rejections"], ["resident_budget_exceeded"])

    def test_zero_and_maximum_commit_boundaries_are_only_arithmetic(self):
        for commit, limit, status in ((0, 0, "SYNTHETIC_ONLY"),
                                      (1, 0, "REJECTED"),
                                      ((1 << 64) - 1, (1 << 64) - 1, "SYNTHETIC_ONLY")):
            with self.subTest(commit=commit, limit=limit):
                case = copy.deepcopy(self.toy)
                case["memory"]["backing_commit_bytes"] = commit
                case["memory"]["commit_limit_bytes"] = limit
                result = self.evaluate(case, status)
                self.assertEqual(result["resident_peak_bytes"], 75)
                self.assertEqual(result["resident_with_margin_bytes"], 100)

    def test_budget_zero_does_not_turn_into_unknown_or_default(self):
        self.case["memory"]["app_budget_bytes"] = 0
        result = self.evaluate(self.case, "REJECTED")
        self.assertEqual(result["headroom_bytes"], -100)
        self.assertNotIn("missing:memory.app_budget_bytes", result["blockers"])

    def test_all_capability_combinations_never_authorize_support(self):
        states = ("unknown", "unavailable", "pc_documented", "synthetic_only")
        for statuses in itertools.product(states, repeat=len(self.gate.CAPABILITIES)):
            case = copy.deepcopy(self.toy)
            case["capabilities"] = dict(zip(self.gate.CAPABILITIES, statuses))
            expected = ("REJECTED" if "unavailable" in statuses else
                        "BLOCKED" if any(value != "synthetic_only" for value in statuses)
                        else "SYNTHETIC_ONLY")
            self.evaluate(case, expected)

    def test_rejection_does_not_erase_unknown_budget_diagnostic(self):
        self.case["memory"]["app_budget_bytes"] = None
        self.case["memory"]["backing_commit_bytes"] = 129
        result = self.evaluate(self.case, "REJECTED")
        self.assertIn("commit_limit_exceeded", result["rejections"])
        self.assertIn("missing:memory.app_budget_bytes", result["blockers"])
        self.assertIsNone(result["headroom_bytes"])

    def test_unknown_virtual_extents_are_not_resident_unknowns(self):
        expected = self.evaluate(self.case, "SYNTHETIC_ONLY")
        for guest, gpu in ((None, None), (0, 0), ((1 << 64) - 1, (1 << 64) - 1)):
            case = copy.deepcopy(self.toy)
            case["virtual_bytes"] = {"guest": guest, "gpu": gpu}
            self.assertEqual(self.evaluate(case, "SYNTHETIC_ONLY"), expected)

    def test_documentary_mib_charge_does_not_manufacture_ps5_data(self):
        case = json.loads(self.pinned["fixtures/pinned-pc-commit.json"])
        result = self.evaluate(case, "BLOCKED")
        charge = case["memory"]["backing_commit_bytes"]
        self.assertEqual(charge, 21474836480)
        self.assertEqual(charge, 20480 * (1 << 20))
        self.assertNotEqual(charge, 20480 * 1000000)
        self.assertEqual(case["virtual_bytes"], {"guest": None, "gpu": None})
        self.assertIsNone(case["memory"]["app_budget_bytes"])
        self.assertIsNone(result["resident_peak_bytes"])
        self.assertIsNone(result["headroom_bytes"])

    def test_no_support_or_verification_fields_can_be_injected(self):
        for path, value in ((("switch2_supported",), True),
                            (("user_process_budget_unknown",), False),
                            (("evidence_scope",), "ps5_verified"),
                            (("memory", "hardware_total_bytes"), 16 * (1 << 30)),
                            (("memory", "app_budget_verified"), True),
                            (("capabilities", "graphics_bridge"), "ps5_verified")):
            with self.subTest(path=path):
                case = copy.deepcopy(self.toy)
                self.assign(case, path, value)
                with self.assertRaises(ValueError):
                    self.gate.evaluate(case)

    def test_input_is_not_mutated_or_contaminated_across_evaluations(self):
        before = copy.deepcopy(self.case)
        result = self.evaluate(self.case, "SYNTHETIC_ONLY")
        self.assertEqual(self.case, before)
        result["switch2_supported"] = True
        result["blockers"].append("caller-owned mutation")
        next_result = self.evaluate(self.case, "SYNTHETIC_ONLY")
        self.assertEqual(next_result["blockers"], [])
        self.assertEqual(self.case, before)

    def invoke_cli(self, path):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            exit_code = self.gate.main([str(path)])
        result = json.loads(output.getvalue())
        self.assertIs(result["switch2_supported"], False)
        self.assertEqual(result["scope"], "research_arithmetic_only")
        return exit_code, result

    def test_cli_all_four_outcomes_never_authorize_support(self):
        cases = []
        consistent = copy.deepcopy(self.toy)
        cases.append((consistent, 0, "SYNTHETIC_ONLY"))
        blocked = copy.deepcopy(self.toy)
        blocked["memory"]["app_budget_bytes"] = None
        cases.append((blocked, 1, "BLOCKED"))
        rejected = copy.deepcopy(self.toy)
        rejected["memory"]["resident_components_bytes"]["runtime"] += 1
        cases.append((rejected, 1, "REJECTED"))
        invalid = copy.deepcopy(self.toy)
        invalid["switch2_supported"] = True
        cases.append((invalid, 2, "INVALID_INPUT"))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "case.json"
            for case, expected_exit, expected_status in cases:
                path.write_text(json.dumps(case), encoding="utf-8")
                exit_code, result = self.invoke_cli(path)
                self.assertEqual(exit_code, expected_exit)
                self.assertEqual(result["status"], expected_status)

    def test_cli_64k_boundary_missing_file_and_deep_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "case.json"
            raw = json.dumps(self.toy).encode("utf-8")
            path.write_bytes(raw + b" " * (65536 - len(raw)))
            exit_code, result = self.invoke_cli(path)
            self.assertEqual((exit_code, result["status"]), (0, "SYNTHETIC_ONLY"))
            path.write_bytes(raw + b" " * (65537 - len(raw)))
            exit_code, result = self.invoke_cli(path)
            self.assertEqual((exit_code, result["status"]), (2, "INVALID_INPUT"))
            path.write_bytes(b"[" * 10000 + b"0" + b"]" * 10000)
            exit_code, result = self.invoke_cli(path)
            self.assertEqual((exit_code, result["status"]), (2, "INVALID_INPUT"))
            exit_code, result = self.invoke_cli(Path(directory) / "missing.json")
            self.assertEqual((exit_code, result["status"]), (2, "INVALID_INPUT"))


if __name__ == "__main__":
    unittest.main()
