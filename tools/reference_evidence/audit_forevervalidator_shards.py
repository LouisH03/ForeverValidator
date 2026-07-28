#!/usr/bin/env python3
"""Audit sharded ForeverValidator batch artifacts without running validation."""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


RESULT_SCHEMA = "forevervalidator-result-v1"
BATCH_SCHEMA = "forevervalidator-batch-v1"
REPORT_SCHEMA = "forevervalidator-artifact-audit-v1"
KNOWN_RESULT_STATUSES = {
    "valid",
    "valid_prefix",
    "wrong_simulation",
    "incomplete_validation_run",
    "race_completion_unavailable",
    "expecting_completed_race",
    "race_time_mismatch",
    "stunts_score_mismatch",
    "respawn_count_mismatch",
    "respawn_expectation_unavailable",
    "trajectory_observation_error",
    "incompatible_replay_version",
    "validation_input_unavailable",
    "scripted_replay",
}
STATUS_VALIDATE_RESULT_CODES: dict[str, int | None] = {
    "valid": 1,
    "valid_prefix": 1,
    "wrong_simulation": 2,
    "incomplete_validation_run": None,
    "race_completion_unavailable": None,
    "expecting_completed_race": 0,
    "race_time_mismatch": 0,
    "stunts_score_mismatch": 0,
    "respawn_count_mismatch": 0,
    "respawn_expectation_unavailable": None,
    "trajectory_observation_error": None,
    "incompatible_replay_version": 0,
    "validation_input_unavailable": None,
    "scripted_replay": 0,
}
VALIDATE_PREFIX = "validate: "
RESULT_RE = re.compile(
    r"^result: (?P<path>.*?) -> (?P<label>valid|invalid|error)"
    r"(?: \((?P<code>-?[0-9]+)\))?$"
)
DIAGNOSTIC_RE = re.compile(
    r"(?:^|\s)category=(?P<category>[^\s]+)"
    r"\s+reason=(?P<reason>[^\s]+)(?:\s|$)"
)
INTEGER_RE = re.compile(r"^[+-]?[0-9]+$")


class DuplicateJsonKey(ValueError):
    pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateJsonKey(f"duplicate JSON key: {key!r}")
        result[key] = value
    return result


def reject_nonfinite_json(value: str) -> Any:
    raise ValueError(f"non-finite JSON number: {value}")


def parse_json_bytes(data: bytes) -> Any:
    text = data.decode("utf-8")
    return json.loads(
        text,
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=reject_nonfinite_json,
    )


def canonical_json_bytes(value: Any) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def normalized_path(path: Path | str, base: Path | None = None) -> str:
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = (base or Path.cwd()) / candidate
    return os.path.realpath(os.path.abspath(os.fspath(candidate)))


def natural_key(text: str) -> tuple[Any, ...]:
    return tuple(
        int(part) if part.isdigit() else part.casefold()
        for part in re.split(r"([0-9]+)", text)
    )


def counter_dict(counter: collections.Counter[str]) -> dict[str, int]:
    return {key: counter[key] for key in sorted(counter, key=natural_key)}


def display_metadata_value(value: Any) -> str:
    if value is None:
        return "<null>"
    if isinstance(value, str):
        return value
    return f"<invalid:{type(value).__name__}>"


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


@dataclass
class LogResult:
    shard: str
    log_path: Path
    replay_raw: str
    replay_id: str
    replay_relative: str | None
    label: str
    code: int | None
    diagnostic: list[str] = field(default_factory=list)
    category: str | None = None
    reason: str | None = None

    @property
    def error_kind(self) -> str | None:
        if self.label != "error":
            return None
        if self.category == "replay":
            return "source_error"
        if self.category is None:
            return "unclassified_error"
        return "validator_error"


@dataclass
class JsonResult:
    shard: str
    path: Path
    output_relative: str
    raw: bytes
    value: dict[str, Any]
    canonical: bytes
    replay_raw: str
    replay_id: str
    replay_relative: str | None
    status: str
    valid: bool
    mode: str
    environment: str


@dataclass
class Shard:
    name: str
    root: Path
    log_path: Path
    result_dir: Path
    log_results: list[LogResult] = field(default_factory=list)
    json_results: list[JsonResult] = field(default_factory=list)
    summaries: list[dict[str, Any]] = field(default_factory=list)
    validate_counts: collections.Counter[str] = field(
        default_factory=collections.Counter
    )
    exit_code: int | None = None
    expected_exit_code: int | None = None


class Auditor:
    def __init__(
        self,
        corpus_root: Path,
        results_root: Path,
        replay_suffix: str,
        require_errors_source_only: bool,
        fail_on_any_error: bool,
    ) -> None:
        self.corpus_root = corpus_root.resolve()
        self.results_root = results_root.resolve()
        self.replay_suffix = replay_suffix.casefold()
        self.require_errors_source_only = require_errors_source_only
        self.fail_on_any_error = fail_on_any_error
        self.issues: list[dict[str, Any]] = []
        self.warnings: list[dict[str, Any]] = []
        self.corpus_by_id: dict[str, str] = {}
        self.corpus_paths: dict[str, Path] = {}
        self.shards: dict[str, Shard] = {}
        self.json_results: list[JsonResult] = []
        self.log_results: list[LogResult] = []
        self.invalid_json_files: list[dict[str, Any]] = []

    def issue(self, code: str, message: str, **context: Any) -> None:
        self.issues.append({"code": code, "message": message, **context})

    def warn(self, code: str, message: str, **context: Any) -> None:
        self.warnings.append({"code": code, "message": message, **context})

    def source_identity(self, raw: str) -> tuple[str, str | None]:
        raw_path = Path(raw)
        candidates: list[str] = []
        if raw_path.is_absolute():
            candidates.append(normalized_path(raw_path))
        else:
            for base in (
                self.corpus_root,
                self.corpus_root.parent,
                Path.cwd(),
            ):
                identity = normalized_path(raw_path, base)
                if identity not in candidates:
                    candidates.append(identity)

        matched = [identity for identity in candidates if identity in self.corpus_by_id]
        if len(matched) == 1:
            identity = matched[0]
            return identity, self.corpus_by_id[identity]
        if len(matched) > 1:
            return matched[0], self.corpus_by_id[matched[0]]

        raw_posix = raw_path.as_posix()
        suffix_matches = [
            (identity, relative)
            for identity, relative in self.corpus_by_id.items()
            if relative == raw_posix or relative.endswith("/" + raw_posix)
        ]
        if len(suffix_matches) == 1:
            return suffix_matches[0]
        identity = candidates[0] if candidates else normalized_path(raw)
        return identity, self.corpus_by_id.get(identity)

    def discover_corpus(self) -> None:
        if not self.corpus_root.is_dir():
            self.issue(
                "corpus_root_not_directory",
                "Corpus root does not exist or is not a directory",
                path=str(self.corpus_root),
            )
            return

        paths = sorted(
            (
                path
                for path in self.corpus_root.rglob("*")
                if path.is_file()
                and path.name.casefold().endswith(self.replay_suffix)
            ),
            key=lambda path: natural_key(path.relative_to(self.corpus_root).as_posix()),
        )
        if not paths:
            self.issue(
                "empty_corpus",
                "No replay files matched the configured suffix",
                path=str(self.corpus_root),
                suffix=self.replay_suffix,
            )
        for path in paths:
            identity = normalized_path(path)
            relative = path.relative_to(self.corpus_root).as_posix()
            if identity in self.corpus_by_id:
                self.issue(
                    "duplicate_corpus_identity",
                    "Multiple corpus paths resolve to the same replay",
                    replay=relative,
                    other_replay=self.corpus_by_id[identity],
                )
                continue
            self.corpus_by_id[identity] = relative
            self.corpus_paths[relative] = path

    def discover_shards(self) -> None:
        if not self.results_root.is_dir():
            self.issue(
                "results_root_not_directory",
                "Results root does not exist or is not a directory",
                path=str(self.results_root),
            )
            return

        logs = sorted(
            self.results_root.rglob("run.log"),
            key=lambda path: natural_key(path.relative_to(self.results_root).as_posix()),
        )
        if not logs:
            self.issue(
                "missing_run_logs",
                "No run.log files were found beneath the results root",
                path=str(self.results_root),
            )
            return

        for log_path in logs:
            shard_root = log_path.parent
            relative_root = shard_root.relative_to(self.results_root).as_posix()
            name = relative_root if relative_root != "." else "."
            if name in self.shards:
                self.issue(
                    "duplicate_shard",
                    "Multiple run logs identify the same shard",
                    shard=name,
                    run_log=str(log_path),
                )
                continue
            self.shards[name] = Shard(
                name=name,
                root=shard_root,
                log_path=log_path,
                result_dir=shard_root / "results",
            )

        owned_json_paths: set[Path] = set()
        for shard in self.shards.values():
            if not shard.result_dir.is_dir():
                self.warn(
                    "missing_result_directory",
                    "Shard has no results directory; this is valid only for an all-error shard",
                    shard=shard.name,
                    path=str(shard.result_dir),
                )
                continue
            for path in shard.result_dir.rglob("*.json"):
                resolved = path.resolve()
                if resolved in owned_json_paths:
                    self.issue(
                        "json_owned_by_multiple_shards",
                        "A result JSON is nested beneath multiple shard result roots",
                        path=str(path),
                    )
                owned_json_paths.add(resolved)

        result_tree_json = {
            path.resolve()
            for path in self.results_root.rglob("*.json")
            if "results" in path.relative_to(self.results_root).parts
        }
        for path in sorted(result_tree_json - owned_json_paths):
            self.issue(
                "unowned_result_json",
                "JSON beneath a results directory is not owned by a shard run log",
                path=str(path),
            )

    def parse_log(self, shard: Shard) -> None:
        try:
            lines = shard.log_path.read_text(encoding="utf-8", errors="strict").splitlines()
        except (OSError, UnicodeError) as error:
            self.issue(
                "run_log_unreadable",
                "Could not read run log as UTF-8",
                shard=shard.name,
                path=str(shard.log_path),
                error=str(error),
            )
            return

        current_raw: str | None = None
        current_id: str | None = None
        current_diagnostic: list[str] = []
        for line_number, line in enumerate(lines, 1):
            if line.startswith(VALIDATE_PREFIX):
                raw = line[len(VALIDATE_PREFIX) :]
                identity, _ = self.source_identity(raw)
                shard.validate_counts[identity] += 1
                previous_id = current_id
                if current_id is not None and current_id != identity:
                    self.warn(
                        "validate_without_result",
                        "A validate line was followed by another replay before its result line",
                        shard=shard.name,
                        line=line_number,
                        replay=current_raw,
                    )
                current_raw = raw
                current_id = identity
                if previous_id != identity:
                    current_diagnostic = []
                continue

            match = RESULT_RE.match(line)
            if match:
                raw = match.group("path")
                identity, relative = self.source_identity(raw)
                code_text = match.group("code")
                code = int(code_text) if code_text is not None else None
                label = match.group("label")
                diagnostic = (
                    list(current_diagnostic)
                    if current_id == identity
                    else []
                )
                category: str | None = None
                reason: str | None = None
                for diagnostic_line in diagnostic:
                    diagnostic_match = DIAGNOSTIC_RE.search(diagnostic_line)
                    if diagnostic_match:
                        category = diagnostic_match.group("category")
                        reason = diagnostic_match.group("reason")
                if current_id != identity:
                    self.warn(
                        "result_without_matching_validate",
                        "Result line did not match the current validate line",
                        shard=shard.name,
                        line=line_number,
                        replay=raw,
                    )
                if label == "error" and code is None:
                    self.issue(
                        "error_without_code",
                        "Logged error result has no numeric exit code",
                        shard=shard.name,
                        line=line_number,
                        replay=relative or raw,
                    )
                if label != "error" and code is not None:
                    self.issue(
                        "non_error_with_code",
                        "Logged valid/invalid result unexpectedly has an exit code",
                        shard=shard.name,
                        line=line_number,
                        replay=relative or raw,
                    )
                result = LogResult(
                    shard=shard.name,
                    log_path=shard.log_path,
                    replay_raw=raw,
                    replay_id=identity,
                    replay_relative=relative,
                    label=label,
                    code=code,
                    diagnostic=diagnostic,
                    category=category,
                    reason=reason,
                )
                shard.log_results.append(result)
                self.log_results.append(result)
                current_raw = None
                current_id = None
                current_diagnostic = []
                continue

            if line.startswith("{") and BATCH_SCHEMA in line:
                try:
                    summary = json.loads(
                        line,
                        object_pairs_hook=reject_duplicate_keys,
                        parse_constant=reject_nonfinite_json,
                    )
                except (ValueError, json.JSONDecodeError) as error:
                    self.issue(
                        "batch_summary_invalid_json",
                        "Batch summary line is not strict JSON",
                        shard=shard.name,
                        line=line_number,
                        error=str(error),
                    )
                    continue
                if not isinstance(summary, dict) or summary.get("schema") != BATCH_SCHEMA:
                    self.issue(
                        "batch_summary_invalid_schema",
                        "Batch summary has an unexpected schema",
                        shard=shard.name,
                        line=line_number,
                    )
                    continue
                shard.summaries.append(summary)
                continue

            if current_id is not None and line:
                current_diagnostic.append(line)

        if current_id is not None:
            self.warn(
                "validate_at_end_without_result",
                "Run log ends with a validate line that has no result",
                shard=shard.name,
                replay=current_raw,
            )

        if len(shard.summaries) != 1:
            self.issue(
                "batch_summary_count",
                "Each shard must contain exactly one batch summary",
                shard=shard.name,
                found=len(shard.summaries),
            )

        observed = collections.Counter(result.label for result in shard.log_results)
        observed_counts = {
            "total": len(shard.log_results),
            "valid": observed["valid"],
            "invalid": observed["invalid"],
            "error": observed["error"],
        }
        if len(shard.summaries) == 1:
            summary = shard.summaries[0]
            for key, value in observed_counts.items():
                if type(summary.get(key)) is not int or summary.get(key) != value:
                    self.issue(
                        "batch_summary_mismatch",
                        "Batch summary does not match parsed result lines",
                        shard=shard.name,
                        field=key,
                        declared=summary.get(key),
                        observed=value,
                    )
            unexpected_keys = sorted(
                set(summary) - {"schema", "total", "valid", "invalid", "error"}
            )
            if unexpected_keys:
                self.warn(
                    "batch_summary_extra_fields",
                    "Batch summary contains additional fields",
                    shard=shard.name,
                    fields=unexpected_keys,
                )

        expected_exit = (
            2
            if observed["error"]
            else 1
            if observed["invalid"]
            else 0
        )
        shard.expected_exit_code = expected_exit
        exit_path = shard.root / "exit.txt"
        if exit_path.exists():
            try:
                exit_text = exit_path.read_text(encoding="ascii").strip()
            except (OSError, UnicodeError) as error:
                self.issue(
                    "exit_file_unreadable",
                    "Could not read shard exit.txt",
                    shard=shard.name,
                    path=str(exit_path),
                    error=str(error),
                )
            else:
                if not INTEGER_RE.fullmatch(exit_text):
                    self.issue(
                        "exit_file_invalid",
                        "Shard exit.txt is not one integer",
                        shard=shard.name,
                        value=exit_text,
                    )
                else:
                    shard.exit_code = int(exit_text)
                    if shard.exit_code != expected_exit:
                        self.issue(
                            "exit_code_mismatch",
                            "Shard exit code does not match its batch outcomes",
                            shard=shard.name,
                            declared=shard.exit_code,
                            expected=expected_exit,
                        )
        else:
            self.warn(
                "missing_exit_file",
                "Shard has no exit.txt; batch summary was still checked",
                shard=shard.name,
                path=str(exit_path),
            )

    def parse_json_result(self, shard: Shard, path: Path) -> None:
        output_relative = path.relative_to(self.results_root).as_posix()
        try:
            raw = path.read_bytes()
        except OSError as error:
            self.issue(
                "result_json_unreadable",
                "Could not read result JSON",
                shard=shard.name,
                path=output_relative,
                error=str(error),
            )
            self.invalid_json_files.append(
                {"shard": shard.name, "path": output_relative, "error": str(error)}
            )
            return
        try:
            value = parse_json_bytes(raw)
        except (UnicodeError, ValueError, json.JSONDecodeError) as error:
            self.issue(
                "result_json_invalid",
                "Result file is not strict UTF-8 JSON",
                shard=shard.name,
                path=output_relative,
                error=str(error),
            )
            self.invalid_json_files.append(
                {"shard": shard.name, "path": output_relative, "error": str(error)}
            )
            return
        if not isinstance(value, dict):
            self.issue(
                "result_json_not_object",
                "Result JSON root must be an object",
                shard=shard.name,
                path=output_relative,
            )
            self.invalid_json_files.append(
                {
                    "shard": shard.name,
                    "path": output_relative,
                    "error": "JSON root is not an object",
                }
            )
            return

        schema = value.get("schema")
        replay_raw = value.get("replay")
        status = value.get("status")
        valid = value.get("valid")
        structurally_valid = True
        if schema != RESULT_SCHEMA:
            self.issue(
                "result_schema_mismatch",
                "Result JSON has an unexpected schema",
                shard=shard.name,
                path=output_relative,
                schema=schema,
            )
            structurally_valid = False
        if not isinstance(replay_raw, str) or not replay_raw:
            self.issue(
                "result_replay_invalid",
                "Result JSON replay field must be a non-empty string",
                shard=shard.name,
                path=output_relative,
            )
            structurally_valid = False
        if not isinstance(status, str) or not status:
            self.issue(
                "result_status_invalid",
                "Result JSON status field must be a non-empty string",
                shard=shard.name,
                path=output_relative,
            )
            structurally_valid = False
        elif status not in KNOWN_RESULT_STATUSES:
            self.warn(
                "result_status_unknown",
                "Result JSON uses a status unknown to this audit script",
                shard=shard.name,
                path=output_relative,
                status=status,
            )
        if type(valid) is not bool:
            self.issue(
                "result_valid_invalid",
                "Result JSON valid field must be a boolean",
                shard=shard.name,
                path=output_relative,
            )
            structurally_valid = False
        validate_result_code = value.get("validate_result_code")
        if "validate_result_code" not in value:
            self.issue(
                "result_code_missing",
                "Result JSON is missing validate_result_code",
                shard=shard.name,
                path=output_relative,
            )
        elif validate_result_code is not None and type(validate_result_code) is not int:
            self.issue(
                "result_code_invalid",
                "Result JSON validate_result_code must be an integer or null",
                shard=shard.name,
                path=output_relative,
                value=validate_result_code,
            )
        elif isinstance(status, str) and status in STATUS_VALIDATE_RESULT_CODES:
            expected_result_code = STATUS_VALIDATE_RESULT_CODES[status]
            if validate_result_code != expected_result_code:
                self.issue(
                    "result_code_mismatch",
                    "Result JSON validate_result_code disagrees with status",
                    shard=shard.name,
                    path=output_relative,
                    status=status,
                    declared=validate_result_code,
                    expected=expected_result_code,
                )
        if not structurally_valid:
            self.invalid_json_files.append(
                {
                    "shard": shard.name,
                    "path": output_relative,
                    "error": "missing or invalid required result fields",
                }
            )
            return

        metadata = value.get("replay_file_metadata")
        if not isinstance(metadata, dict):
            self.issue(
                "result_metadata_invalid",
                "Result JSON replay_file_metadata must be an object",
                shard=shard.name,
                path=output_relative,
            )
            metadata = {}
        mode = display_metadata_value(metadata.get("play_mode"))
        environment = display_metadata_value(metadata.get("map_environment"))
        identity, relative = self.source_identity(replay_raw)
        try:
            canonical = canonical_json_bytes(value)
        except (TypeError, ValueError) as error:
            self.issue(
                "result_json_not_canonicalizable",
                "Result JSON could not be canonicalized",
                shard=shard.name,
                path=output_relative,
                error=str(error),
            )
            self.invalid_json_files.append(
                {"shard": shard.name, "path": output_relative, "error": str(error)}
            )
            return

        result = JsonResult(
            shard=shard.name,
            path=path,
            output_relative=output_relative,
            raw=raw,
            value=value,
            canonical=canonical,
            replay_raw=replay_raw,
            replay_id=identity,
            replay_relative=relative,
            status=status,
            valid=valid,
            mode=mode,
            environment=environment,
        )
        shard.json_results.append(result)
        self.json_results.append(result)

    def parse_artifacts(self) -> None:
        for shard in sorted(
            self.shards.values(), key=lambda candidate: natural_key(candidate.name)
        ):
            self.parse_log(shard)
            if shard.result_dir.is_dir():
                for path in sorted(
                    shard.result_dir.rglob("*.json"),
                    key=lambda candidate: natural_key(
                        candidate.relative_to(shard.result_dir).as_posix()
                    ),
                ):
                    self.parse_json_result(shard, path)

    def verify_coverage(self) -> None:
        json_by_id: dict[str, list[JsonResult]] = collections.defaultdict(list)
        logs_by_id: dict[str, list[LogResult]] = collections.defaultdict(list)
        for result in self.json_results:
            json_by_id[result.replay_id].append(result)
            if result.replay_relative is None:
                self.issue(
                    "json_replay_outside_corpus",
                    "Result JSON identifies a replay outside the corpus",
                    shard=result.shard,
                    path=result.output_relative,
                    replay=result.replay_raw,
                )
        for result in self.log_results:
            logs_by_id[result.replay_id].append(result)
            if result.replay_relative is None:
                self.issue(
                    "logged_replay_outside_corpus",
                    "Run log identifies a replay outside the corpus",
                    shard=result.shard,
                    replay=result.replay_raw,
                )

        for identity, relative in sorted(
            self.corpus_by_id.items(), key=lambda item: natural_key(item[1])
        ):
            json_records = json_by_id.get(identity, [])
            log_records = logs_by_id.get(identity, [])
            error_records = [
                record for record in log_records if record.label == "error"
            ]
            non_error_records = [
                record for record in log_records if record.label != "error"
            ]
            if len(log_records) != 1:
                self.issue(
                    "log_result_coverage",
                    "Corpus replay must have exactly one logged result",
                    replay=relative,
                    found=len(log_records),
                )
            if len(json_records) + len(error_records) != 1:
                self.issue(
                    "output_coverage",
                    "Corpus replay must have exactly one JSON or one logged error",
                    replay=relative,
                    json_results=len(json_records),
                    logged_errors=len(error_records),
                )
            if len(json_records) != len(non_error_records):
                self.issue(
                    "json_log_pairing",
                    "JSON result count does not match logged valid/invalid count",
                    replay=relative,
                    json_results=len(json_records),
                    logged_non_errors=len(non_error_records),
                )
            if len(json_records) == 1 and len(non_error_records) == 1:
                json_result = json_records[0]
                log_result = non_error_records[0]
                expected_label = "valid" if json_result.valid else "invalid"
                if log_result.label != expected_label:
                    self.issue(
                        "json_log_classification_mismatch",
                        "JSON valid flag disagrees with its logged classification",
                        replay=relative,
                        json_valid=json_result.valid,
                        logged=log_result.label,
                    )
                if json_result.shard != log_result.shard:
                    self.issue(
                        "json_log_shard_mismatch",
                        "JSON result and log result belong to different shards",
                        replay=relative,
                        json_shard=json_result.shard,
                        log_shard=log_result.shard,
                    )

        for identity, records in json_by_id.items():
            if identity not in self.corpus_by_id and len(records) > 1:
                self.issue(
                    "duplicate_external_json_replay",
                    "Multiple result JSON files identify the same external replay",
                    replay=records[0].replay_raw,
                    found=len(records),
                )
        for identity, records in logs_by_id.items():
            if identity not in self.corpus_by_id and len(records) > 1:
                self.issue(
                    "duplicate_external_log_replay",
                    "Multiple log results identify the same external replay",
                    replay=records[0].replay_raw,
                    found=len(records),
                )

        for shard in self.shards.values():
            logged_valid = sum(
                result.label == "valid" for result in shard.log_results
            )
            logged_invalid = sum(
                result.label == "invalid" for result in shard.log_results
            )
            json_valid = sum(result.valid for result in shard.json_results)
            json_invalid = sum(not result.valid for result in shard.json_results)
            if (logged_valid, logged_invalid) != (json_valid, json_invalid):
                self.issue(
                    "shard_json_counts_mismatch",
                    "Shard JSON valid/invalid counts disagree with its run log",
                    shard=shard.name,
                    logged_valid=logged_valid,
                    logged_invalid=logged_invalid,
                    json_valid=json_valid,
                    json_invalid=json_invalid,
                )

        for shard in self.shards.values():
            for identity, count in shard.validate_counts.items():
                if count > 1:
                    relative = self.corpus_by_id.get(identity, identity)
                    self.warn(
                        "duplicate_validate_lines",
                        "Replay appears in more than one validate line in a shard",
                        shard=shard.name,
                        replay=relative,
                        count=count,
                    )

    def manifest_key(self, result: JsonResult) -> str:
        if result.replay_relative is not None:
            return result.replay_relative
        return "@external/" + result.replay_raw

    def build_manifest_lines(
        self,
    ) -> tuple[bytes, bytes, bytes, bytes]:
        key_counts = collections.Counter(
            self.manifest_key(result) for result in self.json_results
        )
        exact_entries: list[tuple[str, str]] = []
        canonical_entries: list[tuple[str, str]] = []
        classification_entries: list[tuple[str, bytes]] = []
        coverage_entries: list[tuple[str, bytes]] = []
        for result in self.json_results:
            base_key = self.manifest_key(result)
            key = (
                base_key
                if key_counts[base_key] == 1
                else f"{base_key}#output={result.output_relative}"
            )
            exact_entries.append((key, sha256(result.raw)))
            canonical_digest = sha256(result.canonical)
            canonical_entries.append((key, canonical_digest))
            classification = {
                "kind": "result",
                "replay": key,
                "status": result.status,
                "valid": result.valid,
            }
            coverage = {
                **classification,
                "canonical_result_sha256": canonical_digest,
            }
            classification_entries.append((key, canonical_json_bytes(classification)))
            coverage_entries.append((key, canonical_json_bytes(coverage)))

        for result in self.log_results:
            if result.label != "error":
                continue
            base_key = (
                result.replay_relative
                if result.replay_relative is not None
                else "@external/" + result.replay_raw
            )
            coverage = {
                "kind": result.error_kind,
                "replay": base_key,
                "code": result.code,
                "category": result.category,
                "reason": result.reason,
            }
            classification = {
                "kind": result.error_kind,
                "replay": base_key,
                "code": result.code,
            }
            classification_entries.append(
                (base_key, canonical_json_bytes(classification))
            )
            coverage_entries.append((base_key, canonical_json_bytes(coverage)))

        exact = "".join(
            f"{digest}\t{json.dumps(key, ensure_ascii=True)}\n"
            for key, digest in sorted(exact_entries, key=lambda item: natural_key(item[0]))
        ).encode("utf-8")
        canonical = "".join(
            f"{digest}\t{json.dumps(key, ensure_ascii=True)}\n"
            for key, digest in sorted(
                canonical_entries, key=lambda item: natural_key(item[0])
            )
        ).encode("utf-8")
        classifications = b"".join(
            line + b"\n"
            for _, line in sorted(
                classification_entries, key=lambda item: natural_key(item[0])
            )
        )
        coverage = b"".join(
            line + b"\n"
            for _, line in sorted(
                coverage_entries, key=lambda item: natural_key(item[0])
            )
        )
        return exact, canonical, classifications, coverage

    def non_valid_details(self) -> list[dict[str, Any]]:
        details: list[dict[str, Any]] = []
        for result in self.json_results:
            if result.valid:
                continue
            metadata = result.value.get("replay_file_metadata")
            if not isinstance(metadata, dict):
                metadata = {}
            details.append(
                {
                    "replay": result.replay_relative or result.replay_raw,
                    "shard": result.shard,
                    "result_file": result.output_relative,
                    "status": result.status,
                    "valid": result.valid,
                    "validate_result_code": result.value.get(
                        "validate_result_code"
                    ),
                    "message": result.value.get("message"),
                    "play_mode": result.mode,
                    "map_environment": result.environment,
                    "expected_race_time_ms": metadata.get(
                        "expected_race_time_ms"
                    ),
                    "expected_respawns": metadata.get("expected_respawns"),
                    "expected_stunts_score": metadata.get(
                        "expected_stunts_score"
                    ),
                    "simulation_outcome": result.value.get(
                        "simulation_outcome"
                    ),
                }
            )
        return sorted(details, key=lambda item: natural_key(item["replay"]))

    def error_details(self) -> list[dict[str, Any]]:
        return sorted(
            (
                {
                    "replay": result.replay_relative or result.replay_raw,
                    "shard": result.shard,
                    "kind": result.error_kind,
                    "exit_code": result.code,
                    "category": result.category,
                    "reason": result.reason,
                    "diagnostic": result.diagnostic,
                }
                for result in self.log_results
                if result.label == "error"
            ),
            key=lambda item: natural_key(item["replay"]),
        )

    def shard_reports(self) -> list[dict[str, Any]]:
        reports: list[dict[str, Any]] = []
        for shard in sorted(
            self.shards.values(), key=lambda candidate: natural_key(candidate.name)
        ):
            labels = collections.Counter(result.label for result in shard.log_results)
            statuses = collections.Counter(
                result.status for result in shard.json_results
            )
            reports.append(
                {
                    "shard": shard.name,
                    "run_log": str(shard.log_path),
                    "result_directory": str(shard.result_dir),
                    "batch_summary": (
                        shard.summaries[0] if len(shard.summaries) == 1 else None
                    ),
                    "observed_log_counts": {
                        "total": len(shard.log_results),
                        "valid": labels["valid"],
                        "invalid": labels["invalid"],
                        "error": labels["error"],
                    },
                    "json_result_count": len(shard.json_results),
                    "json_statuses": counter_dict(statuses),
                    "exit_code": shard.exit_code,
                    "expected_exit_code": shard.expected_exit_code,
                }
            )
        return reports

    def report(self) -> tuple[dict[str, Any], tuple[bytes, bytes, bytes, bytes]]:
        exact, canonical, classifications, coverage = self.build_manifest_lines()
        labels = collections.Counter(result.label for result in self.log_results)
        statuses = collections.Counter(result.status for result in self.json_results)
        modes = collections.Counter(result.mode for result in self.json_results)
        environments = collections.Counter(
            result.environment for result in self.json_results
        )
        errors = [result for result in self.log_results if result.label == "error"]
        error_kinds = collections.Counter(
            result.error_kind or "<none>" for result in errors
        )
        represented_ids = {
            result.replay_id for result in self.json_results
        } | {
            result.replay_id for result in errors
        }
        represented_corpus_count = sum(
            identity in represented_ids for identity in self.corpus_by_id
        )

        policy_violations: list[dict[str, Any]] = []
        if self.require_errors_source_only:
            non_source = [
                result
                for result in errors
                if result.error_kind != "source_error"
            ]
            if non_source:
                policy_violations.append(
                    {
                        "code": "non_source_errors_present",
                        "count": len(non_source),
                        "replays": [
                            result.replay_relative or result.replay_raw
                            for result in non_source
                        ],
                    }
                )
        if self.fail_on_any_error and errors:
            policy_violations.append(
                {
                    "code": "logged_errors_present",
                    "count": len(errors),
                }
            )

        integrity_ok = not self.issues
        policy_ok = not policy_violations
        report = {
            "schema": REPORT_SCHEMA,
            "corpus_root": str(self.corpus_root),
            "results_root": str(self.results_root),
            "replay_suffix_casefolded": self.replay_suffix,
            "integrity": {
                "ok": integrity_ok,
                "issue_count": len(self.issues),
                "warning_count": len(self.warnings),
                "issues": self.issues,
                "warnings": self.warnings,
            },
            "policy": {
                "ok": policy_ok,
                "require_errors_source_only": self.require_errors_source_only,
                "fail_on_any_error": self.fail_on_any_error,
                "violations": policy_violations,
            },
            "aggregate": {
                "corpus_replays": len(self.corpus_by_id),
                "represented_corpus_replays": represented_corpus_count,
                "shards": len(self.shards),
                "logged_results": len(self.log_results),
                "json_results": len(self.json_results),
                "invalid_json_files": len(self.invalid_json_files),
                "valid": labels["valid"],
                "invalid": labels["invalid"],
                "error": labels["error"],
                "error_kinds": counter_dict(error_kinds),
                "statuses": counter_dict(statuses),
                "play_modes": counter_dict(modes),
                "map_environments": counter_dict(environments),
            },
            "non_valids": self.non_valid_details(),
            "errors": self.error_details(),
            "invalid_json_files": self.invalid_json_files,
            "shards": self.shard_reports(),
            "manifests": {
                "format": (
                    "exact/canonical lines are SHA256<TAB>JSON-quoted replay"
                    "<LF>; classification/coverage are canonical JSON Lines"
                ),
                "exact_result_bytes": {
                    "entries": len(self.json_results),
                    "result_bytes": sum(
                        len(result.raw) for result in self.json_results
                    ),
                    "manifest_bytes": len(exact),
                    "sha256": sha256(exact),
                },
                "canonical_result_json": {
                    "entries": len(self.json_results),
                    "canonical_json_bytes": sum(
                        len(result.canonical) for result in self.json_results
                    ),
                    "manifest_bytes": len(canonical),
                    "sha256": sha256(canonical),
                },
                "classifications": {
                    "entries": len(self.json_results) + len(errors),
                    "manifest_bytes": len(classifications),
                    "sha256": sha256(classifications),
                },
                "coverage_outcomes": {
                    "entries": len(self.json_results) + len(errors),
                    "manifest_bytes": len(coverage),
                    "sha256": sha256(coverage),
                },
            },
        }
        return report, (exact, canonical, classifications, coverage)

    def run(self) -> tuple[dict[str, Any], tuple[bytes, bytes, bytes, bytes]]:
        self.discover_corpus()
        self.discover_shards()
        self.parse_artifacts()
        self.verify_coverage()
        return self.report()


def write_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(data)
    os.replace(temporary, path)


def write_report(path: Path, report: dict[str, Any], indent: int | None) -> None:
    data = (
        json.dumps(
            report,
            ensure_ascii=False,
            allow_nan=False,
            sort_keys=True,
            indent=indent,
        )
        + "\n"
    ).encode("utf-8")
    write_bytes(path, data)


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Audit one-to-one coverage, JSON classifications, run-log batch "
            "summaries, and deterministic manifests for sharded "
            "ForeverValidator artifacts."
        )
    )
    parser.add_argument("corpus_root", type=Path)
    parser.add_argument("results_root", type=Path)
    parser.add_argument(
        "--replay-suffix",
        default=".replay.gbx",
        help="case-insensitive replay filename suffix (default: .replay.gbx)",
    )
    parser.add_argument(
        "--report",
        type=Path,
        help="also atomically write the JSON report to this path",
    )
    parser.add_argument(
        "--manifest-dir",
        type=Path,
        help="write the four audited manifest payloads to this directory",
    )
    parser.add_argument(
        "--compact",
        action="store_true",
        help="emit compact JSON instead of the default indented report",
    )
    parser.add_argument(
        "--require-errors-source-only",
        action="store_true",
        help=(
            "policy failure if a logged error is not a replay-category "
            "source error"
        ),
    )
    parser.add_argument(
        "--fail-on-any-error",
        action="store_true",
        help="policy failure if any logged error is present",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_arguments(argv)
    auditor = Auditor(
        corpus_root=args.corpus_root,
        results_root=args.results_root,
        replay_suffix=args.replay_suffix,
        require_errors_source_only=args.require_errors_source_only,
        fail_on_any_error=args.fail_on_any_error,
    )
    report, manifests = auditor.run()
    indent = None if args.compact else 2

    if args.manifest_dir is not None:
        names = (
            "exact-results.sha256",
            "canonical-results.sha256",
            "classifications.jsonl",
            "coverage-outcomes.jsonl",
        )
        for name, payload in zip(names, manifests):
            write_bytes(args.manifest_dir / name, payload)
        report["manifest_directory"] = str(args.manifest_dir.resolve())

    if args.report is not None:
        write_report(args.report, report, indent)

    print(
        json.dumps(
            report,
            ensure_ascii=False,
            allow_nan=False,
            sort_keys=True,
            indent=indent,
        )
    )
    if not report["integrity"]["ok"]:
        return 2
    if not report["policy"]["ok"]:
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
