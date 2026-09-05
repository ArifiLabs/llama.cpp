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

### F-13 — A kernel that passes `test-backend-ops` can still change the first token to end-of-generation

**Status: mitigated by a width gate, not root-fixed. The mechanism is still live at two widths, and
the change is held off the default branch because of it.**

While measuring the ROCmFP4-FAST integer-dot mat-vec (the `+27%` drafted win in the changelog), the
new arm returned an **empty completion** — one token, immediately end-of-generation, no text — on one
prompt, twice, deterministically. The float arm, on the same file with the same seed on the same
prompt, returned 72 tokens both times. Those were the only two empty rounds in all 64 rounds of the
comparison.

That is a numerical result, not a harness artifact, and ruling out the harness came first: the
request that produced it sets no presence penalty (see F-16 for why that check is not optional here),
and the two arms differ only in which mat-vec pipelines exist.

The useful part is what happened next, because **three hypotheses were falsified in a row before the
cause was found**, and each falsification was a real experiment rather than an argument.

| # | hypothesis | test | result |
|---|---|---|---|
| 1 | the integer-dot path at width 1 is wrong | rebuild with that width forced back to the float path | **falsified** — the same prompt still returned empty, in both replicates |
| 2 | it is server slot state or prompt cache, not the kernel | run the identical comparison with the two binaries **swapped between slots** | **falsified** — the empties moved *with the binary*, not with the slot. The float binary has never produced one |
| 3 | some other change in the binary, not this port | force the float path for the whole run with `GGML_VK_DISABLE_MMVQ=1` | the empties **disappeared** — the port is implicated, but no width is named |

Naming the width needed one more instrument. The prompt only reproduced under its **exact** cache
history — the same fill and the same preceding rounds, replayed — never in isolation, which is itself
worth knowing. A dispatch census over that exact sequence showed only three widths live on it: 1, 2
and 4. Removing width 4 still reproduced. Removing width 2 returned 72 tokens, twice. **The cause is
width 2**, and the shipped gate retains the path only at widths 3 and 5.

**Two things this finding leaves standing.**

`test-backend-ops` passed the type throughout — 10 MUL_MAT correctness cases and 2 MUL_MAT_ID,
0 FAIL, and the identity-path suite green. It is not a broken test: the divergence sits **inside**
the op's own normalized-error
tolerance, which is exactly the size of divergence that tolerance is designed to admit. A test that
gates on aggregate numerical error over a tensor cannot see a distribution shift at one position that
crosses a sampling boundary. **A green op-level suite is not a statement about the first token.**

And the same arithmetic is still live at widths 3 and 5 — which is where the `+27%` comes from,
because a draft depth of 2 verifies at width 3. The reproducer cannot reach those widths: it is a
plain, undrafted sequence. So what is owed before this can be called closed is named, not implied: an
adversarial cache-history corpus that runs in drafted mode and reaches width 3, or a direct
comparison of the integer-dot and float logits, or a tightened backend error gate. Until one of those
exists, the mechanism stays on its own branch. **Fast but numerically wrong is not a win.**

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

  **The prompt gain's mechanism was ASSUMED here, and the assumption was half wrong. Counted
  2026-07-30.** The earlier text read: "graph splits are 467 in both mode 0 and mode 2, so the same
  ops stay CPU-side during prefill, and in mode 2 those reach the repacked shadow instead of the
  scalar path. On an 8B model that CPU share is large enough to matter where on the 0.5B g64 models
  it was not." A split *count* is not a placement — 467 splits is consistent with any distribution
  of MUL_MATs inside them — so the placements themselves were counted with `GGML_SCHED_DEBUG=2`.
  Counting, not timing: the scheduler's assignment is a function of the graph and the buffer types,
  so a busy machine cannot move it.

  **Confirmed: placement is untouched.** Mode 2's per-graph MUL_MAT placement is identical to mode
  0's across all 24 graph dumps in the run, backend for backend. Dual residency moves no work, which
  is exactly what F-09's design requires and had only asserted.

  **Falsified: the CPU share is not large.** In the offloaded multi-token graph, **249 of 253**
  MUL_MATs run on Vulkan and **4** stay on the CPU — a **1.6%** CPU share, and 0 of 253 in the
  ubatch shape that offloads everything. The 0.5B g64 models F-09 measured were 4 of 169, i.e.
  **2.4%**. The 8B's CPU share is *smaller*, not larger, so "large enough to matter where on the
  0.5B it was not" is wrong on its own terms.

  **What is actually true is better than the guess.** The four CPU-resident MUL_MATs are not a
  random 1.6%; they are, by name, `ffn_gate-35`, `ffn_up-35`, `ffn_out-35` and `result_output` —
  the **final** transformer layer's whole FFN, plus the output head. Three of those are among the
  largest matmuls in the model. So the honest form of the mechanism is not "a big share of prefill
  is CPU-side" but "a *tiny* share of prefill is CPU-side and it is disproportionately expensive,
  and mode 2 is what lets it use the repacked kernel instead of the scalar one". A count and a cost
  are different things, and the original wording confused them.

  **Still owed, and named rather than glossed:** per-operation timing. Identifying the four ops does
  not by itself prove they account for the whole +45.3%; that needs the ops timed individually, not
  inferred from their size. Driver: `trial-evidence/110/g128-kernels/mechanism_probe.py`.

  **Read these numbers as a CPU-tier result, not an absolute one.** The repack path is only
  reachable with `--no-host` (F-04), i.e. with the weights held out of the device host buffer, so
  every figure above is the CPU tier by construction. The GPU tier is faster, and it is now measured
  rather than cited.

  **The GPU tier, on ONE binary (2026-07-30).** An earlier version of this section put the Vulkan
  tier at "roughly 4× faster on both axes" by comparing an *external* record for the PrismML fork's
  Vulkan binary (pp512 356.43 / tg128 28.15) against the 85.84 / 6.69 above. That is the
  cross-binary comparison this project forbids itself — a prior cross-session comparison of that
  shape manufactured a 24% regression that did not exist. Re-measured with the same binary, model,
  prompt and protocol as the table above, three interleaved replicates, medians:

  | tok/s | GPU (`-ngl 99`) | CPU floor (mode 0) | CPU dual (mode 2) |
  |---|---|---|---|
  | prompt | **327.38** | 58.15 (0.18×) | 85.62 (0.26×) |
  | decode | **34.40** | 1.88 (0.05×) | 6.56 (0.19×) |

  The "4× on both axes" figure was wrong, and wrong in the direction that flattered the CPU tier.
  The GPU is **3.8×** the post-repack CPU tier on prompt and **5.2×** on decode — and **18.3×** the
  CPU floor those kernels replaced. What mission #1 fixed is the *floor*: an earlier measurement of
  the 27B g128 recorded its CPU path as "≈33×/2.5× slower; 2-bit CPU kernel unoptimized", and that
  unoptimized kernel is exactly what these replace. The CPU tier is now 3.3× less bad; it is still,
  by a wide margin, the slower tier.

  The two arms with published values were re-measured as the run's own known-answer control and
  reproduced within 6% (mode 0: 58.15 / 1.88 against 59.09 / 2.01; mode 2: 85.62 / 6.56 against
  85.84 / 6.69), which is what makes the third arm trustworthy. Sample counts are uneven and stated
  as measured — prompt n=6 per cell, decode n=3 to n=4, because some rolls emitted no eval-time
  line; the tiers are separated by 4× to 18×, far outside that.

  No prompt comparison against the external 356.43 is drawn even now. That figure is a `pp512`
  synthetic and ours is a 519-token real prefill — comparing them would be the same error one level
  down. Our own Vulkan decode of **34.40** does exceed the external record's 28.15, and that
  observation is offered as an observation, not a benchmark result.

  Where the CPU tier is the one that matters: a build with no GPU at all, a model that will not fit
  in the device's memory, and dual residency's own case, where prefill goes to the GPU and only the
  decode step is served from the repacked shadow. Driver:
  `trial-evidence/110/g128-kernels/gpu_vs_cpu.py`.

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

**Confirmed on a second file, 2026-07-30, and it cost a trial.** This finding was recorded from one
model. Setting up an unrelated speculative-decoding trial, the 27B ternary model was loaded by the
path its own catalog entry gives — and it failed identically:

```
gguf_init_from_reader: tensor 'output_norm.weight' has offset 337715200, expected 357580800
gguf_init_from_reader: failed to read tensor data
```

Same tensor, same overshoot, different model. The practical shape of this defect is now clear and it
is worse than "a file we cannot read": **two files with the same name stem sit side by side, one
loadable and one not**, distinguished only by a `.g128` in the filename. The trial ran four arms
against the wrong one and every arm died before the feature under test was ever reached. Anything
that records a path to one of these models should record the `.g128` spelling, and a reader who
hits the offset error above is not looking at a corrupt download — they are looking at this.

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

### F-14 — The leading explanation for the `q6_K` mat-vec deficit is dead, and the deficit is not

**Status: the change measured neutral and shipped anyway; the deficit it was aimed at is OPEN.**

`q6_K` mat-vec runs about **35% below `q5_K`** at the identical shape on RIG-A. Profiled at
`m=17408 n=3 k=5120`: **41.0 GB/s against 63.8**, a ratio of 0.643, with every `q6_K` row in the
profile sitting in a tight **39.2–41.6 GB/s** band and no `q4_K` or `q5_K` row coming near it. The
deficit is stable and reproducible, and it is not a single unlucky dispatch.

There was one obvious mechanism, and it was structural rather than guessed. `q6_K` was the **only**
`q*_k` mat-vec shader staging its scales through shared memory: it loaded all 16 int8 scales of the
superblock into a double-buffered tile and paid a **`barrier()` per row, inside the row loop**.
`q4_k` and `q5_k` unpack their scales in registers, with no shared memory and no barrier at all. A
per-row workgroup barrier in the hot loop of the one slow shader, absent from the two fast ones, is
about as clean a suspect as this kind of profile produces.

It is not the cause.

The exchange turned out to move no data between threads: all 16 threads of a group work on the same
block, each writes its own scale, and each then reads only four bytes of that same block — off the
same 16-byte cache line the staging was already fetching. So the barrier could be removed for a
**bit-identical** result, and it was, behind a specialization constant so one binary serves both
arms. Measured on the seat serve line, 6 position-balanced interleaved launches per phase: the
`q6_k` op rows read a median ON/OFF ratio of **0.9796** against its own control of **78 untouched op
rows at 1.0061, p10–p90 [0.9479, 1.0636]** (one binary, env-gated); the two-binary diagnostic read
**0.9880** against **0.9990, p10–p90 [0.9520, 1.0527]**. **No `q6_k` row separates from the control
band in either pairing.** Serve level: tied on all six cells. Output: byte-identical, one distinct
sha256 per prompt across 6 serve launches (18 completions).

**The deficit stands, unexplained, and the next attempt has to look somewhere else.** This entry
exists so that nobody spends the same day on the same barrier.

Two corrections to the claim that motivated the work, since they matter to whoever picks it up:
`q5_K` at 63.8 GB/s is confirmed, but `q4_K` **at that same shape** is **60.6 GB/s, not 72.5** — the
72.5 figure is not reproducible there, and the nearest number of that magnitude in the profile belongs
to a *different* shape. The ~35% deficit survives on the `q5_K` comparison alone.

The change is shipped despite measuring neutral, on the ground that it is strictly less work for an
identical result on an engine that runs on other people's hardware, where barrier cost and
shared-memory pressure are not what they are here. **Whether it helps anywhere is unmeasured.** It is
listed in this file, and not in the wins, for that reason.

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

### F-15 — This box's undrafted decode is bimodal, and the mean of a paired comparison follows the slow rounds

**Status: characterised, and mechanised into how verdicts are read. The cause of the slow regime is
itself unresolved.**

On the plain (undrafted) decode line, a minority of rounds land in a slow regime — roughly 3.5–4.4
tok/s against a 5.2 plateau — while generating perfectly normal, complete text. They are not empty
completions and not a content defect. The problem is what they do to a paired A/B: **the cell-wise
mean of differences is dominated by whichever arm catches them**, so the same unchanged kernel can
read as a large win or a large loss depending on where the slow rounds fell.

That was demonstrated, not argued, and the demonstration is the finding. One comparison read the
"after" arm **−1.45 tok/s** with 1 of 8 cells positive — a −33% apparent regression on a width where
the shader is the upstream body bit for bit, so a real loss was mechanically impossible. The obvious
confound was tested first and eliminated: across 28 plain launches over 7 palindromes, the middle
slots the arm occupies read a median-of-medians **5.230** against **5.185** early and **5.076** late,
so there is **no slot-position penalty**. Re-running the identical pair on the identical binaries in
the same epoch then settled it: **medians 5.081 against 5.091 — and the slow rounds landed on the
other arm**, flipping the cell-wise read to +0.60 with nothing changed between the runs. The rounds
are arm-agnostic.

**The rule this pins down:** on the plain line of this box, a verdict is read from the **median plus a
clean-cell interval**, never from the raw cell-wise mean interval alone. The median is stable to
within 0.2% across runs (5.081 / 5.091 / 5.143 / 5.223 over four palindromes in two epochs). The
drafted line does not show this — its medians and its cell-wise intervals agree in every take taken
here — which is worth knowing on its own.

What causes the slow regime is **open**. It cost four separate verdict rows across two mechanisms
before it was characterised, which is why it is written down here rather than left as lore.

### F-16 — A sampler setting on the serve line, inherited by a request that omitted it, manufactured empty completions

A batch of benchmark rounds across several harnesses returned one token and stopped — the same
signature as a real kernel defect, and for a while indistinguishable from one.

The cause was the request shape, not the model and not any kernel. The serve line carries
`--presence-penalty 1.5`. A raw `/v1/completions` body that simply **omits** the field inherits that
value from the server. On a raw, untemplated prompt the presence ring is then primed with the
**prompt's own tokens**, which is enough to put end-of-generation on top at position 0 — the slot
sets its stop reason after one token and releases with no text. Every log signature matched: zero
evaluation time over one token, no draft acceptance line, nothing truncated.

Two things make this worth publishing rather than just fixing.

**It never touched the served product.** The chat endpoint applies a template and was never exposed;
the effect is specific to raw completions, which is exactly where benchmarks live and exactly where
nobody was looking.

**It changed what other measurements were allowed to conclude.** Once the presence penalty is forced
to 0 on every raw request — one shared helper, so a stale caller passing 1.5 is *overridden* rather
than defaulted — an empty completion becomes evidence again. F-13 above rests directly on that: the
sentence "presence is 0 on this endpoint, so this is not the harness class" is only true because this
was fixed first. A measurement harness that can manufacture the exact symptom under investigation
does not merely add noise; it makes the whole class of finding unattributable until it is repaired.

### F-17 — Provenance in a commit message does not stop the next rebase from deleting the result

A fork that keeps rebasing onto upstream has a failure mode that no amount of careful commit
authorship prevents: a measured win is quietly reverted by a bump, an ingest from another fork, or a
conflict resolved the wrong way, and nothing refuses. The commit message still records what was
measured. The code no longer does it.

This fork now carries a machine-readable register of measured wins that a bump has to get past. Each
entry names the mechanism, its commit, the **paths** it lives in, and its **anchors** — the
distinctive symbols and environment gates whose *removal* is itself a collision, so a change that
leaves the file in place but strips the mechanism out of it is caught too. It also carries the entry's
quality gate, the workload the number was taken on, and the measured effect. A bump that touches a
protected path **refuses** unless a comparison authorising it has been recorded.

Three design decisions in it are worth stating, because each one was a defect first:

- **It is authored, never generated.** Protected paths, anchors, quality gates and workloads are not
  derivable from a commit trailer, and a fabricated field makes the guard *lie* — it would pass while
  defending nothing. What *is* derivable is the candidate set, so a `discover` command prints every
  measured commit no entry covers. An entry that cannot be filled in honestly stays out of the
  register and stays on that list, visible.
- **Anchors are searched in source only.** The first version searched the whole tree, which meant the
  register file itself — which lists every anchor by construction — always matched. The rule was
  structurally incapable of reporting any anchor missing. Generated patch mirrors and prose are
  excluded for the same reason: **an anchor that only a document mentions is correctly reported
  absent.**
- **A superseded win is never deleted.** It keeps its entry, its evidence, and the conditions it still
  wins under. "This lost to something better on this box" is not the same statement as "this does not
  work", and on an engine that runs on hardware we do not own, the difference is the whole point.

One bug found by the register's own tests rather than by reading is worth the space, because it is the
shape this whole file exists for: the classifier deciding whether a commit *stated* a measured effect
tested its no-effect vocabulary by **prefix**, and that vocabulary contained a bare `-`. So
`Measured-effect: -12.79% per dispatch` classified as *no effect* — and a speedup written as a
negative delta is the single most common shape a win takes here. **A guard blind to the most likely
shape of the thing it guards is not a guard.** The test that pins it is named after exactly that case.

## Open questions

Listed because they are unresolved, not because they are unimportant.

- **The `q6_K` mat-vec bandwidth deficit** — about 35% below `q5_K` at the same shape. The per-row
  barrier was the leading explanation and is **falsified** (F-14). No replacement hypothesis has been
  measured.
- **The ROCmFP4-FAST integer-dot mat-vec at widths 3 and 5.** The width that changed a sampled token
  is gated out and the win reproduces without it, but the same arithmetic is live at the two retained
  widths and the reproducer cannot reach them (F-13). An adversarial drafted-mode corpus, or a direct
  logits comparison, is owed before that mechanism can go on the default branch.
- **Why some undrafted decode rounds run at two-thirds speed** while generating normal text (F-15).
  Characterised well enough to read verdicts correctly; not explained.
- **The tiled concat-transpose on decode and on mixture-of-experts models.** Only the prefill graph was
  measured. The upstream author's larger claims are on the paths not measured here.
- **One benchmark launch ran about 45% faster than every other launch of either arm**, whole-graph
  totals included. A session-first-launch position effect is real and measured at 7–11%, which is four
  to five times too small to absorb it. The launch is retained and labelled; the pooled figure it sits
  in is **not quotable**.

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
