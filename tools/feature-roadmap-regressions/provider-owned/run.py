#!/usr/bin/env python3
"""Docker/Linux host witnesses over LIVE pinned application source, not snapshots.

Candidate-only is default. Optional red/green requires BOTH explicit external
--baseline-provider and --baseline-sha256. Never patches/reverts live production.
Only actual witness execution can produce HOST_WITNESSES_PASS_*.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import re
import shlex
import signal
import subprocess
import sys
import time

sys.dont_write_bytecode = True  # Importing the extractor must not write into checkout.
import extractor as source

ROOT = Path(__file__).resolve().parent


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def limits():
    import resource
    for category, maximum in ((resource.RLIMIT_CPU, 120), (resource.RLIMIT_AS, 2 << 30),
                              (resource.RLIMIT_FSIZE, 8 << 20), (resource.RLIMIT_CORE, 0),
                              (resource.RLIMIT_NOFILE, 256)):
        _, hard = resource.getrlimit(category)
        value = maximum if hard == resource.RLIM_INFINITY else min(maximum, hard)
        resource.setrlimit(category, (value, value))


def dependency_paths(path):
    # GCC/Clang make dependencies; support escaped spaces/continuations/dollars.
    text = path.read_text(encoding="utf-8").replace("\\\n", "").replace("$$", "$")
    _, separator, prerequisites = text.partition(":")
    if not separator:
        raise ValueError("invalid dependency file: " + str(path))
    # Keep lexical paths until the exact-set audit, so symlinks/aliases cannot
    # resolve out of first-party roots and silently disappear from the audit.
    return {(Path(item) if Path(item).is_absolute() else path.parent / item).absolute()
            for item in shlex.split(prerequisites, comments=False)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true", required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--source-manifest", type=Path, required=True,
                        help="explicit external four-input SHA-256 manifest")
    parser.add_argument("--out", type=Path, required=True, help="new exclusive external output")
    parser.add_argument("--stb-header", type=Path, required=True, help="existing HOST stb_image.h; no fetch")
    parser.add_argument("--baseline-provider", type=Path)
    parser.add_argument("--baseline-sha256")
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--ack-external-enclosure", action="store_true", required=True,
                        help="caller supplies strict whole-tree containment/drain + final log receipt")
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() not in ("x86_64", "amd64"):
        parser.error("execution is Docker Linux x86_64 only")
    if not Path("/.dockerenv").is_file():
        parser.error("Docker marker missing; launch through the approved Docker harness")
    if bool(args.baseline_provider) != bool(args.baseline_sha256):
        parser.error("baseline mode requires explicit --baseline-provider AND --baseline-sha256")
    if args.baseline_sha256 and not re.fullmatch(r"[0-9a-f]{64}", args.baseline_sha256):
        parser.error("baseline SHA-256 must be explicit lowercase hexadecimal")
    repo = args.repo.resolve(strict=True)
    # Reject ambiguous makefile dependency-path syntax, not ordinary spaces.
    for path in (repo, ROOT, args.out, args.stb_header):
        if any(char in str(path) for char in (":", "\n", "\r")):
            parser.error("colon/newline in dependency path requires explicit harness review")
    out = source.new_external_directory(args.out, repo)
    result = {"schema": 1, "status": "PREPARING_NOT_TESTED", "started_at": now(),
        "mode": "differential" if args.baseline_provider else "candidate-only",
        "commands": [], "witnesses": [], "gates": [], "extracted_functions": [],
        "scope": "real extracted functions + live Image/Descriptor; NOT full provider/native ABI/SDK/console",
        "environment_guard": "Linux x86_64 + /.dockerenv is NOT isolation evidence",
        "external_enclosure": "MANDATORY: caller containment/adoption/drain including normal-leader survivors",
        "log_hash_status": "PROVISIONAL_UNTIL_EXTERNAL_WHOLE_TREE_DRAIN",
        "acceptance": "REQUIRES_EXTERNAL_ENCLOSURE_AND_FINAL_LOG_HASH_RECEIPT"}
    stable = lambda: None

    def command(label, argv, phase, timeout=60):
        stable()
        log = out / (label + ".log")
        record = {"label": label, "phase": phase, "argv": [str(x) for x in argv],
            "cwd": str(out), "started_at": now(), "timeout_seconds": timeout,
            "exit": None, "timed_out": False, "leader_reaped": False}
        result["commands"].append(record)
        process = None
        begin = time.monotonic()
        environment = os.environ.copy()
        for name in ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "LD_PRELOAD"):
            environment.pop(name, None)
        environment["LC_ALL"] = "C"
        try:
            with log.open("xb") as stream:
                try:
                    process = subprocess.Popen([str(x) for x in argv], cwd=out,
                        stdout=stream, stderr=subprocess.STDOUT, env=environment,
                        start_new_session=True, preexec_fn=limits)
                    record["pid_pgid"] = process.pid
                    try:
                        process.wait(timeout=timeout)
                    except subprocess.TimeoutExpired:
                        record["timed_out"] = True
                        raise
                finally:
                    if process is not None:
                        # Do NOT poll/reap before killpg on exceptional exit:
                        # an unreaped leader reserves its PID/group identity.
                        # Never signal a group after a completed wait/reap.
                        if process.returncode is None:
                            kill_error = None
                            try:
                                os.killpg(process.pid, signal.SIGKILL)
                                record["killpg_sent"] = True
                            except ProcessLookupError:
                                record["killpg_group_already_gone"] = True
                            except OSError as error:
                                kill_error = error
                                record["killpg_error"] = str(error)
                            try:
                                process.wait(timeout=10)
                            except subprocess.TimeoutExpired:
                                record["reap_failed"] = True
                                raise RuntimeError("leader not reaped after SIGKILL: " + label)
                            if kill_error is not None:
                                record["exit"] = process.returncode
                                record["leader_reaped"] = process.returncode is not None
                                raise RuntimeError("killpg failed: " + label) from kill_error
                        record["exit"] = process.returncode
                        record["leader_reaped"] = process.returncode is not None
        except BaseException as error:
            record["error"] = type(error).__name__ + ": " + str(error)
            raise
        finally:
            record["elapsed_seconds"] = round(time.monotonic() - begin, 6)
            record["finished_at"] = now()
            record["log_sha256"] = source.sha(log) if log.exists() else None
        stable()
        return record["exit"], log.read_text(encoding="utf-8", errors="replace")

    def successful_command(label, argv, phase):
        code, text = command(label, argv, phase)
        if code != 0:
            raise RuntimeError("nonzero " + phase + " command: " + label)
        # This is a prerequisite command, NOT a witness/test pass.
        return text

    def nm_check(label, target):
        text = successful_command(label, [args.nm, "-u", target], "import-inspection")
        banned = r"\b(?:openat(?:64)?|fdopendir|readdir(?:64)?|fstatat(?:64)?)(?:@[^\s]*)?\s*$"
        if re.search(banned, text, re.M):
            raise RuntimeError("gate-off descriptor-walk import present: " + label)

    def dep_check(path, expected, external_required=()):
        observed = dependency_paths(path)
        actual = source.audit_dependencies(observed, expected, (repo, ROOT, out), external_required)
        if any("/vendor/sources/eden/" in str(item).lower() + "/" for item in observed):
            raise RuntimeError("protected core dependency is outside this harness")
        return {"path": str(path.relative_to(out)), "sha256": source.sha(path),
                "exact_first_party_dependencies": sorted(str(item) for item in actual),
                "explicit_host_dependencies": [str(item) for item in external_required]}

    try:
        paths = source.live_sources(repo)
        manifest = source.external(args.source_manifest, repo)
        pins = source.load_pins(manifest)
        manifest_sha = source.sha(manifest)
        source.verify_sources(paths, pins)
        source.check_dependency_surface(paths)
        result["candidate_sources"] = pins
        result["source_manifest_sha256"] = manifest_sha
        fixtures = source.read_json(ROOT / "fixtures.sha256.json")
        expected_fixture_names = {"fd_fixture.cpp", "jpeg_fixture.cpp", "stb_impl.cpp", "native_gate.cpp"}
        if fixtures.get("schema") != 1 or set(fixtures["fixture_sha256"]) != expected_fixture_names:
            raise ValueError("invalid immutable fixture manifest")
        for name, expected in fixtures["fixture_sha256"].items():
            if source.sha(ROOT / name) != expected:
                raise ValueError("previously tested fixture bytes changed: " + name)
        package_hashes = {name: source.sha(ROOT / name) for name in
            (*sorted(expected_fixture_names), "run.py", "extractor.py", "fixtures.sha256.json", "README.md")}
        result["harness_sha256"] = package_hashes
        stb = source.external(args.stb_header, repo).resolve(strict=True)
        if stb.name != "stb_image.h" or not stb.is_file():
            raise ValueError("existing host stb_image.h is required; no fetch fallback")
        if "/vendor/" in str(stb).lower() + "/":
            raise ValueError("use provisioned HOST stb, not a vendored/core dependency")
        stb_sha = source.sha(stb)
        result["host_stb"] = {"path": str(stb), "sha256": stb_sha}
        baseline = None
        if args.baseline_provider:
            baseline = source.external(args.baseline_provider, repo).resolve(strict=True)
            if not baseline.is_file() or source.sha(baseline) != args.baseline_sha256:
                raise ValueError("explicit external baseline SHA-256 mismatch")
            result["baseline_provider"] = {"path": str(baseline), "sha256": args.baseline_sha256}

        fragment_dirs = []

        def stable():
            source.verify_sources(paths, pins)
            if source.sha(manifest) != manifest_sha or source.sha(stb) != stb_sha:
                raise ValueError("pin manifest or host codec changed during run")
            for name, expected in package_hashes.items():
                if source.sha(ROOT / name) != expected:
                    raise ValueError("harness changed during run: " + name)
            if baseline is not None and source.sha(baseline) != args.baseline_sha256:
                raise ValueError("baseline changed during run")
            for item in result["extracted_functions"]:
                if source.sha(out / item["path"]) != item["sha256"]:
                    raise ValueError("extracted body changed during run")
            if fragment_dirs:
                source.preflight_first_party(paths, repo, ROOT, fragment_dirs)

        stable()
        variants = ["candidate"]
        if baseline is not None:
            variants.insert(0, "baseline")
            result["extracted_functions"] += source.write_extracts(out, "baseline", baseline.read_bytes())
        result["extracted_functions"] += source.write_extracts(out, "candidate", paths[source.PROVIDER].read_bytes())
        fragment_dirs.extend(out / "extracted" / variant for variant in variants)
        result["precompile_include_audit"] = source.preflight_first_party(paths, repo, ROOT, fragment_dirs)
        source.write_test_data(out, fixtures["synthetic_input_sha256"])
        # No full provider/header/dependency snapshots are written to checkout or output.
        result["provider_full_file_copied"] = False
        common = [args.compiler, "-std=c++20", "-O0", "-g", "-fno-inline", "-DPS5_NATIVE=1",
            "-iquote", str(repo / "headless"), "-iquote", str(repo / "headless/prosperoeden"),
            "-idirafter", str(stb.parent)]
        image_obj, stb_obj = out / "image.o", out / "stb.o"
        image_dep = out / "image.d"
        successful_command("compile-live-image", common + ["-MMD", "-MF", image_dep, "-c",
            paths[source.IMAGE_SOURCE], "-o", image_obj], "compile-prerequisite")
        result["image_dependencies"] = dep_check(image_dep,
            [paths[source.IMAGE_SOURCE], paths[source.IMAGE_HEADER]])
        stb_dep = out / "stb.d"
        # Include system-classified codec headers too, so the exact host stb
        # path is witnessed regardless of compiler include-directory flavour.
        successful_command("compile-host-stb", common + ["-MD", "-MF", stb_dep, "-c",
            ROOT / "stb_impl.cpp", "-o", stb_obj], "compile-prerequisite")
        result["stb_dependencies"] = dep_check(stb_dep, [ROOT / "stb_impl.cpp"], [stb])
        result["status"] = "RUNNING_HOST_WITNESSES"
        for gate, flags in (("absent", []), ("zero", ["-DEDEN_NATIVE_USER_CONTENT_IO_QUALIFIED=0"])):
            gate_obj, gate_binary, gate_dep = (out / (gate + suffix) for suffix in ("-gate.o", "-gate", "-gate.d"))
            successful_command("compile-live-gate-" + gate, common + flags + ["-MMD", "-MF", gate_dep,
                "-c", ROOT / "native_gate.cpp", "-o", gate_obj], "compile-gate")
            dependencies = dep_check(gate_dep, [ROOT / "native_gate.cpp", paths[source.DESCRIPTOR]])
            nm_check("nm-live-gate-object-" + gate, gate_obj)
            successful_command("link-live-gate-" + gate, [args.compiler, gate_obj, "-o", gate_binary], "link-gate")
            nm_check("nm-live-gate-binary-" + gate, gate_binary)
            rc, _ = command("run-live-gate-" + gate, [gate_binary], "gate-runtime", timeout=10)
            if rc != 0:
                raise RuntimeError("live Descriptor gate runtime failed: " + gate)
            result["gates"].append({"gate": gate, "exit": rc, "live_dependencies": dependencies,
                                    "imports_absent": True, "runtime_executed": True})
            for variant in variants:
                includes = ["-iquote", str(out / "extracted" / variant)]
                for kind, hits in (("fd", 16), ("jpeg", 8)):
                    name = gate + "-" + variant + "-" + kind
                    binary, dependency = out / name, out / (name + ".d")
                    if kind == "fd":
                        files = [ROOT / "fd_fixture.cpp", "-Wl,--wrap=open,--wrap=close"]
                        argv = [binary, out / "fd-root"]
                        required = [ROOT / "fd_fixture.cpp", paths[source.DESCRIPTOR], out / "extracted" / variant / "list_entries.inc"]
                    else:
                        files = [ROOT / "jpeg_fixture.cpp", image_obj, stb_obj,
                                 "-Wl,--wrap=stbi_load,--wrap=stbi_image_free"]
                        argv = [binary, out / "data/tiny.jpg", out / "data/wide.jpg", out / "data/invalid.jpg"]
                        required = [ROOT / "jpeg_fixture.cpp", paths[source.DESCRIPTOR], paths[source.IMAGE_HEADER],
                                    out / "extracted" / variant / "load_image.inc"]
                    successful_command("compile-" + name, common + flags + includes +
                        ["-MMD", "-MF", dependency] + files + ["-o", binary], "compile-witness")
                    deps = dep_check(dependency, required)
                    nm_check("nm-" + name, binary)
                    rc, text = command("run-" + name, argv, "witness-runtime", timeout=10)
                    summary = re.findall(r"^SUMMARY kind=" + kind +
                        r" controls=(\d+) regressions=(\d+) fault_hits=(\d+)\s*$", text, re.M)
                    expected_red = hits if variant == "baseline" else 0
                    expected_rc = 10 if variant == "baseline" else 0
                    passed = rc == expected_rc and summary == [("0", str(expected_red), str(hits))]
                    result["witnesses"].append({"name": name, "exit": rc,
                        "expected_exit": expected_rc, "summaries": summary, "expected_controls": 0,
                        "expected_regressions": expected_red, "expected_fault_hits": hits,
                        "live_dependencies": deps, "runtime_executed": True, "passed": passed})
                    if not passed:
                        raise RuntimeError("exact witness mismatch; not an accepted red/green: " + name)
        stable()
        # Revalidate emitted bodies as well; output tampering is not a passing witness.
        for item in result["extracted_functions"]:
            if source.sha(out / item["path"]) != item["sha256"]:
                raise ValueError("extracted body changed during run")
        if len(result["witnesses"]) != 4 * len(variants) or len(result["gates"]) != 2:
            raise RuntimeError("incomplete witness/gate matrix")
        result["status"] = ("HOST_WITNESSES_PASS_DIFFERENTIAL" if baseline is not None
                            else "HOST_WITNESSES_PASS_CANDIDATE_ONLY")
        return_code = 0
    except BaseException as error:
        result["status"] = "FAILED_OR_BLOCKED_NOT_A_PASS"
        result["error"] = type(error).__name__ + ": " + str(error)
        return_code = 130 if isinstance(error, KeyboardInterrupt) else 2
    finally:
        result["finished_at"] = now()
        (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"] + ": " + str(out / "result.json"))
    return return_code


if __name__ == "__main__":
    # Let Python unwind active command owners on termination, not orphan a compiler.
    def terminate(_signum, _frame):
        raise KeyboardInterrupt("termination requested")
    signal.signal(signal.SIGTERM, terminate)
    sys.exit(main())
