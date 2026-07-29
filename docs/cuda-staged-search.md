# CUDA staged search

## Pipeline

Precise finish-time search uses a GPU-resident staged pipeline:

1. Seed the incumbent sample.
2. Generate mutations into field-oriented event/count/status buffers and
   compact the valid candidate slots into an active queue.
3. Launch a captured CUDA Graph that initializes candidate physics,
   control, and event-cursor fields, then simulates the active queue.
4. Refine finish crossings from the saved pre-finish state and tick.
5. Reduce samples, assemble the winning state, and update the resident
   incumbent.

The graph is captured when the executor is created and rebuilt only when
calibration increases the batch capacity. Candidate state, control state,
event cursors, finish snapshots, samples, and collision scratch remain on
the device between stages. Threads index the same field by active candidate
slot, while invalid mutations never enter the physics stages.

Finish-time searches without stunts use this path. Other evaluators and
stunt-enabled searches retain the general simulation kernel.

## Profiled boundary

The physics stage intentionally covers the full search timeline. Measurements
also tested prepare/physics/finalize kernels for every tick and physics chunks
of 32 and 128 ticks. Fine-grained stages reduced the isolated physics kernel's
stack in the per-tick case, but repeatedly storing and loading the complete
future-affecting vehicle state dominated both representative workloads.
CUDA Graphs removed host launch overhead but could not remove those device
memory round trips. The full-timeline boundary was therefore retained.

The resulting staged physics kernel uses 255 registers and 11,832 bytes of
stack per thread on `sm_120`, compared with 255 registers and 12,528 bytes for
main. Both allow eight 32-thread blocks per SM, or 16.67% theoretical
occupancy. Resource figures come from both `cudaFuncGetAttributes` and
`cuobjdump --dump-resource-usage`.

## Correctness

The 25-test Release CUDA suite passes. After the graph change, the existing
four-replay parity corpus passed recorded and mutated timelines for Puzzle
(1,255 ticks), Platform (1,546), Race (3,530), and Stunts (8,296). The staged
path reuses the same exact physics step covered by that corpus.

The final C02 `few-existing-events differential finish-time` run passed the
optimized-versus-legacy mutation comparison. Representative main and staged
runs selected the same candidate, evaluation tick, nanosecond finish score,
state fingerprint, input count, and input fingerprint on every matching
repetition:

| Workload | Finish time | Candidate | State fingerprint | Input fingerprint |
| --- | ---: | ---: | ---: | ---: |
| C02 | 29,579.580148 ms | 96 | 8782561883133359268 | 11662071590944021419 |
| Tasmania #31 | 23,047.207632 ms | 871 | 15429619988563316987 | 2282917505808104393 |

## Performance

Measurements use an RTX 5060, CUDA 13.3, a Release `sm_120` build, 1,024
candidates, and the `few-existing-events` modifier configured for three to
five existing-event operations. C02 branches at 27,000 ms and simulates 258
ticks. Tasmania #31 branches at 20,000 ms and simulates 305 ticks. Values are
medians of three main runs and five staged runs.

| Metric | C02 main | C02 staged | Tasmania main | Tasmania staged |
| --- | ---: | ---: | ---: | ---: |
| Attempts/s | 965.52 | 1,520.77 | 1,762.85 | 2,776.59 |
| Physics ticks/s | 389,808 | 400,443 | 901,351 | 897,089 |
| Total kernel time | 1,060.18 ms | 672.48 ms | 580.51 ms | 368.51 ms |
| Simulation time | 677.75 ms | 659.75 ms | 346.50 ms | 348.15 ms |
| Mutation time | 5.638 ms | 5.586 ms | 2.645 ms | 2.557 ms |
| Registers/thread | 255 | 255 | 255 | 255 |
| Local stack/thread | 12,528 B | 11,832 B | 12,528 B | 11,832 B |
| Theoretical occupancy | 16.67% | 16.67% | 16.67% | 16.67% |

End-to-end throughput improves by 1.58x on both workloads. Mutation time does
not regress, C02 simulation throughput improves by 2.7%, and Tasmania
simulation throughput is within 0.5% while its finish refinement and winner
capture produce the end-to-end gain.

Machine-readable medians are in
[`cuda-staged-search-performance.json`](cuda-staged-search-performance.json).
