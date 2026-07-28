#!/usr/bin/env python3
"""Run and audit the full Reference same-process determinism gate.

This is an evidence harness, not production code.  The CLI's hidden
--repeat-same-process option validates each request twice in one process and
compares the exit code and serialized JSON.  Repeating each shard directory
five times yields ten validations per replay while retaining five independently
emitted copies for an external byte comparison.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any, Iterable


REPO = Path("/home/mikael/Projects/ForeverValidator-reference-determinism")
DEFAULT_BINARY = REPO / "build-reference-determinism/forevervalidator"
DEFAULT_PAKS = Path("/home/mikael/TmUnitedForever/Packs")
MASSVALIDATION = Path(
    "/home/mikael/Downloads/massvalidation (1)/massvalidation"
)

CORPORA = {
    "united": {
        "root": MASSVALIDATION / "united",
        "shards": tuple(str(index) for index in range(1, 13)),
        "expected_count": 3745,
        "expected_errors": {
            "6/5000431.Replay.Gbx",
            "7/5002044.Replay.Gbx",
            "8/5003362.Replay.Gbx",
        },
    },
    "stadium": {
        "root": MASSVALIDATION / "stadium/kacky",
        "shards": tuple(str(index) for index in range(1, 17)),
        "expected_count": 2169,
        "expected_errors": set(),
    },
}

RESULT_LINE = re.compile(
    r"^result: (?P<path>.+) -> (?P<kind>valid|invalid|error)"
    r"(?: \((?P<exit>[0-9]+)\))?\n?$"
)
CATEGORY_LINE = re.compile(
    r" category=(?P<category>\S+) reason=(?P<reason>\S+)\n?$"
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: Any) -> None:
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def run_checked(args: list[str]) -> str:
    return subprocess.run(
        args,
        cwd=REPO,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ).stdout.strip()


def absolute_path(path: Path) -> Path:
    return path.expanduser().resolve()


def git_head() -> str:
    return run_checked(["git", "rev-parse", "HEAD"])


def require_clean_tracked_tree() -> None:
    for args in (
        ["git", "diff", "--quiet"],
        ["git", "diff", "--cached", "--quiet"],
    ):
        result = subprocess.run(args, cwd=REPO, check=False)
        if result.returncode != 0:
            raise RuntimeError("tracked worktree or index is not clean")


def active_validator_pids() -> list[int]:
    pids: list[int] = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            comm = (entry / "comm").read_text(encoding="utf-8").strip()
            executable = Path(os.readlink(entry / "exe")).name
            argv0 = Path(
                (entry / "cmdline")
                .read_bytes()
                .split(b"\0", 1)[0]
                .decode("utf-8")
            ).name
            if (
                comm in {"forevervalidator", "forevervalidato"}
                or executable == "forevervalidator"
                or argv0 == "forevervalidator"
            ):
                pids.append(int(entry.name))
        except (
            FileNotFoundError,
            PermissionError,
            ProcessLookupError,
            UnicodeDecodeError,
        ):
            pass
    return sorted(pids)


def replay_files(root: Path) -> list[Path]:
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() == ".gbx"
    )


def file_manifest(root: Path, paths: Iterable[Path]) -> dict[str, Any]:
    digest = hashlib.sha256()
    count = 0
    total_bytes = 0
    for path in paths:
        relative = path.relative_to(root).as_posix()
        size = path.stat().st_size
        content_hash = sha256_file(path)
        record = f"{relative}\0{size}\0{content_hash}\n".encode()
        digest.update(record)
        count += 1
        total_bytes += size
    return {
        "count": count,
        "bytes": total_bytes,
        "manifest_sha256": digest.hexdigest(),
    }


def capture_input_manifests(
    paks: Path,
) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    corpus_manifests = {}
    for corpus, config in CORPORA.items():
        files = replay_files(config["root"])
        if len(files) != config["expected_count"]:
            raise RuntimeError(
                f"{corpus}: expected {config['expected_count']}, got {len(files)}"
            )
        corpus_manifests[corpus] = {
            "root": str(absolute_path(config["root"])),
            "expected_errors": sorted(config["expected_errors"]),
            **file_manifest(config["root"], files),
        }
    pack_paths = sorted(path for path in paks.iterdir() if path.is_file())
    return corpus_manifests, file_manifest(paks, pack_paths)


def parse_summary(path: Path) -> dict[str, Any]:
    lines = [
        line for line in path.read_text(encoding="utf-8").splitlines() if line
    ]
    if len(lines) != 1:
        raise RuntimeError(f"{path}: expected one stdout summary line")
    value = json.loads(lines[0])
    if value.get("schema") != "forevervalidator-batch-v1":
        raise RuntimeError(f"{path}: unexpected batch schema")
    count_keys = ("total", "valid", "invalid", "error")
    for key in count_keys:
        count = value.get(key)
        if isinstance(count, bool) or not isinstance(count, int) or count < 0:
            raise RuntimeError(f"{path}: invalid {key} count")
    if value["total"] != value["valid"] + value["invalid"] + value["error"]:
        raise RuntimeError(f"{path}: batch counts do not sum to total")
    return value


def parse_stderr_blocks(data: bytes) -> dict[str, list[dict[str, Any]]]:
    """Extract one raw validate/result block for each externally emitted run."""
    decoded = data.decode("utf-8")
    lines = decoded.splitlines(keepends=True)
    blocks: dict[str, list[dict[str, Any]]] = defaultdict(list)
    current_path: str | None = None
    current_lines: list[str] = []

    for line in lines:
        if line.startswith("validate: "):
            if current_path is not None:
                raise RuntimeError(
                    f"nested validate block before result for {current_path}"
                )
            current_path = line[len("validate: ") :].rstrip("\r\n")
            current_lines = [line]
            continue
        if current_path is None:
            if "same-process replay result mismatch" in line:
                raise RuntimeError(line.strip())
            continue

        current_lines.append(line)
        match = RESULT_LINE.match(line)
        if not match:
            continue
        if match.group("path") != current_path:
            raise RuntimeError(
                f"result path {match.group('path')} != {current_path}"
            )
        raw = "".join(current_lines).encode("utf-8")
        category = None
        reason = None
        for detail in current_lines:
            category_match = CATEGORY_LINE.search(detail)
            if category_match:
                category = category_match.group("category")
                reason = category_match.group("reason")
        blocks[current_path].append(
            {
                "raw": raw,
                "kind": match.group("kind"),
                "exit": int(match.group("exit") or (0 if match.group("kind") == "valid" else 1)),
                "category": category,
                "reason": reason,
            }
        )
        current_path = None
        current_lines = []

    if current_path is not None:
        raise RuntimeError(f"unterminated validate block for {current_path}")
    return blocks


def semantic_projection(value: dict[str, Any]) -> dict[str, Any]:
    """Stable validation semantics, excluding identity and schema framing.

    This deliberately does not claim to be the private full runtime-clone hash.
    """
    return {
        key: value[key]
        for key in sorted(value)
        if key not in {"replay", "schema"}
    }


def semantic_hash(value: dict[str, Any]) -> str:
    encoded = json.dumps(
        semantic_projection(value),
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")
    return sha256_bytes(encoded)


def load_result_groups(results: Path) -> dict[str, list[dict[str, Any]]]:
    groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for path in sorted(results.glob("*.json")):
        raw = path.read_bytes()
        value = json.loads(raw)
        identity = value.get("replay")
        if not isinstance(identity, str):
            raise RuntimeError(f"{path}: missing replay identity")
        groups[identity].append(
            {
                "path": str(path),
                "raw": raw,
                "value": value,
            }
        )
    return groups


def expected_exit(summary: dict[str, Any]) -> int:
    if summary["error"]:
        return 2
    return 1 if summary["invalid"] else 0


def validate_ordinary_manifest(
    ordinary_root: Path,
    corpus: str,
    authority: dict[str, Any],
) -> dict[str, Any]:
    manifest_path = ordinary_root / "manifest.json"
    if not manifest_path.is_file():
        raise RuntimeError(f"{corpus}: ordinary manifest not found at {manifest_path}")
    value = json.loads(manifest_path.read_text(encoding="utf-8"))
    expected = {
        "commit": authority["commit"],
        "binary_sha256": authority["binary_sha256"],
        "backend": "reference",
    }
    for key, expected_value in expected.items():
        if value.get(key) != expected_value:
            raise RuntimeError(
                f"{corpus}: ordinary manifest {key}={value.get(key)!r}, "
                f"expected {expected_value!r}"
            )
    if absolute_path(Path(value.get("corpus", ""))) != absolute_path(
        CORPORA[corpus]["root"]
    ):
        raise RuntimeError(f"{corpus}: ordinary manifest corpus path differs")
    if absolute_path(Path(value.get("pak_dir", ""))) != absolute_path(
        Path(authority["pak_dir"])
    ):
        raise RuntimeError(f"{corpus}: ordinary manifest pack path differs")
    observed_shards = tuple(str(shard) for shard in value.get("shards", ()))
    if observed_shards != CORPORA[corpus]["shards"]:
        raise RuntimeError(f"{corpus}: ordinary manifest shards differ")
    return {
        "root": str(ordinary_root),
        "manifest_sha256": sha256_file(manifest_path),
        "commit": value["commit"],
        "binary_sha256": value["binary_sha256"],
    }


def ordinary_evidence(
    ordinary_root: Path | None,
    corpus: str,
    shard: str,
) -> tuple[dict[str, list[dict[str, Any]]], dict[str, list[dict[str, Any]]]]:
    if ordinary_root is None:
        return {}, {}
    shard_root = ordinary_root / shard
    results = load_result_groups(shard_root / "results")
    log_candidates = [shard_root / "run.log", shard_root / "stderr.log"]
    log = next((candidate for candidate in log_candidates if candidate.exists()), None)
    if log is None:
        raise RuntimeError(f"{corpus}/{shard}: ordinary log not found")
    return results, parse_stderr_blocks(log.read_bytes())


def audit_shard(
    evidence_root: Path,
    corpus: str,
    shard: str,
    emitted_repeats: int,
    ordinary_root: Path | None,
) -> list[dict[str, Any]]:
    config = CORPORA[corpus]
    corpus_root = config["root"]
    shard_root = evidence_root / corpus / shard
    summary = parse_summary(shard_root / "stdout.json")
    exit_code = int((shard_root / "exit.txt").read_text().strip())
    if exit_code != expected_exit(summary):
        raise RuntimeError(
            f"{corpus}/{shard}: exit {exit_code}, summary implies "
            f"{expected_exit(summary)}"
        )

    stderr = (shard_root / "stderr.log").read_bytes()
    if b"same-process replay result mismatch" in stderr or b" (71)" in stderr:
        raise RuntimeError(f"{corpus}/{shard}: CLI same-process mismatch")
    blocks = parse_stderr_blocks(stderr)
    results = load_result_groups(shard_root / "results")
    ordinary_results, ordinary_blocks = ordinary_evidence(
        ordinary_root, corpus, shard
    )

    inputs = replay_files(corpus_root / shard)
    if summary["total"] != len(inputs) * emitted_repeats:
        raise RuntimeError(f"{corpus}/{shard}: unexpected summary total")

    records: list[dict[str, Any]] = []
    observed_counts = Counter()
    for replay in inputs:
        identity = str(replay)
        relative = replay.relative_to(corpus_root).as_posix()
        replay_blocks = blocks.get(identity, [])
        if len(replay_blocks) != emitted_repeats:
            raise RuntimeError(
                f"{relative}: expected {emitted_repeats} log blocks, "
                f"got {len(replay_blocks)}"
            )
        block_kinds = {block["kind"] for block in replay_blocks}
        if len(block_kinds) != 1:
            raise RuntimeError(f"{relative}: unstable emitted classification")
        kind = next(iter(block_kinds))
        observed_counts[kind] += emitted_repeats

        raw_block_hashes = {
            sha256_bytes(block["raw"]) for block in replay_blocks
        }
        if len(raw_block_hashes) != 1:
            raise RuntimeError(f"{relative}: log blocks differ byte-for-byte")

        if relative in config["expected_errors"]:
            if kind != "error" or identity in results:
                raise RuntimeError(f"{relative}: expected source error")
            if any(
                block["exit"] <= 1
                or block["category"] is None
                or block["reason"] is None
                for block in replay_blocks
            ):
                raise RuntimeError(f"{relative}: incomplete error classification")
            error_semantics = {
                (
                    block["exit"],
                    block["category"],
                    block["reason"],
                )
                for block in replay_blocks
            }
            if len(error_semantics) != 1:
                raise RuntimeError(f"{relative}: unstable error semantics")
            result_hash = None
            projection_hash = sha256_bytes(
                json.dumps(
                    next(iter(error_semantics)),
                    separators=(",", ":"),
                ).encode()
            )
            classification = (
                f"error:{replay_blocks[0]['exit']}:"
                f"{replay_blocks[0]['category']}:"
                f"{replay_blocks[0]['reason']}"
            )
        else:
            copies = results.get(identity, [])
            if kind == "error" or len(copies) != emitted_repeats:
                raise RuntimeError(
                    f"{relative}: expected {emitted_repeats} result files"
                )
            raw_hashes = {sha256_bytes(copy["raw"]) for copy in copies}
            if len(raw_hashes) != 1:
                raise RuntimeError(f"{relative}: JSON differs byte-for-byte")
            projection_hashes = {
                semantic_hash(copy["value"]) for copy in copies
            }
            if len(projection_hashes) != 1:
                raise RuntimeError(f"{relative}: semantic projection differs")
            result_hash = next(iter(raw_hashes))
            projection_hash = next(iter(projection_hashes))
            value = copies[0]["value"]
            if (
                value.get("schema") != "forevervalidator-result-v1"
                or not isinstance(value.get("status"), str)
                or not isinstance(value.get("valid"), bool)
            ):
                raise RuntimeError(f"{relative}: malformed result JSON")
            json_kind = "valid" if value["valid"] else "invalid"
            if kind != json_kind:
                raise RuntimeError(
                    f"{relative}: log classification {kind} != JSON {json_kind}"
                )
            classification = f"{value.get('status')}:{value.get('valid')}"

        if ordinary_root is not None:
            if relative in config["expected_errors"]:
                prior = ordinary_blocks.get(identity, [])
                if len(prior) != 1 or prior[0]["raw"] != replay_blocks[0]["raw"]:
                    raise RuntimeError(
                        f"{relative}: differs from ordinary error evidence"
                    )
            else:
                prior = ordinary_results.get(identity, [])
                if len(prior) != 1 or prior[0]["raw"] != results[identity][0]["raw"]:
                    raise RuntimeError(
                        f"{relative}: differs from ordinary JSON evidence"
                    )

        records.append(
            {
                "corpus": corpus,
                "replay": relative,
                "classification": classification,
                "result_sha256": result_hash,
                "semantic_projection_sha256": projection_hash,
                "log_block_sha256": next(iter(raw_block_hashes)),
            }
        )

    expected_counts = Counter(
        {
            "valid": summary["valid"],
            "invalid": summary["invalid"],
            "error": summary["error"],
        }
    )
    if observed_counts != expected_counts:
        raise RuntimeError(
            f"{corpus}/{shard}: log counts {observed_counts} != "
            f"summary {expected_counts}"
        )
    extra_identities = set(results) - {str(path) for path in inputs}
    extra_blocks = set(blocks) - {str(path) for path in inputs}
    if extra_identities or extra_blocks:
        raise RuntimeError(f"{corpus}/{shard}: unexpected replay identities")
    return records


def aggregate_records(records: list[dict[str, Any]]) -> str:
    digest = hashlib.sha256()
    for record in sorted(records, key=lambda item: (item["corpus"], item["replay"])):
        digest.update(
            (
                f"{record['corpus']}\0{record['replay']}\0"
                f"{record['classification']}\0"
                f"{record['result_sha256'] or '-'}\0"
                f"{record['semantic_projection_sha256']}\0"
                f"{record['log_block_sha256']}\n"
            ).encode("utf-8")
        )
    return digest.hexdigest()


def analyze(
    root: Path,
    ordinary_united: Path,
    ordinary_stadium: Path,
) -> None:
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != "forevervalidator-reference-determinism-run-v1":
        raise RuntimeError("unexpected determinism-run manifest schema")
    if manifest.get("status") not in {"runs-complete", "audited"}:
        raise RuntimeError(
            f"determinism-run manifest is not complete: {manifest.get('status')}"
        )
    if sha256_file(Path(__file__).resolve()) != manifest.get("harness_sha256"):
        raise RuntimeError("harness bytes differ from the run manifest")
    repeats = int(manifest["emitted_repeats"])
    if repeats < 1:
        raise RuntimeError("run manifest has invalid emitted repeat count")
    ordinary_manifests = {
        "united": validate_ordinary_manifest(
            ordinary_united, "united", manifest
        ),
        "stadium": validate_ordinary_manifest(
            ordinary_stadium, "stadium", manifest
        ),
    }
    if ordinary_manifests != manifest.get("ordinary_evidence"):
        raise RuntimeError("ordinary evidence manifests changed during run")
    ordinary = {
        "united": ordinary_united,
        "stadium": ordinary_stadium,
    }
    records: list[dict[str, Any]] = []
    for corpus, config in CORPORA.items():
        for shard in config["shards"]:
            records.extend(
                audit_shard(
                    root,
                    corpus,
                    shard,
                    repeats,
                    ordinary[corpus],
                )
            )

    totals = Counter(record["corpus"] for record in records)
    for corpus, config in CORPORA.items():
        if totals[corpus] != config["expected_count"]:
            raise RuntimeError(f"{corpus}: audited {totals[corpus]} replays")

    tsv = root / "semantic-state-projections.tsv"
    with tsv.open("w", encoding="utf-8") as stream:
        stream.write(
            "corpus\treplay\tclassification\tresult_sha256\t"
            "semantic_projection_sha256\tlog_block_sha256\n"
        )
        for record in sorted(
            records, key=lambda item: (item["corpus"], item["replay"])
        ):
            stream.write(
                f"{record['corpus']}\t{record['replay']}\t"
                f"{record['classification']}\t"
                f"{record['result_sha256'] or '-'}\t"
                f"{record['semantic_projection_sha256']}\t"
                f"{record['log_block_sha256']}\n"
            )

    per_corpus = {}
    for corpus in CORPORA:
        corpus_records = [r for r in records if r["corpus"] == corpus]
        classifications = Counter(r["classification"] for r in corpus_records)
        per_corpus[corpus] = {
            "replays": len(corpus_records),
            "classifications": dict(sorted(classifications.items())),
            "evidence_aggregate_sha256": aggregate_records(corpus_records),
        }
    summary = {
        "schema": "forevervalidator-reference-determinism-evidence-v1",
        "validations_per_replay": repeats * 2,
        "externally_compared_copies_per_replay": repeats,
        "hash_scope": (
            "serialized result/error bytes plus canonical validation-report "
            "semantic projection; not the private full runtime clone"
        ),
        "ordinary_evidence_compared": ordinary_manifests,
        "corpora": per_corpus,
        "overall_evidence_aggregate_sha256": aggregate_records(records),
    }
    write_json(root / "summary.json", summary)
    print(json.dumps(summary, indent=2, sort_keys=True))


def run_shard(
    root: Path,
    binary: Path,
    paks: Path,
    corpus: str,
    shard: str,
    emitted_repeats: int,
) -> dict[str, Any]:
    shard_root = root / corpus / shard
    shard_root.mkdir(parents=True, exist_ok=False)
    results = shard_root / "results"
    results.mkdir()
    input_directory = CORPORA[corpus]["root"] / shard
    command = [
        str(binary),
        "--pak-dir",
        str(paks),
        "--backend",
        "reference",
        "--repeat-same-process",
        "--batch-size",
        str(emitted_repeats),
        "--out-dir",
        str(results),
        *([str(input_directory)] * emitted_repeats),
    ]
    write_json(shard_root / "command.json", command)
    started = time.monotonic()
    with (shard_root / "stdout.json").open("wb") as stdout, (
        shard_root / "stderr.log"
    ).open("wb") as stderr:
        process = subprocess.run(
            command,
            cwd=REPO,
            stdin=subprocess.DEVNULL,
            stdout=stdout,
            stderr=stderr,
            check=False,
        )
    elapsed = time.monotonic() - started
    (shard_root / "exit.txt").write_text(
        f"{process.returncode}\n", encoding="utf-8"
    )
    (shard_root / "elapsed.txt").write_text(
        f"wall_s={elapsed:.3f}\n", encoding="utf-8"
    )
    return {
        "corpus": corpus,
        "shard": shard,
        "exit": process.returncode,
        "wall_s": round(elapsed, 3),
        "command_sha256": sha256_file(shard_root / "command.json"),
        "stdout_sha256": sha256_file(shard_root / "stdout.json"),
        "stderr_sha256": sha256_file(shard_root / "stderr.log"),
    }


def run_gate(args: argparse.Namespace) -> None:
    require_clean_tracked_tree()
    active = active_validator_pids()
    if active:
        raise RuntimeError(f"forevervalidator already active: {active}")
    binary = absolute_path(args.binary)
    paks = absolute_path(args.paks)
    ordinary_united = absolute_path(args.ordinary_united)
    ordinary_stadium = absolute_path(args.ordinary_stadium)
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise RuntimeError(f"binary is not executable: {binary}")
    if not paks.is_dir():
        raise RuntimeError(f"pack directory missing: {paks}")
    if args.emitted_repeats < 1:
        raise RuntimeError("emitted repeats must be positive")
    if args.workers < 1:
        raise RuntimeError("workers must be positive")

    head = git_head()
    binary_hash = sha256_file(binary)
    authority = {
        "commit": head,
        "binary_sha256": binary_hash,
        "pak_dir": str(paks),
    }
    ordinary_manifests = {
        "united": validate_ordinary_manifest(
            ordinary_united, "united", authority
        ),
        "stadium": validate_ordinary_manifest(
            ordinary_stadium, "stadium", authority
        ),
    }
    root = absolute_path(args.root or Path(
        f"/tmp/reference-final-{head[:8]}-determinism-"
        f"{args.emitted_repeats * 2}x"
    ))
    if root.exists():
        raise RuntimeError(f"evidence root already exists: {root}")

    corpus_manifests, pack_manifest = capture_input_manifests(paks)
    root.mkdir(parents=True, exist_ok=False)
    manifest = {
        "schema": "forevervalidator-reference-determinism-run-v1",
        "status": "running",
        "commit": head,
        "git_status_porcelain": run_checked(
            ["git", "status", "--porcelain=v1"]
        ).splitlines(),
        "binary": str(binary),
        "binary_sha256": binary_hash,
        "harness": str(Path(__file__).resolve()),
        "harness_sha256": sha256_file(Path(__file__).resolve()),
        "pak_dir": str(paks),
        "pack_manifest": pack_manifest,
        "backend": "reference",
        "repeat_same_process": True,
        "emitted_repeats": args.emitted_repeats,
        "validations_per_replay": args.emitted_repeats * 2,
        "batch_size": args.emitted_repeats,
        "workers": args.workers,
        "corpora": corpus_manifests,
        "ordinary_evidence": ordinary_manifests,
    }
    write_json(root / "manifest.json", manifest)

    tasks = [
        (corpus, shard)
        for corpus, config in CORPORA.items()
        for shard in config["shards"]
    ]
    outcomes = []
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {
            pool.submit(
                run_shard,
                root,
                binary,
                paks,
                corpus,
                shard,
                args.emitted_repeats,
            ): (corpus, shard)
            for corpus, shard in tasks
        }
        for future in as_completed(futures):
            outcome = future.result()
            outcomes.append(outcome)
            print(
                f"{outcome['corpus']}/{outcome['shard']} "
                f"exit={outcome['exit']} wall_s={outcome['wall_s']}",
                flush=True,
            )

    require_clean_tracked_tree()
    postflight_corpora, postflight_packs = capture_input_manifests(paks)
    postflight = {
        "commit": git_head(),
        "binary_sha256": sha256_file(binary),
        "harness_sha256": sha256_file(Path(__file__).resolve()),
        "pack_manifest": postflight_packs,
        "corpora": postflight_corpora,
    }
    expected_postflight = {
        "commit": manifest["commit"],
        "binary_sha256": manifest["binary_sha256"],
        "harness_sha256": manifest["harness_sha256"],
        "pack_manifest": manifest["pack_manifest"],
        "corpora": manifest["corpora"],
    }
    if postflight != expected_postflight:
        manifest["status"] = "integrity-failed"
        manifest["postflight"] = postflight
        write_json(root / "manifest.json", manifest)
        raise RuntimeError("binary, harness, packs, corpus, or commit changed during run")

    manifest["status"] = "runs-complete"
    manifest["postflight"] = postflight
    manifest["outcomes"] = sorted(
        outcomes, key=lambda item: (item["corpus"], int(item["shard"]))
    )
    write_json(root / "manifest.json", manifest)
    analyze(root, ordinary_united, ordinary_stadium)
    manifest["status"] = "audited"
    manifest["summary_sha256"] = sha256_file(root / "summary.json")
    manifest["semantic_state_projections_sha256"] = sha256_file(
        root / "semantic-state-projections.tsv"
    )
    write_json(root / "manifest.json", manifest)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    subparsers = result.add_subparsers(dest="action", required=True)

    run = subparsers.add_parser("run")
    run.add_argument("--root", type=Path)
    run.add_argument("--binary", type=Path, default=DEFAULT_BINARY)
    run.add_argument("--paks", type=Path, default=DEFAULT_PAKS)
    run.add_argument("--workers", type=int, default=12)
    run.add_argument("--emitted-repeats", type=int, default=5)
    run.add_argument("--ordinary-united", type=Path, required=True)
    run.add_argument("--ordinary-stadium", type=Path, required=True)

    audit = subparsers.add_parser("analyze")
    audit.add_argument("root", type=Path)
    audit.add_argument("--ordinary-united", type=Path, required=True)
    audit.add_argument("--ordinary-stadium", type=Path, required=True)
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        if args.action == "run":
            run_gate(args)
        else:
            analyze(
                absolute_path(args.root),
                absolute_path(args.ordinary_united),
                absolute_path(args.ordinary_stadium),
            )
    except Exception as error:
        print(f"determinism gate: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
