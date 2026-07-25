# SpeculativeTicking

`SpeculativeTicking` is an exact deterministic backend built on the
`OptimizedCpu` physics implementation. Its purpose is to execute many logical
ticks per synchronization point without changing ForeverValidator's accepted
physics results.

## Execution model

The backend advances a replay in speculative blocks:

1. A cheap drafter predicts a sequence of complete continuation states.
2. For every drafted state, the authoritative one-tick transition is evaluated
   independently. These transitions can run in parallel across both ticks and
   ForeverTAS search candidates, with GPU execution as the intended target.
3. Each exact transition result is compared bit-for-bit with the next drafted
   state.
4. The longest consecutive matching prefix is committed at once.
5. On the first mismatch, the verifier's exact result becomes the corrected
   continuation state and all later work from that speculative block is
   discarded.

The verifier can run transitions in parallel because the drafter supplies the
otherwise sequential intermediate states. A rejected transition never affects
the committed simulation.

## State and correctness

A draft state must contain every mutable value that can affect future ticks,
not only the visible car transform. This includes dynamic-body state, vehicle
and wheel internals, controls, race progress, timers, respawn state, and other
history-dependent simulation data. Immutable scene geometry and tuning data are
shared by all transitions; derived caches may be rebuilt deterministically.

Acceptance is exact. Floats, integers, flags, counters, and all other semantic
state are compared using their deterministic representation. Approximate or
tolerance-based matches are never committed.

## Scheduling and fallback

Block length is adaptive. Long blocks are useful in predictable regions such as
stable contact or collision-free motion, while short blocks are used near input
changes, collisions, checkpoints, respawns, gear changes, and other uncertain
events. Work is batched across candidate timelines so the verifier has enough
parallel transitions to keep the GPU occupied.

The optimized CPU path remains the authoritative fallback whenever drafting,
parallel verification, or device execution is unavailable or not profitable.
The current implementation provides the complete backend routing and executes
through that exact fallback; the drafter and parallel block verifier will be
added behind the existing `SpeculativeTicking` dispatch boundary.
