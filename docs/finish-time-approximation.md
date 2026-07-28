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
classification rules are otherwise unchanged.

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
