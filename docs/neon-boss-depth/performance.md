# Neon Boss Depth Encounter — performance and resources

## Baseline observations

The fresh existing Neon runtime suite records one same-pose comparison with 30 warmup and 120 measured frames per condition. Both CSVs have 120 valid GPU frames. Raw files are in `generated/neon-boss-depth/baseline/full-validation/artifacts/test_neon_boss_runtime/`; hashes and all metrics are summarized in `generated/neon-boss-depth/baseline/legacy-same-pose-profile-summary.json`.

| Metric (ms) | Legacy representation mean / p95 | Existing small Neon visual mean / p95 |
| --- | --- | --- |
| Frame elapsed, limited | 16.698 / 16.848 | 16.678 / 16.737 |
| CPU work estimate | 5.094 / 5.964 | 4.969 / 5.734 |
| GPU frame | 3.234 / 4.473 | 4.492 / 4.608 |
| Neon Boss GPU scope | absent | 0.794 / 0.940 |

These are Development, frozen-pose, existing comparison temporal settings measurements. The CPU variation does not establish an improvement. Limited elapsed time is not an uncapped throughput measure. The difference between whole-frame conditions includes the old/new representation and post-processing cost; it is not identical to the isolated Neon scope. This is a baseline only, with no Depth body, new attacks or full-cycle performance claim.

The existing runtime suite passed one create/release, stable descriptor counts across same-pose toggles, one draw constant-buffer arena slot under comparison freeze, and no retained visual buffers at terminal Dissolve. Shared warm texture/mask caches intentionally persist. The frozen raw evidence belongs to the pre-change source and executable hashes.

## Completed final Depth measurement

The four serial runs OFF1→ON1→ON2→OFF2 completed in `generated/neon-boss-depth/final-performance/perf_a_20261005/`. The actual run manifest and summary both report COMPLETE. An independent CPU read audit of the original JSON/CSV, source hashes and resource inputs passed; it recalculated every reported mean/median/p95/range and paired delta without rerunning the game or changing original evidence. Raw CSVs, per-frame records and settings are under each run’s `evidence/` directory.

All four use the final All Development EXE SHA256 `931EFCF9D8E12C4D78323B769C4EF65F3B89A4BE8A3522CBD0B8BE6BCFED8480`. The four frozen binary files and 257 runtime resource files match their source copies and exact manifests. Each run uses seed 20261005, fixed 1/60 timestep, the standard 20°/distance 90/focusY 31 Depth camera, 1280×720 rendering, the NVIDIA GeForce RTX 4060 Laptop GPU and the existing 60 Hz limiter. The stationary HP 120 player is invulnerable and boss HP 10,000 is a declared shortcut fixture; there are no extra enemies/projectiles, primary shooting, upgrades, Phase 2 HP injection, pause, comparison freeze, reduced-motion profile, PNG captures or continuous recording. OFF retains CPU model/palette updates, owned model/FX resources and canonical floor/core geometry while disabling the body and airborne decorative draws. This is the same Depth encounter under two display modes, not ordinary player survival or Release performance.

Each completed Session has 2,220 actual source rows: 900 warmup, 1,200 measured and 120 tail rows. The measured consecutive source frames are 901–2100. Actual last Intro is 205, so 695 completed positive gameplay warmup updates precede measurement after Intro. Warmup observes all three attacks and their clips; all three phase 1 Active and Recovery attacks occur in the measurement window. Actual full-session Gameplay and Gameplay/presentation clock traces agree across all four runs. No natural Phase 2 is measured here.

Every run has 1,200 finite CPU/frame samples and 1,199 valid GPU samples plus 1 invalid GPU frame. First measured CSV frame 1/source 901 is invalid and contributes no GPU rows; it is excluded from all GPU statistics. CPU work estimate is `max(0, elapsed − present − fence − limiter)`, verified from the actual CSV within its stored precision. The numbers below are mean / median / p95 in milliseconds; p95 uses the nearest rank of the observed samples.

| Run | CPU work estimate mean / median / p95 | GPU frame mean / median / p95 | Limited elapsed mean / median / p95 | GPU valid / invalid |
| --- | ---: | ---: | ---: | ---: |
| OFF1 | 6.498770 / 6.272700 / 8.224899 | 4.126320 / 4.148224 / 4.488192 | 16.708809 / 16.666800 / 16.859200 | 1199 / 1 |
| ON1 | 6.443544 / 6.181349 / 8.198201 | 4.841559 / 4.825088 / 4.993024 | 16.715337 / 16.666800 / 16.842899 | 1199 / 1 |
| ON2 | 6.484258 / 6.210050 / 8.132600 | 4.863342 / 4.849664 / 4.992000 | 16.734205 / 16.666800 / 16.855900 | 1199 / 1 |
| OFF2 | 6.447489 / 6.214700 / 8.226699 | 4.131638 / 4.155392 / 4.421632 | 16.710288 / 16.666800 / 16.860100 | 1199 / 1 |

The matching-repeat ON-minus-OFF whole-GPU median differences are +0.676864 ms and +0.694272 ms; p95 differences are +0.504832 ms and +0.570368 ms. CPU variation and frame pacing do not establish an improvement. Limited elapsed time is not uncapped throughput, and these two repetitions do not establish a universal performance budget.

Representative boss scopes below are independently measured mean / median / p95 milliseconds. The complete original summary contains all scopes and four runs. CPU `Neon Boss Update` includes its Model Update and FX Build children; overall GPU and Scene 3D scopes contain narrower scopes. Do not sum inclusive parent/child times. `Neon Depth FX Floor` includes floor geometry, grid capture and compositing, so it is not an isolated floor-vertex cost. The Air wrapper can emit an actual scope for an early-return draw; its observed zero median is not a fabricated missing-value zero. The OFF body GPU scope is absent and remains unavailable.

| Independent scope | ON1 mean / median / p95 | ON2 mean / median / p95 | OFF meaning |
| --- | ---: | ---: | --- |
| CPU Neon Boss Update | 0.173880 / 0.095650 / 0.709800 | 0.163700 / 0.095000 / 0.683600 | model/FX update retained |
| CPU Neon Depth Model Update | 0.041414 / 0.037400 / 0.057000 | 0.040693 / 0.036800 / 0.057400 | retained |
| CPU Neon Depth FX Build | 0.129821 / 0.056800 / 0.667900 | 0.120323 / 0.056500 / 0.643300 | retained |
| GPU Neon Boss body | 1.404802 / 1.395712 / 1.522688 | 1.370036 / 1.351680 / 1.483776 | absent / unavailable |
| GPU Neon Depth FX Air | 0.000274 / 0.000000 / 0.001024 | 0.000274 / 0.000000 / 0.001024 | wrapper scope; decorative draw disabled |
| GPU Neon Depth FX Floor | 0.262808 / 0.262144 / 0.281600 | 0.267129 / 0.266240 / 0.284672 | canonical floor/core retained |

## Actual resource observations

Every measured profiler model/FX counter matches its same-frame presentation telemetry. Both owned resource sets remain present, with creates 1/releases 0 and balance 1 each. Model/effect updates advance exactly once per measured source frame. Model draw constant-buffer count is flat at 0 for OFF and 1 for ON. Effect initialization, invalid-frame, capacity, binding and duplicate-update counts are 0; air+floor vertices match the prepared total and remain within 50,688. These are normal-cycle observations; terminal release/reuse remains a separate lifecycle gate in validation.md.

The total Scene SRV count is **not flat**. It includes whole-Scene lazy HUD/texture caches, distinct from the boss-owned resource invariants:

| Run | Scene SRV first → last | Observed shape |
| --- | ---: | --- |
| OFF1 | 353 → 354 | +1 between first/second measured rows; then flat |
| ON1 | 349 → 350 | +1 between first/second measured rows; then flat |
| ON2 | 352 → 353 | +1 between first/second measured rows; then flat |
| OFF2 | 353 → 354 | +1 between first/second measured rows; then flat |

This step coincides with the first GPU-invalid measurement and is consistent with the profiler’s loading-submission exclusion. The exact allocating asset was not independently identified. This bounded whole-Scene change does not establish an unbounded boss allocation leak; it is preserved as measured growth rather than reported as zero.

## Continuous recording whole-process measurement

The separate source-validated 45-second recording in `generated/neon-boss-depth/final-recording/recording_a_20261005/` saved 2,700 consecutive native PNG/JSON pairs during a 2,820-update Session. The parent observed the owned child process from immediately before launch through `WaitForExit` returning: **191.1304011 seconds**, exit 0, with **1,234,238,594 source bytes** and **14.126481 saved PNGs per whole-process second**. This is a throughput observation for the complete child process, including startup/resource initialization, all Session updates and tail rows, synchronous capture/I/O, report writes, scheduling and shutdown. Input preparation, post-exit hashing, finalization, encoding, playback and visual review are outside its timed interval. The 45 seconds is the source/encoded presentation duration; it is not the measured wall-clock duration.

The recording uses moving/firing/dashing Shooter input, actual HP loss and no debug invulnerability, unlike the stationary invulnerable OFF/ON fixture. Pure PNG save time, pure GPU readback time and ordinary rendering FPS were **not measured** by this stopwatch. No subtraction across these different input profiles is performed. The successful camera-reviewed receipt preserves the original failed receipt and binds the exact successful source audit; it does not turn a process-wide throughput number into an isolated capture-overhead estimate.

## Evidence and measurement limits

Original evidence: `generated/neon-boss-depth/final-performance/perf_a_20261005/run-manifest.json`, `summary.json`, `summary.md`, `binary-inputs.json`, `resource-inputs.json` and the four original run directories. Independent audit: `generated/neon-boss-depth/audit-validation/performance-a/receipt.json` and `receipt.md` (status `FINAL_PERFORMANCE_4_RUN_SEMANTIC_AUDIT_PASS`). The independent audit only read files and wrote ignored receipts; no second GPU/app run or evidence rewrite occurred.

| Bound evidence | SHA256 |
| --- | --- |
| Original run manifest | `0941961FA44AE801FF78DA8AAC734B02E4790584E47E261704834F5C53334CE3` |
| Original summary JSON | `A33306189E617DAF8F42A31CE4F395070477BAB7542684895D89B9F1350113A5` |
| Original resource-input manifest | `73CD594C854BCBB03286D06FED0DB9FAC944DE83B87B202D5EC4F94B489DE956` |
| Independent receipt JSON | `28743E2B2E839BF479E527774B9C5791D8769B3EEC1E43DB030A7CCD3D206223` |
| Recording whole-process receipt | `ED10615E04AE8A9CDD87156596215AD4F2D9B5603A041C87DE8577685A69D199` |
| Recording camera-reviewed source audit | `24C5E9B02EAE1A181534F6A3275498F01DAC4A71BFA253BB2752026100B3C403` |

Recording throughput evidence: `generated/neon-boss-depth/final-recording/recording_a_20261005/recording-wall-throughput-camera-reviewed.json` and `recording-audit-camera-reviewed.json`.

Recording overhead remains a separate measurement/limitation. PNG readback/save waits and capture costs are excluded from normal OFF/ON timing, and no recording-overhead value is inferred from these runs. Moving-player combat, natural Phase 2, ordinary Release traversal and human play are outside this fixture. The legacy table above uses another source/binary and different frozen-pose/temporal conditions; it remains a historical reference, not a final new-versus-old encounter performance improvement or an optimization claim.
