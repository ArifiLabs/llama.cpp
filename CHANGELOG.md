# Changelog

Every change this fork adds on top of upstream `ggml-org/llama.cpp`, newest first.

Upstream itself keeps no changelog — it versions by build-number tag (`b10173`) and carries
the narrative in pull requests and issues on GitHub. A fork needs more than that, because the
only question a reader actually has is *"what did you change, and on what base?"* This file
answers exactly that. Entries are grouped by the upstream base they sit on, since a rebase is
the one event that can change behaviour without any of our own code changing.

Three rules for this file:

- **Every performance claim names the hardware it was measured on.** A number without a machine
  is marketing.
- **A mechanism that measured a loss is still listed, with its number.** Losses are results.
- **Every performance claim was taken under the declared power plan.** On an APU the CPU and the
  integrated GPU share one package power budget, so a "performance" power plan can halve iGPU
  inference while speeding CPU work up — see [`docs/FINDINGS.md`](docs/FINDINGS.md) F-12. The bench
  harness refuses to run under an undeclared plan.

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

## On upstream `b10680` (`d7bd3bfca`) — 2026-08-23 to 2026-09-05

Five Vulkan mat-vec / mat-mat mechanisms, one server bug fix, one drafting controller, and one
change that measured neutral and is shipped anyway. Every serve figure below was taken with
`llama-server` on **RIG-A**, one binary per arm or one binary with the arms selected by an
environment variable, and a fresh server per arm.

**On the power plan, which this file requires every performance claim to declare.** The
concat-transpose and `q6_k` entries were taken under **Balanced**, and their runs banked a
per-launch hardware guard file recording it. The `iq4_xs`, `iq3_s` and ROCmFP4-FAST entries were
taken by a harness that pins the machine's configuration but **does not write the active power plan
into its receipts** — so for those, Balanced is what the box was set to and **not** something this
changelog can point at a file to prove. That distinction is drawn rather than smoothed over because
of F-12: a "performance" plan on this APU measured 11.2 tok/s against Balanced's 29.0 on
GPU-resident decode, and cost this project eight days of numbers. Writing the plan into every
guard file is owed.

**Read the summary table with its columns, not across them.** Three of these runs sit in different
measurement epochs — the box took an AMD driver update (to `32.0.31041.1004`) and a reboot on
2026-09-03, and the RAM configuration later moved from 2×16 GB to an asymmetric 32+16 GB. An A/B is
internally valid inside one epoch and its absolute numbers are never differenced against another
epoch's. That is why each row names its own BEFORE binary, AFTER binary, model file and epoch, and
why no row is a continuation of the row above it.

### Before and after, one row per mechanism

| mechanism | model file | BEFORE → AFTER | epoch | quality | verdict |
|---|---|---|---|---|---|
| ROCmFP4-FAST mat-mat pipelines + mat-vec dequant hoist (`e3220bed0`) | julianmb Qwen3.8-27B-ROCmFP4-FAST, 14.56 GB | prefill graph **16112 → 4683 ms**; width-3 verify 338 → 271 ms | E1 | F-141 0/8 ×3, `test-backend-ops` MUL_MAT 0 FAIL | **WIN** |
| Dedicated `iq4_xs` mat-vec shader (`1884a620a`) | outsourc-e Qwen3.8-27B-Unleashed UD-IQ4_XS, 14.30 GB | drafted decode **7.675 → 8.610 t/s (+12%)** | E1 | rolled 35/40 → 35/40, F-141 0/8 | **WIN** |
| Dedicated `iq4_xs` mat-vec shader (`1884a620a`) | outsourc-e Unleashed UD-Q3_K_XL, 13.22 GB | drafted **8.173 → 8.323 (+2%)**; plain **4.952 → 5.426 (+10%)** | E1 | rolled 35/40 → 35/40, F-141 0/8 | **WIN** |
| `iq3_s` mat-vec split at `NUM_COLS == 3 \|\| > 4` (`aa9f0e2aa`) | outsourc-e Unleashed UD-Q3_K_XL, 13.22 GB | drafted decode **8.367 → 9.268 t/s (+11%)**; plain identical | E1 | rolled 35/40 → 35/40, F-141 0/8 | **WIN** |
| `iq3_s` mat-vec split (`aa9f0e2aa`) | outsourc-e Unleashed UD-IQ4_XS, 14.30 GB | drafted decode **+0.199 t/s (+2%)**, twice, across a driver change | E1 and E2 | rolled 35/40 → 35/40, F-141 0/8 | **WIN** (small, reproduced) |
| `iq3_s` split applied at **every** width (`616b877b8`) | — (kernel bench only) | `n=2` ×0.907, `n=4` ×0.925 | E1 | — | **LOSS — superseded by `aa9f0e2aa`** |
| ROCmFP4-FAST `q8_1` MMVQ mat-vec, retained at `n=3` and `n=5` (`f68a4bd25`) | julianmb Qwen3.8-27B-ROCmFP4-FAST, 14.56 GB | drafted decode **7.498 → 9.518 t/s (+27%)**; plain tied | E2 | rolled 32/40 → 33/40, F-141 0/8, zero empty completions | **MEASURED WIN — default-branch integration BLOCKED**, see below |
| Tiled concat-transpose for the delta-net conv state (`5b1b416cb`) | huihui-ai Huihui-Qwen3.8-27B-abliterated-UD Q4_K_XL, 16.2 GB | CONCAT dispatch **−13%**; whole-model **TIED**, twice | E3 | output byte-identical gate ON vs OFF 3/3, F-141 0/8 | **WIN at the op, TIE at the seat** |
| `q6_k` mat-vec reads scales from the block, not through LDS (`4bbebd619`) | huihui-ai Huihui-Qwen3.8-27B-abliterated-UD Q4_K_XL, 16.2 GB | op-level ratio **0.9796** against a control of **0.9990** — no row separates | 32+16 GB | output byte-identical, `test-backend-ops` MUL_MAT 1394/1394 | **NEUTRAL** — shipped for less work, **never quote it as a win** |
| Draft length sized from measured acceptance, `--spec-draft-adaptive` (`6c7d9275f`) | — | **not measured here** | — | — | **UNMEASURED on RIG-A**, default OFF |
| Server: do not re-verify replayed draft tokens after a checkpoint restore (`8a4044a82`) | — | no throughput claim | — | — | **BUG FIX** |

Whole-percent figures above are for reading at a glance; the detail entries carry the raw values,
confidence intervals and cell counts that the verdicts actually rest on.

### Per model file

| model file (publisher) | size | what moved on it | what is still OWED |
|---|---|---|---|
| Qwen3.8-27B-Unleashed **UD-IQ4_XS** (outsourc-e) | 14.30 GB | `iq4_xs` shader **+12%** drafted; `iq3_s` split **+2%** drafted, reproduced across two epochs | MMLU / HumanEval / GSM8K on this exact quant — the harness matrix holds `UD-Q4_K_M`, not this file |
| Qwen3.8-27B-Unleashed **UD-Q3_K_XL** (outsourc-e) | 13.22 GB | `iq4_xs` shader **+2%** drafted / **+10%** plain; `iq3_s` split **+11%** drafted | MMLU / HumanEval / GSM8K on this exact quant — OWED |
| Qwen3.8-27B-**ROCmFP4-FAST** (julianmb) | 14.56 GB | mat-mat pipelines **3.4× prefill**; MMVQ mat-vec **+27%** drafted | adversarial numerical validation at `n=3` / `n=5`; MMLU / HumanEval / GSM8K OWED |
| Huihui-Qwen3.8-27B-abliterated-**UD Q4_K_XL** (huihui-ai) | 16.2 GB | concat-transpose **−13%** on the op, tied at the seat; `q6_k` scales neutral | decode/verify-graph arm of the concat work; the `q6_k` **~35% deficit against `q5_K`** is open |
| DFlash2 **Q4_K_M** drafter (incoai) | — | used as the drafter in every drafted arm above at `--spec-draft-n-max 2` (verify width 3) | — |

Quality on the seat file, from the harness matrix rather than from this work:
MMLU `mmlu_arifi` **0.7333**, HumanEval pass@1 **0.9329** (153/164), GSM8K strict **0.84**.
ARC-challenge is **OWED-RESCORE** — this project has no trustworthy harness scorer for it. None of
these moved: every mechanism below is a kernel change under a fixed model file, and the per-A/B
quality gate is the 40-prompt rolled smoke score with thinking OFF, reported in every entry.

### Added

- **A dedicated `iq4_xs` mat-vec shader.** `iq4_xs` was the one widely-used K-quant-sized format with
  no shader of its own: the shader generator routed only the `*_k` types, the `iq1/iq2/iq3` family and
  `tq2_0` to a dedicated mat-vec, so `iq4_xs` fell to the generic path. That path decodes each block's
  scales once per `dequantize4()` call and uses one nibble half of every word it fetches, so the other
  half is fetched again by the thread sixteen positions away. The new shader follows the K-quant shape
  — 16 threads per 256-value superblock, each taking both nibble halves of two packed words for 16
  values per row, with the scale decoded once per row per superblock.

  **Kernel, on RIG-A** (`test-backend-ops perf`, `m=17408 k=5120`, 3-round medians): `iq4_xs` moves
  from **58–78%** of the `q4_K` yardstick's effective bandwidth to **98–105%** at widths 1 through 4,
  and **117%** at 5 and **144%** at 8. On the `m=4096 k=14336` rows both binaries carry, before against
  after: **×1.209** at `n=1`, ×1.107 at 2, ×1.114 at 3, **×1.863** at 4 (the generic shader's knee),
  ×1.129 at 5, ×1.112 at 8. Three untouched types measured on the same runs read **0.96–1.03**, which
  is this ladder's noise band.

  **Serve, on RIG-A** (`llama-server`, same model file per arm, palindrome of 2 launches × 8 rounds =
  16 rounds per slot, cell-wise CI95 over 8 paired prompt/seed cells, epoch E1): on
  `Unleashed UD-IQ4_XS` the drafted line — DFlash2 Q4_K_M at `--spec-draft-n-max 2`, the mode this
  project actually serves — moves **7.675 → 8.610 tok/s**, cell-wise **+0.617 [+0.255, +0.978], 8 of 8
  cells positive**. On `Unleashed UD-Q3_K_XL` a second clean take reads drafted **8.173 → 8.323**
  (**+0.293 [+0.060, +0.527]**, 7 of 8) and plain **4.952 → 5.426** (**+0.742 [+0.208, +1.276]**,
  7 of 8). Prefill is tied on both files, as expected — the mat-mat path is untouched.

  Correctness: `test-backend-ops -o MUL_MAT -p type_a=iq4_xs` **13 cases OK, 0 FAIL**, and `MUL_MAT_ID`
  OK. Output is not bit-identical to the generic shader — the summation order differs — and sits inside
  the op's own NMSE gate. The rolled 40-prompt smoke score with thinking OFF is **35/40 before and
  35/40 after** on both files, per prompt.

  A caveat that belongs with the number: on the plain (undrafted) line this box is bimodal, and a
  minority of rounds land in a slow regime regardless of which arm they fall on. That is demonstrated,
  not asserted — see [`docs/FINDINGS.md`](docs/FINDINGS.md) F-15. Plain verdicts here are read from the
  median plus a clean-cell interval, never from the raw cell-wise mean alone.

- **A split `iq3_s` mat-vec, at the two widths where it wins.** An unmerged community change reshapes
  this shader to 16 invocations per superblock and drops its `sum[]` array. Applied at **every** width
  it measured a **7–9% loss** on RIG-A at `n=2` (×0.907) and `n=4` (×0.925) — that build (`616b877b8`)
  is listed here because it is a result, and it is superseded. The shipped gate is
  `NUM_COLS == 3 || NUM_COLS > 4`: the source's own widths plus the one width below them that measured
  a win, with every other width taking the upstream body **bit for bit** under a specialization
  constant that folds at pipeline creation.

  **Kernel, on RIG-A** (`m=17408 k=5120`, 3-round medians): `n=3` **964.9 → 711.9 µs, 39.7 → 53.8 GB/s
  = +35%**; `n=5` ×1.472; `n=8` ×4.803 (the previously-measured cliff at that width, closed);
  `n=1`, `n=2` and `n=4` ×1.002 / ×0.993 / ×1.000 — the control that the untouched widths did not move.

  `n=3` is the verify width of DFlash2 at draft depth 2, which is why the serve effect tracks each
  file's `iq3_s` share. **Serve, epoch E1:** on `UD-Q3_K_XL` (26.6% `iq3_s` by bytes) drafted decode
  **8.367 → 9.268 tok/s**, **+0.820 [+0.622, +1.018], 8 of 8 cells**; the plain line is identical, which
  is the control proving the non-split widths are unchanged. On `UD-IQ4_XS` (11.3% `iq3_s`) the drafted
  gain is small and was therefore taken twice, across a driver update and a reboot: **+0.1984
  [−0.0334, +0.4301]** in epoch E1 and **+0.1994 [−0.0039, +0.4028]** in epoch E2 — the same sign and
  the same magnitude to three decimals, 6 of 8 cells positive both times. Each interval grazes zero
  alone; the agreement across two independent epochs is the result.

  Correctness `MUL_MAT` 13 OK / 0 FAIL and `MUL_MAT_ID` OK on every build. Rolled 35/40 → 35/40.

- **`q8_1` integer-dot mat-vec pipelines for the ROCmFP4-FAST format, at widths 3 and 5.** The Vulkan
  backend already *listed* this type in its `q8_1` selector and already returned true from
  `should_use_mmvq` for it on AMD — so it quantized the activations on every mat-vec of this type, then
  looked up a pipeline **that had never been created**, got null, and fell back to the float path in
  silence. It was paying the quantize cost for nothing. The port creates the pipelines and the repack
  and dot-product the shader needs.

  **Kernel, on RIG-A** (`m=17408 k=5120`, epoch E2 ladder): `n=1` ×0.991, `n=2` ×1.192, **`n=3` ×1.361**,
  `n=4` ×1.018, **`n=5` ×1.310**, `n=8` ×1.025. The epoch-E1 ladder read ×0.998 / ×1.183 / ×1.335 /
  ×1.015 / ×1.316 / ×1.040 — the gains reproduce across the driver change. The discriminator is the
  control arm: with `GGML_VK_DISABLE_MMVQ=1` both arms take the identical float shader and every row
  flattens to **×0.991–1.007**, so the 1.19–1.36× is this port and not build drift.

  **Serve, on RIG-A, epoch E2** (julianmb ROCmFP4-FAST, 14.56 GB, DFlash2 at draft depth 2, palindrome,
  16 rounds per slot): drafted decode **7.498 → 9.518 tok/s mean**, **+2.0199 [1.3212, 2.7186], 8 of 8
  cells positive = +27%**; median 7.461 → 9.282. Plain decode **5.084 → 5.086**, **+0.0016
  [−0.0168, +0.0199] = tied**. Prefill tied. Rolled **32/40 → 33/40**. **Zero empty completions across
  all 64 rounds.**

  **This is shipped on the fork's kernel branch and is NOT integrated into the default branch, on
  purpose.** Reaching this result cost four intermediate builds, because an earlier version of this
  port made the model emit an empty completion — deterministically, on one prompt, on this file only.
  Three hypotheses were falsified before the cause was isolated to the `n=2` width, which is why the
  shipped gate excludes it. The same shader arithmetic is still live at `n=3` and `n=5`, and the
  reproducer that found the defect cannot reach those widths. Until an adversarial corpus does,
  **fast-but-numerically-wrong is not a win** and this stays off the default branch. The full trail is
  [`docs/FINDINGS.md`](docs/FINDINGS.md) F-13.

- **A tiled concat-transpose for the delta-net conv state**, taken verbatim from an upstream
  contributor. That path transposes straight into a dim-0 concat, so the generic concat walks its second
  source with a 40960-byte stride — one cache line per element, and one memory channel per row. The
  shape, and only that shape, is routed to a 32×32 shared-memory tile transpose.
  **`GGML_VK_CONCAT_TRANSPOSE=0` opts out**, which also gave the measurement its second arm for free.

  **Kernel, on RIG-A** (one binary, arms selected by the environment variable, 4 interleaved launches,
  32 graph blocks, epoch E3): CONCAT **2550.2 → 2224.1 µs per dispatch = −12.79%**, **−15.66 ms per
  `ub512` prefill graph**. The arms do not overlap — `min(OFF) 2510.5 µs > max(ON) 2325.7 µs`. On a
  binary built from the default branch ten commits later, re-measured over three counterbalanced takes
  and reported position-balanced, the same op reads **−11.4349% / −13.988 ms**. Both are banked; neither
  replaces the other, because they are different binaries and, per this project's equipment register,
  potentially different memory configurations.

  **The whole-model effect is a TIE, and this entry does not claim otherwise.** CONCAT is **1.42%** of
  the prefill graph on this box, so the ceiling on the end-to-end effect is **+0.18%** — far below what
  a serve A/B here resolves (paired CI95 halfwidths of **±6.7% / ±5.9% / ±31.2%** on the three prompt
  shapes). Two counterbalanced 8-launch palindromes both read **TIED** at `pp512@ub512`,
  `pp2048@ub512` and `pp2048@ub2048`, and the *sign* flips between the takes on two of the three
  shapes. That is the arithmetically predicted outcome of a +0.18% ceiling under a ±4–31% instrument,
  not a refutation of the op-level gain. The upstream author's **+3.3%** was measured on gfx1151, where
  this op is a larger share of prefill; it is **upstream-reported, neither reproduced nor contradicted
  here**.

  Non-regression: `test-backend-ops -o CONCAT` **195/195, 0 FAIL, both arms** — and that suite
  provably carries **no** transposed-second-source case, so it proves non-regression and **not** coverage
  of the new kernel. The covering checks are the three that do bear on it: generated text
  **byte-identical with the gate on and off, 3/3 prompts** at temperature 0 on two genuinely separate
  server processes; the content floor **0/8 on every slot**; and a reachability receipt showing
  **1392 generation-time dispatches** of the new kernel with the gate on against **0** with it off.

- **`--spec-draft-adaptive`** — draft length sized from a per-sequence moving average of accepted
  tokens instead of a fixed `--spec-draft-n-max`, taken from an upstream contributor. A fixed depth can
  only be right for one kind of content, and its author measured the correct setting to be *opposite*
  for prose and for JSON. The interesting part is that the action censors the measurement: when every
  drafted token is accepted you learn only that acceptance was *at least* the draft length, so
  averaging that lower bound ratchets the depth down and strands it. Full acceptance therefore probes
  upward while partial acceptance is averaged.

  **The numbers in the source commit are upstream-reported, measured on a Radeon 8060S, and are not
  ours.** They are not restated here for that reason. **Default OFF**, and **UNMEASURED on RIG-A** —
  this fork has no A/B for it. Speculation stays distribution-preserving, so the mechanism can move
  throughput and cannot move output.

### Fixed

- **A server slot could loop forever on one token after a partial draft acceptance.** On a partial
  accept the slot restores the pre-round state and re-decodes the accepted tokens to rebuild it — and
  that replay went back through the same verification as a fresh draft. On a backend whose logits move
  with batch shape or memory layout, **and Vulkan is one**, the re-verification can reject a token the
  original verification already accepted; the rejection restores the same checkpoint and replays again,
  and the slot spins on one position emitting nothing. The replayed prefix is now accepted without
  re-verification and only the continuation is sampled. On backends with batch-shape-invariant logits
  the re-verification always agreed, so nothing changes there.

- **Whole-tensor dequantization on every batched matmul of the ROCmFP4-FAST format.** The shaders were
  generated and the type was listed in the mat-mat selector, but no line ever *created* the pipelines —
  the same selector-without-a-pipeline shape as the MMVQ entry above. Every `n>1` matmul on the type
  therefore fell back to staging the whole tensor to f16, which on the 248320×5120 head meant a
  **2.4 GB** staging buffer. The pipelines are created, and the mat-mat loader gained the case for this
  type that it was missing (without which `test-backend-ops` reported an error of 1.0 above `n=8`).
  Separately, the generic mat-vec had its column loop nested *outside* its row loop with the
  dequantization inside, so each block was decoded once per column; the dequantization is hoisted out,
  with the arithmetic per (row, column) unchanged.

  **Measured on RIG-A** (perf logger): the FP4 prefill graph **16112 → 4683 ms** — parity with `q4_K`
  at 5183 GFLOPS — the FP4 width-3 verify graph **338 → 271 ms**, and the seat file's verify graph
  **268 → 259 ms** from the mat-vec hoist alone. `test-backend-ops` MUL_MAT **0 FAIL**; content floor
  **0/8 on three slots**; the serve A/B reads FP4 prefill **+16–20%** against the seat file with the
  interval excluding zero.

### Changed

- **The `q6_k` mat-vec now reads its scales out of the block instead of staging them through shared
  memory.** It was the only `q*_k` mat-vec shader doing so — it loaded all 16 int8 scales of the
  superblock into a double-buffered tile and paid a **`barrier()` per row, inside the row loop**, while
  `q4_k` and `q5_k` unpack their scales in registers with no shared memory and no barrier at all. The
  exchange moves no data between threads: the 16 threads all work on the same block, each writes its
  own scale, and each then reads only four bytes of that same block. Reading them straight from the
  block yields identical values in identical order off the same 16-byte cache line.

  **This measured NEUTRAL on RIG-A and must not be quoted as a speed win.** One binary, arms selected
  by a specialization constant, 6 position-balanced interleaved launches per phase, on the seat serve
  line: at the op level the `q6_k` rows read a median ON/OFF ratio of **0.9796** against a control of
  **78 untouched op rows at 0.9990, p10–p90 [0.952, 1.053]** — **no `q6_k` row separates from the
  control band**. At the serve level, **TIED on all six cells** — prefill and decode, reported
  separately, at three prompt lengths, every arm range overlapping. Output is **byte-identical**: one
  distinct hash per prompt across all 12 arm-launches. `test-backend-ops -o MUL_MAT` **1394/1394, 0
  FAIL, both arms**.

  It is kept because it is **strictly less work for a bit-identical result** — one fewer workgroup
  barrier per row per superblock, and no shared-memory tile — on an engine that ships to other people's
  hardware, where barrier cost and shared-memory pressure differ. `GGML_VK_Q6K_DIRECT_SCALES=0`
  restores the upstream path byte for byte. What this pass did *not* find is why `q6_K` runs about
  **35% below `q5_K`** at the same shape; the per-row barrier was the leading explanation for that
  deficit and it is now **falsified**. See [`docs/FINDINGS.md`](docs/FINDINGS.md) F-14.

- **Base moved to `b10680` (`d7bd3bfca`)**, the latest upstream tag at the time. Two repairs were
  needed after the merge and both are recorded in the series: a call site that upstream renamed and the
  fork's own code had auto-merged past, and a declaration kept from both sides of a resolution.

### Not shipped on the default branch, with reasons

- **The ROCmFP4-FAST MMVQ mat-vec.** A measured **+27%** on the drafted line of its own file, with the
  interval excluding zero on 8 of 8 cells and the content gates green — and still not on the default
  branch, because the shader arithmetic that produced it is unvalidated at the two widths it is
  retained at. What is owed before it lands is named rather than left implicit: an adversarial
  cache-history corpus that reaches width 3, or a direct comparison of the integer-dot and float logits.

- **The `iq3_s` split applied at every width** (`616b877b8`) — **7–9% slower** at `n=2` and `n=4` on
  RIG-A. Superseded by the gated version above rather than kept as an option, because the two widths it
  loses on are widths the shipped gate simply does not touch.

- **`--spec-draft-adaptive` as a default.** Compiled in, off, and unmeasured here.

### Where these actually are, right now

This section documents measured work in this fork's git history. **Most of it is not yet on the
default branch**, and saying otherwise would be the exact overstatement this changelog's rules exist
to prevent.

| commit(s) | where | note |
|---|---|---|
| `5b1b416cb` — tiled concat-transpose | **on the default branch** | landed by compare-and-swap, four fast-forwards, **0 merge commits added**; all four of its gates were re-run on binaries built from the default branch itself rather than carried over from the measuring branch |
| `6c7d9275f`, `8a4044a82` — adaptive drafting, replay fix | on their working branch | awaiting integration |
| `e3220bed0` — ROCmFP4-FAST mat-mat + mat-vec hoist | on its working branch | awaiting integration |
| `1884a620a`, `aa9f0e2aa` — `iq4_xs` and `iq3_s` mat-vec | on their working branch | measured wins, awaiting integration |
| `616b877b8` — the every-width `iq3_s` split | on its working branch | **superseded by `aa9f0e2aa`** and kept as history; it is the measured 7–9% loss listed above |
| `60f57787f`, `ab04258bf`, `294dfdbdd`, `8e2acfd21`, `f68a4bd25` — ROCmFP4-FAST MMVQ | on its working branch | **held there deliberately** — see the residual above. The five narrow the width gate in order, each on what the one before ruled out, so a subset of them is a different and untested change rather than a smaller one |
| `da218bab7` — comment correction | on its working branch | comment-only; no behaviour |
| `4bbebd619` — `q6_k` direct scales | on its working branch | awaiting integration |

None of them, including the one that landed, is yet a file in
[`patches/series/`](patches/series/). That directory is a generated mirror of the default branch, and
the only lawful way to add to it is a regeneration that currently **stops on ten pre-existing merge
commits** in the fork's history. Regenerating by hand would produce a green-looking artifact that
fails at the next replay, so it was not done. The commit that did land was instead verified at the
right scope — its own patch applies cleanly against the replay result — and the manifest rows the
regeneration will emit are staged in
[`patches/series/PENDING-ROWS.md`](patches/series/PENDING-ROWS.md). Unblocking it needs the fork's
default branch linearized.

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

  **Speed, measured on RIG-A** (same protocol, modes interleaved, n=12 per cell, all ranges
  non-overlapping): decode **2.01 -> 6.67 tok/s (+231.6%)** with the buffer claimed and
  **6.69 (+232.6%)** with dual residency; prompt **59.09 -> 17.44 (-70.5%)** claimed but
  **85.84 (+45.3%)** dual. The scalar g128 path was 2.01 tok/s, so this is a **3.3x decode
  win** - much larger than g64's +22-24%, because g64 already had a fast scalar `vec_dot`.
  Dual residency wins **both** axes here, where on g64 it could only be said not to lose
  prefill; the prompt mechanism is labelled ASSUMED in FINDINGS.

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
