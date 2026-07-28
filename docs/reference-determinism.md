# Reference determinism against TrackMania United Forever

## Scope and authority

This worktree was created from commit `78d55bbf09904eb7455c47430fa3905ea5c5466a` on branch `goal/reference-determinism-20260727`. Reference is the parity and performance target. Shared replay, control, and scene-construction corrections also apply to the other backends; the checkpoint-contact rule is enabled only for a resolved Reference leaf, including Batched Reference. Existing Optimized CPU and CUDA tests remain in the verification suite, but this document does not claim non-Reference corpus parity.

Where `/validatepath` emits a terminal result, authoritative game classification and timing come from the canonical TrackMania United Forever installation at `/home/mikael/Games/trackmania-united-forever/drive_c/Program Files (x86)/TmUnitedForever` under the canonical Wine prefix. Reverse-engineering evidence comes from `/home/mikael/tmnfreverse`. Replays and map assets are read from the user-provided corpora and `/home/mikael/TmUnitedForever/Packs`.

No replay-, map-, UID-, or corpus-specific behavior is permitted. No widened tolerance, recorded-finish acceptance, or validity shortcut is permitted. TMUF's omission of unresolved authored placements is explicit, identity-tracked, and counted rather than implemented as a silent asset fallback.

## Baseline

### Build and tests

- Configuration: Release, tests enabled, developer benchmarks enabled, CUDA disabled.
- Build directory: `build-reference-determinism`.
- Existing test suite: 12/12 passed before any correction.
- Baseline commit: `78d55bbf09904eb7455c47430fa3905ea5c5466a`.
- Preserved baseline executable SHA-256: `61cc1b3d082f9309c68db60044597b90813960ec7a7fbcfeca5e4043e1189a64`.
- Preserved baseline benchmark SHA-256: `0ba6da34aaae3159533fffdae0068e28ac15f5974eddef458c7e51c10453d35f`.

### Stadium screening gate

The requested 2,169-replay Stadium gate is `/home/mikael/Downloads/massvalidation (1)/massvalidation/stadium/kacky`.

Reference-only, single-process-per-shard screening result:

- Replays: 2,169
- Reference valid: 2,138
- Reference race-time mismatch: 31
- Decode, map-load, and execution errors: 0
- Every mismatch is a race replay for which Reference deterministically reports no finish.

The complete lightweight screen was run in six parallel shard processes without changing per-replay semantics. Exact JSON output is retained under `/tmp/reference-baseline-kacky-shards`.

### Canonical game disposition of the Reference failure frontier

The exact 31 replay bytes were staged into an isolated canonical-profile replay directory and validated together by United Forever using:

`/usr/bin/wine TmForever.exe /useexedir /profile=mikael /validatepath=fv-oracle-kacky-reference-baseline\`

Authority hashes and result:

- `TmForever.exe` SHA-256: `4b6a7b31d86766409e94101f1256cd61dffecb23ea497a40b6617506b2ced2d4`
- Wine SHA-256: `8aaef00f60acda8662819b753aa09f4fd531f2198fd79f6b4cb26c2b85cb6157`
- wineserver SHA-256: `a10ea94d3a590e30ec503d6cea0c697a4d5a3d1b78978c46f01f2cb493f1b566`
- Parsed: 31
- Valid: 31
- Invalid: 0
- Wrong simulation: 0
- Incompatible: 0
- Load failures: 0
- Puzzle: 0
- Game and wineserver exit status: 0
- Timeout: no
- Input bytes were hash-checked before and after validation.

The preserved evidence is retained under `/tmp/reference-game-oracle-kacky-baseline`.

### Deterministic repeated-run evidence

All 31 Reference failures were validated as same-process duplicate requests. For every replay, the serialized validation status, simulation outcome, first-divergence field, and first-exact-deviation field remained identical. The executable reported no same-process determinism mismatch. This was a validation-report repeat, not a private runtime-clone hash. Evidence is retained under `/tmp/reference-repeat-kacky-failures`.

### Initial performance baseline

A 101-sample Reference benchmark on `6/557.Replay.Gbx`, with 3,000 iterations and five warmups, produced deterministic fingerprint `5881665736893707250` and checksum `738214917290675611`. The unpinned timing sample was scheduler-noisy (median 200,157,572 ns; minimum 83,980,401 ns; maximum 639,930,696 ns), so it is retained only as a state/performance sanity baseline. The exact output is retained at `/tmp/reference-baseline-performance-557-3000x101.txt`. The final gate below instead uses interleaved baseline/current binaries under CPU affinity and a paired statistical comparison.

## Baseline Reference failure frontier

- `2/125.Replay.Gbx` — SHA-256 `ecd63182e0831e288258112d221dee0fe18ebccaab3c5bb3657dbc989f62320a`, expected/input 339860 ms, respawns 20
- `3/248.Replay.Gbx` — SHA-256 `0dd816aa2d7892e841a3d0bf48e662f44c63443fcb861cb80a402389c79fcd18`, expected/input 6480 ms, respawns 0
- `3/264.Replay.Gbx` — SHA-256 `b440a4e14f183ff1b97dacef68b0d946a0d683c599a6bde8ee6374f26b3d8a8e`, expected/input 6180 ms, respawns 0
- `3/267.Replay.Gbx` — SHA-256 `9f690446d9163ec735d2f6f8d4867229607fe5da5a24f56823e4b0a0d19405d6`, expected/input 7590 ms, respawns 0
- `3/280.Replay.Gbx` — SHA-256 `772216c1712d06e4559884cbe230e058c122482b44c808d94617d9bf0687209c`, expected/input 37890 ms, respawns 0
- `6/557.Replay.Gbx` — SHA-256 `c6e41ad8ca235eb578530c188e899fa9a5dc43066f8d4d5c27b2f3f92a6e4875`, expected/input 31640 ms, respawns 0
- `6/567.Replay.Gbx` — SHA-256 `bb7ed734a668ee907c838e4370a3b6a127d2d08ae4420a8364e91aa6ef2098a8`, expected/input 15810 ms, respawns 0
- `6/579.Replay.Gbx` — SHA-256 `87c9bab5de92347e0182b40e82568f1dca34ca8af021c21a840c0e47e942a335`, expected/input 100170 ms, respawns 0
- `6/659.Replay.Gbx` — SHA-256 `0e3bec9134be27967e4f508356c32916c35b92df72648c62b9537359ca7f72bd`, expected/input 33330 ms, respawns 0
- `7/806.Replay.Gbx` — SHA-256 `a9d5212bab9a6e045e4b4d2d6d6d9d966d7b5f1ac4b9737b0b4dd22d50808d30`, expected/input 19570 ms, respawns 0
- `7/810.Replay.Gbx` — SHA-256 `72e9c9f794a1eedd1a39594ec5eaaacdec28b6d2bdc1fba932bc4b2ed479e990`, expected/input 30320 ms, respawns 0
- `8/853.Replay.Gbx` — SHA-256 `e7272311a1c2746a74c8c75da0b5125d9706c6891a0237b049b12765889ecaca`, expected/input 58010 ms, respawns 0
- `9/1023.Replay.Gbx` — SHA-256 `aeb92c130dfb7c91c9e92d8952b6b0fadf060c6e33c1ff87d1b2ad550553f0f0`, expected/input 16470 ms, respawns 0
- `10/1120.Replay.Gbx` — SHA-256 `2b0cf424c8b022599b064c04256df8fb4bcc13913ae971a680276a40f68f4c77`, expected/input 43710 ms, respawns 0
- `10/1157.Replay.Gbx` — SHA-256 `21e9d37b18548d342dd5ec5ba9a7d6a28fe791fb4b00092cd17422165793d05d`, expected/input 42480 ms, respawns 0
- `10/1160.Replay.Gbx` — SHA-256 `ca4294777fc60c4caa11243f3d8f61140d844523b4235849be2bd6350f7cd2ba`, expected/input 35570 ms, respawns 0
- `10/1164.Replay.Gbx` — SHA-256 `7834453817c0ca7cb9ce08636c2959cc3003f84e85c0003d82764d2498b56a95`, expected/input 31380 ms, respawns 0
- `10/1165.Replay.Gbx` — SHA-256 `663554759c76d0d60ef17fbe3c8dfebfcbd251c14cfd8abe54d45f090ffed2d6`, expected/input 33980 ms, respawns 0
- `10/1169.Replay.Gbx` — SHA-256 `989002fcc692ebdddb701800d23a1da21a61ee7b3aebf37af1abfb20eb792349`, expected/input 39680 ms, respawns 0
- `10/1170.Replay.Gbx` — SHA-256 `dd322c62291c5f3081972f8a6d33d2d34d948c6a3c69bb5a68f010ee9b1028ab`, expected/input 23550 ms, respawns 0
- `10/1225.Replay.Gbx` — SHA-256 `17604f8a6ebe0045327388765e0959bb88a7ee3401b18ea71cd677c26fc4f13c`, expected/input 27280 ms, respawns 0
- `11/1271.Replay.Gbx` — SHA-256 `25d17f011bf321c257f9d7270e0ae1e63a32964f025f4dd52971a44e864cc41b`, expected/input 116100 ms, respawns 0
- `11/1365.Replay.Gbx` — SHA-256 `0dc5d9d26130bbce9ba9b8374ae3cd95fd8d12d8ef15d9e554a0da08d5bdb2c2`, expected/input 59970 ms, respawns 0
- `11/1367.Replay.Gbx` — SHA-256 `cba8589935860c2b52279b6a90d2b2c607a6dbf7ee1e58a1e0a159514e52e012`, expected/input 117480 ms, respawns 0
- `12/1455.Replay.Gbx` — SHA-256 `1bd6d61688065995413e10ea3c5383fc6d57eb6c8f094d4ab40b990ca59aca6d`, expected/input 4730 ms, respawns 0
- `12/1499.Replay.Gbx` — SHA-256 `98e8379c1a804904aceeb7c176db2f29705cdee2be9080ce13a4e92864020e6b`, expected/input 136870 ms, respawns 0
- `13/1557.Replay.Gbx` — SHA-256 `d1167a6a554b95c2beaeb3ac54d7b78129c4793f66268faf0a38813dfd6fcc3d`, expected/input 6060 ms, respawns 0
- `14/1786.Replay.Gbx` — SHA-256 `7f15585336e19363e6225fc5289cc1ba4cdc3c810f3ac20f176acc734b418d91`, expected/input 33840 ms, respawns 0
- `14/1805.Replay.Gbx` — SHA-256 `b48e1f8293605d5b439705c0b308083a52ec25a0b359e8b4852376d42120bcb4`, expected/input 36180 ms, respawns 0
- `15/1814.Replay.Gbx` — SHA-256 `d99da65ce44ecff74eb8b6398060d056c81c02e6fe822eb1ec52eef4cbdf4754`, expected/input 35760 ms, respawns 0
- `15/1816.Replay.Gbx` — SHA-256 `cbe0b6f0955bc13ba1713ea462f9e296ec2dd3f794afbe75e07cb614cc52aae1`, expected/input 34790 ms, respawns 0

## Proven corrections

### Resolve deferred clip junction sources before construction

- Commit: `ac3c2b08484cdf2212bbb2268a35c9c38b3b1ed5`
- Seed replay: `12/1455.Replay.Gbx`
- Phase-aligned divergence: during authored scene construction, United's ground `StadiumGrassClip` at `(20, 1, 16)` installed four collision components. Reference installed only the two default 3-vertex/1-triangle GrassClip components and omitted sides 1 and 3. United loaded both missing sides directly from the standalone `StadiumRoadTiltClipRight` ground solid: 217 vertices, 403 triangles, 578 octree cells, and 5 materials.
- Root cause: block-unit junctions were initially materialized as source-asset placeholders. Reference resolved a placeholder only when that unit happened to be the final spatial neighbour selected for a placed clip. United can construct a clip before later overlapping field units replace that neighbour, so the live construction path must be able to read the canonical junction clip immediately. The unresolved placeholder had no mobil and silently removed the two collision components.
- Correction: resolve every deferred junction source in both mobil families of every authored and construction-zone block info before challenge construction. This is asset-graph resolution, not replay-specific behavior.
- Focused regression: `TestClipJunctionSourceResolution` constructs an unresolved unit junction and proves it is rebound to the canonical clip with its mobil intact. The complete 12-test suite passes.
- Seed result: Reference changed from deterministic no-finish to exact valid at 4,730 ms, matching United and the replay metadata.
- Affected replay set: exact-valid finishes were restored for `6/557`, `6/567`, `6/579`, `7/806`, `7/810`, `9/1023`, `10/1225`, `11/1271`, `12/1455`, and `12/1499`.
- Stadium gate after correction: 2,169 parsed, 2,148 valid, 21 race-time mismatches, 0 decode/map-load/execution errors, and no new failures.
- Evidence: `/tmp/reference-resolved-junction-1455.jsonl`, `/tmp/reference-resolved-blockinfo-1455.tsv`, and `/tmp/reference-ac3c2b0-kacky-shards`.
- Final corpus, deterministic-repeat, and performance gates are recorded below.

### Append the native race-validation tail tick

- Commit: `1587b40`
- Seed replay: `13/1557.Replay.Gbx`
- Phase-aligned divergence: Reference and United are bit-identical through the complete recorded replay horizon. United then performs one more physics tick with the final persistent controls and reaches the finish trigger; Reference previously stopped immediately after the final recorded tick.
- Root cause: race validation omitted United's unobserved trailing simulation tick. This is a validation-timeline rule, not finish-result injection: the finish still has to arise from collision simulation.
- Correction: race validation appends one copy of the final control tick, advances its time by one scheme period, marks it unobserved, and clears comparison data and one-shot race actions. Physics-sandbox timelines retain their previous bounds.
- Focused regression: `TestOptionalUnobservedTrailingTick` proves that the ordinary plan remains unchanged, the requested tail preserves persistent controls, time advances by one period, and finish, spawn, reset, respawn, and comparison actions cannot fire twice. The complete 13-test suite passes.
- Affected replay set: exact-valid classification was restored for `3/248`, `3/264`, `3/267`, `11/1367`, `13/1557`, and `15/1816`.
- Stadium gate after correction: 2,169 parsed, 2,154 valid, 15 race-time mismatches, 0 decode/map-load/execution errors, and no new failures.
- Evidence: `/tmp/reference-1587b40-kacky-shards`.
- Final corpus, deterministic-repeat, and performance gates are recorded below.

### Clear freewheeling on every current-transform checkpoint contact

- Commit: `cd1c509fc5a445e81869def7749802067a6a6737`
- Seed replay: `10/1120.Replay.Gbx`
- Phase-aligned divergence: on repeated contact with the already-passed current-transform checkpoint, United cleared the vehicle's freewheeling state before checking checkpoint-slot acceptance. Reference performed the clear only inside the accepted-checkpoint path, so it retained freewheeling and diverged in the next force update.
- Root cause: United's `RespawnUsesCurrentTransform()` checkpoint handler invokes `VehicleFreeWheelingSet(0)` for every trigger contact, before the slot-acceptance test. Reference had placed the operation inside `InternalOnCheckpoint`, after that test.
- Correction: perform the pre-acceptance clear for current-transform checkpoint contacts. A runtime policy enables this behavior only for a resolved Reference leaf backend; Batched Reference inherits it, while Optimized CPU, Speculative, and CUDA behavior remains unchanged.
- Focused regression: `forevervalidator-checkpoint-freewheel` proves enabled and disabled current-transform contacts, repeated already-passed contacts, ordinary checkpoints, and direct Reference-versus-Optimized routing. The complete 14-test suite passes.
- Direct state evidence: replay `10/1120` matches United for all 4,372 compared ticks and all 83,068 floating-point state components after the correction.
- Affected replay set: valid simulation-derived finishes were restored for `2/125`, `3/280`, `6/659`, `8/853`, `10/1120`, `10/1157`, `10/1160`, `10/1164`, `10/1165`, `10/1169`, `10/1170`, `11/1365`, `14/1786`, `14/1805`, and `15/1814`.
- Stadium gate after correction: 2,169 parsed, 2,169 valid, 0 mismatches, and 0 decode/map-load/execution errors.
- Evidence: focused replay results under `/tmp/reference-checkpoint-freewheel-gated` and the complete gate under `/tmp/reference-cd1c509-kacky-shards`.
- Final corpus, deterministic-repeat, and performance gates are recorded below.

### Ignore respawn commands before the race starts

- Commit: `0bbbf3c`
- Seed replay: `united/5/4504632.Replay.Gbx`
- Direct evidence: the replay contains 83 active respawn events, but one occurs at input time -7 ms while `RaceRunning` is false. Its embedded United result records 82 respawns. The remaining 82 active respawn events occur after the race starts.
- Root cause: the control-plan builder counted every active respawn event before applying its event, regardless of the current `RaceRunning` state. It therefore executed the pre-race command in the first simulation tick.
- Correction: schedule a respawn only when the replay control state is already race-running. Event ordering is retained, so a same-time respawn is accepted only after a preceding `RaceRunning` press has taken effect.
- Focused regression: `forevervalidator-replay-control-plan` includes a pre-race respawn and proves that only the post-start command is scheduled. The complete 14-test suite passes.
- Seed result: Reference changed from a simulation-derived 83 respawns to 82, with all 28 requested ghost states exact and a valid finish.
- Evidence: `/tmp/reference-pre-race-respawn-4504632.json`.
- Final corpus, deterministic-repeat, and performance gates are recorded below.

### Use the replay's serialized grid size for resolved decorations

- Commit: `b6cfa91de50c1558babc11931673ac31329b116c`
- Seed set: United replays `4000357`, `4000378`, `4500337`, `4503542`, `4503914`, `5000399`, `5002390`, `5004391`, and `5004393`.
- Direct evidence: each replay has exactly one installed decoration whose identity matches the serialized map, but the decoration archive records only its nominal base footprint. The replay header records the authored grid, which can be much larger; examples include a 100 x 100 x 100 Bay map using the nominal 45 x 36 x 45 Bay decoration and a 255 x 36 x 45 Island map using the nominal 45 x 36 x 45 Island decoration.
- Root cause: decoration resolution rejected a unique identity match when authored block coordinates exceeded the archive's nominal footprint. Leaving that nominal footprint in the selected definition also allocated an undersized challenge grid.
- Correction: retain identity-based uniqueness as the selection rule, reject ambiguous or zero-sized inputs, and replace the selected decoration's nominal footprint with the replay's serialized grid dimensions before challenge allocation.
- Focused regression: `forevervalidator-replay-decoration-size` proves that a unique 45 x 36 x 45 candidate resolves to a serialized 100 x 100 x 100 grid while preserving its other metadata, and that multiple candidates remain ambiguous. The complete 15-test suite passes.
- Seed result: `4000378`, `4503914`, `5000399`, and `5002390` now validate; the other five advance to the separately isolated cross-pack block boundary. No seed retains a decoration-size failure or crashes.
- Evidence: `/tmp/reference-decoration-size-correction5.6iXtaQ`.
- Final corpus, deterministic-repeat, and performance gates are recorded below.

### Omit unresolved authored block placements

- Commit: `46c26f8`
- Seed set: United replays `4000274`, `4000357`, `4000364`, `4000440`, `4500337`, `4503542`, `5004391`, `5004393`, and `5009760`.
- Direct evidence: each map contains at least one authored block name that does not resolve in the block's serialized collection. Canonical TMUF omits that placement. In `4000440`, constructing the foreign `BayFlatsRoad2` adds a road contact at simulation time 3,620 ms that TMUF does not have; omitting it is bit-exact with TMUF through all 7,258 traced state components and produces TMUF's wrong-simulation threshold crossing at ghost time 3,700 ms, distance `0.556241095`.
- Unknown-name control: replacing `IslandHotel` with the absent same-length identifier `QzNoBlk9999` leaves `4000274` canonically valid and bit-exact for all 12,388 traced components. Replacing `BayFlatsRoad2` with absent `ZayFlatsRoad2` leaves `4000440` canonically wrong-simulation rather than causing a load failure. The mutated replay SHA-256 values are `bcd608f1b53ea654c5e02527cbb696f06587a741b0085be94c8d546cf228e5a1` and `0720e2cc9ab0d07e0b38806687f6e7efb2a7423a8d3b95e4f6726fd3e11a3fee`.
- Root cause: Reference treated every unresolved authored placement as a scene-definition failure. Loading a same-named block from another collection is also incorrect: it constructs collision geometry absent from TMUF.
- Correction: record each unambiguous unresolved placement as explicitly skipped, omit it from scene units and challenge construction, retain its identity and missing-block count, and reject any downstream missing definition that was not explicitly skipped. Ambiguous matches and resolved-but-invalid assets remain failures.
- Focused regression: `forevervalidator-replay-scene-unresolved-blocks` builds a map containing one resolved block, one foreign-collection block, and one globally unknown block. It proves that only the resolved block reaches the scene and challenge, automatic-base construction still succeeds, no foreign asset is loaded, and both skipped identities and missing counts are retained. The complete 16-test suite passes.
- Seed result: `4000274` is valid with 66/66 exact observations and `4000364` is valid with 85/85 exact observations. `4000440` matches canonical wrong-simulation at 3,700 ms. The other six formerly failing maps now receive simulation-derived wrong-simulation classifications instead of construction errors. All nine runs complete with zero construction or execution errors.
- Evidence: `/tmp/united-skip-correction6`, `/tmp/united-skip-mismatch-diag-results`, `/tmp/tmuf-trace-4000440`, `/tmp/tmuf-trace-4000274-unknown-block`, and `/tmp/tmuf-trace-4000440-unknown-block-sentinel`.
- Final corpus, deterministic-repeat, and performance gates are recorded below.

## Final evidence scope

The final corpus gates use the Reference backend at commit `46c26f8208f3c130b45798e0528b8e1da666e426`. The executable SHA-256 is `776a448dd9463988f99e8e19807f3f651255c6ccc63e597460497bc90118d142`. The installed-pack manifest contains ten files and has SHA-256 `f908f56fa3aa3a4d47380aba070092ecc798cfc1ae0cd893fc3b0fbc2aa300e2`.

“Ordinary result” below means ForeverValidator's simulation-derived CLI result. “Canonical” is reserved for a direct `TmForever.exe /validatepath` result. Race completion and finish time come from the simulated race state after checkpoint and lap acceptance; recorded finish, checkpoint, respawn, and score outcomes are not injected.

## Final ordinary corpus results

| Corpus | Represented | Valid | Wrong simulation | Incompatible | Source-invalid | FV processing errors |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Stadium Kacky | 2,169/2,169 | 2,169 | 0 | 0 | 0 | 0 |
| United | 3,745/3,745 | 3,734 | 7 | 1 | 3 | 0 |

The Stadium gate contains 2,169 Race/Stadium results. Every simulated race completes. The simulated finish is equal to the embedded time for 44 replays and one native 10 ms tail tick later for 2,125, inside TMUF's validation rule. This corpus requests no ghost-state samples, and its embedded respawn field uses the unavailable sentinel, so neither is presented as an equality result.

The United gate contains 3,742 JSON results and three logged source errors. Its JSON modes are 3,338 Race, 323 Stunts, 50 Puzzle, and 31 Platform across all seven environments. All 3,734 valid results have equal measured, expected, and bit-exact observation counts, totaling 2,376,615 exact observations with maximum deviation zero. All valid simulations complete; respawns match for 3,734/3,734 and Stunts scores for 323/323.

The seven `wrong_simulation` results are `4000357`, `4000440`, `4500337`, `4503542`, `5004391`, `5004393`, and `5009760`. Replay `4000362` is a `TMr.6` input and is classified `incompatible_replay_version`. These are ordinary simulation-derived dispositions; only `4000440` has a strict terminal TMUF wrong-simulation result.

The three source-invalid inputs are also not implementation failures:

| Replay | Reason | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| `6/5000431.Replay.Gbx` | JPEG, not a replay container | 5,116 | `c1d27479d231b89684a4097e9aac3d9610e850c90c98a0e661025f1d362ac744` |
| `7/5002044.Replay.Gbx` | Challenge-only GBX, no replay header | 47,768 | `aaac2db693049f5b39909fd25509d7f6da55efa4238833ec6bf27ffd09f66e96` |
| `8/5003362.Replay.Gbx` | Replay wrapper with an invalid embedded map | 20,356 | `da133045a39538f124843c916f64d12a7917b54863ec74691bdf1804e7a4b95a` |

| Evidence SHA-256 | Stadium | United |
| --- | --- | --- |
| Corpus input manifest | `36f05af0418881e0432897707b87b982cc57853db4a831a351c942cac8a566c6` | `ff6aa391007a050527cada02b2b7920c8c75b8d47397666ddb4968d4194c84b9` |
| Ordinary-run manifest | `db9d932c5a373e427072e95999b369efcf89e0fd793a16769cfcebf357b4b376` | `4af436d59674d31d0c3fa1c16a87730367fd1fbcbb60a5296a19e1bffcb8d710` |
| Exact result bytes | `1f286d43ccd2b1fed8aa8096601657c822e7ccf5c87a3fa146a0a690b293f550` | `fba931844721d74c138ee36641dbb69810afd4bf72b0ed8e206904a878abd583` |
| Canonicalized-JSON manifest | `ba890bb493e7e85037d294db701e396907dbe861e765d5f0c2284c3c04bc33f0` | `cb47284b9c5321223e1af1b8fa2ef14b895b949a9693727c12c11b9abb6a2a9e` |
| Classifications | `e7d9a6939a5309e7663fe7e74e6327bc71d4281a4234ab253374299e14d0db01` | `5a173e3f1222deaad4e3950cafe35a7a29c0ceee14ed0a06dc21d4690aa45def` |
| Coverage outcomes | `49893ddfdf4e8940e35278091441e9427b1bc50c2a6ad87525e1190dece9f6d3` | `d4d462c942376ae7aa47f45d53c90d1f451626ca98190105ad5f67fb226de6e0` |
| Audit report | `53c1e0497129b94b1022d24cc14dfe1dbad7415dec6dd41267ee897ade989d98` | `609b8d8a107956c87f3b1692b6ba0d9b2cbd7e301fd8ddc3d4bfc4a7ae2d29c8` |

Both audits report zero coverage or integrity issues. The United source-only-error policy also passes. The final, status-aware audit harness SHA-256 is `e9f5075d8f22fc7e0a6f287a3f13adc30122819d9928b0f633f5779b87eb3289`; it produced the United report. The Stadium report predates only that invalid-status result-code check, which cannot affect its 2,169 all-valid results. Evidence is retained under `/tmp/reference-final-46c26f8-{stadium,united}-shards`, the corresponding `-manifests` directories, and the two `-audit.json` reports.

## Canonical TMUF distinctions

Canonical TMUF validates all 31 replays in the original Stadium Reference failure frontier. Direct canonical United runs produced these targeted dispositions:

| Canonical disposition | Replays |
| --- | --- |
| Strict terminal valid | `4000274`, `4000364`, `4504632` |
| Strict terminal wrong simulation | `4000440` |
| Strict terminal incompatible | `4000362` |
| Clean exit without a target terminal footer | `4000357`, `4000378`, `4500337`, `4503542`, `4503914`, `5000399`, `5002390`, `5004391`, `5004393`, `5009760` |
| Source-invalid, ignored, or empty target log | `5000431`, `5002044`, `5003362` |

Within the nonterminal group, ordinary Reference classifies `4000378`, `4503914`, `5000399`, and `5002390` valid, and the other six wrong simulation. Those are simulation-derived ordinary results, not canonical terminal confirmations. Raw canonical evidence is retained at `/tmp/reference-game-oracle-united-individual`.

The canonical authority SHA-256 values are `4b6a7b31d86766409e94101f1256cd61dffecb23ea497a40b6617506b2ced2d4` for `TmForever.exe`, `8aaef00f60acda8662819b753aa09f4fd531f2198fd79f6b4cb26c2b85cb6157` for Wine, and `a10ea94d3a590e30ec503d6cea0c697a4d5a3d1b78978c46f01f2cb493f1b566` for wineserver.

## Final correction table

| Commit | General correction | Focused regression | Result |
| --- | --- | --- | --- |
| `ac3c2b08484cdf2212bbb2268a35c9c38b3b1ed5` | Resolve deferred clip-junction sources before challenge construction | `forevervalidator-render-scene` / `TestClipJunctionSourceResolution` | Stadium 2,138 to 2,148 valid |
| `1587b40913c8a97aef7f2b55d499a7b38b52e93f` | Append TMUF's unobserved race-validation tail tick | `forevervalidator-replay-control-plan` / `TestOptionalUnobservedTrailingTick` | Stadium 2,148 to 2,154 valid |
| `cd1c509fc5a445e81869def7749802067a6a6737` | Clear freewheel on every current-transform checkpoint contact | `forevervalidator-checkpoint-freewheel` | Stadium 2,154 to 2,169 valid |
| `0bbbf3c57ed8f7db9756b196521e1d2fcc56fb22` | Ignore respawn commands before race start | `forevervalidator-replay-control-plan` pre-race case | `4504632`: 82 simulated respawns, valid |
| `b6cfa91de50c1558babc11931673ac31329b116c` | Use the replay's serialized grid for a uniquely resolved decoration | `forevervalidator-replay-decoration-size` | Four frontier replays became valid; five reached the next isolated boundary |
| `46c26f8208f3c130b45798e0528b8e1da666e426` | Explicitly omit and account for unresolved authored block placements | `forevervalidator-replay-scene-unresolved-blocks` | Final United disposition above; zero processing or construction errors |

The final configured test suite passes 16/16.

## Final same-process determinism

The gate covers all 5,914 replays. It runs five emitted copies of every replay; `--repeat-same-process` validates each copy twice in one process, for 59,140 validations. All five emitted result JSON or source-error records are byte-identical per replay. Each hidden pair has the same exit code, and JSON-bearing pairs also have identical serialization; the CLI does not expose the second source-error diagnostic for comparison. Every normalized public-report projection is stable, and every emitted result is exactly equal to its ordinary gate.

Stadium remains 2,169 `valid`; its evidence aggregate SHA-256 is `e3bb85c26a7218b78f8f00901653ba1757c24101421ab0ff3dfffee7b92999ef`. United remains 3,734 `valid`, seven `wrong_simulation`, one `incompatible_replay_version`, two invalid-container sources, and one invalid-map source; its evidence aggregate is `7edbfbe62d17f1548712b5cba847d821b69cb183e30e9a83db38c0d1ad82c9dc`. The combined aggregate is `f5c5253dddcca96da4a24eda5bb00c24f17f5ad025ab203aa01f8b55e144ba11`.

The runner SHA-256 is `f8835067de8ecc8c1f1af3d6e89e15141e79f47f837878633afbe6a3f4e485fb`; the audited manifest pins the same commit, binary, packs, and corpus hashes before and after the run. The summary SHA-256 is `3db6c1ea98cc74f2b42786f794e7fd55081d279738308d698012ac50c08d0ae8`, and the per-replay projection table SHA-256 is `b1e6839dd42e99b34cce000d38788c1e4d202ec090294189ad4b31aea9f5709c`. These are report-evidence hashes, not a private full runtime-clone hash. Evidence is retained at `/tmp/reference-final-46c26f8-determinism-10x`.

## Final Reference performance

The final gate pins CPU 7 and its SMT sibling 1, alternates 31 adjacent AB/BA pairs, advances 3,000 ticks, uses five warmups and 11 measured repetitions per invocation, and retains all 341 measured timings per build. Its recorded preflight idle fractions are 96.52% and 91.00%.

The primary median paired current/baseline ratio is `1.0021475003`, a `+0.214750%` current-time delta. Under independent paired ratios with a common population median, the exact order-statistic interval is `[0.9997498509, 1.0043722766]` at 97.0551% nominal coverage; the two-sided paired sign-test p-value is `0.0707555` (21 above, 10 below, no ties). The interval includes 1.0, so this run does not establish a performance change at the 5% level. Descriptive pooled medians are 62,720,110 ns for baseline and 62,858,864 ns for current; the paired estimator remains authoritative.

Every one of 496 restored state advances per build is internally stable. The baseline fingerprint/checksum pair is `5881665736893707250` / `2614364862972708675`; current is `12475912606587678513` / `7603239607473123`. The fingerprint is a semantic projection containing tick/time, environment and mode, vehicle motion and forces, controls, checkpoints, laps, completion, finish time, respawns, and score; it does not include `durationMs`. Cross-build fingerprint equality is not expected because the corrections intentionally change Reference state.

The preserved baseline benchmark SHA-256/build ID is `0ba6da34aaae3159533fffdae0068e28ac15f5974eddef458c7e51c10453d35f` / `999f1fe9b37edd41b379bcb9ecaa390084238941`. The current value is `8d3b2cfbd25a70fcc84c286f94f7c7433dbf9b735a67862e81e39e6b2493a7ae` / `fd07431e26c6e9c6b35ed057e5b095d12c152414`. The runner SHA-256 is `fc01551d1a3ebcfc859aa346af371b41a6affe247524089f8f4f6aeda9522c73`; all authorities were rehashed after the run and no conflicting process remained. The complete evidence SHA-256 is `19301d9a096ed3e334f21936f762deb055edc2e751e7d7442a6a91252ef58275` at `/tmp/reference-final-performance-557-interleaved.json`.

## Reproduction

```bash
cmake -S . -B build-reference-determinism \
  -DCMAKE_BUILD_TYPE=Release \
  -DFOREVERVALIDATOR_BUILD_TESTS=ON \
  -DFOREVERVALIDATOR_BUILD_BENCHMARKS=ON \
  -DFOREVERVALIDATOR_BUILD_DEVELOPER_BENCHMARKS=ON \
  -DFOREVERVALIDATOR_ENABLE_CUDA=OFF
cmake --build build-reference-determinism -j
ctest --test-dir build-reference-determinism --output-on-failure
```

`FOREVERVALIDATOR_BUILD_BENCHMARKS` is the project option that creates the benchmark target. The additional `FOREVERVALIDATOR_BUILD_DEVELOPER_BENCHMARKS` entry is an unused cache sentinel retained because the exact archived performance runner pins the original build cache.

Run this block with a new, empty `RESULT_ROOT`: once with United and shards 1-12, then once with Stadium Kacky and shards 1-16. It preserves each shard's CLI summary, detailed log, exit status, and result files, then writes the ordinary-root authority manifest required by the repeat runner:

```bash
corpus_root=CORPUS_ROOT
result_root=RESULT_ROOT
shards="$(seq 1 12)"
for shard in $shards; do
  mkdir -p "$result_root/$shard/results"
  set +e
  build-reference-determinism/forevervalidator \
    --pak-dir /home/mikael/TmUnitedForever/Packs \
    --backend reference \
    --out-dir "$result_root/$shard/results" "$corpus_root/$shard" \
    >"$result_root/$shard/stdout.json" \
    2>"$result_root/$shard/run.log"
  status=$?
  set -e
  printf '%s\n' "$status" >"$result_root/$shard/exit.txt"
done

python3 - "$result_root" "$corpus_root" $shards <<'PY'
import hashlib
import json
from pathlib import Path
import subprocess
import sys

binary = Path("build-reference-determinism/forevervalidator").resolve()
paks = Path("/home/mikael/TmUnitedForever/Packs").resolve()
digest = hashlib.sha256(binary.read_bytes()).hexdigest()
manifest = {
    "schema": "forevervalidator-reference-corpus-shards-v1",
    "commit": subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip(),
    "binary": str(binary),
    "binary_sha256": digest,
    "backend": "reference",
    "corpus": str(Path(sys.argv[2]).resolve()),
    "pak_dir": str(paks),
    "shards": [int(value) for value in sys.argv[3:]],
}
(Path(sys.argv[1]) / "manifest.json").write_text(
    json.dumps(manifest, indent=2, sort_keys=True) + "\n",
    encoding="utf-8",
)
PY
```

The final audit commands are:

```bash
python3 tools/reference_evidence/audit_forevervalidator_shards.py \
  "/home/mikael/Downloads/massvalidation (1)/massvalidation/stadium/kacky" \
  /tmp/reference-final-46c26f8-stadium-shards \
  --report /tmp/reference-final-46c26f8-stadium-audit.json \
  --manifest-dir /tmp/reference-final-46c26f8-stadium-manifests \
  --fail-on-any-error

python3 tools/reference_evidence/audit_forevervalidator_shards.py \
  "/home/mikael/Downloads/massvalidation (1)/massvalidation/united" \
  /tmp/reference-final-46c26f8-united-shards \
  --report /tmp/reference-final-46c26f8-united-audit.json \
  --manifest-dir /tmp/reference-final-46c26f8-united-manifests \
  --require-errors-source-only
```

Re-audit the preserved repeat evidence with `analyze`:

```bash
python3 tools/reference_evidence/run_reference_final_determinism_gate.py analyze \
  /tmp/reference-final-46c26f8-determinism-10x \
  --ordinary-united /tmp/reference-final-46c26f8-united-shards \
  --ordinary-stadium /tmp/reference-final-46c26f8-stadium-shards
```

A fresh repeat run intentionally requires the two ordinary manifests to name the current `HEAD` and binary exactly:

```bash
python3 tools/reference_evidence/run_reference_final_determinism_gate.py run \
  --root FRESH_REPEAT_ROOT \
  --workers 12 \
  --emitted-repeats 5 \
  --ordinary-united FRESH_ORDINARY_UNITED_ROOT \
  --ordinary-stadium FRESH_ORDINARY_STADIUM_ROOT
```

For a fresh performance run, first snapshot the current build and choose a nonexistent evidence path:

```bash
install -D -m755 build-reference-determinism/optimized_cpu_benchmark \
  /tmp/reference-final-bin/optimized_cpu_benchmark
python3 tools/reference_evidence/run_reference_baseline_current_interleaved.py \
  --cpu 7 \
  --minimum-idle-fraction 0.85 \
  --output FRESH_PERFORMANCE_JSON
```
