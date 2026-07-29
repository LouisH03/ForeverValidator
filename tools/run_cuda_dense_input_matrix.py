#!/usr/bin/env python3

import argparse
import json
import statistics
import subprocess
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Workload:
    name: str
    modifier: str
    input_rate: int
    branch_offset_ms: int
    modifier_offset_ticks: int = 0
    evaluator: str = "velocity"
    timeline_multiplier: int = 1


WORKLOADS = (
    Workload("random-steering-10eps-early", "random-steering", 10, 0),
    Workload("random-steering-100eps-late", "random-steering", 100, 5000),
    Workload("existing-event-10eps-early", "existing-event", 10, 0),
    Workload("existing-event-100eps-late", "existing-event", 100, 5000),
    Workload(
        "existing-event-static-100eps-early",
        "existing-event-static",
        100,
        0,
    ),
    Workload("smooth-steering-10eps-early", "smooth-steering", 10, 0),
    Workload("smooth-steering-100eps-late", "smooth-steering", 100, 5000),
    Workload("input-insertion-10eps-early", "input-insertion", 10, 0),
    Workload("held-insertion-100eps-late", "dense-insertion", 100, 5000),
    Workload("input-deletion-10eps-early", "input-deletion", 10, 0),
    Workload("input-deletion-100eps-late", "input-deletion", 100, 5000),
    Workload("mixed-10eps-early", "mixed", 10, 0),
    Workload("mixed-100eps-late", "mixed", 100, 5000),
    Workload(
        "mixed-finish-100eps",
        "mixed",
        100,
        0,
        evaluator="finish-time",
    ),
    Workload(
        "existing-event-100eps-long-eval",
        "existing-event",
        100,
        0,
        timeline_multiplier=5,
    ),
    Workload(
        "smooth-steering-100eps-long-eval",
        "smooth-steering",
        100,
        0,
        timeline_multiplier=5,
    ),
    Workload(
        "held-insertion-100eps-long-eval",
        "dense-insertion",
        100,
        0,
        timeline_multiplier=5,
    ),
    Workload(
        "input-deletion-100eps-long-eval",
        "input-deletion",
        100,
        0,
        timeline_multiplier=5,
    ),
    Workload(
        "mixed-100eps-long-eval",
        "mixed",
        100,
        0,
        timeline_multiplier=5,
    ),
    Workload(
        "mixed-finish-100eps-long-eval",
        "mixed",
        100,
        0,
        evaluator="finish-time",
        timeline_multiplier=5,
    ),
)

EXACT_KEYS = (
    "evaluated_candidates",
    "total_mutation_count",
    "cancelled",
    "best_changed",
    "best_is_mutation",
    "best_candidate_id",
    "best_evaluation_tick",
    "best_score",
    "best_time_ms",
    "best_detail_0",
    "best_detail_1",
    "best_mutation_count",
    "mutation_improvement_count",
    "best_state_fingerprint",
    "best_input_count",
    "best_input_fingerprint",
)

MEDIAN_KEYS = (
    "attempts_per_second",
    "wall_ms",
    "kernel_ms",
    "score_initialization_kernel_ms",
    "mutation_kernel_ms",
    "simulation_kernel_ms",
    "finish_refinement_kernel_ms",
    "winner_kernel_ms",
    "winner_reduction_kernel_ms",
    "winner_state_capture_kernel_ms",
    "finalization_kernel_ms",
)


def parse_rows(completed):
    return [
        json.loads(line)
        for line in completed.stdout.splitlines()
        if line.strip()
    ]


def command_for(args, benchmark, workload, candidates, pipeline, repetitions):
    return [
        str(benchmark),
        str(args.packs),
        str(args.replay),
        str(candidates),
        str(args.timeline_ticks * workload.timeline_multiplier),
        str(repetitions),
        str(args.branch_time_ms + workload.branch_offset_ms),
        workload.modifier,
        pipeline,
        workload.evaluator,
        "--input-rate",
        str(workload.input_rate),
        "--boundary-offset-ticks",
        str(workload.modifier_offset_ticks),
    ]


def run_workload(
    args, benchmark, workload, candidates, pipeline, repetitions=None
):
    command = command_for(
        args,
        benchmark,
        workload,
        candidates,
        pipeline,
        repetitions or args.repetitions,
    )
    completed = subprocess.run(
        command, check=True, text=True, capture_output=True
    )
    return parse_rows(completed)


def timed_rows(rows):
    return rows[1:] if len(rows) > 1 else rows


def summarize(label, workload, rows):
    samples = timed_rows(rows)
    final = rows[-1]
    summary = {
        "build": label,
        "workload": workload.name,
        "modifier": workload.modifier,
        "input_events_per_second": workload.input_rate,
        "branch_time_ms": final["branch_time_ms"],
        "modifier_offset_ticks": workload.modifier_offset_ticks,
        "evaluator": workload.evaluator,
        "candidates": final["candidates"],
        "calibrated_batch_size": final["calibrated_batch_size"],
        "timeline_ticks": final["timeline_ticks"],
    }
    for key in MEDIAN_KEYS:
        summary["median_" + key] = statistics.median(
            row[key] for row in samples
        )
    for key in (
        "resident_device_bytes",
        "mutation_device_bytes",
        "candidate_input_device_bytes",
        "mutation_scratch_device_bytes",
        "winner_selection_device_bytes",
        "simulation_registers_per_thread",
        "simulation_local_bytes_per_thread",
        "simulation_active_blocks_per_sm",
        "simulation_theoretical_occupancy",
    ):
        summary[key] = final[key]
    mutation_bytes = (
        final["candidate_input_device_bytes"]
        + final["mutation_scratch_device_bytes"]
    )
    mutation_ms = summary["median_mutation_kernel_ms"]
    summary["mutation_buffer_working_set_gbps"] = (
        0.0 if mutation_ms == 0.0 else mutation_bytes / mutation_ms / 1.0e6
    )
    summary["exact_outcome"] = {key: final[key] for key in EXACT_KEYS}
    return summary


def scenario_key(workload):
    return (workload.branch_offset_ms, workload.timeline_multiplier)


def calibrate(args, benchmark, scenario):
    branch_offset_ms, timeline_multiplier = scenario
    workload = Workload(
        "calibration",
        "mixed",
        100,
        branch_offset_ms,
        timeline_multiplier=timeline_multiplier,
    )
    selected = None
    for candidates in args.calibration_candidates:
        try:
            run_workload(
                args,
                benchmark,
                workload,
                candidates,
                "optimized",
                repetitions=1,
            )
        except subprocess.CalledProcessError:
            break
        selected = candidates
    if selected is None:
        raise RuntimeError("no calibration batch size completed")
    return selected


def ensure_exact(before, after):
    if before["exact_outcome"] != after["exact_outcome"]:
        differing = [
            key
            for key in EXACT_KEYS
            if before["exact_outcome"][key] != after["exact_outcome"][key]
        ]
        raise RuntimeError(
            f"{before['workload']} changed exact outcome fields: "
            + ", ".join(differing)
        )


def markdown_table(comparisons):
    speedups = []
    for before, after in comparisons:
        old_rate = before["median_attempts_per_second"]
        new_rate = after["median_attempts_per_second"]
        speedup = 0.0 if old_rate == 0.0 else new_rate / old_rate
        speedups.append((speedup, before["workload"]))
    median_speedup = statistics.median(item[0] for item in speedups)
    worst_speedup, worst_workload = min(speedups)
    best_speedup, best_workload = max(speedups)
    lines = [
        f"Typical (median) throughput speedup: **{median_speedup:.2f}x**.",
        "",
        f"Worst case: **{worst_speedup:.2f}x** "
        f"({worst_workload}).",
        "",
        f"Best case: **{best_speedup:.2f}x** ({best_workload}).",
        "",
        "| Workload | Before batch | After batch | Before attempts/s | "
        "After attempts/s | Speedup | Mutation ms B/A | "
        "Simulation ms B/A | Winner replay ms B/A |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for before, after in comparisons:
        old_rate = before["median_attempts_per_second"]
        new_rate = after["median_attempts_per_second"]
        speedup = 0.0 if old_rate == 0.0 else new_rate / old_rate
        lines.append(
            f"| {before['workload']} | "
            f"{before['calibrated_batch_size']:,} | "
            f"{after['calibrated_batch_size']:,} | "
            f"{old_rate:,.1f} | {new_rate:,.1f} | {speedup:.2f}x | "
            f"{before['median_mutation_kernel_ms']:.3f} / "
            f"{after['median_mutation_kernel_ms']:.3f} | "
            f"{before['median_simulation_kernel_ms']:.3f} / "
            f"{after['median_simulation_kernel_ms']:.3f} | "
            f"{before['median_winner_kernel_ms']:.3f} / "
            f"{after['median_winner_kernel_ms']:.3f} |"
        )
    lines.extend(
        [
            "",
            "| Workload | Resident MiB B/A | Candidate events MiB B/A | "
            "Scratch MiB B/A | Registers/thread B/A | "
            "Local bytes/thread B/A | Occupancy B/A |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    mib = 1024 * 1024
    for before, after in comparisons:
        lines.append(
            f"| {before['workload']} | "
            f"{before['resident_device_bytes'] / mib:.2f} / "
            f"{after['resident_device_bytes'] / mib:.2f} | "
            f"{before['candidate_input_device_bytes'] / mib:.2f} / "
            f"{after['candidate_input_device_bytes'] / mib:.2f} | "
            f"{before['mutation_scratch_device_bytes'] / mib:.2f} / "
            f"{after['mutation_scratch_device_bytes'] / mib:.2f} | "
            f"{before['simulation_registers_per_thread']} / "
            f"{after['simulation_registers_per_thread']} | "
            f"{before['simulation_local_bytes_per_thread']} / "
            f"{after['simulation_local_bytes_per_thread']} | "
            f"{before['simulation_theoretical_occupancy']:.3f} / "
            f"{after['simulation_theoretical_occupancy']:.3f} |"
        )
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("packs", type=Path)
    parser.add_argument("replay", type=Path)
    parser.add_argument("--after-benchmark", type=Path, required=True)
    parser.add_argument("--before-benchmark", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--candidates", type=int)
    parser.add_argument(
        "--calibration-candidates",
        type=lambda value: tuple(int(item) for item in value.split(",")),
        default=(
            1024,
            2048,
            4096,
            8192,
            16384,
            32768,
            65536,
            131072,
        ),
    )
    parser.add_argument("--timeline-ticks", type=int, default=100)
    parser.add_argument("--repetitions", type=int, default=7)
    parser.add_argument("--branch-time-ms", type=int, default=5000)
    parser.add_argument("--parity-candidates", type=int, default=1024)
    parser.add_argument(
        "--differential-candidates", type=int, default=1024
    )
    parser.add_argument(
        "--differential",
        action=argparse.BooleanOptionalAction,
        default=True,
    )
    args = parser.parse_args()

    if args.repetitions < 1:
        parser.error("--repetitions must be positive")
    if args.timeline_ticks < 1:
        parser.error("--timeline-ticks must be positive")
    if args.candidates is not None and args.candidates < 1:
        parser.error("--candidates must be positive")
    if args.parity_candidates < 1 or args.differential_candidates < 1:
        parser.error("parity candidate counts must be positive")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    raw_path = args.output_dir / "raw.jsonl"
    summary_path = args.output_dir / "summary.jsonl"
    table_path = args.output_dir / "before-after.md"
    calibration_path = args.output_dir / "calibration.json"
    validation_path = args.output_dir / "validation.jsonl"
    builds = []
    if args.before_benchmark:
        builds.append(("before", args.before_benchmark))
    builds.append(("after", args.after_benchmark))
    scenarios = sorted({scenario_key(workload) for workload in WORKLOADS})
    calibrated = {}
    for label, benchmark in builds:
        for scenario in scenarios:
            calibrated[(label, scenario)] = (
                args.candidates
                if args.candidates is not None
                else calibrate(args, benchmark, scenario)
            )
    calibration_document = {
        label: {
            f"branch_offset_ms={scenario[0]},"
            f"timeline_ticks={args.timeline_ticks * scenario[1]}":
                    calibrated[(label, scenario)]
            for scenario in scenarios
        }
        for label, _ in builds
    }
    calibration_path.write_text(
        json.dumps(calibration_document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    summaries = []
    comparisons = []
    with (
        raw_path.open("w", encoding="utf-8") as raw,
        validation_path.open("w", encoding="utf-8") as validation,
    ):
        for workload in WORKLOADS:
            workload_summaries = []
            for label, benchmark in builds:
                candidates = calibrated[
                    (label, scenario_key(workload))
                ]
                rows = run_workload(
                    args,
                    benchmark,
                    workload,
                    candidates,
                    "optimized",
                )
                for row in rows:
                    raw.write(
                        json.dumps(
                            {
                                "build": label,
                                "workload": workload.name,
                                **row,
                            },
                            sort_keys=True,
                        )
                        + "\n"
                    )
                result = summarize(label, workload, rows)
                summaries.append(result)
                workload_summaries.append(result)
            if len(workload_summaries) == 2:
                before_candidates = calibrated[
                    ("before", scenario_key(workload))
                ]
                after_candidates = calibrated[
                    ("after", scenario_key(workload))
                ]
                parity_candidates = min(
                    before_candidates,
                    after_candidates,
                    args.parity_candidates,
                )
                parity = []
                for label, benchmark in builds:
                    parity_rows = run_workload(
                        args,
                        benchmark,
                        workload,
                        parity_candidates,
                        "optimized",
                        repetitions=1,
                    )
                    for row in parity_rows:
                        validation.write(
                            json.dumps(
                                {
                                    "validation": "before-after",
                                    "build": label,
                                    "workload": workload.name,
                                    **row,
                                },
                                sort_keys=True,
                            )
                            + "\n"
                        )
                    parity.append(
                        summarize(label, workload, parity_rows)
                    )
                ensure_exact(*parity)
                comparisons.append(tuple(workload_summaries))

            if args.differential:
                differential_rows = run_workload(
                    args,
                    args.after_benchmark,
                    workload,
                    min(
                        calibrated[
                            ("after", scenario_key(workload))
                        ],
                        args.differential_candidates,
                    ),
                    "differential",
                    repetitions=1,
                )
                for row in differential_rows:
                    validation.write(
                        json.dumps(
                            {
                                "validation": "optimized-legacy",
                                "build": "after",
                                "workload": workload.name,
                                **row,
                            },
                            sort_keys=True,
                        )
                        + "\n"
                    )

    summary_path.write_text(
        "".join(
            json.dumps(summary, sort_keys=True) + "\n"
            for summary in summaries
        ),
        encoding="utf-8",
    )
    if comparisons:
        table_path.write_text(markdown_table(comparisons), encoding="utf-8")

    print(
        json.dumps(
            {
                "calibration": str(calibration_path),
                "calibrated_batch_sizes": calibration_document,
                "raw": str(raw_path),
                "summary": str(summary_path),
                "validation": str(validation_path),
                "before_after": str(table_path) if comparisons else None,
                "workloads": len(WORKLOADS),
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
