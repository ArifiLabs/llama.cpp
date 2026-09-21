# ArifiLabs llama.cpp — `r73i-release-engine-2026-09-21`

> **Where the receipts live.** Receipt paths of the form `r73-evidence/…`, `r66-evidence/…` and other `r*-evidence/…` names in this document are **lane-local**: they name files in the lane worktree that produced them, not paths in this repository. A clone does not contain them. `evidence/MANIFEST.md` lists what is tracked here, maps each file to the claim it supports, and says plainly what is missing.

*(DRAFT release body. Not published. Publication is the President's call.)*

Base: upstream `ggml-org/llama.cpp` **b10825** (`9e0e22059`) + **588 patches**. Series tip
`037b433a9`, source tip `5a7434218`, engine `arifi-b10825-r73i-5a7434218`.

## New in R73 — one Vulkan default ON, with its revert switch

**q6_K takes the q8_1 MMVQ route** — AMD only, **n=7,8 only**, **`MUL_MAT` only**;
`GGML_ARIFI_Q6K_MMVQ=legacy` restores the old path.

Upstream keeps q6_K off MMVQ on an unmeasured source comment ("only a win on Intel"). Measured at
the widths a speculative decoder verifies at, it wins: **+5% to +40% per q6_K verify column**,
**58 of 60 paired rounds won** across five shapes, **worst single round 0.9894**.

| Shape | n=7 | n=8 |
|---|---|---|
| 248320×5120 (lm_head) | **1.400** 6/6 (20,046 → 14,313 us) | **1.131** 6/6 |
| 5120×17408 | **1.163** 6/6 | **1.050** 6/6 |
| 5120×6144 | **1.125** 6/6 | 1.058 5/6 |
| 17408×5120 | **1.122** 6/6 | **1.085** 6/6 |
| 1024×5120 | **1.088** 6/6 | 1.009 5/6 — **a tie, disclosed not banked** |

`MUL_MAT_ID` is **excluded**: measured wash, so the MoE path does not take the route.

**Read the reach before you read the percentages.** A speculative decoder verifies at
**width = 1 + draft depth**. This route fires at n=7,8, so it is reached by a **draft depth of 6 or
7** — the depth the **DFlash2 publisher recipe** calls for — and it is **latent at our own served
draft depth of 4**, which verifies at width 5. **R73 therefore changes zero served tokens on our
box, and no throughput claim is made for it.** The win is an op-level, per-column number. Anyone
running a deeper draft gets it immediately; we do not, yet.

## Unchanged from R66 — three defaults still ON, each with its switch

**1. q5_K takes the MMVQ route** (AMD, widths 5–8, k ≤ 8192) — `GGML_ARIFI_Q5K_MMVQ=0` to revert.
The marginal q5_K verify column drops **113.8 → 5.4 us** (q4_K's is 27). Paired 6-round ratios
1.067 / 1.109 / 1.095 / 1.204 at n=5..8. Perplexity identical: 5.9932 ± 0.14292 vs 5.9918 legacy.

**2. iq3 mat-vec sign hoist v2**, all devices — `GGML_ARIFI_IQ3_MMVQ=1` to revert.
SPIR-V drops from **1229 to 349 instructions per column**. Wall clock on our gfx1103: **no change** —
the AMD driver appears to hoist it already. It ships ON for the drivers that do not. Bit-identical.

**3. iq3 mat-vec, 2 rows per workgroup at width 7** (RDNA3) — `GGML_ARIFI_IQ3_N7_ROWS=4` to revert.
A register spill at exactly one width: 768 B of scratch at n=7, 0 everywhere else. Fixing it gives
**465,400 → 20,890 us/run** at m=248320 — **21–25x**, 6/6 rounds, bit-identical.

Also in tree, all inert by default: `GGML_ARIFI_MMV_MAX_COLS` (R57), the R65 rows knob,
`GGML_ARIFI_OP_DUMP` (R61), R60 coverage rows.

## Verification — R73

- **47 rc files, all rc=0** (24 test cells + 12 bit-identity runs), including a set re-run beside a
  live 27B load.
- **q6_K correctness 83/83.** The planted-RED arm fails 21 cases, **all at n=7,8**, including both
  newly added shapes; the tree restores byte-identical.
- **Perplexity at `-b 7` and `-b 8` within one stderr.** The `-b 7` arm asserts `batch_size=7` per
  arm, so the route was genuinely dispatched: 5.9897 ± 0.143 → 5.9947 ± 0.143.
- **Bit-identity 6/6** vs the r66i stage — scope stated: greedy runs never reach q6_K at n=7/8, so
  this shows **no collateral change**, not route correctness.
- **Series integrity PASS** — every patch byte-identical to a fresh generation and to a committed
  blob; **588** patches replayed cleanly, replayed tree IDENTICAL outside `patches/series`, rc=0.
- **Post-flip smoke rc=0**, 37.4 s, all four startup lines present.
- **0 untrailered commits** in the R73 range.

## R66 validated end to end, with the fixed harness

Unit = **launch**, **8 launches per arm** × 4 rounds, order counterbalanced, **0 invalid launches**,
INTEGRITY clean, Welch CIs. df2 decode R56 → R66: Q4_K_XL 7.34 → 7.46 ([−0.23, +0.47]); S-X8
5.90 → 5.94 ([−0.11, +0.20]); GSQ IQ3_S 8.08 → 8.02 ([−0.18, +0.06]).

**Every interval contains zero — no regression on any line in any mode, quality identical.** At
draft depth 4 the verify width is 5, where only the q5_K route is reached at all. The kernel gains
in this release are **per-column at verify widths 5–8**; the served line is flat. Both statements
are here on purpose. This also kills, at launch level, the prefill "regression" an earlier
cell-level chain reported, and closes the owed Q4_K_XL df2 re-run (16 valid launches).

## What we still cannot show

- **No served throughput win**, in R66 or R73. The `us/run` and instruction counts above are
  `test-backend-ops` op-level numbers, not throughput.
- **An instrument defect, found and disclosed.** A null control ran the *same binary* in both arms
  and read +5.30 t/s prefill, "CI-clean". Every banked prefill or plain-decode delta of that size
  from 2-launch cells is **UNRESOLVED**, including some of our own earlier claims.
- **An unresolved prefill residual**: 27B S-X8 under llama-bench, −1.7%, CI95 [−4.4, +1.0]. Not
  shown nonzero, not shown zero.
- **q6_K × f16 `src1` and 5120×10240 were never timed** — correctness only.
- **The f16 iq3 pipelines ship with 0 executed test cases** under the width-7 default.
- **1024×5120 n=8 is a tie** (1.0087) under our own 1.03 admit bar — disclosed, not banked.

## Where every number was measured

Beelink SER7 Pro — Ryzen 7 7840HS, **Radeon 780M (RDNA3, gfx1103)**, driver 32.0.31041.1004,
Windows 11 build 29648, Balanced power plan, MinGW-w64 + Ninja, one shared pool of system RAM (48 GB
physical, 16 GB reserved in BIOS). No other GPU, OS, compiler or architecture has ever run this code.
Kernels transfer; numbers do not. Results from other GPUs are wanted — see the README.
