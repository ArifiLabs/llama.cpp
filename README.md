# ArifiLabs llama.cpp

An attribution-first, rebaseable llama.cpp fork for local inference across current and future ArifiLabs hardware.

This repository starts from upstream llama.cpp master
(`9e0e220594af405a62835dc3a27495729fd8506b`, tag `b10825`) and carries an ordered,
feature-toggled patch series. Its scope is deliberately broad: one engine for
the current Beelink SER7, future 64/96 GB RAM configurations, an AMD Strix Halo
128 GB system, and an NVIDIA DGX Spark 128 GB system.

The fork preserves upstream backends. Vulkan is the blessed AMD path for the
current gfx1103 estate; upstream CUDA remains available for NVIDIA hardware.
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
running the Vulkan backend — or, where a row says CPU-only, the CPU backend on that same box.

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

Everything ArifiLabs adds lives as a **linear patch series** on top of upstream `b10453`
(`4df29be4f`). The canonical branch is **`arifi/main`** and it carries **zero merge commits** — the
series exists to be replayed onto a newer upstream tag, and a merge commit is a hole in it
(`format-patch` omits merges, so their hand-made conflict resolutions never reach the series; that
was a real, measured failure here before the history was flattened).

```bash
git clone -c core.longpaths=true <this repo> arifilabs-llama.cpp && cd arifilabs-llama.cpp
python tools/arifi-sync/arifi_sync.py series check     # regenerate + verify the series
python tools/arifi-sync/arifi_sync.py series replay --onto 4df29be4f   # reproduces arifi/main
```

The procedure for the next upstream bump, the next fork ingest, and what must be re-verified
afterwards is written out command-by-command in [`UPDATE-RUNBOOK.md`](UPDATE-RUNBOOK.md).

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

## The R66i release — what shipped, how to build it, how to reproduce a number

> **Where the receipts live.** Receipt paths of the form `r66-evidence/…`, `r65-evidence/…` and other `r*-evidence/…` names in this document are **lane-local**: they name files in the lane worktree that produced them, not paths in this repository. A clone does not contain them. `evidence/README.md` lists what is tracked here and says plainly what is not.

*Applied from the release-prep branch, 2026-09-21. Switch meanings below are quoted
verbatim from `docs/OPTIONS-REGISTRY.md`.*

### What this is

An attribution-first, rebaseable llama.cpp fork for local inference on **Vulkan**.

- Upstream base: `ggml-org/llama.cpp` **b10825**, `9e0e220594af405a62835dc3a27495729fd8506b`.
- On top of it: **586 patches**, linear, non-merge, individually toggleable, each carrying its
  provenance in the commit message.
- Release tag: **`r66i-integration-2026-09-19`** → `94461e770`.
- The series is generated from git, never hand-edited, and verified on every run: every patch is
  byte-identical to a fresh generation *and* to a committed blob, and replaying all 586 with `git am`
  reproduces the tree exactly. Receipt: `r66-evidence/74-series-check.txt`.

**It is for anyone on Vulkan.** A proven-safe improvement ships enabled even where our own GPU shows
no gain — other drivers may not already do what AMD's does. One of the three defaults turned on in
this release is exactly that case.

**Every number in this repository was measured on one machine**: a Beelink SER7 Pro — Ryzen 7 7840HS,
**Radeon 780M (RDNA3, gfx1103)**, driver 32.0.31041.1004, Windows 11 build 29648, Balanced power plan,
one pool of system RAM (48 GB physical, 16 GB reserved in BIOS), Vulkan with `KHR_coopmat`. Kernels
transfer; numbers do not. If you are on other hardware, read
[`docs/HARDWARE-PROFILES.md`](docs/HARDWARE-PROFILES.md) first — several defaults are wrong for you.

---

### Build

Windows / MinGW-w64 / Vulkan. **This is the only configuration any number in this repository was
measured on** (source: `UPDATE-RUNBOOK.md:849-884`):

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
  do not compile.
- `-D_WIN32_WINNT=0x0A00` in **both** `C` and `CXX` flags — without it the Windows IOCP path fails to
  compile. One of the two is not enough; the failure lands in whichever language you left out.

**Rig-specific, not requirements:**

- `-j2` is a property of *our* box, not of the fork: Vulkan shader generation is memory-hungry and a
  wider build dies on a machine with one shared pool of RAM. Use what your machine can feed.
- `-D_WIN32_WINNT=0x0A00` is Windows-only.
- The two `GGML_ARIFI_*` CMake flags are optional unless you intend to load those formats.
- Nothing here has ever been built or run on Linux, macOS, MSVC, clang-cl, CUDA, ROCm/HIP, Arm/NEON,
  or any GPU other than gfx1103.

**Verify the binaries by mtime, never by the wrapper's exit code** — a build tool can exit 0 having
relinked nothing:

```powershell
Get-ChildItem build-vulkan\bin\llama-*.exe | Select-Object Name, LastWriteTime, Length
```

---

### Quick start

```powershell
build-vulkan\bin\llama-server.exe -m <model>.gguf -ngl 999 -c 8192 -fa on
```

At startup the Vulkan backend prints one line per non-inherited default it applied. In this release
you should see all three of:

```
q5_k mmvq route: route (n=5..8, k<=8192) (device-probe)
iq3 mat-vec sign-hoist: v2 [GGML_ARIFI_IQ3_MMVQ]
iq3 mat-vec n=7 rows: 2 (device-probe (RDNA3))
```

If a line is missing, the default did not apply on your device — that is information, not a fault.
Lines 1 and 3 are device-gated (AMD; RDNA3 respectively).

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

#### Changed in this release

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
| `GGML_ARIFI_Q6K_MMVQ` | *"unset = the shipped route (MMVQ for Q6_K on Intel only)"* |
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
| `GGML_VK_ALLOC_TRACE=1` | *"OFF when unset, zero cost"* — diagnostic only |
| `GGML_ARIFI_MMVQ_TRACE` | *"OFF when unset"* — *"no behavioural effect in either arm"* |
| `GGML_ARIFI_OP_DUMP` | *"Unset = off"*, test-only |
| `GGML_ARIFI_VNNI_REPACK` | *"OFF when unset"* — **but set it to `1` on a CPU-only path**; the registry measures `+327%` to `+442%` prompt there |
| `GGML_RECURRENT_STATE_F16=1` | *"unset (`f32`)"* — opt in only after a quality gate |
| `GGML_ARIFI_UMA_READ_PATH` | *"`auto` when unset"* — read-back only |

Build-time: `GGML_ARIFI_ROCMFPX_FORMATS` — *"**OFF.** Build-time option in `ggml/CMakeLists.txt`"*.

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

We report **2222 executed / 0 FAIL / 876 not-supported** on the promoted tip
(`r66-evidence/17-correct-MUL_MAT.txt`, `19-executed-counts.txt`).

---

### Reporting a result from another GPU

We cannot test any GPU but one. A result from yours is worth more to this project than another run on
ours. Please include, in the issue:

| Field | Example from our rig |
|---|---|
| GPU + architecture ID | `AMD Radeon 780M Graphics`, RDNA3 / gfx1103 |
| Driver version | `32.0.31041.1004` |
| CPU | `AMD Ryzen 7 7840HS w/ Radeon 780M Graphics` |
| Memory layout | one shared pool: 48 GB physical, 16 GB BIOS reservation, 31.73 GiB system-visible — **say if yours is a discrete GPU with its own memory, because our defaults assume it is not** |
| OS + build | Windows 11, build 29648 |
| Power / performance plan | Balanced (it moved our decode from 29.0 to 11.2 tok/s when changed) |
| Fork tip | `git rev-parse HEAD` — e.g. `94461e770`, tag `r66i-integration-2026-09-19` |
| Build line | the exact `cmake` invocation you used |
| The startup lines | which of the three default lines printed on your device |
| Method | how many launches per arm, whether arms were interleaved, and whether anything else ran on the box |

Those fields are our own rig-stamp schema (`python -m arifi_core.rig_stamp`; history in
`registers/rig-history.jsonl`): rig id, RAM epoch, CPU, GPU + driver, DIMMs, system-visible memory, OS
build, BIOS, power plan. Every number we publish carries it, because a number without its rig and its
method is not a result.

## Status

The patch series is real and complete: every fork commit on `arifi/main` is linear on upstream
`b10453` (`4df29be4f`) with zero merges, and every one carries
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
