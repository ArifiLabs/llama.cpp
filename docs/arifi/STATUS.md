# ArifiLabs llama.cpp — engineering status

The [README](../../README.md) shows what works and what was won. This page is the full ledger:
every protected win with its honest state, every result that showed no gain, and what is still open.
Nothing here is hidden from the front page by accident; it is filed where engineers look.

## This release: b11178-x1i2 (2026-10-07)

### Defects in our previous release, fixed here

Both were found on the previous release's own binary, each with a failing test before the fix.

| Defect in the previous release | Proof | Fix | Cost of the fix |
|---|---|---|---|
| Loading Qwen3.8-Flash-Next drained the Windows side to the alarm line: a whole-file prefetch of the mapping ran even when the device copies the weights out of it | before the fix the Windows side fell to 0.19 / 1.3 GiB; after it, 6.1-7.4 GB stayed free over 4 loads | `llama-mmap`: skip the whole-file prefetch when a device copies weights out of the mapping | none measured (load path only) |
| An MTP checkpoint restore at a position not divisible by 4 wrote the wrong ring plane of the recurrent state | 3 of 10 restores wrong on the previous release; 0 of 10 after the fix, and 3 of 10 again with the fix planted out | ssm row save/restore maps to the physical ring plane | tie (-0.87% / -0.48%, n=4, inside the spread) |

### Output ids against the previous release

| File | Where the ids split | Which build moves | Receipt |
|---|---|---|---|
| S-X8 v4.3 27B, plain, ~600-token prompt | token 5 | the previous release: 369 or 6587 depending on the requests before it; this release 6587 every time | internal receipt `hist-10062340`, not shipped |
| Qwen3.8 27B UD-Q4_K_XL, plain, ~600-token prompt | token 92 | the previous release flips by round; this release is identical on every round and on 100 of 100 teacher-forced steps. Top-two gap 0.0588 nats served, 0.058 in engine logits: a near-tie | internal receipts `np5-10070038` and determinism cell 30, not shipped |
| Ornith-1.5 35B-A3B Q4_K_M, plain, ~600-token prompt | token 48 | the previous release changes between rounds; this release is round-stable | `evidence/b11178-x1i2/sg-orn-10062214-*` |
| Ornith-1.5 35B-A3B Q4_K_M, plain | token 53 (short), 58 (long) | this release, the same way every round: the masked-KV fix changes the last bits of the attention sum, with no history effect | `evidence/b11178-x1i2/sg-orn-10062214-*` |

The history dependence in the previous release is the masked-KV flash-attention defect this release fixes
(`GGML_VK_FA_DEADV_KEEP=1` restores the old path; with it, the 27B token-92 gap moves from 0.058 to 0.010 under history).

### Results that showed no gain, or are not finished

| Area | Item | Result |
|---|---|---|
| MTP drafting | strict byte-identical output, MTP against plain | open: the tie-aware check passes; the strict check is not re-run on this build |
| MTP drafting | a 106K-token draft head, draft depth 4, depths 1 and 2, the n-gram CAP0 gate (both versions), an n-gram n_min gate | rejected by their pre-registered rules |
| MTP drafting | MTP chained with n-gram drafting as the default | not the default: agent-loop repeats win, a cold pure copy is about -7% (n=3), open |
| Memory | embedding-row release, version 1 (one unlock per row) | rejected: -97% prefill |
| Memory | unbuffered ReadFile gather for embedding rows | not ported: the prefetch closes the cold-fault slice |
| Kernels | q6_K and lm_head verify mat-vecs | at 96-101% of the 78 GB/s memory roof; no lossless kernel cut is left |
| Recurrent state | single-write snapshot diagnostic | a byte-floor receipt, not a win (4.83 ms per 4-row verify; the diagnostic breaks rollback) |
| Recurrent state | deferred commit + replay (`LLAMA_GDN_REPLAY=1`) | built, opt-in, unmeasured |
| MTP drafting | `eh_proj` as one 2D matmul | profile only: -1.0 ms per round, -90 ms per 512-token MTP prefill; no served A/B |
| Memory | `LLAMA_PLE_PREFETCH=2` (prefetch during decode) | unmeasured |
| Fusion | epilogue folds | ceiling about 3% or less |
| Fusion | q8_1 activation reuse | dropped: already in our backend |
| Fusion | MUL_MAT_SCALE_SILU fusion | op test passes (planted fault fails); no served number; ships on (`GGML_VK_DISABLE_MM_SCALE_SILU` reverts) |
| Fusion | hyper-connection weights BF16 -> Q8_0 | a quality test only; no adoption, no speed claim |
| Kernels | a new bf16 GEMV kernel | rejected: the stock bf16 mat-vec already reads at 76-78 GB/s |
| Determinism | row-independent verify (a draft row's output independent of the verify width) | not fixed; the carriers are the width-keyed q8_1 MMVQ route and the mat-vec to GEMM switch at width 9 |
| Determinism | drafted output equal to plain output | dense 27B passes; Ornith-1.5 35B-A3B differs at positions 53 / 48 (target side) |
| Determinism | fixed output against the unfixed output | the fixed output equals neither unfixed run on Ornith; the fixed output is the new reference |
| Determinism | cost of the repeatable-decode fix | a tie at every verify width on both models |
| Determinism | GLM-5.3-Flash | not tried: the 117.5 GB file is larger than the 96 GB pool |
| Long context | CPU co-compute during prefill | dropped: about 2% ceiling; a 12-thread CPU loop costs GPU 8K prefill -6.6% |
| Long context | per-token full-mask fill, q8_0 sparse gather, a KV floor below 4102 | not ported: ceilings about 2-3% decode, no q8_0 serve line, dense wins below 8K |
| Long context | sparse prefill flash-attention kernel efficiency | 0.7 against 5.1 TFLOP/s dense; +12-15% prefill at KV >= 8K is derived, not built |
| Long context | bf16 prompt matmul via scaled f16 staging | ships on (`GGML_ARIFI_BF16_F16=0` reverts); its own served number is not in this release's receipts |
| MoE | multi-column expert gather (2 or 4 columns) | byte-identical but ALU-bound; loses at real routing |
| MoE | an earlier served gather A/B | failed its pre-registered rule (order effect); replaced by the balanced ABBA ×2 in the README |
| MoE | DFlash2 on GLM-5.3-Flash streaming | a tie once logging is off (chat +4% sync, -1% dual); an earlier "+62%" was logging-inflated and is withdrawn; break-even at 2.69 accepted tokens |
| MoE | expert cache hit rate on GLM-5.3-Flash | 10.5% in decode; the per-expert resident set is lost to whole-layer residency |
| S-X8 int8 | one-digit int8 on every role | +35% prompt reading, but fails perplexity by one outlier token (2.3 SE); 16-value activation scales failed the same way |
| S-X8 int8 | int8 below 40-token prompts | -29% (server); hence the float path below the 56-column gate |
| Kernels | RDNA3 small-n ffn_down int8 exception | default off (served `-ub 32`: Q4_K_XL 0.920x against 0.977x, GSQ 0.928x against 0.958x of the release before ours); `GGML_ARIFI_CM1_INT_SMALLN=down` restores it |
| Server | `--prio 2` | no gain under real load (-0.5%) |
| llama-quantize | slab reads on Windows | the output-bytes identity and the working-set figures are not in this release's shipped receipts; the README states the mechanism only |

## How the numbers were taken

- **Speed:** Beelink SER7 Pro, Ryzen 7 7840HS, Radeon 780M (RDNA3, gfx1103), Windows 11, Vulkan
  with `KHR_coopmat` and the Balanced power plan. Driver versions for the historical 780M runs are not reconciled. Served numbers are
  `llama-server` decode t/s over interleaved launches with a 95% confidence interval per paired cell;
  kernel numbers are `test-backend-ops perf` medians.
- **Statistics:** the README speed pairs are medians of 16 rounds per arm, except ROCmFP4-FAST, which
  shows means. Values are copied at the receipt's precision; percents are cut to one decimal, never
  rounded up.
- **Memory epochs on that box:** the early-September UD-Q3_K_XL, IQ4_XS and ROCmFP4-FAST runs are inferred to use 2x16 GB from roughly 15.5-15.7 GiB system-visible RAM in their load lines; the register's transition times conflict with those receipts. The ROCmFP4-FAST A/B started 2026-09-03 13:06:59. The S-X8 and Q4_K_XL rows (2026-09-14/15), the q5_K, q6_K, iq3 and q4_K kernel rows
  (2026-09-17 to 2026-09-24) and the latest-release tie (2026-09-24) were measured on 32+16 GB.
- **Baselines:** a served "before → after" compares two builds of this fork on the same file and box.
  A kernel row whose off switch restores the upstream path is a comparison with upstream.
- **Radeon 890M:** Minisforum AI X1 Pro-470, Ryzen AI 9 HX 470, 96 GB DDR5-5600 with 72 GB reserved for the GPU,
  Windows 11. The README's 890M table is one ABBA A/B per row (two passes, 4 rounds per arm, 8 for MTP arms, means).
  Its receipts and the receipts of the "What is new" rows are in
  [`evidence/b11178-x1i2/`](../../evidence/b11178-x1i2/) with their own `MANIFEST.md`.
  The v0.1.3.1 re-gate receipts (four models against v0.1.3.0, same output ids on every row) are in
  [`evidence/v0.1.3.1/`](../../evidence/v0.1.3.1/) with their own `MANIFEST.md`.
- **Receipts in this repository:** [`evidence/`](../../evidence/) with
  [`evidence/MANIFEST.md`](../../evidence/MANIFEST.md) (sha256 per file). These README figures have a
  shipped receipt: the latest-release tie (GSQ IQ3_S and Q4_K_XL), perplexity, bit-identity,
  `test-backend-ops` counts, the q4_K width-5 split, the iq3_s width-6 marginal and every 890M check.
  The other speed figures come from the protected-win manifest below or from unpublished benchmark receipts
  that are not shipped here: the README speed table (ROCmFP4-FAST, UD-Q3_K_XL, Q4_K_XL and S-X8 before
  and after), the S-X8 27B progression (1.831 t/s plain and a drafted line that failed at load on the
  2026-09-14 build, 5.551 t/s drafted at depth 4 on the 2026-09-15 build), the S-X8 kernel rows, the
  q6_K, q5_K and iq3 width-7 kernel rows.
- **Bench-tool numbers:** the TQ rows are `llama-bench` tg32 / tg64 results, not served `llama-server`
  numbers.

## Protected wins

[`tools/arifi-sync/protected-wins.json`](../../tools/arifi-sync/protected-wins.json) holds 61 entries.
`python tools/arifi-sync/arifi_sync.py protected-win validate --ref HEAD` checks them against the tree.
At this tip it reports PASS. The table below is the r86i-era list; the entries added since are in the manifest
itself, each with its measured effect.

| id | State | Measured effect (Radeon 780M unless noted) | Opt out |
|---|---|---|---|
| `vulkan-tq-matvec-subgroup` | win | `llama-bench`: 27B TQ3_4S tg32 0.67 → 1.80 t/s; 0.5B TQ4_1S tg64 44.69 → 126.65, TQ3_1S 45.46 → 107.10; kernel 3.1-3.4x | `GGML_VK_DISABLE_TQ_SUBGROUP=1` |
| `vulkan-escha-mm-column-block` | win | 3.50x at ncols=64 on escha3 17408x5120 (135,386 → 38,587 µs); decode held by the threshold | none |
| `escha-w2-decode-correctness` | correctness fix | Escha-W2 decode from token salad to coherent; weight correlation 0.000 → 0.936 | none |
| `vulkan-iq4xs-matvec-dedicated` | win | IQ4_XS 27B drafted +0.6167 t/s mean paired difference (8/8 cells); Q3_K_XL 27B drafted +0.293, plain +0.742 | none |
| `vulkan-iq3s-matvec-tpb16-union-gate` | win | Q3_K_XL 27B drafted 8.367 → 9.268 t/s medians (+10.7%, 8/8); kernel +35% at width 3 | none |
| `vulkan-rocmfp4-fast-q8_1-mmvq` | win, on by default since b11178-x1i2 | ROCmFP4-FAST 27B drafted 7.498 → 9.518 t/s means (+26.9%, 8/8); plain and prefill tied. The adversarial test at widths 3 and 5 is not in its receipts (see Open) | `GGML_ARIFI_ROCMFP4_MMVQ=0` |
| `vulkan-fa-dequant-kv-runtime-gate` | switch over upstream behaviour | the knob and its receipt line; same-binary gate OFF vs ON contrast: prefill +0.85% [+0.36%, +1.33%], decode +0.49% [-1.62%, +2.59%]. An earlier +8.08% prefill read did not reproduce. Default = upstream (ON) | `GGML_ARIFI_FA_DEQUANT_KV=0` |
| `cpu-vnni-repack-g128-kernels` | win, CPU, opt-in | Q2_0_G128 decode 2.01 → 6.69 t/s (3.3x), prompt +45% | `GGML_ARIFI_VNNI_REPACK` unset |
| `cpu-vnni-repack-dual-residency` | win, CPU, opt-in | decode +22.0% / +23.7% vs mode 0, prefill held | `GGML_ARIFI_VNNI_REPACK` unset |
| `cpu-vnni-repack-default-off` | decision | off by default on Vulkan builds: on, it costs 77-88% prompt to gain 20-28% decode | `GGML_ARIFI_VNNI_REPACK=1` |
| `powerinfer-moe-streaming-hook` | win | streamed MoE server decode 5.99 / 9.79 / 10.48 / 12.31 t/s against 9.00 for the fork's best earlier setup | none |
| `powerinfer-pipeline-init-reuse` | win | pipeline init 2472 µs → 0-2 µs, about 2.5 ms per token | none |
| `powerinfer-windows-port-avx-cure` | enablement | Windows port and crash cure; no throughput claim | none |
| `powerinfer-expert-bundle-generator` | enablement | first disk-streamed sparse inference on Windows | `GENERATE_EXPERT_BUNDLE` unset |
| `powerinfer-streamed-repack-carveout` | correctness fix | streamed-expert output from garbage to coherent | none |
| `powerinfer-gpu-safe-staging` | enablement | `-ngl 99 -cmoe` clean at 4.9 t/s with CPU zero-copy kept | none |
| `rocmfpx-weight-formats` | capability | native ROCmFP4 file loads and answers correctly (CPU path, 3.24 t/s at `-ngl 0`); Vulkan kernels came later | `GGML_ARIFI_ROCMFPX_FORMATS=OFF` |
| `vulkan-concat-transpose-deltanet` | kernel win, served tie | CONCAT dispatch -12.8% per prefill graph; serve-level tied on three shapes | `GGML_VK_CONCAT_TRANSPOSE=0` |
| `vulkan-q6k-matvec-direct-scales` | neutral | neutral on gfx1103; carried for other devices | none |
| `vulkan-rdna3-mmv-id-rows-switch` | no gain | carried as a switch, no measured gain | none |
| `vulkan-uma-read-path-probe` | no gain | flat | `GGML_ARIFI_UMA_READ_PATH=direct` |
| `vulkan-mul-mat-id-staging-receipt-hazard-assert` | guard | the upstream K-padding path cannot engage on this GPU (0 padded rows in 7 logs) | none |
| `spec-dflash-fused-inject-switch` | no gain | carried as a switch | `LLAMA_DFLASH_FUSED_INJECT=1` |
| `kv-ple-ngram-index-switch` | unmeasured | no workload we hold reaches it | `LLAMA_KV_NGRAM_INDEX=0` |
| `ggml-sx8-type-57-cpu-decoder` | carried | S-X8 CPU decoder; harmless when unused. The Vulkan kernels are separate and not yet registered (see below) | none |
| `ggml-turboq-tbq-kv-types-58-59` | carried, CPU only | TBQ3_0/TBQ4_0 KV types; Vulkan reports them unsupported; 3-bit V cache refused by a quality guard | `LLAMA_ALLOW_TBQ3_KV=1` lifts the guard |
| `moe-cache-heat-protected-eviction` | third-party figure | the source reports +12% / +7.5% TG on an RTX 5090; not measured by us | `GGML_CUDA_MOE_CACHE_HOT_USES=0` |
| `cuda-tq3-4s-kernels` | unmeasured | no CUDA hardware here | none |
| `metal-tq3-4s-kernels` | unmeasured | no Metal hardware here | none |
| `arifi-offrig-ci-backend-matrix` | build matrix | compiles CUDA, Metal, SYCL and Vulkan-on-Windows with the fork formats ON; no t/s by construction | none |

### Wins in the code that the manifest does not register yet

These are measured and shipped, and their entries are owed: the S-X8 Vulkan decode kernel, mat-vec
A-hoist and MMVQ route, and packed cooperative-matrix tile; the per-heap Vulkan allocator that serves
27B drafted lines which failed at load; the q4_K wide-width MMVQ route, the Q6_K x-fold and the Q5_K
B-hoist; the q5_K MMVQ route; the iq3 sign hoist; the iq3 width-7 rows fix; the q6_K MMVQ route at
widths 7-8 and its width-6 admit; the q4_K width-5 split; the iq3_s width-6 rows; the Radeon 890M UMA
placement; the draft replay-skip fix; adaptive draft sizing; the hybrid-target recurrent-state fix;
the draft-checkpoint crash fix; the Q2_0_G128 Vulkan port.

## Results that showed no gain

- **The latest release is a tie at our serve line.** Draft depth 4, four launches per arm: GSQ IQ3_S
  7.988 → 7.925 t/s, Q4_K_XL 7.208 → 7.192 t/s; every confidence interval spans zero. Its kernel wins
  live at verify widths 5-7, which deeper drafts reach.
- **Draft depth.** On S-X8 27B depth 6 ties depth 4, depth 8 loses, and the shipped adaptive depth
  (`GGML_DFLASH2_ADAPTIVE`) loses; acceptance falls as fast as verification amortises.
- **buun's DFlash2 adaptive controller** ships off: on a prose-weighted gate it ran 4.04 t/s with 8 of 16
  outputs degenerate, against 6.59 t/s clean for a fixed cap of 3.
- **Drafter precision:** a Q8_0 drafter is no better than Q4_K_M (Q4_K_XL tie, GSQ -3%).
- **iq4_xs MMVQ route** ships opt-in (`GGML_ARIFI_IQ4XS_MMVQ=route`): greedy output diverged on one file.
- **iq3 sign hoist** is flat on the 780M (paired ratios 0.991-1.013) and bit-identical to the inherited
  loop. It ships on for every device, with no device gate, for drivers that do not hoist the select
  themselves; it compiles to 349 instructions per column against 1229. `GGML_ARIFI_IQ3_MMVQ=legacy`
  restores the inherited loop.
- **thecodacus host-transfer prefetch** measured inert on unified memory; its +64.5% prefill belongs to the
  host-offloaded `-cmoe` placement it was written for. Off by default on unified-memory machines.
- **DSpark** fell from 7.89 to 4.27 t/s (0.54x) on the 780M. Off.
- **TurboQuant KV** at n_ctx 16384: f16 1792 MiB at about 20 t/s and `q4_0` 504 MiB at 19.8 t/s pass;
  turbo3 (350 MiB, 8.5 t/s) and turbo2 (238 MiB, 11.2 t/s) fail the quality check. Plain `q4_0` is the
  better trade on this box.
- **MTP is model-dependent.** A 31B root-MTP model went 1.8 → 9.4 t/s and Gemma-4-26B-A4B 29.3 → 37.3; a
  Qwen3.5 MoE lost to plain Q4_0 (22.8 vs 29.2).
- **S-X8 plain on the drafted A/B:** the A/B that gave the S-X8 drafted gain (2026-09-14, 3.334 → 4.475
  t/s) read plain decode 1.995 → 1.705 t/s on the same two builds (medians, 0 of 8 cells). The README plain
  pair comes from the earlier A/B; the cause of this plain loss is not separated.
- **MoE expert cache on this box:** with the experts forced off the GPU (`-ncmoe 48`) on a 35B-A3B model,
  the Vulkan cache read 7.960 t/s against 8.875 with the cache off (medians, -10.31%); the same model fully
  GPU-resident ran 20.517 t/s. On one memory pool the cache has no residency problem to solve.
- **Windows IOCP expert reads:** direction unresolved; the test model's experts mostly fit in cache.
- **Hot-expert RAM cache** (`MAX_N_CACHED`): unmeasured. **Standalone expert prefetch cap:** inconclusive.
- **Power plan:** a "performance" plan cut GPU-resident decode from 29.0 to 11.2 t/s; every default was
  chosen under Balanced.

## Open

- ROCmFP4-FAST MMVQ is on by default since this release; the adversarial test at widths 3 and 5 is not in its receipts.
- q4_K at widths 4-8 still reads below the bus on drafted Q4_K_XL; two mechanism candidates were refuted.
- A rare stale destination on very large Q6_K Vulkan mat-vec inputs (3 of 120, then 0 of 160); root cause open.
- S-X8 PCA correction: built and proven to run; its perplexity effect is within noise.
- CUDA, Metal and SYCL: compiled by the CI matrix only; no fork number on that hardware.
- `arifi-sync` `build` and `judge` legs are written but not yet exercised.
- Q2_0_G128 Vulkan path: ported and loading; not benchmarked here.
- TQ3_0, TQ3_4SE and TQ3_1S_SHIFT (types 49-51): CPU only; the Vulkan backend reports them unsupported.
- The README speed table pairs the build before and after each kernel, early to mid September; those
  four files have not been re-measured on the latest release.

## Reading the commit trailers

- `Taken-from:` names the source repository and commit a change was ported from.
- `Origin:` names who wrote the change and how it was adapted.
- `Measured-effect:` names what was measured, on which hardware, or says `UNMEASURED`.

`python tools/arifi-sync/arifi_sync.py provenance` fails on a fork commit without them.
