#!/usr/bin/env python3

import argparse
import json
import statistics
import subprocess
from pathlib import Path


WORKLOADS = (
    ("sparse-insertion-1t", 15000, 1, "input-insertion"),
    ("sparse-insertion-10t", 15000, 10, "input-insertion"),
    ("random-steering-100t", 15000, 100, "random-steering"),
    ("existing-event-100t", 4096, 100, "existing-event"),
    ("smooth-steering-100t", 4096, 100, "smooth-steering"),
    ("dense-insertion-100t", 4096, 100, "dense-insertion"),
    ("input-deletion-100t", 4096, 100, "input-deletion"),
)


def median(rows, key):
    samples = [row[key] for row in rows[1:]]
    return statistics.median(samples)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("packs", type=Path)
    parser.add_argument("replay", type=Path)
    parser.add_argument(
        "--benchmark",
        type=Path,
        default=Path("build/cuda-mutation-perf/cuda_search_benchmark"),
    )
    parser.add_argument(
        "--pipeline",
        choices=("optimized", "legacy", "differential"),
        default="optimized",
    )
    parser.add_argument("--repetitions", type=int, default=7)
    parser.add_argument("--branch-time-ms", type=int, default=5000)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    output = []
    for name, candidates, ticks, modifier in WORKLOADS:
        command = [
            str(args.benchmark),
            str(args.packs),
            str(args.replay),
            str(candidates),
            str(ticks),
            str(args.repetitions),
            str(args.branch_time_ms),
            modifier,
            args.pipeline,
        ]
        completed = subprocess.run(
            command, check=True, text=True, capture_output=True
        )
        rows = [
            json.loads(line)
            for line in completed.stdout.splitlines()
            if line.strip()
        ]
        summary = {
            "workload": name,
            "pipeline": args.pipeline,
            "candidates": candidates,
            "timeline_ticks": ticks,
            "modifier": modifier,
            "median_wall_ms": median(rows, "wall_ms"),
            "median_kernel_ms": median(rows, "kernel_ms"),
            "median_mutation_ms": median(rows, "mutation_kernel_ms"),
            "median_simulation_ms": median(rows, "simulation_kernel_ms"),
            "median_winner_ms": median(rows, "winner_kernel_ms"),
            "resident_device_bytes": rows[-1]["resident_device_bytes"],
            "mutation_device_bytes": rows[-1]["mutation_device_bytes"],
            "candidate_input_device_bytes": rows[-1][
                "candidate_input_device_bytes"
            ],
            "mutation_scratch_device_bytes": rows[-1][
                "mutation_scratch_device_bytes"
            ],
            "host_to_device_bytes": rows[-1]["host_to_device_bytes"],
            "device_to_host_bytes": rows[-1]["device_to_host_bytes"],
            "initial_host_to_device_bytes": rows[-1][
                "initial_host_to_device_bytes"
            ],
            "baseline_device_to_host_bytes": rows[-1][
                "baseline_device_to_host_bytes"
            ],
            "total_mutation_count": rows[-1]["total_mutation_count"],
            "best_candidate_id": rows[-1]["best_candidate_id"],
            "best_score": rows[-1]["best_score"],
        }
        output.append(summary)
        print(json.dumps(summary, sort_keys=True), flush=True)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            "".join(json.dumps(row, sort_keys=True) + "\n" for row in output),
            encoding="utf-8",
        )


if __name__ == "__main__":
    main()
