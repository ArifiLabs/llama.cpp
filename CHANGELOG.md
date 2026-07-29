# Changelog

Every change this fork adds on top of upstream `ggml-org/llama.cpp`, newest first.

Upstream itself keeps no changelog — it versions by build-number tag (`b10173`) and carries
the narrative in pull requests and issues on GitHub. A fork needs more than that, because the
only question a reader actually has is *"what did you change, and on what base?"* This file
answers exactly that. Entries are grouped by the upstream base they sit on, since a rebase is
the one event that can change behaviour without any of our own code changing.

Two rules for this file:

- **Every performance claim names the hardware it was measured on.** A number without a machine
  is marketing.
- **A mechanism that measured a loss is still listed, with its number.** Losses are results.

Detail beyond the summaries here lives in three places: [`docs/FINDINGS.md`](docs/FINDINGS.md)
(what we learned and how), [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md) (every switch,
its default, and the measurement that chose the default), and
[`patches/series/MANIFEST.md`](patches/series/MANIFEST.md) (per-patch source and measured effect).

## Reference hardware

Unless an entry says otherwise, every measurement was taken on:

- **RIG-A** — Beelink SER7, AMD Ryzen (Zen 4) with Radeon 780M integrated graphics (RDNA3,
  gfx1103, 12 CU), 32 GB DDR5-5600 with a 16 GB unified-memory carve-out, Windows 11,
  MinGW/UCRT toolchain, Vulkan backend.

---

## On upstream `b10173` (`e9fa0781f`) — 2026-07-29

### Fixed

- **A crash in upstream's own x86 repack kernels on Windows.** `q4_0`, `iq4_nl` and `mxfp4`
  models died with an access violation (`0xC0000005`) as soon as the CPU repack path became
  reachable. Root cause: the nibble lookup table was passed to the shared GEMV/GEMM templates
  as a by-value 32-byte vector; the Windows x64 ABI passes that in memory and guarantees only
  16-byte stack alignment, while the compiler emits an alignment-requiring store for the spill.
  Threads whose frame landed 16-mod-32 faulted. The parameter is now derived inside the callee
  from its own block type, so the value never crosses the ABI boundary.
  Reproduces on **stock upstream**, unpatched — this is not a defect introduced by this fork,
  and the fix is offered upstream. See [`docs/FINDINGS.md`](docs/FINDINGS.md) F-01.

### Added

- **CPU repack kernels for the 128-group ternary format (`Q2_0_G128`).** This fork's flagship
  ternary format had no fast CPU path: the interleaved-block template derived its group size from
  the bit width, so it could not express "2-bit at group 128", and a g128 model logged
  `cannot be used with preferred buffer type CPU_REPACK`. The block template now takes the group
  size as a defaulted third parameter — every existing spelling stays valid — and the AVX512-VNNI
  GEMV/GEMM are templated on it. The vector core is unchanged: the expansion helper loads one
  32-byte register and has no notion of the block, and the `qs` stride is a constant of the
  four-row interleave rather than of the group.

  The two group sizes get separate arms and separate kernel instantiations on purpose. A g128 row
  entering a g64 kernel is mis-strided **silent wrong math, not a crash**, so the type is carried
  as a template parameter into the repack, which asserts it.

  Correctness on **RIG-A** (`llama-server`, `Ternary-Bonsai-8B-Q2_0.g128.gguf`, `-ngl 0 --no-host
  -c 2048 -t 8`, temp 0, token-id comparison, 3 rolls per cell): repacked output is
  **token-identical to the scalar path** on both a GEMV-driving and a GEMM-driving prompt, max
  |Δ logprob| **0.021–0.040** — inside the F-05 band and below this fork's own g64 ternary 0.061.
  The repack buffer claims **1759.50 MiB**, 252 of 254 ternary tensors, the two declined being the
  151669-row vocab tensors that fail the same `ne[1] % 4` test the g64 arm has always applied.
  Dual residency (`=2`) covers the new type for free — **252 shadow tensors**, graph splits
  tracking mode 0 rather than mode 1, and bit-identical to mode 1 under `--no-op-offload`.
  The shared g64 kernels were rewritten on the way through, so they were re-run against the
  pre-change answer key rather than assumed unaffected: **36 cells, every token identical, every
  logprob delta 0.000000000**. See [`docs/FINDINGS.md`](docs/FINDINGS.md) F-06.

  **No speed claim.** Whether this is faster, and where, is unmeasured.

- **Dual residency for the ternary repack path — `GGML_ARIFI_VNNI_REPACK=2`.** Until now this
  slot forced a choice: repacking Q1_0/Q2_0 weights bought **+22–24%** decode and cost **−65% to
  −80%** prompt processing on a Vulkan build, because weights in the `CPU_REPACK` buffer stop being
  eligible for large-batch GPU offload. Mode 2 stops choosing. The weight stays in the ordinary
  host buffer — so the scheduler still sends prefill to the GPU — and the CPU path is served from a
  separately allocated repacked *shadow* of the same weights. Measured on RIG-A, one binary with
  only the environment flip, 519-token prefill, three interleaved replicates × 5 rolls (n=12 per
  cell), free RAM 5.60–5.72 GB at every launch:

  | tok/s | off | repack (mode 1) | **dual (mode 2)** |
  |---|---|---|---|
  | prompt, all-Q2_0 | 905.25 | 178.25 (−80.3%) | **963.51** |
  | decode, all-Q2_0 | 40.95 | 50.95 (+24.4%) | **50.65 (+23.7%)** |

  The decode gain has disjoint ranges against the baseline. The apparent prompt *gain* does not —
  the ranges overlap, so it is reported as noise: dual residency does not recover prefill, it never
  loses it. Cost is the affected tensors resident twice — **95.98 MiB** on that model, logged at
  runtime because it sits outside llama.cpp's own buffer accounting. Nothing about precision
  changes: with GPU offload disabled, so that both modes prefill on the CPU and only the shadow
  differs, mode 2 is **bit-identical** to mode 1 across three models and two prompt shapes
  (max |Δ logprob| `0.000000000`, deterministic 3/3). Scoped to Q1_0/Q2_0 and to `MUL_MAT`; the MoE
  `MUL_MAT_ID` path is excluded and labelled, because no ternary MoE model exists here to test it
  on. See [`docs/FINDINGS.md`](docs/FINDINGS.md) F-09, which this closes.

- **TurboQuant `TQ3_1S` / `TQ4_1S` weight formats** behind `GGML_ARIFI_TURBO_WEIGHT_QUANTS`
  (default OFF). Type-ids 45/46 adopted verbatim, so files from the originating project load
  natively with no conversion step. Measured on RIG-A against plain `Q4_0` quantized from the
  same source: **438.3 MiB @ 4.47 tok/s** versus **408.9 MiB @ 49.3 tok/s** — larger *and*
  about 11× slower on the CPU path, because no SIMD kernel exists for it and it is a 5.0
  bits-per-weight operating point against `Q4_0`'s 4.5. Shipped compiled-available and OFF;
  the format is retained for accelerator targets where fused kernels exist.
  One deliberate deviation from the source: its inner dot-product allocated heap memory twice
  per call, which is replaced here by a fixed stack buffer. Numerically identical.

### Changed

- **Base moved from `b10068` to `b10173`** — 105 upstream commits. The 64-patch series replayed
  with a single conflict (upstream's load-mode refactor in the model loader), resolved by hand.
  Verified after the move: the series replays to a byte-identical tree, every commit carries
  provenance, and the regression judge serves a ternary g128 model correctly.
- **Upstream's Vulkan `Q2_0` support (`#25430`) was evaluated and found not to replace this
  fork's g128 work** — it is a 64-value-group format and cannot read 128-value-group files at
  all. It had already merged one day before this fork's previous base was pinned, so it was
  never a competing implementation; our g128 support is an extension of it. Nothing was dropped.

---

## On upstream `b10068` (`571d0d540`) — 2026-07-24 to 2026-07-28

### Added

- **Ternary `Q2_0_G128` support** — loader, CPU dispatch, Vulkan shaders and a fail-closed
  retagging tool for 128-value-group ternary models, which upstream cannot load. On RIG-A a
  27B ternary model (~7 GB on disk) serves at **12.5–13.3 tok/s** and an 8B at **36.8 tok/s**.
- **ROCmFP4 / ROCmFPX weight formats** behind `GGML_ARIFI_ROCMFPX_FORMATS` (default OFF).
  Six format ids carried verbatim. Verified end-to-end: a 9B model in `ROCmFP4_COHERENT` loads
  in 7 s and answers correctly; with the gate off the same file is refused cleanly rather than
  misread.
- **AVX-VNNI repack kernels for ternary weights** behind `GGML_ARIFI_VNNI_REPACK` (default OFF).
  On RIG-A, CPU-only: prompt processing **+327% to +442%**, decode **+20% to +24%**. On a Vulkan
  build the same feature costs **77–88%** of prompt speed, because repacked weights lose
  large-batch GPU-offload eligibility — hence OFF by default, with a startup advisory in the one
  configuration where ON is clearly right.
- **Expert streaming from disk** for mixture-of-experts models, with a Windows-native
  asynchronous read path (`POWERINFER_IOCP`). On RIG-A a streamed 21B model reaches
  **10.5–12.3 tok/s** warm. The asynchronous path's speed effect is **unresolved**: the test
  model's experts largely fit in the OS file cache, so there was little disk traffic to hide.
- **Host prefetch patches** from an unmerged community contribution. **Measured inert on RIG-A**
  — an integrated GPU shares system memory, so there is no host-to-device upload to overlap.
  Carried for discrete-GPU users and labelled accordingly.
- **`--arifi-profile`** — named hardware profiles that log verbatim which options they selected
  and why. Deliberately not automatic detection; see [`docs/FINDINGS.md`](docs/FINDINGS.md) F-08.
- **Maintenance tooling** (`tools/arifi-sync/`) — verifies the patch series replays to a
  byte-identical tree, audits provenance trailers, and reports drift against every upstream
  source. Designed to fail loudly rather than resolve anything automatically.

### Not shipped, with reasons

- **TurboQuant KV-cache compression.** The storage claim is real and reproduced —
  **5.1× to 7.5×** smaller than f16 — but on RIG-A every configuration that stayed coherent
  lost to plain `q4_0` on both size and speed (`q4_0`: 504 MiB @ 19.8 tok/s; the 3-bit variant:
  350 MiB @ 8.5 tok/s). Not shipped. Worth revisiting where memory headroom outranks decode
  speed.
- **A speculative-decoding drafter** for the ternary family: measured **0.54×** on RIG-A
  (7.9 → 4.3 tok/s). Compiled, default OFF.
- **A whole-fork merge of the ROCmFPX project**: lost **14.2%** on long prompts on RIG-A. This
  is why the fork takes individual mechanisms rather than whole trees.
- Several mechanisms were excluded because **upstream already had them** — verified by commit
  ancestry rather than by name. One was unobtainable: its source repository is not public.
