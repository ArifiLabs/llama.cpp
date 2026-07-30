# Findings

The research log for this fork: what we tried, what it did, and how anyone can check.

Most projects publish their wins. This file also publishes the losses, the wrong turns, and
the things we still do not know — because on a fork whose whole pitch is *"we measured it"*,
a missing negative is a missing result.

**Reference hardware — RIG-A:** Beelink SER7, AMD Ryzen (Zen 4) + Radeon 780M integrated
graphics (RDNA3, gfx1103, 12 CU), 32 GB DDR5-5600 with a 16 GB unified-memory carve-out,
Windows 11, MinGW/UCRT, Vulkan backend. Every number below is from this machine unless stated.
A number without a machine attached is not a result.

**How we measure.** The judge is always `llama-server` itself, never a synthetic benchmark
binary. Fresh server per arm, free memory recorded at every launch, and a comparison counts
only when the code path is independently evidenced as having actually run. That last rule
exists because of F-04 below. The **active Windows power plan is declared with every number** and
the harness refuses to run under any other one — that rule exists because of F-12, which cost us
eight days of numbers.

---

## Defects

### F-01 — Upstream's x86 repack kernels crash on Windows (found here, fixed here)

**Status: fixed in this fork; offered upstream.**

`q4_0`, `iq4_nl` and `mxfp4` models crash with an access violation (`0xC0000005`) as soon as
the CPU repack path is reachable — i.e. with `--no-host`. It reproduces on a clean build of
stock upstream `b10173`, unpatched.

The three kernels pass a nibble lookup table to a shared GEMV/GEMM template as a **by-value
32-byte vector**. On the Windows x64 ABI a value that size is passed in memory, and the ABI
guarantees only 16-byte stack alignment — but the compiler emits an alignment-*requiring*
store for the spill, in a frame with no realignment prologue. A worker thread whose frame
lands 16-mod-32 faults. Captured under a debugger:

```
=> ggml_gemv_q4_0_8x8_q8_0+57:  vmovdqa %ymm0,0x40(%rsp)
   rsp = 0x4e8ff830  ->  store target 0x4e8ff870, which is 16-mod-32
```

Three worker threads faulted at the identical address. Whether a given thread survives is
purely whether its frame happened to land 32-aligned, which is why it presented as
intermittent.

This also resolves the one observation that made no sense: `q4_K` never crashed despite using
the very same repack buffer. It has its own kernel and never passes a vector by value. The
crashing set is *exactly* the three types routed through these two templates.

**Fix:** the lookup table is derivable from the template's own block type, so the parameter is
removed and built inside the callee. The value stays in a register and the ABI crossing
disappears.

| build, with `--no-host` | q4_0 | iq4_nl | mxfp4 | q4_K (control) |
|---|---|---|---|---|
| stock `b10173` | crash | crash | crash | OK |
| this fork | serves | serves | serves | OK |

After the fix, greedy output against the same binary with repack disabled: `iq4_nl` and
`mxfp4` byte-identical; `q4_0` coherent and self-consistent across 3/3 rolls, differing by one
token — the pre-existing activation-quantization difference described in F-05, not this change.

### F-02 — A runtime feature flag does not cover compile-time-selected code

Three separate A/B comparisons of the ternary VNNI kernels were worthless before this was
caught. The work was gated behind `GGML_ARIFI_VNNI_REPACK`, so `=0` was *assumed* to restore
baseline. It did not: the same change also touched a dot-product function selected at **build**
time from an architecture dispatch table, entirely outside the runtime gate — and that function
carried its own out-of-bounds read. Every ON/OFF comparison was one broken kernel against a
differently broken one.

**Rule adopted:** before trusting any A/B, establish that the OFF arm *is* the baseline. For
everything the change touches, ask whether it is selected at runtime (a flag covers it) or at
build time (a flag does not). This is why the ROCmFP and TurboQuant formats are gated with a
CMake option rather than an environment variable.

### F-03 — A silent `default:` in a type dispatch switch

Adding a new quantization type appears to work, then aborts at runtime inside an unrelated
operation, because type-id switches carry `default: abort`. Every new format must be walked
through *every* dispatch site. There is a second, opposite trap: switches that are exhaustive
and carry no `default:` will emit `-Wswitch` for the disabled build if the new cases are put
behind the feature gate — those need unconditional cases. Both traps were hit, in that order.

### F-04 — Two identical A/B arms proved nothing, because neither ran the code

The repack path is unreachable without `--no-host`: the device host buffer type is selected
ahead of the CPU extra buffer types, so on a GPU build the repack buffer never wins. Two runs
produced clean "no difference" results where the honest reading was "the feature never
executed in either arm."

**Rule adopted:** an A/B whose arms agree proves nothing until path engagement is independently
evidenced. We check for the repack buffer's own allocation line in the log, per run.

### F-05 — Repack numeric divergence is inherent, not a bug

Repacked weights produce slightly different logits from the same model unrepacked. We built a
control: four models quantized from one source, each carrying a different **upstream** repack
type, all routed through the same activation quantizer.

| repack type | owner | max Δ logprob | output |
|---|---|---|---|
| ternary 4x8 (ours) | this fork | **0.061** | identical |
| Q2_K 8x8 | upstream | 0.140 | identical |
| Q4_K 8x8 | upstream | **1.391** | **text changes** |

Upstream's own source documents the mechanism: the repack activation quantizer *mirrors*
rather than *matches* the scalar path. Bit-identity was the wrong acceptance bar. Ours is the
least divergent of the three measured.

---

## Format findings

### F-06 — Ternary `Q2_0` at 128-value groups is not upstream's `Q2_0`

Upstream's `Q2_0` is a **64**-value group (18 bytes/block). The ternary models this fork
targets use **128**-value groups (34 bytes/block). Same family name, different wire format;
upstream cannot read the latter at all, and the refusal is at the tensor table, before any
kernel runs.

Upstream's Vulkan `Q2_0` support merged one day *before* this fork's earlier base was pinned,
so it was never a competing implementation — this fork's g128 support is an extension on top of
it, not a parallel one. Re-verified at the `b10173` rebase: nothing to drop.

**g128 now has the CPU repack kernels too (2026-07-29).** Until then the flagship ternary format
had no fast CPU path: `block<K, N>` derived its group size from the bit width via `QK_0<K>()`, so
the template could not express "2-bit at group 128" and a g128 model logged
`q2_0_g128 ... cannot be used with preferred buffer type CPU_REPACK`. The fix is a
parameterisation, not a new kernel. `block` takes a third argument `int QK = QK_0<K>()`, which
leaves every existing `block<K,N>` spelling valid, and the AVX512-VNNI GEMV/GEMM are templated on
`<BlockX4, QK>`. The vector core needed no change at all: `__q2_0_expand_x4` loads exactly one
`__m256i` and has no notion of the block, and the `32 * k` qs stride is
`NB_COLS(4) * QK8_0(32) * 2 bits / 8` — a constant of the row interleave, not of the group.

Two group sizes now mean two arms and two kernel instantiations, deliberately. A g128 row entering
a g64 kernel is **mis-strided silent wrong math, not a crash** — 34 bytes read as 18, with the
activation loop running two sub-blocks where it needs four — so the type that selects an arm is
carried as a template parameter all the way into the repack, which asserts it.

Measured on `Ternary-Bonsai-8B-Q2_0.g128.gguf` (RIG-A, `llama-server`, `-ngl 0 --no-host -c 2048
-t 8`, temp 0, streamed oracle compared on token ids, 3 rolls per cell): repacked output is
**token-identical to the scalar path** on both a GEMV-driving and a GEMM-driving prompt, with max
|Δ logprob| **0.021–0.040** — inside the F-05 band and below this fork's own g64 ternary figure of
0.061. The repack buffer claims **1759.50 MiB**, which is 252 of the model's 254 ternary tensors;
the two it declines are `output.weight` and `token_embd.weight`, both `[4096, 151669]`, refused by
the same `ne[1] % 4` condition the g64 arm has always applied.

Dual residency came free. Adding the type to `ggml_arifi_dual_eligible` was enough because the x4
block is exactly four plain blocks — the property the shadow sizing asserts — so g128 gets the
same prefill-and-decode result: **252 shadow tensors, graph splits matching mode 0 (467) rather
than mode 1 (430)**, and mode 2 **bit-identical to mode 1** under `--no-op-offload`
(max |Δ logprob| `0.000000000`).

The refactor rewrote the **shared** g64 kernels on the way through, so g64 was re-run against the
pre-change answer key rather than assumed unaffected: 36 cells over three g64 models, every token
identical and every logprob delta exactly `0.000000000`.

  **Speed, measured 2026-07-29** (RIG-A, llama-server, `Ternary-Bonsai-8B-Q2_0.g128.gguf`,
  `-ngl 0 --no-host -c 2048 -t 8`, 519-token prefill, `n_predict 128`, modes **interleaved** across
  three replicates x 5 rolls, roll 1 dropped, n=12 per cell, one binary with only the env flip):

  | tok/s | mode 0 (off) | mode 1 (claim the buffer) | **mode 2 (dual)** |
  |---|---|---|---|
  | prompt | 59.09 | 17.44 (**-70.5%**) | **85.84 (+45.3%)** |
  | decode | 2.01 | 6.67 (**+231.6%**) | **6.69 (+232.6%)** |

  Every figure has non-overlapping ranges. Two separate results here. The g128 scalar path was
  **2.01 tok/s**, so the repack kernels are a **3.3x decode win** - far larger than g64's +22-24%,
  because g64 already had a fast scalar `vec_dot` and g128 never did. And dual residency wins
  **both** axes on g128, where on g64 the honest reading was only that it never *loses* prefill.

  The prompt gain's mechanism is **ASSUMED, not measured**: graph splits are 467 in both mode 0 and
  mode 2, so the same ops stay CPU-side during prefill, and in mode 2 those reach the repacked
  shadow instead of the scalar path. On an 8B model that CPU share is large enough to matter where
  on the 0.5B g64 models it was not. Separable with `--no-op-offload` or `GGML_SCHED_DEBUG=2`; not
  done here.

  **Read these numbers as a CPU-tier result, not an absolute one.** The repack path is only
  reachable with `--no-host` (F-04), i.e. with the weights held out of the device host buffer, so
  every figure above is the CPU tier by construction. On this same 8B g128 model the Vulkan tier is
  roughly **4x faster on both axes** — an external record for the PrismML fork's Vulkan binary puts
  it at pp512 **356.43** / tg128 **28.15 t/s**, against the 85.84 / 6.69 measured here. So what
  mission #1 fixed is the *floor*: an earlier measurement of the 27B g128 recorded its CPU path as
  "≈33x/2.5x slower; 2-bit CPU kernel unoptimized", and that unoptimized CPU kernel is exactly what
  these kernels replace. The CPU tier is now 3.3x less bad; it is still the slower tier.

  Where the CPU tier is the one that matters: a build with no GPU at all, a model that will not fit
  in the device's memory, and dual residency's own case, where prefill goes to the GPU and only the
  decode step is served from the repacked shadow.

  **Not yet measured on one binary.** The 356.43 / 28.15 figures are another binary's, and this
  project's own rule is to reuse a binary rather than a number — a prior cross-session comparison
  manufactured a 24% fork regression that did not exist. A same-binary GPU-vs-CPU arm for g128 is
  written and owed: `trial-evidence/110/g128-kernels/gpu_vs_cpu.py`.

**Method note:** compare struct *geometry*, never names or comments. An earlier round of this
work recorded the relationship backwards from names alone and propagated the error into
several documents; it was a source of F-02's crash. In-source size comments are also stale in
at least one upstream project — the `static_assert` is the truth.

### F-07 — The type-id "collision" that blocked four features did not exist

Four features were parked on a believed collision between three projects' type-ids. Checking
each project's own file-format registry showed the overlapping ids are **runtime-only** types
that no tool can write into a file. File-format ids and in-memory ids are separate namespaces
that never meet on disk. All four features could adopt their originators' ids verbatim, which
means files from those projects load natively with no conversion.

The real allocation hazard is different and remains live: this fork's one invented id sits on
the next value upstream will hand out. That is tracked in
[`docs/TYPE-ID-ALLOCATION.md`](TYPE-ID-ALLOCATION.md).

### F-08 — Per-hardware defaults are set by named profile, not auto-detection

Automatic detection was rejected on a specific argument: *"is there a GPU"* is the wrong
question. What decides whether a CPU-side optimisation helps is whether **these tensors** get
offloaded, which depends on flags and available memory and is resolved *after* buffer-type
selection — so a startup probe cannot answer the question the default depends on. Profiles log
verbatim what they chose and why; no shipped default moves silently.

---

## Measured losses

Kept because a mechanism that lost here may win elsewhere, and because the numbers are the
point.

| Mechanism | Result on RIG-A | Disposition |
|---|---|---|
| KV-cache compression (TurboQuant) | storage claim real (5.1–7.5× vs f16) but every coherent config lost to plain `q4_0` on **both** size and speed | not shipped; revisit where memory headroom outranks decode speed |
| `TQ4_1S` weight format | 438.3 MiB @ 4.47 tok/s vs `Q4_0` 408.9 MiB @ 49.3 tok/s — larger and ~11× slower | shipped, default OFF; no SIMD kernel exists and it is a higher bits-per-weight operating point |
| Ternary VNNI repack, on a **GPU** build | prompt processing −77% to −88% | default OFF; the same feature is +327–442% on a CPU-only build |
| Speculative drafter for the ternary family | 0.54× (7.9 → 4.3 tok/s) | compiled, default OFF |
| Whole-fork merge of an AMD-format project | −14.2% on long prompts | rejected as a whole; individual mechanisms taken instead |
| Host prefetch patches | inert — an integrated GPU shares system memory, so there is no upload to overlap | carried for discrete-GPU users, labelled |

---

### F-09 — Why the repack/GPU trade-off is not a placement bug

The obvious reading of the −77% prompt regression is that repacked weights are simply put in the
wrong buffer, and that smarter placement would keep both the decode win and the prefill speed.
We investigated that and it is not what is happening.

The two buffer types are not better and worse versions of the same thing. A host-visible buffer
holds weights in normal layout in CPU memory that **the GPU can read directly**, so large-batch
matmuls are offloaded to it. The repack buffer holds the same weights in an interleaved layout
that only the CPU's vector kernels understand, and the GPU cannot read it at all. Choosing one is
choosing which processor gets the batch work.

That is why prefill and decode split the way they do: prefill is a large batch and wants the GPU;
decode is one token at a time, runs on the CPU in both arrangements, and simply prefers the faster
CPU layout. The right choice therefore depends on the *operation*, which is not known when the
buffer is selected at model-load time.

Three ways out, none free, all recorded rather than assumed:

1. **Keep both copies** — a host-visible copy for batch work and a repacked copy for decode.
   **Built and measured. `GGML_ARIFI_VNNI_REPACK=2` on RIG-A.** It is the shipped answer to this
   finding, and it holds both wins at once.

   The framing above — *choose* which processor gets the batch work — is only forced if the goal is
   to pick a single best default. It is the wrong frame for a machine with unified memory, where
   the GPU, the CPU and the RAM are one pool. Duplicating a tensor changes no weight and no router
   decision, so it cannot cost quality; it spends memory, which is a resource, to buy the prefill
   win and the decode win at the same time. Only Q1_0/Q2_0 tensors are affected, so the cost is
   bounded and it is printed.

   **The placement was measured before anything was built**, because the whole design depends on a
   claim this file had only asserted. `GGML_SCHED_DEBUG=2`, all-Q2_0 0.5B, `-ngl 0 --no-host`:
   with the weight in a plain CPU buffer, **165 of 169** prefill `MUL_MAT`s run on Vulkan0 and
   **169 of 169** decode `MUL_MAT`s run on the CPU; with it in `CPU_REPACK`, **all 169** prefill
   `MUL_MAT`s move to the CPU. That is the −77…−88% prompt collapse seen directly as placement.
   Decode is CPU-side in *both* arrangements, so the prefill/decode split is already free — the
   only thing the repack buffer changes is where the bytes live.

   That is also why the implementation is small. The offload gate
   (`ggml/src/ggml-backend.cpp:944`) tests whether the weight's buffer is host-resident and
   resolves to the CPU backend; it never inspects layout. And `ggml_cpu_extra_compute_forward`
   (`ggml/src/ggml-cpu/traits.cpp:12`) consults `get_tensor_traits` for every op regardless of
   buffer type. So the tensor stays exactly where mode 0 puts it — placement is byte-for-byte the
   mode-0 arrangement — and the CPU path is handed a separately allocated repacked shadow. No new
   buffer type: `ggml_backend_cpu_device_supports_op` short-circuits to an extra buffer type's own
   `supports_op` for *every* op whose src lives there, so a dual buffer type would have had to
   answer for ops it knows nothing about.

   **Measured, same protocol as the rows above** (llama-server, `-ngl 0 --no-host -c 2048 -t 8`,
   519-token prefill, `n_predict 128`, temp 0, three interleaved replicates × 5 rolls, roll 1
   dropped, n=12 per cell, one binary with only the env flip, free RAM 5.60–5.72 GB at every
   launch):

   | tok/s | mode 0 (off) | mode 1 (claim the buffer) | **mode 2 (dual)** |
   |---|---|---|---|
   | prompt, mixed-Q2_0 | 898.11 | 310.65 (**−65.4%**) | **958.96** (+6.8%, ranges overlap → noise) |
   | prompt, all-Q2_0 | 905.25 | 178.25 (**−80.3%**) | **963.51** (+6.4%, ranges overlap → noise) |
   | decode, mixed-Q2_0 | 40.30 | 49.53 (**+22.9%**) | **49.16 (+22.0%)** |
   | decode, all-Q2_0 | 40.95 | 50.95 (**+24.4%**) | **50.65 (+23.7%)** |

   Every figure marked with a percentage has ON and OFF ranges that do **not** overlap and a gap
   wider than the largest within-arm spread. The prefill gain for mode 2 is deliberately *not*
   claimed: the ranges overlap, so the honest reading is that dual residency **does not recover
   prefill — it never loses it.** Decode reaches mode 1's win to within the noise.

   Cost, and it is printed rather than estimated: the shadow is **4.68 / 84.16 / 95.98 MiB** on the
   4-, 72- and 168-tensor models — byte-for-byte what mode 1 puts in its own buffer. The per-tensor
   log rows make that check stronger than a byte total: **4 / 72 / 168 distinct tensor names**, so
   the two modes are confirmed to select the same tensor *set*, not merely the same number of bytes.

   Quality: unchanged, and tested at the layer where it can actually be tested. With
   `--no-op-offload` — which pins mode 2's prefill to the CPU exactly like mode 1's, isolating the
   shadow from where prefill ran — mode 2 is **bit-identical to mode 1** on all six model × prompt
   cells: same tokens, max |Δ logprob| **0.000000000**, deterministic 3/3. Under normal offload,
   mode 2 remains bit-identical to mode 1 on any prompt below the batch-32 offload threshold, and
   its divergence from mode 0 is never larger than mode 1's. On the 4-tensor control model all
   three modes are token-identical.

   **A caution the numbers above hide.** The first working version of this was *slower than doing
   nothing* — decode 40.9 → 24.7 tok/s — while passing that entire correctness sweep, because
   `get_tensor_traits` runs once per matmul **per worker thread** and the first implementation left
   a `getenv`, a `strstr`, an exclusive mutex and an atomic RMW on that path. The design was right
   and the plumbing ate it. A green correctness sweep says nothing about the cost of the machinery
   that makes it correct.
2. **Enable repack only when no GPU backend exists at all.** Unlike "will this tensor be
   offloaded", "is there any GPU device" *is* answerable at load time, and when the answer is no
   there is no offload eligibility to lose.

   **We looked at implementing this and stopped, on purpose.** The reference machine has an
   *integrated* GPU with unified memory, and the buffer accounting shows the GPU still offloading
   batch work from ordinary CPU memory even with the host buffer type disabled — because on
   unified memory it can read that memory directly. On a discrete GPU it could not, so the same
   flag has a different cost there. We can only measure one of those two classes on the hardware
   we have. Flipping a shipped default from a test that structurally cannot enter the other case
   is how you ship a regression to users you never tested. The flag stays manual until someone
   measures a discrete GPU.
3. **Repack after prefill.** Layout is a property of the buffer, not of the tensor, so this means
   re-laying-out weights mid-run — plausible for a long-lived server, not cheap.

Until one of those lands, the flag stays off by default and the advisory tells CPU-only users to
turn it on, which is the configuration where the answer is unambiguous.

### F-10 — A validator that runs after the thing it was meant to validate

A GGUF may declare the ternary type-id for a **64**-value group while its payload is laid out at
**128** (F-06). The loader has a function written for exactly that case:
`llama_verify_q2_0_g128_spans` measures each tensor's on-disk span and compares it against *both*
geometries, refusing anything ambiguous. It never runs on such a file.

`gguf_init_from_reader` walks the tensor table first and computes each tensor's expected offset by
accumulating sizes **from the declared type**. At 18 bytes/block against a 34 bytes/block payload
the running offset overshoots on the first ternary tensor, and the file is rejected before any
llama-level code sees it:

```
gguf_init_from_reader: tensor 'output_norm.weight' has offset 165015872, expected 174722688
gguf_init_from_reader: failed to read tensor data
```

The mechanism is sound and the ordering defeats it. Nothing downstream can rescue the file,
because the disambiguation it needs happens two layers above where it dies.

This is recorded rather than fixed because the fix is a real design choice, not a patch: either
`gguf_init` learns to consult the format key before it sizes anything, or the key is read in a
pre-pass. Both widen a hot, security-relevant path — offset validation is what stops a malformed
file from being read out of bounds — so it is not something to do casually on the way past.

**The practical consequence is a documentation one.** Code comments in the repack dispatch used to
justify their g128 exclusion by pointing at the load-time retype that this path was supposed to
perform. That justification rested on something unreachable. The dispatch now keys on the g128
type directly and does not depend on it.

### F-11 — Fallback kernels that no dispatch path can reach

Every interleaved kernel has a portable `_generic` twin, and `arch-fallback.h` maps the public name
onto it for the seven non-x86 architectures. For the ternary types that mapping leads nowhere.

The dispatch arms for `Q2_0` and `Q2_0_G128` are gated on AVX512-VNNI **and nothing else**.
`Q1_0`, sitting immediately above them in the same function, carries three arms — AVX512-VNNI,
NEON with `matmul_int8`, and NEON with `dotprod`. So on ARM, PowerPC, RISC-V, s390 and WASM the
ternary types never claim the repack buffer, their `_generic` kernels are compiled into every one
of those builds, and not one of them is ever called. On x86 the same is true of any build without
AVX512, because the predicate and the kernel body key off the *same* compile-time macros:

```
$ g++ -march=x86-64-v3 -fsyntax-only probe.c     # #error if __AVX512F__/__AVX512VNNI__ absent
probe.c:2:2: error: #error "ggml_cpu_has_avx512/_vnni would return 0 here"
$ g++ -march=native   -fsyntax-only probe.c      # exit 0
```

`ggml_cpu_has_avx512()` is `#if defined(__AVX512F__)`, not a CPU query, so no build configuration
can open the dispatch while closing the kernel. There is no way to reach this code from a model.

That makes it the one class of kernel a model-level correctness sweep can never cover, which is
why [`tests/test-ternary-repack.cpp`](../tests/test-ternary-repack.cpp) calls all four variants
directly — both group sizes, GEMV and GEMM, AVX512 and generic — against a reference written the
other way round: the kernels accumulate integer products and scale per activation sub-block, the
reference dequantizes to float and dots. The test carries its own inverse claim as well, feeding
g128-packed weights to the g64 kernel and requiring the result to be **wrong**; it comes out wrong
by `43064.94` against a tolerance of `0.000053`, which is what "mis-strided is silent wrong math,
not a crash" looks like when you make it visible.

Two harness defects were caught by that test's own scalar self-check before it ever judged a
kernel — an uninitialised `ggml_table_f32_f16` (every scale decoding as zero, so every kernel
returned zero and read exactly like a broken kernel) and a reference holding more precision than
the fp16 block it described. Both would have been reported as kernel failures by a test that
compared only against itself.

**Not fixed here.** Giving ARM a ternary repack arm means shipping kernels this project cannot
measure, which is the one thing its defaults policy (F-08) exists to prevent.

---

## Measurement findings

### F-12 — A "performance" power plan halved our iGPU inference, and every inference-level suspect read clean

We publish this because for eight days it silently taxed everything measured on RIG-A, and
nothing in the inference stack could have told us.

A popular Windows optimizer's performance-focused power plan (installed deliberately, eight days
before anyone noticed) cut GPU-resident decode from **29.3 to 11.2 tok/s** — while making tiny
CPU-bound models run about **4× faster** at the same time. Full GPU residency, correct flags, no
background load, no thermal event: every suspect checked clean, because the fault was not in the
inference stack at all.

The bisection, each row a fresh server on the same model and the same command, minutes apart:

| Power configuration | decode tok/s |
|---|---|
| Windows Balanced | 29.0 |
| Balanced + minimum-processor-state 100% (alone) | 29.0 |
| Balanced + AMD Power Slider "best performance" (alone) | 28.8 |
| **both together** | **14.5** |
| **the optimizer's plan, verbatim from its `.pow` file** | **11.2** |

Every single-variable arm is innocent. Only the pair reproduces the collapse.

**Mechanism.** On an APU the CPU and the integrated GPU draw on **one package power budget**.
Pinning the cores at maximum clock (minimum processor state 100%) *underneath* the maximum AMD
performance slider saturates that budget continuously, and the iGPU can no longer boost. Either
setting alone leaves headroom; together they starve the GPU. **A "CPU performance plan" is an
anti-GPU plan on shared-budget silicon** — and because it genuinely accelerates CPU-bound work,
single-workload benchmarking is structurally incapable of catching it.

Nor is Balanced merely the least bad of the tweaked plans, and we did not assume it: the four plans
were measured against each other on the same box within the same hour, same command — Balanced
**29.12** and **28.95** on two probes, Ultimate Performance **28.90**, High Performance **28.39**,
the optimizer's plan **11.07**. Both shipped "performance" plans are marginally *worse* than
Balanced for this GPU-bound workload, and High Performance is a mild version of the same mechanism.
Balanced is the pinned bench plan because it measured best here, not because it is the default.

Three rules came out of it, and all three are now enforced rather than remembered:

1. **Declare the power plan with every benchmark.** Our harness hard-refuses to run unless the
   active scheme is the declared one. A wrong plan is a refused bench, not a footnote.
2. **A relative A/B is not environment-immune.** An environment shift can move two processors in
   *opposite* directions and so manufacture — or hide — a 20%-plus "win". Compare absolutes against
   the incumbent record before believing any improvement ruling.
3. **Bisect to the interaction, not to the setting.** Every single-variable arm here was innocent.

The one thing this finding cannot do is repair the numbers taken inside the window. A throttled
box hides a real win exactly as easily as it invents a fake one, so measurements from those eight
days are re-verified before they are quoted, not adjusted.

## Open questions

Listed because they are unresolved, not because they are unimportant.

- **Asynchronous disk reads for expert streaming.** Built and correct; speed effect
  **unresolved**, because the test model's experts largely fit in the OS file cache and there
  was little disk traffic to hide. Needs a model genuinely larger than memory.
- **Expert streaming's headline case.** Proven to work, but demonstrated on a model that mostly
  fits. The bigger-than-memory demonstration is owed.
- **Higher-precision recurrent state.** Carried behind a flag, **unmeasured** — nothing in the
  daily workload exercises that model family.
- **An upstream fix for multi-token prediction layer counting** did not reproduce a measurable
  gain here. On unified memory the layer-placement decision it corrects may simply not bind.
  Reported as a hardware-class caveat, not as a refutation of the upstream fix.
