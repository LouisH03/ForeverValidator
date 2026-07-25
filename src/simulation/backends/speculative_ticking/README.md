# SpeculativeTicking

`SpeculativeTicking` is an exact deterministic backend built on the
`OptimizedCpu` physics implementation.

It advances a replay in blocks:

1. A cheap drafter predicts a sequence of complete continuation states.
2. Exact one-tick transitions from those predicted states are verified in
   parallel, targeting GPU execution across ticks and search candidates.
3. The longest bit-identical prefix is committed.
4. The first exact mismatch result becomes the corrected continuation state;
   later speculative work is discarded.

Only complete, bit-exact states are accepted. Static scene data is shared,
block length is selected from confidence and known event boundaries, and the
optimized CPU exact path remains the authoritative fallback whenever drafting
or parallel verification is unavailable or unprofitable.
