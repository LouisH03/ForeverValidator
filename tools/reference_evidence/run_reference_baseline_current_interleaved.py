#!/usr/bin/env python3
"""Pinned 31-pair comparison of preserved and current Reference binaries."""

import argparse
import datetime
import hashlib
import json
import math
import os
import pathlib
import shutil
import statistics
import subprocess
import sys
import time


BASE_COMMIT = "78d55bbf09904eb7455c47430fa3905ea5c5466a"
BASELINE_BINARY_SHA256 = (
    "0ba6da34aaae3159533fffdae0068e28ac15f5974eddef458c7e51c10453d35f"
)
BASELINE_ARTIFACT_SHA256 = (
    "5e0f55afde4d47935c6f709503ad71b7f39ea4c246fde52e89cc6806919045c5"
)
BASELINE_FINGERPRINT = 5881665736893707250
HARNESS_SHA256 = (
    "8d1990cdd80a872cbdb48a23c1f3b1c6331c992ab0f5e33547d1d90ae5e08428"
)
REPLAY_SHA256 = (
    "c6e41ad8ca235eb578530c188e899fa9a5dc43066f8d4d5c27b2f3f92a6e4875"
)
PACKS = {
    "Alpine.pak": (
        20789256,
        "c70dc314290b0629111a177d459a664d9993da5a5926c53fcb23365c913169e3",
    ),
    "Bay.pak": (
        29691144,
        "d8468b5fa562906419b790d48feba274dd1a43c6ea89ab98a4afadffebc4eb4c",
    ),
    "Coast.pak": (
        32325640,
        "fe622d866993b0697004531396e82a8e063782258d6869d4929f2ff127d8593a",
    ),
    "Game.pak": (
        12370184,
        "bad3bc92bc2e448d9030cb59e8a97655db802ffea78cc4601e5013a2d79360d4",
    ),
    "Island.pak": (
        51324168,
        "84be8ae288d82fc0350b3ffaed8582a782065a766960fd187f6393590a064d03",
    ),
    "Rally.pak": (
        20028424,
        "bae1b2300d38fcc0579edb429f5de303ebc35e162f87c71328542d64cc0ad15c",
    ),
    "Resource.pak": (
        418056,
        "4522307fa4ddbe01b14fd6ba78eb02d50424b59405c1b85b233cdb3bf59b8989",
    ),
    "Speed.pak": (
        26421512,
        "64fee353971cfb6bc5e84ed045e77d81badb4b7d4ef547a162acdbec0a95ca0c",
    ),
    "Stadium.pak": (
        90124296,
        "21d42e4ce6c794e9f2dc674b60cb155882407e99601dae6f147e2fdd8987b22b",
    ),
    "packlist.dat": (
        421,
        "1526bb5bc135a8ca535c33f9baeaf34051eceecf57ecd54c13a757bdf3e7f478",
    ),
}
FNV_OFFSET = 1469598103934665603
FNV_PRIME = 1099511628211
MASK64 = (1 << 64) - 1


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while True:
            block = source.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def file_record(path):
    stat = path.stat()
    return {
        "path": str(path.resolve()),
        "size": stat.st_size,
        "sha256": sha256(path),
    }


def absolute_path(path):
    return path.expanduser().resolve()


def verify_file(path, expected_size=None, expected_sha256=None):
    if not path.is_file():
        raise RuntimeError(f"missing file: {path}")
    record = file_record(path)
    if expected_size is not None and record["size"] != expected_size:
        raise RuntimeError(
            f"size mismatch for {path}: {record['size']} != {expected_size}"
        )
    if expected_sha256 is not None and record["sha256"] != expected_sha256:
        raise RuntimeError(
            f"SHA-256 mismatch for {path}: {record['sha256']} != "
            f"{expected_sha256}"
        )
    return record


def run_capture(command, cwd=None, check=True):
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return {
        "command": command,
        "returncode": completed.returncode,
        "stdout": completed.stdout.strip(),
        "stderr": completed.stderr.strip(),
    }


def read_optional(path):
    try:
        return pathlib.Path(path).read_text(encoding="ascii").strip()
    except (FileNotFoundError, PermissionError, UnicodeDecodeError):
        return None


def host_snapshot(cpu, sibling):
    snapshot = {
        "timestamp_utc": utc_now(),
        "load_average": list(os.getloadavg()),
        "cpu": cpu,
        "smt_sibling": sibling,
        "scaling_driver": read_optional(
            f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_driver"
        ),
        "scaling_governor": read_optional(
            f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_governor"
        ),
        "scaling_cur_freq_khz": read_optional(
            f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_cur_freq"
        ),
        "energy_performance_preference": read_optional(
            f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/"
            "energy_performance_preference"
        ),
        "boost": read_optional("/sys/devices/system/cpu/cpufreq/boost"),
        "amd_pstate_status": read_optional(
            "/sys/devices/system/cpu/amd_pstate/status"
        ),
    }
    if shutil.which("powerprofilesctl"):
        snapshot["power_profile"] = run_capture(
            ["powerprofilesctl", "get"], check=False
        )
    if shutil.which("sensors"):
        snapshot["sensors_json"] = run_capture(
            ["sensors", "-j"], check=False
        )
    return snapshot


def host_policy(snapshot):
    keys = (
        "cpu",
        "smt_sibling",
        "scaling_driver",
        "scaling_governor",
        "energy_performance_preference",
        "boost",
        "amd_pstate_status",
        "power_profile",
    )
    return {key: snapshot.get(key) for key in keys}


def parse_cpu_counters(cpu):
    prefix = f"cpu{cpu} "
    for line in pathlib.Path("/proc/stat").read_text(encoding="ascii").splitlines():
        if line.startswith(prefix):
            values = [int(value) for value in line.split()[1:]]
            idle = values[3] + (values[4] if len(values) > 4 else 0)
            return sum(values), idle
    raise RuntimeError(f"CPU {cpu} is absent from /proc/stat")


def idle_fraction(cpu, seconds):
    total0, idle0 = parse_cpu_counters(cpu)
    time.sleep(seconds)
    total1, idle1 = parse_cpu_counters(cpu)
    total_delta = total1 - total0
    if total_delta <= 0:
        raise RuntimeError(f"no CPU accounting progress for CPU {cpu}")
    return (idle1 - idle0) / total_delta


def parse_cpu_list(text):
    cpus = []
    for item in text.split(","):
        if "-" in item:
            first, last = (int(value) for value in item.split("-", 1))
            cpus.extend(range(first, last + 1))
        else:
            cpus.append(int(item))
    return sorted(set(cpus))


def find_sibling(cpu):
    sibling_list = read_optional(
        f"/sys/devices/system/cpu/cpu{cpu}/topology/thread_siblings_list"
    )
    if sibling_list is None:
        return None
    siblings = [value for value in parse_cpu_list(sibling_list) if value != cpu]
    return siblings[0] if len(siblings) == 1 else siblings


def busy_processes():
    prefixes = (
        "forevervalidator",
        "optimized_cpu_benchmark",
        "cmake",
        "make",
        "ninja",
        "ctest",
        "cc1",
        "lto1",
        "collect2",
    )
    busy_scripts = (
        b"run_reference_baseline_current_interleaved.py",
        b"run_reference_final_determinism_gate.py",
    )
    records = []
    own_pid = os.getpid()
    for entry in pathlib.Path("/proc").iterdir():
        if not entry.name.isdigit() or int(entry.name) == own_pid:
            continue
        try:
            stat_text = (entry / "stat").read_text(encoding="ascii")
            stat_tail = stat_text[stat_text.rfind(") ") + 2 :].split()
            if not stat_tail or stat_tail[0] == "Z":
                continue
            comm = (entry / "comm").read_text(encoding="ascii").strip()
            executable = pathlib.Path(os.readlink(entry / "exe")).name
            cmdline_bytes = (entry / "cmdline").read_bytes()
            argv0 = pathlib.Path(
                cmdline_bytes.split(b"\0", 1)[0].decode(
                    "utf-8", errors="replace"
                )
            ).name
            script_busy = (
                comm.startswith(("python", "pypy"))
                or executable.startswith(("python", "pypy"))
            ) and any(script in cmdline_bytes for script in busy_scripts)
            if not (
                any(
                    name.startswith(prefixes)
                    for name in (comm, executable, argv0)
                )
                or script_busy
            ):
                continue
            records.append(
                {
                    "pid": int(entry.name),
                    "comm": comm,
                    "executable": executable,
                    "cmdline": cmdline_bytes.replace(b"\0", b" ")
                    .decode("utf-8", errors="replace")
                    .strip(),
                }
            )
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            continue
    return sorted(records, key=lambda record: record["pid"])


def parse_result(output):
    fields = {}
    for token in output.strip().split():
        key, separator, value = token.partition("=")
        if not separator or key in fields:
            raise RuntimeError(f"malformed benchmark output token: {token}")
        fields[key] = value
    expected = {
        "backend",
        "ticks",
        "warmups",
        "repetitions",
        "median_ns",
        "min_ns",
        "max_ns",
        "final_fingerprint",
        "result_checksum",
        "samples_ns",
    }
    if set(fields) != expected:
        raise RuntimeError(
            f"benchmark output fields differ: {sorted(fields)} != "
            f"{sorted(expected)}"
        )
    samples = [int(value) for value in fields["samples_ns"].split(",")]
    return {
        "backend": fields["backend"],
        "ticks": int(fields["ticks"]),
        "warmups": int(fields["warmups"]),
        "repetitions": int(fields["repetitions"]),
        "median_ns": int(fields["median_ns"]),
        "min_ns": int(fields["min_ns"]),
        "max_ns": int(fields["max_ns"]),
        "final_fingerprint": int(fields["final_fingerprint"]),
        "result_checksum": int(fields["result_checksum"]),
        "samples_ns": samples,
    }


def repeated_fingerprint_checksum(fingerprint, count):
    result = FNV_OFFSET
    encoded = fingerprint.to_bytes(8, sys.byteorder, signed=False)
    for _ in range(count):
        for byte in encoded:
            result ^= byte
            result = (result * FNV_PRIME) & MASK64
    return result


def validate_result(result, ticks, warmups, repetitions):
    if result["backend"] != "reference":
        raise RuntimeError("benchmark did not report the Reference backend")
    if result["ticks"] != ticks:
        raise RuntimeError("benchmark reported an unexpected tick count")
    if result["warmups"] != warmups:
        raise RuntimeError("benchmark reported an unexpected warmup count")
    if result["repetitions"] != repetitions:
        raise RuntimeError("benchmark reported an unexpected repetition count")
    samples = result["samples_ns"]
    if len(samples) != repetitions:
        raise RuntimeError("benchmark reported an unexpected sample count")
    if any(sample <= 0 for sample in samples):
        raise RuntimeError("benchmark reported a non-positive timing sample")
    if samples != sorted(samples):
        raise RuntimeError("benchmark samples are not sorted as documented")
    if result["median_ns"] != samples[len(samples) // 2]:
        raise RuntimeError("benchmark median does not match its raw samples")
    if result["min_ns"] != samples[0] or result["max_ns"] != samples[-1]:
        raise RuntimeError("benchmark extrema do not match its raw samples")
    if not 0 <= result["final_fingerprint"] <= MASK64:
        raise RuntimeError("benchmark fingerprint is outside uint64 range")
    if not 0 <= result["result_checksum"] <= MASK64:
        raise RuntimeError("benchmark checksum is outside uint64 range")
    expected_checksum = repeated_fingerprint_checksum(
        result["final_fingerprint"], warmups + repetitions
    )
    if result["result_checksum"] != expected_checksum:
        raise RuntimeError("benchmark state checksum is inconsistent")


def exact_median_interval(values, confidence=0.95):
    ordered = sorted(values)
    n = len(ordered)
    selected = None
    for k in range(1, n // 2 + 1):
        tail = sum(math.comb(n, index) for index in range(k)) / (2**n)
        coverage = 1.0 - 2.0 * tail
        if coverage >= confidence:
            selected = (k, coverage)
    if selected is None:
        return None
    k, coverage = selected
    return {
        "requested_confidence": confidence,
        "nominal_exact_coverage": coverage,
        "coverage_assumptions": (
            "independent paired ratios with a common population median; "
            "ties can make coverage conservative"
        ),
        "lower": ordered[k - 1],
        "upper": ordered[n - k],
        "lower_order_statistic_one_based": k,
        "upper_order_statistic_one_based": n - k + 1,
    }


def exact_two_sided_sign_test(values, null=1.0):
    above = sum(value > null for value in values)
    below = sum(value < null for value in values)
    ties = len(values) - above - below
    n = above + below
    if n == 0:
        p_value = 1.0
    else:
        smaller = min(above, below)
        tail = sum(math.comb(n, index) for index in range(smaller + 1))
        p_value = min(1.0, 2.0 * tail / (2**n))
    return {
        "null_ratio": null,
        "above": above,
        "below": below,
        "ties_excluded": ties,
        "two_sided_p_value": p_value,
    }


def median(values):
    return statistics.median(values)


def atomic_write_json(path, evidence):
    rendered = json.dumps(evidence, indent=2, sort_keys=True) + "\n"
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(rendered, encoding="utf-8")
    temporary.replace(path)


def verify_repo(repo, build_dir):
    if run_capture(["git", "diff", "--quiet"], cwd=repo, check=False)[
        "returncode"
    ]:
        raise RuntimeError("tracked worktree changes remain")
    if run_capture(
        ["git", "diff", "--cached", "--quiet"], cwd=repo, check=False
    )["returncode"]:
        raise RuntimeError("staged changes remain")
    ancestry = run_capture(
        ["git", "merge-base", "--is-ancestor", BASE_COMMIT, "HEAD"],
        cwd=repo,
        check=False,
    )
    if ancestry["returncode"]:
        raise RuntimeError("preserved base commit is not an ancestor of HEAD")
    harness = repo / "tools/optimized_cpu_benchmark.cpp"
    verify_file(harness, expected_sha256=HARNESS_SHA256)
    unchanged = run_capture(
        [
            "git",
            "diff",
            "--quiet",
            BASE_COMMIT,
            "HEAD",
            "--",
            "tools/optimized_cpu_benchmark.cpp",
        ],
        cwd=repo,
        check=False,
    )
    if unchanged["returncode"]:
        raise RuntimeError("benchmark harness changed since the preserved base")
    cache = build_dir / "CMakeCache.txt"
    cache_text = cache.read_text(encoding="utf-8")
    required_cache = (
        "CMAKE_BUILD_TYPE:STRING=Release",
        "FOREVERVALIDATOR_BUILD_DEVELOPER_BENCHMARKS:UNINITIALIZED=ON",
        "FOREVERVALIDATOR_ENABLE_CUDA:BOOL=OFF",
    )
    for setting in required_cache:
        if setting not in cache_text:
            raise RuntimeError(f"build cache does not contain {setting}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo",
        type=pathlib.Path,
        default=pathlib.Path(
            "/home/mikael/Projects/ForeverValidator-reference-determinism"
        ),
    )
    parser.add_argument(
        "--build-dir",
        type=pathlib.Path,
        default=pathlib.Path(
            "/home/mikael/Projects/ForeverValidator-reference-determinism/"
            "build-reference-determinism"
        ),
    )
    parser.add_argument(
        "--baseline",
        type=pathlib.Path,
        default=pathlib.Path(
            "/tmp/reference-baseline-bin/optimized_cpu_benchmark"
        ),
    )
    parser.add_argument(
        "--current",
        type=pathlib.Path,
        default=pathlib.Path(
            "/tmp/reference-final-bin/optimized_cpu_benchmark"
        ),
    )
    parser.add_argument(
        "--current-build",
        type=pathlib.Path,
        default=pathlib.Path(
            "/home/mikael/Projects/ForeverValidator-reference-determinism/"
            "build-reference-determinism/optimized_cpu_benchmark"
        ),
    )
    parser.add_argument(
        "--baseline-artifact",
        type=pathlib.Path,
        default=pathlib.Path(
            "/tmp/reference-baseline-performance-557-3000x101.txt"
        ),
    )
    parser.add_argument(
        "--packs",
        type=pathlib.Path,
        default=pathlib.Path("/home/mikael/TmUnitedForever/Packs"),
    )
    parser.add_argument(
        "--replay",
        type=pathlib.Path,
        default=pathlib.Path(
            "/home/mikael/Downloads/massvalidation (1)/massvalidation/"
            "stadium/kacky/6/557.Replay.Gbx"
        ),
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path(
            "/tmp/reference-final-performance-557-interleaved.json"
        ),
    )
    parser.add_argument("--cpu", type=int, default=3)
    parser.add_argument("--pairs", type=int, default=31)
    parser.add_argument("--ticks", type=int, default=3000)
    parser.add_argument("--warmups", type=int, default=5)
    parser.add_argument("--repetitions", type=int, default=11)
    parser.add_argument("--settle-seconds", type=int, default=60)
    parser.add_argument("--idle-check-seconds", type=float, default=2.0)
    parser.add_argument("--minimum-idle-fraction", type=float, default=0.90)
    parser.add_argument("--run-timeout-seconds", type=int, default=180)
    arguments = parser.parse_args()

    for name in (
        "repo",
        "build_dir",
        "baseline",
        "current",
        "current_build",
        "baseline_artifact",
        "packs",
        "replay",
        "output",
    ):
        setattr(arguments, name, absolute_path(getattr(arguments, name)))
    if arguments.pairs < 9 or (arguments.pairs % 2) == 0:
        raise RuntimeError("pairs must be odd and at least 9")
    if arguments.repetitions <= 0 or (arguments.repetitions % 2) == 0:
        raise RuntimeError("repetitions must be positive and odd")
    if arguments.warmups <= 0 or arguments.ticks <= 0:
        raise RuntimeError("ticks and warmups must be positive")
    if arguments.cpu < 0:
        raise RuntimeError("CPU must be non-negative")
    if arguments.settle_seconds < 0:
        raise RuntimeError("settle time must be non-negative")
    if arguments.idle_check_seconds <= 0:
        raise RuntimeError("idle check duration must be positive")
    if not 0.0 <= arguments.minimum_idle_fraction <= 1.0:
        raise RuntimeError("minimum idle fraction must be between zero and one")
    if arguments.run_timeout_seconds <= 0:
        raise RuntimeError("run timeout must be positive")
    if arguments.output.exists():
        raise RuntimeError(f"refusing to overwrite evidence: {arguments.output}")

    repo = arguments.repo
    build_dir = arguments.build_dir
    verify_repo(repo, build_dir)
    allowed_cpus = os.sched_getaffinity(0)
    if arguments.cpu not in allowed_cpus:
        raise RuntimeError(
            f"CPU {arguments.cpu} is outside the process affinity mask "
            f"{sorted(allowed_cpus)}"
        )
    sibling = find_sibling(arguments.cpu)
    if not isinstance(sibling, int):
        raise RuntimeError(
            f"expected one SMT sibling for CPU {arguments.cpu}, got {sibling}"
        )
    if sibling not in allowed_cpus:
        raise RuntimeError(
            f"SMT sibling {sibling} is outside the process affinity mask"
        )
    if shutil.which("taskset", path="/usr/bin:/bin") is None:
        raise RuntimeError("taskset is unavailable")

    active = busy_processes()
    if active:
        raise RuntimeError(f"build or validation processes are active: {active}")

    baseline = verify_file(
        arguments.baseline,
        expected_sha256=BASELINE_BINARY_SHA256,
    )
    current = verify_file(arguments.current)
    current_build = verify_file(arguments.current_build)
    for binary in (arguments.baseline, arguments.current, arguments.current_build):
        if not os.access(binary, os.X_OK):
            raise RuntimeError(f"benchmark binary is not executable: {binary}")
    if current["sha256"] != current_build["sha256"]:
        raise RuntimeError("current preserved binary differs from the build output")
    artifact = verify_file(
        arguments.baseline_artifact,
        expected_sha256=BASELINE_ARTIFACT_SHA256,
    )
    baseline_artifact_result = parse_result(
        arguments.baseline_artifact.read_text(encoding="ascii")
    )
    validate_result(baseline_artifact_result, 3000, 5, 101)
    if baseline_artifact_result["final_fingerprint"] != BASELINE_FINGERPRINT:
        raise RuntimeError("preserved baseline artifact fingerprint differs")

    replay = verify_file(
        arguments.replay, expected_sha256=REPLAY_SHA256
    )
    packs = []
    for name, (size, digest) in sorted(PACKS.items()):
        packs.append(
            verify_file(
                arguments.packs / name,
                expected_size=size,
                expected_sha256=digest,
            )
        )

    flags_path = (
        build_dir / "CMakeFiles/optimized_cpu_benchmark.dir/flags.make"
    )
    link_path = (
        build_dir / "CMakeFiles/optimized_cpu_benchmark.dir/link.txt"
    )
    evidence = {
        "schema": "forevervalidator-reference-baseline-current-v1",
        "complete": False,
        "started_utc": utc_now(),
        "runner": file_record(pathlib.Path(__file__).resolve()),
        "source": {
            "repository": str(repo),
            "base_commit": BASE_COMMIT,
            "head_commit": run_capture(
                ["git", "rev-parse", "HEAD"], cwd=repo
            )["stdout"],
            "status_porcelain": run_capture(
                ["git", "status", "--porcelain=v1"], cwd=repo
            )["stdout"],
            "benchmark_harness": file_record(
                repo / "tools/optimized_cpu_benchmark.cpp"
            ),
        },
        "build": {
            "directory": str(build_dir),
            "cmake_cache": file_record(build_dir / "CMakeCache.txt"),
            "flags": file_record(flags_path) if flags_path.is_file() else None,
            "link": file_record(link_path) if link_path.is_file() else None,
            "compiler": run_capture(["/usr/bin/c++", "--version"]),
        },
        "binaries": {
            "baseline": baseline,
            "current_snapshot": current,
            "current_build": current_build,
        },
        "baseline_sanity_artifact": {
            "file": artifact,
            "result": baseline_artifact_result,
        },
        "inputs": {"replay": replay, "packs": packs},
        "protocol": {
            "backend": "reference",
            "clock": "std::chrono::steady_clock",
            "measured_operation": (
                f"PhysicsSandbox::AdvanceTicks({arguments.ticks})"
            ),
            "cpu": arguments.cpu,
            "smt_sibling": sibling,
            "pairs": arguments.pairs,
            "pair_orders": (
                "AB/BA: baseline,current for odd-numbered pairs; "
                "current,baseline for even-numbered pairs"
            ),
            "tick_count": arguments.ticks,
            "tick_duration_ms": 10,
            "prestart_duration_ms": 2600,
            "warmup_count_per_invocation": arguments.warmups,
            "measured_repetitions_per_invocation": arguments.repetitions,
            "invocations_per_build": arguments.pairs,
            "measured_samples_per_build": (
                arguments.pairs * arguments.repetitions
            ),
            "state_advances_checked_per_build": (
                arguments.pairs
                * (arguments.warmups + arguments.repetitions)
            ),
            "restore_before_each_advance": True,
            "thread_count": 1,
            "affinity_command": ["taskset", "-c", str(arguments.cpu)],
            "settle_seconds": arguments.settle_seconds,
            "outlier_policy": "retain all successful measured samples",
            "primary_estimator": (
                "median across adjacent AB/BA pairs of the "
                "current/baseline invocation-median ratio"
            ),
            "interval": (
                "exact distribution-free order-statistic interval for the "
                "population median paired ratio, at least 95% nominal "
                "coverage under independent pairs"
            ),
            "hypothesis_check": (
                "two-sided exact paired sign test of paired ratio = 1"
            ),
        },
        "host": {
            "lscpu": run_capture(["lscpu"]),
            "uname": run_capture(["uname", "-a"]),
            "initial": host_snapshot(arguments.cpu, sibling),
        },
        "runs": [],
    }
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    atomic_write_json(arguments.output, evidence)

    time.sleep(arguments.settle_seconds)
    active = busy_processes()
    if active:
        raise RuntimeError(
            f"build or validation processes became active while settling: {active}"
        )
    idle = {
        str(cpu): idle_fraction(cpu, arguments.idle_check_seconds)
        for cpu in (arguments.cpu, sibling)
    }
    evidence["host"]["pre_measurement_idle_fraction"] = idle
    if min(idle.values()) < arguments.minimum_idle_fraction:
        raise RuntimeError(
            f"CPU {arguments.cpu} or sibling {sibling} was not sufficiently "
            f"idle before measurement: {idle}"
        )
    evidence["host"]["pre_measurement"] = host_snapshot(
        arguments.cpu, sibling
    )
    expected_host_policy = host_policy(evidence["host"]["pre_measurement"])
    evidence["host"]["required_stable_policy"] = expected_host_policy
    atomic_write_json(arguments.output, evidence)

    sanitized_environment = {
        "HOME": os.environ.get("HOME", "/home/mikael"),
        "LANG": "C",
        "LC_ALL": "C",
        "PATH": "/usr/bin:/bin",
        "TZ": "UTC",
    }
    binary_paths = {
        "baseline": pathlib.Path(baseline["path"]),
        "current": pathlib.Path(current["path"]),
    }
    for pair_index in range(arguments.pairs):
        active = busy_processes()
        if active:
            raise RuntimeError(
                f"build or validation processes became active before pair "
                f"{pair_index + 1}: {active}"
            )
        if (pair_index % 2) == 0:
            order = ["baseline", "current"]
        else:
            order = ["current", "baseline"]
        pair_snapshot = host_snapshot(arguments.cpu, sibling)
        if host_policy(pair_snapshot) != expected_host_policy:
            raise RuntimeError(
                f"host power policy changed before pair {pair_index + 1}"
            )
        for slot_index, build_name in enumerate(order):
            command = [
                "taskset",
                "-c",
                str(arguments.cpu),
                str(binary_paths[build_name]),
                str(arguments.packs.resolve()),
                str(arguments.replay.resolve()),
                "reference",
                str(arguments.ticks),
                str(arguments.warmups),
                str(arguments.repetitions),
            ]
            started = utc_now()
            wall_start = time.monotonic_ns()
            completed = subprocess.run(
                command,
                cwd=repo,
                env=sanitized_environment,
                check=True,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=arguments.run_timeout_seconds,
            )
            wall_stop = time.monotonic_ns()
            result = parse_result(completed.stdout)
            validate_result(
                result,
                arguments.ticks,
                arguments.warmups,
                arguments.repetitions,
            )
            if (
                build_name == "baseline"
                and result["final_fingerprint"] != BASELINE_FINGERPRINT
            ):
                raise RuntimeError(
                    "preserved baseline binary fingerprint changed"
                )
            evidence["runs"].append(
                {
                    "pair_one_based": pair_index + 1,
                    "slot_one_based": slot_index + 1,
                    "order": order,
                    "build": build_name,
                    "started_utc": started,
                    "finished_utc": utc_now(),
                    "process_wall_ns": wall_stop - wall_start,
                    "command": command,
                    "stdout": completed.stdout.strip(),
                    "stderr": completed.stderr.strip(),
                    "result": result,
                }
            )
        pair_after = host_snapshot(arguments.cpu, sibling)
        if host_policy(pair_after) != expected_host_policy:
            raise RuntimeError(
                f"host power policy changed after pair {pair_index + 1}"
            )
        active = busy_processes()
        if active:
            raise RuntimeError(
                f"build or validation processes became active after pair "
                f"{pair_index + 1}: {active}"
            )
        evidence.setdefault("pair_host_snapshots", []).append(
            {
                "pair_one_based": pair_index + 1,
                "before": pair_snapshot,
                "after": pair_after,
            }
        )
        atomic_write_json(arguments.output, evidence)

    expected_runs = arguments.pairs * 2
    if len(evidence["runs"]) != expected_runs:
        raise RuntimeError("unexpected completed run count")
    per_build_runs = {
        build: [run for run in evidence["runs"] if run["build"] == build]
        for build in ("baseline", "current")
    }
    for build, records in per_build_runs.items():
        if len(records) != arguments.pairs:
            raise RuntimeError(f"unbalanced invocation count for {build}")
        fingerprints = {
            record["result"]["final_fingerprint"] for record in records
        }
        checksums = {
            record["result"]["result_checksum"] for record in records
        }
        if len(fingerprints) != 1 or len(checksums) != 1:
            raise RuntimeError(f"state hashes were not stable for {build}")

    paired_ratios = []
    for pair_index in range(arguments.pairs):
        records = [
            record
            for record in evidence["runs"]
            if record["pair_one_based"] == pair_index + 1
        ]
        baseline_medians = [
            record["result"]["median_ns"]
            for record in records
            if record["build"] == "baseline"
        ]
        current_medians = [
            record["result"]["median_ns"]
            for record in records
            if record["build"] == "current"
        ]
        if len(baseline_medians) != 1 or len(current_medians) != 1:
            raise RuntimeError("AB/BA pair is incomplete")
        paired_ratios.append(
            {
                "pair_one_based": pair_index + 1,
                "order": records[0]["order"],
                "baseline_median_ns": baseline_medians[0],
                "current_median_ns": current_medians[0],
                "current_over_baseline_ratio": (
                    current_medians[0] / baseline_medians[0]
                ),
            }
        )

    ratios = [
        record["current_over_baseline_ratio"] for record in paired_ratios
    ]
    primary_ratio = median(ratios)
    all_samples = {
        build: [
            sample
            for record in records
            for sample in record["result"]["samples_ns"]
        ]
        for build, records in per_build_runs.items()
    }
    fingerprints = {
        build: sorted(
            {
                record["result"]["final_fingerprint"]
                for record in records
            }
        )
        for build, records in per_build_runs.items()
    }
    checksums = {
        build: sorted(
            {
                record["result"]["result_checksum"] for record in records
            }
        )
        for build, records in per_build_runs.items()
    }
    evidence["paired_ratios"] = paired_ratios
    evidence["result"] = {
        "primary_current_over_baseline_ratio": primary_ratio,
        "primary_current_delta_percent": (primary_ratio - 1.0) * 100.0,
        "primary_baseline_over_current_speedup": 1.0 / primary_ratio,
        "exact_median_ratio_interval": exact_median_interval(ratios),
        "exact_paired_sign_test": exact_two_sided_sign_test(ratios),
        "paired_ratio_geometric_mean": math.exp(
            sum(math.log(value) for value in ratios) / len(ratios)
        ),
        "paired_ratio_min": min(ratios),
        "paired_ratio_max": max(ratios),
        "baseline_first_ratio_median": median(ratios[0::2]),
        "baseline_first_pair_count": len(ratios[0::2]),
        "current_first_ratio_median": median(ratios[1::2]),
        "current_first_pair_count": len(ratios[1::2]),
        "first_half_ratio_median": median(
            ratios[: len(ratios) // 2]
        ),
        "first_half_pair_count": len(ratios[: len(ratios) // 2]),
        "second_half_ratio_median": median(
            ratios[len(ratios) // 2 :]
        ),
        "second_half_pair_count": len(ratios[len(ratios) // 2 :]),
        "descriptive_pooled_samples": {
            build: {
                "count": len(samples),
                "median_ns": median(samples),
                "min_ns": min(samples),
                "max_ns": max(samples),
            }
            for build, samples in all_samples.items()
        },
        "stable_final_fingerprint": fingerprints,
        "stable_result_checksum": checksums,
        "state_hash_note": (
            "Cross-build equality is not required: corrections may change the "
            "Reference state. Each build must be internally stable."
        ),
    }
    evidence["host"]["final"] = host_snapshot(arguments.cpu, sibling)
    if host_policy(evidence["host"]["final"]) != expected_host_policy:
        raise RuntimeError("host power policy changed during measurement")
    verify_repo(repo, build_dir)
    evidence["post_verification"] = {
        "runner": verify_file(pathlib.Path(__file__).resolve()),
        "head_commit": run_capture(
            ["git", "rev-parse", "HEAD"], cwd=repo
        )["stdout"],
        "benchmark_harness": verify_file(
            repo / "tools/optimized_cpu_benchmark.cpp",
            expected_sha256=HARNESS_SHA256,
        ),
        "cmake_cache": verify_file(build_dir / "CMakeCache.txt"),
        "flags": verify_file(flags_path) if flags_path.is_file() else None,
        "link": verify_file(link_path) if link_path.is_file() else None,
        "baseline": verify_file(
            arguments.baseline,
            expected_sha256=BASELINE_BINARY_SHA256,
        ),
        "current_snapshot": verify_file(arguments.current),
        "current_build": verify_file(arguments.current_build),
        "baseline_artifact": verify_file(
            arguments.baseline_artifact,
            expected_sha256=BASELINE_ARTIFACT_SHA256,
        ),
        "replay": verify_file(
            arguments.replay, expected_sha256=REPLAY_SHA256
        ),
        "packs": [
            verify_file(
                arguments.packs / name,
                expected_size=size,
                expected_sha256=digest,
            )
            for name, (size, digest) in sorted(PACKS.items())
        ],
        "busy_processes": busy_processes(),
    }
    post = evidence["post_verification"]
    unchanged_records = (
        ("runner", evidence["runner"], post["runner"]),
        (
            "benchmark harness",
            evidence["source"]["benchmark_harness"],
            post["benchmark_harness"],
        ),
        ("CMake cache", evidence["build"]["cmake_cache"], post["cmake_cache"]),
        ("compiler flags", evidence["build"]["flags"], post["flags"]),
        ("link command", evidence["build"]["link"], post["link"]),
        (
            "baseline binary",
            evidence["binaries"]["baseline"],
            post["baseline"],
        ),
        (
            "current snapshot",
            evidence["binaries"]["current_snapshot"],
            post["current_snapshot"],
        ),
        (
            "current build",
            evidence["binaries"]["current_build"],
            post["current_build"],
        ),
        (
            "baseline artifact",
            evidence["baseline_sanity_artifact"]["file"],
            post["baseline_artifact"],
        ),
        ("replay", evidence["inputs"]["replay"], post["replay"]),
    )
    for label, before, after in unchanged_records:
        if before != after:
            raise RuntimeError(f"{label} changed during measurement")
    if post["packs"] != evidence["inputs"]["packs"]:
        raise RuntimeError("installed packs changed during measurement")
    if post["head_commit"] != evidence["source"]["head_commit"]:
        raise RuntimeError("repository HEAD changed during measurement")
    if post["busy_processes"]:
        raise RuntimeError(
            "build or validation processes are active after measurement: "
            f"{post['busy_processes']}"
        )
    evidence["complete"] = True
    evidence["finished_utc"] = utc_now()
    atomic_write_json(arguments.output, evidence)

    interval = evidence["result"]["exact_median_ratio_interval"]
    print(
        "current/baseline="
        f"{primary_ratio:.6f} "
        f"delta={evidence['result']['primary_current_delta_percent']:+.3f}% "
        f"exact_interval=[{interval['lower']:.6f},"
        f"{interval['upper']:.6f}] "
        f"nominal_coverage={interval['nominal_exact_coverage']:.6f} "
        f"p={evidence['result']['exact_paired_sign_test']['two_sided_p_value']:.6g}"
    )
    print(f"evidence={arguments.output.resolve()}")


if __name__ == "__main__":
    try:
        main()
    except (
        OSError,
        RuntimeError,
        subprocess.CalledProcessError,
        subprocess.TimeoutExpired,
        ValueError,
    ) as error:
        print(
            f"run_reference_baseline_current_interleaved.py: {error}",
            file=sys.stderr,
        )
        sys.exit(1)
