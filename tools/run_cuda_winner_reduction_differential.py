#!/usr/bin/env python3

import argparse
import json
import subprocess
import sys


CONTRACT_FIELDS = (
    "candidates",
    "evaluated_candidates",
    "timeline_ticks",
    "branch_time_ms",
    "modifier",
    "evaluator",
    "cancelled",
    "best_changed",
    "best_is_mutation",
    "best_candidate_id",
    "best_evaluation_tick",
    "best_score",
    "best_time_ms",
    "best_detail0",
    "best_detail1",
    "best_mutation_count",
    "best_state_fingerprint",
    "best_input_count",
    "best_input_fingerprint",
)

CASES = (
    ("short", 64, 32, "random-steering", "velocity"),
    ("long", 128, 1000, "random-steering", "velocity"),
    ("point", 16, 32, "random-steering", "point"),
    ("pose", 16, 32, "random-steering", "pose"),
    ("volume-entry", 16, 32, "random-steering", "volume-entry"),
    ("finish-time", 16, 32, "random-steering", "finish-time"),
    ("input-insertion", 64, 128, "input-insertion", "velocity"),
    ("cancelled", 64, 128, "cancelled", "velocity"),
)


def run(binary, packs, replay, candidates, ticks, modifier, evaluator):
    command = [
        binary,
        packs,
        replay,
        str(candidates),
        str(ticks),
        "1",
        "5000",
        modifier,
        evaluator,
    ]
    completed = subprocess.run(
        command,
        check=True,
        capture_output=True,
        text=True,
        timeout=900,
    )
    lines = [line for line in completed.stdout.splitlines() if line]
    if len(lines) != 1:
        raise RuntimeError(
            f"{binary} emitted {len(lines)} result lines, expected one"
        )
    return json.loads(lines[0])


def contract(result):
    return {field: result[field] for field in CONTRACT_FIELDS}


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Compare the legacy O(NT) CUDA winner implementation with "
            "the O(N) implementation."
        )
    )
    parser.add_argument("baseline_binary")
    parser.add_argument("current_binary")
    parser.add_argument("packs")
    parser.add_argument("replay")
    args = parser.parse_args()

    report = []
    for name, candidates, ticks, modifier, evaluator in CASES:
        baseline = run(
            args.baseline_binary,
            args.packs,
            args.replay,
            candidates,
            ticks,
            modifier,
            evaluator,
        )
        current = run(
            args.current_binary,
            args.packs,
            args.replay,
            candidates,
            ticks,
            modifier,
            evaluator,
        )
        repeated = run(
            args.current_binary,
            args.packs,
            args.replay,
            candidates,
            ticks,
            modifier,
            evaluator,
        )
        expected = contract(baseline)
        actual = contract(current)
        if actual != expected:
            raise RuntimeError(
                f"{name}: legacy/current contract mismatch\n"
                f"legacy={json.dumps(expected, sort_keys=True)}\n"
                f"current={json.dumps(actual, sort_keys=True)}"
            )
        if contract(repeated) != actual:
            raise RuntimeError(
                f"{name}: current result was not deterministic"
            )
        report.append(
            {
                "case": name,
                "winner_contract": actual,
                "legacy_resident_device_bytes": baseline[
                    "resident_device_bytes"
                ],
                "current_resident_device_bytes": current[
                    "resident_device_bytes"
                ],
                "current_winner_selection_device_bytes": current[
                    "winner_selection_device_bytes"
                ],
            }
        )

    print(json.dumps({"status": "passed", "cases": report}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.SubprocessError, RuntimeError, KeyError) as error:
        print(f"cuda winner differential failed: {error}", file=sys.stderr)
        sys.exit(1)
