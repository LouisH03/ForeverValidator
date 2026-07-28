# CUDA Mutation Generation Performance

## Pinned setup

- Base commit: `3a0dcb826943f23b33f4fffdcdb3e09c418aed74`
- GPU: NVIDIA GeForce RTX 5060 8 GiB
- Driver: 610.43.03
- CUDA toolkit: 13.3.73
- Build: Release, IPO enabled, `CMAKE_CUDA_ARCHITECTURES=native`
- Replay: `Stadium/StadiumCar/7186162.Replay.Gbx`
- Branch time: 5000 ms
- Samples: seven repetitions; repetition zero discarded; table values are medians

The raw pinned profiles are
[`cuda-mutation-generation-before.jsonl`](cuda-mutation-generation-before.jsonl)
and
[`cuda-mutation-generation-after.jsonl`](cuda-mutation-generation-after.jsonl).
The instrumented legacy-layout memory sample is in
[`cuda-mutation-generation-legacy-layout.jsonl`](cuda-mutation-generation-legacy-layout.jsonl).

`kernel_ms` is the full search batch GPU time: score initialization,
mutation generation/application, candidate simulation, winner reduction,
winner-state replay, and finalization. `mutation_ms` and `simulation_ms`
are separately timed CUDA event ranges.

## Evidence and result

| Workload | Mutation before -> after | Mutation speedup | Simulation before -> after | Full kernel before -> after | Kernel speedup | Mutation share before -> after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Sparse insertion, 15k candidates, 1 tick | 26.140 -> 12.569 ms | 2.08x | 2.552 -> 2.344 ms | 31.685 -> 17.833 ms | 1.78x | 82.5% -> 70.5% |
| Sparse insertion, 15k candidates, 10 ticks | 25.259 -> 12.623 ms | 2.00x | 15.008 -> 14.536 ms | 61.501 -> 39.964 ms | 1.54x | 41.1% -> 31.6% |
| Random steering, 15k candidates, 100 ticks | 23.354 -> 1.726 ms | 13.53x | 208.370 -> 195.486 ms | 425.632 -> 380.574 ms | 1.12x | 5.49% -> 0.45% |

The one-tick insertion case intentionally contains almost no simulation and
remains a mutation stress case. In the representative 10- and 100-tick
searches, mutation generation no longer dominates. The remaining insertion
cost is MT19937 generation plus event normalization; the 100-tick random
search is now dominated by simulation and winner replay.

## Design

- Random-steering-only pipelines keep one immutable baseline event stream and
  a compact per-candidate overlay containing only steering values that can be
  changed by a configured window.
- Simulation reads the baseline plus overlay lazily. Only the selected winner
  is materialized into the persistent best-input buffer.
- MT19937 state is transposed by state word and candidate slot, making warp
  seed and twist accesses coalesced without changing the generator or draw
  order.
- General modifiers retain the materialized representation, but scratch
  buffers are allocated only for operations that use them.
- Canonical event streams skip normalization after operations that preserve
  time order, uniqueness, and value bounds. Held insertions and pre-branch
  windows retain per-pass snapshots.
- The original materialization and normalization pipeline remains selectable
  only for differential testing. Production never falls back to it or to CPU.

## Memory and transfer

Random steering at 15k candidates:

| Measurement | Legacy layout | Optimized | Change |
| --- | ---: | ---: | ---: |
| Total resident device memory | 1,200,985,051 B | 1,057,825,815 B | -11.9% |
| Mutation working set | 181,218,088 B | 38,058,852 B | -79.0% |
| Per-candidate input storage | 44,160,000 B | 420,000 B | -99.0% |
| Mutation scratch | 136,860,000 B | 37,440,000 B | -72.6% |
| Initial H2D | 73,072 B | 73,836 B | +764 B |
| Per-batch H2D | 0 B | 0 B | unchanged |
| Per-batch D2H | 65,760 B | 65,760 B | unchanged |

The compact index and offset maps add 764 bytes to the one-time upload. No
candidate mutation payload crosses PCIe in either direction.

## Differential and stress coverage

`cuda_search_benchmark ... differential` runs optimized and legacy sessions
from the same branch snapshot, grows both capacities from one candidate to the
requested batch size, and compares every observable batch field:
candidate IDs and counts, evaluator and mutation counts, cancellation,
capacity status, winner identity and score details, exact input events, and
the captured winner state. It also exercises immediate cancellation and
`UINT64_MAX` candidate-ID rollover before the timed batches.

The final differential matrix covers:

| Case | Candidates | Ticks | Purpose | Result |
| --- | ---: | ---: | --- | --- |
| Sparse insertion | 15,000 | 1 and 10 | Maximum batch, one inserted event | Exact |
| Random steering | 15,000 | 100 | Compact overlay and maximum batch | Exact |
| Existing-event mutation | 4,096 | 100 | Eligibility scratch and shifts | Exact |
| Smooth steering | 4,096 | 100 | Dense value deformation | Exact |
| Dense insertion | 4,096 | 100 | 48 insertions plus held-state restores | Exact |
| Input deletion | 4,096 | 100 | Sparse output and eligibility scratch | Exact |
| Dense insertion boundary | 1 | 100 | Minimum batch and dense capacity sizing | Exact |

Reference/OptimizedCpu/CUDA replay parity is checked separately with
`cuda_replay_parity ... both`.

## Unrelated CUDA path

The non-search `cuda_backend_benchmark` was built once from exact `main` and
once from this branch, then run in 11 interleaved process pairs. Median results:

| Metric | Main | Optimized branch | Change |
| --- | ---: | ---: | ---: |
| Batch wall time | 57.059 ms | 53.253 ms | -6.67% |
| Kernel time | 35.331 ms | 33.930 ms | -3.97% |
| Timeline throughput | 112,164.556 ticks/s | 120,180.675 ticks/s | +7.15% |
| H2D / D2H volume | 4,476,932 / 4,014,080 B | same | 0% |

This is comfortably inside the 5% unrelated-path regression limit.

## Reproduction

```sh
tools/run_cuda_mutation_profile.py PACKS REPLAY \
  --benchmark build/cuda-mutation-perf/cuda_search_benchmark \
  --pipeline optimized --repetitions 7

tools/run_cuda_mutation_profile.py PACKS REPLAY \
  --benchmark build/cuda-mutation-perf/cuda_search_benchmark \
  --pipeline differential --repetitions 2

build/cuda-mutation-perf/cuda_replay_parity PACKS REPLAY both
ctest --test-dir build/cuda-mutation-perf --output-on-failure
```
