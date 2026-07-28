# CUDA winner reduction

## Contract

CUDA search keeps one best `DeviceSample` per candidate and one incumbent
sample. A candidate best retains the same score, time, evaluator details,
evaluation tick, candidate id, candidate slot, mutation flag, and logical
order as the winning entry in the former per-tick history.

`BetterSample` still breaks score ties by the original logical order:

1. The incumbent has logical order 0.
2. Candidate slots are ordered by slot.
3. Evaluation ticks within a candidate are ordered from earliest to latest.

The winner-state kernel is unchanged. It replays only the reduced winner to
its winning evaluation tick, then finalization copies that state, the winning
inputs, and the mutation count into the persistent global best.

`mutationImprovementCount` has changed semantics. It now counts candidate-best
samples that strictly improve the incumbent while candidates are visited in
logical slot order. It no longer counts every improving evaluation tick.
This definition is deterministic, useful at candidate granularity, and does
not require retaining evaluation history.

The public `bestEvaluationTick` reports the selected sample directly.
`winnerSelectionDeviceBytes` reports the candidate-best array, reduction
output, and CUB temporary storage. The legacy
`scoreInitializationKernelMilliseconds` name is retained for compatibility;
it now measures only the constant-size incumbent seed.

## Complexity

`DeviceSample` is 64 bytes in the tested ABI.

| Winner-selection operation | Before | After |
| --- | ---: | ---: |
| Resident samples | `1 + N*T + N` | `N + 2` |
| Sample storage | `O(NT)` | `O(N)` |
| Initialization | `O(NT)` | `O(1)` seed plus existing `O(N)` generation |
| Simulation sample writes | `N*T` | at most `N` |
| CUB reduction inputs | `1 + N*T` | `1 + N` |
| Improvement finalization scan | `N*T` | `N` |
| Winner state capture | one winner replay | one winner replay |

CUB temporary storage is sized from `N + 1`, so it is also `O(N)` and
independent of `evaluationTickCount`. The overall executor still owns the
baseline timeline and physics scratch; `winnerSelectionDeviceBytes` isolates
the storage governed by this change.

## Correctness

`forevervalidator-cuda-search-winner-reduction` compares the legacy flattened
selection with candidate-best selection for minimize and maximize evaluators,
ties within and across candidates, an incumbent tie, invalid candidates,
candidate-level improvement counting, deterministic repetition, and a
1,024-candidate by 4,096-tick synthetic workload.

`run_cuda_winner_reduction_differential.py` runs preserved pre-change and
current binaries against the same replay. It compares winner identity,
evaluation tick, score, time, details, mutation count, all returned input
events, and a full public-state fingerprint. Its cases cover all five
evaluator kinds, the baseline incumbent, invalid evaluator results, random
steering, input insertion, cancellation, short and long windows, and a
second current run for determinism.

On replay `7186170.Replay.Gbx`, all eight differential cases matched exactly.
The 128-candidate, 1,000-tick case selected candidate 13 at evaluation tick
753 in both implementations, with identical state and input fingerprints.
The cancellation case also returned the same incumbent state and inputs.

## Performance

Measurements use an RTX 5060, CUDA 13.3, the Release build, replay
`7186170.Replay.Gbx`, 256 candidates, and medians of six runs (eight for the
short window). The short case branches at 5,000 ms; the longer cases branch
at 0 ms.

| Ticks | Metric | Before | After | Change |
| ---: | --- | ---: | ---: | ---: |
| 32 | Resident device bytes | 22,073,027 | 21,548,739 | -2.4% |
| 32 | Winner storage bytes | 569,983 | 45,695 | -92.0% |
| 32 | Initialization kernel | 0.084 ms | 0.073 ms | -12.9% |
| 32 | Reduction kernel | 0.008 ms | 0.006 ms | -25.0% |
| 32 | Finalization kernel | 0.592 ms | 0.033 ms | -94.5% |
| 32 | Total kernels | 42.159 ms | 40.033 ms | -5.0% |
| 1,000 | Resident device bytes | 38,002,435 | 21,618,435 | -43.1% |
| 1,000 | Winner storage bytes | 16,429,695 | 45,695 | -99.7% |
| 1,000 | Initialization kernel | 0.182 ms | 0.065 ms | -64.3% |
| 1,000 | Reduction kernel | 0.051 ms | 0.008 ms | -83.8% |
| 1,000 | Finalization kernel | 32.093 ms | 0.039 ms | -99.9% |
| 1,000 | Total kernels | 4,649.125 ms | 4,184.672 ms | -10.0% |
| 1,500 | Resident device bytes | 46,230,435 | 21,654,435 | -53.2% |
| 1,500 | Winner storage bytes | 24,621,695 | 45,695 | -99.8% |
| 1,500 | Initialization kernel | 0.231 ms | 0.078 ms | -66.2% |
| 1,500 | Reduction kernel | 0.072 ms | 0.008 ms | -88.6% |
| 1,500 | Finalization kernel | 49.235 ms | 0.038 ms | -99.9% |
| 1,500 | Total kernels | 8,251.370 ms | 7,384.972 ms | -10.5% |

The new winner-storage result is 45,695 bytes for all three evaluation
windows. At fixed `N`, changing `T` from 32 to 1,500 changes timeline storage
but not winner-selection storage. The short workload improves by 5.0%, so it
has no short-window regression; the 1,500-tick workload improves total kernel
time by 10.5%.

Machine-readable medians are in
[`cuda-winner-reduction-performance.json`](cuda-winner-reduction-performance.json).

The final table is generated from:

```sh
cmake -S . -B build/cuda-winner -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DFOREVERVALIDATOR_ENABLE_CUDA=ON \
  -DFOREVERVALIDATOR_BUILD_TESTS=ON \
  -DFOREVERVALIDATOR_BUILD_BENCHMARKS=ON
cmake --build build/cuda-winner -j
ctest --test-dir build/cuda-winner --output-on-failure
```
