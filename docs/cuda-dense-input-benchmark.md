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
Markdown before/after table plus retained parity evidence:

```sh
tools/run_cuda_dense_input_matrix.py PACKS REPLAY \
  --before-benchmark /tmp/forevervalidator-dense-before/cuda_search_benchmark \
  --after-benchmark build/cuda-dense/cuda_search_benchmark \
  --output-dir docs/cuda-dense-input-results
```

Without `--candidates`, the runner calibrates each build independently for
each branch/evaluation-length scenario on the dense mixed workload. The first
repetition is treated as warmup.
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
610.43.03, CUDA 13.3.73, GCC 16.1.1, Release IPO, and native `sm_120` code.
Each build calibrated a real resident batch independently by doubling from
1,024 candidates until reservation failed. The preserved build selected
32,768 candidates for the early 100-tick scenario and 16,384 for the late and
500-tick scenarios. The compact build selected 32,768 for all three. Each
timed workload used five repetitions with the first treated as warmup.

Synthetic 100/s inputs start at replay time zero, not at the mutable boundary,
so branch-state reconstruction includes the dense immutable prefix. The matrix
uses 5,000 and 10,000 ms branch times plus 100 and 500 evaluation ticks. The
behavior baseline is commit `696d835`; its benchmark executable additionally
contains benchmark-only commits `e7d4318`, `d98dcb2`, and `17656c7`.

Raw samples, summaries, and the complete per-workload table are retained in
`docs/cuda-dense-input-results/`. The exact command was:

```sh
python tools/run_cuda_dense_input_matrix.py PACKS REPLAY \
  --before-benchmark BEFORE/cuda_search_benchmark \
  --after-benchmark build/cuda-dense/cuda_search_benchmark \
  --output-dir docs/cuda-dense-input-results \
  --repetitions 5
```

The typical median throughput improvement across 20 workloads is 2.20x. The
worst case is RandomSteering at 1.15x and the best is dense SmoothSteering over
500 ticks at 6.45x. Dense mixed search improves by 4.78x over 100 ticks and
5.49x over 500 ticks. The table intentionally reports every workload and the
worst case rather than selecting only the largest speedup.

| Workload group | End-to-end attempts/s | Mutation-stage speedup | Candidate-event allocation |
| --- | ---: | ---: | ---: |
| Random steering, 10/100 eps | 1.16x / 1.15x | approximately flat per candidate | unchanged per candidate |
| Existing event, 10/100 eps | 1.40x / 1.28x | 1.59x / 2.78x per candidate | 61.0 to 15.7 / 307.3 to 15.3 MiB |
| Static existing event, 100 eps | 1.25x | 3.23x per candidate | 191.3 to 21.3 MiB |
| Smooth steering, 10/100 eps | 2.48x / 3.89x | 3.11x / 5.55x per candidate | 105.0 to 77.4 / 351.3 to 77.0 MiB |
| Input insertion, sparse/held | 1.25x / 4.66x | 1.60x / 12.17x per candidate | 61.5 to 9.6 / 331.3 to 47.8 MiB |
| Input deletion, 10/100 eps | 1.35x / 2.15x | 8.04x / 38.86x per candidate | 61.0 to 9.2 / 307.3 to 8.8 MiB |
| Mixed, 10/100 eps | 2.43x / 4.78x | 2.83x / 6.53x per candidate | 129.0 to 127.0 / 375.3 to 142.0 MiB |
| Dense mixed, 500 ticks | 5.49x | 8.63x per candidate | 352.3 to 243.6 MiB |

The throughput kernel reports 128 registers per thread and 33.33% theoretical
occupancy in both builds. Local memory falls from 12,592 to 12,024 bytes per
thread. The full timing, chosen batch, resident allocation, scratch allocation,
winner replay, register, local-memory, and occupancy results are in
`before-after.md`.

Nsight Compute is not installed on the benchmark host, so no hardware-counter
DRAM throughput claim is made. `summary.jsonl` reports the reproducible
candidate-input-plus-scratch working-set/time proxy instead.

## Architecture

The executor partitions the normalized replay once at
`branchTimeMs + tickDurationMs`. Events before that boundary remain an
immutable host-owned prefix; candidate buffers contain only zero-based
suffix-relative events. Cached branch and mutable-boundary control states
carry held values, their exact change timestamps, and stunt timestamps into
generation, simulation, finish refinement, and winner replay. The unused
second branch-control allocation was removed.

Each candidate is stored as sorted output edits and sorted suppressed baseline
indices over one shared suffix. Fields are ordinal-major so adjacent lanes read
the same edit ordinal coalescently. Common input/action dimensions use packed
16- and 8-bit fields; large configurations retain the wide fallback. A
sequential cursor merges unchanged baseline events with sparse edits during
simulation, finish refinement, state capture, and finalization. Only the
selected winner is materialized and joined to the immutable prefix.

Value-only RandomSteering retains its smaller value overlay. Pure
InputDeletion writes suppressed source indices directly and never materializes
candidate streams. ExistingEvent time shifts, SmoothSteering, held insertion,
and mixed ordered passes materialize a suffix only while later modifier
semantics require observing prior edits. Their final normalization scratch is
then overlaid with the compact edit storage, avoiding another resident copy.

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
32-candidate batch checks. For all 19 workloads, `validation.jsonl` retains a
1,024-candidate exact before/after check and a 1,024-candidate
optimized-versus-legacy differential. Mutation counts, winner IDs and scores,
physics-state fingerprints, and normalized winner-input fingerprints match.

The matrix additionally covers early/late boundaries, dense inputs before the
branch, 10/100 events per second, 100/500 evaluation ticks, crossing time
shifts, overlapping smooth deformations, held restoration, multi-channel
deletion, mixed passes, finish refinement, winner capture, cancellation,
capacity growth, and candidate-ID rollover.

RandomSteering was already compact and remains flat. ExistingEvent and sparse
insertion are increasingly simulation-bound after their preprocessing
reductions. Ordered structural pipelines still require full temporary suffixes
during mutation generation, but those arrays are no longer candidate storage
for simulation or winner replay. Removing that ordered-pass scratch without
changing observable modifier order is the remaining memory opportunity.
