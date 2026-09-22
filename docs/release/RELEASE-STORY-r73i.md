# The Arifi Labs llama.cpp fork — R73 release story (DRAFT)

> **Where the receipts live.** Receipt paths of the form `r66-evidence/…`, `r65-evidence/…` and other `r*-evidence/…` names in this document are **lane-local**: they name files in the lane worktree that produced them, not paths in this repository. A clone does not contain them. `evidence/README.md` lists what is tracked here and says plainly what is not.


> **DRAFT for the President's read. Nothing here has been published. No GitHub push has happened,
> no remote has been touched.**
> Written 2026-09-19 by a docs maker. It **carries forward** the R56 draft
> (`2026-09-16-arifilabs-llamacpp-fork-r56-RELEASE-STORY.md`, studio commit `72d6689c`, 481 lines,
> *"Draft ends. Publication is the President's call."*) and extends it through the R66 promotion.
> The R56 file is left in place, unedited.
> **Extended to R73 on 2026-09-21 by the release-prep lane.** The identity table in §1, the new
> §5a and §6a, and the closing gates section carry R73; **the R56 and R66 narrative below is left
> as written**, because it is the history of how the fork got here and re-writing it would destroy
> the record. Where a section says "R66", it is a statement about R66 and is still true.
> Every number carries the receipt path it came from. A number we could not source is written
> **OWED** — never invented. **CHECKED** = read from a receipt or ledger row. **ASSUMED** = a reading
> that no measurement pins down. **UNRESOLVED** = two CHECKED records disagree, or the instrument
> cannot separate the claim from its own noise.

**The mandate, verbatim (President, 2026-09-16):**

> "chase performance gains everywhere through a greedy path without breaking things or degrading quality and if possible increasing quality too."

That sentence is the whole design. Greedy: take every gain, on every format, on every model, wherever
it is. Without breaking things: bit-identity proofs, correctness sweeps and quality gates sit in front
of every merge. Without degrading quality: a speed win that moves the output is not a win.

**Who this fork is for (President law, 2026-09-18 ~19:5x, verbatim from seat-62 §4l):**

> "The fork is for EVERYONE on Vulkan, not for the 780M. A proven-safe improvement (bit-identical or
> PPL-gated, never slower here) SHIPS ON even when our GPU shows no gain — other drivers/GPUs may not
> do what AMD's driver already does."

That law is why one of the three R66 defaults ships ON with **zero measured wall-clock change on our
own GPU**. Receipt: `board/hq/succession/2026-09-18-hq-seat-62-state.md` §4l.

Company law behind the mandate: `registers/company/canon.json` →
`canon-maximum-gains-everywhere-all-quants` (Adopted 2026-09-03, President verbatim 2026-09-02):
*"it's fine to put effort into all quants, etc… I want maximum gains everywhere"*.

---

## 1. What this is

| Item | Value | Receipt |
|---|---|---|
| Upstream base | `ggml-org/llama.cpp` tag **b10825**, `9e0e220594af405a62835dc3a27495729fd8506b` | `cache/main-wt/tools/arifi-sync/sources.json` → `base` |
| Our series | **588 patches**, all non-merge, applied in filename order | `cache/r73-wt/r73-evidence/73-regen.txt`, `74-series-check.txt` |
| Branch tip | `arifi/main` = **`037b433a9`** (series tip), source-code tip **`5a7434218`** | `cache/r73-wt/R73-PROMOTION-RECORD.md` §1 |
| Tag | **`r73i-release-engine-2026-09-21`** → `037b433a9` | `r73-evidence/75-tag.txt` |
| Integrity | `series check` **PASS** — every patch byte-identical to a fresh generation *and* to a committed blob; **588** patches replayed cleanly, replayed tree **IDENTICAL** to `arifi/main` outside `patches/series`, rc=0 | `r73-evidence/74-series-check.txt` |
| Engine of record | **`arifi-b10825-r73i-5a7434218`** — the RELEASE ENGINE (superseding `arifi-b10825-r66i-f6f5ab0cf`, which stays on disk as the rollback, with `r56i-280b1553f` behind it) | `registers/current-state.json`; `r73-evidence/76-flip.txt` |
| Promotion | ff `94461e770` → `5a7434218`, 2 commits, 0 merges, **LOCAL ONLY — nothing pushed** | `r73-evidence/71-promote-ff.txt` |
| Trailers | **0 untrailered commits** in the R73 range | `r73-evidence/72-trailers.txt` |

*(The R66 identity this table replaced — series 586, tip `94461e770`, tag
`r66i-integration-2026-09-19`, engine `arifi-b10825-r66i-f6f5ab0cf`, ff `1b6185951` → `f6f5ab0cf`
over 20 commits — is the previous release and is recorded in `cache/r66-wt/R66-PROMOTION-RECORD.md`.)*

### The measurement box — every number below was measured here

Beelink SER7 Pro — AMD Ryzen 7 7840HS, **Radeon 780M (RDNA3, gfx1103)**, driver 32.0.31041.1004,
Windows 11 build 29648, BIOS `SER7PRO_P5C8V38`, Balanced power plan, Vulkan backend with
`KHR_coopmat`, no discrete GPU.

Rig stamp (`python -m arifi_core.rig_stamp`, 2026-09-19): rig `beelink-ser7-pro`, **RAM epoch
`32+16-asymmetric`**. **One pool of RAM. There is no VRAM on this machine.** 48 GB physical with a
16 GB BIOS reservation — the President-set maximum of the 4 / 8 / auto / 16 options. Speed on this box
is **bytes per token against bandwidth achieved**, and nothing else. `ARIFI_GPU_BUDGET_GIB=26` is our
own load-governor policy switch, not a second memory. Register:
`registers/equipment/beelink-ser7-pro.md`; rig history `registers/rig-history.jsonl`.

**Kernels transfer, numbers do not.** Every figure in this document is stamped with that rig and that
RAM epoch. Nothing here is a claim about your GPU.

Bandwidth reference used throughout:

| Number | Value | Receipt |
|---|---|---|
| Theoretical, dual channel DDR5-5600 | 89.6 GB/s | `research/local-inference/2026-09-10-bandwidth-envelope-program.md` |
| Best ever measured on this box | 78.4 GB/s (q4_K decode map, lane-185) | same |
| Beyond-reservation host-visible path (R58 probe, 4 GB, best of 3) | 39.9 GB/s (memory type 1, uncached) | `cache/r58-memtype-wt/r58-evidence/10-memtypes.txt`; seat-60 §3g |

### The product ruling

2026-09-16 (seat-60 §3h): this fork is the **first Arifi Labs open-source product**. The release is a
story with dated receipts, and the President reads the draft before anything is published. That ruling
is why this document exists and why nothing has been pushed.

---

## 2. What we ingested, and from where

The fork is built by *greedy ingestion*: anything that is a quality-safe gain gets taken, pinned, and
replayed as a patch with a provenance trailer. Nine upstream/third-party remotes are tracked with a
reviewed pin each.

| Remote | URL | Pin | Patches taken (by trailer) |
|---|---|---|---|
| upstream | https://github.com/ggml-org/llama.cpp | `d7bd3bfca` | base b10825 + 2 |
| powerinfer | https://github.com/Tiiny-AI/PowerInfer | `8bd56d699` | 12 |
| prisml | https://github.com/PrismML-Eng/llama.cpp | `7529fdaaf` | 9 (+3 `prisml/prism*` rows) |
| rocmfpx | https://github.com/charlie12345/ROCmFPX | `0a59add89` | 5 |
| tq3 | https://github.com/turbo-tan/llama.cpp-tq3 | `7e24c7caa` | 3 |
| ciru | https://github.com/ciru-ai/ROCmFPX | `64d528be7` | OWED (no trailer rows counted) |
| buun | https://github.com/spiritbuun/buun-llama-cpp | `39d97a876` | 1 |
| zuijdwijk | https://github.com/LaurentZuijdwijk/llama.cpp | `f97c0e6fe` | 3 |
| turboq | https://github.com/jtrefon/llama.cpp-turboq-mtp | `6a02d0494` | 1 |
| (via trailers) TheTom/llama-cpp-turboquant | — | — | 9 |
| (via trailers) thecodacus/llama.cpp | — | — | 3 |

Receipt: `cache/main-wt/tools/arifi-sync/sources.json` (remotes + pins); counts computed from the
`Taken-from` column of `cache/main-wt/patches/series/MANIFEST.md` (566 rows at R56; the R66 regen
added 20 entries, 0567–0586, **none of which carries a `Taken-from:` trailer** — CHECKED by scanning
`patches/series/05*.patch`, where the highest-numbered file carrying one is `0532`, so the per-remote
counts above are unchanged by R66).

The series is generated from git, never hand-edited, and `series check` replays it with `git am` and
diffs the result against `master` on every run. Rebuild it yourself:

```bash
git checkout -b my-rebuild 9e0e22059
git am patches/series/*.patch
```

### S-X8 provenance and licence

The S-X8 v4.3 format is **not ours**. It is the work of **Martí Vidal Leandro (MarlaLabs)**, Apache-2.0,
DOI **10.5281/zenodo.21922640** (v1.0, 2026-08-13). We fetched the paper, methodology, container spec,
reference CUDA kernels and the author's own llama.cpp patch, with a sha256 per file, on 2026-09-15.
What we wrote is the **Vulkan** port and the correction path.
Receipt: `research/local-inference/lane-evidence/2026-09-15-sx8-paper-docs/PROVENANCE.md` (10 files, sha256 each).

### Upstream currency

2026-09-15 review of **b10825 → b10903**, the Vulkan / allocator / loader / server surface: 24 commits
classified — **11 TAKE-VERBATIM, 5 TAKE-BEHIND-SWITCH, 8 CONFLICTS-WITH-LANE**. Classification is by
hunk-range overlap, not file overlap (the fork adds 4,390 lines to `ggml-vulkan.cpp` alone, so file-level
overlap is meaningless). The base move is scheduled as **R55** and has not happened.
Receipt: `research/local-inference/lane-evidence/2026-09-15-upstream-currency-b10825-b10903/UPSTREAM-VULKAN-DELTA-REVIEW.md`.

### One open item before any publication

`sources.json` → `other_remotes` records remotes that were configured and fetched while appearing in no
governed artifact. Its own note: *a fork about to be published should not ship remotes no register
accounts for.* **Disposition is OWED from the President** — track them with a reviewed pin, or remove them.

---

## 3. Formats added, and how

| Format | Type id | What we added | Dated result | Receipt |
|---|---|---|---|---|
| **S-X8 v4.3** (MarlaLabs) | 57 | Full Vulkan port: mat-vec, `mul_mm`, MMQ branch, prompt-processing route, whole-block decode, packed tile decode under coopmat, PCA correction path, companion identity guard | 2026-09-08 first Vulkan kernel; shipped from R56 on | `board/programs/local-inference/lanes/2026-09-08-lane-224-sx8-vulkan-REPORT.md`; `…2026-09-10-lane-230-r46-sx8-mmv-REPORT.md`; `…2026-09-15-lane-243-r52b-sx8-coopmat-mulmm-REPORT.md`; `…2026-09-15-lane-242-r53-sx8-pca-REPORT.md` |
| **TBQ3_0 / TBQ4_0** (TurboQuant) | 58 / 59 | Fork-owned type ids, write path and Vulkan coverage | 2026-08-18 write path | `…2026-08-18-lane-150-tq-writepath-REPORT.md` |
| **TQ3_1S / TQ4_1S** | 45 / 46 | Subgroup mat-vec rewrite (`mul_mat_vec_tq_sg.comp`, register WHT via `subgroupShuffleXor`, zero hot-loop barriers) | 2026-08-20, landed `69324887f` | `…2026-08-20-lane-163-REPORT.md` |
| **TQ3_4S / TQ3_0 / TQ3_4SE / TQ3_1S_SHIFT** | 48 / 49 / 50 / 51 | CUDA + Metal lift from `tq3/master`; **no Vulkan coverage** — census says NONE in all five stages | 2026-08-20 (CUDA `28e4af5c0`, Metal `827f683e4`); hardware validation OWED by design | `…2026-08-20-lane-160-REPORT.md`; `research/local-inference/2026-08-17-vulkan-kernel-coverage-census.md` |
| **Q2_0_G128** (ternary, PrismML lineage) | 43 | Full Vulkan coverage: dequant, get_rows, mat-vec, `mul_mm`, `mul_mm_id` | 2026-08-17 census confirms all five columns present | census §1 |
| **ROCmFPX family** — Q4_0_ROCMFP4, Q4_0_ROCMFP4_FAST, Q6_0_ROCMFPX, Q8_0_ROCMFPX, Q3_0_ROCMFPX, Q2_0_ROCMFPX | 100–104, 107 | dequant + get_rows + mat-vec on Vulkan; `mul_mm` / `mul_mm_id` are NONE; Q6_0 numerical fix; honest `supports_op` shaped as a pipeline-existence query | 2026-08-17 | `…2026-08-17-lane-145-REPORT.md`; `…2026-08-17-lane-146-REPORT.md`; census §1 |
| **NVFP4** | — | Trialed as a model format on the 35B/27B lines, not a kernel we wrote | 35B NVFP4 > iq4_xs graft, blind-read; 9B NVFP4 > Q4_K_M 4–2 blind | SPINE §3 (DPR-080, DPR-086) |

**The census correction that made all of this honest** (lane-144, 2026-08-17): a format can be at three
independent stages — SPIR-V generated, host pipeline created, dispatched at runtime — and prose had been
collapsing them. "The shader exists" had repeatedly failed to mean "it runs", because an uncreated
pipeline entry is a valid pointer to an all-null struct, not a null. The empty-pipeline guard
(`9755b2946`) turns that into `nullptr` and sends the caller down the dequant fallback. Every cell in the
coverage table is a `file:line` citation or an explicit NONE.
Receipt: `research/local-inference/2026-08-17-vulkan-kernel-coverage-census.md`.
Per-format state as it stands today: `research/local-inference/KERNEL-FORMAT-MATRIX.md`.

---

## 4. Kernels attacked, per quant — which, why, what came back

Attack order is set by **byte share of the file under test**, never by which model happens to be seated.

| Wave | Target | Why that target | Result, dated | Receipt |
|---|---|---|---|---|
| **R46** (2026-09-10 → 09-14) | S-X8 whole-block decode | The S-X8 decode read the weights twice per width | 27B plain **1.831 → 2.247 t/s (+22%)**, CI95 [+0.386, +0.422], 8/8, 2026-09-14 | `…lane-230-r46-sx8-mmv-REPORT.md`; `lane-209/r46i-lane-230/c1-sx8-27b/SX8/AB.json` |
| **R48** (2026-09-14) | q5_K mat-vec activation hoist | q5_K is **33.5%** of the Q4_K_XL 27B file by bytes | shipped gated to n≤3 | `…lane-234-r48-q5k-matvec-REPORT.md` |
| **R48b** (2026-09-14) | MMVQ A-side hoist + per-type/width gate + route change | Decode each weight block once per row and k-slice instead of once per column | **The largest measured kernel win of the wave.** S-X8 lm_head 248320×5120 widths 4–8: **51.9/43.0/40.7/37.2/35.7 → 73.0/73.4/73.3/72.7/71.0 GB/s** (+29% to +50%, flat at the bus) | `lane-234/32-paired-r48ivsr46i.txt` |
| **R48b** | S-X8 ffn-gate-up 17408×5120, widths 2/3 | Narrow widths were off the route | 31.6/27.6 → **36.5/36.5 GB/s** (+13% / +24%) | same |
| **R48c** (2026-09-14) | q6_K x-fold + activation hoist + direct scales | q6_K is **19.7%** of the Q4_K_XL 27B file | shipped (`GGML_VK_Q6K_DIRECT_SCALES`, serve bit-identity IDENTICAL ×3) | `…lane-236-r48c-q6k-matvec-REPORT.md` |
| **R48b** | q4_K mat-vec, widths 5–8 | The wide-width tail on the interactive-brain file | 45.6/35.7/35.5/29.0 → **55.3/51.7/47.9/45.2 GB/s** (+17% to +36%); tail shortened, not closed | `lane-234/32-paired-r48ivsr46i.txt` |
| **R50** (2026-09-15) | q4_K wide-width tail | Chase the tail to the bus | **BLOCKED with receipts.** Both mechanism candidates falsified. Switch `GGML_ARIFI_KQ_MMVQ_ROWS` ships default 0 as a pairing instrument | `…lane-239-r50-q4k-wide-width-REPORT.md`; `cache/r50-q4k-wt/CHECK-R50-FABLE.md` |
| **R52** (2026-09-15) | S-X8 integer MMQ prefill | Premise: MMQ would beat the coopmat path | **Premise refuted.** MMQ is dead for every type on the 780M — the coopmat arm registers zero MMQ pipelines | `…lane-241-r52-sx8-int-mmq-REPORT.md` |
| **R52b** (2026-09-15) | Packed S-X8 tile decode under coopmat | Find where the S-X8 prefill deficit lives | Packed tile decode = **bit-identical TIE, ships**. `GGML_ARIFI_MMQ_UNDER_COOPMAT` default **OFF** (q8_0 arm lost 37% at graph level) | `…lane-243-r52b-sx8-coopmat-mulmm-REPORT.md`; seat-60 §3i |
| **R64** (2026-09-17) | q5_K → MMVQ **route** at n=5..8 | q5_K's ffn mat-vec marginal column cost 128 us vs q4_K's 27; q5_K = 33.5% of the served Q4_K_XL file | **Marginal verify column 113.8 → 5.4 us/col**; ratios 1.067 / 1.109 / 1.095 / 1.204 at n=5..8, 6/6 rounds each; n=1 0.999; `k<=8192` gate measured; **PPL identical** | seat-61 §3p, §3s; `cache/r64-q5k-route-wt/OPUS-R64-Q5K-MMVQ-ROUTE-REPORT.md`; `r64-lane-252/c24-q4kxl-27b/SEAT/AB.json` |
| **R65** (2026-09-18) | The k-quant MMVQ **width-4 knee** (79 → 61 GB/s at n=3→4) | The knee caps every k-quant verify column | **Refuted as a B-side capacity effect**: width-locked across k=4096..12288 under a pre-registered falsifier; B read once per workgroup; split-k not bit-identical; rows 1→4 10% slower | `cache/r65-width-knee-wt/CHECK-R65-FABLE.md`; `r65-evidence/22-ksweep-table.txt`; `registers/closed-levers.jsonl` → `mmvq-width-knee-capacity` |
| **R61** (2026-09-18) | iq3 mat-vec: sign hoist + the n=7 cliff | iq3_s is the GSQ 27B line's format | **Two results, opposite kinds** — see §5 | seat-62 §4j, §4k |
| **R60 / R57** (2026-09-16/17) | S-X8 small-n tile decode; a cheaper q4_K dot | The two named prefill/decode levers | **Both refuted**, no switch shipped from R60; R57 cuts 1–3 refuted from the thread→data mapping | `registers/closed-levers.jsonl` → `sx8-smalln-tile-decode`, `q4k-instruction-cuts` |

---

## 5. What R66 shipped — three defaults ON, each with a legacy switch

R66 (lane-254) is one integration commit that flips three Vulkan mat-vec defaults ON. Every one keeps
the old behaviour reachable by an environment switch. Checker: `cache/r66-wt/CHECK-R66-FABLE.md`
(Fable 5.1) — **PASS-GATED, PROMOTE**. Record: `cache/r66-wt/R66-PROMOTION-RECORD.md`.

| # | Default now ON | Scope | Legacy switch | Startup line it prints |
|---|---|---|---|---|
| 1 | q5_K MMVQ **route** | AMD only, `n=5..8`, `k<=8192` (`ggml-vulkan.cpp:9627`, admit `:12459`, `:12487-12491`) | `GGML_ARIFI_Q5K_MMVQ=0` | `q5_k mmvq route: route (n=5..8, k<=8192) (device-probe)` |
| 2 | iq3 mat-vec sign-hoist **v2** | all devices (`:7455`, fallback `:7464`) | `GGML_ARIFI_IQ3_MMVQ=1` (v1) | `iq3 mat-vec sign-hoist: v2 [GGML_ARIFI_IQ3_MMVQ]` |
| 3 | iq3 mat-vec **n=7 rows = 2** | RDNA3 only, n=7 pipeline only (`:7493`, `:7509-7511`) | `GGML_ARIFI_IQ3_N7_ROWS=4` | `iq3 mat-vec n=7 rows: 2 (device-probe (RDNA3))` |

Line cites are the checker's corrected ones (CHECK-R66 F1). Source: `R66-PROMOTION-RECORD.md` §2.

### 5.1 q5_K MMVQ route — a real op-level win, no served effect

**Before → after: the marginal q5_K verify column costs 113.8 us, then 5.4 us** (q4_K's is 27).
Paired 6-round measurement on one binary, 17408×5120: route/default ratios **1.067 / 1.109 / 1.095 /
1.204 at n=5..8**, 6/6 rounds each; n=1 is 0.999 (the route is inert there). The `k<=8192` gate was
measured, not assumed — at k=17408 the route loses 0/6 at n=5, and **90.5%** of the served file's q5_K
bytes are k≤8192. Widths n≥5 are where MMVQ switches from 1 row per workgroup to 4.

**Quality: identical.** Perplexity at batch 8 on Q4_K_XL read **5.9932 ± 0.14292** with the route on
and **5.9918** on the legacy arm — the same number twice, once in chain149 (R64) and again in chain150
(R66). Receipts: seat-61 §3p/§3s; `r64-lane-252/c24-q4kxl-27b/SEAT/AB.json`;
`100-r64-q5k-route/10-,11-ppl-*-b8.txt`; seat-62 §4q.

**Served effect: none shown.** Every valid served contrast is a tie (§6). The op-level number is the
claim; the served number is not.

The maker withdrew two of its own lines before shipping: the n=3/4 "loss" (a null control bounds the
artifact at 1–3%) and the k=17408 n=7/8 row. Named here because a withdrawn claim is part of the record.

### 5.2 iq3 sign hoist v2 — ships ON with no measured gain on our GPU

The v2 shader hoists the iq3 sign application out of the `NUM_COLS` loop. The SPIR-V it emits drops
from **1229 to 349 instructions per column** (counted with SPIRV-Tools). Wall clock on gfx1103:
**nothing** — v2/legacy ratios 0.99–1.01 at every width, 6 rounds, both iq3 types. Bit-identity passes
sha-for-sha at n=1..8, and a planted defect reddens the v2 arm only, so the arm is provably live.

Why it buys nothing here, honestly: the AMD driver's own compiled pipeline statistics are **identical
between the two arms at all eight widths**, so the driver's compiler is **ASSUMED** to perform the same
hoist itself. Other drivers are untested. Under the President's §4l law this ships **default ON on all
devices** — fewer instructions per column for the drivers that do not already do it, and never slower
here. Receipts: seat-62 §4j, §4k (`cache/r61-iq3-wide-wt/CHECK-R61-FABLE.md`).

For our own throughput program the idea is **closed** — that closes the idea for our t/s, never the
code. Register row owed: `registers/closed-levers.jsonl` → `iq3-sign-hoist` (seat-62 §4w, OWED).

### 5.3 iq3 n=7 rows = 2 — a 21–25x latent cliff, removed

The iq3 mat-vec had a cliff at exactly one width. It is a **register spill**, read from the AMD
driver's own `GGML_VK_PIPELINE_STATS`: `scratchMemUsageInBytes` = 768 at n=7 and 0 at every other
width; VGPRs used at n=1..8 run 115/133/134/**216**/165/180/239/**255** (255 is the RDNA3 maximum).
No disassembler was needed — the driver reports it.

Confirmed 6/6 rounds at three shapes (2026-09-19):

| Shape (iq3_s) | Before | After | Factor |
|---|---|---|---|
| m=248320 | 465,400 us/run | **20,890 us/run** | ~22x |
| (second shape) | 34,674 | **1,636** | ~21x |
| (third shape) | 39,284 | **1,573** | ~25x |

iq3_xxs gains 1.20–1.42x, 6/6; n=6 and n=8 are flat. Identity: five OP_DUMP arms produce one sha
(`271472A9…C993`), 76 executed cases each. Scratch goes 768 → 0 under rows=1 and rows=2, and back to
768 at rows=4. Receipts: seat-62 §4h (`cbefd4e30`), §4j, §4p.

**What it is and is not.** Dispatch width is `1 + draft` (`server-context.cpp:809-819`), so only a
draft depth of 6 ever reaches n=7 — and our serve line runs n-max 4. This is a **latent-cliff removal**
that protects anyone running draft depth 6, not a change to our own throughput. RDNA3-gated, iq3_s and
iq3_xxs, f32 and f16 pipelines.

### 5.4 What else rode along, inert

R57 `GGML_ARIFI_MMV_MAX_COLS`, the R65 rows knob, R61 `GGML_ARIFI_OP_DUMP`, R60 coverage rows — all in
tree, all default-inert. Receipt: seat-62 §4w.

---

## 5a. What R73 shipped — one new default ON, and it is honest about its reach

R73 (lane-257) is the **release engine**, `arifi-b10825-r73i-5a7434218`. It integrates R71's q6_K
work and flips **one** new Vulkan default ON. The three R66 defaults are unchanged and still print
at startup. Checker: `cache/r73-wt/CHECK-R73-FABLE.md` — **"VERDICT: PASS. PROMOTION RULE: PROMOTE
`arifi-b10825-r73i-5a7434218`"**. Record: `cache/r73-wt/R73-PROMOTION-RECORD.md`.

**Read the reach before you read the size column.** The route fires at **n=7,8**, a speculative
decoder verifies at **width = 1 + draft depth**, so this default is reached by a **draft depth of
6 or 7** and is **latent at our own served draft depth of 4** — zero served tokens change on this
box. The section below states it in full.

| Default now ON | Scope | Size | Legacy switch |
|---|---|---|---|
| **q6_K q8_1 MMVQ route** | AMD, **n=7,8 only**, **MUL_MAT only** | **+5% to +40% per q6_K verify column**; **58/60 paired rounds won** across five shapes; **worst single round 0.9894** | `GGML_ARIFI_Q6K_MMVQ=legacy` |

Upstream keeps q6_K off the MMVQ path on an unmeasured source comment ("only a win on Intel"). We
measured it at the widths a speculative decoder actually verifies at, and admitted only the cells
that won.

**Per shape, legacy/route (>1 = route faster), 6 counterbalanced paired rounds each**
(`R73-PROMOTION-RECORD.md` §2; seat-62 §4ah):

| Shape | n=7 | n=8 |
|---|---|---|
| 248320×5120 (lm_head) | **1.400** 6/6 (20,046 → 14,313 us) | **1.131** 6/6 |
| 5120×17408 | **1.163** 6/6 | **1.050** 6/6 |
| 5120×6144 | **1.125** 6/6 | 1.058 5/6 |
| 17408×5120 | **1.122** 6/6 | **1.085** 6/6 |
| 1024×5120 | **1.088** 6/6 | 1.009 5/6 — **a tie, disclosed not banked** |

**`MUL_MAT_ID` is excluded — a measured wash, not an oversight.** The MoE path does not take this
route.

### The reach, stated plainly, because it is the part a benchmark table hides

The route fires at **n=7 and n=8**. A speculative decoder verifies at **width = 1 + draft depth**.
So this default is reached by a **draft depth of 6 or 7** — which is the depth the **DFlash2
publisher recipe** calls for — and it is **latent at our own served draft depth of 4**, which
verifies at width 5.

**Therefore R73 changes zero served tokens on our box today, and no throughput claim is made for
it.** The promotion record says so in its own words: *"Served effect today: none … No t/s claim is
banked from this promotion."* The win is real, it is per-column at verify widths 7–8, and it is
banked as an op-level number only. Anyone running a deeper draft gets it on the day they upgrade;
we do not, yet, and we are not going to imply otherwise by quoting the +40% without this paragraph
attached to it.

That is the same law that put one of the three R66 defaults on with zero measured change here: the
fork is for everyone on Vulkan, not for the 780M.

### The gates R73 passed

- **47 rc files, all rc=0** — 24 test cells + 12 bit-identity runs, including a set re-run beside a
  live 27B load (`r73-evidence/*.rc`).
- **q6_K correctness 83/83.** The planted-RED arm fails 21 cases, **all of them at n=7,8**, including
  both newly added shapes, and the tree restores byte-identical afterwards.
- **Perplexity at `-b 7` and at `-b 8` within one stderr of each other.** The `-b 7` arm asserts
  `batch_size=7` per arm, so the route was genuinely dispatched: 5.9897 ± 0.143 → 5.9947 ± 0.143.
- **Bit-identity 6/6** against the r66i stage — with the scope stated: greedy runs never reach q6_K
  at n=7/8, so this proves **no collateral change**, not route correctness, which rests on the
  83/83, the RED arm and the PPL pair.
- **Series check PASS**, 588 patches replayed, tree IDENTICAL outside `patches/series`, rc=0.
- **Post-flip smoke rc=0**, 37.4 s, all four startup lines present.

Startup lines as captured (`77-smoke.txt`):

```
q5_k mmvq route: route (n=5..8, k<=8192) (device-probe)
q6_k mmvq route: route (MUL_MAT only, n=7..8) (device-probe)
iq3 mat-vec sign-hoist: v2 [GGML_ARIFI_IQ3_MMVQ]
iq3 mat-vec n=7 rows: 2 (device-probe (RDNA3)) [GGML_ARIFI_IQ3_N7_ROWS]
```

### What R73 left open, verbatim from the checker

- **1024×5120 n=8 is a tie** (1.0087) under the lane's own 1.03 admit bar — re-run with the next
  decider; fence by shape if it ever reads below 1.
- q6_K × **f16** `src1` (the 17408×5120 f16 case) and 5120×10240 were **never timed** — correctness
  only.
- A source comment still reads "36/36, worst 1.003" against the true 58/60, worst 0.9894. It is a
  comment fix owed to the next lane that touches that file; a promotion makes no code changes.
- `docs/OPTIONS-REGISTRY.md` carries two rows for `GGML_ARIFI_Q6K_MMVQ`; the older one is stale and
  they need merging.
- The real follow-up is the **q6_K shader lane** — both pipelines have cliffs (MMVQ at n=5,6; f32 at
  n=6,7), and that is what would reach verify width 5, where our own serve lives.


## 6. What we measured, and what we could not

This chapter is the one most forks skip. It is why the numbers above can be quoted at all.

### 6.1 The served lines tie

R66 vs the R56 stage, 16 rounds, INTEGRITY clean, rig-stamped:

| File | decode, drafted (DFlash2) | decode, plain | rolled40 |
|---|---|---|---|
| S-X8 27B | 5.433 → 5.530, CI95 [−0.15, +0.35] — **TIE** | 2.109 → 2.103 — **TIE** | 35/35 |
| GSQ IQ3_S 27B | 7.771 → 7.681, CI95 [−0.24, +0.06] — **TIE** | 5.157 → 5.166 — **TIE** | 31/31 |
| Q4_K_XL 27B | **INVALID session** (see 6.2) | 4.095 → 4.108 — **TIE** | — |

The ties hold at launch level as well as cell level. Receipts: `r66-lane-254/c27-sx8-27b/SX8/AB.json`,
`c28-gsq3s-27b/GSQ3S/AB.json`; `CHECK-R66-FABLE.md` §2; seat-62 §4s.

**Both served lines moved to R66 on a tie, not on a win.** There is no CI-clean served win in R66, and
the promotion record says so: *"No served-throughput GAINS row is proposed for R66: every valid served
contrast is a tie."*

### 6.2 The harness defect we found in our own instrument

This is the most important negative result of the release, because it retroactively demotes some of
our own earlier claims.

**The null control.** In chain149 the *same binary* ran in both arms with the switch inert in prefill.
It should have read zero. It read **plain prefill 58.92 vs 53.62 t/s, +5.30, 16/16 rounds,
CI95 [+4.77, +5.83]** — identical code, "CI-clean". Receipt:
`r64-lane-252/c24-q4kxl-27b/SEAT/AB.json`; seat-62 §4t.

**The mechanism.** Each arm × mode used only **2 server launches**; the 16 "cells" are pseudo-replicates
of those 2 loads, and the CI is computed over cells. Prefill and plain decode carry a **per-launch
level** that the CI never sees. The checker measured same-arm launch pairs differing **4–26%** in
prefill, and found the launch order was **fixed** (the FIX arm always in positions 2–5), violating the
counterbalancing law F-145-AMEND-1. Receipt: `CHECK-R66-FABLE.md` §3; seat-62 §4u.

**What it cost us.** chain150 first read a "prefill regression" of **−11.7%** on 27B S-X8
(42.32 → 37.35) and −3.3% on GSQ, and R66 was **not promoted**. A dedicated bisect lane (R66b, 84
llama-bench launches / 276 pp cells, rc=0) then showed **no engine regression**: 4B S-X8 pp128/512
−0.36%/−0.09%; a third arm with all three flips reverted by environment never recovers R56 either, so
the flips are not a cause; the prefill path `ggml_vk_mul_mat_q_f16` is untouched and there is zero diff
in `common/` or `tools/server/`. The −11.7% was **one launch** (33.66 vs 45.26). Receipt:
`cache/r66-wt/OPUS-R66B-PREFILL-REGRESSION-REPORT.md`; seat-62 §4t; `CHECK-R66-FABLE.md` §3.

**What it demotes, named.** Two rows carried in the R56 draft are now **UNRESOLVED**, not withdrawn and
not confirmed:

- *"Q4_K_XL drafted, registered line: R48i 7.656 → R56 7.832, +2.3%, CI95 [+0.11, +0.38], 8/8 — the one
  CI-clean win in the R56 integration."*
- *"plain prefill +1.7 t/s CI-clean on R56"* and the R56 promotion's *"prefill 35.6 → 39.4"*.

Both are small deltas from 2-launch cells, which is exactly the class the null control invalidates.

**UNRESOLVED — the two CHECKED records disagree on where the bar sits.** Seat-62 §4t writes *"mark every
banked served prefill / plain-decode 'win' or 'loss' of **≤10%** from 2-launch cells as UNRESOLVED"*.
`CHECK-R66-FABLE.md` §5.4, as quoted in `R66-PROMOTION-RECORD.md` §4, writes *"Every banked served
prefill or plain-decode delta **≤ ~25%** from 2-launch cells is UNRESOLVED (R56's '35.6 → 39.4'
included) — HQ wrote ≤10%; C1 moves the bar."* We do not pick one. Both bounds are recorded, both
paths are above, and the rows are marked UNRESOLVED under either.

The fix is in flight: CI over **launch** means (or ≥8 launches per arm), counterbalanced launch order,
and an invalid-launch gate. Until it lands, no new served delta of this size is quotable.

### 6.3 The unresolved prefill residual

After the artifact was accounted for, one number did not go to zero and did not stay a loss:
**27B S-X8 prefill under llama-bench, −1.7%, CI95 [−4.4, +1.0]**; pp512 −1.29% at 2/6 sets. The checker
banked it as **UNRESOLVED — "not shown nonzero, not shown zero; never bank as zero"**, and named the
closure: ≥12 counterbalanced sets at pp512 on the serve config (`-fa on -ctk q8_0 -ctv q8_0 -ub 512`),
arms A/B/C. Receipt: `CHECK-R66-FABLE.md` §3; `R66-PROMOTION-RECORD.md` §4.3.

### 6.4 The Q4_K_XL drafted session we threw away

Two of four drafted launches in chain150's Q4_K_XL cell recorded **draft acceptance 0.000** — one on
R66 (04:24), one on R56 (04:57) — while the other two accepted 0.576 normally. Both engines, so it is
not an R66 regression; **cause UNKNOWN and not reproduced**. The harness printed a "CI-clean +0.207 win"
on that garbage, which is the proof the invalid-launch gate is needed. A first guess (missing host
split) was **killed by the next day's census**: 8/8 post-reboot launches accept normally, and the GSQ
launches log no host split at all yet accept fine. Verdict in the record: *"Q4_K_XL df2: no served
number for R66."* Receipts: seat-62 §4q, §4r, §4s; `R66-PROMOTION-RECORD.md` §4.1.

A related self-correction belongs here. HQ first wrote a RAM/page-corruption story for an unclean
shutdown on 2026-09-19 and for those zero-acceptance launches. The President struck it —
*"stop assuming — the system did not crash because of RAM"* — and the rig law already forbids citing
the RAM kit as a cause. What is CHECKED about the shutdown: Kernel-Power 41, BugcheckCode 0, no
minidump, **cause UNKNOWN**. The zero-acceptance launches are owned as a software defect in our own
stack. Receipt: seat-62 §4r.

### 6.5 Coverage we know is missing

- **f16 iq3 pipelines ship with 0 executed test cases** under the new n=7 default. One test row is owed.
  (`CHECK-R66-FABLE.md` §5.5.)
- **R49-class intermittents** (a rare wrong destination on ~1 GB Q6_K Vulkan mat-vec inputs, m=248320)
  did not fire in the R66 run — 0 FAIL in 2222 executed MUL_MAT cases. The class stays **OPEN**; no
  failure has yet been caught with probes attached.
- **MUL_MAT_ID 1004 executed / 0 FAIL** is carried in the promotion record as **ASSUMED** by its own
  author: read from the checker and HQ's message, not recounted from the file.
- **The "load-level lever"** — the idea that pinning every launch at the fast level buys 5–10% — is a
  **HYPOTHESIS**, explicitly demoted by the checker: *"no receipt ties the launch level to placement;
  do not bank it as a lever or a gain size."* (`R66-PROMOTION-RECORD.md` §4.)

### 6.6 The measurement laws that made the rest trustworthy

- **The box shares its DDR5 with everything else on it** (lane-166). n=25, one server, one unchanging
  request: decode is bimodal **22.68 → 15.25 t/s** while prefill is unchanged. **Every A/B must be
  interleaved**; the guard is code (`lane-evidence/2026-08-29-lane-166/interleaved_ab.py`) and returns
  TIED unless the CI excludes 1.0. SPINE §4.
- **HQ itself was the noise.** A "post-reboot 1.86 t/s regime" turned out to be HQ's own file writes and
  git commands inside the measurement window. Quiet, the same binaries read 2.145 / 2.162 — a tie. The
  law: during a perf leg, HQ does nothing on the box. Receipt: `lane-234/77-chain138-hq-activity-note.txt`.
- **F-165**: the A/B palindrome ran the AFTER arm's plain slot back to back with no idle, so every
  candidate carried a systematic penalty — read as a regression by two checkers before it was caught.
  Cool-downs are mandatory. `registers/failure-ledger.jsonl` F-165.
- **Thermal state moves prefill more than kernels do.** The same 4B S-X8 file read pp128 **493 and 400**
  in two cooled runs. Receipt: `lane-234/84-*`, `81-*`.
- **F-143**: an LDS pad-2 change measured +15% CI-clean on a q4_K micro-bench, then **−40%** at
  whole-model pp2048. A micro-bench win is not a win until the graph agrees.
- **F-127**: a perplexity scalar was carried estate-wide with its geometry stripped; a second receipt
  for the same model and quant read 0.5 PPL apart. Every PPL number now carries `n_ctx` and chunk count
  or it is not quotable.
- **Reuse a binary, never a number.** Lane-71's 27B pp512 of 105.2 did not reproduce on the *identical*
  binary weeks later (85.41, −19%). SPINE §6b.

---

## 6a. R66 validated end to end — and the honest reading of it

Between R66 and R73, the R66 engine was validated against R56 with the **fixed** harness: unit =
**launch**, **8 launches per arm** × 4 rounds, arm order counterbalanced [2,2,2,2], **0 invalid
launches**, INTEGRITY clean three times over, Welch confidence intervals
(`r66-lane-254/c29..c31-*-launchlevel/*/AB.json`; seat-62 §4aj).

| 27B file | df2 decode R56 → R66 (CI95 of the difference) | plain decode |
|---|---|---|
| Q4_K_XL | 7.34 → 7.46 (+0.12, [−0.23, +0.47]) | 3.73 → 3.86 (+0.14, [−0.35, +0.63]) |
| S-X8 | 5.90 → 5.94 (+0.04, [−0.11, +0.20]) | 2.19 → 2.18 (−0.004, [−0.05, +0.04]) |
| GSQ IQ3_S | 8.08 → 8.02 (−0.06, [−0.18, +0.06]) | 5.200 → 5.202 (+0.001, [−0.005, +0.008]) |

**Every confidence interval contains zero.** Read it exactly as it reads: at the served draft depth
of 4, **R66 is R56 end to end — no regression on any line in any mode, and quality identical.** That
is the expected result, not a disappointing one: at draft depth 4 the verify width is 5, and only
the q5_K route is reached there at all, with a ceiling of about 2%.

Two things this closes, and one it does not:

- The prefill "regression" that chain150 reported at cell level is **dead at launch level**.
- The owed c26 Q4_K_XL df2 re-run is **CLOSED** — 16 valid df2 launches with the acceptance gate
  clean.
- It does **not** turn the per-column kernel wins into served wins. They are reached at verify
  widths 5–8; our serve verifies at 5. The gains are real and they are per-column; the served line
  is flat, and both statements are in this document on purpose.

A harness note worth keeping: the first launch of each arm × mode reads low on Q4_K_XL (6.69 against
about 7.4) — a cold-first-launch effect present **in both arms**, so it cancels, but launch 0 should
be dropped or flagged.


## 7. The ideas we killed, with their mechanisms

Negative results with receipts are part of this release. `registers/closed-levers.jsonl` holds ten
rows; a closed row closes the idea **for our throughput program only** — it never means drop the code.

| Lever | Closed | What the receipt said |
|---|---|---|
| `depth-nmax` | 2026-09-17 | Draft depth 2→4 WIN +20%; 4v6 TIE at 16 rounds; 6v8 LOSS; adaptive LOSS; 4v9 LOSS 5.42 vs 7.14, CI [−1.96,−1.07]. Acceptance halves past position 4. |
| `host-split-memtype` | 2026-09-16 | Host-split memory type 1 vs 3: probe 39.9 vs 36.1 GB/s; served plain 1.998 → 1.321 LOSS. Memory type is not a lever. |
| `spill-reorder-by-name` | 2026-09-15 | Every host-visible byte on the 27B S-X8 plain line is a hot late-layer weight; read-cost order produced **byte-identical** placement. |
| `q8_0-mmq-under-coopmat-serve` | 2026-09-16 | As a serve default: llama-bench pp128 −37% (494 → 313); S-X8 +31.8% in the microbench. Default OFF. |
| `sx8-smalln-tile-decode` | 2026-09-16 | R60: the sx8/q8_0 ratio is FLAT in n (0.94–0.98 at 9216×2560). Dtype path refuted — the types are identical in both files. No switch shipped. |
| `q4k-instruction-cuts` | 2026-09-17 | R50 rows/hoist switches and R57 cuts 1–3 refuted from the thread→data mapping; the width-4 knee is k-dependent, not instruction count. |
| `sampler-arms-via-harness` | 2026-09-17 | The sampler arms measured **nothing**: the harness sends its own sampler in the request body, and one flag was dead. |
| `checkpoint-mismatch-drafter` | 2026-09-17 | Stock vs abliterated Q4_K_XL with the same drafter: identical acceptance 0.571. The drafter is not mismatched. |
| `spec-p-min-and-publisher-recipe` | 2026-09-17 | `--spec-draft-p-min 0.75` raises acceptance to 0.85 but loses t/s; the publisher's full recipe reaches 0.865 acceptance and 7.0 vs 7.5 t/s. |
| `mmvq-width-knee-capacity` | 2026-09-18 | R65: the width-4 knee (79 → 61 GB/s) is **not** a B-side capacity effect — width-locked across k=4096..12288 under a pre-registered falsifier; split-k not bit-identical. |

Older dead ends with their mechanisms (SPINE §5): prefill-mm interior (WMMA-MAC-bound) · decode kernel,
7 variants (DRAM-latency wall) · D3 rollback-store fusion · tree speculation · draft-head requant ·
prefetch-experts · **RDNA3 wave32 subgroup tuning** — the defect is real and upstream-wide, but it buys
0.998 / 0.999 medians across 280 properly-paired kernel cases, because mat-vec is bandwidth-bound ·
**MoE router fusion** (disabling all existing fusion costs 1.3%, which caps what more could return) ·
**coopmat2** — permanently impossible here; the 780M exposes only `VK_KHR_cooperative_matrix`.
The one survivor: `GGML_VK_MAX_NODES_PER_SUBMIT=4096`, +1.3% CI-clean, adopted.

---

## 8. The allocator, speculation, and the quality gates

*(Carried forward from the R56 draft; nothing in R66 changed these.)*

**The 27B drafted line used to die at load**, and it was never a capacity problem. Both drafted
launches refused a **156,893,184 B** checkpoint buffer while the box had **22.9 GB free** — in one pool
of RAM that is a heap-choice glitch, not exhaustion (`lane-209/r46i-lane-230/c1-sx8-27b/SX8/SEAT-df2-L0.server.log`).
The fix shipped in R46i: a per-heap ledger, a per-type planner, a loader-wide transaction and a bounded
host split with a staging reserve. **Crash → serving**, same file, same recipe. F-175 records what
getting it wrong cost: a server that answered HTTP 500 while holding about 16 GB, a process-tree kill
that did not reach it, and a hard reboot. Mechanized.

**Speculation.** DFlash2 is the drafter of record; the serve line is n-max 4 and stays there (`depth-nmax`
above). Depth 2→4 bought **4.617 → 5.551 t/s, +20%**, CI95 [+0.62, +1.76], 8/8. **LL-180**: run the MTP
draft only when base and draft quant are compatible — a Q4/Q8 base gives 35–50% acceptance, a **Q2 base
gives 0% and MTP backfires 2.6× slower**.

**The gates that run before anything ships**, R66 results:

- **40/40 gate cells rc=0** (`r66-evidence/*.rc`).
- **MUL_MAT 2222 executed / 0 FAIL / 876 not-supported**, with the flipped routes actually exercised
  (iq3_s + iq3_xxs at n=7 across 4 shapes; q5_K n=5..8 at k=256/4096/5120) —
  `17-correct-MUL_MAT.txt`, `19-executed-counts.txt`.
- **MUL_MAT_ID 1004 / 0 FAIL** (ASSUMED, see §6.5); CPY, GET_ROWS, ADD rc=0.
- **Seven legacy arms all rc=0** — route-legacy, hoist-off, q6k-ds-off, mmq-coopmat-on, q5k-legacy,
  iq3-legacy, iq3-n7rows4 (`18-*`..`24-*`). The old behaviour is not theoretical; it is tested.
- **Bit-identity 6/6 IDENTICAL** vs the R56 stage — with the honest caveat printed in the record:
  *this proves nothing about the flips*, because n=1 is outside every flipped route.
- **Perplexity batch 8: 5.9932 ± 0.14292** (legacy arm 5.9918).
- **Series check PASS** byte-identical, 586 patches replayed → tree IDENTICAL.
- **Smoke cell post-flip rc=0**, 16.7 s, all three startup lines present.

**Quality: did we increase it?** The mandate's stretch goal got one honest attempt and one honest
answer. Every llama.cpp integration of S-X8 drops the author's PCA output correction; we built it.
Measured on wikitext-2: **Q8_0 9.9742 · S-X8 correction off 9.9989 · on 9.9955** — a **−0.034%** move,
about 5% of one cell's ±0.072 stderr. **That is a sign, not a win, and it is reported as a sign.**
Receipt: `…lane-242-r53-sx8-pca-REPORT.md`. R66 adds nothing to this: PPL is identical across its arms.

---

## 9. Timeline, per served line

All figures are llama-server decode tokens/s on the 27B, palindrome A/B, cell-wise CI95, cool-downs
between slots, nothing else running on the box. **Read §6.2 before quoting any small delta here.**

### 27B S-X8, plain decode

| Date | Engine | t/s | Read |
|---|---|---|---|
| 2026-09-14 | R45 | **1.831** | historical baseline, `r46i-lane-230/c1-sx8-27b/SX8/AB.json` |
| 2026-09-14 | R46i | **2.247** | +22%, CI95 [+0.386, +0.422], 8/8 |
| 2026-09-15 (quiet re-run) | R46i / R48i | **2.145 / 2.162** | TIE — supersedes the "1.86 post-reboot" readings, which were HQ noise |
| 2026-09-16 | R48i / R56 | **2.099 / 2.170** | TIE. *(The R56 "plain prefill +1.7 t/s CI-clean" that accompanied this row is now **UNRESOLVED** — §6.2.)* |
| 2026-09-19 | R56 / R66 | **2.109 / 2.103** | TIE, 16 rounds, tie holds at launch level |

### 27B S-X8, drafted (DFlash2)

| Date | Configuration | t/s | Read |
|---|---|---|---|
| before R46 | R45, n-max 2 | **both launches DIED** | the B3 allocation crash |
| 2026-09-15 | R46i, n-max 2, same-night control | **3.334** | `r48i-lane-234/c1-sx8-27b/SX8/AB.json` |
| 2026-09-15 | R48i, n-max 2 | **4.475** | +34% vs the same-night control, CI95 [+0.43, +1.87], 7/8 |
| 2026-09-15 | R48i, n-max 4 | **5.551** | +20% over n-max 2, CI95 [+0.62, +1.76], 8/8 |
| 2026-09-16 | R48i / R56, n-max 4, quiet 16 rounds | **5.916 / 5.848** | TIE |
| 2026-09-19 | R56 / R66, n-max 4, 16 rounds | **5.433 / 5.530** | TIE, CI95 [−0.15, +0.35] |

### 27B Q4_K_XL (Huihui UD) — the interactive-brain line

| Date | Serve line | Before | After | Read |
|---|---|---|---|---|
| 2026-09-15 | plain | R45 **3.864** | R48i **4.104** | +6.2%, CI95 [+0.12, +0.58], 8/8 |
| 2026-09-15 | drafted, n-max 2 | R45 **7.085** | R48i **7.578** | +7%, CI95 [+0.28, +0.70], 8/8 |
| 2026-09-16 | drafted, registered line (n-max 3 + `--ctx-checkpoints 32`) | R48i **7.656** | R56 **7.832** | +2.3%, CI95 [+0.11, +0.38], 8/8 — **now UNRESOLVED, §6.2** |
| 2026-09-19 | plain | R56 **4.095** | R66 **4.108** | TIE |
| 2026-09-19 | drafted | — | — | **no served number for R66** — invalid session, §6.4 |

### 27B GSQ IQ3_S — the newest line

| Date | Serve line | R56 | R66 | Read |
|---|---|---|---|---|
| 2026-09-19 | plain | **5.157** | **5.166** | TIE, 13/16. Plain 5.17 t/s is the **fastest 27B plain measured on this box** (seat-61 §9) |
| 2026-09-19 | drafted | **7.771** | **7.681** | TIE, CI95 [−0.24, +0.06] |

### The 3.0× headline, with its caveat attached

The best working served configuration on the 27B S-X8 went **1.831 t/s (R45, plain — drafted crashed)
→ 5.551 t/s (R48i, drafted n-max 4)** between 2026-09-14 and 2026-09-15.

**This is a configuration progression, not a like-for-like kernel speedup.** It changes engine,
allocator, speculation (crashed → DFlash2) and draft depth. The 1.831 endpoint is historical; today's
quiet plain line is 2.10–2.17. The like-for-like evidence is the component A/B rows above, each with
its own CI. That caveat is in the GAINS ledger row itself, not a footnote.

---

## 10. What is next (roadmap, not a claim)

Nothing here is measured yet.

1. **The harness fix** — CI over launch means, counterbalanced launch order, invalid-launch gate on
   acceptance < 0.2, re-analysis of the banked cells. Everything else waits behind it (§6.2).
2. **R68** — read the driver's own pipeline statistics on the q4_K/q5_K/q6_K mat-vec pipelines at
   n=1..8. The R61 finding says the method works: the same field already showed a VGPR jump at n=4
   (134 → 216), the exact signature R65 asked for at the width-4 knee. RGA 2.14.2 is installed for the
   ISA and live-register read.
3. **R70 — a load-time kernel autotuner**, from the President's 2026-09-18 question *"dynamic or tuned
   to our iGPU? make it work for any"*. Two classes of win: **less-work fixes** (the sign hoist, the
   R48 hoists) are faster on every GPU with nothing to tune; **shape choices** (rows per workgroup,
   route widths, the n=7 reshape) were measured on gfx1103 and differ per GPU and driver. R70 times the
   candidates on the real device at first model load, caches the pick, re-probes when the driver version
   changes, and rejects any variant the driver reports as spilling. Every switch stays a manual
   override; `--no-autotune` for reproducible benches. This is the fork's portability answer.
   Receipt: seat-62 §4i.
4. **R59 — ownership-aware placement.** Ceiling honestly priced: 2.97 GB at 40 GB/s is about 7% of the
   plain line at most.
5. **R55 — the base move** b10825 → b10903 (11 verbatim, 5 behind-switch, 8 conflicting).
6. **The MoE / FreeToken program**, P1–P14. President's ruling, verbatim: *"we would open that same path
   to vulkan ppl not just cuda/metal ppl."* Dense rows first.
7. **The stated target** on the 27B dense line is **15 to 20 t/s**. The arithmetic says it needs *both*
   a cheaper verify column *and* accepted length past position 4 — the marginal verify column is 43 ms
   = 0.177 plain-token-equivalent on Q4_K_XL, and a perfect drafter at n-max 4 tops out at 10.9 t/s
   (seat-61 §9). It is a target, not a projection.

---

## OWED — every number and decision this draft could not close

| Item | Why |
|---|---|
| Q4_K_XL **drafted** served number for R66 | The session is invalid (two zero-acceptance launches, cause unknown) — §6.4 |
| The banked R56 "+2.3% CI-clean" and "prefill 35.6 → 39.4" rows | UNRESOLVED under the launch-level defect; two CHECKED records disagree on the bound (≤10% vs ≤~25%) — §6.2 |
| 27B S-X8 llama-bench prefill residual −1.7%, CI95 [−4.4, +1.0] | Not shown nonzero, not shown zero — §6.3 |
| f16 iq3 pipelines | 0 executed test cases under the new default; one test row owed |
| R49 quiet repro with `GGML_R49_DUMP=1` | The class is open; no failure yet caught with probes attached |
| MUL_MAT_ID 1004/0 for R66 | ASSUMED in the promotion record, not recounted from the file |
| The "load-level lever" | HYPOTHESIS — no receipt ties launch level to placement |
| `other_remotes` disposition; `ciru` patch count | OWED from the President before publication |
| `closed-levers.jsonl` row `iq3-sign-hoist` | Owed (seat-62 §4w) |
| `KERNEL-FORMAT-MATRIX.md` §5 "Not-our-rig gains" | The section the President's §4l law requires does not exist yet |
| 27B S-X8 served plain **t/s** before R46 (2026-09-08) | The lane-224 receipt banks GB/s achieved (48), not a served t/s cell |
| TurboQuant TQ types — exact served t/s on `b10447-turbo` | No banked cell found |
| Per-region bandwidth probe; device-local reference probe | Never run; the R58 device-local probe crashed (OutOfDeviceMemory on a forced 1 GB alloc) |
| A dual-channel vs single-channel A/B | No matched kit exists on this box; the single-channel tail is ASSUMED |
| HumanEval and GSM8K cells for Escha-W2; ARC rescores | SPINE §2b.1 marks them OWED |
| Machine-readable evidence manifest for the GAINS ledger | The page cannot yet prove its own freshness |

---

*Draft ends. Publication is the President's call. Blocking items before any push:*
`RELEASE-CHECKLIST.md` *beside this file.*
