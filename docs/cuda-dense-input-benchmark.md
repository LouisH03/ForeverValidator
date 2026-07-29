# CUDA dense-input benchmark matrix

`run_cuda_dense_input_matrix.py` exercises every CUDA search modifier at
approximately 10 and 100 synthetic input events per second. It also covers
early and late branch/mutable-suffix boundaries, ordered mixed modifier passes,
overlapping smooth steering, held multi-channel insertion and restoration,
multi-channel deletion, finish refinement, winner capture, capacity growth,
cancellation, and candidate-ID rollover.

The benchmark executable preserves its original positional interface. Dense
workloads add two optional arguments after an explicit evaluator. The boundary
offset delays modifier eligibility within the suffix; the matrix uses distinct
branch times for the actual early/late suffix boundary:

```sh
build/cuda-dense/cuda_search_benchmark PACKS REPLAY \
  4096 100 7 5000 mixed optimized velocity \
  --input-rate 100 --boundary-offset-ticks 75
```

The matrix runner can execute a preserved base binary and the integration
binary, reject any exact-result difference, run the optimized-versus-legacy
differential for every workload, and emit raw JSONL, summarized JSONL, and a
Markdown before/after table:

```sh
tools/run_cuda_dense_input_matrix.py PACKS REPLAY \
  --before-benchmark /tmp/forevervalidator-dense-before/cuda_search_benchmark \
  --after-benchmark build/cuda-dense/cuda_search_benchmark \
  --output-dir docs/cuda-dense-input-results
```

Without `--candidates`, the runner calibrates the batch size on the dense
late-boundary mixed workload. The first repetition is treated as warmup.
`mutation_buffer_working_set_gbps` is only the touched-buffer-size/time proxy;
it is not a hardware-counter claim. Use Nsight Compute for achieved DRAM
throughput when the profiler is installed.

Replay parity remains a separate gate:

```sh
build/cuda-dense/cuda_replay_parity PACKS REPLAY both
ctest --test-dir build/cuda-dense --output-on-failure
```

## Integrated results

The final matrix was recorded on an NVIDIA GeForce RTX 5060 8 GiB with driver
610.43.03, CUDA 13.3.73, GCC 16.1.1, Release IPO, native `sm_120` code, 4,096
candidates, 100 ticks, and five repetitions. The first repetition was warmup.
The behavior baseline is commit `696d835`; its benchmark executable also
contains the benchmark-only commit `e7d4318`.

Raw samples, summaries, and the complete per-workload table are retained in
`docs/cuda-dense-input-results/`. The exact command was:

```sh
python tools/run_cuda_dense_input_matrix.py PACKS REPLAY \
  --before-benchmark BEFORE/cuda_search_benchmark \
  --after-benchmark build/cuda-dense/cuda_search_benchmark \
  --output-dir docs/cuda-dense-input-results \
  --candidates 4096 --repetitions 5
```

The runner also executed an optimized-versus-legacy differential for every
workload. All 13 workloads matched the preserved implementation exactly,
including mutation counts, winner IDs and scores, replay-state fingerprints,
and normalized winner-input fingerprints.

| Workload group | End-to-end attempts/s | Mutation-stage speedup | Candidate input memory | Mutation scratch |
| --- | ---: | ---: | ---: | ---: |
| Random steering, 10/100 eps | 1.01x / 1.01x | 0.99x / 1.02x | unchanged | unchanged |
| Existing event, 10/100 eps | 1.00x / 1.00x | 1.01x / 1.28x | -25.8% / -50.5% | -15.7% / -34.8% |
| Smooth steering, 10/100 eps | 1.17x / 1.20x | 2.16x / 2.34x | -13.5% / -30.9% | -9.5% / -23.0% |
| Input insertion, sparse/held | 1.00x / 1.08x | 1.07x / 2.64x | -25.5% / -37.5% | -14.2% / -31.0% |
| Input deletion, 10/100 eps | 1.01x / 1.02x | 2.51x / 3.49x | -25.8% / -50.5% | -15.7% / -34.8% |
| Mixed, 10/100 eps | 1.18x / 1.20x | 2.23x / 2.33x | -10.7% / -25.5% | -9.3% / -22.6% |
| Mixed finish-time, 100 eps | 1.32x | 2.17x | -9.0% | -8.0% |

Dense late-boundary resident memory fell by 20.6 MB for ExistingEvent,
20.6 MB for InputDeletion, 27.5 MB for held insertion, and 29.8 MB for the
mixed pipeline. The measured simulation kernel stayed at 255 registers per
thread and 16.67% theoretical occupancy for the 4,096-candidate latency
launch. Local memory fell from 12,528 to 11,944 bytes per thread. The
8,192-candidate throughput launch was also capacity-checked and reported 128
registers, 12,008 local bytes, and 33.33% theoretical occupancy.

Nsight Compute is not installed on the benchmark host, so no hardware-counter
DRAM throughput claim is made. `summary.jsonl` reports the reproducible
candidate-input-plus-scratch working-set/time proxy instead. It rises from
1.31 to 2.25 GB/s for dense SmoothSteering, 3.54 to 6.27 GB/s for held
insertion, 12.70 to 26.40 GB/s for dense deletion, and 1.49 to 2.66 GB/s for
the dense mixed pipeline.

## Architecture

The executor partitions the normalized replay once at
`branchTimeMs + tickDurationMs`. Events before that boundary remain an
immutable host-owned prefix; candidate buffers contain only zero-based
suffix-relative events. Cached branch and mutable-boundary control states
carry held values, their exact change timestamps, and stunt timestamps into
generation, simulation, finish refinement, and winner replay. Only the winning
suffix is copied back and joined to the immutable prefix.

The shared candidate-event core provides ordinal-major, warp-coalesced compact
edits for value/time changes, replacement, insertion, and erasure. Value-only
RandomSteering remains overlay-only. Structural pipelines materialize only
their suffix and use a stable merge plus equal-time hash deduplication,
preserving first-key ordering and last-write-wins values without the previous
quadratic insertion and duplicate scans.

Modifier execution retains ordered pass boundaries and MT19937 draw order.
ExistingEvent uses bounded indexed eligibility, SmoothSteering queries its
canonical stream plus appended run, insertion combines state lookup with
range removal, and deletion selects the same logical ranks before one channel
compaction. Focused helper measurements at 1,000 events showed 49.3x bounded
window lookup, 39.9x steering lookup, 1.37x insertion replacement, and 9.32x
32-event deletion.

## Validation and tradeoffs

The merged build passed all 28 configured tests. Recorded and mutated replay
parity passed across Reference, OptimizedCpu, and CUDA for 1,255 ticks, with
32-candidate batch checks. The dense matrix additionally covers early/late
boundaries, 10/100 events per second, crossing time shifts, overlapping smooth
deformations, held restoration, multi-channel deletion, mixed passes, finish
refinement, winner capture, cancellation, two-step capacity growth, and
candidate-ID rollover.

RandomSteering was already compact and therefore remains essentially flat.
ExistingEvent and sparse insertion are now dominated by simulation and winner
capture, so their reduced mutation time and memory do not materially change
whole-batch attempts/s. Structural modifiers still keep a materialized
candidate suffix when later passes must observe earlier edits; the compact
edit interface leaves a path to remove more of that storage, but doing so
without changing ordered-pass semantics is the main remaining bottleneck.
