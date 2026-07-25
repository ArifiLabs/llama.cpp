# ArifiLabs llama.cpp

An attribution-first, rebaseable llama.cpp fork for local inference across current and future ArifiLabs hardware.

This repository starts from upstream llama.cpp master
(`4df29be4f`, 2026-08-16) and carries an ordered,
feature-toggled patch series. Its scope is deliberately broad: one engine for
the current Beelink SER7, future 64/96 GB RAM configurations, an AMD Strix Halo
128 GB system, and an NVIDIA DGX Spark 128 GB system.

The fork preserves upstream backends. Vulkan is the blessed AMD path for the
current gfx1103 estate; upstream CUDA remains available for NVIDIA hardware.
ROCm/HIP and wholesale ROCmFPX adoption are not the running path on the current
rig, but Charlie’s fork remains a tracked source for discrete mechanisms and
currency review.

## Principles

- Start from a known upstream anchor and make every local change rebaseable.
- Compile capabilities in and expose them as documented runtime options.
- Attribute every carried change in repository documentation and commit trailers.
- Treat benchmark evidence as placement-specific: a gain on host-offloaded
  experts is not a gain on GPU-resident unified-memory execution.
- Accept an upstream or fork update only after a clean rebase, Windows Vulkan
  build, functional checks, and the llama-server benchmark regression judge.
- Preserve negative evidence. A failed kernel or an inert toggle is useful
  engineering knowledge, not material to erase.

## What this fork adds to upstream

Everything ArifiLabs adds lives as a **linear, 40-patch series** on top of upstream `b10068`.
There are no merge commits: the series is designed to be replayed onto a newer upstream tag.

```bash
git clone <this repo> && cd llama.cpp
git checkout -b my-rebuild 571d0d540
git am patches/series/*.patch     # reproduces master, file-for-file
```

That is checked, not claimed: `series check` regenerates the series, byte-compares it against what
is committed, replays it with `git am`, and diffs the result against `master`. The one thing the
replay does not reproduce is `patches/series/` itself — the series is generated into the tree it
describes, and a patch cannot contain itself, so the generated directory is excluded from its own
generation. Everything outside it is identical.

[`patches/series/MANIFEST.md`](patches/series/MANIFEST.md) lists every patch with its source,
its gate, and its `Measured-effect:` trailer. Per-mechanism detail is in
[`docs/CATALOG.md`](docs/CATALOG.md); every runtime toggle is in
[`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md), which is authoritative where this
summary and it disagree.

| Feature | Source | Toggle | Default | Honest status |
|---|---|---|---|---|
| Sparse / expert SSD streaming | PowerInfer | `EXPERT_BUNDLE_PATH` | unset (off) | **Measured capability**, not a speed claim: 21B streamed coherently at `5.9–6.1` CPU decode t/s; 35B from a 17 GB bundle at `1.6` t/s; `-ngl 99 -cmoe` tiered at `3.0` t/s |
| Hot-expert RAM cache | PowerInfer | `MAX_N_CACHED` | implementation-selected | **UNMEASURED** — no accepted ON/OFF benchmark |
| Standalone expert prefetch | ArifiLabs / PowerInfer | `LANE110_PREFETCH_CAP` | `3*n_expert_used` | **Inconclusive** — cap sweeps differ but need controlled retest; default retained |
| Per-op profiling | ArifiLabs | `LANE110_PROF=1` | unset | Diagnosis only; **overhead UNMEASURED**. Pipeline-init reuse itself cut rebuild `2472 µs → 0–2 µs` |
| Host-transfer prefetch (3 patches) | thecodacus | `GGML_SCHED_PREFETCH_EXPERTS`, `GGML_SCHED_MAX_PREFETCH_SLOTS` | **OFF** on unified memory | `+64.5%` prefill in the host-offloaded `-cmoe` regime it was written for; **measured inert** on our GPU-resident unified-memory Vulkan rig |
| Ternary `Q2_0_g128` CPU + Vulkan | PrismML | GGUF metadata key `GGML_Q2_0_G128=1` | gate absent → off | Ported and loading; fail-closed loader gate + verified retag tool. **Not independently benchmarked in this fork** |
| f16 recurrent S-state | PrismML | `GGML_RECURRENT_STATE_F16=1` | unset (`f32`) | Halves S-state residency and traffic. **Opt-in**: recurrent rounding compounds; quality gate owed per target |
| M-RoPE embedded-batch guard | ROCmFPX | none (always on) | on | Narrow correctness fix. **Build-validated only** — a mixed token+embedding M-RoPE batch is still owed |
| Windows IOCP async expert reads | ArifiLabs | `POWERINFER_IOCP` | off | **Direction UNRESOLVED** — the A/B was noise. The 21B's experts largely fit cache, so there was little read traffic to hide; needs a bigger-than-RAM model |
| DSpark iGPU detection fix | PrismML | n/a | on | Ships as an **iGPU-detection correctness fix**, explicitly *not* a DSpark recommendation here |
| GGUF g128 retag tooling | ArifiLabs | `tools/gguf` | n/a | Fail-closed, structure-only parser; verified type-id rewrite 42→43 |

## Honest negative results

These are published because the fork's credibility depends on them more than on its wins.

- **The three thecodacus patches do nothing on our hardware.** They pin and prefetch host-side
  weights to speed a host→device transfer. On a unified-memory 780M there is no bus to beat, so
  they measured inert. They are carried, compiled in and OFF by default, because they are a real
  win on the discrete-GPU placement they were written for. Their author's `+64.5%` is his result
  in his regime, and we reproduce it as a citation, not as ours.
- **DSpark is a net loss on this rig** — it engaged on 780M Vulkan and fell from `7.89` to
  `4.27` t/s (`0.54×`). Documented OFF.
- **TurboQuant KV loses to plain `q4_0`.** Measured at n_ctx 16384: f16 1792 MiB @ ~20 t/s PASS;
  `q4_0` 504 MiB @ ~19.8 t/s PASS; turbo3 350 MiB @ 8.5 t/s **FAIL**; turbo2@wht0 238 MiB @
  11.2 t/s **FAIL**. The storage claim is real and exact, but every coherent configuration still
  needs a non-turbo K — so plain `q4_0` is both smaller and faster. Not shipped.
- **We caused a NaN and then fixed it.** The ported x86 VNNI Q2_0 GEMM hardcoded an activation
  stride of `4`, which is `QK2_0/QK8_0` in PrismML's 128-wide block tree but `2` in our 64-wide
  one. The pointer ran past the activation buffer and decoded garbage as FP16 NaN. Two files,
  four lines. A second defect in the same port sat *outside* the runtime gate because `vec_dot`
  is selected at compile time — so the "OFF" arm was not the baseline, and three A/B comparisons
  before that discovery were worthless.
- **MTP is not universal.** 31B root-MTP went `1.8 → 9.4` t/s; Gemma-4-26B-A4B `29.3 → 37.3` at
  `N=3`; but Qwen35MoE *lost* to plain Q4_0 (`22.8` tuned vs `29.2`).
- **`POWERINFER_IOCP` has no verdict.** An earlier registry row claimed one; it was corrected.

## Reproducing our numbers

The regression and benchmark judge is **`llama-server`**, not `llama-cli` and not `llama-bench`.
Reuse one binary across the arms of a comparison; never reuse a number from a previous build.

**If you are A/B-testing anything that touches a CPU repack path, you must pass `--no-host`.**
`make_cpu_buft_list()` in `src/llama-model.cpp` appends the device host buffer type *before* the
CPU extra buffer types, so on a Vulkan build `Vulkan_Host`/`CPU_Mapped` always wins and the
repack path never engages. Two runs here produced **false passes** exactly this way — both arms
agreed because the code under test ran in neither.

Verify engagement independently before believing any comparison. For the repack path the only
trustworthy signal is the load line:

```
load_tensors: CPU_REPACK model buffer size = <N> MiB
```

present in the ON arm and **absent** in the OFF arm. An A/B whose arms agree proves nothing until
path engagement has been evidenced separately.

## Maintaining the fork

[`tools/arifi-sync/`](tools/arifi-sync/README.md) drives currency, the patch series, and upstream
bumps. Every subcommand fails loudly and stops rather than guessing.

```bash
python tools/arifi-sync/arifi_sync.py status        # where the fork stands
python tools/arifi-sync/arifi_sync.py series check  # series still byte-matches git, and replays
python tools/arifi-sync/arifi_sync.py provenance    # every commit still carries its source
python tools/arifi-sync/arifi_sync.py currency      # fetch all sources, report drift vs pins
python tools/arifi-sync/arifi_sync.py bump --onto upstream/master
```

`bump` computes the rebase surface, replays the series patch by patch, rebuilds, and runs the
`llama-server` judge — and refuses the bump unless all three pass. `currency` cannot be satisfied
by re-running it: clearing a drift row requires reviewing the new commits and recording the sha
you reviewed in `sources.json`.

## Options and hardware defaults

[`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md) is the authoritative
runtime-option registry. It records defaults, placement constraints, measured
effects, known negative results, and benchmark rules.

Important defaults already ruled by evidence:

- `--ctx-checkpoints 0` is the default; disabling rewind is the program’s
  largest historical gain.
- KV f16 is primary under the President’s 64 GB RAM ruling; q4_0 remains an
  explicit capacity trade-off rather than a silent default.
- DSpark is documented OFF on the current 780M Vulkan rig after a measured
  `0.54×` net result.
- thecodacus host-transfer prefetch patches remain compiled and carried, but
  are OFF by default on unified-memory machines. Their documented gain belongs
  to host-offloaded/discrete-transfer placement, while the GPU-resident Vulkan
  configuration measured them inert.

## Credits and maximal attribution

This fork exists because of upstream and community work. Attribution is not a
footer: it is part of the patch series, documentation, source ledger, and
commit history.

- [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp) is the upstream
  engine and the base for this repository. Upstream’s MIT notice is retained.
- [Tiiny-AI/PowerInfer](https://github.com/Tiiny-AI/PowerInfer) supplies the
  sparse-execution and expert-streaming lineage used by the planned
  PowerInfer graft. Its root and `smallthinker` MIT notices are retained.
- [PrismML-Eng/llama.cpp](https://github.com/PrismML-Eng/llama.cpp) supplied
  the observed Vulkan path for PrismML `Q2_0_g128` ternary GGUFs and the
  ternary-port lineage. The President’s rule #0 authorizes carrying this work
  with maximal credit and provenance; the Phase-0 factual observation that the
  retained PrismML releases did not include a source `LICENSE` or `NOTICE`
  remains documented rather than rewritten.
- [charlie12345/ROCmFPX](https://github.com/charlie12345/ROCmFPX), Charlie’s
  fork, informed checkpoint and format investigations. It is tracked for
  currency and diff audit, not blindly merged; its MIT notice is retained.
- [thecodacus/llama.cpp](https://github.com/thecodacus/llama.cpp), branch
  `fable5/prefetch-experts`, authored the three community prefetch patches
  carried in their original dependency order:
  [`20f5994`](https://github.com/thecodacus/llama.cpp/commit/20f5994bfeb91d24da328077c4b6095998cc9888.patch),
  [`1163cb3`](https://github.com/thecodacus/llama.cpp/commit/1163cb34939fe4a9cb07aec034c5954144497ae9.patch),
  and
  [`5f83fbb`](https://github.com/thecodacus/llama.cpp/commit/5f83fbbe7c668c59912a1fe09e86a0ef580406c4.patch).
  Their discovery record is
  [thecodacus’s Fable5/prefetch-experts video](https://www.youtube.com/watch?v=VytSYCDhWQ0).
  No separate channel URL was captured in the estate record, so none is
  invented here.
- `llama-cpp-turboquant` contributes MIT-licensed tooling lineage.
- `turboquant_plus` contributes Apache-2.0 tooling lineage; its Apache license
  and NOTICE are retained.

Every later carried commit must identify its source with `Taken-from:`, its
local responsibility with `Origin:`, and its measured result or intentionally
unmeasured status with `Measured-effect:`.

## License notices

`LICENSE` is the verbatim upstream llama.cpp MIT license. Retained third-party
notices and the exact source from which each was copied are enumerated in
[`LICENSES/README.md`](LICENSES/README.md).

The President’s 2026-07-22 rule #0 lifted the previous publication blocks for
PrismML, thecodacus, PowerInfer, and Charlie/ROCmFPX. This authorization does
not justify inventing license text: no PrismML or thecodacus license file is
manufactured where Phase 0 did not observe one. Their credit and provenance
remain explicit in this README, the series manifest, and each eventual commit.

## Status

The patch series is real and complete: 40 linear commits on upstream `b10068`, every one carrying
provenance trailers, generated into [`patches/series/`](patches/series/) and verified on demand to
replay to a tree identical to `master` outside the generated series directory itself.

What that does **not** mean:

- It is not a claim that the sparse Vulkan kernel, the ternary port, the IOCP overlap, or any
  individual patch has been independently benchmarked. Where a number is absent the registry says
  `UNMEASURED`, and that is the honest value, not a placeholder.
- CI is still a placeholder. A bump is currently verified by a human running
  `tools/arifi-sync`, not by a build matrix.
- The `build` and `judge` legs of `arifi-sync` are written but **unexercised**; see the status
  table in [`tools/arifi-sync/README.md`](tools/arifi-sync/README.md). The rest of the tool has
  been run against this repository.

Publication remains gated by local proof, committed evidence for every
published gain claim and negative result, and the President’s explicit release
word.
