"""Research arithmetic only. No emulator, allocator, backend or compatibility probe."""

import argparse
import json
from pathlib import Path


CAPABILITIES = (
    "graphics_bridge", "shader_semantics", "task_mesh_or_validated_fallback",
    "cpu_jit_abi", "service_contracts", "mapping_lifetime_sync", "provenance",
)
COMPONENTS = ("guest_backing", "graphics", "jit", "runtime")
MAX_INPUT_BYTES = 65536
MAX_QUANTITY = (1 << 64) - 1  # Input-format bound, not an emulator ABI.


def exact_object(value, fields, path):
    if type(value) is not dict or set(value) != set(fields):
        raise ValueError(f"{path}: expected exactly {', '.join(fields)}")


def quantity(value, path):
    if value is not None and (
        type(value) is not int or not 0 <= value <= MAX_QUANTITY
    ):
        raise ValueError(f"{path}: expected null or unsigned 64-bit byte count")


def evaluate(case):
    """Evaluate a declared model, never discover or certify platform properties."""
    exact_object(case, (
        "schema_version", "evidence_scope", "virtual_bytes", "memory", "capabilities",
    ), "case")
    if type(case["schema_version"]) is not int or case["schema_version"] != 1:
        raise ValueError("schema_version: expected integer 1")
    if case["evidence_scope"] not in ("synthetic", "pinned_pc_documentation"):
        raise ValueError("evidence_scope: unsupported evidence class")
    exact_object(case["virtual_bytes"], ("guest", "gpu"), "virtual_bytes")
    for name, value in case["virtual_bytes"].items():
        quantity(value, f"virtual_bytes.{name}")
    memory = case["memory"]
    exact_object(memory, (
        "backing_commit_bytes", "commit_limit_bytes", "resident_components_bytes",
        "app_budget_bytes", "safety_margin_bytes",
    ), "memory")
    components = memory["resident_components_bytes"]
    exact_object(components, COMPONENTS, "memory.resident_components_bytes")
    quantities = {f"memory.{k}": v for k, v in memory.items()
                  if k != "resident_components_bytes"}
    quantities.update({f"memory.resident_components_bytes.{k}": v
                       for k, v in components.items()})
    for path, value in quantities.items():
        quantity(value, path)
    exact_object(case["capabilities"], CAPABILITIES, "capabilities")
    for name, status in case["capabilities"].items():
        if type(status) is not str or status not in (
            "unknown", "unavailable", "pc_documented", "synthetic_only",
        ):
            raise ValueError(f"capabilities.{name}: unsupported status")

    blockers = [f"missing:{path}" for path, v in quantities.items() if v is None]
    rejections = []
    if case["evidence_scope"] != "synthetic":
        blockers.append("evidence:not_a_synthetic_contract_check")
    if memory["safety_margin_bytes"] == 0:
        blockers.append("safety_margin_not_positive")
    for name in CAPABILITIES:
        status = case["capabilities"][name]
        if status == "unavailable":
            rejections.append(f"capability_unavailable:{name}")
        elif status != "synthetic_only":
            blockers.append(f"capability_unproven:{name}:{status}")

    commit = memory["backing_commit_bytes"]
    limit = memory["commit_limit_bytes"]
    if commit is not None and limit is not None and commit > limit:
        rejections.append("commit_limit_exceeded")
    peak = None if any(v is None for v in components.values()) else sum(components.values())
    margin = memory["safety_margin_bytes"]
    charged = None if peak is None or margin is None else peak + margin
    budget = memory["app_budget_bytes"]
    if charged is not None and budget is not None and charged > budget:
        rejections.append("resident_budget_exceeded")
    status = "REJECTED" if rejections else "BLOCKED" if blockers else "SYNTHETIC_ONLY"
    return {
        "status": status,
        "scope": "research_arithmetic_only",
        "switch2_supported": False,
        "evidence_scope": case["evidence_scope"],
        "resident_peak_bytes": peak,
        "resident_with_margin_bytes": charged,
        "headroom_bytes": None if charged is None or budget is None else budget - charged,
        "blockers": blockers,
        "rejections": rejections,
    }


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"non-finite JSON value: {value}")


def load_case(path):
    with Path(path).open("rb") as stream:
        data = stream.read(MAX_INPUT_BYTES + 1)
    if len(data) > MAX_INPUT_BYTES:
        raise ValueError("input exceeds 65536 bytes")
    return json.loads(data.decode("utf-8"), object_pairs_hook=unique_object,
                      parse_constant=reject_constant)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", help="One explicit research JSON fixture; no directory scan")
    args = parser.parse_args(argv)
    try:
        result = evaluate(load_case(args.fixture))
    except (OSError, ValueError, RecursionError) as error:
        print(json.dumps({"status": "INVALID_INPUT", "scope": "research_arithmetic_only",
                          "switch2_supported": False, "error": str(error)}))
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0 if result["status"] == "SYNTHETIC_ONLY" else 1


if __name__ == "__main__":
    raise SystemExit(main())
