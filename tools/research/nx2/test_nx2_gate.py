"""NOT_RUN by A12. Intended for A50's isolated Docker host runner."""

import contextlib
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest

import nx2_gate as gate


ROOT = Path(__file__).resolve().parent


class GateTests(unittest.TestCase):
    def setUp(self):
        self.case = gate.load_case(ROOT / "fixtures/synthetic-exact-boundary.json")

    def check_status(self, case, status):
        result = gate.evaluate(case)
        self.assertEqual(result["status"], status)
        self.assertIs(result["switch2_supported"], False)
        self.assertEqual(result["scope"], "research_arithmetic_only")
        return result

    def test_exact_boundary(self):
        result = self.check_status(self.case, "SYNTHETIC_ONLY")
        self.assertEqual(result["resident_peak_bytes"], 75)
        self.assertEqual(result["resident_with_margin_bytes"], 100)
        self.assertEqual(result["headroom_bytes"], 0)

    def test_virtual_space_is_not_residency(self):
        expected = gate.evaluate(self.case)
        self.case["virtual_bytes"] = {"guest": None, "gpu": gate.MAX_QUANTITY}
        self.assertEqual(gate.evaluate(self.case), expected)

    def test_one_byte_over_resident_budget(self):
        self.case["memory"]["resident_components_bytes"]["graphics"] += 1
        result = self.check_status(self.case, "REJECTED")
        self.assertEqual(result["headroom_bytes"], -1)
        self.assertIn("resident_budget_exceeded", result["rejections"])

    def test_commit_limit_is_separate(self):
        self.case["memory"]["commit_limit_bytes"] = 70
        self.check_status(self.case, "SYNTHETIC_ONLY")
        self.case["memory"]["commit_limit_bytes"] = 69
        result = self.check_status(self.case, "REJECTED")
        self.assertEqual(result["resident_peak_bytes"], 75)
        self.assertEqual(result["rejections"], ["commit_limit_exceeded"])

    def test_unknown_is_never_zero(self):
        for field in ("backing_commit_bytes", "commit_limit_bytes", "app_budget_bytes",
                      "safety_margin_bytes"):
            with self.subTest(field=field):
                case = copy.deepcopy(self.case)
                case["memory"][field] = None
                self.check_status(case, "BLOCKED")
        for component in gate.COMPONENTS:
            with self.subTest(component=component):
                case = copy.deepcopy(self.case)
                case["memory"]["resident_components_bytes"][component] = None
                result = self.check_status(case, "BLOCKED")
                self.assertIsNone(result["resident_peak_bytes"])

    def test_margin_required(self):
        self.case["memory"]["safety_margin_bytes"] = 0
        self.assertIn("safety_margin_not_positive",
                      self.check_status(self.case, "BLOCKED")["blockers"])

    def test_capabilities_fail_closed_individually(self):
        for capability in gate.CAPABILITIES:
            for status, expected in (("unknown", "BLOCKED"), ("pc_documented", "BLOCKED"),
                                     ("unavailable", "REJECTED")):
                with self.subTest(capability=capability, status=status):
                    case = copy.deepcopy(self.case)
                    case["capabilities"][capability] = status
                    self.check_status(case, expected)

    def test_pc_evidence_cannot_be_upgraded_by_numbers(self):
        self.case["evidence_scope"] = "pinned_pc_documentation"
        self.check_status(self.case, "BLOCKED")

    def test_documented_configuration_has_no_ps5_budget(self):
        case = gate.load_case(ROOT / "fixtures/pinned-pc-commit.json")
        result = self.check_status(case, "BLOCKED")
        self.assertIsNone(result["resident_peak_bytes"])
        self.assertIsNone(case["memory"]["app_budget_bytes"])
        self.assertEqual(case["memory"]["backing_commit_bytes"], 20480 * 1024 * 1024)
        self.assertEqual(result["rejections"], [])  # Not a proof of impossibility.

    def test_pinned_document_bytes_and_quote(self):
        data = Path(os.environ.get("NX2_DOCUMENTARY_INPUT", str(ROOT / "inputs/G-NOTES.md"))).read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(),
                         "5cda9a49eb2a9798472597e0e1ecd0dc1c29f5289f19f13ec50aefa5b1bea67f")
        self.assertEqual(hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest(),
                         "8da5873a5cea4f96880aefe2150c80e6d4b0bfd3")
        self.assertIn("Failed to allocate 20480 MiB of backing memory",
                      data.decode("utf-8").splitlines()[185])

    def test_quantities_reject_invalid_types_and_ranges(self):
        for value in (True, False, -1, 1.0, "100", float("inf"), gate.MAX_QUANTITY + 1):
            with self.subTest(value=repr(value)):
                self.case["memory"]["app_budget_bytes"] = value
                with self.assertRaises(ValueError):
                    gate.evaluate(self.case)

    def test_sum_does_not_wrap(self):
        self.case["memory"]["resident_components_bytes"] = dict.fromkeys(
            gate.COMPONENTS, gate.MAX_QUANTITY)
        self.case["memory"]["app_budget_bytes"] = gate.MAX_QUANTITY
        result = self.check_status(self.case, "REJECTED")
        self.assertEqual(result["resident_peak_bytes"], 4 * gate.MAX_QUANTITY)

    def test_unrecognized_or_incomplete_contract_is_invalid(self):
        mutations = (
            lambda c: c.update(schema_version=True),
            lambda c: c.update(switch2_supported=True),
            lambda c: c.update(evidence_scope="ps5_verified"),
            lambda c: c["capabilities"].pop("graphics_bridge"),
            lambda c: c["capabilities"].update(graphics_bridge="launcher_21_objects"),
            lambda c: c["capabilities"].update(graphics_bridge="ps5_verified"),
            lambda c: c["memory"].update(resident_components_bytes={}),
        )
        for mutation in mutations:
            case = copy.deepcopy(self.case)
            mutation(case)
            with self.assertRaises(ValueError):
                gate.evaluate(case)

    def test_cli_exit_and_json_contract(self):
        for name, exit_code, status in (
            ("synthetic-exact-boundary.json", 0, "SYNTHETIC_ONLY"),
            ("pinned-pc-commit.json", 1, "BLOCKED"),
        ):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(gate.main([str(ROOT / "fixtures" / name)]), exit_code)
            result = json.loads(output.getvalue())
            self.assertEqual(result["status"], status)
            self.assertIs(result["switch2_supported"], False)

    def test_invalid_json_cli_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "invalid.json"
            for data in (b'{"schema_version":1,"schema_version":1}', b'{"x":NaN}',
                         b'{"x":Infinity}', b'[]', b'\xff', b'{' * 65537):
                path.write_bytes(data)
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(gate.main([str(path)]), 2)
                result = json.loads(output.getvalue())
                self.assertEqual(result["status"], "INVALID_INPUT")
                self.assertIs(result["switch2_supported"], False)


if __name__ == "__main__":
    unittest.main()
