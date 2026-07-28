# Reference parity evidence tools

These scripts are the exact harness bytes used for the final TMUF Reference
evidence described in `docs/reference-determinism.md`:

- `audit_forevervalidator_shards.py` audits ordinary shard logs and result JSON.
- `run_reference_final_determinism_gate.py` performs and audits same-process
  repeats against pinned ordinary gates.
- `run_reference_baseline_current_interleaved.py` performs the pinned AB/BA
  Reference performance comparison.

They intentionally pin the local corpus, pack, binary, and baseline authorities
for this evidence set. Generated results, replay files, binaries, and traces stay
outside the repository. See the document's Reproduction section for commands and
the recorded SHA-256 of each harness.
