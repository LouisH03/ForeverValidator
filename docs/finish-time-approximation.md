# Finish-time approximation

ForeverValidator reports a simulated race finish as a nanosecond bracket:

```text
(lowerBoundNs, upperBoundNs]
```

Both bounds use the simulation timeline's absolute clock. The bracket is
valid only when its width is at most one nanosecond. `estimatedNs` is defined
as the inclusive upper bound, which makes the reported estimate deterministic
even when the physical transition lies between integer nanoseconds.

Validation subtracts the configured prestart duration from both bounds before
publishing the result. The JSON fields are:

```json
{
  "race_time_ms": 19181,
  "race_time_ns": 19181995196,
  "race_time_bracket_ns": {
    "lower_exclusive": 19181995195,
    "upper_inclusive": 19181995196
  }
}
```

`race_time_ms` remains for compatibility and is the nanosecond estimate
rounded down to milliseconds. Existing finish-time tolerances and validation
classification rules are otherwise unchanged. In particular, classification
continues to compare the legacy prepared-tick time; changing the precision of
the reported result does not move an existing replay across a tolerance
boundary.

## Refinement contract

The ordinary simulation first identifies the adaptive physics substep where
the race state changes from not completed to completed. The runtime restores
the exact state captured immediately before that substep and reruns partial
versions of the same collision substep. Each probe performs collision
detection, collision response, and the real checkpoint/finish callback path.
It does not interpolate vehicle position or consult a recorded replay outcome.

The probe duration is selected by binary search on the integer-nanosecond
timeline. A failed probe moves the exclusive lower bound; a successful probe
moves the inclusive upper bound. Search ends once the bounds are adjacent.
The full substep must independently reproduce the finish or no estimate is
reported.

Every probe begins from the same pre-substep runtime and race snapshot.
After refinement, the authoritative post-tick runtime and race state are
restored. Checkpoints, lap state, respawns, wheel-surface bindings, pending
collision replacements, stunt state, and backend-specific runtime state
therefore remain those produced by the ordinary simulation.

Reference and OptimizedCpu share the host refinement implementation and call
their respective collision kernels for each probe. CUDA timeline execution
uses the same bracket convention and reruns only candidates that actually
finished, avoiding snapshot traffic for non-finishing candidates.

## Determinism

The approximation is deterministic for an identical build, backend, replay,
map data, validation seed, and control timeline. It intentionally describes
the transition produced by that backend's floating-point physics; it is not a
claim that the underlying continuous collision event is known exactly.

The finish bracket and estimate are part of runtime snapshots, CUDA state
serialization, semantic hashes, differential comparisons, and public
simulation outcomes.

## Verification evidence

The implementation was checked on 2026-07-28 against CPU baseline `3a0dcb8`.
The corpus contained 3,745 United replays and 2,169 Stadium replays. Reference
and OptimizedCpu each produced identical baseline/branch classifications for
all 5,914 inputs:

| Classification | Count |
| --- | ---: |
| Valid | 5,903 |
| Wrong simulation | 7 |
| Incompatible replay version | 1 |
| Processing error | 3 |

The 5,911 serialized reports had zero differences in `status`,
`validate_result_code`, `valid`, or `wrong_simulation`. The three processing
errors and all per-directory valid/invalid/error totals also matched.

Backend differential replay `1858.Replay.Gbx` produced the same bracket on
Reference, OptimizedCpu, and CUDA:

```text
(2862623472 ns, 2862623473 ns]
```

CUDA's per-tick CPU/device differential passed all 287 ticks of that replay.
A separate replay with one respawn produced `(7667909287 ns,
7667909288 ns]` on all three backends and retained a respawn count of one.
Five repeated runs on each CPU backend reproduced the same bounds.

## Runtime cost

Measurements used a Release build pinned to one core of an AMD Ryzen 5 7500F.
Each value is the mean of two 31-sample medians, with five warmups per sample
set. The 1,000-tick case stops before the finish. The 1,919-tick case includes
one finish refinement.

| Backend and workload | Baseline | Nanosecond refinement | Change |
| --- | ---: | ---: | ---: |
| OptimizedCpu, 1,000 ticks | 7.184 ms | 7.337 ms | +2.13% |
| Reference, 1,000 ticks | 38.491 ms | 38.439 ms | -0.14% |
| OptimizedCpu, 1,919 ticks + finish | 12.654 ms | 13.923 ms | +10.03% |
| Reference, 1,919 ticks + finish | 70.006 ms | 71.530 ms | +2.18% |

The initial host implementation allocated fresh snapshot vectors on every
tick and measured about 12% overhead on the pre-finish OptimizedCpu workload.
Reusable snapshot storage now preserves vector capacity, reducing that steady
state overhead to 2.13%. CUDA performs its extra replay only for candidates
that finished, so non-finishing timeline candidates do not pay per-tick
snapshot or probe costs.
