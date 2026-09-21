# ArifiLabs llama.cpp — `r66i-integration-2026-09-19`

*(DRAFT release body. Not published. Publication is the President's call.)*

Base: upstream `ggml-org/llama.cpp` **b10825** (`9e0e22059`) + **586 patches**. Tip `94461e770`.

## What changed — three Vulkan mat-vec defaults are now ON, each keeping a revert switch

**1. q5_K takes the MMVQ route** (AMD, widths 5–8, k ≤ 8192) — `GGML_ARIFI_Q5K_MMVQ=0` to revert.
The marginal q5_K verify column drops **113.8 → 5.4 us** (q4_K's is 27). Paired 6-round ratios
1.067 / 1.109 / 1.095 / 1.204 at n=5..8. Perplexity is identical: 5.9932 ± 0.14292 vs 5.9918 legacy.
90.5% of a typical served file's q5_K bytes are k ≤ 8192. **No served throughput change was shown** —
every served contrast in this release is a tie.

**2. iq3 mat-vec sign hoist v2**, all devices — `GGML_ARIFI_IQ3_MMVQ=1` to revert.
SPIR-V drops from **1229 to 349 instructions per column**. Wall clock on our gfx1103: **no change** —
the AMD driver's compiled pipeline statistics are identical between the arms, so its compiler appears
to hoist it already. It ships ON for the drivers that do not. Bit-identical, never slower here.

**3. iq3 mat-vec, 2 rows per workgroup at width 7** (RDNA3) — `GGML_ARIFI_IQ3_N7_ROWS=4` to revert.
A register spill at exactly one width: the driver reports 768 B of scratch at n=7 and 0 everywhere
else. Fixing it gives **465,400 → 20,890 us/run** at m=248320, and 34,674 → 1,636 / 39,284 → 1,573 at
two other shapes — **21–25x**, 6/6 rounds, bit-identical. iq3_xxs gains 1.20–1.42x. Dispatch width is
`1 + draft`, so only a draft depth of 6 reaches width 7: a **latent cliff** removed for that depth.

Also in tree, all inert by default: `GGML_ARIFI_MMV_MAX_COLS` (R57), the R65 rows knob,
`GGML_ARIFI_OP_DUMP` (R61), R60 coverage rows.

## Verification

- 40/40 gate cells rc = 0. `MUL_MAT` **2222 executed / 0 FAIL** / 876 not-supported, with the flipped
  routes exercised (iq3 at n=7 across 4 shapes; q5_K n=5..8 at k=256/4096/5120); `MUL_MAT_ID` 1004 / 0.
- Seven legacy arms all rc = 0 — the old behaviour is tested, not just reachable.
- Bit-identity 6/6 vs the previous stage (this proves the build, not the flips: n=1 is outside every
  flipped route). Perplexity at batch 8: 5.9932 ± 0.14292.
- Series integrity: every patch byte-identical to a fresh generation and to a committed blob; 586
  patches replayed cleanly, replayed tree identical to the branch outside `patches/series`.

## What we could not show

- **No served throughput win.** Under llama-server (our method of record): S-X8 5.433 → 5.530, GSQ
  IQ3_S 7.771 → 7.681, Q4_K_XL plain 4.095 → 4.108 — all ties; served prefill sits inside the
  launch-level band below. The `us/run` and instruction counts above are `test-backend-ops` op-level
  numbers, not throughput.
- **An instrument defect, found and disclosed.** A null control ran the *same binary* in both arms and
  read +5.30 t/s prefill, 16/16, "CI-clean". Cells were pseudo-replicates of 2 server launches, and
  launch level moves 4–26%. Every banked prefill or plain-decode delta of this size from 2-launch cells
  is therefore **UNRESOLVED**, including some of our own earlier claims. The harness fix is in flight.
- **An unresolved prefill residual**: 27B S-X8 under llama-bench, −1.7%, CI95 [−4.4, +1.0]. Not shown
  nonzero, not shown zero.
- **The Q4_K_XL drafted cell has no number**: two of four launches recorded zero draft acceptance on
  *both* engines. Cause unknown, not reproduced.
- **The f16 iq3 pipelines ship with 0 executed test cases** under the new width-7 default.

## Where every number was measured

Beelink SER7 Pro — Ryzen 7 7840HS, **Radeon 780M (RDNA3, gfx1103)**, driver 32.0.31041.1004,
Windows 11 build 29648, Balanced power plan, MinGW-w64 + Ninja, one shared pool of system RAM (48 GB
physical, 16 GB reserved in BIOS). No other GPU, OS, compiler or architecture has ever run this code.
Kernels transfer; numbers do not. Results from other GPUs are wanted — see the README.
