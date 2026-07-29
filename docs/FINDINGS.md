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
exists because of F-04 below.

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

1. **Keep both copies** — a host-visible copy for batch work and a repacked copy for decode. This
   genuinely gets both wins and costs the memory of the affected tensors twice.

   **This is the direction, and the reason is worth stating.** The framing above — *choose* which
   processor gets the batch work — is only forced if the goal is to pick the single best default.
   It is the wrong frame for a machine with unified memory, where the GPU, the CPU and the RAM are
   one pool and the point is to use all three at once. Duplicating a tensor changes no weight and
   no router decision, so it cannot cost quality; it spends memory, which is a resource, to buy
   the prefill win and the decode win at the same time instead of trading one for the other. Only
   Q1_0/Q2_0 tensors are affected, so the cost is bounded and measurable rather than global.

   Shape of the work: leave the tensor in a GPU-visible buffer so the scheduler can still offload
   large-batch matmuls, and maintain a lazily-built repacked shadow that the CPU path uses when an
   operation lands on the CPU — which is every single-token decode. The existing measurements say
   what to expect: prefill keeps the ~950 tok/s it has today instead of collapsing to ~100, and
   decode keeps the +20–28% the repack path already demonstrated.
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
