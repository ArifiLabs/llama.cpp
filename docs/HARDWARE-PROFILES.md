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

## Pick your profile

### CPU-only (no GPU backend compiled, or `-dev none`)

| Setting | Value | Why |
|---|---|---|
| `GGML_ARIFI_VNNI_REPACK` | **`1`** — change this | The single largest CPU-only win in the fork. Requires a **g64** Q1_0/Q2_0 model, and `--no-host` on a Vulkan-enabled build. |
| thecodacus prefetch (`GGML_SCHED_PREFETCH_EXPERTS`) | leave OFF | There is no host→device transfer to hide. |
| `--ctx-checkpoints 0` | keep | Largest historical gain; not hardware-specific. |
| KV cache | f16, or `q4_0` for capacity | Not `turbo*` — see the KV row below. |
| `GGML_ARIFI_ROCMFPX_FORMATS` | OFF unless you have such a file | Build-time. These types are CPU-only anyway, so a CPU-only host is the one place they cost nothing extra. |

> Caution: `--no-host` is what makes `CPU_REPACK` reachable — and also what exposes upstream's
> 8×8 repack crash (`0xC0000005`). See [`BUILDING.md` §7](BUILDING.md#7-troubleshooting).

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

**The proposal we would accept**, in order of preference:

- **Named profiles.** `--arifi-profile cpu-only|gpu-offload` resolving to a documented flag set,
  logged verbatim at startup so any benchmark record shows what was actually in effect. Explicit,
  greppable, no silent behaviour, no cross-backend dependency.
- **A startup advisory.** When the CPU backend is the only one registered *and* the model contains
  Q1_0/Q2_0 tensors *and* the toggle is unset, log one line naming the flag and its measured
  effect. Changes nothing, costs nothing, and closes the discovery gap for the person it affects.

Both are small. Neither has been implemented, and this paragraph is not a claim that they have.
