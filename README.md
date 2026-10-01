# ArifiLabs llama.cpp

An attribution-first, rebaseable llama.cpp fork for local inference across current and future ArifiLabs hardware.

This repository starts from upstream llama.cpp master
(`9e0e220594af405a62835dc3a27495729fd8506b`, tag `b10825`) and carries an ordered,
feature-toggled patch series. Its scope is deliberately broad: one engine for
the Beelink SER7 (Radeon 780M, where every performance number here was measured),
the Minisforum AI X1 Pro (Radeon 890M, where this tip's placement fix was checked
for correctness), an AMD Strix Halo 128 GB system, and an NVIDIA DGX Spark 128 GB system.

The fork preserves upstream backends. Vulkan is the blessed AMD path for the
780M (gfx1103) and 890M estate; upstream CUDA remains available for NVIDIA hardware.
ROCm/HIP and wholesale ROCmFPX adoption are not the running path on the current
rig, but Charlie’s fork remains a tracked source for discrete mechanisms and
currency review.

## Scope — what this is, and what it is not

**It is** upstream llama.cpp `b10825` plus a linear, individually-toggleable, fully-attributed
patch series, carrying real mechanisms from PowerInfer, PrismML, ROCmFPX, TurboQuant, tq3 and
thecodacus that are not in upstream, each one gated and each one documented with its measured effect
or an explicit `UNMEASURED`. The series replays byte-identically onto its base, and that is verified by a command
you can run yourself.

**It is not** a faster llama.cpp. Most of what is carried here is either a *capability* (run a model
that otherwise would not fit) or a win confined to a specific hardware placement. Several carried
patches measure **inert** on our own hardware and are documented as such.

**Every number in this repository was measured on one machine**: a Beelink SER7 (Ryzen 7 7840HS,
Radeon 780M / gfx1103, unified memory) on Windows 11, built with WinLibs MinGW-w64 GCC 14.2.0,
running the Vulkan backend — or, where a row says CPU-only, the CPU backend on that same box. The
one exception is the Radeon 890M section, which reports correctness checks (no speed) from a second box.

**Never built or run by anyone, anywhere, on:** Linux, macOS, MSVC, clang-cl, CUDA, ROCm/HIP,
Arm/NEON, or any GPU other than gfx1103. Upstream supports all of them and nothing here removes
that support; a CPU-only build is verified. But no result on this page transfers to that hardware,
and we will not imply it does. If you are on any of it, start from
[`docs/HARDWARE-PROFILES.md`](docs/HARDWARE-PROFILES.md) — several of our defaults are wrong for you.

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

## Building

Full instructions, including the failures each flag prevents, are in
[`docs/BUILDING.md`](docs/BUILDING.md). The short version for MinGW on Windows:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00" \
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
  -DLLAMA_USE_PREBUILT_UI=OFF          # add -DGGML_VULKAN=ON for the Vulkan build
cmake --build build --target llama-server -j 6
```

Those flags are not tuning. Without `-D_WIN32_WINNT=0x0A00` the build fails at
`vendor/cpp-httplib/httplib.cpp:1471` with `'::CreateFile2' has not been declared` — an error that
names neither the flag nor the fix, and which this fork lost three builds to. Without the static
GCC runtime the binaries can die at startup with `0xC0000139` when a foreign `libstdc++-6.dll` is
on `PATH`. A CPU-only build needs no Vulkan SDK and is verified working.

## What this fork adds to upstream

Everything ArifiLabs adds lives as a **linear patch series** on top of upstream **`b10825`**
(`9e0e220594af405a62835dc3a27495729fd8506b`). The canonical branch carries **zero merge commits** — the
series exists to be replayed onto a newer upstream tag, and a merge commit is a hole in it
(`format-patch` omits merges, so their hand-made conflict resolutions never reach the series; that
was a real, measured failure here before the history was flattened).

```bash
git clone -c core.longpaths=true <this repo> arifilabs-llama.cpp && cd arifilabs-llama.cpp
python tools/arifi-sync/arifi_sync.py series check     # regenerate + verify the series
python tools/arifi-sync/arifi_sync.py series replay --onto 9e0e220594af405a62835dc3a27495729fd8506b
```

The procedure for the next upstream bump, the next fork ingest, and what must be re-verified
afterwards is written out command-by-command in `UPDATE-RUNBOOK.md`. **That document is internal
and is not published**: it is a studio operating procedure, not part of the released software, and
it is absent from this repository and from its history. The commands it drives are the public ones
— `tools/arifi-sync/arifi_sync.py series check|regen|replay`, `provenance`, `currency` — and
`python tools/arifi-sync/arifi_sync.py --help` lists them.

### The seven ingested sources

Seven third-party trees besides upstream are tracked for currency, each pinned in
[`tools/arifi-sync/sources.json`](tools/arifi-sync/sources.json). "Tracked" is not "merged", and the
distinction is the point: a source can be registered, pinned and reviewed and still contribute zero
lines, and several do.

| source | what it is | what actually landed here |
|---|---|---|
| [PrismML-Eng/llama.cpp](https://github.com/PrismML-Eng/llama.cpp) | ternary `Q2_0`/`Q2_0_G128` lineage | the g128 weight format (our id 43), its CPU dispatch, and the 4x8 VNNI repack GEMV/GEMM design |
| [Tiiny-AI/PowerInfer](https://github.com/Tiiny-AI/PowerInfer) | sparse execution + expert streaming | the MoE streaming/expert-bundle graft and its Windows IOCP read path |
| [charlie12345/ROCmFPX](https://github.com/charlie12345/ROCmFPX) | AMD FP weight formats | the six ROCmFPX serialized formats (ids 100-104, 107) and their CPU path, behind `GGML_ARIFI_ROCMFPX_FORMATS` |
| [ciru-ai/ROCmFPX](https://github.com/ciru-ai/ROCmFPX) | a second ROCmFPX lineage | **nothing yet** — registered and pinned for currency/diff audit only |
| [turbo-tan/llama.cpp-tq3](https://github.com/turbo-tan/llama.cpp-tq3) | the `TQ3_4S` rotated-domain family | the codec at our **retagged** ids 48-51, behind `GGML_ARIFI_TURBO_WEIGHT_QUANTS`, plus `tools/gguf-retag-tq3` |
| `llama-cpp-turboquant` | TurboQuant KV + `TQ3_1S`/`TQ4_1S` weights | the Turbo3 KV cache types and the `TQ3_1S`/`TQ4_1S` weight formats (ids 45/46) and their Vulkan kernels |
| `turboquant_plus` | Apache-2.0 KV-fidelity scoring package | **nothing** — it is not a llama.cpp fork at all; see the note below |

Two of the seven are honest zeroes and stay in the table for exactly that reason. `turboquant_plus`
was once believed to be free capability because 57 of 57 of its patches applied cleanly; opening its
root tree showed no `ggml/`, no `src/llama.cpp` and a `pyproject.toml` naming a Python package. A
high clean-apply rate measures **collision, not value**. The three community expert-prefetch patches
from [thecodacus/llama.cpp](https://github.com/thecodacus/llama.cpp) are carried too, but as
individual patches rather than a tracked remote.

Use the driver, not a hand-written `git am`. The naive form —
`git checkout -b x 571d0d540 && git am patches/series/*.patch` — **cannot work**, and the reason is
worth knowing: `patches/series/` does not exist at the upstream base commit, because the series
itself is what adds it. Checking out the base removes the very files you were about to apply.
`series replay` avoids this by replaying into its own worktree, and it names the patch, file and
rejected hunk on any failure.

That is checked, not claimed: `series check` regenerates the series, byte-compares it against what
is committed, replays it with `git am`, and diffs the result against `master`. The one thing the
replay does not reproduce is `patches/series/` itself — the series is generated into the tree it
describes, and a patch cannot contain itself, so the generated directory is excluded from its own
generation. Everything outside it is identical.

`core.longpaths=true` is not optional on Windows: upstream carries a 161-character path under
`tools/ui/`, and the clone fails to check out without it. See [`docs/BUILDING.md`](docs/BUILDING.md).

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

## Reading our commit messages

Commits, patches and evidence files carry identifiers like `lane-110C`. These are our internal
work-unit ids, kept deliberately rather than scrubbed: each one ties a change to the exact block
of work that produced it, the machine it was measured on, and the evidence file that holds the
raw numbers. If a claim in this repository looks unsupported, the id is how you find what backs
it. They are traceability, not organisational trivia — treat them the way you would a ticket
reference in any other project.

Two other conventions worth knowing:

- **`Taken-from:`** in a commit body names the upstream repository and commit a change was ported
  from. Every ported line has one.
- **`Measured-effect:`** names what we actually measured, on which hardware, or says `UNMEASURED`.
  That word is a legal value and appears where it is honest; an absent trailer is a gap, and the
  provenance audit fails on it.

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
- KV f16 is primary on machines with the memory for it; q4_0 remains an
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
  ternary-port lineage. Its MIT notice is retained as `licenses/prisml-MIT.txt`.
  The Phase-0 observation that the retained PrismML *releases* carried no
  `LICENSE` was true of that material and false of the project; the correction,
  and why the absence check looked in the wrong place, is in
  [`licenses/README.md`](licenses/README.md).
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
- [turbo-tan/llama.cpp-tq3](https://github.com/turbo-tan/llama.cpp-tq3) authored the `TQ3_4S`
  rotated-domain weight family — `TQ3_0_CENTROIDS`, `TQ3_0_SIGNS`, the randomized Hadamard
  transform, and the `tq3_4s` scale codecs — ported here from `58ad80ffb` and carried at
  **renumbered** ids 48-51 because tq3's own `46` collides with our `TQ4_1S`. The renumbering and
  its whole justification are in [`docs/TYPE-ID-ALLOCATION.md`](docs/TYPE-ID-ALLOCATION.md) §3.1.
  Its MIT notice is retained as `licenses/tq3-MIT.txt`.
- [ciru-ai/ROCmFPX](https://github.com/ciru-ai/ROCmFPX) is tracked for currency and diff audit. It has
  contributed no code; its MIT notice is retained anyway, so a notice can never lag an ingest.
- `llama-cpp-turboquant` contributes MIT-licensed tooling lineage, and — via the core-types patch,
  not via tq3 — the `TQ3_1S`/`TQ4_1S` weight formats and the Turbo3 KV cache types.
- `turboquant_plus` contributes Apache-2.0 tooling lineage; its Apache license
  and NOTICE are retained.

Every later carried commit must identify its source with `Taken-from:`, its
local responsibility with `Origin:`, and its measured result or intentionally
unmeasured status with `Measured-effect:`.

## License notices

**`NOTICE` at the repository root** names every third-party component carried here with its licence, its source, and the file in `licenses/` holding its licence text verbatim. Apache-2.0 components (S-X8 v4.3, turboquant_plus) are listed there with their required attribution notices.

`LICENSE` is the verbatim upstream llama.cpp MIT license. Retained third-party
notices and the exact source from which each was copied are enumerated in
[`licenses/README.md`](licenses/README.md).

A 2026-07-22 policy decision lifted the previous publication blocks for
PrismML, thecodacus, PowerInfer, and Charlie/ROCmFPX. This authorization does
not justify inventing license text: no PrismML or thecodacus license file is
manufactured where Phase 0 did not observe one. Their credit and provenance
remain explicit in this README, the series manifest, and each eventual commit.

## The R86i release — what shipped, how to build it, how to reproduce a number

> **Where the receipts live.** Receipt paths of the form `r66-evidence/…`, `r65-evidence/…` and other `r*-evidence/…` names in this document are **lane-local**: they name files in the lane worktree that produced them, not paths in this repository. A clone does not contain them. `evidence/README.md` lists what is tracked here and says plainly what is not.

*Applied from the release-prep branch, 2026-09-21. Switch meanings below are quoted
verbatim from `docs/OPTIONS-REGISTRY.md`.*

### What this is

An attribution-first, rebaseable llama.cpp fork for local inference on **Vulkan**.

- Upstream base: `ggml-org/llama.cpp` **b10825**, `9e0e220594af405a62835dc3a27495729fd8506b`.
  **That commit is upstream's own, byte for byte, signature and all.** This repository is a real
  fork: every commit at or below the base is shared with `ggml-org/llama.cpp`, so
  `git remote add upstream https://github.com/ggml-org/llama.cpp && git fetch upstream` gives you
  a common ancestor and a rebase that works. Check it yourself:
  `git merge-base --is-ancestor 9e0e220594af405a62835dc3a27495729fd8506b HEAD`.
- On top of it: the fork's own commits, linear, non-merge, individually toggleable, each carrying
  its provenance in the commit message. Count them with
  `git rev-list --count 9e0e220594af405a62835dc3a27495729fd8506b..HEAD` rather than trusting a
  number written here, which goes stale the moment a commit lands.
- Engine of record: **`arifi-b10825-r86i-6fb7a425a`**. Read that as a **label, not a git ref**: it is
  the internal build-stage identifier for the engine this release carries, and the `6fb7a425a` inside
  it names a commit in the studio's own repository that **does not exist here**. Nothing in this
  repository needs it; it is printed so a bug report can say which engine stage it came from. The
  previous engine, `arifi-b10825-r73i-5a7434218`, is the same kind of label.
- Release tag: `r86i-public-2026-09-29` marks the R86i tree **before** the Radeon 890M placement
  fix; this tip carries that fix and its documents on top (see *Added after the R86i tag* below). The
  tag this tip is published under is named at publication, and it will be the only tag pushed with it.
  Every other tag you may see mentioned in the history (`r73i-public-2026-09-22b`,
  `r73i-release-engine-2026-09-21`, `r86i-integration-2026-09-24`, and the `b*` upstream tags) is
  **internal, superseded or upstream** and is not part of this publication. Resolve the tree with
  `git rev-parse HEAD`, never with those names.
- The series is generated from git, never hand-edited, and verified on every run: every patch is
  byte-identical to a fresh generation *and* to a committed blob, and replaying all of them with
  `git am` reproduces the tree exactly. Run it yourself:
  `python tools/arifi-sync/arifi_sync.py series check`.

**It is for anyone on Vulkan.** A proven-safe improvement ships enabled even where our own GPU shows
no gain — other drivers may not already do what AMD's does. The iq3 sign hoist (R66, still ON) is
exactly that case.

**Every performance number in this repository was measured on one machine**: a Beelink SER7 Pro — Ryzen 7 7840HS,
**Radeon 780M (RDNA3, gfx1103)**, driver 32.0.31041.1004, Windows 11 build 29648, Balanced power plan,
one pool of system RAM (48 GB physical, 16 GB reserved in BIOS), Vulkan with `KHR_coopmat`. A second
machine, a Minisforum AI X1 Pro with a **Radeon 890M**, has run this tip's **correctness** checks only
(placement, `test-backend-ops`, greedy identity — see *Added after the R86i tag*); no 890M speed
number is published. Kernels transfer; numbers do not. If you are on other hardware, read
[`docs/HARDWARE-PROFILES.md`](docs/HARDWARE-PROFILES.md) first — several defaults are wrong for you.

---

### Build

Windows / MinGW-w64 / Vulkan. **This is the only configuration any number in this repository was
measured on** (source: the internal `UPDATE-RUNBOOK.md`, not published). The step-by-step
version, with the static-runtime and web-UI flags and the runtime-DLL step, is
[`docs/BUILDING.md`](docs/BUILDING.md) §3–§4.5; follow that page on a new machine:

```powershell
cmake -S . -B build-vulkan -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DGGML_VULKAN=ON `
  -DGGML_ARIFI_ROCMFPX_FORMATS=ON `
  -DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON `
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" `
  -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00"

cmake --build build-vulkan -j2 --target llama-cli llama-server llama-quantize `
                                        test-backend-ops test-quantize-fns
```

All three extra flags are load-bearing:

- `GGML_ARIFI_ROCMFPX_FORMATS` — without it the six ROCmFPX types do not compile and a ROCmFP4 GGUF is
  refused at load.
- `GGML_ARIFI_TURBO_WEIGHT_QUANTS` — without it `TQ3_1S`/`TQ4_1S` and the tq3 family (type ids 48–51)
  do not compile. It is a **CMake option** (`ggml/CMakeLists.txt`), not a runtime switch, which is
  why `docs/OPTIONS-REGISTRY.md` — a registry of environment switches — does not carry a row for it.
- `-D_WIN32_WINNT=0x0A00` in **both** `C` and `CXX` flags — without it the Windows IOCP path fails to
  compile. One of the two is not enough; the failure lands in whichever language you left out.

**Rig-specific, not requirements:**

- `-j2` is a property of *our* box, not of the fork: Vulkan shader generation is memory-hungry and a
  wider build dies on a machine with one shared pool of RAM. Use what your machine can feed.
- `-D_WIN32_WINNT=0x0A00` is Windows-only.
- The two `GGML_ARIFI_*` CMake flags are optional unless you intend to load those formats.
- Nothing here has ever been built or run on Linux, macOS, MSVC, clang-cl, CUDA, ROCm/HIP, Arm/NEON,
  or any GPU other than the Radeon 780M (gfx1103) and, for correctness checks only, the Radeon 890M.

**Verify the binaries by mtime, never by the wrapper's exit code** — a build tool can exit 0 having
relinked nothing:

```powershell
Get-ChildItem build-vulkan\bin\llama-*.exe | Select-Object Name, LastWriteTime, Length
```

**Then copy the toolchain's runtime DLLs beside the binaries** — [`docs/BUILDING.md` §4.5](docs/BUILDING.md#45-make-bin-self-sufficient-before-you-run-or-copy-it).
Without that step the binaries start only while your `PATH` happens to hold the right MinGW runtime,
and die with `0xC0000135` or `0xC0000139` when it does not.

---

### Quick start

```powershell
build-vulkan\bin\llama-server.exe -m <model>.gguf -ngl 999 -c 8192 -fa on
```

At startup the Vulkan backend prints one line per non-inherited default it applied. On an RDNA3
device this release prints these seven, among others (copied from our own startup receipt):

```
ggml_vulkan: q4_k w5 split: ON (device-probe (RDNA3)) (n=5..6, m>=131072 or m<=1024, k<=8192)
ggml_vulkan: q5_k mmvq route: route (n=5..8, k<=8192) (device-probe)
ggml_vulkan: q6_k mmvq route: route (MUL_MAT only, n=7..8; n=6 at 5120x6144 on RDNA3 when the n=6 pipeline is rows 1) (device-probe)
ggml_vulkan: iq4_xs mmvq route: legacy (default)
ggml_vulkan: iq3 mat-vec sign-hoist: v2 [GGML_ARIFI_IQ3_MMVQ]
ggml_vulkan: iq3 mat-vec n=7 rows: 2 (device-probe (RDNA3)) [GGML_ARIFI_IQ3_N7_ROWS]
ggml_vulkan: iq3_s mat-vec n=6 rows: 2 (device-probe (RDNA3)) [GGML_ARIFI_IQ3S_N6_ROWS]
```

If a line reads differently, the default did not apply on your device — that is information, not a
fault. The q4_K split, the q6_K n=6 cell and both iq3 rows lines are RDNA3-gated; the q5_K and q6_K
routes are AMD-gated; the sign hoist is ON everywhere.

---

### Switches

Full contract: [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md) — *"the public contract for
runtime-toggled capabilities … An option is never silently enabled because it exists. Every option
records its default, placement boundary, evidence, and safe measurement posture."*

Two warnings from that page, quoted because they change results:

> *"Every default below was chosen under the Windows Balanced power plan."* On this APU the CPU and the
> integrated GPU share one package power budget, and a "performance" plan measured **11.2 tok/s against
> Balanced's 29.0** on GPU-resident decode while *speeding up* CPU-bound work (`FINDINGS.md` F-12).

> *"If you are not on one of the four machines below, read `HARDWARE-PROFILES.md` first."* At least one
> default (`GGML_ARIFI_VNNI_REPACK`) is measurably the wrong choice on a CPU-only path.

#### Added after the R86i tag — UMA placement fix for the Radeon 890M

One engine commit sits on top of R86i: `vulkan: UMA device-local-first placement on the Radeon 890M
(GGML_VK_UMA_PLACEMENT, probe + override)`. Without it, no 27B model we hold loads on the 890M.
Release notes: [`docs/release/RELEASE-NOTES-r86i-x1.md`](docs/release/RELEASE-NOTES-r86i-x1.md).

**The defect.** On a UMA device the upstream chain asks first for memory type
`DEVICE_LOCAL|HOST_VISIBLE|HOST_COHERENT`. On the 890M under Windows the driver accepts every such
allocation but bills it to the WDDM *shared* segment, not to the BIOS reservation. That segment is
capped near half of the system-visible RAM, so a load past about 11.2 GiB dies at the first upload
submit with `vk::Queue::submit: ErrorUnknown`, while the reservation stays empty. Plain `DEVICE_LOCAL`
lands in the reservation.

**The fix.** On AMD device `0x150e` (Radeon 890M) only, the placement chain becomes
`{DEVICE_LOCAL, HOST_VISIBLE|HOST_COHERENT}`. Every other device, the 780M included, keeps the
upstream chain byte for byte. `GGML_VK_UMA_PLACEMENT=auto|legacy|device-local` overrides the probe, and
a startup line names the chain in force:

```
ggml_vulkan: UMA placement: device-local first {DL, HV|HC} (device 0x150e, GGML_VK_UMA_PLACEMENT=auto)
```

**What is checked on the 890M** (correctness only; every receipt below is in `evidence/`):

| Check | Result | Receipt |
|---|---|---|
| GSQ IQ3_S 27B (11.28 GiB file) loads, fix ON | **10.75 GiB in `DEVICE_LOCAL` (the reservation)**, 0.38 GiB host, 0 failed allocations | `evidence/x1-first-contact__placement-summary.txt` |
| Q4_K_XL 27B (16.17 GiB file) loads, fix ON | **15.36 GiB in `DEVICE_LOCAL`**, 0.67 GiB host, 0 failed | same |
| S-X8 v4.3 27B (24.33 GiB file) loads, fix ON | **23.40 GiB in `DEVICE_LOCAL`**, 0.00 GiB host, 0 failed | same |
| the same box with `GGML_VK_UMA_PLACEMENT=legacy` (IQ3_XXS 27B, 9.72 GiB) | 9.19 GiB in the shared-billed type, 0 in `DEVICE_LOCAL`: the upstream chain, reproduced | same |
| `test-backend-ops test -o MUL_MAT`, fix ON | **2305 executed, 2305 OK, 0 FAIL** (876 not supported) — the same 3181 case statuses as the R86i 780M receipt, 0 differ | `evidence/x1-first-contact__chain2-summary.txt` |
| `test-backend-ops test -o MUL_MAT_ID`, fix ON | **1004 executed, 1004 OK, 0 FAIL** (10 not supported), the same counts as the 780M | `evidence/x1-first-contact__chain1-executed-counts.txt` |
| greedy identity, Qwen3.5-9B Q8_0, 4 prompts, `--temp 0 --top-k 1` | **4/4 identical** in each of 3 pairs: unfixed R86i vs fix ON, fix `legacy` vs fix ON, unfixed vs `legacy` — the fix moves memory, not arithmetic | `evidence/x1-first-contact__chain2-summary.txt` |

The unfixed R86i fails to load GSQ IQ3_S and Q4_K_XL on this box (`ErrorUnknown` at the first submit);
that receipt is lane-local and not shipped. The 9B file is the only one we checked that loads on
every arm, which is why identity is shown on it and not on a 27B file.

**Not published: any 890M speed number.** Every 890M throughput round so far was taken under a
measurement harness that we found was disturbing it, so they are indications, not results. The
780M-tuned defaults in this release reach the 890M by feature probe (`RDNA3`-class, AMD vendor), not by
an 890M measurement; treat them as untuned there.

Measured 2026-09-30 on a Minisforum AI X1 Pro-470 — Ryzen AI 9 HX 470, **Radeon 890M** (Vulkan device
`0x150e`), driver 32.0.31041.1004 (AMD 26.8.1), Windows 11 build 29671, Balanced power plan, one pool of
system RAM: 48 GB physical (32 + 16, asymmetric), **24 GB reserved in BIOS**, 23.6 GiB system-visible.

#### Changed in this release (R86i)

Three RDNA3 mat-vec defaults turn ON and one route ships opt-in. Every figure below is copied from
a file in `evidence/`, named beside it; `evidence/MANIFEST.md` carries each file's sha256.

| Change | Switch (revert) | Before → after | Rounds | Receipt |
|---|---|---|---|---|
| q4_K width-5/6 mat-vec split 4+1 (R85), lm_head 248320×5120 | `GGML_ARIFI_Q4K_W5_SPLIT=0` | n=5 **12,749.2 → 9,747.1 us** (1.3082); n=6 13,636.0 → 10,496.4 us (1.3011) | 6/6 each | `evidence/r85-evidence__13-paired-c3.txt` |
| iq3_s 2 rows per workgroup at width 6 (R87), served IQ3_S 27B | `GGML_ARIFI_IQ3S_N6_ROWS` | iq3_s marginal cost of the 6th verify column **+47.23 ± 1.33 → +23.78 ± 2.10 ms/step** | 3 vs 3 launches, Welch 95% | `evidence/r87-evidence__34-served-after-n6r2.txt` |
| q6_K MMVQ admit at width 6, ONE shape (R74b), 5120×6144 | `GGML_ARIFI_Q6K_MMVQ_ROWS` | **713.3 → 691.1 us** (1.0363) | 6/6 | `evidence/r74b-evidence__21-paired.txt` |
| iq4_xs MMVQ route (R75) | `GGML_ARIFI_IQ4XS_MMVQ=route` to opt in | default stays `legacy`: greedy output diverged on the Unsloth file, so no speed claim is made | — | registry row `GGML_ARIFI_IQ4XS_MMVQ` |

Whole-engine checks against r73i, same rig:

| Check | Result | Receipt |
|---|---|---|
| `test-backend-ops test -o MUL_MAT` | **2305 executed, 2305 OK, 0 FAIL** (876 not supported) | `evidence/r86-evidence__19-executed-counts.txt` |
| greedy bit-identity vs r73i | **6/6 identical** (3 prompts × Q4_K_XL and S-X8 27B) | `evidence/r86-evidence__31-bitid-summary.txt` |
| perplexity `-b 6`, GSQ IQ3_S 27B | **6.0281 ± 0.14257 = 6.0281 ± 0.14257** | `evidence/r86-evidence__50-ppl-b6-gsq3s-r73.txt`, `…-r86.txt` |
| perplexity `-b 6`, Q4_K_XL 27B | **5.9932 ± 0.14292 → 5.9928 ± 0.14290** | `evidence/r86-evidence__50-ppl-b6-q4kxl-r73.txt`, `…-r86.txt` |
| served decode, draft depth 4, 4 launches/arm | GSQ **7.988 → 7.925 t/s**, −0.8% ± 6.1%; Q4_K_XL **7.208 → 7.192 t/s**, −0.2% ± 7.0% — **TIE**, CI spans 0 | `evidence/r86-evidence__80-launchlevel.txt` |
| GSQ marginal cost depth 5→6 | predicted **−23.0 ms**; measured cross-session **−23.21 ± 16.06 ms**; same-session **−28.66 ± 34.46 ms** (the number of record, CI too wide to decide alone) | `evidence/r86-evidence__82-marginals.txt`, `…__81-depth-table.txt` |

**What R86i does not show:** no served throughput win at our serve line (draft depth 4, GSQ
**8.118 t/s**, `…__81-depth-table.txt`). The wins live in the width-5/6 verify columns, which a
deeper draft reaches; at our depth the step stays verify-dominated. The Q4_K_XL plain-decode
launch rows are wide (−1.6% ± 27.7%) and carry no claim.

`GGML_ARIFI_Q4K_W5_SPLIT` and `GGML_ARIFI_IQ3S_N6_ROWS` have no row in `docs/OPTIONS-REGISTRY.md`
yet; their defaults above are read from `ggml/src/ggml-vulkan/ggml-vulkan.cpp` (the startup-line
code), and the rows are owed.

#### Carried from R73

**`GGML_ARIFI_Q6K_MMVQ` — q6_K takes the q8_1 MMVQ route. NEW IN R73, default ON (AMD, n=7,8,
`MUL_MAT` only).** `GGML_ARIFI_Q6K_MMVQ=legacy` restores the old path.

**Read the reach before you read the percentages.** A speculative decoder verifies at
**width = 1 + draft depth**, so n=7,8 is reached by a **draft depth of 6 or 7** — the DFlash2
publisher recipe's depth. At our own served draft depth of 4 (verify width 5) it is **latent: zero
served tokens change on this box**, and no throughput claim is made for it.

Upstream keeps q6_K off MMVQ on an unmeasured source comment. Measured at the widths a speculative
decoder verifies at: **+5% to +40% per q6_K verify column**, **58 of 60 paired rounds won** across
five shapes, worst single round **0.9894**. Best cell 248320×5120 at n=7: **20,046 → 14,313 us**,
6/6. `MUL_MAT_ID` is excluded — measured wash. Correctness 83/83; perplexity at `-b 7` and `-b 8`
within one stderr. *These five figures have no receipt shipped in `evidence/` — see
`evidence/MANIFEST.md`, "claims without a shipped receipt".*


| Switch | Registry default (quoted) | Restores the old behaviour |
|---|---|---|
| `GGML_ARIFI_Q5K_MMVQ` | *"**DEFAULT `route` on AMD, `legacy` on every other vendor**"* — scope: *"the `Q5_K` arm of `ggml_vk_should_use_mmvq()` on AMD at `n > 1`, and nothing else"* | `GGML_ARIFI_Q5K_MMVQ=0` |
| `GGML_ARIFI_IQ3_MMVQ` | *"**DEFAULT `v2` on ALL devices — flipped 2026-09-18 (lane-254)**"* — scope: *"the application of the iq3 sign bits, and nothing else"* | `GGML_ARIFI_IQ3_MMVQ=1` (v1) |
| `GGML_ARIFI_IQ3_N7_ROWS` | *"**DEFAULT `2` on AMD RDNA3 (gfx110x)**"* — scope: *"`NUM_ROWS` … of the **`NUM_COLS == 7` iq3 mat-vec pipelines**, `iq3_s` AND `iq3_xxs`, f32 and f16"* | `GGML_ARIFI_IQ3_N7_ROWS=4` |

#### The rest, by area (meanings quoted from the registry)

| Switch | Default (quoted) |
|---|---|
| `GGML_ARIFI_MMVQ_A_HOIST` | *"ON by device probe when unset on AMD, OFF on every other vendor"* |
| `GGML_ARIFI_MMVQ_ROUTE` | *"unset = R48b routing"*; only the exact string `legacy` pins the old rules |
| `GGML_ARIFI_MMVQ_WIDE` / `_WIDE_ROWS_FROM` | *"`legacy` when unset — default-inert"* / *"unset = `4` — default-inert"* |
| `GGML_ARIFI_MMV_MAX_COLS` | *"unset = the inherited routing"* — *"Scope: which of two EXISTING code paths a MUL_MAT takes"* |
| `GGML_ARIFI_Q6K_XFOLD` | *"ON when unset"* (`=0` opts out) |
| `GGML_VK_Q6K_DIRECT_SCALES` | *"ON when unset — this row inverts the usual shape of this registry"* |
| `GGML_ARIFI_Q5K_B_HOIST` | *"ON when unset, but only for mat-vec pipelines with `NUM_COLS` …"* |
| `GGML_ARIFI_SX8_MMVQ` | inverted 2026-09-14 (lane-235/R48b phase 2) |
| `GGML_ARIFI_SX8_MM_PACKED` | *"ON by device probe when unset on AMD RDNA3, OFF on every other"* |
| `GGML_ARIFI_MMQ_UNDER_COOPMAT` | *"OFF when unset — shipped behaviour is byte-identical without it"* |
| `GGML_ARIFI_SX8_PCA` / `_PCA_FILE` | *"ON when a PCA companion GGUF is present"* |
| `GGML_VK_SX8_MMV_ROWS` / `_WG` | *"unset = the probed default"* (an override is taken WHOLE or refused WHOLE) |
| `GGML_ARIFI_FA_DEQUANT_KV` | *"Device default when unset, which is ON on every device"* |
| `GGML_ARIFI_MMV_ID_ROWS` | *"unset = upstream's 4"* (RDNA3) |
| `GGML_VK_HOST_SPLIT_MAX` | *"unset = the derived bound"* — caps bytes one load may place off `DEVICE_LOCAL` |
| `GGML_VK_HOST_SPLIT_MEMTYPE` / `_CACHED` | *"unset = `legacy`, which changes no byte of today's placement"* |
| `GGML_VK_PLACEMENT=bulk-large-heap` | *"OFF when unset; unset changes no byte of the existing placement"* |
| `GGML_VK_UMA_PLACEMENT` | `auto` when unset: device-local first on the Radeon 890M (`0x150e`) only, the upstream chain everywhere else; `legacy` / `device-local` force either chain (see *Added after the R86i tag*) |
| `GGML_VK_ALLOC_TRACE=1` | *"OFF when unset, zero cost"* — diagnostic only |
| `GGML_ARIFI_MMVQ_TRACE` | *"OFF when unset"* — *"no behavioural effect in either arm"* |
| `GGML_ARIFI_OP_DUMP` | *"Unset = off"*, test-only |
| `GGML_ARIFI_VNNI_REPACK` | *"OFF when unset"* — **but set it to `1` on a CPU-only path**; the registry measures `+327%` to `+442%` prompt there |
| `GGML_RECURRENT_STATE_F16=1` | *"unset (`f32`)"* — opt in only after a quality gate |
| `GGML_ARIFI_UMA_READ_PATH` | *"`auto` when unset"* — read-back only |

Build-time: `GGML_ARIFI_ROCMFPX_FORMATS` — *"**OFF.** Build-time option in `ggml/CMakeLists.txt`"*.

#### Every protected win at this tip

The fork keeps a manifest of the changes it must never lose on a rebase:
[`tools/arifi-sync/protected-wins.json`](tools/arifi-sync/protected-wins.json) (30 entries; `arifi_sync.py
protected-win validate` checks it against the tree). The table quotes each entry's registered effect —
its first sentence, verbatim — so the full figure, its baseline, its workload and its quality gate are
one lookup away under the same `id`. "Radeon 780M box" means the SER7 above (the manifest calls it the
seat or RIG-A). A row marked third-party is someone else's figure, quoted and not adopted as ours.

| Protected win (`id`) | Effect as registered (first sentence, verbatim) | Where measured | Opt out |
|---|---|---|---|
| `vulkan-concat-transpose-deltanet` | CONCAT dispatch -12.79% (-15.66 ms per ub512 prefill graph) on the LANE binary | Radeon 780M box (SER7) | GGML_VK_CONCAT_TRANSPOSE=0 |
| `vulkan-tq-matvec-subgroup` | 27B tq3_4s tg32 0.67 -> 1.80 t/s | Radeon 780M box (SER7) | GGML_VK_DISABLE_TQ_SUBGROUP=1 |
| `vulkan-escha-mm-column-block` | 3.51x on escha3 17408x5120 at ncols=64 (135386 -> 38587 us), 3.09x at ncols=8 | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `escha-w2-decode-correctness` | Escha-W2 native decode goes from token salad to coherent | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `cpu-vnni-repack-dual-residency` | decode +22.0% (mixed) and +23.7% (all-Q2_0) against mode 0, ranges DISJOINT, with prefill held at baseline (958.96 vs 898.11 and 963.51 vs 905.25 prompt t/s, ranges overlapping so no prefill gain is claimed). | Radeon 780M box (SER7) | GGML_ARIFI_VNNI_REPACK unset (mode 0) |
| `powerinfer-moe-streaming-hook` | server decode climbs 5.99 / 9.79 / 10.48 / 12.31 t/s against the fork's best 9.00 (RESULTS lane-110C). | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `powerinfer-pipeline-init-reuse` | 9 of 263 rebuilds | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `powerinfer-windows-port-avx-cure` | ENABLEMENT AND CRASH CURE, no throughput number claimed by this commit. | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `powerinfer-expert-bundle-generator` | ENABLEMENT: first disk-streamed sparse inference on Windows proven on this path (RESULTS M2a). | Radeon 780M box (SER7) | GENERATE_EXPERT_BUNDLE unset (the generator never runs) |
| `powerinfer-streamed-repack-carveout` | CORRECTNESS: streamed-expert output goes from garbage to coherent (RESULTS rung2). | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `powerinfer-gpu-safe-staging` | GPU -ngl 99 -cmoe clean at 4.9 t/s with CPU zero-copy retained (RESULTS lane-110B/C). | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `rocmfpx-weight-formats` | CAPABILITY, measured as loadable and correct, not as fast: the native ROCmFP4 artifact loads in llama-server and completes "The capital of France is" with " Paris." at 3.24 tok/s on -ngl 0. | Radeon 780M box (SER7) | GGML_ARIFI_ROCMFPX_FORMATS OFF (the default build) |
| `cpu-vnni-repack-default-off` | The DECISION is what is measured, and it is a two-sided trade taken on numbers: on the default Vulkan-enabled build, OFF avoids a -77.1% / -88.1% prompt regression and forgoes a +20.1% / +28.4% decode gain, ranges disjoint in all four. | Radeon 780M box (SER7) | GGML_ARIFI_VNNI_REPACK=1 restores the upstream default behaviour exactly |
| `cpu-vnni-repack-g128-kernels` | Against the g128 scalar path (mode 0), mode 2 (dual residency): decode 2.01 -> 6.69 tok/s (+232.6%, a 3.3x decode win) and prompt 59.09 -> 85.84 tok/s (+45.3%). | Radeon 780M box (SER7) | GGML_ARIFI_VNNI_REPACK unset (the g128 arm is inside the same slot) |
| `moe-cache-heat-protected-eviction` | THIRD-PARTY, NOT OURS AND NOT ADOPTED AS OURS: llama-cpp-turboquant reports +12% TG (soft) / +7.5% (auto) on an RTX 5090. | not measured by us; the figure is third-party | GGML_CUDA_MOE_CACHE_HOT_USES=0 (every slot is cold, i.e. plain LRU) |
| `cuda-tq3-4s-kernels` | UNMEASURED ON THIS SEAT (no CUDA silicon). | not measured by us; the figure is third-party | none (always on, or the format is selected by the file) |
| `metal-tq3-4s-kernels` | UNMEASURED ON THIS SEAT (no Metal silicon). | not measured by us; the figure is third-party | none (always on, or the format is selected by the file) |
| `vulkan-iq4xs-matvec-dedicated` | LANE-BINARY MEASUREMENT (engine-before -> engine-r5, epochs E1/E2, matched 2x16 GB RAM): U-IQ4XS df2 +0.617 t/s [+0.255, +0.978], 8/8 cells positive | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `vulkan-iq3s-matvec-tpb16-union-gate` | LANE-BINARY MEASUREMENT (engine-r5 -> engine-r4c, E1/E2, matched 2x16 GB RAM): U-Q3KXL df2 +0.820 t/s [+0.622, +1.018], 8/8 positive (8.367 -> 9.268, +10.8%) | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `vulkan-q6k-matvec-direct-scales` | NEUTRAL on gfx1103, and that is the whole claim. | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `vulkan-rocmfp4-fast-q8_1-mmvq` | LANE-BINARY MEASUREMENT (engine-r4 -> engine-r10, epoch E2, matched 2x16 GB RAM): drafted decode 7.498 -> 9.518 mean t/s, +2.0199 [+1.3212, +2.7186], 8/8 cells positive = +26.9% | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `arifi-offrig-ci-backend-matrix` | UNMEASURED and unmeasurable by construction - a build matrix has no t/s. | build matrix, no t/s by construction | none (always on, or the format is selected by the file) |
| `vulkan-fa-dequant-kv-runtime-gate` | R19D D1 (ON vs OFF, ONE binary, same day): plain prefill clean +8.08% [+1.63%, +14.52%], pooled +24.14% | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `spec-dflash-fused-inject-switch` | DOCUMENTED-TAKE, no gain. | Radeon 780M box (SER7) | LLAMA_DFLASH_FUSED_INJECT=1 (restores upstream 662a0b012 behaviour) |
| `vulkan-rdna3-mmv-id-rows-switch` | DOCUMENTED-TAKE, no gain. | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `kv-ple-ngram-index-switch` | DOCUMENTED-TAKE, mechanism NEVER MEASURED. | never reached by any workload we hold | LLAMA_KV_NGRAM_INDEX=0 (the pre-b356fa262 cell scan) |
| `vulkan-mul-mat-id-staging-receipt-hazard-assert` | DOCUMENTED-TAKE, and the receipt is the result: 0 K-padded rows in 7 logs, so the upstream K-padding path CANNOT engage on this GPU. | Radeon 780M box (SER7) | none (always on, or the format is selected by the file) |
| `vulkan-uma-read-path-probe` | DOCUMENTED-TAKE, flat. | Radeon 780M box (SER7) | GGML_ARIFI_UMA_READ_PATH=direct (upstream behaviour) |
| `ggml-sx8-type-57-cpu-decoder` | DOCUMENTED-TAKE. | Radeon 780M box (SER7), CPU path | none (always on, or the format is selected by the file) |
| `ggml-turboq-tbq-kv-types-58-59` | DOCUMENTED-TAKE. | Radeon 780M box (SER7) | -nkvo, -dev none, or a supported KV type (the device guard); LLAMA_ALLOW_TBQ3_KV=1 (the 3-bit KV quality guard) |

The 890M placement fix above is not yet a manifest entry: it is a load fix with a correctness
receipt, and the manifest registers a win with its measured effect.

---

### Reproduce a headline number

The clearest single result in this release is the **iq3 `n=7` register-spill fix** (21–25x on the
affected pipeline). It needs no model file — it is a kernel micro-benchmark.

```powershell
# AFTER (the shipped default on RDNA3)
build-vulkan\bin\test-backend-ops.exe perf -o MUL_MAT -b Vulkan0 > after.txt

# BEFORE (the pre-R66 behaviour)
$env:GGML_ARIFI_IQ3_N7_ROWS="4"
build-vulkan\bin\test-backend-ops.exe perf -o MUL_MAT -b Vulkan0 > before.txt
Remove-Item Env:\GGML_ARIFI_IQ3_N7_ROWS
```

Compare the `iq3_s` rows at `n=7`. On gfx1103 we measured **465,400 → 20,890 us/run** at m=248320,
**34,674 → 1,636** and **39,284 → 1,573** at the other two shapes, 6/6 rounds each. iq3_xxs moves
1.20–1.42x; n=6 and n=8 are flat. Receipt: `2026-09-18-hq-seat-62-state.md` §4p.

Three honest caveats. Dispatch width is `1 + draft`, so only a draft depth of 6 reaches n=7 in
serving — this is a **latent cliff removal**, not a change at draft depth 4. Run it on an otherwise
idle machine: our own measurement law is that any A/B not interleaved on a quiet box is not
trustworthy. And this is a `test-backend-ops` **op-level** number, not throughput — under
**llama-server**, which is our method of record and prints its own perf receipt, this release changes
nothing we could measure: decode ties on all three 27B lines (S-X8 5.433 → 5.530, GSQ IQ3_S
7.771 → 7.681, Q4_K_XL plain 4.095 → 4.108) and the served prefill column beside them sits inside a
launch-to-launch band of 4–26% that our harness could not see past. Full account: the release story
§6.

Correctness for the same paths:

```powershell
build-vulkan\bin\test-backend-ops.exe test -o MUL_MAT -b Vulkan0
```

We report **2305 executed / 0 FAIL / 876 not-supported** on the R86i tip
(`evidence/r86-evidence__19-executed-counts.txt`; the R66 count, 2222, is kept in
`evidence/r66-evidence__19-executed-counts.txt`).

---

### Reporting a result from another GPU

We measure speed on one GPU only (the 780M; the 890M so far has correctness checks only). A result from yours is worth more to this project than another run on
ours. Please include, in the issue:

| Field | Example from our rig |
|---|---|
| GPU + architecture ID | `AMD Radeon 780M Graphics`, RDNA3 / gfx1103 |
| Driver version | `32.0.31041.1004` |
| CPU | `AMD Ryzen 7 7840HS w/ Radeon 780M Graphics` |
| Memory layout | one shared pool: 48 GB physical, 16 GB BIOS reservation, 31.73 GiB system-visible — **say if yours is a discrete GPU with its own memory, because our defaults assume it is not** |
| OS + build | Windows 11, build 29648 |
| Power / performance plan | Balanced (it moved our decode from 29.0 to 11.2 tok/s when changed) |
| Fork tip | `git rev-parse HEAD` in your clone, plus `git describe --tags` (the release tag is named at publication; `r86i-public-2026-09-29` is the tree before the 890M fix) |
| Build line | the exact `cmake` invocation you used |
| The startup lines | which of the three default lines printed on your device |
| Method | how many launches per arm, whether arms were interleaved, and whether anything else ran on the box |

Those fields are our own rig-stamp schema (`python -m arifi_core.rig_stamp`; history in
`registers/rig-history.jsonl`): rig id, RAM epoch, CPU, GPU + driver, DIMMs, system-visible memory, OS
build, BIOS, power plan. Every number we publish carries it, because a number without its rig and its
method is not a result.

## Status

The patch series is real and complete: every fork commit is linear on upstream
**`b10825`** (`9e0e220594af405a62835dc3a27495729fd8506b`) with zero merges, and every one carries
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
published gain claim and negative result, and an explicit release
word.
