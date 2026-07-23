# Options registry

This registry is the public contract for runtime-toggled capabilities in the
ArifiLabs llama.cpp series. An option is never silently enabled because it
exists. Every option records its default, placement boundary, evidence, and
safe measurement posture.

## Defaults by hardware path

| Hardware path | Default posture |
|---|---|
| Beelink SER7 / Radeon 780M unified memory | Vulkan is the AMD path. Keep thecodacus host-transfer prefetch OFF by default; it measured inert when experts are GPU-resident. DSpark is OFF after a `0.54×` result. Use `--ctx-checkpoints 0`. |
| SER7 upgraded to 64 GB or 96 GB DDR5 | Same Vulkan posture. KV f16 becomes unconditional under the 64 GB ruling when model placement permits. Prefer fitting the model in unified memory before adding streamed tiers. |
| AMD Strix Halo 128 GB unified memory | Vulkan remains the expected AMD path. Start with the unified-memory defaults: prefetch OFF until a host-offloaded trial proves a transfer boundary; KV f16 preferred. |
| NVIDIA DGX Spark 128 GB unified memory | Use upstream CUDA backend. Keep backend-agnostic streaming and graph work available; start with prefetch OFF unless a genuine host-to-device transfer path is present and measured. |

## Runtime controls

| Control | Default | What it controls | When to turn it on | Evidence and cost |
|---|---|---|---|---|
| `EXPERT_BUNDLE_PATH` | unset | Loads a generated Q4_0 expert bundle from SSD for supported MoE models. | Use when the model’s experts cannot remain in RAM/VRAM and a compatible bundle exists. | Capability proven: 21B streamed coherently at `5.9–6.1` CPU decode t/s; Qwen3.6-35B streamed from a 17 GB bundle at `1.6` CPU decode t/s; tiered `-ngl 99 -cmoe` reached `3.0` t/s. It is not a claim of sparse Vulkan execution. |
| `MAX_N_CACHED` | implementation-selected; document the chosen value in a run record | Bounds hot-expert RAM cache capacity. | Increase only after measuring cache pressure, RAM headroom, and bundle I/O behavior. | No independently accepted ON/OFF benchmark is recorded. Treat as a measured tuning variable, not a presumed gain. |
| `LANE110_PROF=1` | unset | Emits per-operation timing for pipeline initialization, scheduling, build tasks, and forward execution. | Diagnosis only. Disable for benchmark runs. | Pipeline-init reuse itself reduced rebuild work from `2472 µs` to `0–2 µs`; profiler overhead has not been independently quantified. |
| `LANE110_PREFETCH_CAP` | implementation default, currently `3*n_expert_used` | Limits standalone expert prefetch aggressiveness. | Sweep under cache pressure after establishing the baseline. | Recorded server sequences: default approximately `[5.07/9.03/9.58/12.01]`; cap `1` `[5.13/9.88/10.29/11.22]`; cap `96` `[5.79/11.73/12.19/11.26]`. Default retained because apparent ramp differences require controlled retest. |
| `--ctx-checkpoints 0` | `0` | Disables server prompt rewind/checkpoints. | Keep off unless a workload demonstrably requires rewind semantics. | Largest historical gain: stock rewind `39.9/68.4/280`; ON_DEVICE checkpoint `84.8/128.8/341.6`; disabled approximately `101/150/372` across short/medium/long prefill. |
| `--spec-type draft-mtp` and `--spec-draft-n-max N` | model- and hardware-specific | Enables MTP speculative decoding and sets draft depth. | Use only with a validated compatible draft and an `N` sweep. | Not universal: 31B root-MTP improved `1.8 → 9.4` t/s; Qwen35MoE lost to plain Q4_0 (`22.8` best tuned versus `29.2`); Gemma-4-26B-A4B improved `29.3 → 37.3` t/s at `N=3`. |
| KV f16 / q4_0 | f16 primary where capacity permits | KV-cache storage precision. | Use q4_0 only as an explicit context-capacity trade-off. | President ruling: f16 is primary; at 64 GB RAM f16 is unconditional for the intended path. Do not claim an unsourced universal speed or quality delta. |
| `-cmoe` and `-ngl` | placement-specific | Controls CPU MoE placement and GPU layer offload. | First prefer full unified placement when it fits; use tiered placement when it does not. | Full unified `-ngl 99 --n-cpu-moe 0 -c 4096` reached `29.2` t/s in the recorded fit case. CPU-only streamed was `1.6` t/s; GPU-tiered `-ngl 99 -cmoe` was `3.0` t/s. |
| DSpark: `--spec-type draft-dspark --spec-draft-n-max 4 -ngld 999` | OFF on current rig | Target-specific speculative drafter for Ternary-Bonsai-27B. | Only after a target-specific controlled benchmark on another hardware path. | It engaged on 780M Vulkan but fell from `7.89` to `4.27` t/s (`0.54×`). `N=4` is required; other values crash. |
| `GGML_SCHED_PREFETCH_EXPERTS` | OFF on unified-memory defaults | thecodacus patch 2: overlaps host-offloaded expert upload with compute. | Discrete-GPU or proven host-offloaded transfer regime. | Documented `+64.5%` prefill in the host-offloaded `-cmoe` condition. Inert when experts are already GPU-resident; it does not enable MTP. |
| `GGML_SCHED_MAX_PREFETCH_SLOTS` | implementation default | thecodacus patch 3: sizes per-layer prefetch slots. | Tune only with patch 1 and 2 in dependency order and a transfer-bound workload. | No independent isolated gain accepted; part of the host-transfer bundle and inert in the shipped GPU-resident Vulkan configuration. |
| mmap pinning | compiled capability; no universal default | thecodacus patch 1 pins mmap-backed CPU weights. | Use only where CPU-mapped weights must cross a host-to-device boundary. | First dependency in the bundle; inert as part of the triplet on GPU-resident unified-memory Vulkan. |

## Required benchmark discipline

1. Record exact model, quant, prompt shape, flags, backend, hardware, and engine SHA.
2. Compare a fresh binary against a fresh binary; reuse binaries, never historical
   timing numbers.
3. Do not time under DR activity or sibling load.
4. Use the President-ruled `llama-server` benchmark harness for acceptance,
   including repeated in-process requests where warm behavior matters.
5. Preserve both positive and negative results. “No effect in this placement” is
   an accepted outcome, not permission to omit the option from the registry.

## Build variants

Local builds default to **web-UI ON** (President ruling 2026-07-22: he wants the
llama.cpp web UI available for interactive testing). The earlier UI-OFF posture was
purely crash containment for `ki-webui-0xC0000139` (`STATUS_ENTRYPOINT_NOT_FOUND` from a
shadowed mingw `libstdc++` DLL when `ui-assets.cmake` runs the embed helper
unconditionally, even with the UI-OFF flags set). That crash is now **cured by the static
GCC runtime** (`-DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"`), which makes
the embed helper immune to DLL shadowing, so UI-ON builds link cleanly.

| Variant | Flags | Use | Verified |
|---|---|---|---|
| UI ON (local default) | omit the four UI/WEBUI OFF flags; keep static-libgcc/libstdc++ + `-D_WIN32_WINNT=0x0A00` | Local dev / interactive web-UI testing. | 2026-07-22 (lane-110): `build-vulkan-ui` — embed ran without `0xC0000139`, server binary 70.59 MB vs 67.81 MB UI-OFF (+2.78 MB embedded assets), generated `tools/ui/ui.cpp` = 14.3 MB. Verified statically (no server launched). |
| UI OFF (minimal / CI-bench) | `-DLLAMA_BUILD_UI=OFF -DLLAMA_BUILD_WEBUI=OFF -DLLAMA_USE_PREBUILT_UI=OFF -DLLAMA_USE_PREBUILT_WEBUI=OFF` | Bench-proof and CI builds where the UI is irrelevant to decode paths. | Remains available; `build-vulkan` bench artifact built this way. |

**UI asset source (2026-07-22):** the npm build path (`node v24.14.1` already installed —
`npm install` + `npm run build` into `tools/ui/dist`), not the prebuilt-release download.
The UI embed touches no decode path; a UI-ON server binary is bench-equivalent to UI-OFF —
note the binary used in any RESULTS entry.

### Q2_0 g128 compatibility gate

| Option | Default | Scope | Safety rule |
|---|---|---|---|
| GGUF metadata `GGML_Q2_0_G128=1` | Absent by default; loader gate is off when absent on all hardware | Per-file compatibility declaration for PrismML/HF Q2_0-g128 GGUFs | The loader remaps serialized `GGML_TYPE_Q2_0` only when this key is present with scalar value `1` and at least one Q2_0 tensor span unambiguously matches g128 geometry. It refuses a g64-only, neither-match, or all-alignment-ambiguous file. Never add the key to upstream g64 Q2_0. |
| `GGML_TYPE_Q2_0_G128` | Internal only | CPU and Vulkan dispatch after verified metadata remap | Not serialized into existing GGUFs; preserves canonical serialized `GGML_TYPE_Q2_0` g64 behavior. |
| Q2_0 g128 Vulkan | Available, OFF unless gate is set | 780M, discrete AMD, CUDA hosts with Vulkan build | Enable only for verified g128 models; use CPU fallback if the selected backend lacks the g128 pipeline. |

Existing verified g128 files are marked with
`tools/gguf-retag-g128/retag_g128.py`. The tool writes a new sibling copy by
default, validates GGUF magic plus the same raw-or-alignment-padded Q2_0 span
math used by the loader, and only then writes `GGML_Q2_0_G128=1`. `--in-place`
is an explicit opt-in and warns before replacement. This is a metadata-key
gate, not the superseded ambient environment-variable design: the model file
declares its required geometry, while an absent key keeps normal g64 behavior.
