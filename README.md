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

## Current planned series

The manifest at [`patches/series/MANIFEST.md`](patches/series/MANIFEST.md)
defines, but intentionally does not yet contain, the following work:

- the ordered `0001`–`0011` PowerInfer/Windows sparse-streaming graft;
- the three ordered thecodacus community-prefetch patches;
- a PrismML ternary `Q2_0_g128` Vulkan-port slot;
- a Windows IOCP-overlap streaming slot; and
- future ArifiLabs profiling and tooling work.

No patch content is included in this Phase-1 skeleton. Conversion of the
verified graft scripts into reviewable commits is a separate batch.

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

Phase 1 is a repository skeleton and provenance layer only. It does not claim
that the planned sparse Vulkan kernel, ternary port, IOCP overlap, or any
individual patch-series contribution has been independently benchmarked.

Publication remains gated by local proof, committed evidence for every
published gain claim and negative result, and the President’s explicit release
word.
