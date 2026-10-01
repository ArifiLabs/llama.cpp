# ArifiLabs llama.cpp — engineering status

The [README](../../README.md) shows what works and what was won. This page is the full ledger:
every protected win with its honest state, every result that showed no gain, and what is still open.
Nothing here is hidden from the front page by accident; it is filed where engineers look.

## How the numbers were taken

- **Speed:** Beelink SER7 Pro, Ryzen 7 7840HS, Radeon 780M (RDNA3, gfx1103), Windows 11, Vulkan
  with `KHR_coopmat`, AMD driver 32.0.31041.1004, Balanced power plan. Served numbers are
  `llama-server` decode t/s over interleaved launches with a 95% confidence interval per paired cell;
  kernel numbers are `test-backend-ops perf` medians.
- **Memory epochs on that box:** 2x16 GB matched DDR5-5600 until 2026-09-03, then 32+16 GB. The
  ROCmFP4-FAST, UD-Q3_K_XL and IQ4_XS rows on the README were measured on 2x16 GB in early September
  2026. The S-X8 rows (2026-09-14/15), the q5_K, q6_K, iq3 and q4_K kernel rows (2026-09-17 to 2026-09-24)
  and the GSQ IQ3_S and Q4_K_XL rows (2026-09-24) were measured on 32+16 GB.
- **Baselines:** a served "before → after" compares two builds of this fork on the same file and box.
  A kernel row whose off switch restores the upstream path is a comparison with upstream.
- **Radeon 890M:** correctness only so far. The placement and `test-backend-ops` receipts were taken
  on 2026-09-30 with 48 GB of RAM and a 24 GB GPU reservation, before the box moved to 96 GB with
  72 GB reserved. Speed measurement on the 890M is in progress; no 890M speed number is published.
- **Receipts in this repository:** [`evidence/`](../../evidence/) with
  [`evidence/MANIFEST.md`](../../evidence/MANIFEST.md) (sha256 per file). These README figures have a
  shipped receipt: GSQ IQ3_S and Q4_K_XL plain and drafted speeds, perplexity, bit-identity,
  `test-backend-ops` counts, the q4_K width-5 split, the iq3_s width-6 marginal and every 890M check.
  The other speed figures come from the protected-win manifest below or from studio benchmark receipts
  that are not shipped here: the S-X8 27B progression (1.83 t/s plain and a drafted line that failed at
  load on the 2026-09-14 build, 5.55 t/s drafted on the 2026-09-15 build), the S-X8 kernel rows, the
  q6_K, q5_K and iq3 width-7 kernel rows.

## Protected wins

[`tools/arifi-sync/protected-wins.json`](../../tools/arifi-sync/protected-wins.json) holds 30 entries.
`python tools/arifi-sync/arifi_sync.py protected-win validate --ref HEAD` checks them against the tree.
**At this tip it reports FAIL:** the manifest lags the code. Measured commits without an entry, baseline
rows refused on ancestry, and one evidence locator that does not resolve in
this repository. Run it for the current count; none of the problems concern a code path.

| id | State | Measured effect (Radeon 780M unless noted) | Opt out |
|---|---|---|---|
| `vulkan-tq-matvec-subgroup` | win | 27B TQ3_4S 0.67 → 1.80 t/s; 0.5B TQ4_1S 44.7 → 126.7, TQ3_1S 45.5 → 107.1; kernel 3.1-3.4x | `GGML_VK_DISABLE_TQ_SUBGROUP=1` |
| `vulkan-escha-mm-column-block` | win | 3.51x at ncols=64 on escha3 17408x5120; decode held by the threshold | none |
| `escha-w2-decode-correctness` | correctness fix | Escha-W2 decode from token salad to coherent; weight correlation 0.000 → 0.936 | none |
| `vulkan-iq4xs-matvec-dedicated` | win | IQ4_XS 27B drafted +0.62 t/s (8/8 cells); Q3_K_XL 27B drafted +0.29, plain +0.74 | none |
| `vulkan-iq3s-matvec-tpb16-union-gate` | win | Q3_K_XL 27B drafted 8.37 → 9.27 t/s (+10.8%, 8/8); kernel +35% at width 3 | none |
| `vulkan-rocmfp4-fast-q8_1-mmvq` | win, opt-in | ROCmFP4-FAST 27B drafted 7.50 → 9.52 t/s (+26.9%, 8/8); plain and prefill tied. Default stays off until an adversarial test at widths 3 and 5 clears a numerical residual | `GGML_ARIFI_ROCMFP4_MMVQ` unset |
| `vulkan-fa-dequant-kv-runtime-gate` | switch over upstream behaviour | plain prefill +8.1% with the scratch copy ON vs OFF; default = upstream (ON) | `GGML_ARIFI_FA_DEQUANT_KV=0` |
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
- **iq3 sign hoist** is flat on the 780M (0.99-1.01). It ships on everywhere because drivers that do not
  hoist the select themselves gain from it.
- **thecodacus host-transfer prefetch** measured inert on unified memory; its +64.5% prefill belongs to the
  host-offloaded `-cmoe` placement it was written for. Off by default on unified-memory machines.
- **DSpark** fell from 7.89 to 4.27 t/s (0.54x) on the 780M. Off.
- **TurboQuant KV** at n_ctx 16384: f16 1792 MiB at about 20 t/s and `q4_0` 504 MiB at 19.8 t/s pass;
  turbo3 (350 MiB, 8.5 t/s) and turbo2 (238 MiB, 11.2 t/s) fail the quality check. Plain `q4_0` is the
  better trade on this box.
- **MTP is model-dependent.** A 31B root-MTP model went 1.8 → 9.4 t/s and Gemma-4-26B-A4B 29.3 → 37.3; a
  Qwen3.5 MoE lost to plain Q4_0 (22.8 vs 29.2).
- **Windows IOCP expert reads:** direction unresolved; the test model's experts mostly fit in cache.
- **Hot-expert RAM cache** (`MAX_N_CACHED`): unmeasured. **Standalone expert prefetch cap:** inconclusive.
- **Power plan:** a "performance" plan cut GPU-resident decode from 29.0 to 11.2 t/s; every default was
  chosen under Balanced.

## Open

- Radeon 890M speed: measurement in progress; the 780M-tuned defaults reach it by feature probe.
- ROCmFP4-FAST MMVQ default: owed an adversarial test at widths 3 and 5 before it turns on.
- q4_K at widths 4-8 still reads below the bus on drafted Q4_K_XL; two mechanism candidates were refuted.
- A rare stale destination on very large Q6_K Vulkan mat-vec inputs (3 of 120, then 0 of 160); root cause open.
- S-X8 PCA correction: built and proven to run; its perplexity effect is within noise.
- CUDA, Metal and SYCL: compiled by the CI matrix only; no fork number on that hardware.
- `arifi-sync` `build` and `judge` legs are written but not yet exercised.
- Q2_0_G128 Vulkan path: ported and loading; not benchmarked here.

## Reading the commit trailers

- `Taken-from:` names the source repository and commit a change was ported from.
- `Origin:` names who wrote the change and how it was adapted.
- `Measured-effect:` names what was measured, on which hardware, or says `UNMEASURED`.

`python tools/arifi-sync/arifi_sync.py provenance` fails on a fork commit without them.
