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


WORKLOADS = (
    Workload("random-steering-10eps-early", "random-steering", 10, 0),
    Workload("random-steering-100eps-late", "random-steering", 100, 5000),
    Workload("existing-event-10eps-early", "existing-event", 10, 0),
    Workload("existing-event-100eps-late", "existing-event", 100, 5000),
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
        str(args.timeline_ticks),
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


def calibrate(args, benchmark):
    workload = next(item for item in WORKLOADS if item.name == "mixed-100eps-late")
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
    lines = [
        "| Workload | Before attempts/s | After attempts/s | Speedup | "
        "Before mutation ms | After mutation ms | Candidate input change | "
        "Scratch change |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for before, after in comparisons:
        old_rate = before["median_attempts_per_second"]
        new_rate = after["median_attempts_per_second"]
        speedup = 0.0 if old_rate == 0.0 else new_rate / old_rate
        old_inputs = before["candidate_input_device_bytes"]
        new_inputs = after["candidate_input_device_bytes"]
        old_scratch = before["mutation_scratch_device_bytes"]
        new_scratch = after["mutation_scratch_device_bytes"]
        input_change = 0.0 if old_inputs == 0 else new_inputs / old_inputs - 1.0
        scratch_change = (
            0.0 if old_scratch == 0 else new_scratch / old_scratch - 1.0
        )
        lines.append(
            f"| {before['workload']} | {old_rate:,.1f} | "
            f"{new_rate:,.1f} | {speedup:.2f}x | "
            f"{before['median_mutation_kernel_ms']:.3f} | "
            f"{after['median_mutation_kernel_ms']:.3f} | "
            f"{input_change:+.1%} | {scratch_change:+.1%} |"
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
        default=(1024, 2048, 4096, 8192),
    )
    parser.add_argument("--timeline-ticks", type=int, default=100)
    parser.add_argument("--repetitions", type=int, default=7)
    parser.add_argument("--branch-time-ms", type=int, default=5000)
    parser.add_argument(
        "--differential",
        action=argparse.BooleanOptionalAction,
        default=True,
    )
    args = parser.parse_args()

    if args.repetitions < 1:
        parser.error("--repetitions must be positive")
    if args.candidates is not None and args.candidates < 1:
        parser.error("--candidates must be positive")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    candidates = args.candidates or calibrate(args, args.after_benchmark)
    raw_path = args.output_dir / "raw.jsonl"
    summary_path = args.output_dir / "summary.jsonl"
    table_path = args.output_dir / "before-after.md"

    summaries = []
    comparisons = []
    with raw_path.open("w", encoding="utf-8") as raw:
        for workload in WORKLOADS:
            builds = []
            if args.before_benchmark:
                builds.append(("before", args.before_benchmark))
            builds.append(("after", args.after_benchmark))
            workload_summaries = []
            for label, benchmark in builds:
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
                ensure_exact(*workload_summaries)
                comparisons.append(tuple(workload_summaries))

            if args.differential:
                run_workload(
                    args,
                    args.after_benchmark,
                    workload,
                    candidates,
                    "differential",
                    repetitions=1,
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
                "calibrated_batch_size": candidates,
                "raw": str(raw_path),
                "summary": str(summary_path),
                "before_after": str(table_path) if comparisons else None,
                "workloads": len(WORKLOADS),
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
