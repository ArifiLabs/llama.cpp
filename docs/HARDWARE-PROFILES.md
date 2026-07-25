# Hardware profiles — which defaults are wrong for you

Every default in this fork was chosen on **one machine**: a Beelink SER7 with a Radeon 780M and
unified memory, running Vulkan. Those defaults are correct there because they were measured there.

A single default is necessarily wrong for whoever it does not match. This page exists so you can
find out which ones are wrong for *you* in one read, instead of discovering it in a benchmark.

[`OPTIONS-REGISTRY.md`](OPTIONS-REGISTRY.md) remains authoritative for the evidence behind every
row here. This page is the routing layer, not a second source of truth.

---

## The one question that changes the most

> **Do your weights end up on a GPU, or on the CPU?**

Not "do you own a GPU" — do the matmuls for *this model, at this `-ngl`, in this build* run on it?
The fork's most consequential toggle flips on exactly that, and in opposite directions:

| | CPU-only path | GPU-offloaded path |
|---|---|---|
| `GGML_ARIFI_VNNI_REPACK=1` | **+327% to +442% prompt**, +20% to +24% decode | **−77% to −88% prompt** (decode still +20% to +28%) |

Same flag, same binary, same models. Repacked weights lose GPU-offload eligibility, so the batch
matmuls that were running on the iGPU move back to the CPU. It ships **OFF**, which is right for
the rig it was measured on and wrong for a CPU-only user.

Measured 2026-07-24, `llama-server`, `-ngl 0 --no-host -c 2048 -t 8`, 419-token prefill, n=9 per
arm, ON/OFF ranges disjoint in all eight comparisons. Full protocol and every raw value are in the
registry's `GGML_ARIFI_VNNI_REPACK` row.

---

## Two mechanisms ship for this

Both are **opt-in or informational**. Neither changes a shipped default, and neither exists to
guess: they exist so the thing you would have had to discover in a benchmark is on your screen
before the model finishes loading.

### 1. `--arifi-profile NAME` (also `LLAMA_ARG_ARIFI_PROFILE`)

`cpu-only` or `gpu-offload`. Sets a documented group of runtime toggles and prints every one of
them, with the evidence, before anything loads:

```
$ llama-server --arifi-profile cpu-only -m model.gguf
arifi profile: 'cpu-only' selected - no GPU backend in use - every matmul runs on the CPU
arifi profile: evidence for every line below is docs/HARDWARE-PROFILES.md and docs/OPTIONS-REGISTRY.md
arifi profile:   GGML_ARIFI_VNNI_REPACK=1 - measured on a CPU-only path: prompt +326.7% / +441.8% ...
arifi profile:   GGML_SCHED_PREFETCH_EXPERTS=0 - nothing is offloaded, so there is no host-to-device ...
arifi profile:   GGML_ARIFI_ROCMFPX_FORMATS is build-time, compiled OFF, NOT settable here - ...
arifi profile: no shipped default was changed by this fork; a profile is opt-in and sets only the ...
```

Rules it obeys:

- **A variable you set yourself always wins.** The profile logs `KEPT - your environment wins` at
  warning level and moves on. A profile never overrides an explicit choice.
- **An unknown name is a hard error**, naming the profiles that exist. It never falls back.
- **`gpu-offload` changes no behaviour at all.** `GGML_ARIFI_VNNI_REPACK=0` and unset are the same
  branch. It exists so a benchmark log states what was in effect instead of implying it.
- **Build-time options are reported, never set** - see below.
- It must be applied before the first model load, which is why it lives in the argument parser:
  the toggles latch into function-local statics the first time a tensor buffer is initialised.

Only one thing differs from shipped defaults in either profile: `cpu-only` sets
`GGML_ARIFI_VNNI_REPACK=1`, and only because you asked for it by name.

### 2. A startup advisory, when the defaults are wrong for you

If a load ends with **no offload device**, the model carries **Q1_0/Q2_0 tensors**, and you have
**not** set the toggle either way, the fork says so once and changes nothing:

```
llama_arifi_vnni_repack_advisory: ArifiLabs advisory: no offload device was selected and this model carries 168 Q1_0/Q2_0 tensor(s)
llama_arifi_vnni_repack_advisory: ArifiLabs advisory: GGML_ARIFI_VNNI_REPACK=1 measured prompt +326.7% / +441.8% and decode +19.9% / +23.6% on a CPU-only path (two g64 models, ArifiLabs rig, 2026-07-24). UNMEASURED on any other machine
llama_arifi_vnni_repack_advisory: ArifiLabs advisory: it ships OFF because it costs prompt -77.1% / -88.1% when weights are GPU-offloaded, which is the rig it was measured on. NOTHING HAS BEEN CHANGED - set it yourself, or pass --arifi-profile cpu-only. docs/HARDWARE-PROFILES.md
```

The predicate is **not** "is a GPU present". It is "did this load end up with zero offload
devices", evaluated after device selection and before tensors load - the first moment the question
is actually answerable, and the last moment before the expensive part of the load. It is silent on
a GPU path, silent when you have already decided, silent for a model with no Q1_0/Q2_0 tensors,
and silent during the memory-estimation pass `llama-server` runs before the real load.

`GGML_TYPE_Q2_0_G128` deliberately does **not** count. The loader has already remapped it away
from `Q2_0` and every repack kernel is hard-wired to 64-value blocks, so the toggle provably does
nothing there; advising it would be a false advisory.

> The advisory is a library-level `INFO` log. `llama-server` filters those out at its default
> verbosity - raise it (`-lv 9`) if you do not see it.

### Build-time options are never faked

`GGML_ARIFI_ROCMFPX_FORMATS` is a **CMake option**, not a runtime toggle: its `vec_dot` lives in
`type_traits_cpu[]` and is chosen when the binary is linked. A profile that claimed to flip it at
runtime would be wrong on arrival, so:

- profiles **report** it (`is build-time, compiled OFF, NOT settable here`) and never set it;
- setting it in the environment produces a **warning that it is being ignored**, whether or not a
  profile was selected, because the trap does not require a profile to fall into.

---

## Pick your profile

### CPU-only (no GPU backend compiled, or `-dev none`)

`--arifi-profile cpu-only` sets the first two rows for you and prints what it did. The rest are
your call, for the reasons in "What is still not implemented" below.

| Setting | Value | Why |
|---|---|---|
| `GGML_ARIFI_VNNI_REPACK` | **`1`** - change this | The single largest CPU-only win in the fork. Requires a **g64** Q1_0/Q2_0 model. |
| thecodacus prefetch (`GGML_SCHED_PREFETCH_EXPERTS`) | leave OFF | There is no host-to-device transfer to hide. |
| `--ctx-checkpoints 0` | keep | Largest historical gain; not hardware-specific. |
| KV cache | f16, or `q4_0` for capacity | Not `turbo*` - see the KV row below. |
| `GGML_ARIFI_ROCMFPX_FORMATS` | OFF unless you have such a file | Build-time. These types are CPU-only anyway, so a CPU-only host is the one place they cost nothing extra. |

> **`--no-host` is about which case you are in, not about the toggle.** `make_cpu_buft_list()`
> only inserts a device host buffer type by iterating the *selected offload devices*. With none
> selected - `-dev none`, or a build with no GPU backend - the list is empty, nothing outranks the
> CPU extra buffer types, and `CPU_REPACK` is reachable without `--no-host`. Verified: `-dev none
> --arifi-profile cpu-only` and no `--no-host` puts 95.98 MiB into `CPU_REPACK` on
> `qwen2.5-0.5b-instruct-Q2_0-g64.gguf`, against 0.00 MiB with the toggle off.
>
> `--no-host` *is* required in the mixed case, where a GPU device is selected but you push the
> weights back with `-ngl 0` - which is exactly the condition the measurements above were taken
> in. It is also what exposes upstream's 8x8 repack crash (`0xC0000005`). See
> [`BUILDING.md` section 7](BUILDING.md#7-troubleshooting).

### AMD unified memory + Vulkan (the rig everything was measured on)

Ship defaults as-is. `GGML_ARIFI_VNNI_REPACK` **stays OFF** — turning it on costs you 77–88% of
prompt throughput. DSpark stays OFF (`0.54×` measured). Prefetch stays OFF (measured inert).

### NVIDIA / CUDA

Use upstream's CUDA backend; this fork does not modify it. Start from upstream defaults and treat
every ArifiLabs toggle as OFF until you measure it. **Nothing in this fork has been built or run on
CUDA hardware.** The DGX Spark row in the registry is a *plan*, not a result.

### Discrete GPU with a real host→device boundary

This is the one placement where the three thecodacus prefetch patches are expected to help — their
author measured `+64.5%` prefill in the host-offloaded `-cmoe` regime they were written for. That
is **his** number in **his** regime, reproduced here as a citation. On our unified-memory rig they
measured inert. Turn them on in dependency order and measure.

---

## Every toggle, and who should change it

| Mechanism | Default | Change it if… | Do **not** change it if… |
|---|---|---|---|
| `GGML_ARIFI_VNNI_REPACK` | OFF | You are CPU-only **and** running a g64 Q1_0/Q2_0 model | Any weights are GPU-offloaded — prompt throughput collapses |
| `POWERINFER_IOCP` | ON (Windows) | You hit a streaming stall — set `0` to fall back to synchronous reads | You are chasing throughput. **The A/B was noise; there is no verdict.** Do not quote a number for this flag |
| TurboQuant KV (`turbo2`/`turbo3`) | **not shipped** | — | Ever, for speed. Measured smaller *and* slower than plain `q4_0`: turbo3 350 MiB @ 8.5 t/s vs `q4_0` 504 MiB @ 19.8 t/s. The storage claim is real; the trade is not worth it |
| PowerInfer expert streaming (`EXPERT_BUNDLE_PATH`) | unset | Your model's experts **cannot** fit in RAM/VRAM and you have a compatible bundle | The model fits. This is a capability for running what otherwise would not run — `1.6–6.1` t/s — not a speed-up |
| `MAX_N_CACHED` | implementation-selected | You have measured cache pressure and RAM headroom | You are guessing. **UNMEASURED** — no accepted ON/OFF benchmark exists |
| Q2_0 g128 (GGUF key `GGML_Q2_0_G128=1`) | absent → off | You have a verified PrismML/HF g128 file, retagged with `tools/gguf-retag-g128/` | You have an ordinary g64 Q2_0 file. Never add the key to one — the loader gate is fail-closed for a reason |
| `GGML_ARIFI_ROCMFPX_FORMATS` | OFF (build-time) | You need to read or write a ROCmFPX GGUF | You want speed. **Performance UNMEASURED.** No GPU kernels exist for these types, so every matmul falls to CPU |
| MTP (`--spec-type draft-mtp`) | model-specific | You have a validated draft **and** you sweep `N` | You expect it to generalise. It does not: 31B `1.8 → 9.4` t/s, but Qwen35MoE *lost* (`22.8` vs `29.2`) |
| DSpark drafter | OFF | You are on hardware that is not a 780M **and** you benchmark it | You are on a 780M — measured `7.89 → 4.27` t/s (`0.54×`) |
| `GGML_RECURRENT_STATE_F16` | unset (f32) | A recurrent/hybrid model passed your own quality gate | You have not run that gate. Recurrent rounding compounds |
| `--ctx-checkpoints 0` | `0` | Your workload genuinely needs rewind semantics | Otherwise — this is the program's largest historical gain |

---

## Why this is a table and not automatic detection

The obvious improvement is to detect the situation at runtime: if no GPU backend is present, turn
`GGML_ARIFI_VNNI_REPACK` on by itself. We considered it and **did not implement it**, for reasons
worth stating rather than hiding:

1. **Layering.** The decision lives in `ggml_repack_get_optimal_repack_type()` inside `ggml-cpu`.
   Making it depend on whether a Vulkan or CUDA backend exists would make the CPU backend
   introspect its siblings, which is a dependency the upstream architecture does not have and
   which we would then have to carry across every rebase.
2. **"GPU present" is the wrong predicate.** What actually matters is whether *these tensors* get
   offloaded, which depends on `-ngl`, `--no-host`, model size and free VRAM — all resolved after
   buffer-type selection has already run. A build-time or device-count proxy would silently get it
   wrong for the mixed case, and a silently-wrong default is exactly the failure this page exists
   to prevent.
3. **It would change a measured default with no measurement behind the change.** This fork's own
   rule is that a default flips only on evidence. Auto-detection has none yet.

Those three reasons still stand, and auto-detection is still **not implemented**. What shipped
instead are the two mechanisms above: `--arifi-profile`, and the startup advisory. Both are
explicit, both are greppable, neither adds a cross-backend dependency, and neither moves a
measured default without a measurement.

### What is still not implemented

- **Runtime auto-detection.** Nothing flips `GGML_ARIFI_VNNI_REPACK` for you, ever. The advisory
  tells you; you decide.
- **Profiles for anything but the two CPU/GPU cases.** There is no `nvidia`, no `discrete-gpu`, no
  `unified-memory` profile, because there is no measurement on that hardware to build one from.
- **Everything else in the table below** stays manual: KV cache type, `--ctx-checkpoints`, MTP,
  DSpark, PowerInfer expert streaming, `POWERINFER_IOCP`, `MAX_N_CACHED`,
  `GGML_RECURRENT_STATE_F16`. A profile may only contain settings this fork has measured and can
  defend; most of these have a verdict of UNMEASURED, UNRESOLVED, or "depends on your model", and
  a profile that bundled them would be inventing a recommendation.
- **`GGML_SCHED_PREFETCH_EXPERTS` in `gpu-offload`.** Its author measured `+64.5%` prefill in the
  host-offloaded `-cmoe` regime; on our unified-memory rig it measured inert. Two different
  answers, neither of them ours to generalise, so the profile does not touch it on a GPU path.
- **Any measurement off this rig.** Every number quoted by the profile and the advisory was
  measured on one machine. That is stated in the log text itself, not just here.
