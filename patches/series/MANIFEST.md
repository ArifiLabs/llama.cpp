# ArifiLabs patch-series MANIFEST

<!-- GENERATED FILE - DO NOT EDIT BY HAND.
     Regenerate:  python tools/arifi-sync/arifi_sync.py series regen
     Verify:      python tools/arifi-sync/arifi_sync.py series check  -->

## What this is

`patches/series/` is the complete, ordered, replayable definition of everything ArifiLabs
adds to upstream llama.cpp. It is generated from git, never hand-maintained, and an
integrity check refuses to pass if it has drifted from git by so much as a byte.

- Base: `ggml-org/llama.cpp` tag `b11178`, `f9af9be219ca647a59106f6201bf0d85fab00224`
- Patches: **803**, all non-merge, applied in filename order.

## Applying the series

```bash
git checkout -b my-rebuild f9af9be21
git am patches/series/*.patch
```

The result is byte-identical to `master` everywhere outside `patches/series` itself:
same file contents, same 803 commit messages, same provenance trailers. Verified, not
asserted - `series check` replays the series with `git am` and diffs the result against
`master` on every run.

## Why the series excludes itself

`patches/series` is generated INTO the tree it describes, so it is excluded from its own
generation with `':(exclude)patches/series'`. This is not tidiness, it is what makes the
artifact converge. Include it, and regenerating emits a patch whose content is the patch
files; committing that changes the series; regenerating emits a patch for THAT commit,
forever. The commit that publishes the series can never be inside the series it
published - a self-containing patch is impossible, since `format-patch` writes the
commit's own sha into its `From` line. With the exclusion, a commit touching only the
series generates nothing, the next regeneration produces zero diff, and `series check`
can actually pass.

The manifest deliberately does NOT enumerate the commits that rule drops, and must never
start to. Anything generated from an excluded commit puts the loop straight back: every
regeneration commit would add its own line, requiring another regeneration, forever. This
file is a function of the commits it INCLUDES, and of nothing else. List the dropped ones
on demand instead:

```bash
git log --oneline f9af9be21..master -- patches/series
```

## Load-bearing generation flags

Never drop any of these:

- `--binary` - patch `0007` vendors 19 binary files (PowerInfer's bundled cli11/fmt
  fixtures and images). Without `--binary` `git format-patch` emits *"Binary files
  differ"* and `git am` cannot apply the series at all.
- `--no-signature` - otherwise every patch is terminated with the local git version
  string, and the integrity check starts failing the day git is upgraded.
- `':(exclude)patches/series'` - see above; without it there is no fixpoint.

`.gitattributes` pins `patches/**` to `-text`, so these files are byte-identical in
every clone. Without it, `core.autocrlf=true` (the Git-for-Windows default) checks them
out as CRLF while `format-patch` writes LF, and the byte-compare cannot pass at all.

## Flat numbering, grouped in the table

Patches are flat and globally numbered because **apply order is the authoritative**
**property** - the groups interleave (an ArifiLabs docs commit sits between the
PowerInfer series and the thecodacus series), so per-group subdirectories would put
the order at the mercy of shell glob expansion. The grouping is metadata, and lives
in the table below and in `SERIES`.

`patches/banked-source/` is **not part of this series.** Those are other projects' own
format-patches, banked as *source material* for a port. Do not `git am` them. They were
moved out of `patches/series` by `5d1486012` precisely because everything inside the
generated directory is destroyed and rewritten on the next `series regen`.

## The series

| # | Patch | Group | Commit | Taken-from | Gate / toggle | Effect |
|---|---|---|---|---|---|---|
| 1 | `0001-chore-establish-ArifiLabs-fork-identity-and-retained.patch` | arifi-fork-base | `6ffe2b530` | ggml-org/llama.cpp@571d0d540df04f25298d0e159e520d9fc62ed121, Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4, charlie12345/ROCmFPX@a6a93765f7ce9779c13f9881164a65f7a9f31198 | - | chore: establish ArifiLabs fork identity and retained notices |
| 2 | `0002-docs-add-options-registry-and-unified-A-F-catalog.patch` | arifi-fork-base | `5ef30b117` | ArifiLabs local-inference catalog and trial evidence | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PROF`, `LANE110_PREFETCH_CAP` | docs: add options registry and unified A-F catalog |
| 3 | `0003-ci-add-Windows-Vulkan-and-bench-smoke-placeholders.patch` | arifi-fork-base | `7710a35a8` | - | - | ci: add Windows Vulkan and bench-smoke placeholders |
| 4 | `0004-docs-require-loud-fail-upstream-currency-procedure.patch` | arifi-fork-base | `b68cb5e8f` | ggml-org/llama.cpp@571d0d540df04f25298d0e159e520d9fc62ed121, charlie12345/ROCmFPX@a6a93765f7ce9779c13f9881164a65f7a9f31198, PrismML-Eng/llama.cpp@ternary-Q2_0_g128-lineage, Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | docs: require loud-fail upstream currency procedure |
| 5 | `0005-powerinfer-add-MoE-pipeline-fused-sparse-ggml-ops-se.patch` | powerinfer-streaming | `0a2627c47` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: add MoE pipeline + fused-sparse ggml ops [series 0001] |
| 6 | `0006-powerinfer-CPU-kernels-dispatch-for-streamed-MoE-ops.patch` | powerinfer-streaming | `4373d5587` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: CPU kernels + dispatch for streamed-MoE ops [series 0002] |
| 7 | `0007-powerinfer-vendor-streaming-library-incl.-lane-110-W.patch` | powerinfer-streaming | `d41faca3e` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED` | powerinfer: vendor streaming library incl. lane-110 Windows port [series 0003] |
| 8 | `0008-build-wire-powerinfer-lib-AVX-spill-cure-into-CMake-.patch` | powerinfer-streaming | `823265f1d` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | build: wire powerinfer lib + AVX-spill cure into CMake [series 0004] |
| 9 | `0009-powerinfer-expert-bundle-loader-GENERATE_EXPERT_BUND.patch` | powerinfer-streaming | `9f4d5285c` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | powerinfer: expert-bundle loader + GENERATE_EXPERT_BUNDLE [series 0005] |
| 10 | `0010-powerinfer-generic-MoE-streaming-hook-SiLU-op-type-s.patch` | powerinfer-streaming | `e879872d7` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP` | powerinfer: generic MoE streaming hook, SiLU op-type, standalone expert prefetch [series 0006+0009] |
| 11 | `0011-powerinfer-per-ubatch-pipeline-init-guarded-reuse-PR.patch` | powerinfer-streaming | `fddd31e2a` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `LANE110_PROF` | powerinfer: per-ubatch pipeline init, guarded reuse, PROF field [series 0007+0010] |
| 12 | `0012-powerinfer-repack-carve-out-for-streamed-tensors-ser.patch` | powerinfer-streaming | `3344b1a75` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | powerinfer: repack carve-out for streamed tensors [series 0008a] |
| 13 | `0013-powerinfer-down-projection-transpose-fix-series-0008.patch` | powerinfer-streaming | `581d3e97f` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: down-projection transpose fix [series 0008c] |
| 14 | `0014-powerinfer-GPU-safe-staging-CPU-zero-copy-LANE110_PR.patch` | powerinfer-streaming | `976703cc2` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `LANE110_PROF` | powerinfer: GPU-safe staging, CPU zero-copy, LANE110_PROF [series 0011] |
| 15 | `0015-lane-110-document-UI-ON-as-local-default-build-varia.patch` | arifi-fork-base | `f7685300e` | - | `LLAMA_USE_PREBUILT_UI` | lane-110: document UI-ON as local default build variant (static-runtime cure for ki-webui-0xC0000139) |
| 16 | `0016-community-llama-pin-mmap-backed-CPU-weights-for-fast.patch` | community-prefetch | `02149696b` | thecodacus/llama.cpp@20f5994bfeb91d24da328077c4b6095998cc9888 | - | community: llama : pin mmap-backed CPU weights for faster H2D uploads |
| 17 | `0017-community-ggml-overlap-offloaded-expert-weight-uploa.patch` | community-prefetch | `f332b35b9` | thecodacus/llama.cpp@1163cb34939fe4a9cb07aec034c5954144497ae9 | - | community: ggml : overlap offloaded expert weight uploads with compute |
| 18 | `0018-community-ggml-size-prefetch-slots-per-layer-and-fix.patch` | community-prefetch | `154819d7f` | thecodacus/llama.cpp@5f83fbbe7c668c59912a1fe09e86a0ef580406c4 | - | community: ggml : size prefetch slots per layer and fix fallback use-after-free |
| 19 | `0019-tools-gguf-add-fail-closed-g128-retagger.patch` | ternary-g128 | `3e8caafa1` | - | - | tools(gguf): add fail-closed g128 retagger |
| 20 | `0020-tools-gguf-retagger-v2-structure-only-parser-no-tens.patch` | ternary-g128 | `ef5625735` | - | - | tools(gguf): retagger v2 — structure-only parser, no tensor materialization |
| 21 | `0021-g128-engine-port-applied-BUILD-BLOCKED-on-copy_to_qu.patch` | ternary-g128 | `56763ff4e` | PrismML-Eng/llama.cpp@984bf9723, PrismML-Eng/llama.cpp@34dc5812c | - | g128 engine port applied — BUILD-BLOCKED on copy_to_quant.comp shader (Sol's fix owed) |
| 22 | `0022-vulkan-fix-f16-promotion-in-q2_0_g128-copy_to_quant.patch` | ternary-g128 | `06980aa5f` | - | - | vulkan: fix f16 promotion in q2_0_g128 copy_to_quant |
| 23 | `0023-vulkan-g128-shader-gen-parity-MMQ-helper-completion.patch` | ternary-g128 | `d54b61152` | - | - | vulkan: g128 shader-gen parity + MMQ helper completion |
| 24 | `0024-vulkan-g128-DMMV-repack4-byte-oriented-rewrite.patch` | ternary-g128 | `30933f2ed` | - | - | vulkan: g128 DMMV repack4 byte-oriented rewrite |
| 25 | `0025-loader-g128-include-get_next_tensor-ctx-arg.patch` | ternary-g128 | `42fe2dd9a` | - | - | loader: g128 include + get_next_tensor ctx arg |
| 26 | `0026-loader-use-public-ggml_blck_size-for-g128-drop-inter.patch` | ternary-g128 | `12fb2d47e` | - | - | loader: use public ggml_blck_size for g128, drop internal include |
| 27 | `0027-tools-gguf-retag-v3-verified-type-id-rewrite-42-43.patch` | ternary-g128 | `6b5ca0005` | - | - | tools(gguf): retag v3 — verified type-id rewrite 42->43 |
| 28 | `0028-loader-accept-native-Q2_0_G128-tensors.patch` | ternary-g128 | `6b579f14a` | - | - | loader: accept native Q2_0_G128 tensors |
| 29 | `0029-ggml-cpu-dispatch-Q2_0_G128-at-the-7-Q2_0-sites.patch` | ternary-g128 | `b4694b396` | - | - | ggml-cpu: dispatch Q2_0_G128 at the 7 Q2_0 sites |
| 30 | `0030-port-f16-recurrent-S-state-env-gated-GGML_RECURRENT_.patch` | prismml-recurrent | `fe7c0694e` | PrismML-Eng/llama.cpp@3e0855571 | `GGML_RECURRENT_STATE_F16` | port: f16 recurrent S-state (env-gated GGML_RECURRENT_STATE_F16) |
| 31 | `0031-port-M-RoPE-embedded-batch-position-guard.patch` | rocmfpx-mrope | `eca4b73c2` | charlie12345/ROCmFPX@rocmfpx-a6a9376 | - | port: M-RoPE embedded-batch position guard |
| 32 | `0032-docs-phase-3a-OPTIONS-REGISTRY-rows-M-RoPE-f16-recur.patch` | arifi-fork-base | `1c1cb5cc9` | - | `GGML_RECURRENT_STATE_F16` | docs: phase-3a OPTIONS-REGISTRY rows (M-RoPE, f16-recurrent, DSpark-iGPU) |
| 33 | `0033-port-Windows-IOCP-async-expert-bundle-reads-M2b.patch` | iocp-async | `c7fc33561` | - | - | port: Windows IOCP async expert-bundle reads (M2b) |
| 34 | `0034-feat-POWERINFER_IOCP-runtime-toggle-for-the-Windows-.patch` | iocp-async | `2467e263a` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | feat: POWERINFER_IOCP runtime toggle for the Windows expert-read path |
| 35 | `0035-docs-OPTIONS-REGISTRY-POWERINFER_IOCP-measured-row-s.patch` | iocp-async | `401194c61` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | docs: OPTIONS-REGISTRY POWERINFER_IOCP measured row (same-binary A/B) |
| 36 | `0036-docs-correct-POWERINFER_IOCP-row-A-B-was-noise-direc.patch` | iocp-async | `3262ec3d8` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | docs: correct POWERINFER_IOCP row - A/B was noise, direction UNRESOLVED |
| 37 | `0037-patches-generate-the-full-auditable-38-patch-series-.patch` | arifi-fork-base | `7e7ec5b6a` | ArifiLabs lane-110C fork-maintainability slot | - | patches: generate the full auditable 38-patch series from git |
| 38 | `0038-tools-add-arifi-sync-the-currency-series-and-upstrea.patch` | arifi-fork-base | `6860f6822` | ArifiLabs lane-110C fork-maintainability slot | `LLAMA_USE_PREBUILT_UI`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `ARIFI_TOOL_NVFP4_REMAP` | tools: add arifi-sync - the currency, series and upstream-bump driver |
| 39 | `0039-docs-README-section-a-GitHub-visitor-can-read-cold.patch` | arifi-fork-base | `d999f9e59` | ArifiLabs lane-110C fork-maintainability slot | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP` | docs: README section a GitHub visitor can read cold |
| 40 | `0040-patches-move-banked-PrismML-source-patches-out-of-th.patch` | arifi-fork-base | `cd1866098` | PrismML-Eng/llama.cpp@ternary-Q2_0_g128-lineage | - | patches: move banked PrismML source patches out of the generated series directory |
| 41 | `0041-patches-make-the-series-generator-implement-its-own-.patch` | arifi-fork-base | `3fcab8134` | - | - | patches: make the series generator implement its own convergence rule |
| 42 | `0042-patches-stop-the-manifest-deriving-anything-from-exc.patch` | arifi-fork-base | `380c0ba9e` | - | - | patches: stop the manifest deriving anything from excluded commits |
| 43 | `0043-port-Q1_0-repack-scaffolding-Arm-NEON-DP-GEMV-GEMM-k.patch` | arifi-fork-base | `8da948373` | PrismML-Eng/llama.cpp@720e06b16 | - | port: Q1_0 repack scaffolding + Arm NEON/DP GEMV-GEMM kernels |
| 44 | `0044-port-native-x86-Q2_0-vec_dot-AVX512-VNNI-AVX-VNNI-dr.patch` | arifi-fork-base | `9196f0b03` | PrismML-Eng/llama.cpp@4d88cd4eb, PrismML-Eng/llama.cpp@79697f23a | - | port: native x86 Q2_0 vec_dot (AVX512-VNNI / AVX-VNNI), drop scalar fallback alias |
| 45 | `0045-port-x86-AVX512-VNNI-repack-GEMV-GEMM-for-Q1_0-and-Q.patch` | arifi-fork-base | `d65eb11c4` | PrismML-Eng/llama.cpp@9fcaed763 | - | port: x86 AVX512-VNNI repack GEMV/GEMM for Q1_0 and Q2_0 |
| 46 | `0046-feat-GGML_ARIFI_VNNI_REPACK-runtime-toggle-explicit-.patch` | ternary-g128 | `590ba38cd` | - | `GGML_ARIFI_VNNI_REPACK` | feat: GGML_ARIFI_VNNI_REPACK runtime toggle + explicit g64/g128 tripwire |
| 47 | `0047-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_VNNI_REPACK.patch` | arifi-fork-base | `a292bda9a` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY row for GGML_ARIFI_VNNI_REPACK - UNMEASURED, artifact-blocked |
| 48 | `0048-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-CORRECT.patch` | arifi-fork-base | `50706e354` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - CORRECTNESS FAIL, GEMM returns NaN |
| 49 | `0049-fix-ggml-cpu-parameterize-Q2_0-GEMM-activation-strid.patch` | arifi-fork-base | `5e0e1a101` | PrismML-Eng/llama.cpp (Q2_0 4x8 VNNI GEMM design) | - | fix(ggml-cpu): parameterize Q2_0 GEMM activation stride by QK2_0/QK8_0 - g64 NaN |
| 50 | `0050-fix-ggml-cpu-same-g64-stride-defect-in-the-ported-x8.patch` | arifi-fork-base | `723a68af5` | - | - | fix(ggml-cpu): same g64 stride defect in the ported x86 Q2_0 vec_dot |
| 51 | `0051-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-NaN-FIX.patch` | arifi-fork-base | `06ccc5122` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - NaN FIXED, bit-identity NOT established |
| 52 | `0052-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-bit-div.patch` | arifi-fork-base | `51a99e0e1` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - bit-divergence ADJUDICATED, performance MEASURED |
| 53 | `0053-docs-correct-the-VNNI-row-s-base-commit-labelling-f7.patch` | arifi-fork-base | `dfa924949` | - | `GGML_ARIFI_VNNI_REPACK` | docs: correct the VNNI row's base-commit labelling - f7c4d9d2c is NOT master |
| 54 | `0054-docs-fix-two-stray-table-breaking-pipes-in-the-VNNI-.patch` | arifi-fork-base | `3eb7da7ef` | - | `GGML_ARIFI_VNNI_REPACK` | docs: fix two stray table-breaking pipes in the VNNI registry row |
| 55 | `0055-docs-OPTIONS-REGISTRY-VNNI-row-topology-cell-now-ref.patch` | arifi-fork-base | `39e0a8b4c` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY VNNI row - topology cell now reflects the linear-master rebase |
| 56 | `0056-ggml-cpu-GGML_ARIFI_VNNI_REPACK-defaults-to-OFF-dive.patch` | ternary-g128 | `518d06764` | - | `GGML_ARIFI_VNNI_REPACK` | ggml-cpu: GGML_ARIFI_VNNI_REPACK defaults to OFF (diverging from upstream, on measurement) |
| 57 | `0057-docs-fork-wide-GGUF-type-ID-allocation-contract-keys.patch` | arifi-fork-base | `830879e04` | - | `ARIFI_TOOL_NVFP4_REMAP` | docs: fork-wide GGUF type-ID allocation contract (keystone, unblocks 4 slots) |
| 58 | `0058-ggml-reserve-the-ArifiLabs-type-ID-blocks-at-the-enu.patch` | arifi-fork-base | `fc46a74e3` | - | - | ggml: reserve the ArifiLabs type-ID blocks at the enum itself |
| 59 | `0059-ggml-ROCmFP4-ROCmFPX-weight-formats-behind-GGML_ARIF.patch` | arifi-fork-base | `672273b1e` | charlie12345/ROCmFPX@3edc3d31e | - | ggml: ROCmFP4/ROCmFPX weight formats behind GGML_ARIFI_ROCMFPX_FORMATS |
| 60 | `0060-llama-ROCmFPX-file-types-quantizer-wiring-and-the-gg.patch` | arifi-fork-base | `f9224cefb` | charlie12345/ROCmFPX@3edc3d31e | - | llama: ROCmFPX file types, quantizer wiring and the gguf-py registry |
| 61 | `0061-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_ROCMFPX_FOR.patch` | arifi-fork-base | `58eaccdf8` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY row for GGML_ARIFI_ROCMFPX_FORMATS |
| 62 | `0062-tools-record-GGML_ARIFI_ROCMFPX_FORMATS-in-the-build.patch` | arifi-fork-base | `519204b59` | - | - | tools: record GGML_ARIFI_ROCMFPX_FORMATS in the build-recipe snapshot |
| 63 | `0063-docs-make-the-fork-buildable-by-someone-who-did-not-.patch` | arifi-fork-base | `2b0e775aa` | - | `LLAMA_USE_PREBUILT_UI`, `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `GGML_RECURRENT_STATE_F16` | docs: make the fork buildable by someone who did not build it |
| 64 | `0064-feat-arifi-profile-and-a-CPU-only-VNNI-repack-startu.patch` | arifi-fork-base | `53c28a68c` | - | `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP`, `MAX_N_CACHED`, `GGML_RECURRENT_STATE_F16` | feat: --arifi-profile and a CPU-only VNNI-repack startup advisory |
| 65 | `0065-sync-bump-base-to-upstream-b10173-e9fa0781f-series-r.patch` | ternary-g128 | `249d5b5a4` | upstream b10173 e9fa0781f | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PROF`, `LANE110_PREFETCH_CAP`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | sync: bump base to upstream b10173 (e9fa0781f) — series replayed (1 hand-resolved conflict: loader load-mode refactor e6dd0e29a), judge harness wired + exercised (F-085 llama-server, g128 ternary 27B PASS), pins updated, series regenerated |
| 66 | `0066-sync-currency-GREEN-TurboQuant-checkouts-pinned-at-t.patch` | arifi-fork-base | `3e8e15fa5` | - | - | sync: currency GREEN — TurboQuant checkouts pinned at their lane-110C-reviewed shas (c26cbdffc, ba52ad107), rocmfpx pin advanced to db6844d9b after 6-commit review (HIP pinned-staging noted for the HIP-deltas slot) |
| 67 | `0067-fix-ggml-cpu-x86-don-t-pass-__m256i-by-value-across-.patch` | arifi-fork-base | `055efbf0d` | - | - | fix(ggml-cpu/x86): don't pass __m256i by value across the repack GEMV/GEMM ABI boundary (Windows 0xC0000005) |
| 68 | `0068-ggml-TurboQuant-TQ3_1S-TQ4_1S-weight-formats-behind-.patch` | arifi-fork-base | `10adb5b3b` | - | - | ggml: TurboQuant TQ3_1S/TQ4_1S weight formats behind GGML_ARIFI_TURBO_WEIGHT_QUANTS |
| 69 | `0069-docs-CHANGELOG-and-FINDINGS-the-public-trail-a-stran.patch` | arifi-fork-base | `fea64f9b9` | - | `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP` | docs: CHANGELOG and FINDINGS - the public trail a stranger can actually read |
| 70 | `0070-docs-USAGE-a-task-oriented-guide-for-humans-not-an-o.patch` | arifi-fork-base | `7d6d4db01` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `POWERINFER_IOCP` | docs: USAGE - a task-oriented guide for humans, not an option reference |
| 71 | `0071-ggml-cpu-x86-use-the-row-stride-parameter-in-repack-.patch` | arifi-fork-base | `17ac2969b` | - | - | ggml-cpu/x86: use the row-stride parameter in repack GEMV stores instead of the row count |
| 72 | `0072-ggml-cpu-fuse-the-TurboQuant-dot-product-transform-t.patch` | ternary-g128 | `efb03325e` | - | - | ggml-cpu: fuse the TurboQuant dot product - transform the activation, not the weights |
| 73 | `0073-docs-FINDINGS-F-09-the-repack-GPU-trade-off-is-a-com.patch` | arifi-fork-base | `ee199085a` | - | - | docs(FINDINGS): F-09 - the repack/GPU trade-off is a compute-placement choice, not a bug |
| 74 | `0074-docs-neutralise-internal-governance-vocabulary-expla.patch` | arifi-fork-base | `1525f60a0` | - | - | docs: neutralise internal governance vocabulary; explain the work-unit ids instead |
| 75 | `0075-docs-FINDINGS-record-why-the-repack-auto-enable-was-.patch` | arifi-fork-base | `a6daca42b` | - | - | docs(FINDINGS): record why the repack auto-enable was NOT shipped |
| 76 | `0076-provenance-catch-the-silent-trailer-block-break-inst.patch` | arifi-fork-base | `8024f23d7` | - | - | provenance: catch the silent trailer-block break instead of only reporting it |
| 77 | `0077-gitattributes-pin-.githooks-to-LF-so-the-hook-runs-o.patch` | arifi-fork-base | `16ac24b4e` | - | - | gitattributes: pin .githooks to LF so the hook runs on Linux and macOS |
| 78 | `0078-githooks-force-a-normalized-blob-for-the-commit-msg-.patch` | arifi-fork-base | `f887a3fd5` | - | - | githooks: force a normalized blob for the commit-msg hook |
| 79 | `0079-docs-FINDINGS-F-09-exit-1-is-the-direction-dual-resi.patch` | arifi-fork-base | `43ea0678b` | - | - | docs(FINDINGS): F-09 exit 1 is the direction - dual residency, not a forced choice |
| 80 | `0080-repack-dual-residency-GGML_ARIFI_VNNI_REPACK-2-keeps.patch` | arifi-fork-base | `d2574a071` | - | `GGML_ARIFI_VNNI_REPACK` | repack: dual residency - GGML_ARIFI_VNNI_REPACK=2 keeps prefill on the GPU AND repacks decode |
| 81 | `0081-repack-dual-residency-measured-both-wins-held-fix-th.patch` | arifi-fork-base | `169bbd309` | - | `GGML_ARIFI_VNNI_REPACK` | repack: dual residency measured - both wins held; fix the hot-path cost that ate the first one |
| 82 | `0082-repack-fix-the-shadow-tensor-COUNT-in-the-accounting.patch` | arifi-fork-base | `7afc0e8a1` | - | `GGML_ARIFI_VNNI_REPACK` | repack: fix the shadow tensor COUNT in the accounting line, and strengthen the claim it supports |
| 83 | `0083-repack-CPU-kernels-for-the-128-group-ternary-format.patch` | arifi-fork-base | `524a1bbf3` | - | `GGML_ARIFI_VNNI_REPACK` | repack: CPU kernels for the 128-group ternary format |
| 84 | `0084-tests-direct-equivalence-test-for-the-ternary-repack.patch` | arifi-fork-base | `44aa767aa` | - | - | tests: direct equivalence test for the ternary repack kernels |
| 85 | `0085-docs-the-g128-repack-SPEED-numbers-measured.patch` | arifi-fork-base | `c86a04471` | - | `GGML_ARIFI_VNNI_REPACK` | docs: the g128 repack SPEED numbers, measured |
| 86 | `0086-docs-frame-the-g128-repack-numbers-as-a-CPU-tier-res.patch` | arifi-fork-base | `dbd440ef8` | - | - | docs: frame the g128 repack numbers as a CPU-tier result |
| 87 | `0087-docs-publish-the-power-plan-finding-as-F-12-and-bind.patch` | arifi-fork-base | `603364366` | - | - | docs: publish the power-plan finding as F-12, and bind the bench protocol to it |
| 88 | `0088-F-06-measure-the-GPU-tier-on-one-binary-and-count-th.patch` | arifi-fork-base | `0816b04dd` | - | - | F-06: measure the GPU tier on one binary, and count the placement the prompt claim rested on |
| 89 | `0089-registry-the-dual-residency-prompt-mechanism-is-meas.patch` | arifi-fork-base | `71b37680b` | - | `GGML_ARIFI_VNNI_REPACK` | registry: the dual-residency prompt mechanism is measured now, not ASSUMED |
| 90 | `0090-powerinfer-MAX_N_CACHED-admitted-two-values-that-bre.patch` | powerinfer-streaming | `0b0926702` | - | `MAX_N_CACHED` | powerinfer: MAX_N_CACHED admitted two values that break the eviction invariant |
| 91 | `0091-rows-per-workgroup-on-g128-a-null-and-F-10-confirmed.patch` | ternary-g128 | `47be182e1` | - | - | rows-per-workgroup on g128: a null, and F-10 confirmed on a second file |
| 92 | `0092-registry-our-DSpark-verdict-was-never-ours-the-draft.patch` | arifi-fork-base | `8164d2b48` | - | - | registry: our DSpark verdict was never ours -- the drafter cannot load on this fork |
| 93 | `0093-speculative-DFlash-DSpark-drafters-have-no-vocab-tak.patch` | arifi-fork-base | `f19a3d829` | - | - | speculative: DFlash/DSpark drafters have no vocab -- take the mask token from metadata |
| 94 | `0094-expert-cache-stride-the-bundle-by-the-PADDED-matrix-.patch` | arifi-fork-base | `0c0cd9a57` | - | - | expert cache: stride the bundle by the PADDED matrix size -- unblocks our own brain |
| 95 | `0095-powerinfer-POWERINFER_NO_BUFFERING-1-the-Windows-cou.patch` | powerinfer-streaming | `badc4da3f` | - | `POWERINFER_IOCP` | powerinfer: POWERINFER_NO_BUFFERING=1 -- the Windows counterpart of the O_DIRECT Linux already uses |
| 96 | `0096-expert-cache-POWERINFER_EXPERT_HEATMAP-measure-acces.patch` | arifi-fork-base | `3b6bec7f4` | - | - | expert cache: POWERINFER_EXPERT_HEATMAP -- measure access skew before building a pin policy |
| 97 | `0097-ours-1163cb34939fe4a9cb07aec034c5954144497ae9.patch.patch` | arifi-fork-base | `ab0332a9e` | - | - | ours/1163cb34939fe4a9cb07aec034c5954144497ae9.patch |
| 98 | `0098-1-core-types-0001-WIP-add-TurboQuant-KV-cache-types-.patch` | arifi-fork-base | `79edc9c98` | - | - | 1-core-types/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 99 | `0099-1-core-types-0002-ggml-port-TurboQuant-core-quant-ty.patch` | arifi-fork-base | `7d42a5921` | - | - | 1-core-types/0002-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 100 | `0100-2-cpu-reference-0001-ggml-port-TurboQuant-core-quant.patch` | arifi-fork-base | `5cc1488b5` | - | - | 2-cpu-reference/0001-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 101 | `0101-2-cpu-reference-0007-ggml-cpu-declare-turbo3_cpu_wht.patch` | arifi-fork-base | `fc9802e79` | - | - | 2-cpu-reference/0007-ggml-cpu-declare-turbo3_cpu_wht_group_size-extern-no.patch |
| 102 | `0102-2-cpu-reference-0008-merge-fix-post-merge-build-and-.patch` | arifi-fork-base | `36f5faa97` | - | - | 2-cpu-reference/0008-merge-fix-post-merge-build-and-crash-issues.patch |
| 103 | `0103-3-vulkan-0001-vulkan-metal-hip-add-TurboQuant-kernel.patch` | arifi-fork-base | `58179605c` | - | - | 3-vulkan/0001-vulkan-metal-hip-add-TurboQuant-kernel-support.patch |
| 104 | `0104-3-vulkan-0002-vulkan-port-fork-241-wave64-ballot-fix.patch` | arifi-fork-base | `e238783e1` | - | - | 3-vulkan/0002-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch |
| 105 | `0105-3-vulkan-0003-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `780e81fbb` | - | - | 3-vulkan/0003-fix-close-complete-audit-findings-7-port-regressions.patch |
| 106 | `0106-3-vulkan-0004-vulkan-reconstruct-supports_op-after-r.patch` | arifi-fork-base | `24a1643b4` | - | - | 3-vulkan/0004-vulkan-reconstruct-supports_op-after-rebase-merge-da.patch |
| 107 | `0107-3-vulkan-0006-vulkan-restore-TQ4_1S-weight-type-wiri.patch` | arifi-fork-base | `aba4835a5` | - | - | 3-vulkan/0006-vulkan-restore-TQ4_1S-weight-type-wiring-lost-in-the.patch |
| 108 | `0108-3-vulkan-0007-vulkan-add-the-TQ3_1S-weight-type.patc.patch` | arifi-fork-base | `9307230fd` | - | - | 3-vulkan/0007-vulkan-add-the-TQ3_1S-weight-type.patch |
| 109 | `0109-3-vulkan-0008-vulkan-reject-MUL_MAT_ID-for-the-Turbo.patch` | arifi-fork-base | `24caccd26` | - | - | 3-vulkan/0008-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch |
| 110 | `0110-3-vulkan-0013-vulkan-TQ-rotated-matmul-shader-ground.patch` | arifi-fork-base | `7e7f885f8` | - | - | 3-vulkan/0013-vulkan-TQ-rotated-matmul-shader-groundwork-A-side-lo.patch |
| 111 | `0111-3-vulkan-0015-vulkan-create-the-TQ-activation-rotate.patch` | arifi-fork-base | `e310fe9ee` | - | - | 3-vulkan/0015-vulkan-create-the-TQ-activation-rotate-pipeline.patch |
| 112 | `0112-3-vulkan-0017-vulkan-rotate-the-staged-activation-co.patch` | arifi-fork-base | `1af80c3e2` | - | - | 3-vulkan/0017-vulkan-rotate-the-staged-activation-copy-in-place-dr.patch |
| 113 | `0113-3-vulkan-0018-vulkan-register-the-TQ-rotated-mul_mm_.patch` | arifi-fork-base | `87f6b4862` | - | - | 3-vulkan/0018-vulkan-register-the-TQ-rotated-mul_mm_id-pipelines.patch |
| 114 | `0114-3-vulkan-0021-vulkan-fix-SET_ROWS-block-decompositio.patch` | arifi-fork-base | `5633b6c81` | - | - | 3-vulkan/0021-vulkan-fix-SET_ROWS-block-decomposition-for-turbo-we.patch |
| 115 | `0115-4-other-backends-0006-laguna-MoE-down-proj-f16-overf.patch` | arifi-fork-base | `15305fe1d` | - | - | 4-other-backends/0006-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch |
| 116 | `0116-4-other-backends-0007-cuda-first-class-MoE-path-plai.patch` | arifi-fork-base | `e3887560f` | - | - | 4-other-backends/0007-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch |
| 117 | `0117-4-other-backends-0008-hip-add-mixed-f16-bf16-q8_0-fa.patch` | arifi-fork-base | `84f20e818` | - | - | 4-other-backends/0008-hip-add-mixed-f16-bf16-q8_0-fattn-vec-instances-to-t.patch |
| 118 | `0118-4-other-backends-0009-cuda-revert-GQA-ratio-dispatch.patch` | arifi-fork-base | `a67726d61` | - | - | 4-other-backends/0009-cuda-revert-GQA-ratio-dispatch-to-comparison-form-fi.patch |
| 119 | `0119-4-other-backends-0010-cuda-restore-Volta-GQA-modulo-.patch` | arifi-fork-base | `1c4ddb0b4` | - | - | 4-other-backends/0010-cuda-restore-Volta-GQA-modulo-dispatch-over-revert-i.patch |
| 120 | `0120-4-other-backends-0015-cuda-rework-MoE-expert-cache-e.patch` | arifi-fork-base | `efaa788c6` | - | - | 4-other-backends/0015-cuda-rework-MoE-expert-cache-execution.patch |
| 121 | `0121-4-other-backends-0016-cuda-fix-MoE-cache-capacity-ac.patch` | arifi-fork-base | `a7468d449` | - | - | 4-other-backends/0016-cuda-fix-MoE-cache-capacity-accounting.patch |
| 122 | `0122-4-other-backends-0017-cuda-fix-MoE-cache-scratch-bud.patch` | arifi-fork-base | `2a49603c9` | - | - | 4-other-backends/0017-cuda-fix-MoE-cache-scratch-budget-replacement.patch |
| 123 | `0123-4-other-backends-0018-cuda-bound-MoE-MMV-tail-row-re.patch` | arifi-fork-base | `26a5befeb` | - | - | 4-other-backends/0018-cuda-bound-MoE-MMV-tail-row-reads.patch |
| 124 | `0124-4-other-backends-0019-cuda-tune-MoE-cache-CPU-overla.patch` | arifi-fork-base | `cedcecc08` | - | - | 4-other-backends/0019-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 125 | `0125-4-other-backends-0020-cuda-adapt-MoE-cache-admission.patch` | arifi-fork-base | `a7342bb64` | - | - | 4-other-backends/0020-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 126 | `0126-4-other-backends-0021-cuda-align-MoE-cache-pool-allo.patch` | arifi-fork-base | `a542082d7` | - | - | 4-other-backends/0021-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 127 | `0127-4-other-backends-0022-cuda-prefer-generic-MMV-for-co.patch` | arifi-fork-base | `33cabbe6b` | - | - | 4-other-backends/0022-cuda-prefer-generic-MMV-for-compatible-MoE-cache-nod.patch |
| 128 | `0128-4-other-backends-0023-cuda-parallelize-MoE-cache-fil.patch` | arifi-fork-base | `0c125ebee` | - | - | 4-other-backends/0023-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch |
| 129 | `0129-4-other-backends-0024-cuda-fuse-cached-MoE-SwiGLU-ro.patch` | arifi-fork-base | `5eda84c8b` | - | - | 4-other-backends/0024-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 130 | `0130-4-other-backends-0025-cuda-aggregate-small-MoE-tenso.patch` | arifi-fork-base | `3ec648746` | - | - | 4-other-backends/0025-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 131 | `0131-4-other-backends-0026-cuda-bound-automatic-MoE-cache.patch` | arifi-fork-base | `a60d46121` | - | - | 4-other-backends/0026-cuda-bound-automatic-MoE-cache-admission.patch |
| 132 | `0132-4-other-backends-0027-cuda-reject-undersized-automat.patch` | arifi-fork-base | `96232ed24` | - | - | 4-other-backends/0027-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 133 | `0133-4-other-backends-0028-cuda-enable-multi-token-MoE-ca.patch` | arifi-fork-base | `d7b5ef5ef` | - | - | 4-other-backends/0028-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 134 | `0134-4-other-backends-0029-common-surface-MoE-cache-activ.patch` | arifi-fork-base | `2dfa46b0f` | - | - | 4-other-backends/0029-common-surface-MoE-cache-activation.patch |
| 135 | `0135-4-other-backends-0030-cuda-accelerate-complete-MoE-c.patch` | arifi-fork-base | `403b70e20` | - | - | 4-other-backends/0030-cuda-accelerate-complete-MoE-cache-pools.patch |
| 136 | `0136-4-other-backends-0031-cuda-report-oversized-MoE-cach.patch` | arifi-fork-base | `ab3ec95d3` | - | - | 4-other-backends/0031-cuda-report-oversized-MoE-cache-nodes.patch |
| 137 | `0137-4-other-backends-0032-cuda-clarify-MoE-cache-session.patch` | arifi-fork-base | `39fdfb689` | - | - | 4-other-backends/0032-cuda-clarify-MoE-cache-session-diagnostics.patch |
| 138 | `0138-4-other-backends-0033-cuda-report-MoE-cache-pair-res.patch` | arifi-fork-base | `847a02d4c` | - | - | 4-other-backends/0033-cuda-report-MoE-cache-pair-residency.patch |
| 139 | `0139-4-other-backends-0034-cuda-pack-MoE-cache-dispatch-i.patch` | arifi-fork-base | `aaee3b626` | - | - | 4-other-backends/0034-cuda-pack-MoE-cache-dispatch-inputs.patch |
| 140 | `0140-4-other-backends-0037-metal-port-TQ_NO_ROTATE-escape.patch` | arifi-fork-base | `87660fa66` | - | - | 4-other-backends/0037-metal-port-TQ_NO_ROTATE-escape-hatch-from-stash-opt-.patch |
| 141 | `0141-4-other-backends-0039-moe-cache-enable-HIP-backend-b.patch` | arifi-fork-base | `51a001a57` | - | - | 4-other-backends/0039-moe-cache-enable-HIP-backend-by-removing-no-op-stubs.patch |
| 142 | `0142-4-other-backends-0040-moe-cache-count-dispatch-conte.patch` | arifi-fork-base | `e42036adb` | - | - | 4-other-backends/0040-moe-cache-count-dispatch-contention-bypasses-P1.patch |
| 143 | `0143-4-other-backends-0041-moe-cache-provider-registry-wi.patch` | arifi-fork-base | `eca3f76f5` | - | - | 4-other-backends/0041-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 144 | `0144-4-other-backends-0042-moe-cache-audit-fixes-H1-F1-F2.patch` | arifi-fork-base | `918588bfb` | - | - | 4-other-backends/0042-moe-cache-audit-fixes-H1-F1-F2-A1.patch |
| 145 | `0145-4-other-backends-0043-moe-cache-fix-registry-build-I.patch` | arifi-fork-base | `5e9cf1aa0` | - | - | 4-other-backends/0043-moe-cache-fix-registry-build-I1-follow-up.patch |
| 146 | `0146-4-other-backends-0044-moe-cache-allow-automatic-mode.patch` | arifi-fork-base | `a16ad0072` | - | - | 4-other-backends/0044-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 147 | `0147-4-other-backends-0045-cuda-fix-HIP-MoE-cache-compati.patch` | arifi-fork-base | `b4984db5b` | - | - | 4-other-backends/0045-cuda-fix-HIP-MoE-cache-compatibility-and-q8_1-sums.patch |
| 148 | `0148-4-other-backends-0046-moe-cache-add-logging-at-silen.patch` | arifi-fork-base | `fb5fd21ed` | - | - | 4-other-backends/0046-moe-cache-add-logging-at-silent-failure-points-acros.patch |
| 149 | `0149-4-other-backends-0048-cuda-fit-fix-two-Werror-build-.patch` | arifi-fork-base | `3bd06c183` | - | - | 4-other-backends/0048-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch |
| 150 | `0150-4-other-backends-0049-sycl-drop-the-duplicated-nthre.patch` | arifi-fork-base | `18ab077fc` | - | - | 4-other-backends/0049-sycl-drop-the-duplicated-nthreads-declarations-in-fa.patch |
| 151 | `0151-4-other-backends-0050-fattn-vec-split-the-turbo-K-do.patch` | arifi-fork-base | `7a3339536` | - | - | 4-other-backends/0050-fattn-vec-split-the-turbo-K-dot-at-D-128-to-stop-the.patch |
| 152 | `0152-5-models-server-0001-WIP-add-TurboQuant-KV-cache-typ.patch` | arifi-fork-base | `be9541074` | - | - | 5-models-server/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 153 | `0153-5-models-server-0003-cli-break-interactive-loop-on-s.patch` | arifi-fork-base | `41448a06b` | - | - | 5-models-server/0003-cli-break-interactive-loop-on-stdin-EOF-fixes-hot-sp.patch |
| 154 | `0154-5-models-server-0004-tools-expose-TQ3_1S-TQ4_1S-in-q.patch` | arifi-fork-base | `37929a5d6` | - | - | 5-models-server/0004-tools-expose-TQ3_1S-TQ4_1S-in-quantize-table-and-lla.patch |
| 155 | `0155-5-models-server-0007-llama-support-DeepSeek-V4-tenso.patch` | arifi-fork-base | `0af9f9be5` | - | - | 5-models-server/0007-llama-support-DeepSeek-V4-tensor-split.patch |
| 156 | `0156-5-models-server-0008-llama-mirror-DS4-q_a-kv-down-pr.patch` | arifi-fork-base | `b630bb5a8` | - | - | 5-models-server/0008-llama-mirror-DS4-q_a-kv-down-projections-in-tensor-s.patch |
| 157 | `0157-5-models-server-0009-glm-dsa-guard-lightning-indexer.patch` | arifi-fork-base | `40b4bdb0a` | - | - | 5-models-server/0009-glm-dsa-guard-lightning-indexer-Hadamard-rotation-wh.patch |
| 158 | `0158-5-models-server-0010-laguna-add-arch-tables-llm_arch.patch` | arifi-fork-base | `c2260d79e` | - | - | 5-models-server/0010-laguna-add-arch-tables-llm_arch-KV-keys-tensors-and-.patch |
| 159 | `0159-5-models-server-0011-laguna-add-model-class-hparams-.patch` | arifi-fork-base | `e3f4ad3a4` | - | - | 5-models-server/0011-laguna-add-model-class-hparams-wiring-vocab-pre-toke.patch |
| 160 | `0160-5-models-server-0013-laguna-MoE-down-proj-f16-overfl.patch` | arifi-fork-base | `9870e2ede` | - | - | 5-models-server/0013-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch |
| 161 | `0161-5-models-server-0014-cuda-first-class-MoE-path-plain.patch` | arifi-fork-base | `a0a40906e` | - | - | 5-models-server/0014-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch |
| 162 | `0162-5-models-server-0015-cuda-sum-MoE-expert-outputs-on-.patch` | arifi-fork-base | `d998e675a` | - | - | 5-models-server/0015-cuda-sum-MoE-expert-outputs-on-decode-n_tokens-1.patch |
| 163 | `0163-5-models-server-0016-laguna-deduplicate-definitions-.patch` | arifi-fork-base | `6b6bc88a4` | - | - | 5-models-server/0016-laguna-deduplicate-definitions-vs-the-early-snapshot.patch |
| 164 | `0164-5-models-server-0017-dflash-fix-DSpark-tensor-meta-a.patch` | arifi-fork-base | `3100ea638` | - | - | 5-models-server/0017-dflash-fix-DSpark-tensor-meta-accessor-after-laguna-.patch |
| 165 | `0165-5-models-server-0037-merge-fix-post-merge-build-and-.patch` | arifi-fork-base | `91eb46d0c` | - | - | 5-models-server/0037-merge-fix-post-merge-build-and-crash-issues.patch |
| 166 | `0166-5-models-server-0041-moe-cache-route-fit-probing-and.patch` | arifi-fork-base | `3f162dcd2` | - | - | 5-models-server/0041-moe-cache-route-fit-probing-and-test-session-calls-t.patch |
| 167 | `0167-5-models-server-0048-llama-bench-document-pw-prefetc.patch` | arifi-fork-base | `f12c5c9cf` | - | - | 5-models-server/0048-llama-bench-document-pw-prefetch-weights-in-help-and.patch |
| 168 | `0168-5-models-server-0050-llama-bench-remove-stale-prefet.patch` | arifi-fork-base | `6bc432494` | - | - | 5-models-server/0050-llama-bench-remove-stale-prefetch-weights-documentat.patch |
| 169 | `0169-5-models-server-0051-moe-cache-fix-auto-asymmetric-M.patch` | arifi-fork-base | `f0878e0ba` | - | - | 5-models-server/0051-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch |
| 170 | `0170-6-tests-build-0001-tests-re-add-test-turbo-quant.c-r.patch` | arifi-fork-base | `ea5c2d260` | - | - | 6-tests-build/0001-tests-re-add-test-turbo-quant.c-round-trip-test.patch |
| 171 | `0171-6-tests-build-0002-tests-port-fork-turbo-backend-and.patch` | arifi-fork-base | `fc45de9d2` | - | - | 6-tests-build/0002-tests-port-fork-turbo-backend-and-quantize-fns-test-.patch |
| 172 | `0172-6-tests-build-0003-fix-close-complete-audit-findings.patch` | arifi-fork-base | `cf6398926` | - | - | 6-tests-build/0003-fix-close-complete-audit-findings-7-port-regressions.patch |
| 173 | `0173-6-tests-build-0004-laguna-tool-call-whitespace-toler.patch` | arifi-fork-base | `b54c821d6` | - | - | 6-tests-build/0004-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch |
| 174 | `0174-6-tests-build-0006-ggml-webgpu-add-support-for-f16-r.patch` | arifi-fork-base | `961be3ed3` | - | - | 6-tests-build/0006-ggml-webgpu-add-support-for-f16-repeat-26307.patch |
| 175 | `0175-6-tests-build-0010-tests-restore-DSV4_HC_COMB-eps-sw.patch` | arifi-fork-base | `00e01961e` | - | - | 6-tests-build/0010-tests-restore-DSV4_HC_COMB-eps-sweep-lost-in-the-244.patch |
| 176 | `0176-6-tests-build-0011-tests-cast-float-printf-args-in-t.patch` | arifi-fork-base | `87b2e0bc8` | - | - | 6-tests-build/0011-tests-cast-float-printf-args-in-test-turbo-quant-arm.patch |
| 177 | `0177-6-tests-build-0012-vulkan-add-the-TQ3_1S-weight-type.patch` | arifi-fork-base | `b0eb26776` | - | - | 6-tests-build/0012-vulkan-add-the-TQ3_1S-weight-type.patch |
| 178 | `0178-6-tests-build-0013-vulkan-reject-MUL_MAT_ID-for-the-.patch` | arifi-fork-base | `a6f090b30` | - | - | 6-tests-build/0013-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch |
| 179 | `0179-6-tests-build-0014-ggml-cpu-remove-the-per-call-mall.patch` | arifi-fork-base | `2b40e5de9` | - | - | 6-tests-build/0014-ggml-cpu-remove-the-per-call-mallocs-from-the-TurboQ.patch |
| 180 | `0180-6-tests-build-0015-vulkan-add-mul_mat_vec_id-for-the.patch` | arifi-fork-base | `e73bc52e8` | - | - | 6-tests-build/0015-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch |
| 181 | `0181-6-tests-build-0016-feat-add-Vulkan-TQ4_1S-weight-pip.patch` | arifi-fork-base | `c16526481` | - | - | 6-tests-build/0016-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch |
| 182 | `0182-6-tests-build-0017-tests-port-11-DSv4-shaped-TQ-MUL_.patch` | arifi-fork-base | `4c810c716` | - | - | 6-tests-build/0017-tests-port-11-DSv4-shaped-TQ-MUL_MAT-cases-from-sync.patch |
| 183 | `0183-6-tests-build-0019-cuda-fix-MoE-cache-capacity-accou.patch` | arifi-fork-base | `7731084a4` | - | - | 6-tests-build/0019-cuda-fix-MoE-cache-capacity-accounting.patch |
| 184 | `0184-6-tests-build-0020-common-preserve-implicit-MoE-cach.patch` | arifi-fork-base | `38e89c1f7` | - | - | 6-tests-build/0020-common-preserve-implicit-MoE-cache-provider-settings.patch |
| 185 | `0185-6-tests-build-0021-cuda-tune-MoE-cache-CPU-overlap-a.patch` | arifi-fork-base | `41c45982f` | - | - | 6-tests-build/0021-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 186 | `0186-6-tests-build-0022-cuda-adapt-MoE-cache-admission-to.patch` | arifi-fork-base | `99ca800ee` | - | - | 6-tests-build/0022-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 187 | `0187-6-tests-build-0023-cuda-align-MoE-cache-pool-allocat.patch` | arifi-fork-base | `786ab2d52` | - | - | 6-tests-build/0023-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 188 | `0188-6-tests-build-0024-cuda-parallelize-MoE-cache-fills-.patch` | arifi-fork-base | `db72c4193` | - | - | 6-tests-build/0024-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch |
| 189 | `0189-6-tests-build-0025-cuda-fuse-cached-MoE-SwiGLU-rows..patch` | arifi-fork-base | `17c7c07cd` | - | - | 6-tests-build/0025-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 190 | `0190-6-tests-build-0026-cuda-aggregate-small-MoE-tensors-.patch` | arifi-fork-base | `089040972` | - | - | 6-tests-build/0026-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 191 | `0191-6-tests-build-0027-cuda-bound-automatic-MoE-cache-ad.patch` | arifi-fork-base | `c1794fdc9` | - | - | 6-tests-build/0027-cuda-bound-automatic-MoE-cache-admission.patch |
| 192 | `0192-6-tests-build-0028-cuda-reject-undersized-automatic-.patch` | arifi-fork-base | `abe0313b9` | - | - | 6-tests-build/0028-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 193 | `0193-6-tests-build-0029-cuda-enable-multi-token-MoE-cache.patch` | arifi-fork-base | `e049aa5dc` | - | - | 6-tests-build/0029-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 194 | `0194-6-tests-build-0030-common-surface-MoE-cache-activati.patch` | arifi-fork-base | `3aeabc456` | - | - | 6-tests-build/0030-common-surface-MoE-cache-activation.patch |
| 195 | `0195-6-tests-build-0031-cuda-accelerate-complete-MoE-cach.patch` | arifi-fork-base | `b8b8445b9` | - | - | 6-tests-build/0031-cuda-accelerate-complete-MoE-cache-pools.patch |
| 196 | `0196-6-tests-build-0032-cuda-report-oversized-MoE-cache-n.patch` | arifi-fork-base | `f6f4793eb` | - | - | 6-tests-build/0032-cuda-report-oversized-MoE-cache-nodes.patch |
| 197 | `0197-6-tests-build-0033-tests-initialise-non-contiguous-t.patch` | arifi-fork-base | `77200cb60` | - | - | 6-tests-build/0033-tests-initialise-non-contiguous-tensors-row-by-row.patch |
| 198 | `0198-6-tests-build-0034-tests-cover-turbo-KV-flash-attent.patch` | arifi-fork-base | `54d8576e7` | - | - | 6-tests-build/0034-tests-cover-turbo-KV-flash-attention-at-head-dim-256.patch |
| 199 | `0199-6-tests-build-0037-moe-cache-provider-registry-with-.patch` | arifi-fork-base | `42b1dc6da` | - | - | 6-tests-build/0037-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 200 | `0200-6-tests-build-0038-moe-cache-route-fit-probing-and-t.patch` | arifi-fork-base | `7b75d2d81` | - | - | 6-tests-build/0038-moe-cache-route-fit-probing-and-test-session-calls-t.patch |
| 201 | `0201-6-tests-build-0039-moe-cache-route-context-eligibili.patch` | arifi-fork-base | `352f820cc` | - | - | 6-tests-build/0039-moe-cache-route-context-eligibility-probe-and-remain.patch |
| 202 | `0202-6-tests-build-0040-moe-cache-allow-automatic-mode-on.patch` | arifi-fork-base | `a129a4af8` | - | - | 6-tests-build/0040-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 203 | `0203-6-tests-build-0041-moe-cache-fix-auto-asymmetric-MLA.patch` | arifi-fork-base | `6845f5eef` | - | - | 6-tests-build/0041-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch |
| 204 | `0204-6-tests-build-0042-test-moe-cache-accept-IGPU-device.patch` | arifi-fork-base | `5be7df7cb` | - | - | 6-tests-build/0042-test-moe-cache-accept-IGPU-devices-not-just-GPU.patch |
| 205 | `0205-6-tests-build-0043-test-moe-cache-make-cache-shared-.patch` | arifi-fork-base | `c63d600e1` | - | - | 6-tests-build/0043-test-moe-cache-make-cache-shared-budget-UMA-aware-wi.patch |
| 206 | `0206-6-tests-build-0044-test-moe-cache-save-and-restore-t.patch` | arifi-fork-base | `cbf7302e8` | - | - | 6-tests-build/0044-test-moe-cache-save-and-restore-the-CUDA-device-arou.patch |
| 207 | `0207-6-tests-build-0045-tests-tell-ctest-that-77-means-sk.patch` | arifi-fork-base | `90818e781` | - | - | 6-tests-build/0045-tests-tell-ctest-that-77-means-skip-for-test-moe-cac.patch |
| 208 | `0208-resolved-0011-fix-correct-Vulkan-turbo3-pipeline-wir.patch` | arifi-fork-base | `896e01245` | - | - | resolved/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch |
| 209 | `0209-resolved-0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-ga.patch` | arifi-fork-base | `de73817f4` | - | - | resolved/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch |
| 210 | `0210-resolved-0005-merge-close-TurboQuant-parity-gaps.pat.patch` | arifi-fork-base | `cc4cf49d4` | - | - | resolved/0005-merge-close-TurboQuant-parity-gaps.patch |
| 211 | `0211-resolved-0036-common-surface-MoE-cache-activation.pa.patch` | arifi-fork-base | `c45506904` | - | - | resolved/0036-common-surface-MoE-cache-activation.patch |
| 212 | `0212-resolved-0010-feat-add-Vulkan-turbo3-KV-cache-pipeli.patch` | arifi-fork-base | `65379afea` | - | - | resolved/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch |
| 213 | `0213-resolved-0004-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `2e27fa2ac` | - | - | resolved/0004-fix-close-complete-audit-findings-7-port-regressions.patch |
| 214 | `0214-resolved-0005-metal-restore-lost-kernel-templates-ad.patch` | arifi-fork-base | `c3b9f9607` | - | - | resolved/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch |
| 215 | `0215-resolved-0012-metal-implement-DeepSeek-V4-hyper-conn.patch` | arifi-fork-base | `41b8d5f27` | - | - | resolved/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch |
| 216 | `0216-resolved-0035-cuda-gate-fused-TQ-mul_mat-paths-on-co.patch` | arifi-fork-base | `f0535038d` | - | - | resolved/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch |
| 217 | `0217-resolved-0036-cuda-disable-fused-TQ3_1S-mul_mat-kern.patch` | arifi-fork-base | `87718e709` | - | - | resolved/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch |
| 218 | `0218-resolved-0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HI.patch` | arifi-fork-base | `3213b5ae6` | - | - | resolved/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch |
| 219 | `0219-resolved-0047-sycl-hip-fix-two-build-breaks-in-fork-.patch` | arifi-fork-base | `e52ddca47` | - | - | resolved/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch |
| 220 | `0220-resolved-0006-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `c824d2526` | - | - | resolved/0006-fix-close-complete-audit-findings-7-port-regressions.patch |
| 221 | `0221-resolved-0022-DeepseekV4-MTP-DSpark-25784.patch.patch` | arifi-fork-base | `271b69fee` | - | - | resolved/0022-DeepseekV4-MTP-DSpark-25784.patch |
| 222 | `0222-resolved-0023-dflash-fix-merge-artifacts-from-upstre.patch` | arifi-fork-base | `6c26a9a2e` | - | - | resolved/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch |
| 223 | `0223-resolved-0029-server-harden-shared-draft-device-plac.patch` | arifi-fork-base | `40cf2c16d` | - | - | resolved/0029-server-harden-shared-draft-device-placement.patch |
| 224 | `0224-resolved-0025-fix-server-batch-restored-checkpoint-p.patch` | arifi-fork-base | `985955a40` | - | - | resolved/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch |
| 225 | `0225-resolved-0026-cuda-rework-MoE-expert-cache-execution.patch` | arifi-fork-base | `178d136a7` | - | - | resolved/0026-cuda-rework-MoE-expert-cache-execution.patch |
| 226 | `0226-resolved-0033-server-account-for-MTP-placement-in-fi.patch` | arifi-fork-base | `97847f655` | - | - | resolved/0033-server-account-for-MTP-placement-in-fit-reservations.patch |
| 227 | `0227-resolved-0035-cuda-reject-undersized-automatic-MoE-c.patch` | arifi-fork-base | `7acdcab79` | - | - | resolved/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 228 | `0228-resolved-0038-moe-cache-add-moe-cache-soft-mode-with.patch` | arifi-fork-base | `939e27273` | - | - | resolved/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch |
| 229 | `0229-resolved-0039-model-Muse-Glimmer-Support-26841.patch.patch` | arifi-fork-base | `178a18de4` | - | - | resolved/0039-model-Muse-Glimmer-Support-26841.patch |
| 230 | `0230-resolved-0043-moe-cache-audit-fixes-H1-F1-F2-A1.patc.patch` | arifi-fork-base | `5d3cee1ab` | - | - | resolved/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch |
| 231 | `0231-resolved-0044-moe-cache-restore-moe_cache-params-on-.patch` | arifi-fork-base | `6c5b1b31f` | - | - | resolved/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch |
| 232 | `0232-resolved-0046-llama-remove-dead-MSA-indexer-scaffold.patch` | arifi-fork-base | `6deea2ec3` | - | - | resolved/0046-llama-remove-dead-MSA-indexer-scaffolding.patch |
| 233 | `0233-resolved-0047-moe-cache-add-logging-at-silent-failur.patch` | arifi-fork-base | `a8cb67290` | - | - | resolved/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch |
| 234 | `0234-resolved-0052-cuda-fit-fix-two-Werror-build-failures.patch` | arifi-fork-base | `32b80fe16` | - | - | resolved/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch |
| 235 | `0235-resolved-0005-test-fix-some-CI-errors-26415.patch.patch` | arifi-fork-base | `3c94f5827` | - | - | resolved/0005-test-fix-some-CI-errors-26415.patch |
| 236 | `0236-resolved-remaining-conflict-markers.patch` | arifi-fork-base | `11838d097` | - | - | resolved/remaining-conflict-markers |
| 237 | `0237-layer7-0001-WIP-add-TurboQuant-KV-cache-types-turbo3.patch` | arifi-fork-base | `43a7b1982` | - | - | layer7/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 238 | `0238-layer7-0002-Update-GGMLQuantizationType-and-LlamaFil.patch` | arifi-fork-base | `45dc09ac7` | - | - | layer7/0002-Update-GGMLQuantizationType-and-LlamaFileType-enums-.patch |
| 239 | `0239-layer7-0003-ggml-port-TurboQuant-core-quant-types-an.patch` | arifi-fork-base | `2627f7d80` | - | - | layer7/0003-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 240 | `0240-layer7-0005-docs-add-rebase-plan-and-TurboQuant-reci.patch` | arifi-fork-base | `a9f8dab7f` | - | - | layer7/0005-docs-add-rebase-plan-and-TurboQuant-recipes.patch |
| 241 | `0241-layer7-0007-gguf-py-dedupe-stacked-merge-artifacts-i.patch` | arifi-fork-base | `a55de4467` | - | - | layer7/0007-gguf-py-dedupe-stacked-merge-artifacts-in-constants..patch |
| 242 | `0242-layer7-0008-docs-add-KV-cache-quantization-guide-upd.patch` | arifi-fork-base | `7bee8a8a3` | - | - | layer7/0008-docs-add-KV-cache-quantization-guide-update-rebase-p.patch |
| 243 | `0243-layer7-0009-vulkan-port-fork-241-wave64-ballot-fix-c.patch` | arifi-fork-base | `77eaf7ec9` | - | - | layer7/0009-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch |
| 244 | `0244-layer7-0010-merge-close-TurboQuant-parity-gaps.patch.patch` | arifi-fork-base | `1247ae4dc` | - | - | layer7/0010-merge-close-TurboQuant-parity-gaps.patch |
| 245 | `0245-layer7-0011-metal-restore-lost-kernel-templates-add-.patch` | arifi-fork-base | `a03eaa89d` | - | - | layer7/0011-metal-restore-lost-kernel-templates-add-offline-shad.patch |
| 246 | `0246-layer7-0014-laguna-tool-call-whitespace-tolerance-do.patch` | arifi-fork-base | `4a3978b3d` | - | - | layer7/0014-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch |
| 247 | `0247-layer7-0019-fix-apply-rebase-audit-fixes-stack-overf.patch` | arifi-fork-base | `722270795` | - | - | layer7/0019-fix-apply-rebase-audit-fixes-stack-overflow-quality-.patch |
| 248 | `0248-layer7-0020-docs-add-TurboQuant-project-overview-to-.patch` | arifi-fork-base | `6dbd2992c` | - | - | layer7/0020-docs-add-TurboQuant-project-overview-to-AGENTS.md-an.patch |
| 249 | `0249-layer7-0021-cuda-rework-MoE-expert-cache-execution.p.patch` | arifi-fork-base | `fd695760c` | - | - | layer7/0021-cuda-rework-MoE-expert-cache-execution.patch |
| 250 | `0250-layer7-0022-docs-update-MoE-cache-validation.patch.patch` | arifi-fork-base | `10728bd54` | - | - | layer7/0022-docs-update-MoE-cache-validation.patch |
| 251 | `0251-layer7-0023-common-preserve-implicit-MoE-cache-provi.patch` | arifi-fork-base | `15541d4a9` | - | - | layer7/0023-common-preserve-implicit-MoE-cache-provider-settings.patch |
| 252 | `0252-layer7-0024-server-harden-shared-draft-device-placem.patch` | arifi-fork-base | `85996f73c` | - | - | layer7/0024-server-harden-shared-draft-device-placement.patch |
| 253 | `0253-layer7-0025-cuda-tune-MoE-cache-CPU-overlap-automati.patch` | arifi-fork-base | `1003100ac` | - | - | layer7/0025-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 254 | `0254-layer7-0026-cuda-adapt-MoE-cache-admission-to-device.patch` | arifi-fork-base | `25b1a46be` | - | - | layer7/0026-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 255 | `0255-layer7-0027-cuda-align-MoE-cache-pool-allocation-wit.patch` | arifi-fork-base | `c481fab9a` | - | - | layer7/0027-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 256 | `0256-layer7-0028-docs-update-MoE-cache-validation.patch.patch` | arifi-fork-base | `77d4dfd82` | - | - | layer7/0028-docs-update-MoE-cache-validation.patch |
| 257 | `0257-layer7-0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch.patch` | arifi-fork-base | `ebec51f9e` | - | - | layer7/0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 258 | `0258-layer7-0030-cuda-aggregate-small-MoE-tensors-into-ca.patch` | arifi-fork-base | `50099fe0d` | - | - | layer7/0030-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 259 | `0259-layer7-0031-cuda-bound-automatic-MoE-cache-admission.patch` | arifi-fork-base | `0672d5f09` | - | - | layer7/0031-cuda-bound-automatic-MoE-cache-admission.patch |
| 260 | `0260-layer7-0032-cuda-reject-undersized-automatic-MoE-cac.patch` | arifi-fork-base | `d59ec184e` | - | - | layer7/0032-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 261 | `0261-layer7-0033-cuda-enable-multi-token-MoE-cache-in-for.patch` | arifi-fork-base | `19d016fe6` | - | - | layer7/0033-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 262 | `0262-layer7-0034-common-surface-MoE-cache-activation.patc.patch` | arifi-fork-base | `afca1f4e4` | - | - | layer7/0034-common-surface-MoE-cache-activation.patch |
| 263 | `0263-layer7-0035-docs-fix-MoE-cache-benchmark-verbosity.p.patch` | arifi-fork-base | `dc21ee7fe` | - | - | layer7/0035-docs-fix-MoE-cache-benchmark-verbosity.patch |
| 264 | `0264-layer7-0036-cuda-accelerate-complete-MoE-cache-pools.patch` | arifi-fork-base | `2fb9ffe81` | - | - | layer7/0036-cuda-accelerate-complete-MoE-cache-pools.patch |
| 265 | `0265-layer7-0037-cuda-report-oversized-MoE-cache-nodes.pa.patch` | arifi-fork-base | `964e46ae1` | - | - | layer7/0037-cuda-report-oversized-MoE-cache-nodes.patch |
| 266 | `0266-layer7-0038-cuda-clarify-MoE-cache-session-diagnosti.patch` | arifi-fork-base | `3d4ba04f7` | - | - | layer7/0038-cuda-clarify-MoE-cache-session-diagnostics.patch |
| 267 | `0267-layer7-0039-docs-record-MoE-cache-workload-convergen.patch` | arifi-fork-base | `6426da001` | - | - | layer7/0039-docs-record-MoE-cache-workload-convergence.patch |
| 268 | `0268-layer7-0040-cuda-report-MoE-cache-pair-residency.pat.patch` | arifi-fork-base | `1927e6559` | - | - | layer7/0040-cuda-report-MoE-cache-pair-residency.patch |
| 269 | `0269-layer7-0041-docs-document-what-the-test-suites-do-an.patch` | arifi-fork-base | `deb396a84` | - | - | layer7/0041-docs-document-what-the-test-suites-do-and-do-not-cov.patch |
| 270 | `0270-layer7-0044-moe-cache-provider-registry-with-per-sch.patch` | arifi-fork-base | `ece8a21a0` | - | - | layer7/0044-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 271 | `0271-layer7-0046-moe-cache-allow-automatic-mode-on-a-sing.patch` | arifi-fork-base | `bb807156c` | - | - | layer7/0046-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 272 | `0272-layer7-0048-docs-document-INFO-level-MoE-cache-disab.patch` | arifi-fork-base | `b223bd67e` | - | - | layer7/0048-docs-document-INFO-level-MoE-cache-disable-reasons-i.patch |
| 273 | `0273-layer7-0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CAC.patch` | arifi-fork-base | `61e4d9d46` | - | - | layer7/0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CACHE.md-and-re.patch |
| 274 | `0274-layer7-0050-docs-correct-MOE-CACHE.md-against-the-im.patch` | arifi-fork-base | `3a5a90712` | - | - | layer7/0050-docs-correct-MOE-CACHE.md-against-the-implementation.patch |
| 275 | `0275-layer7-0051-ggml-split-the-turbo3-group-size-declara.patch` | arifi-fork-base | `4e2473c6f` | - | - | layer7/0051-ggml-split-the-turbo3-group-size-declaration-from-it.patch |
| 276 | `0276-resolve-8-committed-conflict-markers-on-merit.patch` | arifi-fork-base | `b2abd491c` | - | - | resolve 8 committed conflict markers on merit |
| 277 | `0277-fix-the-merge-so-it-builds-5-files-6-defects.patch` | arifi-fork-base | `7e987f524` | - | - | fix the merge so it builds: 5 files, 6 defects |
| 278 | `0278-merge-repairs-seven-defect-classes-and-one-the-compi.patch` | arifi-fork-base | `194fc0b68` | - | - | merge repairs: seven defect classes, and one the compiler could never have reported |
| 279 | `0279-ROCmFPX-on-Vulkan-the-shader-half-ported.-NOT-YET-WO.patch` | arifi-fork-base | `6c46ac5f0` | - | - | ROCmFPX on Vulkan: the shader half, ported. NOT YET WORKING - it segfaults in execution. |
| 280 | `0280-currency-register-ALL-SEVEN-forks-so-nothing-can-dri.patch` | arifi-fork-base | `7c75ae264` | - | - | currency: register ALL SEVEN forks, so nothing can drift unseen again |
| 281 | `0281-series-base-and-the-branch-it-describes-are-ONE-pair.patch` | arifi-fork-base | `dc23ffe2b` | - | - | series: base and the branch it describes are ONE pair, and the linearity rule is now a gate |
| 282 | `0282-hygiene-the-code-review-skill-taught-a-type-ID-map-t.patch` | arifi-fork-base | `6727d0d79` | - | - | hygiene: the code-review skill taught a type-ID map the arbitration deleted, and the UI was fetched from a moving tag |
| 283 | `0283-contract-tq3-is-the-first-family-we-must-renumber-so.patch` | arifi-fork-base | `b415d09f6` | - | - | contract: tq3 is the first family we must renumber, so write down why before anyone ports it |
| 284 | `0284-sources-honest-pins-turboquant-re-pinned-off-a-dead-.patch` | arifi-fork-base | `0778348ae` | - | - | sources: honest pins - turboquant re-pinned off a dead lineage, tq3 main-understates note REFUTED, five type-id contract collisions recorded (lane-143) |
| 285 | `0285-sources-lane-143-self-refutation-the-turbo3-ODR-defe.patch` | arifi-fork-base | `81862eb23` | - | - | sources: lane-143 self-refutation - the turbo3 ODR 'defect' does not exist in our tree (GGML_API already carries extern on all four branches); prisml pin REVERTED to keep the BEHIND signal live; FLAG E severity re-derived from loader code |
| 286 | `0286-sources-r2-checker-BLOCK-F1-F6-ciru-160-was-145-comm.patch` | arifi-fork-base | `1e35ace0c` | - | - | sources r2 (checker BLOCK F1-F6): ciru 160 was 145 commits of rocmfpx history - corrected to a 7-15 bracket, no-ancestor premise refuted, GOAL-3 ciru review performed, rocmfpx.ref revert, prisml contradiction removed, FLAG E sensitivity added |
| 287 | `0287-series-point-base.ref-at-the-LINEAR-branch-recut-lin.patch` | arifi-fork-base | `e17ed8a5c` | - | - | series: point base.ref at the LINEAR branch - recut-linear/2026-08-17 is the byte-identical flatten of the blessed union branch, so the series is replayable at last (lane-147) |
| 288 | `0288-series-disclose-in-the-record-itself-that-the-flatte.patch` | arifi-fork-base | `5a799984b` | - | - | series: disclose in the record itself that the flatten absorbed 17 empty commits - content unchanged, history count changed (lane-147 checker finding 4) |
| 289 | `0289-ggml-port-the-tq3-TQ3_4S-family-codec-at-ids-48-51-T.patch` | arifi-fork-base | `9658c1826` | - | - | ggml: port the tq3 TQ3_4S family codec at ids 48-51 (TYPE-ID-ALLOCATION 3.1.1) - Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb |
| 290 | `0290-tests-cover-the-tq3-family-in-test-quantize-fns-and-.patch` | arifi-fork-base | `840064a30` | - | - | tests: cover the tq3 family in test-quantize-fns and test-backend-ops (kilobyte-scale proof before the 13 GB run) |
| 291 | `0291-contract-tq3-rows-48-51-implemented-behind-GGML_ARIF.patch` | arifi-fork-base | `308878402` | - | - | contract: tq3 rows 48-51 implemented behind GGML_ARIFI_TURBO_WEIGHT_QUANTS, plus the three corrections the port forces (their 200 = our TURBO2_0; six ids, four destinations; token_embd is Q6_K) |
| 292 | `0292-tests-tq3_4s-tq3_4se-skipped-in-test-quantize-fns-wi.patch` | arifi-fork-base | `96ab60613` | - | - | tests: tq3_4s/tq3_4se skipped in test-quantize-fns with the measured reason (E3M5 scale caps at 0.492, harness data needs 1.4); tq3_0 given documented 3.5bpw bounds |
| 293 | `0293-tools-gguf-retag-tq3-lands-LAST-after-the-coherence-.patch` | arifi-fork-base | `29cee8978` | - | - | tools: gguf-retag-tq3 - lands LAST, after the coherence proof, per TYPE-ID-ALLOCATION 3.1.1. Decides ambiguous id 46 by measured byte span; five refusals, no in-place path |
| 294 | `0294-checker-fixes-choose_index-restored-verbatim-inverte.patch` | arifi-fork-base | `7906db777` | - | - | checker fixes: choose_index restored verbatim (inverted tie-break + 2 drifted constants, encoder-only); tq3 numeric block gives TQ3_4S real proof and pins every dot mapping; drop redundant GGML_API redecl |
| 295 | `0295-retag-verify-geometry-per-TENSOR-for-every-mapped-id.patch` | arifi-fork-base | `091fa1ab5` | - | - | retag: verify geometry per TENSOR for every mapped id, not a file-level vote on 46 only - fixes silent rewrite of mixed/ambiguous files and of unguarded 36/200; guard empty and overlapping tables |
| 296 | `0296-retag-README-per-tensor-geometry-check-for-every-map.patch` | arifi-fork-base | `97ab4c1f9` | - | - | retag README: per-tensor geometry check for every mapped id, ten refusals, byte-level assertion |
| 297 | `0297-lane-144-GOAL-2-step-1-TQ3_4S-id-48-Vulkan-port-dequ.patch` | arifi-fork-base | `c61e737ab` | - | - | lane-144 GOAL 2 step 1: TQ3_4S (id 48) Vulkan port - dequant + mat-vec + mat-vec_id shaders, wired |
| 298 | `0298-lane-144-sweep-ROCmFP4-ROCmFPX-in-test-backend-ops-a.patch` | arifi-fork-base | `595f513fe` | - | - | lane-144: sweep ROCmFP4/ROCmFPX in test-backend-ops all_types (six identifiers, never listed before) |
| 299 | `0299-lane-144-CHECKER-FIX-TQ3_4S-was-missing-from-the-MUL.patch` | arifi-fork-base | `f2f0fa247` | - | - | lane-144 CHECKER FIX: TQ3_4S was missing from the MUL_MAT_ID staging-overflow guard |
| 300 | `0300-lane-145-Q6_0_ROCMFPX-Vulkan-fix-implement-the-6-bit.patch` | arifi-fork-base | `28acc28ea` | - | - | lane-145: Q6_0_ROCMFPX Vulkan fix - implement the 6-bit unpack (block struct was 34B vs CPU 26B; qs read as whole signed bytes). 60/60 MUL_MAT + 24/24 GET_ROWS on Vulkan0, was 50/60 + 20/24. |
| 301 | `0301-lane-145-checker-finding-7-delete-block_rocmfpx_fp6_.patch` | arifi-fork-base | `b22cf30bc` | - | - | lane-145 checker finding 7: delete block_rocmfpx_fp6_packed16 + its A_TYPE_PACKED16 define (26B block has no valid uint16 view; nothing reads it). Re-verified 60/60 MUL_MAT, 24/24 GET_ROWS, 12/12 MUL_MAT_ID. |
| 302 | `0302-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | ternary-g128 | `0f26f771b` | - | - | vulkan: ask the pipelines whether they exist, not the type id (SET_ROWS + CPY) |
| 303 | `0303-test-backend-ops-cover-the-CPY-types-supports_op-nam.patch` | arifi-fork-base | `9645be4b4` | - | - | test-backend-ops: cover the CPY types supports_op names that all_types omits |
| 304 | `0304-lane-148-defect-A-ROOT-FIX-Vulkan-turbo3-centroid-LU.patch` | arifi-fork-base | `259c90e5a` | - | - | lane-148 defect A ROOT FIX: Vulkan turbo3 centroid LUT was stale at 5 sites - CPY turbo3->f32 ERR 8.4302e-5 -> OK. CPU/CUDA/Metal/SYCL all carry CENTROIDS_3BIT (-0.190207,-0.118786,-0.066822,-0.021663); Vulkan alone carried a table whose midpoints do not match nearest_centroid_3bit(). copy_to_quant.comp's TM decision boundaries were derived from the stale table and are corrected too. |
| 305 | `0305-lane-148-defect-B-ROOT-FIX-FA_TYPE_TURBO2-3-4_0-GLSL.patch` | arifi-fork-base | `8ef93b515` | - | - | lane-148 defect B ROOT FIX: FA_TYPE_TURBO2/3/4_0 GLSL spec-constant ids were stale (43/44/47) while enum ggml_type puts them at 200/201/202. Host passes the raw enum, so both switches fell through: fa_block_elems() returned its 1u default and dequantize4() returned vec4(0), zeroing V and the whole FA output. FLASH_ATTN_EXT -p turbo 0/1728 -> 1728/1728. |
| 306 | `0306-lane-148-F-69-sibling-coverage-test-backend-ops-FLAS.patch` | arifi-fork-base | `43d13a0ef` | - | - | lane-148 F-69 sibling coverage: test-backend-ops FLASH_ATTN_EXT now covers TURBO2_0 too. Vulkan supports_op has always claimed turbo2 for FA but no test case existed, so its stale FA_TYPE id was invisible. 1728 -> 2592 cases, all PASS. |
| 307 | `0307-lane-148-checker-findings-4-5-FIXED-turbo4-SET_ROWS-.patch` | arifi-fork-base | `2ea7d28da` | - | - | lane-148 checker findings 4+5 FIXED: turbo4 SET_ROWS was 0/21 declined behind a green 'Backend Vulkan0: OK' banner and turbo2 had no SET_ROWS case at all. Root cause: no cpy_turbo{2,4}_0_f32 copy-from-quant shader, so the tests' read-back leg declined. Added turbo2/turbo4 dequantize()/dequantize4()/get_dm(), the two spv+pipeline registrations, both supports_op and the DISPATCH switch (a third hand-synced pair - supports_op alone aborts with 'Missing CPY op'), and a test_set_rows_turbo2 class. SET_ROWS_TURBO2/3/4 now 21/21 EXECUTED each. |
| 308 | `0308-lane-149-F-110-guard-rung-b-arifi_sync_check.py-CMak.patch` | arifi-fork-base | `9554d9710` | - | - | lane-149 F-110 guard (rung b): arifi_sync_check.py + CMake wiring + 16 ARIFI-SYNC markers. Checks 49 LUT pairs, 16 FA_TYPE ids, 35 QUANT_K block sizes, 4 type-list SETs, FA K/V coverage; unregistered mirrors FAIL |
| 309 | `0309-lane-149-F-112-F-109-harness-guards-NOT_SUPPORTED-ca.patch` | ternary-g128 | `58bb59926` | - | - | lane-149 F-112 + F-109 harness guards: NOT_SUPPORTED cases counted and named, zero-executed run now FAILS; all_types coverage-parity assert with reason-bearing waivers; Q2_0_G128 added to all_types |
| 310 | `0310-lane-149-F-109-payoff-q2_0_g128-SET_ROWS-tie-roundin.patch` | ternary-g128 | `7842efc41` | - | - | lane-149 F-109 payoff: q2_0_g128 SET_ROWS tie-rounding fixed. GLSL round() breaks .5 ties implementation-chosen; CPU roundf is half-away-from-zero. 8 f16 failures -> 2, ERR 5.1e-4 -> 1.7e-5 |
| 311 | `0311-lane-149-checker-driven-hardening-of-arifi-sync-chec.patch` | arifi-fork-base | `ac2a3f565` | - | - | lane-149 checker-driven hardening of arifi-sync-check: FA_TYPE/QUANT_K literal spellings, ordinal (not line) pins, per-definition completeness, any array type, recursive shader walk, scalar-mirror class, expected-count assertions |
| 312 | `0312-lane-149-F-112-second-half-checker-finding-11-per-op.patch` | arifi-fork-base | `6e8148b4d` | - | - | lane-149 F-112 second half (checker finding 11): per-op tallies name every op whose cases were ALL declined, so a zero-coverage family cannot hide inside a green full sweep |
| 313 | `0313-lane-150-step-1-TQ4_1S-read-back-leg-wired-CPY-tq4_1.patch` | arifi-fork-base | `40e796cd9` | - | - | lane-150 step 1: TQ4_1S read-back leg wired - CPY(tq4_1s,f32) now EXECUTES, 291/291 -> 293/293 |
| 314 | `0314-lane-150-step-2-TQ4_1S-write-leg-EXECUTES-SET_ROWS_T.patch` | arifi-fork-base | `dd0d54208` | - | - | lane-150 step 2: TQ4_1S write leg EXECUTES - SET_ROWS_TQ4_1S 17 cases/0 executed -> 17/17 PASS, and the inherited 5.0 waive is retired |
| 315 | `0315-lane-150-step-3-root-fix-for-the-hole-that-hid-the-w.patch` | arifi-fork-base | `9911ffe9e` | - | - | lane-150 step 3 (root fix for the hole that hid the whole class): per-op x TYPE zero-coverage tally |
| 316 | `0316-lane-150-checker-driven-fixes-type_bucket-prefix-bou.patch` | arifi-fork-base | `f3909b07a` | - | - | lane-150 checker-driven fixes: type_bucket prefix-boundary bug + dispatch block-size assert + the tallys limits named in code |
| 317 | `0317-lane-151-GOAL-6-hygiene-drop-648-tracked-_lane-out-d.patch` | arifi-fork-base | `a76cc84ed` | - | `GGML_ARIFI_VNNI_REPACK` | lane-151 GOAL 6 hygiene: drop 648 tracked _lane-out debris files (incl. an embedded shadow.git with commit receipts) and gitignore the dir; genericize estate paths in docs/, licenses/, tools/gguf-retag-tq3 |
| 318 | `0318-lane-151-GOAL-9-reserve-tq3-s-three-destination-less.patch` | arifi-fork-base | `61f1a6a6e` | - | - | lane-151 GOAL 9: reserve tq3's three destination-less serialized ids (37 TQ3_4SV -> 52, 31 TQ3_1S_AP1 -> 53, 44 TQ3_1S -> 54) as RESERVED-not-implemented in docs/TYPE-ID-ALLOCATION.md 3.1.2; geometry marked NOT DERIVED, no enumerators added, retag-tool refusals unchanged |
| 319 | `0319-lane-151-GOALs-6-7-README-refreshed-to-b10453-canoni.patch` | arifi-fork-base | `84b5f5700` | - | - | lane-151 GOALs 6+7: README refreshed to b10453 canonical arifi-main + the seven-ingested-sources table (two honest zeroes named); tq3 and ciru credits added; licenses/README.md gains the full Taken-from audit incl. the tq3 license GAP; UPDATE-RUNBOOK.md written (O(delta) recipe, THREE-flag configure, guards, the standard) |
| 320 | `0320-lane-151-GOAL-2-sources.json-base.ref-arifi-main-the.patch` | arifi-fork-base | `359b4d2f8` | - | `GGML_ARIFI_VNNI_REPACK` | lane-151 GOAL 2: sources.json base.ref -> arifi/main (the canonical branch is now the series base) + series regen over the full canonical range: 323 patches, series check PASS, replay onto 4df29be4f identical outside patches/series |
| 321 | `0321-lane-151-GOAL-5-gitignore-.tokensave-the-fork-repo-n.patch` | arifi-fork-base | `cc5b7dd2c` | - | - | lane-151 GOAL 5: gitignore .tokensave/ - the fork repo now carries its own code graph (63344 nodes, 154451 edges over 3407 files) and the index is per-machine, never committed |
| 322 | `0322-lane-151-CHECKER-ROOT-FIX-series-check-now-asserts-e.patch` | arifi-fork-base | `891c58f0d` | - | - | lane-151 CHECKER ROOT FIX: series check now asserts every patch is a COMMITTED BLOB, not just a file on disk. The byte-compare ran against the working directory, so untracked patches made it green while a fresh clone would die at git am - the defect that hid 33 missing patches twice. Help text corrected to match what the code does |
| 323 | `0323-lane-151-CHECKER-FIXES-docs-UPDATE-RUNBOOK-4.5-no-lo.patch` | arifi-fork-base | `3f9a7608c` | - | - | lane-151 CHECKER FIXES (docs): UPDATE-RUNBOOK 4.5 no longer asserts a self-containment standard nothing on this estate meets - the four MinGW runtime DLLs are named and staged, and the measured stripped-PATH failure is recorded as identical for the raw build tree and the previous engine. licenses/README.md audit header stops claiming a purely mechanical trailer derivation: the tq3 row is a SUBJECT-line citation git never parses as a trailer, which is exactly why a trailer-only sweep would have missed the one real license gap |
| 324 | `0324-lane-151-HQ-DIRECTIVE-RETAIN-the-licenses-instead-of.patch` | arifi-fork-base | `1de9526c5` | - | - | lane-151 HQ DIRECTIVE: RETAIN the licenses instead of flagging them. tq3-MIT, prisml-MIT and ciru-MIT extracted verbatim from this repo's OWN fetched remotes (git show refs/remotes/<r>/<head>:LICENSE). REFUTES my own GOAL-6 finding and a year-old one: I wrote 'no clone remains on the estate' and Phase 0 wrote 'PrismML has no LICENSE' - both were absence claims checked against the wrong artifact while the remotes sat fetched in this very repository. All seven registered sources audited at once. thecodacus STAYS open: no remote, no text, nothing invented |
| 325 | `0325-licenses-thecodacus-MIT-retained-from-the-public-Git.patch` | arifi-fork-base | `14d598653` | - | - | licenses: thecodacus-MIT retained from the public GitHub fork - the last open attribution CLOSED (no local remote != no source; President's correction 2026-08-18) |
| 326 | `0326-lane-152-GOAL-1-upstream-bump-b10453-b10481-the-FIRS.patch` | arifi-fork-base | `b6a4231ce` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | lane-152 GOAL 1: upstream bump b10453 -> b10481, the FIRST live execution of UPDATE-RUNBOOK 2. 348 patches replayed onto 25ae3a9b3, 4 conflicts resolved by hand, series regenerated to 329, zero merges. |
| 327 | `0327-lane-152-what-the-FIRST-live-run-of-UPDATE-RUNBOOK-s.patch` | arifi-fork-base | `3959ac94a` | - | - | lane-152: what the FIRST live run of UPDATE-RUNBOOK section 2 corrected - one build break, two arifi_sync defects, the runbook itself, and the last license row |
| 328 | `0328-lane-152-GOAL-2-batch-1-ciru-s-Vulkan-SPIRV-Headers-.patch` | arifi-fork-base | `4f63c2198` | ciru@8ffb2cd5a9b9e7eecc49335e0c79291a0df81d65, tq3@b1dd18410620bbf16909444d95ee2f9c10015b085, rocmfpx@8e6277f855df2a27ce072525ed19f3adc4138c47 | - | lane-152 GOAL 2 batch 1: ciru's Vulkan SPIRV-Headers fallback, tq3's f32 hybrid-SSM state gates, rocmfpx's -ffast-math guard |
| 329 | `0329-lane-152-GOAL-2-batch-2-three-lfm2-pre-tokenizer-has.patch` | arifi-fork-base | `7ad1fae5a` | rocmfpx@95bb4c798ee8c571e6b34ba4dd0ce15f4c2b9232 | - | lane-152 GOAL 2 batch 2: three lfm2 pre-tokenizer hashes that only rocmfpx had - and the reason they must be pre-computed |
| 330 | `0330-lane-152-pins-THREE-advance-with-reviews-upstream-ro.patch` | arifi-fork-base | `fd64ae867` | - | - | lane-152 pins: THREE advance with reviews (upstream, rocmfpx, tq3), TWO deliberately stay BEHIND (prisml, ciru) |
| 331 | `0331-lane-152-checker-F7-the-old-base-is-tag-b10454-not-b.patch` | arifi-fork-base | `568b35126` | - | - | lane-152 checker F7: the old base is tag b10454, not b10453 - correction of record |
| 332 | `0332-repo-env.json-WI-1540-tail-the-engine-repo-s-env-man.patch` | arifi-fork-base | `9306ba0fb` | - | - | repo-env.json (WI-1540 tail): the engine repo's env manifest - PowerShell-only law (F-107), three-flag configure (F-111), cmd.exe redirection (F-108), type-id FLAG E, series discipline, runbook pointer |
| 333 | `0333-WI-1580-sub-2-UPDATE-RUNBOOK-section-7-the-4.5-line-.patch` | arifi-fork-base | `52b068a53` | - | - | WI-1580 sub-2: UPDATE-RUNBOOK section 7 - the 4.5 line was stale. lane-153 staged models/engines/arifi-b10481-union/ from a verified build of canonical, smoked from the staged copies, and flipped current-state.json + artifact-catalog.json, so 4.5 HAS now been executed once, by hand. The bump distinction is KEPT because it is still true: no bump has ever driven staging end to end (lane-152's bump refused at leg 2). Narrow edit - the section-2 and section-3 paragraphs around it are untouched. Note for the record: the lane-153 REPORT cites this text at UPDATE-RUNBOOK.md:238-244; it actually lives at :308-323 |
| 334 | `0334-vulkan-land-the-MoE-expert-cache-provider-files-VK-M.patch` | ternary-g128 | `0ccf048cc` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: land the MoE expert cache provider files (VK-MOE-CACHE step 1 of 2 - files, not yet wired) |
| 335 | `0335-graph-derive-the-V-unpad-head-count-from-the-attenti.patch` | arifi-fork-base | `a98e992d2` | TheTom/llama-cpp-turboquant@f0420e056 | - | graph: derive the V-unpad head count from the attention output tensor, not from hparams |
| 336 | `0336-vulkan-wire-the-MoE-expert-cache-provider-into-the-b.patch` | ternary-g128 | `a76259e8e` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: wire the MoE expert cache provider into the build (VK-MOE-CACHE step 2 of 2) |
| 337 | `0337-vulkan-complete-the-MoE-cache-port-the-56-provider-h.patch` | ternary-g128 | `08f378b0b` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: complete the MoE-cache port - the +56 provider-header addition, the four handle accessors, and the include-order ROOT FIX the link error exposed |
| 338 | `0338-sources-advance-the-turboquant-pin-f6124e914-7ebcbb0.patch` | arifi-fork-base | `edf49e037` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: advance the turboquant pin f6124e914 -> 7ebcbb0b6 with pin_review_2026_08_18_lane154 |
| 339 | `0339-off-rig-backends-land-the-12-CUDA-Metal-SYCL-HIP-row.patch` | arifi-fork-base | `baabb42eb` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | off-rig backends: land the 12 CUDA/Metal/SYCL/HIP rows unbuilt, per the HQ ruling of 2026-08-18 |
| 340 | `0340-sources-pin_review-names-the-12-off-rig-shas-and-the.patch` | arifi-fork-base | `547cfecf6` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: pin_review names the 12 off-rig shas and their BUILD-UNPROVEN status (HQ condition 3) |
| 341 | `0341-sources-correct-the-pin_review-the-contested-12-12-a.patch` | arifi-fork-base | `12bba67d5` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: correct the pin_review - the contested 12/12 apply-conflict count is no longer the justification (checker F1) |
| 342 | `0342-sources-pin_review-records-the-apply-count-claim-as-.patch` | arifi-fork-base | `1481633fb` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: pin_review records the apply-count claim as WRONG, not disputed (checker F1, resolved against me) |
| 343 | `0343-base-b10481-25ae3a9b3-b10488-9d77fa172-series-regen-.patch` | arifi-fork-base | `4155f071f` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | base: b10481/25ae3a9b3 -> b10488/9d77fa172 + series regen 346 (lane-156 upstream bump) |
| 344 | `0344-moe-cache-heat-protected-eviction-keeps-hot-experts-.patch` | arifi-fork-base | `118a60223` | llama-cpp-turboquant@730cc87ed (+ its doc follow-up cef75bf06) | - | moe-cache: heat-protected eviction keeps hot experts resident (turboquant tail) |
| 345 | `0345-tests-initialise-non-contiguous-tensors-row-by-row-t.patch` | arifi-fork-base | `aa564eee3` | llama-cpp-turboquant@f58ee0e97 | - | tests: initialise non-contiguous tensors row by row (turboquant row f58ee0e97) |
| 346 | `0346-sources-advance-the-turboquant-pin-7ebcbb0b6-d14e368.patch` | arifi-fork-base | `a9aee5843` | - | - | sources: advance the turboquant pin 7ebcbb0b6 -> d14e36827 with a dated lane-156 pin_review |
| 347 | `0347-sources-prisml-pin-HELD-deliberately-with-a-sharper-.patch` | arifi-fork-base | `7f32b975b` | - | - | sources: prisml pin HELD deliberately with a sharper reason + the v6 K-cache-mean-center verdict (lane-156 GOAL 4) |
| 348 | `0348-sources-ciru-re-derived-315-15-and-HELD-with-all-15-.patch` | arifi-fork-base | `fc388cf2c` | - | - | sources: ciru re-derived 315 -> 15 and HELD with all 15 reviewed (lane-156 GOAL 5) |
| 349 | `0349-sources-advance-remotes.upstream.pin-25ae3a9b3-9d77f.patch` | arifi-fork-base | `392ae16a9` | - | - | sources: advance remotes.upstream.pin 25ae3a9b3 -> 9d77fa172 (b10488) with its review |
| 350 | `0350-sources-correct-the-turboquant-pin_review-s-FALSE-bl.patch` | arifi-fork-base | `42cf661b3` | - | - | sources: correct the turboquant pin_review's FALSE blocker for e130aef60 (checker F8, BLOCKING) |
| 351 | `0351-sources-trim-the-e130aef60-blocker-to-the-ONE-item-t.patch` | arifi-fork-base | `d6fbafdd7` | - | - | sources: trim the e130aef60 blocker to the ONE item that survives (checker pass 2, advisory 1) |
| 352 | `0352-feat-kv-cache-port-flag-gated-mean-centering.patch` | arifi-fork-base | `865814017` | prisml/prism-v6@b075a360c | - | feat(kv-cache): port flag-gated mean centering |
| 353 | `0353-feat-kv-cache-bind-calibration-to-exact-model.patch` | arifi-fork-base | `cdd7270be` | prisml/prism-v6@b075a360c | - | feat(kv-cache): bind calibration to exact model |
| 354 | `0354-fix-server-keep-target-calibration-out-of-draft-cont.patch` | arifi-fork-base | `421e9dc97` | prisml/prism@4dd165625 | - | fix(server): keep target calibration out of draft contexts |
| 355 | `0355-test-kv-cache-add-Vulkan-tensor-probe-and-bench-pure.patch` | arifi-fork-base | `4436772c6` | - | - | test(kv-cache): add Vulkan tensor probe and bench-pure A/B harness |
| 356 | `0356-sources-record-lane-158-PrismML-K-cache-disposition.patch` | arifi-fork-base | `94e8bfbde` | - | - | sources: record lane-158 PrismML K-cache disposition |
| 357 | `0357-test-kv-cache-fail-close-default-OFF-binary-identity.patch` | arifi-fork-base | `1259c9ec0` | - | - | test(kv-cache): fail-close default-OFF binary identity |
| 358 | `0358-test-kv-cache-assert-both-rotation-basis-polarities-.patch` | arifi-fork-base | `d88c0798f` | - | - | test(kv-cache): assert both rotation-basis polarities against the resolved attn_rot_k |
| 359 | `0359-fix-kv-cache-make-the-A-B-harness-able-to-launch-and.patch` | arifi-fork-base | `b22bc53f7` | - | - | fix(kv-cache): make the A/B harness able to launch, and actually enforce the RAM floor it claims |
| 360 | `0360-fix-kv-cache-floor-check-the-model-named-by-m-not-th.patch` | arifi-fork-base | `7acf51c63` | - | - | fix(kv-cache): floor-check the model named by -m, not the largest .gguf on the command line |
| 361 | `0361-fix-kv-cache-wait-for-the-RAM-floor-between-arms-and.patch` | arifi-fork-base | `bc0261903` | - | - | fix(kv-cache): wait for the RAM floor between arms, and pin -fit off so the A/B arms match |
| 362 | `0362-fix-kv-cache-probe-the-layer-ids-the-calibration-act.patch` | arifi-fork-base | `e144e5df0` | - | - | fix(kv-cache): probe the layer ids the calibration actually has, and let an existing calibration be reused |
| 363 | `0363-support-DFlash2.patch` | arifi-fork-base | `bf1e474bc` | - | - | support DFlash2 |
| 364 | `0364-dflash2-build-fixes-on-the-PR-27342-pick-dedupe-DFLA.patch` | arifi-fork-base | `82bc66c5f` | - | - | dflash2: build fixes on the PR #27342 pick - dedupe DFLASH_BLOCK_SIZE enum/hparam/kv-map (fork DFlash1 originals keep authority, default 16) + ml. reference fix (the shadow the fork's own comment documents) |
| 365 | `0365-sources-register-buun-spiritbuun-buun-llama-cpp-as-a.patch` | arifi-fork-base | `bdb9cc1c5` | - | - | sources: register buun (spiritbuun/buun-llama-cpp) as a greedy-ingestion watch source - President ruling, VBR/KV + DFlash2-for-3.8 lineage |
| 366 | `0366-cuda-lift-TQ3_4S-kernel-set-from-tq3-master-14-files.patch` | arifi-fork-base | `22f812d66` | turbo-tan/llama.cpp-tq3@854516439 | - | cuda: lift TQ3_4S kernel set from tq3/master (14 files) |
| 367 | `0367-metal-lift-TQ3_4S-kernel-set-from-tq3-master-5-files.patch` | arifi-fork-base | `5e675284d` | turbo-tan/llama.cpp-tq3@854516439 | - | metal: lift TQ3_4S kernel set from tq3/master (5 files + alloc hook) |
| 368 | `0368-metal-declare-the-tq3_rht-pipeline-getter-in-ggml-me.patch` | arifi-fork-base | `d6725a4fa` | turbo-tan/llama.cpp-tq3@854516439 | - | metal: declare the tq3_rht pipeline getter in ggml-metal-device.h |
| 369 | `0369-sync-move-base-b10488-b10524-regenerate-series-372-p.patch` | arifi-fork-base | `0eb8604f1` | upstream@9ee9fc04c (tag b10524) | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | sync: move base b10488 -> b10524, regenerate series (372 patches) |
| 370 | `0370-spec-ingest-turboquant-MTP-boost-wave-effective-KV-b.patch` | arifi-fork-base | `d83108b2b` | llama-cpp-turboquant@bd1bf025f (524531e57, f9e04f5d7, 4be91d62b, 5b105dfb7, 4c4131bf8, b20e97012, 7544b18cf, 275963f50, cd638bc13, e82fe159b) | - | spec: ingest turboquant MTP-boost wave + effective-KV bench reporting |
| 371 | `0371-sync-re-pin-wave-3-sources-regenerate-series-374-pat.patch` | arifi-fork-base | `0d2e5e3b6` | - | `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `LLAMA_USE_PREBUILT_UI`, `ARIFI_TOOL_NVFP4_REMAP` | sync: re-pin wave-3 sources + regenerate series (374 patches) |
| 372 | `0372-arifi-sync-provenance-native-class-root-fix-sha-pinn.patch` | arifi-fork-base | `b6bf5fb31` | - | - | arifi-sync: provenance native-class root fix - sha-pinned grandfather ledger + native Origin convention |
| 373 | `0373-vulkan-subgroup-cooperative-TQ-mat-vec-tq3_1s-tq4_1s.patch` | ternary-g128 | `bb6c17b86` | - | - | vulkan: subgroup-cooperative TQ mat-vec (tq3_1s/tq4_1s/tq3_4s) |
| 374 | `0374-vulkan-register-blocked-4-elem-lane-TQ-subgroup-mat-.patch` | ternary-g128 | `d13313f1f` | - | - | vulkan: register-blocked 4-elem/lane TQ subgroup mat-vec (packed32 loads, in-register WHT stages 1-2, rows=4) |
| 375 | `0375-escha-native-Escha-W2-types-ESCHA2-55-ESCHA3-56-fuse.patch` | arifi-fork-base | `ede218289` | - | - | escha: native Escha-W2 types (ESCHA2=55/ESCHA3=56) + fused GGML_OP_ESCHA_MM, CPU + Vulkan |
| 376 | `0376-escha-F-125-fix-full-64-bit-pair-sourcing-row-parity.patch` | arifi-fork-base | `2a03445f4` | - | - | escha: F-125 fix - full 64-bit pair sourcing + row-parity correction (K=2 AND K=3) |
| 377 | `0377-escha-test-backend-ops-un-nest-ESCHA_MM-eval-cases-m.patch` | arifi-fork-base | `2f1b6f34f` | - | - | escha: test-backend-ops — un-nest ESCHA_MM eval cases + multi-chunk ncols coverage + perf ncols curve |
| 378 | `0378-escha-vulkan-bound-every-escha_mm-dispatch-to-32-col.patch` | arifi-fork-base | `9cfd7e246` | - | - | escha vulkan: bound every escha_mm dispatch to 32 columns (F-124 host-freeze guard) |
| 379 | `0379-escha-vulkan-column-blocked-escha_mm-one-weight-deco.patch` | arifi-fork-base | `835a6717c` | - | - | escha vulkan: column-blocked escha_mm - one weight decode serves 4 columns (part 2) |
| 380 | `0380-escha-vulkan-column-block-C-8-fall-back-to-the-one-c.patch` | arifi-fork-base | `29a2be66d` | - | - | escha vulkan: column block C=8 + fall back to the one-column kernel below a full block |
| 381 | `0381-escha-tests-cover-the-column-blocked-kernel-AT-MODEL.patch` | arifi-fork-base | `366e6fa09` | - | - | escha tests: cover the column-blocked kernel AT MODEL DEPTH (ncols=9, real 27B shapes) |
| 382 | `0382-escha-vulkan-one-hardware-f32-f16-convert-instead-of.patch` | arifi-fork-base | `e73bcc519` | - | - | escha vulkan: one hardware f32->f16 convert instead of the 15-instruction software RTE |
| 383 | `0383-escha-vulkan-subgroup-shuffle-Hadamard-14-barriers-p.patch` | arifi-fork-base | `f3195d818` | - | - | escha vulkan: subgroup-shuffle Hadamard - 14 barriers per input block become 3 |
| 384 | `0384-escha-vulkan-the-hardware-f32-f16-convert-must-be-fl.patch` | arifi-fork-base | `41ebd38ae` | - | - | escha vulkan: the hardware f32->f16 convert must be float16_t, NOT packHalf2x16 (RTZ) |
| 385 | `0385-escha-vulkan-ESCHA_SG_HADAMARD-defaults-OFF-the-leve.patch` | arifi-fork-base | `66493e606` | - | - | escha vulkan: ESCHA_SG_HADAMARD defaults OFF - the lever is refuted by measurement |
| 386 | `0386-escha-vulkan-f16-native-generator-decode-18-bit-exac.patch` | arifi-fork-base | `1ea7ec310` | - | - | escha vulkan: f16-native generator - decode +18%, bit-exact |
| 387 | `0387-escha-vulkan-two-more-levers-tried-and-REFUTED-by-me.patch` | arifi-fork-base | `29e47ad01` | - | - | escha vulkan: two more levers tried and REFUTED by measurement - occupancy and packed-pair |
| 388 | `0388-escha-vulkan-the-multi-column-threshold-was-disablin.patch` | arifi-fork-base | `04ea349dd` | - | - | escha vulkan: the multi-column threshold was disabling speculation - decode 3.96 -> 5.60 t/s |
| 389 | `0389-escha-vulkan-column-ladder-C2-C4-C8-C16-and-the-gene.patch` | arifi-fork-base | `165c38e38` | - | - | escha vulkan: column ladder C2/C4/C8/C16, and the generator table measured and REFUTED |
| 390 | `0390-dflash-ingest-buun-s-DFlash2-adaptive-controller-bui.patch` | arifi-fork-base | `b0d7e5307` | - | - | dflash: ingest buun's DFlash2 adaptive controller - built, gated, and defaulted OFF by measurement |
| 391 | `0391-escha-vulkan-prefill-is-OCCUPANCY-bound-coopmat-and-.patch` | arifi-fork-base | `134e2448b` | - | - | escha vulkan: prefill is OCCUPANCY-bound - coopmat and wider rungs both refuted, and the profile that said otherwise was lying |
| 392 | `0392-escha-vulkan-f16-activation-staging-built-correct-an.patch` | arifi-fork-base | `992fe54f7` | - | - | escha vulkan: f16 activation staging - built, correct, and refuted; the real prefill win was the build |
| 393 | `0393-dflash-reject-an-out-of-extent-draft-depth-instead-o.patch` | arifi-fork-base | `c3af00159` | - | - | dflash: reject an out-of-extent draft depth instead of silently clamping it, and block the mask token |
| 394 | `0394-spec-load-the-draft-model-from-the-same-variable-the.patch` | arifi-fork-base | `5a3fb20b2` | - | - | spec: load the draft model from the same variable the log prints, and add an env-gated DFlash verify trace |
| 395 | `0395-F-136-ROOT-CAUSE-FIX-the-recurrent-snapshot-bank-n_r.patch` | prismml-recurrent | `6b0224d5a` | - | - | F-136 ROOT CAUSE + FIX: the recurrent snapshot-bank (n_rs_seq) machinery corrupts hybrid targets; drafters no longer flip the target onto it |
| 396 | `0396-spec-ON-DEVICE-speculative-checkpoints-drafter-now-B.patch` | arifi-fork-base | `cc1a2741c` | - | - | spec: ON-DEVICE speculative checkpoints - drafter now BEATS plain (8.02 vs 4.5 t/s, byte-clean) |
| 397 | `0397-spec-ring-rollback-re-enabled-DFlash2-depth-3-8.80-t.patch` | arifi-fork-base | `f75ce7aad` | - | - | spec: ring rollback re-enabled - DFlash2 depth 3 = 8.80 t/s (2.0x plain), 0 degenerate |
| 398 | `0398-rs-zero-on-clear-ROOT-FIX-cross-request-recurrent-st.patch` | prismml-recurrent | `5566b6f3d` | - | - | rs: zero-on-clear ROOT FIX - cross-request recurrent-state leak was the residual F-136 mechanism |
| 399 | `0399-tests-zero_gather-CPY-GET_ROWS-Vulkan-probe-cases-se.patch` | arifi-fork-base | `bb34f2996` | - | - | tests: zero_gather + CPY/GET_ROWS Vulkan probe cases; server: depth>cap clamps with warning instead of load abort (F-136 arc) |
| 400 | `0400-rs-zeroing-hunt-CLOSED-leak-is-backend-independent-C.patch` | arifi-fork-base | `9196ee9ce` | - | - | rs zeroing hunt CLOSED: leak is backend-independent (CPU repro), in-graph rs_z zero never lands on the live cache tensor; zero-on-clear promoted belt->contract. Env-gated discriminators: PDH hash, plane0/sl/rl belt scoping, zero-gather-out, hybrid set_input trace |
| 401 | `0401-vulkan-F-136-residual-ROOT-FIX-in-graph_optimize-is_.patch` | ternary-g128 | `2bd3edfa3` | - | - | vulkan: F-136 residual ROOT FIX in graph_optimize is_src_of - full view-chain roots + RAW/WAR hazards through node sources; reorder no longer hoists reads over aliased inplace writes (repeat repro: belt-off now 3/3 byte-identical) |
| 402 | `0402-arifi-sync-base-move-b10524-9ee9fc04c-b10636-4d19b28.patch` | arifi-fork-base | `fe5e1e98f` | - | - | arifi-sync: base move b10524/9ee9fc04c -> b10636/4d19b2876 (runbook 2.2 manual path); upstream pin advanced as the same pair; prisml + tq3 pins HELD with 3 reviews recorded |
| 403 | `0403-b10636-bump-repair-restore-upstream-s-DOTS3NOTE-inde.patch` | arifi-fork-base | `1d15ee76c` | - | - | b10636 bump repair: restore upstream's DOTS3NOTE indexer arch + the two TAG_LLAMA_SEQ_ID_NEG TODOs that a whole-file --theirs resolution reverted. checkout --theirs takes stage 3 (the WHOLE file), not just the conflicting hunks - upstream's 4-line b10524..b10636 delta to this file was lost with it. GLM_DSA is absent here PRE-EXISTING (also absent on lane165-pre-bump-arifi-main), so it is NOT restored by this commit and stays a President item. |
| 404 | `0404-b10636-bump-repair-2-close-llm_graph_input_attn_k_ds.patch` | arifi-fork-base | `4d644b7d9` | - | - | b10636 bump repair 2: close llm_graph_input_attn_k_dsa_iswa::can_reuse - the union resolution at ef642481d kept both sides' bodies but only one shared 'return res; }' tail, nesting every following definition inside it (build FAILED, 18 errors) |
| 405 | `0405-dflash-a-rejected-draft-checkpoint-image-must-not-ki.patch` | arifi-fork-base | `583e803bf` | github.com/spiritbuun/buun-llama-cpp@e332b2494 | - | dflash: a rejected draft checkpoint image must not kill the server process |
| 406 | `0406-seat-46-MSVC-portability-root-fixes-for-the-ROCm-HIP.patch` | arifi-fork-base | `dc2fd361d` | - | - | seat-46: MSVC portability root-fixes for the ROCm/HIP build (6 latent bugs: -fPIC on Windows, _MSC_VER fp16 macro parens, math.h/unistd.h guards, ssize_t->streamsize, dllimport on static lib) - build-rocm GREEN gfx1103, Vulkan build confirmed no-op |
| 407 | `0407-server-re-add-the-shared_draft_devices-VRAM-accounti.patch` | arifi-fork-base | `766ca387d` | - | - | server: re-add the shared_draft_devices VRAM accounting on top of common_fit_extra_model (multi-device only) |
| 408 | `0408-arifi-sync-base-move-b10636-4d19b2876-b10680-d7bd3bf.patch` | arifi-fork-base | `97b5c4f76` | - | - | arifi-sync: base move b10636/4d19b2876 -> b10680/d7bd3bfca (LATEST upstream tag, President mandate 2026-08-29); upstream pin advanced as one pair |
| 409 | `0409-vulkan-sync-check-follow-the-FA_TYPE-defines-into-b1.patch` | arifi-fork-base | `75c4d67f9` | - | - | vulkan sync-check: follow the FA_TYPE defines into b10680's new fa_types.glsl - reading only flash_attn_base.glsl found ZERO ids and silently turned checks 2 and 5 into no-ops (the exact F-110 class the guard exists for); now 16 FA_TYPE ids + 12 FA K/V types again |
| 410 | `0410-b10680-bump-repair-common-fit.cpp-still-called-llm_f.patch` | arifi-fork-base | `3ecc017f1` | - | - | b10680 bump repair: common/fit.cpp still called llm_ffn_exps_block_regex(idx), which b10680 renamed to llm_ffn_block_regex(idx, ffn_regex) - the fork's moe_cache code auto-merged past the rename and the build failed. Same body, LLM_FFN_EXPS_REGEX passed explicitly, so behaviour is identical. |
| 411 | `0411-b10680-bump-repair-2-my-hunk-1-union-in-the-3aa479a0.patch` | arifi-fork-base | `24750cb53` | - | - | b10680 bump repair 2: my hunk-1 union in the 3aa479a04 (support DFlash2) resolution kept selector_top_k from BOTH sides - duplicate member, build FAILED. One copy kept. The union trap the runbook already names, this time on a declaration rather than a shared block terminator. |
| 412 | `0412-runbook-4.1b-COLLISION-MAP-law-President-2026-08-29-.patch` | arifi-fork-base | `75212ca5a` | - | - | runbook 4.1b: COLLISION MAP law (President 2026-08-29) - never lose our patches; A/B only where upstream delta intersects our changes; collision list = required bump-report line |
| 413 | `0413-attribution-gap-fix-seat-48-re-key-the-sha-dead-prov.patch` | arifi-fork-base | `693f9ee39` | - | - | attribution gap-fix (seat-48): re-key the sha-dead provenance record + close the license/source-inventory gaps |
| 414 | `0414-runbook-2.2.1-RE-KEY-the-governed-sha-pointers-after.patch` | arifi-fork-base | `f24b83d8a` | - | - | runbook 2.2.1: RE-KEY the governed sha pointers after every base move - mechanize G1 so the class cannot silently return |
| 415 | `0415-attribution-wave-2-seat-48-zuijdwijk-is-a-LIVE-take-.patch` | arifi-fork-base | `d55298b75` | - | - | attribution wave 2 (seat-48): zuijdwijk is a LIVE take source, not a dormant remote - promote it, and record the G7 trailer debt instead of amending |
| 416 | `0416-G12-re-audit-seat-48-52-of-the-180-are-PROVEN-not-fo.patch` | arifi-fork-base | `31bb9e21f` | - | - | G12 re-audit (seat-48): 52 of the 180 are PROVEN not fork-authored - and none of them is unlicensed |
| 417 | `0417-R1-the-bank-plane-is-a-RUNTIME-INDEX-TENSOR-so-the-c.patch` | arifi-fork-base | `43705bf3d` | - | - | R1: the bank plane is a RUNTIME INDEX TENSOR, so the copies go AND graph reuse stays |
| 418 | `0418-R1-fix-guard-the-bank-index-tensors-on-ALLOCATION-no.patch` | arifi-fork-base | `369f2188c` | - | - | R1 fix: guard the bank index tensors on ALLOCATION, not on rs_r1 |
| 419 | `0419-lane-178-the-ring-s-extra-graph-split-is-the-SPLIT-I.patch` | arifi-fork-base | `ed7de13c2` | - | - | lane-178: the ring's extra graph split is the SPLIT-INPUTS CAP, not a CPU fallback - runtime-selectable so one binary is both arms |
| 420 | `0420-ggml-backend-adopt-split-inputs-cap-64-as-the-defaul.patch` | arifi-fork-base | `4b7379f86` | - | - | ggml-backend : adopt split-inputs cap 64 as the default |
| 421 | `0421-q2_0_g128-the-tie-predicate-is-exact-on-both-sides-n.patch` | ternary-g128 | `acbab1a4c` | - | - | q2_0_g128: the tie predicate is exact on both sides, not a reciprocal |
| 422 | `0422-lane-180-the-ring-s-26-extra-graph-inputs-are-23-IDE.patch` | prismml-recurrent | `17b329514` | - | - | lane-180: the ring's 26 extra graph inputs are 23 IDENTICAL s_wrow views - one per recurrent layer (delta-net-base.cpp:849), 4 bytes each; share one view behind LLAMA_RS_WROW_SHARE=1 |
| 423 | `0423-lane-180-GATE-0-receipt-report-eHostVisible-in-the-v.patch` | arifi-fork-base | `243bba4b7` | - | - | lane-180: GATE 0 receipt - report eHostVisible in the vk memory logger, not just eDeviceLocal |
| 424 | `0424-lane-180-ADOPT-share-ONE-s_wrow-view-by-default-29-i.patch` | arifi-fork-base | `1ef4b5a9b` | - | - | lane-180 ADOPT: share ONE s_wrow view by default - 29 identical per-layer split inputs collapsed |
| 425 | `0425-vulkan-perf-logger-must-not-abort-when-an-async-copy.patch` | ternary-g128 | `563dba74f` | - | - | vulkan: perf logger must not abort when an async copy left a compute context alive |
| 426 | `0426-lane-188-SHIPPING-KV-precision-tail-store-side-quant.patch` | arifi-fork-base | `2eb23e176` | 52c9acec Implement exact tails for quantized standard KV caches, a0a884b9 Optimize fully covered exact KV groups, 9f5850a0 Implement exact standard KV tail storage planning | - | lane-188: SHIPPING KV precision tail (store-side) - quantized body + exact F16 ring |
| 427 | `0427-lane-188-refuse-the-tail-on-MLA-caches-the-write-ord.patch` | arifi-fork-base | `6d2a5ced5` | - | - | lane-188: refuse the tail on MLA caches - the write-order detector cannot see the mla/lid idxs paths |
| 428 | `0428-lane-192-FIX-a-the-precision-tail-survives-a-prefix-.patch` | arifi-fork-base | `45d9a8bae` | - | - | lane-192 FIX (a): the precision tail survives a prefix-cache rewind |
| 429 | `0429-lane-194-q4_0-F16-Vulkan-cpy-shader-F16-KV-tail-comp.patch` | arifi-fork-base | `9bc2d1503` | - | - | lane-194: q4_0 -> F16 Vulkan cpy shader + F16 KV tail compose (OPT-IN, default OFF) |
| 430 | `0430-lane-194-the-CPU-reference-for-quant-F16-dup-which-d.patch` | arifi-fork-base | `32a781fac` | - | - | lane-194: the CPU reference for quant -> F16 dup, which did not exist |
| 431 | `0431-lane-194-name-the-compose-type-in-the-tail-banner-th.patch` | arifi-fork-base | `e94314d34` | - | - | lane-194: name the compose type in the tail banner - the receipt that stops a vacuous fidelity green |
| 432 | `0432-lane-195-multi-segment-flash-attention-the-ggml-API-.patch` | arifi-fork-base | `af5b7753c` | - | - | lane-195: multi-segment flash attention - the ggml API + CPU reference + its gate |
| 433 | `0433-lane-195-LEG-0-the-regression-leg-the-branch-was-mis.patch` | arifi-fork-base | `35785f324` | - | - | lane-195: LEG 0 - the regression leg the branch was missing, plus the sinks condition stated |
| 434 | `0434-lane-196-Design-B-BUILT-heterogeneous-split-k-FA-per.patch` | arifi-fork-base | `b160572e4` | - | - | lane-196: Design B BUILT - heterogeneous split-k FA (per-query source selection), the Vulkan dispatch |
| 435 | `0435-lane-196-promote-the-tail-ON-banner-to-WARN-llama-se.patch` | arifi-fork-base | `bae77d0a2` | - | - | lane-196: promote the tail-ON banner to WARN - llama-server drops INFO at default verbosity |
| 436 | `0436-lane-197-honor-spec-draft-p-min-in-the-DFlash2-sampl.patch` | arifi-fork-base | `0575ab003` | - | - | lane-197: honor --spec-draft-p-min in the DFlash2 sampled selector path - the gate was greedy-branch-only, so the seated serve line (temp 0.7) ignored the flag entirely; hoisted to the shared site, greedy duplicate removed, flag-unset path untouched (p_min defaults 0.0) |
| 437 | `0437-lane-198-gate-Q5_K-n-1-and-Q4_K-n-5-off-the-q8_1-MMV.patch` | arifi-fork-base | `28b56a336` | - | - | lane-198: gate Q5_K (n>1) and Q4_K (n>=5) off the q8_1 MMVQ mat-vec shader on AMD - measured on the live speculative-verify graph (780M, proprietary driver): MMVQ per-dispatch cost steps 2.0-2.3x (Q4_K) / 1.55x (Q5_K) from NUM_COLS 4 to 5 while the f32 dequant shader rises 1.4-1.6x, and Q5_K is faster on the f32 shader at every measured n>1 (0.73-0.88x). GGML_VK_FORCE_MMVQ / GGML_VK_DISABLE_MMVQ still override; n==1 path untouched. Evidence: research/local-inference/lane-evidence/2026-09-01-lane-198/TABLE.json |
| 438 | `0438-runbook-4.1b-duty-3-a-named-collision-is-decided-by-.patch` | arifi-fork-base | `54e25c113` | - | - | runbook 4.1b duty 3: a named collision is decided by MECHANISM READ + THREE ARMS (theirs / ours / union), never by one number - President 2026-09-02 |
| 439 | `0439-vulkan-tiled-concat-transpose-for-the-delta-net-conv.patch` | ternary-g128 | `a5bd0995a` | LaurentZuijdwijk/llama.cpp@1674aa4b4b70b1482c21a1b3881c4506191c4f6c | - | vulkan: tiled concat-transpose for the delta-net conv state |
| 440 | `0440-runbook-4.1-warn-that-series-regen-is-destructive-th.patch` | arifi-fork-base | `9ca43057e` | - | - | runbook 4.1: warn that `series regen` is destructive-then-abort, and require a pre-landing baseline |
| 441 | `0441-arifi-sync-root-fix-the-destructive-series-regen-and.patch` | arifi-fork-base | `c259de148` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `LANE110_PROF` | arifi-sync: root-fix the destructive series regen, and mechanize protected-win preservation |
| 442 | `0442-vulkan-label-the-concat-transpose-prior-art-numbers-.patch` | ternary-g128 | `5c1a0a7f8` | - | - | vulkan: label the concat-transpose prior-art numbers as upstream-reported, not ours |
| 443 | `0443-arifi-sync-name-the-incoming-ref-in-full-when-the-pr.patch` | arifi-fork-base | `d51df1750` | - | - | arifi-sync: name the incoming ref in full when the protected-win preflight refuses |
| 444 | `0444-arifi-sync-make-an-UNREGISTERED-measured-win-refuse-.patch` | arifi-fork-base | `00281ea43` | - | `GENERATE_EXPERT_BUNDLE`, `LANE110_PROF`, `GGML_ARIFI_VNNI_REPACK`, `MAX_N_CACHED` | arifi-sync: make an UNREGISTERED measured win refuse the update path |
| 445 | `0445-arifi-sync-close-the-two-live-false-negatives-in-the.patch` | arifi-fork-base | `3d174e706` | - | `GGML_ARIFI_VNNI_REPACK` | arifi-sync: close the two live false negatives in the protected-win gate, and mechanize the baseline boundary |
| 446 | `0446-arifi-sync-discharge-eleven-REGISTRATION-OWED-rows-a.patch` | arifi-fork-base | `974cc8ecd` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PROF` | arifi-sync: discharge eleven REGISTRATION OWED rows, and close two live holes in the guard itself |
| 447 | `0447-arifi-sync-search-anchors-in-SOURCE-only-so-the-mani.patch` | arifi-fork-base | `0639393e5` | - | - | arifi-sync: search anchors in SOURCE only, so the manifest cannot vouch for itself |
| 448 | `0448-arifi-sync-bring-the-manifest-s-own-_schema.anchors-.patch` | arifi-fork-base | `fe323a578` | - | - | arifi-sync: bring the manifest's own `_schema.anchors` fact up to the source-only rule |
| 449 | `0449-arifi-sync-recognize-correctness-only-as-no-effect-v.patch` | arifi-fork-base | `d8b9904bd` | - | - | arifi-sync: recognize `correctness only` as no-effect vocabulary |
| 450 | `0450-vulkan-create-the-ROCmFP4-FAST-mat-mat-pipelines-and.patch` | ternary-g128 | `ba5650057` | - | - | vulkan: create the ROCmFP4-FAST mat-mat pipelines and dequantize once per row in the generic mat-vec |
| 451 | `0451-vulkan-dedicated-IQ4_XS-mat-vec-shader-K-quant-style.patch` | ternary-g128 | `f9b779ca7` | - | - | vulkan: dedicated IQ4_XS mat-vec shader (K-quant style, 16 threads per superblock) + the 17408x5120 perf rows for test-backend-ops (lane-209 rank 5; gates run next) |
| 452 | `0452-vulkan-IQ3_S-mat-vec-16-invocations-per-superblock-a.patch` | ternary-g128 | `156d940e8` | - | - | vulkan: IQ3_S mat-vec 16 invocations per superblock at NUM_COLS >= 2 (zuijdwijk 211abf6a9 via lane-166, gate extended from >4; lane-209 rank 4; gates run next) |
| 453 | `0453-vulkan-IQ3_S-mat-vec-split-gate-becomes-NUM_COLS-3-N.patch` | ternary-g128 | `67516781f` | LaurentZuijdwijk/llama.cpp 211abf6a9 (via lane-166/matvec-vgpr-fix, never merged), gate widened. | - | vulkan: IQ3_S mat-vec split gate becomes (NUM_COLS == 3 \|\| NUM_COLS > 4); the stock widths keep the upstream body bit for bit |
| 454 | `0454-vulkan-create-the-ROCmFP4-FAST-q8_1-MMVQ-mat-vec-pip.patch` | ternary-g128 | `da3c87ebf` | rocmfpx/main mul_mat_vecq_funcs.glsl + its :4867/:4931 registrations and shaders-gen :752/:624 | - | vulkan: create the ROCmFP4-FAST q8_1 MMVQ mat-vec pipelines and gate them to n <= 5 on AMD |
| 455 | `0455-vulkan-keep-the-ROCmFP4-FAST-q8_1-MMVQ-path-to-n-2.5.patch` | ternary-g128 | `5213bd976` | - | - | vulkan: keep the ROCmFP4-FAST q8_1 MMVQ path to n=2..5 only |
| 456 | `0456-vulkan-enforce-ROCmFP4-MMVQ-width-gate-before-batch-.patch` | ternary-g128 | `fb734e5e2` | - | - | vulkan: enforce ROCmFP4 MMVQ width gate before batch routing |
| 457 | `0457-vulkan-exclude-ROCmFP4-MMVQ-n-4-tie-width.patch` | ternary-g128 | `8d0d74680` | - | - | vulkan: exclude ROCmFP4 MMVQ n=4 tie width |
| 458 | `0458-vulkan-isolate-ROCmFP4-MMVQ-to-n-3-and-n-5.patch` | ternary-g128 | `9f32ebf5f` | - | - | vulkan: isolate ROCmFP4 MMVQ to n=3 and n=5 |
| 459 | `0459-vulkan-correct-lane-209-backend-coverage-comment.patch` | ternary-g128 | `603d9155a` | - | - | vulkan: correct lane-209 backend coverage comment |
| 460 | `0460-speculative-size-drafts-from-measured-acceptance-spe.patch` | arifi-fork-base | `88413c9bf` | github.com/LaurentZuijdwijk/llama.cpp@ca2616991 | - | speculative: size drafts from measured acceptance (--spec-draft-adaptive) |
| 461 | `0461-server-do-not-re-verify-replayed-draft-tokens-after-.patch` | arifi-fork-base | `c3e065b4f` | github.com/LaurentZuijdwijk/llama.cpp@aeca25573 | - | server: do not re-verify replayed draft tokens after a checkpoint restore |
| 462 | `0462-vulkan-q6_k-mat-vec-reads-its-scales-from-the-block-.patch` | ternary-g128 | `e213bdd09` | - | - | vulkan: q6_k mat-vec reads its scales from the block, not through LDS - MEASURED NEUTRAL on gfx1103 |
| 463 | `0463-arifi-sync-register-the-three-lane-209-mechanisms-th.patch` | arifi-fork-base | `d29fcfe5f` | - | - | arifi-sync: register the three lane-209 mechanisms this integration lands |
| 464 | `0464-test-backend-ops-adversarial-MUL_MAT-data-patterns-f.patch` | arifi-fork-base | `64b0e3c35` | - | - | test-backend-ops: adversarial MUL_MAT data patterns for the ROCmFP4 MMVQ widths |
| 465 | `0465-vulkan-ROCmFP4-MMVQ-becomes-opt-in-GGML_ARIFI_ROCMFP.patch` | ternary-g128 | `6bf658f0b` | - | - | vulkan: ROCmFP4 MMVQ becomes opt-in (GGML_ARIFI_ROCMFP4_MMVQ=1) - kept in full, not yet cleared |
| 466 | `0466-arifi-sync-register-the-rank-7-ROCmFP4-MMVQ-mechanis.patch` | arifi-fork-base | `5f3d0de93` | - | - | arifi-sync: register the rank-7 ROCmFP4 MMVQ mechanism, default-OFF and all |
| 467 | `0467-docs-the-public-trail-for-the-b10680-kernel-wave-lan.patch` | arifi-fork-base | `3b6ffb063` | - | - | docs: the public trail for the b10680 kernel wave - lanes 202, 204 and 209 |
| 468 | `0468-docs-scope-the-power-plan-claim-to-the-runs-that-ban.patch` | arifi-fork-base | `7c243e7c3` | - | - | docs: scope the power-plan claim to the runs that banked a receipt, and fix two counts |
| 469 | `0469-docs-apply-the-six-independent-check-fixes-to-the-b1.patch` | arifi-fork-base | `07e866610` | - | - | docs: apply the six independent-check fixes to the b10680 kernel-wave trail |
| 470 | `0470-arifi-sync-base-move-b10680-d7bd3bfca-b10819-6a1a922.patch` | arifi-fork-base | `d068fcf51` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `LANE110_PROF`, `GENERATE_EXPERT_BUNDLE` | arifi-sync: base move b10680/d7bd3bfca -> b10819/6a1a922d2, and the governed sha re-key that follows it |
| 471 | `0471-b10819-sync-repair-six-merge-defects-that-stopped-th.patch` | arifi-fork-base | `cd5ce0aa7` | - | - | b10819 sync: repair six merge defects that stopped the tree from building |
| 472 | `0472-vulkan-tq_rotate-is-a-src1-reformat-restore-the-TQ-M.patch` | ternary-g128 | `9dcd8f623` | - | - | vulkan: tq_rotate is a src1 reformat - restore the TQ MUL_MAT_ID path |
| 473 | `0473-metal-union-the-dropped-upstream-FA-machinery-back-i.patch` | arifi-fork-base | `42ce1f0e9` | - | - | metal: union the dropped upstream FA machinery back in - the tree did not compile |
| 474 | `0474-cuda-union-upstream-s-mul_mat_id_needs_sync-predicat.patch` | arifi-fork-base | `ffeb40523` | - | - | cuda: union upstream's mul_mat_id_needs_sync predicate with our TQ arm |
| 475 | `0475-ci-a-backend-matrix-that-compiles-what-this-rig-cann.patch` | arifi-fork-base | `af67f085d` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `LANE110_PROF`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI` | ci: a backend matrix that compiles what this rig cannot, with our formats ON |
| 476 | `0476-arifi-sync-restore-the-2-space-JSON-indent-the-R17-r.patch` | arifi-fork-base | `ebc3629cb` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `LANE110_PROF`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI` | arifi-sync: restore the 2-space JSON indent the R17 register edits reformatted |
| 477 | `0477-ci-the-test-quantize-fns-gate-was-decorative-and-Met.patch` | arifi-fork-base | `2c6eae38b` | - | - | ci: the test-quantize-fns gate was decorative, and Metal needs both library modes |
| 478 | `0478-cuda-restore-the-fused-MoE-cache-matvec-on-upstream-.patch` | arifi-fork-base | `cccea939e` | - | - | cuda: restore the fused MoE-cache matvec on upstream's fusion kernel (WI-1699) |
| 479 | `0479-ci-make-the-link-contract-real-and-run-test-backend-.patch` | arifi-fork-base | `a1e4d1bf6` | - | - | ci: make the link contract real, and run test-backend-ops on the CPU job |
| 480 | `0480-arifi-sync-R13B-s-two-b10819-repair-commits-become-a.patch` | arifi-fork-base | `58c15eb90` | - | - | arifi-sync: R13B's two b10819 repair commits become also_commits of the mechanisms they keep alive |
| 481 | `0481-sycl-the-MMVQ-launchers-declared-one-pair-of-ranges-.patch` | arifi-fork-base | `14de968ab` | - | - | sycl: the MMVQ launchers declared one pair of ranges and launched with another |
| 482 | `0482-ci-each-job-now-proves-the-unioned-file-itself-produ.patch` | arifi-fork-base | `3cf1ede05` | - | - | ci: each job now proves the unioned file itself produced an object |
| 483 | `0483-metal-replace-the-garbled-library-loader-with-upstre.patch` | arifi-fork-base | `de85e39cb` | - | - | metal: replace the garbled library loader with upstream's per-kind loader plus an ArifiLabs library kind |
| 484 | `0484-ci-the-cpu-ubuntu-job-runs-protected-win-validate-so.patch` | arifi-fork-base | `039a72fb3` | - | - | ci: the cpu-ubuntu job runs protected-win validate, so the manifest is gated where the code is |
| 485 | `0485-arifi-sync-protected-win-learns-a-symbols-class-a-re.patch` | arifi-fork-base | `79125644b` | - | - | arifi-sync: protected-win learns a symbols class - a registered kernel entry point must have a DEFINITION, not a mention (WI-1700) |
| 486 | `0486-arifi-sync-the-symbols-guard-ignores-commented-out-b.patch` | arifi-fork-base | `d2d95ed37` | - | - | arifi-sync: the symbols guard ignores commented-out bodies, and the CUDA/Metal seed gaps the check found are closed (WI-1700 FIX-1/FIX-2) |
| 487 | `0487-docs-put-the-integration-A-B-numbers-into-the-public.patch` | arifi-fork-base | `3da652704` | - | - | docs: put the integration A/B numbers into the public trail, and record the b10819 base move |
| 488 | `0488-docs-state-the-power-plan-and-RAM-as-pre-registered-.patch` | arifi-fork-base | `19f5dafd0` | - | - | docs: state the power plan and RAM as pre-registered conditions, not per-launch receipts |
| 489 | `0489-arifi-sync-sources.json-post_move_tip-re-keyed-to-th.patch` | arifi-fork-base | `dc3bc57b5` | - | - | arifi-sync: sources.json post_move_tip re-keyed to the lane-214 integration tip |
| 490 | `0490-ggml-flash-attention-segment-count-moves-to-op_param.patch` | arifi-fork-base | `62014225f` | - | - | ggml: flash-attention segment count moves to op_params[5] - it collided with upstream's n_kv_max in [4] |
| 491 | `0491-vulkan-FA-dequant-KV-f16-scratch-is-a-runtime-gate-d.patch` | ternary-g128 | `63bf739ba` | - | - | vulkan: FA dequant-KV f16 scratch is a runtime gate, default ON on every device (upstream behaviour) - GGML_ARIFI_FA_DEQUANT_KV overrides both ways |
| 492 | `0492-arifi-sync-base-move-b10819-6a1a922d2-b10825-9e0e220.patch` | arifi-fork-base | `f83b62b29` | - | - | arifi-sync: base move b10819/6a1a922d2 -> b10825/9e0e22059 (R22 read, R27 execution); 552/552 replayed clean, 0 merges, 0 hunk collisions, marker scan 0, protected wins PASS; R20 slot fix + R19 gate (default ON per R19D) carried Lane: lane-220 |
| 493 | `0493-arifi-sync-rekey-3-protected-win-manifests-to-the-b1.patch` | arifi-fork-base | `1a6925977` | - | - | arifi-sync: rekey 3 protected-win manifests to the b10825 line (R27 check 05:1x: validate PASS only with this rekey; committed so the built line validates) Lane: lane-220 |
| 494 | `0494-arifi-sync-scope-subcommand-F-161-the-ingest-A-B-sco.patch` | arifi-fork-base | `ce5d4046e` | - | - | arifi-sync: scope subcommand (F-161) - the ingest A/B scope derived from the collision map (upstream files x our files x hunk overlap x speed paths) = sanity\|named\|full; UPDATE-RUNBOOK 4.1b.4 scope law; selfcheck Lane: lane-220 |
| 495 | `0495-provenance-route-B-G7-append-only-R24-s-58-classific.patch` | arifi-fork-base | `c9075b246` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | provenance route B (G7 append-only): R24's 58 classifications recorded in native-grandfather (55 ARIFI, remapped to the b10825 shas) + pending-trailers (2 FORK, 1 UPSTREAM); 46 fork-authored tooling/manifest commits grandfathered; provenance PASS; series regen 496 patches |
| 496 | `0496-spec-runtime-switch-for-the-DFlash-encoder-fusion-LL.patch` | arifi-fork-base | `4fea45e7e` | - | - | spec: runtime switch for the DFlash encoder fusion (LLAMA_DFLASH_FUSED_INJECT), both injection paths kept, default LEGACY on this fork |
| 497 | `0497-vulkan-RDNA3-mul_mat_vec_id-rows-runtime-selectable-.patch` | ternary-g128 | `6833c83bc` | - | - | vulkan: RDNA3 mul_mat_vec_id rows runtime-selectable (GGML_ARIFI_MMV_ID_ROWS) + Ornith expert-shape perf rows |
| 498 | `0498-kv-cache-the-PLE-n-gram-history-lookup-becomes-a-run.patch` | arifi-fork-base | `7788c6102` | - | - | kv-cache: the PLE n-gram history lookup becomes a runtime switch - seq_pos index by default, the cell scan behind LLAMA_KV_NGRAM_INDEX=0 |
| 499 | `0499-vulkan-MUL_MAT_ID-B-staging-receipt-and-a-loud-asser.patch` | ternary-g128 | `7b949bc57` | - | - | vulkan: MUL_MAT_ID B-staging receipt, and a loud assert on the tq_rotate x K-padding hazard |
| 500 | `0500-vulkan-UMA-read-back-takes-the-direct-memcpy-only-wh.patch` | ternary-g128 | `ae805d1f9` | - | - | vulkan: UMA read-back takes the direct memcpy only when the mapping is HOST_CACHED (WI-1693 fix A) |
| 501 | `0501-ggml-add-GGML_TYPE_SX8-57-the-S-X8-v4.3-CPU-decoder-.patch` | arifi-fork-base | `cb36687a0` | MarlaLabs llama-cpp-sx8.patch (github.com/MarlaLabs, Apache-2.0), cloned at cache/r16-sx8 | - | ggml: add GGML_TYPE_SX8 (57), the S-X8 v4.3 CPU decoder, retagged from the author's 41, plus its retag/cross-check tooling |
| 502 | `0502-ggml-port-jtrefon-TBQ3_0-TBQ4_0-at-ids-58-59-with-th.patch` | arifi-fork-base | `e832c80a9` | jtrefon/llama.cpp-turboq-mtp@6a02d0494 (ggml/src/ggml-turboq.c, ggml-turboq-tables.h, ggml-common.h, ggml.c, ggml-cpu, src/llama-kv-cache.cpp, src/llama-graph.cpp, src/llama-context.cpp, common/arg.cpp, tests) | - | ggml: port jtrefon TBQ3_0/TBQ4_0 at ids 58/59 with the type-id arbitration, plus the SET_ROWS device guard (WI-1694 half A) |
| 503 | `0503-docs-protected-wins-the-R18-switch-table-eight-DOCUM.patch` | arifi-fork-base | `070c6c71d` | - | - | docs + protected-wins: the R18 switch table - eight DOCUMENTED-TAKE runtime switches registered with their measured effect |
| 504 | `0504-vulkan-add-the-S-X8-v4.3-kernel-for-GGML_TYPE_SX8-57.patch` | arifi-fork-base | `77f825fd8` | MarlaLabs S-X8 v4.3 decode arithmetic (carried into the fork by WI-1692, repeated here in GLSL) | - | vulkan : add the S-X8 v4.3 kernel for GGML_TYPE_SX8 (57) |
| 505 | `0505-vulkan-integer-MMQ-Q8_1-activations-for-GGML_TYPE_SX.patch` | arifi-fork-base | `40347768b` | MarlaLabsAI/sx8-quantization docs/S-X-METHODOLOGY.md section 2 (per-8 affine sub-block decomposition), Apache-2.0 | - | vulkan : integer MMQ (Q8_1 activations) for GGML_TYPE_SX8 (57) - matmul_sx8_q8_1, CREATE_MMQ, per-8 activation sums hoisted into block_b_to_registers (8 dots + 8 FMAs per 32 weights) |
| 506 | `0506-vulkan-mul_mm-prompt-processing-for-GGML_TYPE_SX8-57.patch` | arifi-fork-base | `4cfca125b` | MarlaLabsAI/sx8-quantization docs/S-X-METHODOLOGY.md section 2 (Apache-2.0) - the S-X8 affine decomposition (per-block fp16 endpoints, four 2-bit sub-block range strategies, 6-bit levels split across qh nibbles and ql quads). | - | vulkan : mul_mm (prompt processing) for GGML_TYPE_SX8 (57) - the device said the gap is the missing mul_mm, not the missing MMQ |
| 507 | `0507-server-device-resident-context-checkpoint-ring-tool-.patch` | arifi-fork-base | `16f8ff2de` | - | - | server : device-resident context checkpoint ring + tool-call anchor (R31 P1/P2, FreeToken M14/M15) |
| 508 | `0508-llama-Windows-unbuffered-model-reads-load-mode-direc.patch` | arifi-fork-base | `f85706ebb` | - | - | llama : Windows unbuffered model reads (--load-mode direct_io) + PR #28223 host bufts under mmap (R31 M17 / M07) |
| 509 | `0509-tests-test-save-load-state-Test-9-device-storage-rin.patch` | arifi-fork-base | `89737a7db` | - | - | tests: test-save-load-state Test 9 (device storage ring) - the first draft read logits the ring never stores and went RED on the ring code itself; the repaired test checks the restored state, GREEN on cdff32828 (R31 checker F1) |
| 510 | `0510-server-llama-device-checkpoint-storage-ID-ownership-.patch` | arifi-fork-base | `0621d6a4e` | - | - | server/llama : device checkpoint storage-ID ownership + quantized device-view extent (R45, lane-229) |
| 511 | `0511-server-LLAMA_CKPT_STORAGE_TRACE-one-INFO-line-per-ch.patch` | arifi-fork-base | `f61eb424d` | - | - | server : LLAMA_CKPT_STORAGE_TRACE - one INFO line per checkpoint save and restore (R45, lane-229) |
| 512 | `0512-test-backend-ops-R46-real-27B-S-X8-decode-shapes-174.patch` | arifi-fork-base | `348f38ca2` | - | - | test-backend-ops: R46 real 27B S-X8 decode shapes (17408x5120, 5120x17408, 248320x5120) for sx8/q8_0/q4_K at n=1..8 |
| 513 | `0513-vulkan-S-X8-mat-vec-decodes-a-whole-32-weight-block-.patch` | ternary-g128 | `9f4c3e926` | - | - | vulkan: S-X8 mat-vec decodes a whole 32-weight block per thread (R46 ranks 2+4) |
| 514 | `0514-vulkan-gate-the-S-X8-whole-block-mat-vec-decode-at-n.patch` | ternary-g128 | `3f426168b` | - | - | vulkan: gate the S-X8 whole-block mat-vec decode at n <= 3 (R46 commit B2) |
| 515 | `0515-vulkan-S-X8-integer-dot-mat-vec-against-Q8_1-activat.patch` | ternary-g128 | `2ba37051e` | - | - | vulkan: S-X8 integer-dot mat-vec against Q8_1 activations (R46 commit C) |
| 516 | `0516-vulkan-S-X8-q8_1-mat-vec-ships-OPT-IN-GGML_ARIFI_SX8.patch` | ternary-g128 | `4c7ea8edb` | - | - | vulkan: S-X8 q8_1 mat-vec ships OPT-IN, GGML_ARIFI_SX8_MMVQ=1 (R46 commit C decision) |
| 517 | `0517-vulkan-GGML_VK_ALLOC_TRACE-allocation-submit-instrum.patch` | arifi-fork-base | `2f46d74ae` | - | - | vulkan : GGML_VK_ALLOC_TRACE allocation/submit instrument + S-X8 mat-vec rows/workgroup switch (R47a, lane-232) |
| 518 | `0518-vulkan-alloc-trace-reads-per-heap-live-bytes-under-t.patch` | arifi-fork-base | `cbcbd447c` | - | - | vulkan : alloc-trace reads per-heap live bytes under the lock (R47a, lane-232) |
| 519 | `0519-vulkan-the-R47d-780M-S-X8-mat-vec-shape-lookup-as-a-.patch` | ternary-g128 | `f1f77b876` | - | - | vulkan: the R47d 780M S-X8 mat-vec shape lookup as a device-probed default (R46b commit D) |
| 520 | `0520-vulkan-opt-in-explicit-placement-policy-GGML_VK_PLAC.patch` | ternary-g128 | `c78dc8593` | - | - | vulkan: opt-in explicit placement policy GGML_VK_PLACEMENT=bulk-large-heap (R46b commit E) |
| 521 | `0521-test-backend-ops-S-X8-ragged-m-K-tail-f16-activation.patch` | arifi-fork-base | `3181f30b8` | - | - | test-backend-ops: S-X8 ragged-m, K-tail, f16-activation and real-27B-shape mat-vec correctness (R46b, checker F2) |
| 522 | `0522-test-backend-ops-drop-the-S-X8-m-1-correctness-cases.patch` | arifi-fork-base | `f08dd0a9a` | - | - | test-backend-ops: drop the S-X8 m=1 correctness cases, they are not comparable (R46b commit G) |
| 523 | `0523-test-vulkan-add-independent-S-X8-route-checks.patch` | arifi-fork-base | `11a85ec23` | - | - | test(vulkan): add independent S-X8 route checks |
| 524 | `0524-feat-vulkan-account-heap-reservations-transactionall.patch` | arifi-fork-base | `8aa5ef3cf` | - | - | feat(vulkan): account heap reservations transactionally |
| 525 | `0525-feat-backend-preflight-Vulkan-buffer-type-batches.patch` | arifi-fork-base | `635df85aa` | - | - | feat(backend): preflight Vulkan buffer-type batches |
| 526 | `0526-feat-alloc-loader-wide-Vulkan-allocation-transaction.patch` | arifi-fork-base | `328385ebb` | - | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | feat(alloc): loader-wide Vulkan allocation transaction (R46b B7b) |
| 527 | `0527-feat-vulkan-bounded-host-split-with-explicit-staging.patch` | arifi-fork-base | `88b188b31` | - | - | feat(vulkan): bounded host split with explicit staging reserve (R46b B7c) |
| 528 | `0528-fix-checkpoint-make-device-finalization-fallible.patch` | arifi-fork-base | `f56b7451a` | - | - | fix(checkpoint): make device finalization fallible |
| 529 | `0529-feat-updater-preserve-fork-work-across-rebases.patch` | arifi-fork-base | `a0c408f8f` | - | - | feat(updater): preserve fork work across rebases |
| 530 | `0530-arifi-sync-regenerate-the-patch-series-at-the-R46i-t.patch` | arifi-fork-base | `3d64ac823` | - | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | arifi-sync: regenerate the patch series at the R46i tip; grandfather the nine untrailered R46i-line commits |
| 531 | `0531-gitattributes-evidence-receipt-files-are-byte-exact-.patch` | arifi-fork-base | `bf2ce0742` | - | - | gitattributes: evidence receipt files are byte-exact (-text) |
| 532 | `0532-arifi-sync-grandfather-the-untrailered-series-regen-.patch` | arifi-fork-base | `115f4e5a4` | - | - | arifi-sync: grandfather the untrailered series-regen commit 833881db4 |
| 533 | `0533-arifi-sync-series-replay-applies-with-core.autocrlf-.patch` | arifi-fork-base | `58de85ad0` | - | - | arifi-sync: series replay applies with core.autocrlf=false so replayed bytes equal the committed blobs |
| 534 | `0534-arifi-sync-series-replay-keeps-CR-keep-cr-evidence-r.patch` | arifi-fork-base | `42de753fe` | - | - | arifi-sync: series replay keeps CR (--keep-cr); evidence receipts restored to their original LF bytes |
| 535 | `0535-feat-vulkan-MMVQ-A-side-decode-hoist-behind-spec-con.patch` | arifi-fork-base | `c2b438cfa` | - | - | feat(vulkan): MMVQ A-side decode hoist behind spec constant (R48b) |
| 536 | `0536-docs-r48b-report-section-10-reconstruction-rc-receip.patch` | arifi-fork-base | `98cd65e48` | - | - | docs(r48b): report section 10 (reconstruction) + rc receipts |
| 537 | `0537-docs-r48b-HQ-planted-red-receipts-on-the-committed-h.patch` | arifi-fork-base | `42375fb51` | - | - | docs(r48b): HQ planted-red receipts on the committed hoist (RED-1 both arms n=1..4, RED-2 ON-only n=2..4) |
| 538 | `0538-WIP-r48b-phase-2-hoist-type-width-gate-MMVQ-routing-.patch` | arifi-fork-base | `993ad050a` | - | - | WIP(r48b phase 2): hoist type/width gate + MMVQ routing draft (maker output, unreviewed, unmeasured) |
| 539 | `0539-docs-r48b-phase-2-gate-routing-verified-correctness-.patch` | arifi-fork-base | `21b68eb74` | - | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | docs(r48b): phase 2 gate + routing verified, correctness receipts, report section 11 |
| 540 | `0540-WIP-r48c-Q6_K-mat-vec-x-fold-activation-hoist-draft-.patch` | arifi-fork-base | `5b9f31eec` | - | - | WIP(r48c): Q6_K mat-vec x-fold + activation hoist draft (maker output, unreviewed, unmeasured) |
| 541 | `0541-docs-r48c-clear-the-Q6_K-shader-of-HQ-finding-1-loca.patch` | arifi-fork-base | `d1b8aebaf` | - | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | docs(r48c): clear the Q6_K shader of HQ finding 1; localise a pre-existing defect |
| 542 | `0542-docs-r48c-independent-Fable-LOW-check-PASS-GATED-its.patch` | arifi-fork-base | `1fe947413` | - | - | docs(r48c): independent Fable LOW check PASS-GATED + its three receipts (upstream-path 3/60, S-X8 0/60, direct-scales arms 43/43) |
| 543 | `0543-docs-r48c-close-the-Fable-LOW-gates-measured-registr.patch` | arifi-fork-base | `ea298363e` | - | - | docs(r48c): close the Fable LOW gates - measured registry rows, corrected coverage and failure-signature claims |
| 544 | `0544-feat-vulkan-Q5_K-mat-vec-activation-hoist-behind-spe.patch` | arifi-fork-base | `8e7ac9e94` | - | - | feat(vulkan): Q5_K mat-vec activation hoist behind spec constant (R48 phase 1) |
| 545 | `0545-docs-r48-Q5_K-report-section-10-reconstruction-HQ-pl.patch` | arifi-fork-base | `40a526a19` | - | - | docs(r48): Q5_K report section 10 (reconstruction) + HQ planted-red summary and rc receipts |
| 546 | `0546-feat-vulkan-gate-Q5_K-mat-vec-activation-hoist-to-NU.patch` | arifi-fork-base | `3bd085b08` | - | - | feat(vulkan): gate Q5_K mat-vec activation hoist to NUM_COLS<=3 (R48 phase 1b) |
| 547 | `0547-docs-r48-name-the-phase-1b-commit-SHA-and-downgrade-.patch` | arifi-fork-base | `d43f74f7a` | - | - | docs(r48): name the phase-1b commit SHA and downgrade the f16 gate claim |
| 548 | `0548-arifi-sync-series-replay-checks-out-its-worktree-wit.patch` | arifi-fork-base | `242a44a3b` | - | - | arifi-sync: series replay checks out its worktree with autocrlf=false (index LF vs worktree CRLF killed patch 283/550) |
| 549 | `0549-vulkan-R52b-S-X8-mul_mm-PACKED-tile-decode-and-integ.patch` | arifi-fork-base | `2d5b6b2c9` | - | - | vulkan : R52b — S-X8 mul_mm PACKED tile decode, and integer MMQ reachable under coopmat (opt-in) |
| 550 | `0550-docs-r52b-the-three-arm-diagnostic-MMQ-under-coopmat.patch` | arifi-fork-base | `1ff06a694` | - | - | docs(r52b): the three-arm diagnostic — MMQ under coopmat LOSES, the packed decode wins 2.2%, and S-X8 mul_mm already beats q8_0 at this shape |
| 551 | `0551-fix-r52b-perf-mode-supersedes-the-single-dispatch-nu.patch` | arifi-fork-base | `50a9d4916` | - | - | fix(r52b): perf-mode supersedes the single-dispatch numbers — MMQ under coopmat WINS for q8_0 and loses for S-X8, and the packed decode is a TIE |
| 552 | `0552-docs-r52b-correct-the-perf-case-list-line-citation-1.patch` | arifi-fork-base | `b3233140c` | - | - | docs(r52b): correct the perf case-list line citation (12042-12049) |
| 553 | `0553-docs-r52b-close-the-PASS-GATED-docs-gate-the-coverag.patch` | arifi-fork-base | `7d0c7ce27` | - | - | docs(r52b): close the PASS-GATED docs gate — the coverage claim was overstated and the build log was never captured |
| 554 | `0554-docs-r52b-re-point-the-two-test-case-list-citations-.patch` | arifi-fork-base | `89539e2ac` | - | - | docs(r52b): re-point the two test-case-list citations that this lane's own comment growth shifted |
| 555 | `0555-feat-sx8-recover-the-S-X8-v4.3-PCA-correction-that-G.patch` | arifi-fork-base | `8d8c8ca89` | - | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | feat(sx8): recover the S-X8 v4.3 PCA correction that GGUF conversion drops (lane-242/R53) |
| 556 | `0556-fix-sx8-close-the-R53-check-gate-same-run-verifier-r.patch` | arifi-fork-base | `112470efb` | - | - | fix(sx8): close the R53 check gate -- same-run verifier receipt + companion identity guard (lane-242) |
| 557 | `0557-WIP-r51-host-split-order-mechanism-plan-order-trace-.patch` | arifi-fork-base | `c43f09c9c` | - | - | WIP r51: host-split order mechanism + plan-order trace; token_embd proven absent from the Vulkan plan |
| 558 | `0558-vulkan-name-the-buffers-a-bounded-host-split-places-.patch` | ternary-g128 | `c17b38c36` | - | - | vulkan: name the buffers a bounded host split places, and refute the read-cost order |
| 559 | `0559-docs-lane-240-R51-check-gate-cache_r_l0-relabelled-a.patch` | prismml-recurrent | `619dc3a9b` | - | - | docs(lane-240 R51): check gate — cache_r_l0 relabelled as the target recurrent state (device-local, hot), withdrawn prediction figure removed |
| 560 | `0560-feat-vulkan-choose-the-host-split-memory-type-by-mea.patch` | arifi-fork-base | `c0f6d3c08` | - | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | feat(vulkan): choose the host-split memory type by measurement, not by index order |
| 561 | `0561-docs-r58-name-the-regime-and-the-receipt-line-assert.patch` | arifi-fork-base | `7d6e94b9f` | - | - | docs(r58): name the regime and the receipt-line assertion the reorder needs |
| 562 | `0562-test-r58-phase-B-proof-cells-the-reorder-runs-the-ou.patch` | arifi-fork-base | `f4a0d6352` | - | - | test(r58): phase B proof cells - the reorder runs, the output does not move |
| 563 | `0563-series-regen-at-the-R56-tip-565-entries-G7-rows-for-.patch` | arifi-fork-base | `0232ea0c0` | - | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | series: regen at the R56 tip (565 entries) + G7 rows for two untrailered lane commits |
| 564 | `0564-test-lane-246-R60-paired-S-X8-q8_0-mul_mm-coverage-a.patch` | arifi-fork-base | `cddff4b48` | - | - | test(lane-246 R60): paired S-X8/q8_0 mul_mm coverage across the small-n band, and the two refutations it produced |
| 565 | `0565-feat-vulkan-make-the-mat-vec-admit-width-runtime-sel.patch` | arifi-fork-base | `c3ed6236b` | - | - | feat(vulkan): make the mat-vec admit width runtime-selectable, default inert |
| 566 | `0566-test-r57-three-k-two-effects-the-width-4-knee-moves-.patch` | arifi-fork-base | `62a2e4531` | - | - | test(r57): three k, two effects - the width-4 knee moves, the width-7 cliff does not |
| 567 | `0567-docs-r57-registry-rows-for-the-admit-switch-and-for-.patch` | arifi-fork-base | `ac525a678` | - | - | docs(r57): registry rows for the admit switch and for the n=7 cliff it uncovered |
| 568 | `0568-test-r57-the-decider-answers-past-the-mat-vec-bounda.patch` | arifi-fork-base | `6064d93cc` | - | - | test(r57): the decider answers - past the mat-vec boundary a verify column is ~46x cheaper |
| 569 | `0569-fix-vulkan-restore-the-admit-width-receipt-line-the-.patch` | arifi-fork-base | `636558ae8` | - | - | fix(vulkan): restore the admit-width receipt line the base check discarded |
| 570 | `0570-docs-r57-fold-the-Fable-check-s-corrections-into-the.patch` | arifi-fork-base | `49961fdcd` | - | - | docs(r57): fold the Fable check's corrections into the report and the registry |
| 571 | `0571-feat-r64-GGML_ARIFI_Q5K_MMVQ-lever-the-two-hoists-fi.patch` | arifi-fork-base | `b9d0b4de8` | - | - | feat(r64): GGML_ARIFI_Q5K_MMVQ lever + the two-hoists find + step-0(c) counts |
| 572 | `0572-fix-r64-the-q5_K-admit-is-n-5-not-n-2.8-the-measurem.patch` | arifi-fork-base | `b93613485` | - | - | fix(r64): the q5_K admit is n>=5, not n=2..8 - the measurement narrowed it |
| 573 | `0573-feat-r64-the-q5_K-MMVQ-route-lands-proof-null-contro.patch` | arifi-fork-base | `d0edc90a9` | - | - | feat(r64): the q5_K MMVQ route lands - proof, null control, registry rows |
| 574 | `0574-fix-r64-withdraw-the-repeatable-at-every-width-claim.patch` | arifi-fork-base | `a0dd546da` | - | - | fix(r64): withdraw the "repeatable at every width" claim on the k=17408 arm |
| 575 | `0575-docs-r64-Fable-check-gates-corrected-counts-paired-c.patch` | arifi-fork-base | `bf27706b0` | - | - | docs(r64): Fable-check gates - corrected counts, paired cites, generators, served pairing |
| 576 | `0576-feat-vulkan-r65-a-rows-per-workgroup-knob-on-the-pla.patch` | arifi-fork-base | `3a49c5eb9` | - | - | feat(vulkan,r65): a rows-per-workgroup knob on the plain MMVQ path, and the rows the knee test is read off |
| 577 | `0577-test-r65-the-width-4-knee-is-width-locked-not-a-capa.patch` | arifi-fork-base | `df6e77e2e` | - | - | test(r65): the width-4 knee is width-locked, not a capacity - REFUTATION |
| 578 | `0578-test-r61-iq3_s-iq3_xxs-iq4_xs-rows-on-the-shapes-the.patch` | arifi-fork-base | `e0a94115b` | - | - | test(r61): iq3_s/iq3_xxs/iq4_xs rows on the shapes the GSQ file serves, plus a mat-vec pipeline-identity trace |
| 579 | `0579-feat-r61-hoist-the-iq3-sign-application-out-of-the-N.patch` | arifi-fork-base | `35058539c` | - | - | feat(r61): hoist the iq3 sign application out of the NUM_COLS loop, behind GGML_ARIFI_IQ3_MMVQ |
| 580 | `0580-fix-r61-the-dump-s-same-arm-control-FAILED-and-it-ca.patch` | arifi-fork-base | `85e321ae9` | - | - | fix(r61): the dump's same-arm control FAILED, and it caught a real defect in the method |
| 581 | `0581-docs-r61-three-registry-rows-for-the-R61-step-2-vari.patch` | arifi-fork-base | `2edf5cac0` | - | - | docs(r61): three registry rows for the R61 step-2 variables |
| 582 | `0582-chore-r61-commit-every-raw-receipt-and-the-wrappers-.patch` | arifi-fork-base | `cd1e2d3ae` | - | - | chore(r61): commit every raw receipt and the wrappers that actually produced them |
| 583 | `0583-feat-vulkan-lane-254-R66-three-default-flips-q5_K-ro.patch` | arifi-fork-base | `b6e9accfc` | - | - | feat(vulkan,lane-254 R66): three default flips - q5_K route on AMD, iq3 sign-hoist everywhere, iq3 n=7 rows on RDNA3 |
| 584 | `0584-vulkan-measure-the-q6_K-q8_1-MMVQ-route-at-verify-wi.patch` | ternary-g128 | `2d4a8050b` | - | - | vulkan: measure the q6_K q8_1 MMVQ route at verify widths, ship it as a switch, keep the default |
| 585 | `0585-vulkan-q6_K-MMVQ-route-DEFAULT-ON-for-n-7-8-on-AMD-s.patch` | ternary-g128 | `c780814eb` | - | - | vulkan: q6_K MMVQ route DEFAULT ON for n=7,8 on AMD; strike the n=2,3 clause the duplicate-case parser manufactured |
| 586 | `0586-vulkan-q6_K-correct-the-R71b-route-admission-comment.patch` | arifi-fork-base | `15ecccad4` | - | - | vulkan q6_K: correct the R71b route-admission comment to the CHECK-R73 record |
| 587 | `0587-vulkan-q6_K-GGML_ARIFI_Q6K_F32_ROWS-R74-step-0-drive.patch` | arifi-fork-base | `b5f6f00f9` | - | - | vulkan q6_K: GGML_ARIFI_Q6K_F32_ROWS + R74 step 0 driver statistics + pre-registration |
| 588 | `0588-vulkan-q6_K-GGML_ARIFI_Q6K_MMVQ_ROWS-the-q6_K-scoped.patch` | arifi-fork-base | `65906d3b9` | - | - | vulkan q6_K: GGML_ARIFI_Q6K_MMVQ_ROWS, the q6_K-scoped integer-dot rows knob |
| 589 | `0589-R74-report-the-q6_K-width-cliffs-are-REFUTED-at-the-.patch` | arifi-fork-base | `cb25daea8` | - | - | R74 report: the q6_K width cliffs are REFUTED at the served width, with the mechanism named |
| 590 | `0590-R74b-WIP-resumed-after-box-freeze-exact-n-q6_K-MMVQ-.patch` | arifi-fork-base | `7aed5a952` | - | - | R74b WIP (resumed after box freeze): exact-n q6_K MMVQ rows knob, RDNA3 1@n6 default, route n=6 (m,k) cells, registry rows |
| 591 | `0591-vulkan-q6_K-fence-the-R74b-n-6-route-cells-to-RDNA3-.patch` | arifi-fork-base | `eaf82069c` | - | - | vulkan q6_K: fence the R74b n=6 route cells to RDNA3 (off RDNA3 rm_int_n is already 1 row) |
| 592 | `0592-vulkan-q6_K-ship-the-R74b-n-6-MMVQ-route-cell-5120x6.patch` | arifi-fork-base | `316f2006d` | - | - | vulkan q6_K: ship the R74b n=6 MMVQ route cell 5120x6144 ON (RDNA3, rows 1 at NUM_COLS 6 exactly) |
| 593 | `0593-vulkan-q6_K-R74b-startup-route-line-names-RDNA3-text.patch` | arifi-fork-base | `6d3275f85` | - | - | vulkan q6_K: R74b startup route line names RDNA3 (text only, no behavior change) + final-binary re-green + R49 side-finding |
| 594 | `0594-WIP-R85-q4_K-w5-arms-C1-next-slice-prefetch-C2-rows-.patch` | arifi-fork-base | `791bfcbfb` | - | - | WIP R85: q4_K w5 arms C1 (next-slice prefetch), C2 (rows=2), C3 (4+1 column split) behind default-off env switches; decider + pre-screen scripts |
| 595 | `0595-R85-ship-the-C3-4-1-column-split-for-q4_K-MMVQ-at-th.patch` | arifi-fork-base | `bc5b0b3af` | - | - | R85: ship the C3 4+1 column split for q4_K MMVQ at the measured-winning cells (n=5,6; m>=131072 or m<=1024; k<=8192), device-probed ON on RDNA3, GGML_ARIFI_Q4K_W5_SPLIT=0\|1 |
| 596 | `0596-R87-ship-iq3_s-NUM_COLS-6-mat-vec-at-2-rows-per-work.patch` | arifi-fork-base | `31dade044` | - | - | R87: ship iq3_s NUM_COLS=6 mat-vec at 2 rows per workgroup (was 4), device-probed on RDNA3, GGML_ARIFI_IQ3S_N6_ROWS=1\|2\|4 overrides |
| 597 | `0597-R87-correction-iq3_s-n-6-rows-knob-narrowed-to-the-f.patch` | arifi-fork-base | `8a233ac96` | - | - | R87 correction: iq3_s n=6 rows knob narrowed to the f32-B pipeline; mechanism comment reworded |
| 598 | `0598-R75-lane-262-WIP-iq4_xs-q8_1-MMVQ-shader-arm-registr.patch` | arifi-fork-base | `b8ca07de6` | - | - | R75 (lane-262) WIP: iq4_xs q8_1 MMVQ shader arm + registrations inherited from the stopped Opus 5 maker (unreviewed, unbuilt, default unrouted) |
| 599 | `0599-vulkan-GGML_ARIFI_IQ4XS_MMVQ-legacy-route-all-switch.patch` | ternary-g128 | `8d5a1f5df` | - | - | vulkan: GGML_ARIFI_IQ4XS_MMVQ=<legacy\|route\|all> switch + startup line; iq4_xs decides in should_use_mmvq and never falls through |
| 600 | `0600-tests-perf-rows-bs-1.9-at-the-served-attention-shape.patch` | arifi-fork-base | `6887522f7` | - | - | tests: perf rows bs 1..9 at the served attention shapes (10240x5120, 6144x5120, 12288x5120 for q4_K/q5_K/q6_K/iq4_xs/iq3_s/iq3_xxs; 5120x6144, 1024x5120 for q5_K/iq4_xs/iq3_s/iq3_xxs, bs=9 only for q4_K/q6_K) |
| 601 | `0601-vulkan-iq4_xs-q8_1-MMVQ-admit-table-6-measured-cells.patch` | ternary-g128 | `6ed3910b0` | - | - | vulkan: iq4_xs q8_1 MMVQ admit table (6 measured cells) + AMD default route; test-backend-ops iq4_xs served-shape eval rows and _id perf rows |
| 602 | `0602-vulkan-GGML_ARIFI_IQ4XS_MMVQ-default-back-to-legacy-.patch` | ternary-g128 | `c00ce0742` | - | - | vulkan: GGML_ARIFI_IQ4XS_MMVQ default back to legacy on every vendor - the 6-cell route failed the greedy identity gate |
| 603 | `0603-R86-lane-278-receipts-part-1-linearize-RED-x4-gate-b.patch` | arifi-fork-base | `40562e795` | - | - | R86 (lane-278) receipts part 1: linearize, RED x4, gate battery, re-take, stage + REVERIFY, PPL runner |
| 604 | `0604-R86-lane-278-REPORT-receipts-part-2-timed-chain-dept.patch` | arifi-fork-base | `de6d71376` | - | - | R86 (lane-278) REPORT + receipts part 2: timed chain, depth table, analysis; PROMOTE-READY yes |
| 605 | `0605-R86-lane-278-report-correction-pre-registered-same-s.patch` | arifi-fork-base | `e91d62c86` | - | - | R86 (lane-278) report correction: pre-registered same-session marg is the number of record (UNRESOLVED), host-load audit, same-day r73i anchor |
| 606 | `0606-R86-lane-278-report-summary-trimmed-under-300-words.patch` | arifi-fork-base | `664b7289d` | - | - | R86 (lane-278) report: summary trimmed under 300 words |
| 607 | `0607-OW-014-lane-285-KV-precision-tail-trusts-ring-entrie.patch` | arifi-fork-base | `8f451c9c9` | - | - | OW-014 (lane-285): KV precision tail trusts ring entries by exact ownership, not the high-water mark |
| 608 | `0608-W1-base-move-fixup-heapres-test-block-after-upstream.patch` | arifi-fork-base | `c82b7d2cf` | - | - | W1 base-move fixup: heapres test block after upstream's relocated llama_build(test-backend-ops.cpp) |
| 609 | `0609-W1-base-move-fixup-arifi_sync_check-expected-counts-.patch` | arifi-fork-base | `fc48b46ba` | - | - | W1 base-move fixup: arifi_sync_check expected counts on b11178 (QUANT_K 37, marked lists 16) |
| 610 | `0610-W1-split-fixup-fork-buffer-layer-symbols-called-acro.patch` | arifi-fork-base | `8379d345f` | - | - | W1 split fixup: fork buffer-layer symbols called across the split are extern (declared in ggml-vulkan-common.h) |
| 611 | `0611-W1-base-move-fixup-DFlash-deferred-drafting-reads-po.patch` | arifi-fork-base | `02c73feeb` | - | - | W1 base-move fixup: DFlash deferred drafting reads pos0; RS zero audit reads gf_res_prev_active |
| 612 | `0612-W1-base-move-fixup-ARIFI-SYNC-SOLO-on-upstream-s-coo.patch` | arifi-fork-base | `4e739f4e0` | - | - | W1 base-move fixup: ARIFI-SYNC-SOLO on upstream's coopmat1 int-MMQ shmem probe (marked type lists 16 -> 17) |
| 613 | `0613-arifi-sync-base-move-b10825-9e0e22059-b11178-f9af9be.patch` | arifi-fork-base | `e52a5b5d2` | - | - | arifi-sync: base move b10825/9e0e22059 -> b11178/f9af9be21 (R55 read, W1 execution); 688/688 fork commits replayed via staged milestones S1..S7, hand stops carry Conflict-resolved trailers |
| 614 | `0614-arifi-sync-re-key-provenance-ledgers-after-the-b1082.patch` | arifi-fork-base | `80e47f738` | - | - | arifi-sync: re-key provenance ledgers after the b10825 -> b11178 move (UPDATE-RUNBOOK 2.2.1) + G7 rows for 14 untrailered commits |
| 615 | `0615-OW-028-lane-279-ROCmFP4-FAST-q8_1-MMVQ-route-DEFAULT.patch` | arifi-fork-base | `d525d6670` | - | - | OW-028 (lane-279): ROCmFP4-FAST q8_1 MMVQ route DEFAULT ON for FP4-format files |
| 616 | `0616-W1-split-fixup-Vulkan-test-fault-seam-atomics-are-in.patch` | arifi-fork-base | `9678f7da1` | - | - | W1 split fixup: Vulkan test-fault seam atomics are inline, not static (heapres mid-plan failure RED -> one definition) |
| 617 | `0617-W1-collision-fixup-N3-MMQ-under-coopmat-opt-in-leave.patch` | arifi-fork-base | `38f348fab` | - | - | W1 collision fixup (N3): MMQ-under-coopmat opt-in leaves Q8_0 to upstream int8 coopmat1 on RDNA3/4 |
| 618 | `0618-W1-collision-arms-N1-upstream-IQ4_XS-mat-vec-shader-.patch` | arifi-fork-base | `df9ecc1f7` | - | - | W1 collision arms (N1): upstream IQ4_XS mat-vec shader, MMVQ body and route as runtime switches beside ours, default ours |
| 619 | `0619-protected-wins-vulkan-uma-read-path-probe-symbol-re-.patch` | arifi-fork-base | `59c65e0be` | - | - | protected-wins: vulkan-uma-read-path-probe symbol re-homed to ggml-vulkan-buffers.cpp after the f172be756 split |
| 620 | `0620-W1-collisions-N11-N12-coopmat1-f16-B-guard-for-f32-B.patch` | arifi-fork-base | `aa6ebfce2` | - | - | W1 collisions N11/N12: coopmat1 f16-B guard for f32-B-only types; int8 coopmat1 MMQ min-n switch |
| 621 | `0621-W1-collisions-N12-RDNA3-device-probe-default-for-the.patch` | arifi-fork-base | `e0d733dde` | - | - | W1 collisions N12: RDNA3 device-probe default for the int8 coopmat1 MMQ width switch |
| 622 | `0622-W1-collisions-N12-RDNA3-f16-B-width-floor-24-for-the.patch` | arifi-fork-base | `11eef8565` | - | - | W1 collisions N12: RDNA3 f16-B width floor 24 for the coopmat1 f32->f16 B conversion |
| 623 | `0623-W1-collisions-N12-RDNA3-small-n-ffn_down-int8-except.patch` | arifi-fork-base | `5432990f3` | - | - | W1 collisions N12: RDNA3 small-n ffn_down int8 exception defaults OFF (served -ub 32: q4kxl 0.920x vs 0.977x, GSQ 0.928x vs 0.958x of r86i); GGML_ARIFI_CM1_INT_SMALLN=down restores it |
| 624 | `0624-protected-wins-carry-registrations-across-the-W1-reb.patch` | arifi-fork-base | `9bf09374d` | - | `GENERATE_EXPERT_BUNDLE`, `LANE110_PROF`, `GGML_ARIFI_VNNI_REPACK`, `MAX_N_CACHED` | protected-wins: carry registrations across the W1 rebase and register the unregistered measured wins (Sol REJECT fix 3) |
| 625 | `0625-metal-restore-upstream-s-dk-96-dv-64-sparse-FA-shape.patch` | arifi-fork-base | `0f50ef68d` | ggml-org/llama.cpp 1e7bcf3da4b2741868d152fa47976fb2501c85e3 (#28599) | - | metal: restore upstream's (dk=96, dv=64) sparse-FA shape lost in the W1 re-home |
| 626 | `0626-server-carry-the-tool-call-anchor-across-upstream-s-.patch` | arifi-fork-base | `4eed112ad` | - | - | server: carry the tool-call anchor across upstream's same-n_tokens checkpoint supersede (W1 collision C077) |
| 627 | `0627-qwen35-chained-MTP-drafting-works-with-fused-QKV-GGU.patch` | arifi-fork-base | `ba3683034` | - | - | qwen35: chained MTP drafting works with fused-QKV GGUFs (W1 collision C071) |
| 628 | `0628-ggml-cpu-upstream-s-ARM-NEON-Q1_0-repack-arms-stay-d.patch` | ternary-g128 | `f2b078a88` | - | `GGML_ARIFI_VNNI_REPACK` | ggml-cpu: upstream's ARM NEON Q1_0 repack arms stay default ON outside the fork VNNI gate (W1 collision C019) |
| 629 | `0629-vulkan-graph_optimize-no-longer-hoists-a-UNARY-away-.patch` | ternary-g128 | `c63cad345` | - | - | vulkan: graph_optimize no longer hoists a UNARY away from the MUL it fuses with (W1 collision V023) |
| 630 | `0630-arifi-sync-carry-spec-dflash-fused-inject-switch-to-.patch` | ternary-g128 | `a1b1f611a` | - | - | arifi-sync: carry spec-dflash-fused-inject-switch to 4fea45e7e and register the Vulkan Q2_0_G128 port 56763ff4e (lane-296 Sol-fix, protected-win validate) |
| 631 | `0631-docs-GGML_ARIFI_VNNI_REPACK-scope-after-C019-ARM-NEO.patch` | arifi-fork-base | `51a6f49a3` | - | `GGML_ARIFI_VNNI_REPACK` | docs: GGML_ARIFI_VNNI_REPACK scope after C019 - ARM NEON Q1_0 arms are upstream default ON, outside the fork gate (comment + OPTIONS-REGISTRY row) |
| 632 | `0632-arifi-sync-protected-win-resolutions.json-for-the-36.patch` | arifi-fork-base | `0d62bfd1e` | - | `GGML_ARIFI_VNNI_REPACK`, `GENERATE_EXPERT_BUNDLE` | arifi-sync: protected-win-resolutions.json for the 36 wins the b10825 to b11178 range touches (lane-296 Sol-fix item 3, runbook 4.1c) |
| 633 | `0633-tests-V023-asserting-regression-check-SILU-must-stay.patch` | arifi-fork-base | `16f0ae402` | - | - | tests: V023 asserting regression check - SILU must stay fused with its MUL on Vulkan (lane-296 B3) |
| 634 | `0634-arifi-sync-protected-win-resolutions-B1-rows-measure.patch` | arifi-fork-base | `d43dc95e7` | - | - | arifi-sync: protected-win-resolutions B1 rows measured (lane-296 run15/17/23): 12 daily-dense rows carry r86i/U8 numbers, vanilla where it runs; open op re-pair cells listed per row |
| 635 | `0635-arifi-sync-B1-rows-corrected-lane-296-248320-head-ce.patch` | arifi-fork-base | `450b82a5b` | - | - | arifi-sync: B1 rows corrected (lane-296): 248320 head cell is in the perf list and owed (run24 d), iq3 N7 off arm = registered legacy cliff, concat dispatch count wording, owed flags + run24 scope |
| 636 | `0636-tests-SILU-MUL-perf-eval-cases-at-the-27B-FFN-width-.patch` | arifi-fork-base | `feabf1a0b` | - | - | tests: SILU*MUL perf/eval cases at the 27B FFN width (n_ff 17408, tokens 1/5/512) + ARIFI_PRINT_ERR error print (lane-296 B2b receipt source) |
| 637 | `0637-lane-296-B1-run24-stage-h-close-in-protected-win-res.patch` | arifi-fork-base | `2bd33c42b` | - | - | lane-296 B1: run24 + stage h close in protected-win resolutions |
| 638 | `0638-lane-296-B1-correct-run24-order-effect-sign-record-k.patch` | arifi-fork-base | `39e8e1ac2` | - | - | lane-296 B1: correct run24 order-effect sign, record kernel identity R vs U8 |
| 639 | `0639-lane-296-B1-run25-close-of-OPEN-1-OPEN-2-protected-w.patch` | arifi-fork-base | `c597892ae` | - | - | lane-296 B1: run25 close of OPEN 1 + OPEN 2 (protected-win resolutions) |
| 640 | `0640-lane-296-B1-OPEN-1-row-harness-sync-submit-pipeline-.patch` | arifi-fork-base | `d70b85455` | - | - | lane-296 B1: OPEN 1 row - harness/sync/submit/pipeline identity CHECKED, n=6 off the registered GSQ depth-4 line |
| 641 | `0641-lane-296-B1-OPEN-1-not-yet-a-documented-loss-mechani.patch` | arifi-fork-base | `ef308e5d8` | - | - | lane-296 B1: OPEN 1 not yet a documented loss (mechanism unnamed), depth-5 width mapping, OPEN 2 q4_K closed at line level |
| 642 | `0642-night-sx8-numerics-revert-GDN-q-k-norm-to-ggml_l2_no.patch` | arifi-fork-base | `ff323b02f` | - | - | night/sx8-numerics: revert GDN q/k norm to ggml_l2_norm (5fdfa62829, qwen35 only) - throwaway numerics probe |
| 643 | `0643-Revert-night-sx8-numerics-revert-GDN-q-k-norm-to-ggm.patch` | arifi-fork-base | `8305fcd90` | - | - | Revert "night/sx8-numerics: revert GDN q/k norm to ggml_l2_norm (5fdfa62829, qwen35 only) - throwaway numerics probe" |
| 644 | `0644-vulkan-keep-Q8_0-x-Q8_1-int8-coopmat1-under-GGML_ARI.patch` | ternary-g128 | `9c0e74c2f` | - | - | vulkan: keep Q8_0 x Q8_1 int8 coopmat1 under GGML_ARIFI_MMQ_UNDER_COOPMAT (default OFF) |
| 645 | `0645-vulkan-GGML_ARIFI_Q8_0_CM1-off-on-min-rows-for-the-R.patch` | ternary-g128 | `9223743e1` | - | - | vulkan: GGML_ARIFI_Q8_0_CM1 = off \| on \| <min rows> for the RDNA3/4 Q8_0 x Q8_1 int8 coopmat1 MMQ |
| 646 | `0646-vulkan-Q8_0-int8-cm1-default-FLOOR-int8-on-ssm_out-f.patch` | ternary-g128 | `d8c538d52` | - | - | vulkan: Q8_0 int8 cm1 default = FLOOR (int8 on, ssm_out float) + per-role switches |
| 647 | `0647-vulkan-GGML_ARIFI_Q8_0_CM1_2D-roles-two-digit-int8-a.patch` | ternary-g128 | `471494483` | - | - | vulkan: GGML_ARIFI_Q8_0_CM1_2D=<roles> two-digit int8 activations for Q8_0 MMQ (candidate B) |
| 648 | `0648-lane-296-night-int8-accuracy-deliverable-floor-A-pas.patch` | arifi-fork-base | `fe49d8b17` | - | - | lane-296 night: int8 accuracy deliverable - floor A passes 3/3, goal B (int8 ssm_out) open |
| 649 | `0649-vulkan-two-digit-int8-second-digit-prescaled-by-2-GG.patch` | ternary-g128 | `c445bedf3` | - | - | vulkan: two-digit int8 second digit prescaled by 2^GGML_ARIFI_Q8_0_CM1_2D_SHIFT (default 8), candidate B3 |
| 650 | `0650-vulkan-q8_1-residual-and-scale-f16-rounding-use-the-.patch` | ternary-g128 | `1a88a9c09` | - | - | vulkan: q8_1 residual and scale-f16 rounding use the ds store's own f16 conversion; arifi-op-probe op-level int8 tools |
| 651 | `0651-lane-296-int8-goal-INT8-MATH-derivation-op-level-err.patch` | arifi-fork-base | `9a3783ec7` | - | - | lane-296 int8 goal: INT8-MATH derivation + op-level error tables, INT8-ACCURACY update (B5 accurate at op level, 7.431166 vs bar 7.4163; instrument paired SE 0.006-0.012) |
| 652 | `0652-lane-296-int8-goal-conclusions-made-conditional-on-t.patch` | arifi-fork-base | `abbda3401` | - | - | lane-296 int8 goal: conclusions made conditional on the 64-chunk check; B1-B3 pair (+0.0209 PPL at 3.4 SE with equal ssm_out op error); section renumber; magic-bias note |
| 653 | `0653-vulkan-GGML_ARIFI_F32ACC-roles-all-forces-f32-accumu.patch` | ternary-g128 | `3e85f0cb6` | - | - | vulkan: GGML_ARIFI_F32ACC=<roles>\|all forces f32 accumulation on the float matmul path; arifi-op-probe bias decomposition |
| 654 | `0654-lane-296-int8-goal-stage-8-64-chunk-gap-is-real-2.7-.patch` | arifi-fork-base | `6813ffc87` | - | - | lane-296 int8 goal stage 8: 64-chunk gap is real (+2.7 SE, broad); H-B and H-A falsified; float-path error named as f16 accumulation (random, 7.7e-3 -> 2.66e-4 with f32 acc); chain s8 pre-registered |
| 655 | `0655-lane-296-int8-goal-stage-8-pre-register-x16-pairing-.patch` | arifi-fork-base | `a1eaa94a3` | - | - | lane-296 int8 goal stage 8: pre-register x16 pairing and the c64 x x16 joint reading before c64 lands; smoke downgraded (2 seqs/pass); S-X8 f32acc route checked by code; price disclosed |
| 656 | `0656-lane-296-int8-goal-stage-9-F32ACC-takes-type-role-an.patch` | arifi-fork-base | `1691b758c` | - | - | lane-296 int8 goal stage 9: F32ACC takes <type>:<role> and <type>:all; op probe replays any weight type and times widths |
| 657 | `0657-vulkan-UMA-device-local-first-placement-on-the-Radeo.patch` | ternary-g128 | `2fa02db7f` | - | - | vulkan: UMA device-local-first placement on the Radeon 890M (port of 3a68edb49 onto night/sx8-numerics 1691b758c4) |
| 658 | `0658-sx8-int8-stage-10-S-X8-on-the-int8-coopmat1-MMQ-kern.patch` | arifi-fork-base | `d871a4fae` | - | - | sx8-int8 stage 10: S-X8 on the int8 coopmat1 MMQ kernel (exact (d,m) remap, exact b sums, RED twin, role/min-n knobs, tests) |
| 659 | `0659-sx8-int8-stage-10-GGML_ARIFI_SX8_CM1_2D-fix-knob-two.patch` | arifi-fork-base | `3f5c70921` | - | - | sx8-int8 stage 10: GGML_ARIFI_SX8_CM1_2D fix knob (two-digit activations for listed S-X8 roles, lane-296 R3 route) |
| 660 | `0660-sx8-int8-stage-10-two-digit-activations-default-ON-f.patch` | arifi-fork-base | `e650ca8a8` | - | - | sx8-int8 stage 10: two-digit activations default ON for ffn_up (fix B) |
| 661 | `0661-sx8-int8-attempt-E-16-value-activation-scales-for-th.patch` | arifi-fork-base | `7e8380ca6` | - | - | sx8-int8 attempt E: 16-value activation scales for the S-X8 int8 cm1 kernel (env-only arm) |
| 662 | `0662-sx8-int8-attempt-E-quantize_q8_1-HALF-ds-store-via-v.patch` | arifi-fork-base | `b43fe423b` | - | - | sx8-int8 attempt E: quantize_q8_1 HALF ds store via vec2 (f16 locals need the arithmetic extension) |
| 663 | `0663-sx8-int8-attempt-D-two-digit-activations-only-for-ou.patch` | arifi-fork-base | `fb251eed0` | - | - | sx8-int8 attempt D: two-digit activations only for outlier blocks, skip kernel for the second pass |
| 664 | `0664-sx8-int8-default-to-gate20-selective-two-digit-ffn_u.patch` | arifi-fork-base | `16884e572` | - | - | sx8-int8: default to gate20 selective two-digit ffn_up activations |
| 665 | `0665-vulkan-bound-SX8-residual-masks-and-exercise-gate-fi.patch` | ternary-g128 | `f9df20b32` | - | - | vulkan: bound SX8 residual masks and exercise gate fixtures |
| 666 | `0666-vulkan-use-float-below-S-X8-served-crossover.patch` | ternary-g128 | `841093695` | - | - | vulkan: use float below S-X8 served crossover |
| 667 | `0667-vulkan-S-X8-width-gate-covers-prompt-widths-only-n-8.patch` | ternary-g128 | `fecb34207` | - | - | vulkan: S-X8 width gate covers prompt widths only (n > 8); decode widths keep the int8 route (check finding F1; S-X8 bank 149/149 + fixtures 10/10 green on build9) |
| 668 | `0668-S-X8-gate-comment-threshold-is-in-dispatch-columns-n.patch` | arifi-fork-base | `926e2931d` | - | - | S-X8 gate comment: threshold is in dispatch columns, not request tokens |
| 669 | `0669-server-keep-prompt-images-in-reusable-device-snapsho.patch` | arifi-fork-base | `c23bd8cc5` | - | - | server: keep prompt images in reusable device snapshots |
| 670 | `0670-lane-298-skip-whole-file-mmap-prefetch-when-a-device.patch` | arifi-fork-base | `b651d7f98` | - | - | lane-298: skip whole-file mmap prefetch when a device copies weights out of the mapping |
| 671 | `0671-lane-300-F-141-coopmat1-FA-zeroes-V-rows-masked-for-.patch` | arifi-fork-base | `8608d59a3` | - | - | lane-300 F-141: coopmat1 FA zeroes V rows masked for every valid tile row (stale/foreign KV content no longer reaches the P*V WMMA sum); flags bit 32 default ON, GGML_VK_FA_DEADV_KEEP=1 = planted-RED switch; fa-replay sign-class arms. Lane: lane-300 |
| 672 | `0672-lane-298-R1-checkpoint-fix-ssm-row-save-restore-maps.patch` | arifi-fork-base | `1cfa48571` | - | - | lane-298 R1 checkpoint fix: ssm row save/restore maps to the physical ring plane |
| 673 | `0673-lane-298-qwen4exp-MTP-sidecar-load-graph_mtp-target-.patch` | arifi-fork-base | `8a721b95d` | - | - | lane-298: qwen4exp MTP sidecar load + graph_mtp, target 4-stream capture, Windows unmap_fragment trim (env-gated) |
| 674 | `0674-lane-298-fix-Windows-lm-dio-probe-failure-unaligned-.patch` | arifi-fork-base | `a3cdce01b` | - | - | lane-298: fix Windows -lm dio probe failure (unaligned file-pointer moves on NO_BUFFERING handles) |
| 675 | `0675-lane-298-unbuffered-read_raw_unsafe-throws-on-an-una.patch` | arifi-fork-base | `30347e2b6` | - | - | lane-298: unbuffered read_raw_unsafe throws on an unaligned cursor/len/ptr instead of returning floor-sector bytes |
| 676 | `0676-lane-298-qwen4exp-MTP-capture-is-a-real-graph-node-f.patch` | arifi-fork-base | `52566ff2c` | - | - | lane-298: qwen4exp MTP capture is a real graph node (fixes GGML_ASSERT backend_h != nullptr) |
| 677 | `0677-lane-298-server-emits-n_probs-on-the-speculative-pat.patch` | arifi-fork-base | `1980c2b73` | - | - | lane-298: server emits n_probs on the speculative path (verify-row index per emitted token) |
| 678 | `0678-lane-298-spec-round-cost-timers-env-draft-recent-tok.patch` | arifi-fork-base | `c0c175642` | - | - | lane-298: spec round-cost timers + env draft recent-token penalty (OW-036) |
| 679 | `0679-lane-298-qwen4exp-MTP-draft-head-over-a-token-subset.patch` | arifi-fork-base | `99b1bd597` | - | - | lane-298: qwen4exp MTP draft head over a token subset (d2t + gathered output rows, Strata draft_vocab) |
| 680 | `0680-lane-298-MTP-eh_proj-as-one-2D-matmul.patch` | arifi-fork-base | `6350b8f7e` | - | - | lane-298: MTP eh_proj as one 2D matmul |
| 681 | `0681-lane-298-OW-036-draft-penalty-default-on-ngram-mod-m.patch` | arifi-fork-base | `c1f0071d8` | - | - | lane-298: OW-036 draft penalty default on, ngram-mod miss-cost cap, MoE-union counter |
| 682 | `0682-lane-298-ngram-mod-CAP0-gate-starts-at-full-width-ca.patch` | arifi-fork-base | `e3ec87d92` | - | - | lane-298: ngram-mod CAP0 gate starts at full width, caps only after a miss |
| 683 | `0683-lane-298-item-5-drop-the-previous-ubatch-PLE-row-pag.patch` | arifi-fork-base | `e8f260b80` | - | - | lane-298 item 5: drop the previous ubatch PLE row pages from the working set (LLAMA_PLE_RELEASE=1) |
| 684 | `0684-lane-298-item-5-v2-PLE-release-one-whole-table-Virtu.patch` | arifi-fork-base | `8291ce4ed` | - | - | lane-298 item 5 v2: PLE release = one whole-table VirtualUnlock per row budget, not per row |
| 685 | `0685-lane-298-item-5-PLE-release-default-ON-at-256-MiB-LL.patch` | arifi-fork-base | `671a9594e` | - | - | lane-298 item 5: PLE release default ON at 256 MiB; LLAMA_PLE_RELEASE=0 turns it off |
| 686 | `0686-lane-299-WIP-vulkan-HC_POST_W-fusion-SCALE-SIGMOID-S.patch` | arifi-fork-base | `1ea9394f5` | - | - | lane-299 WIP: vulkan HC_POST_W fusion (SCALE->SIGMOID->SCALE->DSV4_HC_POST, one dispatch) |
| 687 | `0687-lane-299-WIP-vulkan-GDN_BANK-fusion-GATED_DELTA_NET-.patch` | prismml-recurrent | `c474cc1d9` | - | - | lane-299 WIP: vulkan GDN_BANK fusion (GATED_DELTA_NET reads/writes the R1 recurrent bank in place; state GET_ROWS + SET_ROWS skipped). Off GGML_VK_DISABLE_GDN_BANK, plant GGML_VK_GDN_BANK_PLANT, test GATED_DELTA_NET_BANK. UNBUILT at commit time (GPU-TIMED-moe hold). |
| 688 | `0688-lane-299-WIP-GDN_BANK-prepass-reads-SET_ROWS-operand.patch` | arifi-fork-base | `df91200f0` | - | - | lane-299 WIP: GDN_BANK prepass reads SET_ROWS operands in ggml order (src0 data, src1 row ids, src2 dst); the old order never matched, so the fused path never fired (optest3 reject 2). Adds GGML_VK_GDN_BANK_DEBUG reject-stage prints. |
| 689 | `0689-lane-298-item-5-prefetch-ahead-one-PrefetchVirtualMe.patch` | arifi-fork-base | `300d9d691` | - | - | lane-298 item 5 prefetch-ahead: one PrefetchVirtualMemory over each ubatch's PLE row pages (LLAMA_PLE_PREFETCH=1 opt-in) |
| 690 | `0690-lane-298-item-5-prefetch-ahead-prefetch-after-the-PL.patch` | arifi-fork-base | `e30a7e459` | - | - | lane-298 item 5 prefetch-ahead: prefetch after the PLE trim, not before |
| 691 | `0691-lane-298-item-5-prefetch-ahead-LLAMA_PLE_PREFETCH-de.patch` | arifi-fork-base | `e2eb456e4` | - | - | lane-298 item 5 prefetch-ahead: LLAMA_PLE_PREFETCH default 1 (pfa-10051313 PASS) |
| 692 | `0692-lane-298-item-5-prefetch-ahead-keep-the-per-ubatch-W.patch` | arifi-fork-base | `4295f4827` | - | - | lane-298 item 5 prefetch-ahead: keep the per-ubatch WARN receipt on the default path |
| 693 | `0693-lane-298-GDN-snapshot-byte-floor-diag-GGML_VK_GDN_SN.patch` | arifi-fork-base | `4b1fb8b39` | - | - | lane-298 GDN snapshot-byte floor diag: GGML_VK_GDN_SNAP1_DIAG clamps GDN_BANK n_written to 1 |
| 694 | `0694-lane-298-GDN-replay-cost-bounding-diag-GGML_VK_GDN_R.patch` | arifi-fork-base | `0d511dd76` | - | - | lane-298 GDN replay-cost bounding diag: GGML_VK_GDN_REPLAY_DIAG runs a replay prologue (n_rep push constant) in the GDN_BANK shader |
| 695 | `0695-lane-298-GDN-deferred-commit-replay-opt-in-LLAMA_GDN.patch` | prismml-recurrent | `a8b20d128` | - | - | lane-298 GDN deferred commit + replay (opt-in LLAMA_GDN_REPLAY=1): replay op, Vulkan GDN_REPLAY bank pipeline, recurrent wiring |
| 696 | `0696-lane-298-fix-two-log-format-strings-carried-raw-newl.patch` | arifi-fork-base | `f5276ee24` | - | - | lane-298 fix: two log format strings carried raw newlines instead of \n (cf4f877c07 build break) |
| 697 | `0697-lane-298-GDN-replay-verify-routes-an-unlogged-wide-b.patch` | arifi-fork-base | `d42ee555f` | - | - | lane-298 GDN replay: verify routes an unlogged wide-batch partial reject to the draft-time checkpoint |
| 698 | `0698-lane-301-Vulkan-FA-sparse-gather-for-QSA-prefill-gqa.patch` | arifi-fork-base | `634bbd673` | - | - | lane-301: Vulkan FA sparse gather for QSA prefill (gqa layout for sparse-hinted multi-row FA) + prefill sparse op cases |
| 699 | `0699-lane-301-qwen4exp-row-25-persistent-pooled-indexer-k.patch` | arifi-fork-base | `7d61001f0` | - | - | lane-301: qwen4exp row 25 - persistent pooled indexer-key cache (pool+norm+rope once per full block, incremental fresh blocks), GGML_ARIFI_QSA_POOL_CACHE=0 / GGML_ARIFI_QSA_POOL_RED=1; test-backend-ops CPU census perf shapes + GGML_ARIFI_TBO_THREADS |
| 700 | `0700-lane-301-row-25-gate-fix-M-RoPE-token-ubatches-carry.patch` | rocmfpx-mrope | `ea75fe8ca` | - | - | lane-301: row 25 gate fix - M-RoPE token ubatches carry n_pos=4, only an embedding ubatch disables the pooled cache; path-class WARN log; RED=2 drops every fresh write; protected-win entry for row-26 sparse prefill (c3 logger-off x2) |
| 701 | `0701-lane-301-protected-win-entry-qwen4exp-qsa-pooled-ind.patch` | arifi-fork-base | `49c4e1891` | - | - | lane-301: protected-win entry qwen4exp-qsa-pooled-indexer-cache (row 25, c4 logger-off x2: decode 32K 8.29->10.35 tok/s, ids identical) |
| 702 | `0702-lane-302-MUL_MAT_ID-expert-gather-for-small-batch-ma.patch` | arifi-fork-base | `c49a7c15e` | - | - | lane-302: MUL_MAT_ID expert gather for small-batch mat-vec (opt-in GGML_ARIFI_MOE_GATHER=1\|2\|4) |
| 703 | `0703-lane-302-gather-leader-test-uses-a-mask-G-is-1-2-or-.patch` | arifi-fork-base | `ba68186d9` | - | - | lane-302: gather leader test uses a mask (G is 1, 2 or 4), comment points at the real pipeline block |
| 704 | `0704-lane-302-served-qwen4exp-MoE-op-test-shapes-n-1.8-ga.patch` | arifi-fork-base | `fe35f4fa7` | - | - | lane-302: served qwen4exp MoE op-test shapes n=1..8 + gather plant guard |
| 705 | `0705-lane-302-served-MoE-op-test-cases-at-16-experts-only.patch` | arifi-fork-base | `afbe861ef` | - | - | lane-302: served MoE op-test cases at 16 experts only |
| 706 | `0706-lane-302-gather-pipelines-walk-slots-fastest-slot-ma.patch` | arifi-fork-base | `57c45ac84` | - | - | lane-302: gather pipelines walk slots fastest (slot-major workgroup order) so shared experts hit L2 |
| 707 | `0707-lane-302-protected-wins-entry-for-the-slot-major-MoE.patch` | arifi-fork-base | `6350bcf72` | - | - | lane-302: protected-wins entry for the slot-major MoE gather (op-level win, default off; served A/B not shown) Lane: lane-302 |
| 708 | `0708-lane-302-MoE-gather-default-ON-at-width-1-cellE-1005.patch` | arifi-fork-base | `e8bf3e812` | - | - | lane-302: MoE gather default ON at width 1 (cellE-10052000 served ABBA x2: mean +0.34%, ids IDENT 48/48); GGML_ARIFI_MOE_GATHER=0 opts out; protected-wins entry amended with served fields |
| 709 | `0709-lane-302-GGML_ARIFI_MOE_GATHER_ROWS-1-2-4-8-sets-row.patch` | arifi-fork-base | `8c913a9d2` | - | - | lane-302: GGML_ARIFI_MOE_GATHER_ROWS (1\|2\|4\|8) sets rows per workgroup of the gather pipelines; unset = rm_iq (no default moves) |
| 710 | `0710-lane-302-IQ3_S-gather-pipeline-defaults-to-2-rows-pe.patch` | arifi-fork-base | `7c9a6bc21` | - | - | lane-302: IQ3_S gather pipeline defaults to 2 rows per workgroup (cellG-10052319: n4 -10%/-13% vs rm_iq in both rounds, harness 80/80 byte-identical); GGML_ARIFI_MOE_GATHER_ROWS still overrides every type |
| 711 | `0711-lane-302-protected-wins-entry-for-the-IQ3_S-gather-r.patch` | arifi-fork-base | `35841fffa` | - | - | lane-302: protected-wins entry for the IQ3_S gather rows-2 default (cellH-10060008 served ABBA x2: mean +0.07%, worst prompt -0.54%, ids IDENT = KEEP; op -10%/-13% cellG-10052319) |
| 712 | `0712-lane-302-IQ4_NL-gather-K-tail-redistribution-opt-in-.patch` | arifi-fork-base | `d5deb283b` | - | - | lane-302: IQ4_NL gather K-tail redistribution, opt-in GGML_ARIFI_MOE_GATHER_TAIL=1 (=2 planted wrong arm) |
| 713 | `0713-lane-302-IQ4_NL-q8_1-integer-dot-mat-vec-expert-gath.patch` | arifi-fork-base | `3d333a096` | - | - | lane-302: IQ4_NL q8_1 integer-dot mat-vec + expert-gather twin, opt-in GGML_ARIFI_IQ4NL_MMVQ=1 (every n) |
| 714 | `0714-lane-302-IQ2_S-gather-body-arms-opt-in-GGML_ARIFI_IQ.patch` | arifi-fork-base | `ad9f46e05` | - | - | lane-302: IQ2_S gather body arms (opt-in GGML_ARIFI_IQ2S_BODY) + ISA dump under GGML_VK_PIPELINE_STATS |
| 715 | `0715-lane-302-IQ2_S-gather-batched-grid-init-arms-opt-in-.patch` | arifi-fork-base | `ad0ef50aa` | - | - | lane-302: IQ2_S gather batched grid init arms (opt-in GGML_ARIFI_IQ2S_BODY=4\|5), body stays opt-in |
| 716 | `0716-lane-302-IQ2_S-gather-body-5-int8-sign-batched-grid-.patch` | arifi-fork-base | `88eeef9a0` | - | - | lane-302: IQ2_S gather body 5 (int8 sign + batched grid init) DEFAULT ON |
| 717 | `0717-lane-302-protected-wins-entry-vulkan-moe-gather-iq2s.patch` | arifi-fork-base | `32f06513d` | - | - | lane-302: protected-wins entry vulkan-moe-gather-iq2s-body5 (cellO-10060849 KEEP) Lane: lane-302 |
| 718 | `0718-llama-quant-same-type-tensors-are-byte-copied-so-ski.patch` | arifi-fork-base | `b1c60b4e9` | - | - | llama-quant: same-type tensors are byte-copied, so skip the imatrix guard for them (lets --tensor-type-file pin quantized tensors to their current type while requantizing others) |
| 719 | `0719-llama-quantize-read-via-slabbed-file-path-on-Windows.patch` | arifi-fork-base | `16557e772` | - | - | llama-quantize: read via slabbed file path on Windows, not mmap |
| 720 | `0720-lane-299-protected-wins-entry-vulkan-qwen4exp-hc-pos.patch` | arifi-fork-base | `edc570ed3` | - | - | lane-299: protected-wins entry vulkan-qwen4exp-hc-post-w (measured ABBA +1.28%/+6.69%, ids exact) Lane: lane-299 |
| 721 | `0721-lane-299-protected-wins-entry-vulkan-qwen4exp-gdn-ba.patch` | arifi-fork-base | `936985888` | - | - | lane-299: protected-wins entry vulkan-qwen4exp-gdn-bank (measured 3-round medians +4.47%/+3.09%, ids exact 12/12) |
| 722 | `0722-lane-299-gdn-bank-protected-wins-entry-carries-the-i.patch` | arifi-fork-base | `ea904964e` | - | - | lane-299: gdn-bank protected-wins entry carries the interleaved_ab bootstrap CI (short SIGNIFICANT, long TIED at n=3) |
| 723 | `0723-lane-299-runbook-for-the-qwen4exp-decode-fusion-gate.patch` | arifi-fork-base | `9ef9ad5b7` | - | - | lane-299: runbook for the qwen4exp decode-fusion gates (build, op test with MATCH>0 rule, interleaved served A/B, noise floor) |
| 724 | `0724-moe-cache-x1-GLM-5.3-NVMe-expert-streaming-x12.x21-m.patch` | arifi-fork-base | `b61ddcec3` | - | - | moe-cache-x1: GLM-5.3 NVMe expert streaming, x12..x21 measured source |
| 725 | `0725-kimi-k3-read-the-ssm-bank-through-the-R1-rotated-ind.patch` | arifi-fork-base | `f144818c1` | - | - | kimi-k3: read the ssm bank through the R1 rotated index (bank=true) |
| 726 | `0726-lane-298-integ-protected-win-registration-fixes-carr.patch` | arifi-fork-base | `0130f822a` | - | - | lane-298 integ: protected-win registration fixes carried in from lanes 301/302 + HQ80 f2c5fad421 resolution rows |
| 727 | `0727-vulkan-scope-allocator-wide-shared-admission-to-MoE-.patch` | ternary-g128 | `284fa77c4` | - | - | vulkan: scope allocator-wide shared admission to MoE NVMe streaming |
| 728 | `0728-lane-301-coopmat1-f16-B-staging-for-MUL_MAT_ID-on-no.patch` | arifi-fork-base | `1d7f47ec4` | - | - | lane-301: coopmat1 f16-B staging for MUL_MAT_ID on non-Intel devices, default on |
| 729 | `0729-lane-301-protected-win-vulkan-cm1-f16b-staging-mul-m.patch` | arifi-fork-base | `44450ec46` | - | - | lane-301: protected-win vulkan-cm1-f16b-staging-mul-mat-id + row-26 workload 780M -> Radeon 890M (X1) |
| 730 | `0730-lane-302-IQ4_NL-gather-byte-pair-LUT-IQ3_S-gather-ba.patch` | arifi-fork-base | `48468238d` | - | - | lane-302: IQ4_NL gather byte-pair LUT + IQ3_S gather batched grid init arms (opt-in) |
| 731 | `0731-lane-302-IQ3_XXS-gather-batched-grid-init-arm-opt-in.patch` | arifi-fork-base | `70e4f2303` | - | - | lane-302: IQ3_XXS gather batched grid init arm (opt-in GGML_ARIFI_IQ3XXS_INIT=1\|2) |
| 732 | `0732-lane-300-arifi-det-probe-repeat-determinism-probe-wi.patch` | arifi-fork-base | `9ca84f74a` | - | - | lane-300: arifi-det-probe - repeat-determinism probe with history arms (DETP_CLEAR, DETP_PRE) |
| 733 | `0733-lane-300-det-probe-DETP_FA-FA-mask-live-column-dead-.patch` | arifi-fork-base | `9ac5c5b4a` | - | - | lane-300: det-probe DETP_FA (FA mask live-column + dead K/V report) and DETP_ROWS (GOAL 3b row-independence) |
| 734 | `0734-lane-300-det-probe-DETP_FA_DUMP-FA-node-inputs-outpu.patch` | arifi-fork-base | `f0701495e` | - | - | lane-300: det-probe DETP_FA_DUMP (FA node inputs/output per run) + DETP_ROWS_OPS (per-op last-row compare by width); GGML_VK_FA_MASK_OPT_DISABLE diagnosis switch Lane: lane-300 |
| 735 | `0735-lane-300-arifi-fa-replay-model-free-replay-of-a-dump.patch` | arifi-fork-base | `565bb1a2e` | - | - | lane-300: arifi-fa-replay - model-free replay of a dumped FLASH_ATTN_EXT node (swap/constant/pad/row-width arms) for the F-141 coopmat FA locate |
| 736 | `0736-lane-300-fa-replay-IGPU-backend-fallback-cell-10-dea.patch` | arifi-fork-base | `fa61e861f` | - | - | lane-300: fa-replay IGPU backend fallback + cell-10 dead-V value-class/column-range arms; det-probe prefill ends at min(t-7,t-w+1) for widths > 8 |
| 737 | `0737-lane-300-F-141-coopmat1-FA-zeroes-V-rows-masked-for-.patch` | arifi-fork-base | `b5ea4e3ee` | - | - | lane-300 F-141: coopmat1 FA zeroes V rows masked for every valid tile row (stale/foreign KV content no longer reaches the P*V WMMA sum); flags bit 32 default ON, GGML_VK_FA_DEADV_KEEP=1 = planted-RED switch; fa-replay sign-class arms. Lane: lane-300 |
| 738 | `0738-lane-300-det-probe-DETP_VERIFY-cost-mode-teacher-for.patch` | arifi-fork-base | `f0be30eb6` | - | - | lane-300: det-probe DETP_VERIFY cost mode - teacher-forced w-row verify timing, fix vs planted-RED on identical ids (HQ79 Q7). Lane: lane-300 |
| 739 | `0739-lane-300-det-probe-DETP_ROWS_OPS-2-full-node-dump-pe.patch` | arifi-fork-base | `630ed8e76` | - | - | lane-300: det-probe DETP_ROWS_OPS=2 full node dump per width (last-row hash, I32 nodes included); cell-16 name-walk compare was blind across width-dependent graphs. Lane: lane-300 |
| 740 | `0740-lane-300-F-141-3b-LLAMA_ARIFI_MOE_SUMROWS_MAX-N-keep.patch` | arifi-fork-base | `41cc36d84` | - | - | lane-300: F-141 3b: LLAMA_ARIFI_MOE_SUMROWS_MAX=<N> keeps the n=1 permute+sum_rows expert-sum order for n_tokens<=N (cell 17: n=1 SUM_ROWS vs n>1 ADD chain = first width carrier, ffn_moe_out layer 0). Default 1 = unchanged. Lane: lane-300 |
| 741 | `0741-lane-300-F-141-3b-FA-split_k-row-stable-under-GQA-wo.patch` | arifi-fork-base | `3bea6eff0` | - | - | lane-300: F-141 3b: FA split_k row-stable under GQA - workgroups_x is the token count, so split_k (KV reduction split) moved with verify width; for neq1 <= GGML_ARIFI_FA_ROWSTABLE_N (default 8, 0 = old) split_k is chosen as for one token. Cell 18: SR8+MMVQ-force first diff = layer-3 attention. Lane: lane-300 |
| 742 | `0742-lane-300-F-141-3b-row-stable-FA-split_k-also-on-the-.patch` | arifi-fork-base | `4274c5f12` | - | - | lane-300: F-141 3b: row-stable FA split_k also on the segmented body dispatch (one helper, same GGML_ARIFI_FA_ROWSTABLE_N rule); GGML_ARIFI_FA_TRACE=1 prints each distinct FA dispatch tuple; det-probe DETP_ROWS_OPS=3 hashes every token slice of every node keyed by absolute position (single steps = per-position reference). Cell 19: w=4/8 still first-differ at layer-3 FA. Lane: lane-300 |
| 743 | `0743-lane-300-F-141-3b-GGML_ARIFI_ADD_RMS_ROWSTABLE_N-N-k.patch` | arifi-fork-base | `d3c419a01` | - | - | lane-300: F-141 3b: GGML_ARIFI_ADD_RMS_ROWSTABLE_N=<N> keeps the residual ADD -> RMS_NORM partials fusion for rows <= N (was one row only: a verify batch normalized every row with a different reduction order than n=1 decode). add/multi_add already write partials by flat index/512, so row r owns its n=1 partials; rms_norm_partials reads row = WorkGroupID.y; admission keeps the one-row budget so every width fuses the same layers. Default 0 = unchanged. Cell 21: fused RED, unfused GREEN. Lane: lane-300 |
| 744 | `0744-lane-300-F-141-3b-arifi-mmv-width-model-free-MUL_MAT.patch` | arifi-fork-base | `56d0974bd` | - | - | lane-300: F-141 3b: arifi-mmv-width - model-free MUL_MAT width replay: column 0 of an n-column mat-vec byte-compared with the n=1 result per (type, m, k). Cell 23: qwen4exp layer-0 z (q4_K 6144x2560) differs n=4 vs n=1 with hoist off; f32-dequant route moves the first diff to the q6_K qkv sibling. Lane: lane-300 |
| 745 | `0745-lane-300-F-141-3b-GGML_ARIFI_MMV_ROWSTABLE_N-N-targe.patch` | arifi-fork-base | `e13cd0de8` | - | - | lane-300: F-141 3b: GGML_ARIFI_MMV_ROWSTABLE_N=<N> targets mat-vec column j bit-equal to n=1 for n <= N. Cell 24 (arifi-mmv-width, model-free) showed three width carriers: (1) the MMVQ vs f32-dequant route moved with n (q5_K, q8_0, q6_K, small-k q4_K); now n <= N takes n=1's route. (2) the q6_K -32 fold (ARIFI_Q6K_XFOLD) applied only at NUM_COLS <= 3, so n >= 4 used other arithmetic (source CHECKED); spec constant 4 = 2 folds at every width. (3) Q4_K/Q5_K MMVQ column 0 differed n>1 vs n=1 at k = 2560/3072 only, same shader + pipeline by trace; mechanism ASSUMED = FMA contraction of the a*b*c - d*e dot varying per NUM_COLS specialization (the k pattern fits num_iters = 3 at wave64, wave size ASSUMED); spec constant 5 builds that dot `precise`. (3) moves the n=1 bits of Q4_K/Q5_K MMVQ under the flag only. Default 0 = unchanged (receipt arms O-FX/Q-FX in cell 25). Lane: lane-300 |
| 746 | `0746-lane-300-F-141-3b-precise-Q4_K-Q5_K-MMVQ-dot-moves-t.patch` | arifi-fork-base | `538198cd9` | - | - | lane-300: F-141 3b: precise Q4_K/Q5_K MMVQ dot moves to its own SPIR-V (*_prec_q8_1_f32), iq3_s stock shape under GGML_ARIFI_MMV_ROWSTABLE_N |
| 747 | `0747-lane-300-F-141-3b-GGML_ARIFI_FA_ROWSTABLE_N-default-.patch` | arifi-fork-base | `a26d71b8f` | - | - | lane-300: F-141 3b: GGML_ARIFI_FA_ROWSTABLE_N default 8 -> 0 (opt-in). bf1de9485b shipped it ON before the default-OFF rule; its verify-width cost is unmeasured (cell 28 held), so per HQ82 the default returns to stock split_k. N=0 never selects the n=1 split (neq1 >= 1), so ggml_vk_fa_rowstable_wx returns workgroups_x on the single and the segmented dispatch = merge-base behaviour by construction. Served proof: cell 27 OP-0/QP-0 (env FA_ROWSTABLE_N=0, other flags unset) ids == the 96a6a8d976 binary's plain ids (cells 12 S-fix, 13 O-fix) on short/2k/long, both models. |
| 748 | `0748-lane-299-layout-copies-conv-tails-CPY-strided-no-CON.patch` | arifi-fork-base | `35a67b322` | - | - | lane-299: layout copies - conv tails CPY strided (no CONT, 4 per conv per token), PLE one transpose-copy + one kernel cast (replaces 4 CONT + 4 CONT + 4 CAST + 1 CONT). Same bytes. Off: LLAMA_L299_LAYOUT_OFF=1; RED plant: LLAMA_L299_LAYOUT_PLANT=1 (stale slot-0 history). Op test L299_LAYOUT. |
| 749 | `0749-lane-299-item-B-alpha-bias-joins-the-ssm_alpha-mat-v.patch` | arifi-fork-base | `23de61bab` | - | - | lane-299: item B alpha bias joins the ssm_alpha mat-vec - ADD(ssm_dt) moves before the reshape so MUL_MAT + ADD are adjacent and the backend MUL_MAT_ADD fusion engages (36 ADD dispatches/token; same math). Off: LLAMA_L299_ALPHA_OFF=1; RED plant: LLAMA_L299_ALPHA_PLANT=1 (bias twice). Op test: MUL_MAT_VEC_FUSION bf16 m=1 n=48 k=2560 with bias. |
| 750 | `0750-lane-299-protected-wins-entry-qwen4exp-layout-copies.patch` | arifi-fork-base | `063b87820` | - | - | lane-299: protected-wins entry qwen4exp-layout-copies (measured +1.48%/+1.19%, ids exact 10/10) |
| 751 | `0751-lane-298-integ-GGML_VK_GDN_SNAP1_DIAG-prints-a-WARN-.patch` | arifi-fork-base | `40b6dc11a` | - | - | lane-298 integ: GGML_VK_GDN_SNAP1_DIAG prints a WARN at first use; OPTIONS-REGISTRY rows for both GDN diagnostics |
| 752 | `0752-lane-298-integ-round-2-protected-wins-commit-fields-.patch` | arifi-fork-base | `7013014ac` | - | - | lane-298 integ round 2: protected-wins commit fields repointed to the integ cherry-pick shas |
| 753 | `0753-arifi-sync-port-the-sha-pinned-trailer-exemption-led.patch` | arifi-fork-base | `01cde6911` | - | - | arifi-sync: port the sha-pinned trailer-exemption ledger (r86i 58187870f + a9187ded7) natively |
| 754 | `0754-arifi-sync-trailer-exemptions.json-rows-for-the-63-s.patch` | arifi-fork-base | `0f112f59b` | - | `GGML_ARIFI_VNNI_REPACK` | arifi-sync: trailer-exemptions.json rows for the 63 --strict offenders under the shipped engine c23bd8cc5 |
| 755 | `0755-arifi-sync-re-key-the-protected-win-ledgers-from-the.patch` | arifi-fork-base | `c00417ff7` | - | `GGML_ARIFI_VNNI_REPACK` | arifi-sync: re-key the protected-win ledgers from the pre-rebase chain to the x1i commits |
| 756 | `0756-arifi-sync-x1i-reword-trailer-table-and-replay-scrip.patch` | arifi-fork-base | `0e4f3c72b` | - | - | arifi-sync: x1i-reword - trailer table and replay script for the 58 post-x1i commits |
| 757 | `0757-arifi-sync-x1i-reword-commits-the-ledger-re-key-as-t.patch` | arifi-fork-base | `d6ad887f3` | - | - | arifi-sync: x1i-reword commits the ledger re-key as the tip commit's identity |
| 758 | `0758-arifi-sync-x1i-reword-table-one-commit-per-win-carri.patch` | arifi-fork-base | `3c2a74ae8` | - | - | arifi-sync: x1i-reword table - one commit per win carries the number |
| 759 | `0759-arifi-sync-x1i-reword-table-context-rows-carry-no-fi.patch` | arifi-fork-base | `0dcbd357c` | - | - | arifi-sync: x1i-reword table - context rows carry no figures |
| 760 | `0760-lane-298-integ-round-2-freeze-prep-full-shas-on-the-.patch` | arifi-fork-base | `acf329085` | - | - | lane-298 integ round 2 freeze prep: full shas on the round-2 win commit fields; Q39 Measured-effect cells |
| 761 | `0761-arifi-sync-re-key-the-sha-pinned-ledgers-after-the-p.patch` | arifi-fork-base | `4476c4221` | - | - | arifi-sync: re-key the sha-pinned ledgers after the post-x1i trailer reword |
| 762 | `0762-vulkan-IQ3_S-mat-vec-spec-id-clash-from-merge-round-.patch` | ternary-g128 | `5f37b7400` | - | - | vulkan: IQ3_S mat-vec spec id clash from merge round 2 (IQ3S_INIT moves to spec 5) |
| 763 | `0763-protected-wins-register-the-integ-wins-W5-W6-W7-W8-W.patch` | arifi-fork-base | `aa9cac23a` | - | - | protected-wins: register the integ wins W5 W6 W7 W8 W12 W13 W18, join W9 W16, cover the round-2 non-wins |
| 764 | `0764-common-fit-block-treats-draft-mtp-adaptive-as-MTP-lo.patch` | arifi-fork-base | `2adf5cc55` | - | - | common: fit block treats draft-mtp-adaptive as MTP (load crash fix) |
| 765 | `0765-tools-qwen4exp-MTP-sidecar-builders-with-Strata-s-fe.patch` | arifi-fork-base | `7dbdd0731` | Niko1221/Strata@82f46a8c8f475f001ad76d92f58f4a4f8ffb0253 (tools/mtp_fetch.py, tools/test_mtp_fetch.py, LICENSE; copied unchanged) | - | tools: qwen4exp MTP sidecar builders, with Strata's fetch tool and its MIT license |
| 766 | `0766-lane-300-F-141-producer-fix-zero-freed-KV-cells-befo.patch` | arifi-fork-base | `1ea866f86` | - | - | lane-300: F-141 producer fix - zero freed KV cells before they are read (LLAMA_ARIFI_KV_ZERO_FREED, default on) |
| 767 | `0767-lane-300-kv-zero-hardening-mid-decode-window-bound-t.patch` | arifi-fork-base | `7ecb9a918` | - | - | lane-300: kv-zero hardening - mid-decode window bound, transposed-V block assert, zeroing log |
| 768 | `0768-lane-300-LLAMA_ARIFI_KV_ZERO_FREED-default-1-0-cell-.patch` | arifi-fork-base | `2fc868a68` | - | - | lane-300: LLAMA_ARIFI_KV_ZERO_FREED default 1 -> 0 (cell 31 OO re-cut failed the pooled clause) |
| 769 | `0769-build-info-version-prints-the-arifilabs-release-and-.patch` | arifi-fork-base | `58d81805c` | - | - | build-info: --version prints the arifilabs release and its upstream base |
| 770 | `0770-docs-lane-299-runbook-NMSE-op-tests-miss-n_tokens-2..patch` | arifi-fork-base | `417d26b36` | - | - | docs: lane-299 runbook - NMSE op tests miss n_tokens 2..8 bit-exactness; verify-window census recipe |
| 771 | `0771-lane-302-lever-9-MUL_MAT_ID-expert-gather-for-Q2_0-a.patch` | arifi-fork-base | `bfa4b73e2` | - | - | lane-302 lever 9: MUL_MAT_ID expert gather for Q2_0 and every legacy / q8_1 MMVQ id type |
| 772 | `0772-lane-302-lever-9-GGML_ARIFI_MOE_GATHER_EXT-default-O.patch` | arifi-fork-base | `5991bb1e3` | - | - | lane-302 lever 9: GGML_ARIFI_MOE_GATHER_EXT default OFF (opt-in, measured slower on Q2_0) |
| 773 | `0773-protected-wins-register-the-v0.1.3.1-opt-in-arms-KV_.patch` | arifi-fork-base | `a5ff6df2b` | - | - | protected-wins: register the v0.1.3.1 opt-in arms (KV_ZERO_FREED entry, GATHER_EXT joins the slot-major gather) |
| 774 | `0774-lane-300-LLAMA_ARIFI_KV_ZERO_FREED-default-0-1-HQ84-.patch` | arifi-fork-base | `b0b41ed54` | - | - | lane-300: LLAMA_ARIFI_KV_ZERO_FREED default 0 -> 1 (HQ84 item 3: kv-zero ships default ON in the point release) |
| 775 | `0775-protected-wins-kv-zero-freed-cells-is-default-ON-in-.patch` | arifi-fork-base | `410f8e275` | - | - | protected-wins: kv-zero-freed-cells is default ON in v0.1.3.1 (b0b41ed548), id drops -opt-in |
| 776 | `0776-tools-replace-local-C-ArifiLabs-paths-with-repo-rela.patch` | arifi-fork-base | `66f36df66` | - | - | tools: replace local C:/ArifiLabs paths with repo-relative ones in hand-written tools (HQ84 item 9) |
| 777 | `0777-vulkan-moe-cache-mask-the-IQ3_S-sub-block-scale-nibb.patch` | arifi-fork-base | `86d8ca4c2` | - | - | vulkan moe-cache: mask the IQ3_S sub-block scale nibble in moe_cache_mv |
| 778 | `0778-tests-per-type-check-of-the-Vulkan-MoE-cache-matvec-.patch` | arifi-fork-base | `3ef4ebe6d` | - | - | tests: per-type check of the Vulkan MoE cache matvec against the CPU |
| 779 | `0779-tests-mul_mat-mul_mat_id-cases-on-a-slice-of-a-large.patch` | arifi-fork-base | `d2d304af9` | ggml-org/llama.cpp@52624716156a25af417b616e23bc712452d9396c (#28956) | - | tests: mul_mat / mul_mat_id cases on a slice of a larger cache (RED on 7156c61607) |
| 780 | `0780-vulkan-read-the-batch-stride-of-an-in-place-src0-src.patch` | ternary-g128 | `6ce45c9a6` | ggml-org/llama.cpp@52624716156a25af417b616e23bc712452d9396c (#28956) | - | vulkan: read the batch stride of an in-place src0/src1 from nb[2] (mul_mat on a cache slice) |
| 781 | `0781-tests-matmul-wide-soft_max-matmul-on-the-same-input-.patch` | arifi-fork-base | `d1e94cd73` | ggml-org/llama.cpp@806eee9841 (#29591, test from PR text, not merged upstream) | - | tests: matmul -> wide soft_max -> matmul on the same input (prealloc_y reuse, RED before #29591) |
| 782 | `0782-vulkan-fix-stale-prealloc_y-reuse-across-flash-atten.patch` | ternary-g128 | `729043be9` | ggml-org/llama.cpp@806eee9841 (#29591) | - | vulkan: fix stale prealloc_y reuse across flash attention and soft_max |
| 783 | `0783-vulkan-fix-flash-attention-Q8_1-shared-memory-write-.patch` | ternary-g128 | `76d05142b` | ggml-org/llama.cpp@6c59c40076 (#29988) | - | vulkan: fix flash attention Q8_1 shared-memory write out of bounds |
| 784 | `0784-vulkan-null-vkEnumerateInstanceVersion-guard-argsort.patch` | ternary-g128 | `57ac8b7d1` | ggml-org/llama.cpp@58cb9138e4 (#29872), ggml-org/llama.cpp@cea74625fa (#29469) | - | vulkan: null vkEnumerateInstanceVersion guard + argsort WG size capped at the pipeline count |
| 785 | `0785-tests-MUL_MAT_PREALLOC_REUSE-quantized-tolerance-and.patch` | arifi-fork-base | `ed182b41f` | ggml-org/llama.cpp@806eee9841 (#29591, test from PR text) | - | tests: MUL_MAT_PREALLOC_REUSE - quantized tolerance and forced matmul-before-soft_max order |
| 786 | `0786-tests-MUL_MAT_PREALLOC_REUSE-v2-strided-input-forces.patch` | arifi-fork-base | `d1bd56ba2` | - | - | tests: MUL_MAT_PREALLOC_REUSE v2 - strided input forces staging, FA mask-opt arm, noise-free copy |
| 787 | `0787-tests-gguf-handcrafted-file-whose-tensor-size-wraps-.patch` | arifi-fork-base | `484df5e38` | ggml-org/llama.cpp@a6ea155d3d (#26979), test part | - | tests: gguf handcrafted file whose tensor size wraps after padding (RED on 7156c61607) |
| 788 | `0788-gguf-reject-crafted-files-that-hang-or-overflow-the-.patch` | arifi-fork-base | `504b41e20` | ggml-org/llama.cpp@c13e04e1dd (#29598), ggml-org/llama.cpp@a6ea155d3d (#26979), ggml-org/llama.cpp@2149c00f44 (#29384) | - | gguf: reject crafted files that hang or overflow the loader |
| 789 | `0789-tests-CLAMP-on-a-non-contiguous-view-RED-on-7156c616.patch` | arifi-fork-base | `cdbd475a4` | ggml-org/llama.cpp@65840ed53c (#29517), test part | - | tests: CLAMP on a non-contiguous view (RED on 7156c61607) |
| 790 | `0790-ggml-cpu-cuda-CPU-reference-fixes-CLAMP-views-soft_m.patch` | arifi-fork-base | `999a1790f` | ggml-org/llama.cpp@65840ed53c (#29517), ggml-org/llama.cpp@dd4c286f38 (#27096), ggml-org/llama.cpp@d3954b9324 (#29575), ggml-org/llama.cpp@284153e069 (#29545) | - | ggml-cpu, cuda: CPU reference fixes (CLAMP views, soft_max_back alias, get_rows_back bounds, AVX512-FP16 dot) |
| 791 | `0791-tests-MUL_MAT_PREALLOC_REUSE-runs-as-one-whole-graph.patch` | arifi-fork-base | `23b8c36b5` | - | - | tests: MUL_MAT_PREALLOC_REUSE runs as one whole graph |
| 792 | `0792-tests-draft-acceptance-stops-at-EOG-RED-on-7156c6160.patch` | arifi-fork-base | `305502a07` | - | - | tests: draft acceptance stops at EOG (RED on 7156c61607) |
| 793 | `0793-common-stop-accepting-draft-tokens-at-EOG.patch` | arifi-fork-base | `1632f134e` | ggml-org/llama.cpp@d280808f5d (#29638) | - | common : stop accepting draft tokens at EOG |
| 794 | `0794-llama-preserve-original-batch-order-for-speculative-.patch` | arifi-fork-base | `10c7a2816` | ggml-org/llama.cpp@4453b535fd (#29019) | - | llama : preserve original batch order for speculative decoding layer inputs |
| 795 | `0795-tests-state-restore-must-refuse-a-different-KV-rotat.patch` | arifi-fork-base | `8eac38cd7` | - | - | tests: state restore must refuse a different KV rotation (RED on 7156c61607) |
| 796 | `0796-kv-cache-save-the-exact-KV-rotation-with-the-state-r.patch` | arifi-fork-base | `9905714c2` | ggml-org/llama.cpp@210791069b (#28498) | - | kv-cache: save the exact KV rotation with the state, reject a mismatched restore |
| 797 | `0797-lane-301-lever-4-fuse-the-qwen4exp-QSA-mask-chain-in.patch` | arifi-fork-base | `5543c29e1` | - | - | lane-301 lever 4: fuse the qwen4exp QSA mask chain into the Vulkan FA |
| 798 | `0798-lane-301-tests-QSA_MASK_FA-perf-mode-cases-RGP-targe.patch` | arifi-fork-base | `03a822b6e` | - | - | lane-301 tests: QSA_MASK_FA perf-mode cases (RGP target for lever 4) |
| 799 | `0799-lane-301-protected-win-vulkan-qsa-mask-fa-fusion-for.patch` | arifi-fork-base | `8632241b9` | - | - | lane-301: protected-win vulkan-qsa-mask-fa-fusion for lever 4 (f42f16e38c) |
| 800 | `0800-ggml-collect-all-input-tensors-into-graph_inputs.patch` | arifi-fork-base | `00cea8047` | ggml-org/llama.cpp@8019dc563b (#29634) | - | ggml : collect all input tensors into graph_inputs |
| 801 | `0801-ggml-require-input-tensors-to-be-GGML_OP_NONE.patch` | arifi-fork-base | `dbf3364ad` | ggml-org/llama.cpp@b5cf8ce02a (#29647) | - | ggml : require input tensors to be GGML_OP_NONE |
| 802 | `0802-llama-re-reserve-the-sched-when-the-nextn-extraction.patch` | arifi-fork-base | `81bf18af2` | ggml-org/llama.cpp@1a3011cc0c (#30020) | - | llama : re-reserve the sched when the nextn extraction flags change |
| 803 | `0803-glm5-next-give-dead-indexer-slots-unique-scatter-row.patch` | arifi-fork-base | `f433dfe70` | ggml-org/llama.cpp@05af0d2b13 (#29745) | - | glm5-next: give dead indexer slots unique scatter rows |

## Measured effect, per patch

Reproduced verbatim from each commit's `Measured-effect:` trailer. `UNMEASURED` is a
legal and honest value; an absent trailer is a gap and is named as one.

| Patch | Measured-effect |
|---|---|
| `0001-chore-establish-ArifiLabs-fork-identity-and-retained.patch` | Documentation and retained-notice baseline only; no runtime change. |
| `0002-docs-add-options-registry-and-unified-A-F-catalog.patch` | Documents placement-specific results and negative findings; no runtime change. |
| `0003-ci-add-Windows-Vulkan-and-bench-smoke-placeholders.patch` | CI contract only; no benchmark result is produced. |
| `0004-docs-require-loud-fail-upstream-currency-procedure.patch` | Process guardrail only; acceptance still requires a clean rebase, build, and bench judge. |
| `0005-powerinfer-add-MoE-pipeline-fused-sparse-ggml-ops-se.patch` | Enables the streamed-MoE op set; no runtime change until wired. |
| `0006-powerinfer-CPU-kernels-dispatch-for-streamed-MoE-ops.patch` | Kernel dispatch only; measured effects land with the hook. |
| `0007-powerinfer-vendor-streaming-library-incl.-lane-110-W.patch` | Carries io_uring->Win32 read path + -mprefer-vector-width=128 AVX-spill cure (novel MinGW codegen finding, RESULTS lane-110). |
| `0008-build-wire-powerinfer-lib-AVX-spill-cure-into-CMake-.patch` | Build-system only. |
| `0009-powerinfer-expert-bundle-loader-GENERATE_EXPERT_BUND.patch` | First disk-streamed sparse inference on Windows proven on this path (RESULTS M2a). |
| `0010-powerinfer-generic-MoE-streaming-hook-SiLU-op-type-s.patch` | Warm-regime fix: server decode climbs 5.99/9.79/10.48/12.31 t/s vs fork best 9.00 (RESULTS lane-110C). |
| `0011-powerinfer-per-ubatch-pipeline-init-guarded-reuse-PR.patch` | Reuse: 9/263 rebuilds, init 2472us->0-2us (~2.5ms/token), coherent (RESULTS lane-110C round 2). |
| `0012-powerinfer-repack-carve-out-for-streamed-tensors-ser.patch` | Fixes garbage output from AARCH64 repack of streamed experts (RESULTS rung2). |
| `0013-powerinfer-down-projection-transpose-fix-series-0008.patch` | Correctness fix documented in script header (RESULTS lane-110B). |
| `0014-powerinfer-GPU-safe-staging-CPU-zero-copy-LANE110_PR.patch` | GPU -ngl99 -cmoe clean 4.9 t/s; CPU zero-copy kept (RESULTS lane-110B/C). |
| `0015-lane-110-document-UI-ON-as-local-default-build-varia.patch` | Build-variant documentation; UI embed verified with static runtime |
| `0016-community-llama-pin-mmap-backed-CPU-weights-for-fast.patch` | inert on GPU-resident unified memory (ki-prefetch-experts-conditionality); carried for discrete-GPU + streamed-path trial owed |
| `0017-community-ggml-overlap-offloaded-expert-weight-uploa.patch` | inert on GPU-resident unified memory (ki-prefetch-experts-conditionality); carried for discrete-GPU + streamed-path trial owed |
| `0018-community-ggml-size-prefetch-slots-per-layer-and-fix.patch` | inert on GPU-resident unified memory (ki-prefetch-experts-conditionality); carried for discrete-GPU + streamed-path trial owed |
| `0019-tools-gguf-add-fail-closed-g128-retagger.patch` | tooling only (no runtime engine change) |
| `0020-tools-gguf-retagger-v2-structure-only-parser-no-tens.patch` | tooling only (no runtime engine change); unblocks g128 gate-ON key injection |
| `0021-g128-engine-port-applied-BUILD-BLOCKED-on-copy_to_qu.patch` | g128 engine port staged; build-blocked at one Vulkan shader (Sol owed) |
| `0022-vulkan-fix-f16-promotion-in-q2_0_g128-copy_to_quant.patch` | build fix only |
| `0023-vulkan-g128-shader-gen-parity-MMQ-helper-completion.patch` | build fix only |
| `0024-vulkan-g128-DMMV-repack4-byte-oriented-rewrite.patch` | build fix only |
| `0025-loader-g128-include-get_next_tensor-ctx-arg.patch` | build fix only |
| `0026-loader-use-public-ggml_blck_size-for-g128-drop-inter.patch` | build fix only |
| `0027-tools-gguf-retag-v3-verified-type-id-rewrite-42-43.patch` | build fix only |
| `0028-loader-accept-native-Q2_0_G128-tensors.patch` | build fix only |
| `0029-ggml-cpu-dispatch-Q2_0_G128-at-the-7-Q2_0-sites.patch` | build fix only |
| `0030-port-f16-recurrent-S-state-env-gated-GGML_RECURRENT_.patch` | *(no Measured-effect trailer)* |
| `0031-port-M-RoPE-embedded-batch-position-guard.patch` | *(no Measured-effect trailer)* |
| `0032-docs-phase-3a-OPTIONS-REGISTRY-rows-M-RoPE-f16-recur.patch` | *(no Measured-effect trailer)* |
| `0033-port-Windows-IOCP-async-expert-bundle-reads-M2b.patch` | *(no Measured-effect trailer)* |
| `0034-feat-POWERINFER_IOCP-runtime-toggle-for-the-Windows-.patch` | *(no Measured-effect trailer)* |
| `0035-docs-OPTIONS-REGISTRY-POWERINFER_IOCP-measured-row-s.patch` | *(no Measured-effect trailer)* |
| `0036-docs-correct-POWERINFER_IOCP-row-A-B-was-noise-direc.patch` | *(no Measured-effect trailer)* |
| `0037-patches-generate-the-full-auditable-38-patch-series-.patch` | structure and tooling only; no engine behaviour changed. Series replays to a byte-identical tree (verified). |
| `0038-tools-add-arifi-sync-the-currency-series-and-upstrea.patch` | tooling only; no engine behaviour changed. series check replays 38 patches to a byte-identical tree; currency fetched 4 remotes + 2 local checkouts and reported real drift. |
| `0039-docs-README-section-a-GitHub-visitor-can-read-cold.patch` | documentation only; every figure quoted is copied from docs/OPTIONS-REGISTRY.md or committed trial evidence, none newly measured. |
| `0040-patches-move-banked-PrismML-source-patches-out-of-th.patch` | file relocation only; patch contents byte-identical, no engine behaviour changed. |
| `0041-patches-make-the-series-generator-implement-its-own-.patch` | tooling and generated-artifact rules only; no engine behaviour changed. series check --ref master goes 1 -> 0; 40/40 patches byte-identical to a fresh generation and git am replays them to a tree identical to master outside patches/series. |
| `0042-patches-stop-the-manifest-deriving-anything-from-exc.patch` | tooling only; no engine behaviour changed. Second consecutive regeneration now produces zero diff. |
| `0043-port-Q1_0-repack-scaffolding-Arm-NEON-DP-GEMV-GEMM-k.patch` | *(no Measured-effect trailer)* |
| `0044-port-native-x86-Q2_0-vec_dot-AVX512-VNNI-AVX-VNNI-dr.patch` | *(no Measured-effect trailer)* |
| `0045-port-x86-AVX512-VNNI-repack-GEMV-GEMM-for-Q1_0-and-Q.patch` | *(no Measured-effect trailer)* |
| `0046-feat-GGML_ARIFI_VNNI_REPACK-runtime-toggle-explicit-.patch` | *(no Measured-effect trailer)* |
| `0047-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_VNNI_REPACK.patch` | *(no Measured-effect trailer)* |
| `0048-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-CORRECT.patch` | *(no Measured-effect trailer)* |
| `0049-fix-ggml-cpu-parameterize-Q2_0-GEMM-activation-strid.patch` | *(no Measured-effect trailer)* |
| `0050-fix-ggml-cpu-same-g64-stride-defect-in-the-ported-x8.patch` | *(no Measured-effect trailer)* |
| `0051-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-NaN-FIX.patch` | *(no Measured-effect trailer)* |
| `0052-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-bit-div.patch` | documentation only; this commit changes one line of docs/OPTIONS-REGISTRY.md and no code, build option or engine behaviour. The numbers the row records were measured by the run it documents, on the ArifiLabs rig only (Beelink SER7, Radeon 780M, Windows): CPU-only prompt +326.7% / +441.8% and decode +19.9% / +23.6%; Vulkan prompt -77.1% / -88.1% and decode +20.1% / +28.4%. UNMEASURED on any other machine. |
| `0053-docs-correct-the-VNNI-row-s-base-commit-labelling-f7.patch` | documentation only; this commit changes one line of docs/OPTIONS-REGISTRY.md and no code, build option or engine behaviour. It corrects a base-commit label; no recorded measurement moves. UNMEASURED - a labelling correction has nothing to measure. |
| `0054-docs-fix-two-stray-table-breaking-pipes-in-the-VNNI-.patch` | documentation only; this commit changes one line of docs/OPTIONS-REGISTRY.md and no code, build option or engine behaviour. It repairs GFM table rendering; no recorded measurement moves. UNMEASURED - a Markdown rendering fix has nothing to measure. |
| `0055-docs-OPTIONS-REGISTRY-VNNI-row-topology-cell-now-ref.patch` | documentation only; no runtime change. |
| `0056-ggml-cpu-GGML_ARIFI_VNNI_REPACK-defaults-to-OFF-dive.patch` | default-only change; neither kernel is touched. An unset environment now takes the no-repack branch, which on the default Vulkan build avoids the measured -77.1%/-88.1% prompt regression and forgoes the +20.1%/+28.4% decode gain. GGML_ARIFI_VNNI_REPACK=1 restores the previous behaviour exactly. Engagement re-verified in both arms; performance deliberately not re-measured, since the flip changes branch selection and not either kernel. |
| `0057-docs-fork-wide-GGUF-type-ID-allocation-contract-keys.patch` | documentation only; no engine behaviour changed |
| `0058-ggml-reserve-the-ArifiLabs-type-ID-blocks-at-the-enu.patch` | comment only; no code generated differs |
| `0059-ggml-ROCmFP4-ROCmFPX-weight-formats-behind-GGML_ARIF.patch` | OFF (default) is a no-op on every existing path - no code outside the new ifdefs changes behaviour, and the only unconditional change is GGML_TYPE_COUNT 44 to 256, which grows the ggml type_traits tables and the twelve bool[COUNT] plus eight pipeline[COUNT] arrays in vk_device_struct by 212 ids (estimated ~320 KB per Vulkan device, NOT measured) and lengthens two device-init loops from 44 to 256 trivial iterations. ON was verified end to end on a native artifact - models/rocmfpx-trial/ornith-9b-mtp-kl-Q4_0_ROCMFP4_COHERENT.gguf (257 tensors at type-id 100) loaded in llama-server per F-085 and completed "The capital of France is" with " Paris." at 3.24 tok/s on -ngl 0. OFF refuses the same file fail-closed: "tensor 'output.weight' of type 100 ((null)) has 4096 elements per row, not a multiple of block size (0)". Both ROCmFPX conformance suites pass on our compiler against the ported sources (test_rocmfpx: fp3/fp6/fp8 mse and imatrix weighting; test_rocmfp2_reference: 32520 checks, fingerprint 78389c1c8a0a57a2). No ON-path performance number is claimed - UNMEASURED. |
| `0060-llama-ROCmFPX-file-types-quantizer-wiring-and-the-gg.patch` | no behaviour change on any existing file type; the C wiring is inside GGML_ARIFI_ROCMFPX_FORMATS and the default build is OFF. The ftype enum values and their name strings are unconditional because a file may carry them regardless of what this build can decode. gguf-py has no build gate and is unconditional by nature. Quantizer output for these types is UNMEASURED - no round-trip quantization was performed. |
| `0061-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_ROCMFPX_FOR.patch` | documentation only; no engine behaviour changed |
| `0062-tools-record-GGML_ARIFI_ROCMFPX_FORMATS-in-the-build.patch` | generated artifact only; no source or engine behaviour changed |
| `0063-docs-make-the-fork-buildable-by-someone-who-did-not-.patch` | *(no Measured-effect trailer)* |
| `0064-feat-arifi-profile-and-a-CPU-only-VNNI-repack-startu.patch` | FUNCTIONAL, NOT TIMED - no throughput number was produced or claimed by this work, and no shipped default moved. Verified on build-vulkan (arifi_sync build exit 0, recipe diff PASS before and after) with qwen2.5-0.5b-instruct-Q2_0-g64.gguf: -dev none --arifi-profile cpu-only puts 95.98 MiB into CPU_REPACK and drops the rejected-tensor count from 290 to 122, where the same run without the profile leaves CPU_REPACK at 0.00 MiB. The advisory prints exactly one block on the CPU-only path (168 Q1_0/Q2_0 tensors) and is silent on the Vulkan path, when GGML_ARIFI_VNNI_REPACK is already set, and on the no_alloc pass. A run with no new flags produces no new output at all. |
| `0065-sync-bump-base-to-upstream-b10173-e9fa0781f-series-r.patch` | three-arm bench 2026-07-28 — F>=O all cells; U refuses g128 (expected); gemma fit-path F 39.1 t/s vs stock-upstream 15-21 t/s |
| `0066-sync-currency-GREEN-TurboQuant-checkouts-pinned-at-t.patch` | 'currency' subcommand now PASSES on every source (first green run) |
| `0067-fix-ggml-cpu-x86-don-t-pass-__m256i-by-value-across-.patch` | *(no Measured-effect trailer)* |
| `0068-ggml-TurboQuant-TQ3_1S-TQ4_1S-weight-formats-behind-.patch` | *(no Measured-effect trailer)* |
| `0069-docs-CHANGELOG-and-FINDINGS-the-public-trail-a-stran.patch` | documentation only; no engine behaviour changed. |
| `0070-docs-USAGE-a-task-oriented-guide-for-humans-not-an-o.patch` | *(no Measured-effect trailer)* |
| `0071-ggml-cpu-x86-use-the-row-stride-parameter-in-repack-.patch` | *(no Measured-effect trailer)* |
| `0072-ggml-cpu-fuse-the-TurboQuant-dot-product-transform-t.patch` | *(no Measured-effect trailer)* |
| `0073-docs-FINDINGS-F-09-the-repack-GPU-trade-off-is-a-com.patch` | *(no Measured-effect trailer)* |
| `0074-docs-neutralise-internal-governance-vocabulary-expla.patch` | documentation only; no engine behaviour changed. |
| `0075-docs-FINDINGS-record-why-the-repack-auto-enable-was-.patch` | no code change - a deliberate non-change, with its evidence. |
| `0076-provenance-catch-the-silent-trailer-block-break-inst.patch` | tooling and documentation only, no engine behaviour changed. Hook verified
  on three crafted messages covering reject/accept/no-op. |
| `0077-gitattributes-pin-.githooks-to-LF-so-the-hook-runs-o.patch` | packaging only, no engine behaviour changed. Verified the file is stored with
  LF after re-adding under the new attribute. |
| `0078-githooks-force-a-normalized-blob-for-the-commit-msg-.patch` | packaging only, no engine behaviour changed. Verified against the RAW object
  via git cat-file - git show applies the checkout filter and reported the opposite of the truth. |
| `0079-docs-FINDINGS-F-09-exit-1-is-the-direction-dual-resi.patch` | analysis only, no code change. The numbers quoted are the already-recorded
  repack ON/OFF measurements, not new ones. |
| `0080-repack-dual-residency-GGML_ARIFI_VNNI_REPACK-2-keeps.patch` | correctness and placement only; the SPEED claim is deliberately not made here
  and is owed from a timed run. llama-server, 3 models (4 / 72 / 168 Q2_0 tensors) x 2 offload
  variants x 3 modes x 2 prompt shapes x 3 rolls, temp 0, top_k 1, seed 42, fresh server per arm.
  With --no-op-offload - which pins mode 2's prefill to the CPU like mode 1's, isolating the
  shadow from where prefill ran - mode 2 is BIT-IDENTICAL to mode 1 on all 6 cells: tokens
  identical, max \|delta logprob\| = 0.000000000, deterministic 3/3. Under default offload mode 2's
  graph splits track MODE 0 (335) on all three models and never mode 1 (339/312/286), and mode 2
  is still bit-identical to mode 1 on sub-threshold prompts. Shadow bytes equal mode 1's
  CPU_REPACK buffer exactly - 4.68 / 84.16 / 95.98 MiB over 4 / 72 / 168 tensors - an independent
  check that the predicate selects the same tensor set. Divergence from mode 0 is never larger
  than mode 1's over the common prefix; on the 4-tensor control model all three modes are
  token-identical. Evidence: trial-evidence/110/dual-residency/ |
| `0081-repack-dual-residency-measured-both-wins-held-fix-th.patch` | decode +22.0% (mixed) and +23.7% (all-Q2_0) against mode 0, ranges DISJOINT, while
  prefill is held at baseline instead of collapsing -65%/-80% as mode 1 does. Shadow cost 4.68 /
  84.16 / 95.98 MiB on the 4- / 72- / 168-tensor models, byte-for-byte equal to what mode 1 places
  in CPU_REPACK. Precision unchanged: with --no-op-offload, which isolates the shadow from where
  prefill ran, mode 2 is bit-identical to mode 1 on all six model x prompt cells (max \|delta
  logprob\| 0.000000000, deterministic 3/3). Re-verified on the shipped binary after the hot-path
  fix. Evidence: research/local-inference/trial-evidence/110/dual-residency/ |
| `0082-repack-fix-the-shadow-tensor-COUNT-in-the-accounting.patch` | none on any code path - the changed line is a log format string. The claim it
  supports is now verified by tensor name rather than inferred from byte totals. |
| `0083-repack-CPU-kernels-for-the-128-group-ternary-format.patch` | correctness only; speed UNMEASURED. RIG-A (Beelink SER7,
  Ryzen 7840HS + Radeon 780M), llama-server, Ternary-Bonsai-8B-Q2_0.g128.gguf,
  -ngl 0 --no-host -c 2048 -t 8, temp 0, token-id comparison, 3 rolls per cell:
  repacked output token-identical to the scalar path on both a GEMV- and a
  GEMM-driving prompt, max \|delta logprob\| 0.021-0.040, inside the F-05 band
  and below this fork's own g64 ternary figure of 0.061. Repack buffer 1759.50
  MiB = 252 of 254 ternary tensors. Dual residency covers the new type: 252
  shadows, graph splits tracking mode 0 rather than mode 1, bit-identical to
  mode 1 under --no-op-offload. The shared g64 kernels were rewritten here, so
  they were re-run against the pre-change answer key: 36 cells, every token
  identical, every logprob delta 0.000000000. |
| `0084-tests-direct-equivalence-test-for-the-ternary-repack.patch` | UNMEASURED - test-only, no runtime path changes. The test
  reports max \|err\| 1e-6 to 4e-6 against a 1e-5 relative tolerance across all
  ten kernel/group-size combinations, and 43064.94 against a 0.000053
  tolerance for the deliberate g128-through-g64 negative control. |
| `0085-docs-the-g128-repack-SPEED-numbers-measured.patch` | no code changed; documents measured throughput. decode 2.01 ->
  6.67/6.69 tok/s and prompt 59.09 -> 17.44/85.84 on RIG-A, n=12 per cell, all
  ranges disjoint. |
| `0086-docs-frame-the-g128-repack-numbers-as-a-CPU-tier-res.patch` | no code changed. Adds context to figures already measured; names
  the same-binary GPU-vs-CPU arm as owed rather than implying it was run. |
| `0087-docs-publish-the-power-plan-finding-as-F-12-and-bind.patch` | UNMEASURED by this commit - documentation only. The figures it
  publishes were measured 2026-07-29 and are taken from the register row that
  owns the issue rather than from a prose retelling of it. |
| `0088-F-06-measure-the-GPU-tier-on-one-binary-and-count-th.patch` | RIG-A, llama-server, Ternary-Bonsai-8B-Q2_0.g128.gguf, 519-token
  prefill, n_predict 128, three interleaved replicates, one binary. GPU (-ngl 99)
  327.38 prompt / 34.40 decode; CPU floor 58.15 / 1.88; CPU dual 85.62 / 6.56.
  The two arms with published values reproduced within 6% as the run's own
  known-answer control. Placement counted separately with GGML_SCHED_DEBUG=2:
  mode 2 identical to mode 0 across all 24 graph dumps, CPU share 4 of 253. |
| `0089-registry-the-dual-residency-prompt-mechanism-is-meas.patch` | UNMEASURED by this commit - it records the measurements made in
  the preceding commit rather than making new ones. |
| `0090-powerinfer-MAX_N_CACHED-admitted-two-values-that-bre.patch` | no throughput effect by construction - the default (6144) and
  unset paths are unchanged. Behaviour verified on the built llama-server on
  RIG-A: MAX_N_CACHED=512 and MAX_N_CACHED=-1 are refused with the new message,
  513 and unset start normally. Before the change the same binary shape accepted
  512, and accepted -1 as 18446744073709551615. |
| `0091-rows-per-workgroup-on-g128-a-null-and-F-10-confirmed.patch` | *(no Measured-effect trailer)* |
| `0092-registry-our-DSpark-verdict-was-never-ours-the-draft.patch` | no throughput measurement was possible -- that IS the result.
  Evidence is the two load failures, quoted verbatim, plus the offset arithmetic
  in trial-evidence/110/dspark/inspect_drafter.py. |
| `0093-speculative-DFlash-DSpark-drafters-have-no-vocab-tak.patch` | *(no Measured-effect trailer)* |
| `0094-expert-cache-stride-the-bundle-by-the-PADDED-matrix-.patch` | *(no Measured-effect trailer)* |
| `0095-powerinfer-POWERINFER_NO_BUFFERING-1-the-Windows-cou.patch` | *(no Measured-effect trailer)* |
| `0096-expert-cache-POWERINFER_EXPERT_HEATMAP-measure-acces.patch` | *(no Measured-effect trailer)* |
| `0097-ours-1163cb34939fe4a9cb07aec034c5954144497ae9.patch.patch` | *(no Measured-effect trailer)* |
| `0098-1-core-types-0001-WIP-add-TurboQuant-KV-cache-types-.patch` | *(no Measured-effect trailer)* |
| `0099-1-core-types-0002-ggml-port-TurboQuant-core-quant-ty.patch` | *(no Measured-effect trailer)* |
| `0100-2-cpu-reference-0001-ggml-port-TurboQuant-core-quant.patch` | *(no Measured-effect trailer)* |
| `0101-2-cpu-reference-0007-ggml-cpu-declare-turbo3_cpu_wht.patch` | *(no Measured-effect trailer)* |
| `0102-2-cpu-reference-0008-merge-fix-post-merge-build-and-.patch` | *(no Measured-effect trailer)* |
| `0103-3-vulkan-0001-vulkan-metal-hip-add-TurboQuant-kernel.patch` | *(no Measured-effect trailer)* |
| `0104-3-vulkan-0002-vulkan-port-fork-241-wave64-ballot-fix.patch` | *(no Measured-effect trailer)* |
| `0105-3-vulkan-0003-fix-close-complete-audit-findings-7-po.patch` | *(no Measured-effect trailer)* |
| `0106-3-vulkan-0004-vulkan-reconstruct-supports_op-after-r.patch` | *(no Measured-effect trailer)* |
| `0107-3-vulkan-0006-vulkan-restore-TQ4_1S-weight-type-wiri.patch` | *(no Measured-effect trailer)* |
| `0108-3-vulkan-0007-vulkan-add-the-TQ3_1S-weight-type.patc.patch` | *(no Measured-effect trailer)* |
| `0109-3-vulkan-0008-vulkan-reject-MUL_MAT_ID-for-the-Turbo.patch` | *(no Measured-effect trailer)* |
| `0110-3-vulkan-0013-vulkan-TQ-rotated-matmul-shader-ground.patch` | *(no Measured-effect trailer)* |
| `0111-3-vulkan-0015-vulkan-create-the-TQ-activation-rotate.patch` | *(no Measured-effect trailer)* |
| `0112-3-vulkan-0017-vulkan-rotate-the-staged-activation-co.patch` | *(no Measured-effect trailer)* |
| `0113-3-vulkan-0018-vulkan-register-the-TQ-rotated-mul_mm_.patch` | *(no Measured-effect trailer)* |
| `0114-3-vulkan-0021-vulkan-fix-SET_ROWS-block-decompositio.patch` | *(no Measured-effect trailer)* |
| `0115-4-other-backends-0006-laguna-MoE-down-proj-f16-overf.patch` | *(no Measured-effect trailer)* |
| `0116-4-other-backends-0007-cuda-first-class-MoE-path-plai.patch` | *(no Measured-effect trailer)* |
| `0117-4-other-backends-0008-hip-add-mixed-f16-bf16-q8_0-fa.patch` | *(no Measured-effect trailer)* |
| `0118-4-other-backends-0009-cuda-revert-GQA-ratio-dispatch.patch` | *(no Measured-effect trailer)* |
| `0119-4-other-backends-0010-cuda-restore-Volta-GQA-modulo-.patch` | *(no Measured-effect trailer)* |
| `0120-4-other-backends-0015-cuda-rework-MoE-expert-cache-e.patch` | *(no Measured-effect trailer)* |
| `0121-4-other-backends-0016-cuda-fix-MoE-cache-capacity-ac.patch` | *(no Measured-effect trailer)* |
| `0122-4-other-backends-0017-cuda-fix-MoE-cache-scratch-bud.patch` | *(no Measured-effect trailer)* |
| `0123-4-other-backends-0018-cuda-bound-MoE-MMV-tail-row-re.patch` | *(no Measured-effect trailer)* |
| `0124-4-other-backends-0019-cuda-tune-MoE-cache-CPU-overla.patch` | *(no Measured-effect trailer)* |
| `0125-4-other-backends-0020-cuda-adapt-MoE-cache-admission.patch` | *(no Measured-effect trailer)* |
| `0126-4-other-backends-0021-cuda-align-MoE-cache-pool-allo.patch` | *(no Measured-effect trailer)* |
| `0127-4-other-backends-0022-cuda-prefer-generic-MMV-for-co.patch` | *(no Measured-effect trailer)* |
| `0128-4-other-backends-0023-cuda-parallelize-MoE-cache-fil.patch` | *(no Measured-effect trailer)* |
| `0129-4-other-backends-0024-cuda-fuse-cached-MoE-SwiGLU-ro.patch` | *(no Measured-effect trailer)* |
| `0130-4-other-backends-0025-cuda-aggregate-small-MoE-tenso.patch` | *(no Measured-effect trailer)* |
| `0131-4-other-backends-0026-cuda-bound-automatic-MoE-cache.patch` | *(no Measured-effect trailer)* |
| `0132-4-other-backends-0027-cuda-reject-undersized-automat.patch` | *(no Measured-effect trailer)* |
| `0133-4-other-backends-0028-cuda-enable-multi-token-MoE-ca.patch` | *(no Measured-effect trailer)* |
| `0134-4-other-backends-0029-common-surface-MoE-cache-activ.patch` | *(no Measured-effect trailer)* |
| `0135-4-other-backends-0030-cuda-accelerate-complete-MoE-c.patch` | *(no Measured-effect trailer)* |
| `0136-4-other-backends-0031-cuda-report-oversized-MoE-cach.patch` | *(no Measured-effect trailer)* |
| `0137-4-other-backends-0032-cuda-clarify-MoE-cache-session.patch` | *(no Measured-effect trailer)* |
| `0138-4-other-backends-0033-cuda-report-MoE-cache-pair-res.patch` | *(no Measured-effect trailer)* |
| `0139-4-other-backends-0034-cuda-pack-MoE-cache-dispatch-i.patch` | *(no Measured-effect trailer)* |
| `0140-4-other-backends-0037-metal-port-TQ_NO_ROTATE-escape.patch` | *(no Measured-effect trailer)* |
| `0141-4-other-backends-0039-moe-cache-enable-HIP-backend-b.patch` | *(no Measured-effect trailer)* |
| `0142-4-other-backends-0040-moe-cache-count-dispatch-conte.patch` | *(no Measured-effect trailer)* |
| `0143-4-other-backends-0041-moe-cache-provider-registry-wi.patch` | *(no Measured-effect trailer)* |
| `0144-4-other-backends-0042-moe-cache-audit-fixes-H1-F1-F2.patch` | *(no Measured-effect trailer)* |
| `0145-4-other-backends-0043-moe-cache-fix-registry-build-I.patch` | *(no Measured-effect trailer)* |
| `0146-4-other-backends-0044-moe-cache-allow-automatic-mode.patch` | *(no Measured-effect trailer)* |
| `0147-4-other-backends-0045-cuda-fix-HIP-MoE-cache-compati.patch` | *(no Measured-effect trailer)* |
| `0148-4-other-backends-0046-moe-cache-add-logging-at-silen.patch` | *(no Measured-effect trailer)* |
| `0149-4-other-backends-0048-cuda-fit-fix-two-Werror-build-.patch` | *(no Measured-effect trailer)* |
| `0150-4-other-backends-0049-sycl-drop-the-duplicated-nthre.patch` | *(no Measured-effect trailer)* |
| `0151-4-other-backends-0050-fattn-vec-split-the-turbo-K-do.patch` | *(no Measured-effect trailer)* |
| `0152-5-models-server-0001-WIP-add-TurboQuant-KV-cache-typ.patch` | *(no Measured-effect trailer)* |
| `0153-5-models-server-0003-cli-break-interactive-loop-on-s.patch` | *(no Measured-effect trailer)* |
| `0154-5-models-server-0004-tools-expose-TQ3_1S-TQ4_1S-in-q.patch` | *(no Measured-effect trailer)* |
| `0155-5-models-server-0007-llama-support-DeepSeek-V4-tenso.patch` | *(no Measured-effect trailer)* |
| `0156-5-models-server-0008-llama-mirror-DS4-q_a-kv-down-pr.patch` | *(no Measured-effect trailer)* |
| `0157-5-models-server-0009-glm-dsa-guard-lightning-indexer.patch` | *(no Measured-effect trailer)* |
| `0158-5-models-server-0010-laguna-add-arch-tables-llm_arch.patch` | *(no Measured-effect trailer)* |
| `0159-5-models-server-0011-laguna-add-model-class-hparams-.patch` | *(no Measured-effect trailer)* |
| `0160-5-models-server-0013-laguna-MoE-down-proj-f16-overfl.patch` | *(no Measured-effect trailer)* |
| `0161-5-models-server-0014-cuda-first-class-MoE-path-plain.patch` | *(no Measured-effect trailer)* |
| `0162-5-models-server-0015-cuda-sum-MoE-expert-outputs-on-.patch` | *(no Measured-effect trailer)* |
| `0163-5-models-server-0016-laguna-deduplicate-definitions-.patch` | *(no Measured-effect trailer)* |
| `0164-5-models-server-0017-dflash-fix-DSpark-tensor-meta-a.patch` | *(no Measured-effect trailer)* |
| `0165-5-models-server-0037-merge-fix-post-merge-build-and-.patch` | *(no Measured-effect trailer)* |
| `0166-5-models-server-0041-moe-cache-route-fit-probing-and.patch` | *(no Measured-effect trailer)* |
| `0167-5-models-server-0048-llama-bench-document-pw-prefetc.patch` | *(no Measured-effect trailer)* |
| `0168-5-models-server-0050-llama-bench-remove-stale-prefet.patch` | *(no Measured-effect trailer)* |
| `0169-5-models-server-0051-moe-cache-fix-auto-asymmetric-M.patch` | *(no Measured-effect trailer)* |
| `0170-6-tests-build-0001-tests-re-add-test-turbo-quant.c-r.patch` | *(no Measured-effect trailer)* |
| `0171-6-tests-build-0002-tests-port-fork-turbo-backend-and.patch` | *(no Measured-effect trailer)* |
| `0172-6-tests-build-0003-fix-close-complete-audit-findings.patch` | *(no Measured-effect trailer)* |
| `0173-6-tests-build-0004-laguna-tool-call-whitespace-toler.patch` | *(no Measured-effect trailer)* |
| `0174-6-tests-build-0006-ggml-webgpu-add-support-for-f16-r.patch` | *(no Measured-effect trailer)* |
| `0175-6-tests-build-0010-tests-restore-DSV4_HC_COMB-eps-sw.patch` | *(no Measured-effect trailer)* |
| `0176-6-tests-build-0011-tests-cast-float-printf-args-in-t.patch` | *(no Measured-effect trailer)* |
| `0177-6-tests-build-0012-vulkan-add-the-TQ3_1S-weight-type.patch` | *(no Measured-effect trailer)* |
| `0178-6-tests-build-0013-vulkan-reject-MUL_MAT_ID-for-the-.patch` | *(no Measured-effect trailer)* |
| `0179-6-tests-build-0014-ggml-cpu-remove-the-per-call-mall.patch` | *(no Measured-effect trailer)* |
| `0180-6-tests-build-0015-vulkan-add-mul_mat_vec_id-for-the.patch` | *(no Measured-effect trailer)* |
| `0181-6-tests-build-0016-feat-add-Vulkan-TQ4_1S-weight-pip.patch` | *(no Measured-effect trailer)* |
| `0182-6-tests-build-0017-tests-port-11-DSv4-shaped-TQ-MUL_.patch` | *(no Measured-effect trailer)* |
| `0183-6-tests-build-0019-cuda-fix-MoE-cache-capacity-accou.patch` | *(no Measured-effect trailer)* |
| `0184-6-tests-build-0020-common-preserve-implicit-MoE-cach.patch` | *(no Measured-effect trailer)* |
| `0185-6-tests-build-0021-cuda-tune-MoE-cache-CPU-overlap-a.patch` | *(no Measured-effect trailer)* |
| `0186-6-tests-build-0022-cuda-adapt-MoE-cache-admission-to.patch` | *(no Measured-effect trailer)* |
| `0187-6-tests-build-0023-cuda-align-MoE-cache-pool-allocat.patch` | *(no Measured-effect trailer)* |
| `0188-6-tests-build-0024-cuda-parallelize-MoE-cache-fills-.patch` | *(no Measured-effect trailer)* |
| `0189-6-tests-build-0025-cuda-fuse-cached-MoE-SwiGLU-rows..patch` | *(no Measured-effect trailer)* |
| `0190-6-tests-build-0026-cuda-aggregate-small-MoE-tensors-.patch` | *(no Measured-effect trailer)* |
| `0191-6-tests-build-0027-cuda-bound-automatic-MoE-cache-ad.patch` | *(no Measured-effect trailer)* |
| `0192-6-tests-build-0028-cuda-reject-undersized-automatic-.patch` | *(no Measured-effect trailer)* |
| `0193-6-tests-build-0029-cuda-enable-multi-token-MoE-cache.patch` | *(no Measured-effect trailer)* |
| `0194-6-tests-build-0030-common-surface-MoE-cache-activati.patch` | *(no Measured-effect trailer)* |
| `0195-6-tests-build-0031-cuda-accelerate-complete-MoE-cach.patch` | *(no Measured-effect trailer)* |
| `0196-6-tests-build-0032-cuda-report-oversized-MoE-cache-n.patch` | *(no Measured-effect trailer)* |
| `0197-6-tests-build-0033-tests-initialise-non-contiguous-t.patch` | *(no Measured-effect trailer)* |
| `0198-6-tests-build-0034-tests-cover-turbo-KV-flash-attent.patch` | *(no Measured-effect trailer)* |
| `0199-6-tests-build-0037-moe-cache-provider-registry-with-.patch` | *(no Measured-effect trailer)* |
| `0200-6-tests-build-0038-moe-cache-route-fit-probing-and-t.patch` | *(no Measured-effect trailer)* |
| `0201-6-tests-build-0039-moe-cache-route-context-eligibili.patch` | *(no Measured-effect trailer)* |
| `0202-6-tests-build-0040-moe-cache-allow-automatic-mode-on.patch` | *(no Measured-effect trailer)* |
| `0203-6-tests-build-0041-moe-cache-fix-auto-asymmetric-MLA.patch` | *(no Measured-effect trailer)* |
| `0204-6-tests-build-0042-test-moe-cache-accept-IGPU-device.patch` | *(no Measured-effect trailer)* |
| `0205-6-tests-build-0043-test-moe-cache-make-cache-shared-.patch` | *(no Measured-effect trailer)* |
| `0206-6-tests-build-0044-test-moe-cache-save-and-restore-t.patch` | *(no Measured-effect trailer)* |
| `0207-6-tests-build-0045-tests-tell-ctest-that-77-means-sk.patch` | *(no Measured-effect trailer)* |
| `0208-resolved-0011-fix-correct-Vulkan-turbo3-pipeline-wir.patch` | *(no Measured-effect trailer)* |
| `0209-resolved-0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-ga.patch` | *(no Measured-effect trailer)* |
| `0210-resolved-0005-merge-close-TurboQuant-parity-gaps.pat.patch` | *(no Measured-effect trailer)* |
| `0211-resolved-0036-common-surface-MoE-cache-activation.pa.patch` | *(no Measured-effect trailer)* |
| `0212-resolved-0010-feat-add-Vulkan-turbo3-KV-cache-pipeli.patch` | *(no Measured-effect trailer)* |
| `0213-resolved-0004-fix-close-complete-audit-findings-7-po.patch` | *(no Measured-effect trailer)* |
| `0214-resolved-0005-metal-restore-lost-kernel-templates-ad.patch` | *(no Measured-effect trailer)* |
| `0215-resolved-0012-metal-implement-DeepSeek-V4-hyper-conn.patch` | *(no Measured-effect trailer)* |
| `0216-resolved-0035-cuda-gate-fused-TQ-mul_mat-paths-on-co.patch` | *(no Measured-effect trailer)* |
| `0217-resolved-0036-cuda-disable-fused-TQ3_1S-mul_mat-kern.patch` | *(no Measured-effect trailer)* |
| `0218-resolved-0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HI.patch` | *(no Measured-effect trailer)* |
| `0219-resolved-0047-sycl-hip-fix-two-build-breaks-in-fork-.patch` | *(no Measured-effect trailer)* |
| `0220-resolved-0006-fix-close-complete-audit-findings-7-po.patch` | *(no Measured-effect trailer)* |
| `0221-resolved-0022-DeepseekV4-MTP-DSpark-25784.patch.patch` | *(no Measured-effect trailer)* |
| `0222-resolved-0023-dflash-fix-merge-artifacts-from-upstre.patch` | *(no Measured-effect trailer)* |
| `0223-resolved-0029-server-harden-shared-draft-device-plac.patch` | *(no Measured-effect trailer)* |
| `0224-resolved-0025-fix-server-batch-restored-checkpoint-p.patch` | *(no Measured-effect trailer)* |
| `0225-resolved-0026-cuda-rework-MoE-expert-cache-execution.patch` | *(no Measured-effect trailer)* |
| `0226-resolved-0033-server-account-for-MTP-placement-in-fi.patch` | *(no Measured-effect trailer)* |
| `0227-resolved-0035-cuda-reject-undersized-automatic-MoE-c.patch` | *(no Measured-effect trailer)* |
| `0228-resolved-0038-moe-cache-add-moe-cache-soft-mode-with.patch` | *(no Measured-effect trailer)* |
| `0229-resolved-0039-model-Muse-Glimmer-Support-26841.patch.patch` | *(no Measured-effect trailer)* |
| `0230-resolved-0043-moe-cache-audit-fixes-H1-F1-F2-A1.patc.patch` | *(no Measured-effect trailer)* |
| `0231-resolved-0044-moe-cache-restore-moe_cache-params-on-.patch` | *(no Measured-effect trailer)* |
| `0232-resolved-0046-llama-remove-dead-MSA-indexer-scaffold.patch` | *(no Measured-effect trailer)* |
| `0233-resolved-0047-moe-cache-add-logging-at-silent-failur.patch` | *(no Measured-effect trailer)* |
| `0234-resolved-0052-cuda-fit-fix-two-Werror-build-failures.patch` | *(no Measured-effect trailer)* |
| `0235-resolved-0005-test-fix-some-CI-errors-26415.patch.patch` | *(no Measured-effect trailer)* |
| `0236-resolved-remaining-conflict-markers.patch` | *(no Measured-effect trailer)* |
| `0237-layer7-0001-WIP-add-TurboQuant-KV-cache-types-turbo3.patch` | *(no Measured-effect trailer)* |
| `0238-layer7-0002-Update-GGMLQuantizationType-and-LlamaFil.patch` | *(no Measured-effect trailer)* |
| `0239-layer7-0003-ggml-port-TurboQuant-core-quant-types-an.patch` | *(no Measured-effect trailer)* |
| `0240-layer7-0005-docs-add-rebase-plan-and-TurboQuant-reci.patch` | *(no Measured-effect trailer)* |
| `0241-layer7-0007-gguf-py-dedupe-stacked-merge-artifacts-i.patch` | *(no Measured-effect trailer)* |
| `0242-layer7-0008-docs-add-KV-cache-quantization-guide-upd.patch` | *(no Measured-effect trailer)* |
| `0243-layer7-0009-vulkan-port-fork-241-wave64-ballot-fix-c.patch` | *(no Measured-effect trailer)* |
| `0244-layer7-0010-merge-close-TurboQuant-parity-gaps.patch.patch` | *(no Measured-effect trailer)* |
| `0245-layer7-0011-metal-restore-lost-kernel-templates-add-.patch` | *(no Measured-effect trailer)* |
| `0246-layer7-0014-laguna-tool-call-whitespace-tolerance-do.patch` | *(no Measured-effect trailer)* |
| `0247-layer7-0019-fix-apply-rebase-audit-fixes-stack-overf.patch` | *(no Measured-effect trailer)* |
| `0248-layer7-0020-docs-add-TurboQuant-project-overview-to-.patch` | *(no Measured-effect trailer)* |
| `0249-layer7-0021-cuda-rework-MoE-expert-cache-execution.p.patch` | *(no Measured-effect trailer)* |
| `0250-layer7-0022-docs-update-MoE-cache-validation.patch.patch` | *(no Measured-effect trailer)* |
| `0251-layer7-0023-common-preserve-implicit-MoE-cache-provi.patch` | *(no Measured-effect trailer)* |
| `0252-layer7-0024-server-harden-shared-draft-device-placem.patch` | *(no Measured-effect trailer)* |
| `0253-layer7-0025-cuda-tune-MoE-cache-CPU-overlap-automati.patch` | *(no Measured-effect trailer)* |
| `0254-layer7-0026-cuda-adapt-MoE-cache-admission-to-device.patch` | *(no Measured-effect trailer)* |
| `0255-layer7-0027-cuda-align-MoE-cache-pool-allocation-wit.patch` | *(no Measured-effect trailer)* |
| `0256-layer7-0028-docs-update-MoE-cache-validation.patch.patch` | *(no Measured-effect trailer)* |
| `0257-layer7-0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch.patch` | *(no Measured-effect trailer)* |
| `0258-layer7-0030-cuda-aggregate-small-MoE-tensors-into-ca.patch` | *(no Measured-effect trailer)* |
| `0259-layer7-0031-cuda-bound-automatic-MoE-cache-admission.patch` | *(no Measured-effect trailer)* |
| `0260-layer7-0032-cuda-reject-undersized-automatic-MoE-cac.patch` | *(no Measured-effect trailer)* |
| `0261-layer7-0033-cuda-enable-multi-token-MoE-cache-in-for.patch` | *(no Measured-effect trailer)* |
| `0262-layer7-0034-common-surface-MoE-cache-activation.patc.patch` | *(no Measured-effect trailer)* |
| `0263-layer7-0035-docs-fix-MoE-cache-benchmark-verbosity.p.patch` | *(no Measured-effect trailer)* |
| `0264-layer7-0036-cuda-accelerate-complete-MoE-cache-pools.patch` | *(no Measured-effect trailer)* |
| `0265-layer7-0037-cuda-report-oversized-MoE-cache-nodes.pa.patch` | *(no Measured-effect trailer)* |
| `0266-layer7-0038-cuda-clarify-MoE-cache-session-diagnosti.patch` | *(no Measured-effect trailer)* |
| `0267-layer7-0039-docs-record-MoE-cache-workload-convergen.patch` | *(no Measured-effect trailer)* |
| `0268-layer7-0040-cuda-report-MoE-cache-pair-residency.pat.patch` | *(no Measured-effect trailer)* |
| `0269-layer7-0041-docs-document-what-the-test-suites-do-an.patch` | *(no Measured-effect trailer)* |
| `0270-layer7-0044-moe-cache-provider-registry-with-per-sch.patch` | *(no Measured-effect trailer)* |
| `0271-layer7-0046-moe-cache-allow-automatic-mode-on-a-sing.patch` | *(no Measured-effect trailer)* |
| `0272-layer7-0048-docs-document-INFO-level-MoE-cache-disab.patch` | *(no Measured-effect trailer)* |
| `0273-layer7-0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CAC.patch` | *(no Measured-effect trailer)* |
| `0274-layer7-0050-docs-correct-MOE-CACHE.md-against-the-im.patch` | *(no Measured-effect trailer)* |
| `0275-layer7-0051-ggml-split-the-turbo3-group-size-declara.patch` | *(no Measured-effect trailer)* |
| `0276-resolve-8-committed-conflict-markers-on-merit.patch` | *(no Measured-effect trailer)* |
| `0277-fix-the-merge-so-it-builds-5-files-6-defects.patch` | *(no Measured-effect trailer)* |
| `0278-merge-repairs-seven-defect-classes-and-one-the-compi.patch` | *(no Measured-effect trailer)* |
| `0279-ROCmFPX-on-Vulkan-the-shader-half-ported.-NOT-YET-WO.patch` | *(no Measured-effect trailer)* |
| `0280-currency-register-ALL-SEVEN-forks-so-nothing-can-dri.patch` | *(no Measured-effect trailer)* |
| `0281-series-base-and-the-branch-it-describes-are-ONE-pair.patch` | *(no Measured-effect trailer)* |
| `0282-hygiene-the-code-review-skill-taught-a-type-ID-map-t.patch` | *(no Measured-effect trailer)* |
| `0283-contract-tq3-is-the-first-family-we-must-renumber-so.patch` | *(no Measured-effect trailer)* |
| `0284-sources-honest-pins-turboquant-re-pinned-off-a-dead-.patch` | *(no Measured-effect trailer)* |
| `0285-sources-lane-143-self-refutation-the-turbo3-ODR-defe.patch` | *(no Measured-effect trailer)* |
| `0286-sources-r2-checker-BLOCK-F1-F6-ciru-160-was-145-comm.patch` | *(no Measured-effect trailer)* |
| `0287-series-point-base.ref-at-the-LINEAR-branch-recut-lin.patch` | *(no Measured-effect trailer)* |
| `0288-series-disclose-in-the-record-itself-that-the-flatte.patch` | *(no Measured-effect trailer)* |
| `0289-ggml-port-the-tq3-TQ3_4S-family-codec-at-ids-48-51-T.patch` | *(no Measured-effect trailer)* |
| `0290-tests-cover-the-tq3-family-in-test-quantize-fns-and-.patch` | *(no Measured-effect trailer)* |
| `0291-contract-tq3-rows-48-51-implemented-behind-GGML_ARIF.patch` | *(no Measured-effect trailer)* |
| `0292-tests-tq3_4s-tq3_4se-skipped-in-test-quantize-fns-wi.patch` | *(no Measured-effect trailer)* |
| `0293-tools-gguf-retag-tq3-lands-LAST-after-the-coherence-.patch` | *(no Measured-effect trailer)* |
| `0294-checker-fixes-choose_index-restored-verbatim-inverte.patch` | *(no Measured-effect trailer)* |
| `0295-retag-verify-geometry-per-TENSOR-for-every-mapped-id.patch` | *(no Measured-effect trailer)* |
| `0296-retag-README-per-tensor-geometry-check-for-every-map.patch` | *(no Measured-effect trailer)* |
| `0297-lane-144-GOAL-2-step-1-TQ3_4S-id-48-Vulkan-port-dequ.patch` | *(no Measured-effect trailer)* |
| `0298-lane-144-sweep-ROCmFP4-ROCmFPX-in-test-backend-ops-a.patch` | *(no Measured-effect trailer)* |
| `0299-lane-144-CHECKER-FIX-TQ3_4S-was-missing-from-the-MUL.patch` | *(no Measured-effect trailer)* |
| `0300-lane-145-Q6_0_ROCMFPX-Vulkan-fix-implement-the-6-bit.patch` | *(no Measured-effect trailer)* |
| `0301-lane-145-checker-finding-7-delete-block_rocmfpx_fp6_.patch` | *(no Measured-effect trailer)* |
| `0302-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | *(no Measured-effect trailer)* |
| `0303-test-backend-ops-cover-the-CPY-types-supports_op-nam.patch` | *(no Measured-effect trailer)* |
| `0304-lane-148-defect-A-ROOT-FIX-Vulkan-turbo3-centroid-LU.patch` | *(no Measured-effect trailer)* |
| `0305-lane-148-defect-B-ROOT-FIX-FA_TYPE_TURBO2-3-4_0-GLSL.patch` | *(no Measured-effect trailer)* |
| `0306-lane-148-F-69-sibling-coverage-test-backend-ops-FLAS.patch` | *(no Measured-effect trailer)* |
| `0307-lane-148-checker-findings-4-5-FIXED-turbo4-SET_ROWS-.patch` | *(no Measured-effect trailer)* |
| `0308-lane-149-F-110-guard-rung-b-arifi_sync_check.py-CMak.patch` | *(no Measured-effect trailer)* |
| `0309-lane-149-F-112-F-109-harness-guards-NOT_SUPPORTED-ca.patch` | *(no Measured-effect trailer)* |
| `0310-lane-149-F-109-payoff-q2_0_g128-SET_ROWS-tie-roundin.patch` | *(no Measured-effect trailer)* |
| `0311-lane-149-checker-driven-hardening-of-arifi-sync-chec.patch` | *(no Measured-effect trailer)* |
| `0312-lane-149-F-112-second-half-checker-finding-11-per-op.patch` | *(no Measured-effect trailer)* |
| `0313-lane-150-step-1-TQ4_1S-read-back-leg-wired-CPY-tq4_1.patch` | *(no Measured-effect trailer)* |
| `0314-lane-150-step-2-TQ4_1S-write-leg-EXECUTES-SET_ROWS_T.patch` | *(no Measured-effect trailer)* |
| `0315-lane-150-step-3-root-fix-for-the-hole-that-hid-the-w.patch` | *(no Measured-effect trailer)* |
| `0316-lane-150-checker-driven-fixes-type_bucket-prefix-bou.patch` | *(no Measured-effect trailer)* |
| `0317-lane-151-GOAL-6-hygiene-drop-648-tracked-_lane-out-d.patch` | *(no Measured-effect trailer)* |
| `0318-lane-151-GOAL-9-reserve-tq3-s-three-destination-less.patch` | *(no Measured-effect trailer)* |
| `0319-lane-151-GOALs-6-7-README-refreshed-to-b10453-canoni.patch` | *(no Measured-effect trailer)* |
| `0320-lane-151-GOAL-2-sources.json-base.ref-arifi-main-the.patch` | *(no Measured-effect trailer)* |
| `0321-lane-151-GOAL-5-gitignore-.tokensave-the-fork-repo-n.patch` | *(no Measured-effect trailer)* |
| `0322-lane-151-CHECKER-ROOT-FIX-series-check-now-asserts-e.patch` | *(no Measured-effect trailer)* |
| `0323-lane-151-CHECKER-FIXES-docs-UPDATE-RUNBOOK-4.5-no-lo.patch` | *(no Measured-effect trailer)* |
| `0324-lane-151-HQ-DIRECTIVE-RETAIN-the-licenses-instead-of.patch` | *(no Measured-effect trailer)* |
| `0325-licenses-thecodacus-MIT-retained-from-the-public-Git.patch` | *(no Measured-effect trailer)* |
| `0326-lane-152-GOAL-1-upstream-bump-b10453-b10481-the-FIRS.patch` | *(no Measured-effect trailer)* |
| `0327-lane-152-what-the-FIRST-live-run-of-UPDATE-RUNBOOK-s.patch` | *(no Measured-effect trailer)* |
| `0328-lane-152-GOAL-2-batch-1-ciru-s-Vulkan-SPIRV-Headers-.patch` | UNMEASURED,UNMEASURED - the source reports reasoning depth 17k-64k to 33k-120k chars and
  structural codegen bugs eliminated on Qwen3.8-27B at tq3_4s, plus about 12 MiB per family of
  size cost. Theirs, not ours.,UNMEASURED - the source measured 60 to 14 test-backend-ops failures on
  gfx1151 / ROCm 7.14 at about 2 percent decode cost. We do not build HIP in the configuration
  any number in this repository was measured on. |
| `0329-lane-152-GOAL-2-batch-2-three-lfm2-pre-tokenizer-has.patch` | UNMEASURED - no conversion of an LFM2.5 model was run, and the pre-tokenizer
  workflow was not executed. Syntax-checked with ast.parse, nothing more. |
| `0330-lane-152-pins-THREE-advance-with-reviews-upstream-ro.patch` | *(no Measured-effect trailer)* |
| `0331-lane-152-checker-F7-the-old-base-is-tag-b10454-not-b.patch` | *(no Measured-effect trailer)* |
| `0332-repo-env.json-WI-1540-tail-the-engine-repo-s-env-man.patch` | *(no Measured-effect trailer)* |
| `0333-WI-1580-sub-2-UPDATE-RUNBOOK-section-7-the-4.5-line-.patch` | *(no Measured-effect trailer)* |
| `0334-vulkan-land-the-MoE-expert-cache-provider-files-VK-M.patch` | UNMEASURED at this step - the files are not wired into the build yet; step 2 carries the build and guard proofs. |
| `0335-graph-derive-the-V-unpad-head-count-from-the-attenti.patch` | UNMEASURED on this rig. Their report is a live abort in ggml_reshape_3d during graph reserve for gpt-oss-20b (V head_dim 64 padded to 128, 64 q heads over 8 kv heads) with -ctk turbo3 -ctv turbo3; we do not run that model, so the fix is taken as a correctness guard and the claim is NOT restated as ours. |
| `0336-vulkan-wire-the-MoE-expert-cache-provider-into-the-b.patch` | UNMEASURED for throughput - this lane makes NO timed claim about the cache. The proofs carried are correctness proofs only: baseline exe mtimes banked before the ingest, then a rebuild with fresh mtimes, test-backend-ops and test-quantize-fns compared against the banked baseline, and arifi_sync_check.py green. |
| `0337-vulkan-complete-the-MoE-cache-port-the-56-provider-h.patch` | UNMEASURED for throughput; this lane makes NO timed claim about the cache. Correctness only: F-107 exe mtimes moved for all five targets (10:52 to 11:30), llama-cli grew 97090161 to 97433306 bytes, test-backend-ops 26213/26260 identical to the banked baseline, test-quantize-fns 6 failures identical, arifi_sync_check.py OK on the same 49/7/16/35/16/12 tallies. |
| `0338-sources-advance-the-turboquant-pin-f6124e914-7ebcbb0.patch` | UNMEASURED - a pin and its review change no runtime behaviour. |
| `0339-off-rig-backends-land-the-12-CUDA-Metal-SYCL-HIP-row.patch` | UNMEASURED AND UNPROVABLE ON THIS RIG - that is the point of the ruling. This code never compiles here (no CUDA/Metal/SYCL/HIP device or toolchain), so no F-107 exe proof and no backend-ops execution proof exists for it. A future lane with the hardware owes roughly 30 min to verify it; the debt is a named OWED row in the REPORT. |
| `0340-sources-pin_review-names-the-12-off-rig-shas-and-the.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0341-sources-correct-the-pin_review-the-contested-12-12-a.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0342-sources-pin_review-records-the-apply-count-claim-as-.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0343-base-b10481-25ae3a9b3-b10488-9d77fa172-series-regen-.patch` | *(no Measured-effect trailer)* |
| `0344-moe-cache-heat-protected-eviction-keeps-hot-experts-.patch` | UNMEASURED here. The source reports +12% TG (soft) / +7.5%
  (auto) on an RTX 5090 with Laguna S-2.1 - a CUDA measurement on hardware this
  estate does not have, quoted, NOT adopted as our number. |
| `0345-tests-initialise-non-contiguous-tensors-row-by-row-t.patch` | UNMEASURED as a delta - the sweep baseline 26213/26260 is
  re-run after this lane's build and compared case-for-case. The source notes
  the fix does NOT make their TQ4_1S k_v=1600 case pass; it unmasks a separate
  CUDA defect, which is off-rig for us. |
| `0346-sources-advance-the-turboquant-pin-7ebcbb0b6-d14e368.patch` | *(no Measured-effect trailer)* |
| `0347-sources-prisml-pin-HELD-deliberately-with-a-sharper-.patch` | *(no Measured-effect trailer)* |
| `0348-sources-ciru-re-derived-315-15-and-HELD-with-all-15-.patch` | *(no Measured-effect trailer)* |
| `0349-sources-advance-remotes.upstream.pin-25ae3a9b3-9d77f.patch` | *(no Measured-effect trailer)* |
| `0350-sources-correct-the-turboquant-pin_review-s-FALSE-bl.patch` | *(no Measured-effect trailer)* |
| `0351-sources-trim-the-e130aef60-blocker-to-the-ONE-item-t.patch` | *(no Measured-effect trailer)* |
| `0352-feat-kv-cache-port-flag-gated-mean-centering.patch` | UNMEASURED |
| `0353-feat-kv-cache-bind-calibration-to-exact-model.patch` | UNMEASURED (native toolchain execution denied in managed seat) |
| `0354-fix-server-keep-target-calibration-out-of-draft-cont.patch` | UNMEASURED (native toolchain execution denied in managed seat) |
| `0355-test-kv-cache-add-Vulkan-tensor-probe-and-bench-pure.patch` | harness selftest PASS; native build/model effect UNMEASURED (HQ-OWED) |
| `0356-sources-record-lane-158-PrismML-K-cache-disposition.patch` | source census corrected; native model effect UNMEASURED (HQ-OWED) |
| `0357-test-kv-cache-fail-close-default-OFF-binary-identity.patch` | clean fixture PASS; planted byte mismatch RED; native identity UNMEASURED (HQ-OWED) |
| `0358-test-kv-cache-assert-both-rotation-basis-polarities-.patch` | *(no Measured-effect trailer)* |
| `0359-fix-kv-cache-make-the-A-B-harness-able-to-launch-and.patch` | *(no Measured-effect trailer)* |
| `0360-fix-kv-cache-floor-check-the-model-named-by-m-not-th.patch` | *(no Measured-effect trailer)* |
| `0361-fix-kv-cache-wait-for-the-RAM-floor-between-arms-and.patch` | *(no Measured-effect trailer)* |
| `0362-fix-kv-cache-probe-the-layer-ids-the-calibration-act.patch` | *(no Measured-effect trailer)* |
| `0363-support-DFlash2.patch` | *(no Measured-effect trailer)* |
| `0364-dflash2-build-fixes-on-the-PR-27342-pick-dedupe-DFLA.patch` | *(no Measured-effect trailer)* |
| `0365-sources-register-buun-spiritbuun-buun-llama-cpp-as-a.patch` | *(no Measured-effect trailer)* |
| `0366-cuda-lift-TQ3_4S-kernel-set-from-tq3-master-14-files.patch` | UNMEASURED ON THIS RIG (no CUDA silicon); tq3-side references: RTX 3090 tg128 43.1 -> 47.4 t/s (PRMT vec_dot), Ampere decode +35% (3d3888c9f) |
| `0367-metal-lift-TQ3_4S-kernel-set-from-tq3-master-5-files.patch` | UNMEASURED ON THIS RIG (no Metal silicon); tq3-side reference: M3 Pro 27B decode ~4 t/s, ~27x over CPU (02d51f048) |
| `0368-metal-declare-the-tq3_rht-pipeline-getter-in-ggml-me.patch` | UNMEASURED ON THIS RIG (no Metal toolchain) |
| `0369-sync-move-base-b10488-b10524-regenerate-series-372-p.patch` | UNMEASURED (build+sweep follow in this lane) |
| `0370-spec-ingest-turboquant-MTP-boost-wave-effective-KV-b.patch` | UNMEASURED (their mtp-boost numbers are theirs; no arm run on this rig yet) |
| `0371-sync-re-pin-wave-3-sources-regenerate-series-374-pat.patch` | UNMEASURED (build+sweep follow) |
| `0372-arifi-sync-provenance-native-class-root-fix-sha-pinn.patch` | provenance audit flips ambient-red to enforcing; 289 grandfathered, 0 missing, new untrailered commits fail again |
| `0373-vulkan-subgroup-cooperative-TQ-mat-vec-tq3_1s-tq4_1s.patch` | 27B tq3_4s tg32 0.67->1.80 t/s; 0.5b tq4_1s tg64 44.7->126.7, tq3_1s 45.5->107.1; mat-vec kernel n=1 3.1-3.4x (780M, evidence lane-163) |
| `0374-vulkan-register-blocked-4-elem-lane-TQ-subgroup-mat-.patch` | *(no Measured-effect trailer)* |
| `0375-escha-native-Escha-W2-types-ESCHA2-55-ESCHA3-56-fuse.patch` | *(no Measured-effect trailer)* |
| `0376-escha-F-125-fix-full-64-bit-pair-sourcing-row-parity.patch` | Escha-W2 native decode goes from token salad to coherent; worst-projection weight correlation 0.9358, K=2 and K=3 both correct |
| `0377-escha-test-backend-ops-un-nest-ESCHA_MM-eval-cases-m.patch` | none yet (test registration only); unlocks the ncols duration curve |
| `0378-escha-vulkan-bound-every-escha_mm-dispatch-to-32-col.patch` | per-dispatch column count capped at 32, was ncols up to 2048 |
| `0379-escha-vulkan-column-blocked-escha_mm-one-weight-deco.patch` | pending on this commit, before-numbers are 1.15 and 2.12 ms per column |
| `0380-escha-vulkan-column-block-C-8-fall-back-to-the-one-c.patch` | 3.51x on escha3 17408x5120 at ncols=64, decode unchanged |
| `0381-escha-tests-cover-the-column-blocked-kernel-AT-MODEL.patch` | shipped C=8 prefill path now covered at real 27B shapes, 10/10 GREEN |
| `0382-escha-vulkan-one-hardware-f32-f16-convert-instead-of.patch` | UNMEASURED at commit time - per-weight instruction count drops ~21 to ~7 by inspection; decode/prefill deltas and the exhaustive rounding sweep are owed |
| `0383-escha-vulkan-subgroup-shuffle-Hadamard-14-barriers-p.patch` | UNMEASURED at commit time - barrier count per 128-input block drops 14 to 3 at subgroup 64 by inspection; correctness gate and speed delta owed |
| `0384-escha-vulkan-the-hardware-f32-f16-convert-must-be-fl.patch` | CORRECTNESS ONLY at this commit - removes a one-ulp truncation error affecting 25.6% of codewords that 1ee8e9ad7 would have shipped; speed deltas for this lever alone are measured next and reported separately. |
| `0385-escha-vulkan-ESCHA_SG_HADAMARD-defaults-OFF-the-leve.patch` | SG lever OFF by default; no speed change vs the shipped RTE-only configuration, and ~3% of prefill recovered versus having it on |
| `0386-escha-vulkan-f16-native-generator-decode-18-bit-exac.patch` | *(no Measured-effect trailer)* |
| `0387-escha-vulkan-two-more-levers-tried-and-REFUTED-by-me.patch` | *(no Measured-effect trailer)* |
| `0388-escha-vulkan-the-multi-column-threshold-was-disablin.patch` | *(no Measured-effect trailer)* |
| `0389-escha-vulkan-column-ladder-C2-C4-C8-C16-and-the-gene.patch` | *(no Measured-effect trailer)* |
| `0390-dflash-ingest-buun-s-DFlash2-adaptive-controller-bui.patch` | *(no Measured-effect trailer)* |
| `0391-escha-vulkan-prefill-is-OCCUPANCY-bound-coopmat-and-.patch` | *(no Measured-effect trailer)* |
| `0392-escha-vulkan-f16-activation-staging-built-correct-an.patch` | *(no Measured-effect trailer)* |
| `0393-dflash-reject-an-out-of-extent-draft-depth-instead-o.patch` | *(no Measured-effect trailer)* |
| `0394-spec-load-the-draft-model-from-the-same-variable-the.patch` | *(no Measured-effect trailer)* |
| `0395-F-136-ROOT-CAUSE-FIX-the-recurrent-snapshot-bank-n_r.patch` | *(no Measured-effect trailer)* |
| `0396-spec-ON-DEVICE-speculative-checkpoints-drafter-now-B.patch` | *(no Measured-effect trailer)* |
| `0397-spec-ring-rollback-re-enabled-DFlash2-depth-3-8.80-t.patch` | *(no Measured-effect trailer)* |
| `0398-rs-zero-on-clear-ROOT-FIX-cross-request-recurrent-st.patch` | *(no Measured-effect trailer)* |
| `0399-tests-zero_gather-CPY-GET_ROWS-Vulkan-probe-cases-se.patch` | *(no Measured-effect trailer)* |
| `0400-rs-zeroing-hunt-CLOSED-leak-is-backend-independent-C.patch` | *(no Measured-effect trailer)* |
| `0401-vulkan-F-136-residual-ROOT-FIX-in-graph_optimize-is_.patch` | *(no Measured-effect trailer)* |
| `0402-arifi-sync-base-move-b10524-9ee9fc04c-b10636-4d19b28.patch` | *(no Measured-effect trailer)* |
| `0403-b10636-bump-repair-restore-upstream-s-DOTS3NOTE-inde.patch` | *(no Measured-effect trailer)* |
| `0404-b10636-bump-repair-2-close-llm_graph_input_attn_k_ds.patch` | *(no Measured-effect trailer)* |
| `0405-dflash-a-rejected-draft-checkpoint-image-must-not-ki.patch` | UNMEASURED (no benchmark arm; the guard replaces a process abort on a path that needs a mismatched draft image to reach) |
| `0406-seat-46-MSVC-portability-root-fixes-for-the-ROCm-HIP.patch` | *(no Measured-effect trailer)* |
| `0407-server-re-add-the-shared_draft_devices-VRAM-accounti.patch` | *(no Measured-effect trailer)* |
| `0408-arifi-sync-base-move-b10636-4d19b2876-b10680-d7bd3bf.patch` | *(no Measured-effect trailer)* |
| `0409-vulkan-sync-check-follow-the-FA_TYPE-defines-into-b1.patch` | *(no Measured-effect trailer)* |
| `0410-b10680-bump-repair-common-fit.cpp-still-called-llm_f.patch` | *(no Measured-effect trailer)* |
| `0411-b10680-bump-repair-2-my-hunk-1-union-in-the-3aa479a0.patch` | *(no Measured-effect trailer)* |
| `0412-runbook-4.1b-COLLISION-MAP-law-President-2026-08-29-.patch` | *(no Measured-effect trailer)* |
| `0413-attribution-gap-fix-seat-48-re-key-the-sha-dead-prov.patch` | *(no Measured-effect trailer)* |
| `0414-runbook-2.2.1-RE-KEY-the-governed-sha-pointers-after.patch` | *(no Measured-effect trailer)* |
| `0415-attribution-wave-2-seat-48-zuijdwijk-is-a-LIVE-take-.patch` | *(no Measured-effect trailer)* |
| `0416-G12-re-audit-seat-48-52-of-the-180-are-PROVEN-not-fo.patch` | *(no Measured-effect trailer)* |
| `0417-R1-the-bank-plane-is-a-RUNTIME-INDEX-TENSOR-so-the-c.patch` | *(no Measured-effect trailer)* |
| `0418-R1-fix-guard-the-bank-index-tensors-on-ALLOCATION-no.patch` | *(no Measured-effect trailer)* |
| `0419-lane-178-the-ring-s-extra-graph-split-is-the-SPLIT-I.patch` | *(no Measured-effect trailer)* |
| `0420-ggml-backend-adopt-split-inputs-cap-64-as-the-defaul.patch` | *(no Measured-effect trailer)* |
| `0421-q2_0_g128-the-tie-predicate-is-exact-on-both-sides-n.patch` | *(no Measured-effect trailer)* |
| `0422-lane-180-the-ring-s-26-extra-graph-inputs-are-23-IDE.patch` | *(no Measured-effect trailer)* |
| `0423-lane-180-GATE-0-receipt-report-eHostVisible-in-the-v.patch` | *(no Measured-effect trailer)* |
| `0424-lane-180-ADOPT-share-ONE-s_wrow-view-by-default-29-i.patch` | *(no Measured-effect trailer)* |
| `0425-vulkan-perf-logger-must-not-abort-when-an-async-copy.patch` | *(no Measured-effect trailer)* |
| `0426-lane-188-SHIPPING-KV-precision-tail-store-side-quant.patch` | *(no Measured-effect trailer)* |
| `0427-lane-188-refuse-the-tail-on-MLA-caches-the-write-ord.patch` | *(no Measured-effect trailer)* |
| `0428-lane-192-FIX-a-the-precision-tail-survives-a-prefix-.patch` | *(no Measured-effect trailer)* |
| `0429-lane-194-q4_0-F16-Vulkan-cpy-shader-F16-KV-tail-comp.patch` | *(no Measured-effect trailer)* |
| `0430-lane-194-the-CPU-reference-for-quant-F16-dup-which-d.patch` | *(no Measured-effect trailer)* |
| `0431-lane-194-name-the-compose-type-in-the-tail-banner-th.patch` | *(no Measured-effect trailer)* |
| `0432-lane-195-multi-segment-flash-attention-the-ggml-API-.patch` | *(no Measured-effect trailer)* |
| `0433-lane-195-LEG-0-the-regression-leg-the-branch-was-mis.patch` | *(no Measured-effect trailer)* |
| `0434-lane-196-Design-B-BUILT-heterogeneous-split-k-FA-per.patch` | *(no Measured-effect trailer)* |
| `0435-lane-196-promote-the-tail-ON-banner-to-WARN-llama-se.patch` | *(no Measured-effect trailer)* |
| `0436-lane-197-honor-spec-draft-p-min-in-the-DFlash2-sampl.patch` | *(no Measured-effect trailer)* |
| `0437-lane-198-gate-Q5_K-n-1-and-Q4_K-n-5-off-the-q8_1-MMV.patch` | *(no Measured-effect trailer)* |
| `0438-runbook-4.1b-duty-3-a-named-collision-is-decided-by-.patch` | *(no Measured-effect trailer)* |
| `0439-vulkan-tiled-concat-transpose-for-the-delta-net-conv.patch` | CONCAT dispatch -12.79% (-15.66 ms per ub512 prefill graph);
  end-to-end ceiling +0.18%, below serve resolution on this box |
| `0440-runbook-4.1-warn-that-series-regen-is-destructive-th.patch` | none (documentation) |
| `0441-arifi-sync-root-fix-the-destructive-series-regen-and.patch` | no engine behaviour changed and no shipped default moved; tooling and
  documentation only, verified by the 14-case suite including four fail-closed negative cases. |
| `0442-vulkan-label-the-concat-transpose-prior-art-numbers-.patch` | none - comment text only, no generated code differs. |
| `0443-arifi-sync-name-the-incoming-ref-in-full-when-the-pr.patch` | none - one diagnostic message string; 14/14 tests still green. |
| `0444-arifi-sync-make-an-UNREGISTERED-measured-win-refuse-.patch` | tooling and documentation only; no engine behaviour changed and no shipped default moved |
| `0445-arifi-sync-close-the-two-live-false-negatives-in-the.patch` | tooling and documentation only; no engine behaviour changed and no shipped default moved |
| `0446-arifi-sync-discharge-eleven-REGISTRATION-OWED-rows-a.patch` | tooling and documentation only; no engine behaviour changed and no shipped default moved |
| `0447-arifi-sync-search-anchors-in-SOURCE-only-so-the-mani.patch` | correctness only, no throughput claim |
| `0448-arifi-sync-bring-the-manifest-s-own-_schema.anchors-.patch` | tooling and documentation only; a stale schema fact corrected, no code path changed |
| `0449-arifi-sync-recognize-correctness-only-as-no-effect-v.patch` | tooling and documentation only; no engine behaviour changed and no shipped
  default moved. |
| `0450-vulkan-create-the-ROCmFP4-FAST-mat-mat-pipelines-and.patch` | *(no Measured-effect trailer)* |
| `0451-vulkan-dedicated-IQ4_XS-mat-vec-shader-K-quant-style.patch` | *(no Measured-effect trailer)* |
| `0452-vulkan-IQ3_S-mat-vec-16-invocations-per-superblock-a.patch` | *(no Measured-effect trailer)* |
| `0453-vulkan-IQ3_S-mat-vec-split-gate-becomes-NUM_COLS-3-N.patch` | *(no Measured-effect trailer)* |
| `0454-vulkan-create-the-ROCmFP4-FAST-q8_1-MMVQ-mat-vec-pip.patch` | *(no Measured-effect trailer)* |
| `0455-vulkan-keep-the-ROCmFP4-FAST-q8_1-MMVQ-path-to-n-2.5.patch` | *(no Measured-effect trailer)* |
| `0456-vulkan-enforce-ROCmFP4-MMVQ-width-gate-before-batch-.patch` | *(no Measured-effect trailer)* |
| `0457-vulkan-exclude-ROCmFP4-MMVQ-n-4-tie-width.patch` | *(no Measured-effect trailer)* |
| `0458-vulkan-isolate-ROCmFP4-MMVQ-to-n-3-and-n-5.patch` | *(no Measured-effect trailer)* |
| `0459-vulkan-correct-lane-209-backend-coverage-comment.patch` | *(no Measured-effect trailer)* |
| `0460-speculative-size-drafts-from-measured-acceptance-spe.patch` | *(no Measured-effect trailer)* |
| `0461-server-do-not-re-verify-replayed-draft-tokens-after-.patch` | *(no Measured-effect trailer)* |
| `0462-vulkan-q6_k-mat-vec-reads-its-scales-from-the-block-.patch` | NEUTRAL on gfx1103 - op-level q6_k median ON/OFF 0.9796 inside a
  78-row untouched control at 0.9990, p10-p90 [0.952, 1.053]; serve TIED on all
  six prefill/decode cells; output byte-identical, 1 sha per prompt over 12
  launches. No speed win is claimed. |
| `0463-arifi-sync-register-the-three-lane-209-mechanisms-th.patch` | *(no Measured-effect trailer)* |
| `0464-test-backend-ops-adversarial-MUL_MAT-data-patterns-f.patch` | none - test coverage only |
| `0465-vulkan-ROCmFP4-MMVQ-becomes-opt-in-GGML_ARIFI_ROCMFP.patch` | none on the seated line (no ROCmFP4 tensors); on ROCmFP4 files
  this defers a measured +26.9% drafted decode behind an opt-in env var |
| `0466-arifi-sync-register-the-rank-7-ROCmFP4-MMVQ-mechanis.patch` | none - manifest registration only |
| `0467-docs-the-public-trail-for-the-b10680-kernel-wave-lan.patch` | UNMEASURED - documentation only; no source, shader, default or
  build input is modified, and every performance figure is transcribed from a
  cited prior measurement rather than taken here |
| `0468-docs-scope-the-power-plan-claim-to-the-runs-that-ban.patch` | UNMEASURED |
| `0469-docs-apply-the-six-independent-check-fixes-to-the-b1.patch` | UNMEASURED |
| `0470-arifi-sync-base-move-b10680-d7bd3bfca-b10819-6a1a922.patch` | UNMEASURED - tooling and governed records only; no engine behaviour changed by this commit. |
| `0471-b10819-sync-repair-six-merge-defects-that-stopped-th.patch` | build FAIL at 6/305 -> RC=0 with all five targets linked (llama-cli, llama-server, llama-quantize, test-backend-ops, test-quantize-fns); runtime effect of the speculative.cpp union is measured by the F-141 content gate and the served DFlash2 arm, not by this build. |
| `0472-vulkan-tq_rotate-is-a-src1-reformat-restore-the-TQ-M.patch` | test-backend-ops goes from ABORT at case 12343 (GGML_ASSERT y_needs_reformat, rc 0xC0000409, no result at all) to a completed sweep; build stays RC=0 on all five targets. |
| `0473-metal-union-the-dropped-upstream-FA-machinery-back-i.patch` | *(no Measured-effect trailer)* |
| `0474-cuda-union-upstream-s-mul_mat_id_needs_sync-predicat.patch` | *(no Measured-effect trailer)* |
| `0475-ci-a-backend-matrix-that-compiles-what-this-rig-cann.patch` | *(no Measured-effect trailer)* |
| `0476-arifi-sync-restore-the-2-space-JSON-indent-the-R17-r.patch` | *(no Measured-effect trailer)* |
| `0477-ci-the-test-quantize-fns-gate-was-decorative-and-Met.patch` | *(no Measured-effect trailer)* |
| `0478-cuda-restore-the-fused-MoE-cache-matvec-on-upstream-.patch` | *(no Measured-effect trailer)* |
| `0479-ci-make-the-link-contract-real-and-run-test-backend-.patch` | *(no Measured-effect trailer)* |
| `0480-arifi-sync-R13B-s-two-b10819-repair-commits-become-a.patch` | *(no Measured-effect trailer)* |
| `0481-sycl-the-MMVQ-launchers-declared-one-pair-of-ranges-.patch` | *(no Measured-effect trailer)* |
| `0482-ci-each-job-now-proves-the-unioned-file-itself-produ.patch` | *(no Measured-effect trailer)* |
| `0483-metal-replace-the-garbled-library-loader-with-upstre.patch` | UNMEASURED - off-rig backend, union-by-mechanism-read; compile verification PENDING the first metal-macos run |
| `0484-ci-the-cpu-ubuntu-job-runs-protected-win-validate-so.patch` | UNMEASURED - CI workflow only; never run, the branch is not pushed |
| `0485-arifi-sync-protected-win-learns-a-symbols-class-a-re.patch` | UNMEASURED - tooling and governed records only; no engine behaviour changed by this commit. |
| `0486-arifi-sync-the-symbols-guard-ignores-commented-out-b.patch` | UNMEASURED - tooling and governed records only; no engine behaviour changed by this commit. |
| `0487-docs-put-the-integration-A-B-numbers-into-the-public.patch` | UNMEASURED - documentation of measurements already banked; no engine behaviour changed by this commit. |
| `0488-docs-state-the-power-plan-and-RAM-as-pre-registered-.patch` | UNMEASURED - documentation wording; no engine behaviour changed by this commit. |
| `0489-arifi-sync-sources.json-post_move_tip-re-keyed-to-th.patch` | UNMEASURED - register only |
| `0490-ggml-flash-attention-segment-count-moves-to-op_param.patch` | *(no Measured-effect trailer)* |
| `0491-vulkan-FA-dequant-KV-f16-scratch-is-a-runtime-gate-d.patch` | R19D-ENGAGEMENT-LEGS.md D1 (ON vs OFF, one binary): prefill clean +8.08% [+1.63%, +14.52%]; this commit = the ON default, expected 0 vs the engine of record (same path) |
| `0492-arifi-sync-base-move-b10819-6a1a922d2-b10825-9e0e220.patch` | *(no Measured-effect trailer)* |
| `0493-arifi-sync-rekey-3-protected-win-manifests-to-the-b1.patch` | *(no Measured-effect trailer)* |
| `0494-arifi-sync-scope-subcommand-F-161-the-ingest-A-B-sco.patch` | *(no Measured-effect trailer)* |
| `0495-provenance-route-B-G7-append-only-R24-s-58-classific.patch` | *(no Measured-effect trailer)* |
| `0496-spec-runtime-switch-for-the-DFlash-encoder-fusion-LL.patch` | DOCUMENTED-TAKE. Seat DFlash2 decode -0.98% CI95 [-1.16%, -0.80%] fused vs legacy; U-IQ4XS +0.05% [-0.56%, +0.67%]; U-Q3KXL -0.87% [-2.50%, +0.77%]. The wave's INERT control on the same pair moved +3.71% [+0.80%, +6.61%] on decode, so none of these clears the build floor. |
| `0497-vulkan-RDNA3-mul_mat_vec_id-rows-runtime-selectable-.patch` | DOCUMENTED-TAKE. 20 op rows, 3 rounds each: x0.979 to x1.031 against the upstream default of 4 - no setting separates from the ladder's own spread. Correctness 80/80 per setting, TBOID GREEN. No serve cell ran (no MoE file that fits this box was staged). |
| `0498-kv-cache-the-PLE-n-gram-history-lookup-becomes-a-run.patch` | DOCUMENTED-TAKE, mechanism UNMEASURED. Receipt grep: 0 hits in 10 server logs - the mechanism never ran. The same engine pair still moved DFlash2 prefill +8.03% [+5.56%, +10.49%] and plain decode -2.20% [-2.79%, -1.60%], which is therefore pure build floor, not this switch. Witness owed: test-llama-archs -a qwen4exp under both envs (PREREG-r15c4-kv-ngram-index.md). |
| `0499-vulkan-MUL_MAT_ID-B-staging-receipt-and-a-loud-asser.patch` | DOCUMENTED-TAKE. Receipt proved the path cannot engage on this GPU: 0 K-padded rows in 7 logs (no coopmat2 on gfx1103). Op ladder 48 MUL_MAT_ID ops, 3 rounds per arm, x0.976 to x1.032, no CI claimed at n=3. No default changed, so there is nothing to lose here. |
| `0500-vulkan-UMA-read-back-takes-the-direct-memcpy-only-wh.patch` | DOCUMENTED-TAKE. Mechanism cell (ONE binary, direct vs auto) read decode +0.21% CI95 [-0.26%, +0.69%] - flat, and one of the two quietest cells in the whole wave. The four c3 files land within -1.88% and -0.03% on DFlash2 decode. Cell c1 NOT BANKED (resumed across the 21:30 job). Still owed: a host-cached unified-memory device, where the probe would actually select direct. |
| `0501-ggml-add-GGML_TYPE_SX8-57-the-S-X8-v4.3-CPU-decoder-.patch` | DOCUMENTED-TAKE. The harmless cell (the port present but unreachable) read DFlash2 decode +0.43% CI95 [-0.47%, +1.33%] and plain decode -5.33% [-12.26%, +1.60%]; the same pair moved plain PREFILL -8.28% [-11.00%, -5.56%], the wave's largest inert move, which is the build floor and not this port. TBO ladder x0.991 to x1.033. The S-X8 file cell never ran: no S-X8 file exists and the decoder is CPU-only. |
| `0502-ggml-port-jtrefon-TBQ3_0-TBQ4_0-at-ids-58-59-with-th.patch` | DOCUMENTED-TAKE. The harmless cell (types present, no launch names them) read DFlash2 decode +0.60% CI95 [-1.28%, +2.47%] and DFlash2 prefill +8.15% [+5.38%, +10.92%], the latter pure build floor. The KV MECHANISM WAS NEVER MEASURED: the baseline arm refused the type ("Unsupported cache type: tbq4_0") and the candidate arm was aborted by the schedule horizon. What ran is engine-vs-engine at KV-on-host, +1.60% [+1.30%, +1.90%] on a 0.515 t/s crawl. Owed: candidate-engine tbq4_0 vs candidate-engine q8_0; the baseline-engine design is impossible because the base engine does not know the type. |
| `0503-docs-protected-wins-the-R18-switch-table-eight-DOCUM.patch` | none - registry and manifest only, no code path touched |
| `0504-vulkan-add-the-S-X8-v4.3-kernel-for-GGML_TYPE_SX8-57.patch` | *(no Measured-effect trailer)* |
| `0505-vulkan-integer-MMQ-Q8_1-activations-for-GGML_TYPE_SX.patch` | *(no Measured-effect trailer)* |
| `0506-vulkan-mul_mm-prompt-processing-for-GGML_TYPE_SX8-57.patch` | *(no Measured-effect trailer)* |
| `0507-server-device-resident-context-checkpoint-ring-tool-.patch` | UNMEASURED - correctness test staged (test-save-load-state Test 9); served A/B owed to the serial lane |
| `0508-llama-Windows-unbuffered-model-reads-load-mode-direc.patch` | *(no Measured-effect trailer)* |
| `0509-tests-test-save-load-state-Test-9-device-storage-rin.patch` | *(no Measured-effect trailer)* |
| `0510-server-llama-device-checkpoint-storage-ID-ownership-.patch` | UNMEASURED for speed - correctness only. Fixes the reproducible R44 abort at task 594 of the c1 drafted sanity run (FIX-df2-L2/L5); served A/B in the lane-229 report |
| `0511-server-LLAMA_CKPT_STORAGE_TRACE-one-INFO-line-per-ch.patch` | UNMEASURED - diagnostic only, off by default |
| `0512-test-backend-ops-R46-real-27B-S-X8-decode-shapes-174.patch` | *(no Measured-effect trailer)* |
| `0513-vulkan-S-X8-mat-vec-decodes-a-whole-32-weight-block-.patch` | *(no Measured-effect trailer)* |
| `0514-vulkan-gate-the-S-X8-whole-block-mat-vec-decode-at-n.patch` | *(no Measured-effect trailer)* |
| `0515-vulkan-S-X8-integer-dot-mat-vec-against-Q8_1-activat.patch` | *(no Measured-effect trailer)* |
| `0516-vulkan-S-X8-q8_1-mat-vec-ships-OPT-IN-GGML_ARIFI_SX8.patch` | *(no Measured-effect trailer)* |
| `0517-vulkan-GGML_VK_ALLOC_TRACE-allocation-submit-instrum.patch` | *(no Measured-effect trailer)* |
| `0518-vulkan-alloc-trace-reads-per-heap-live-bytes-under-t.patch` | *(no Measured-effect trailer)* |
| `0519-vulkan-the-R47d-780M-S-X8-mat-vec-shape-lookup-as-a-.patch` | *(no Measured-effect trailer)* |
| `0520-vulkan-opt-in-explicit-placement-policy-GGML_VK_PLAC.patch` | *(no Measured-effect trailer)* |
| `0521-test-backend-ops-S-X8-ragged-m-K-tail-f16-activation.patch` | *(no Measured-effect trailer)* |
| `0522-test-backend-ops-drop-the-S-X8-m-1-correctness-cases.patch` | *(no Measured-effect trailer)* |
| `0523-test-vulkan-add-independent-S-X8-route-checks.patch` | *(no Measured-effect trailer)* |
| `0524-feat-vulkan-account-heap-reservations-transactionall.patch` | *(no Measured-effect trailer)* |
| `0525-feat-backend-preflight-Vulkan-buffer-type-batches.patch` | *(no Measured-effect trailer)* |
| `0526-feat-alloc-loader-wide-Vulkan-allocation-transaction.patch` | *(no Measured-effect trailer)* |
| `0527-feat-vulkan-bounded-host-split-with-explicit-staging.patch` | *(no Measured-effect trailer)* |
| `0528-fix-checkpoint-make-device-finalization-fallible.patch` | *(no Measured-effect trailer)* |
| `0529-feat-updater-preserve-fork-work-across-rebases.patch` | *(no Measured-effect trailer)* |
| `0530-arifi-sync-regenerate-the-patch-series-at-the-R46i-t.patch` | UNMEASURED - manifests only |
| `0531-gitattributes-evidence-receipt-files-are-byte-exact-.patch` | UNMEASURED - repository hygiene |
| `0532-arifi-sync-grandfather-the-untrailered-series-regen-.patch` | UNMEASURED - manifest only |
| `0533-arifi-sync-series-replay-applies-with-core.autocrlf-.patch` | UNMEASURED - tooling |
| `0534-arifi-sync-series-replay-keeps-CR-keep-cr-evidence-r.patch` | UNMEASURED - tooling and evidence bytes |
| `0535-feat-vulkan-MMVQ-A-side-decode-hoist-behind-spec-con.patch` | UNMEASURED - correctness only; pairing owed to HQ chain134 |
| `0536-docs-r48b-report-section-10-reconstruction-rc-receip.patch` | UNMEASURED - receipts only |
| `0537-docs-r48b-HQ-planted-red-receipts-on-the-committed-h.patch` | UNMEASURED - routing proof only |
| `0538-WIP-r48b-phase-2-hoist-type-width-gate-MMVQ-routing-.patch` | UNMEASURED - draft |
| `0539-docs-r48b-phase-2-gate-routing-verified-correctness-.patch` | UNMEASURED at default routing - forced-route pairing: S-X8 flat at the bus n=1..8 (+40..+110% at n>=4 on 248320x5120), q4_K n=6/7 tail cliff removed (5->52 GB/s), q4_K k=5120 n>=5 +21..+57%; default-route re-pair owed to HQ |
| `0540-WIP-r48c-Q6_K-mat-vec-x-fold-activation-hoist-draft-.patch` | UNMEASURED - correctness arms running, pairing owed |
| `0541-docs-r48c-clear-the-Q6_K-shader-of-HQ-finding-1-loca.patch` | UNMEASURED - correctness only; pairing owed to HQ chain134 |
| `0542-docs-r48c-independent-Fable-LOW-check-PASS-GATED-its.patch` | UNMEASURED - check receipts |
| `0543-docs-r48c-close-the-Fable-LOW-gates-measured-registr.patch` | UNMEASURED - documentation and coverage-claim corrections after independent check |
| `0544-feat-vulkan-Q5_K-mat-vec-activation-hoist-behind-spe.patch` | UNMEASURED - correctness only; pairing owed to HQ chain134 |
| `0545-docs-r48-Q5_K-report-section-10-reconstruction-HQ-pl.patch` | UNMEASURED - receipts only |
| `0546-feat-vulkan-gate-Q5_K-mat-vec-activation-hoist-to-NU.patch` | UNMEASURED - hoist gated to NUM_COLS<=3 after HQ pairing showed n>=4 spill (-30..-180%); re-pair owed |
| `0547-docs-r48-name-the-phase-1b-commit-SHA-and-downgrade-.patch` | UNMEASURED - report-only, names the phase-1b commit SHA and adds the 63-* receipt |
| `0548-arifi-sync-series-replay-checks-out-its-worktree-wit.patch` | UNMEASURED - tooling; replay of 550 patches now PASSES on a Git-for-Windows default config (receipt cache/r48i-evidence/35-series-check-autocrlf-fix.txt) |
| `0549-vulkan-R52b-S-X8-mul_mm-PACKED-tile-decode-and-integ.patch` | diagnostic only (one shape, receipts); HQ benches quiet (target: 4B S-X8 pp128 from ~380 toward q8_0's ~495 t/s) |
| `0550-docs-r52b-the-three-arm-diagnostic-MMQ-under-coopmat.patch` | diagnostic only (one shape, 3 interleaved rounds, receipts); HQ benches quiet |
| `0551-fix-r52b-perf-mode-supersedes-the-single-dispatch-nu.patch` | diagnostic only (one shape, perf mode, 6 interleaved rounds, receipts); HQ benches quiet |
| `0552-docs-r52b-correct-the-perf-case-list-line-citation-1.patch` | *(no Measured-effect trailer)* |
| `0553-docs-r52b-close-the-PASS-GATED-docs-gate-the-coverag.patch` | none - documentation only |
| `0554-docs-r52b-re-point-the-two-test-case-list-citations-.patch` | none - documentation only |
| `0555-feat-sx8-recover-the-S-X8-v4.3-PCA-correction-that-G.patch` | QUALITY - PPL wikitext-2 4B S-X8 9.9989 -> 9.9955 (-0.034%) with PCA ON (Q8_0 9.9742); llama-perplexity -c 512, 580 chunks, one cell at a time; speed UNMEASURED |
| `0556-fix-sx8-close-the-R53-check-gate-same-run-verifier-r.patch` | none - verifier receipt, identity guard (UNBUILT), docs |
| `0557-WIP-r51-host-split-order-mechanism-plan-order-trace-.patch` | *(no Measured-effect trailer)* |
| `0558-vulkan-name-the-buffers-a-bounded-host-split-places-.patch` | UNMEASURED by the maker - identity and receipts only, no placement change, bit-identical (27B S-X8 generated text sha 04312f94162fc4d31633 across all arms); no t/s predicted, the ordering lever was refuted |
| `0559-docs-lane-240-R51-check-gate-cache_r_l0-relabelled-a.patch` | none - documentation only |
| `0560-feat-vulkan-choose-the-host-split-memory-type-by-mea.patch` | UNMEASURED by the maker - placement only, bit-identical (every eligible heap-1 type is HOST_COHERENT, so no flush/invalidate obligation changes); HQ probes every memory type quiet and serves the winner (target: S-X8 27B plain 56 -> ~73 GB/s effective) |
| `0561-docs-r58-name-the-regime-and-the-receipt-line-assert.patch` | NONE - documentation only, no code touched |
| `0562-test-r58-phase-B-proof-cells-the-reorder-runs-the-ou.patch` | NONE measured by the maker - correctness and placement receipts only; bit-identical output across memory types 1 and 3; HQ chain139 owns every throughput number |
| `0563-series-regen-at-the-R56-tip-565-entries-G7-rows-for-.patch` | none - patch series regeneration and provenance ledgers |
| `0564-test-lane-246-R60-paired-S-X8-q8_0-mul_mm-coverage-a.patch` | op-level perf-mode only (rows in receipts); served pp128 UNMEASURED in this lane - HQ pairs quiet |
| `0565-feat-vulkan-make-the-mat-vec-admit-width-runtime-sel.patch` | op-level perf-mode only (rows in receipts); 71/71 OK at default and at =0, no default moves on any device; served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0566-test-r57-three-k-two-effects-the-width-4-knee-moves-.patch` | op-level perf-mode only (rows in receipts); no default moves, no shipped path touched; served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0567-docs-r57-registry-rows-for-the-admit-switch-and-for-.patch` | documentation only - no code path touched, no default moves |
| `0568-test-r57-the-decider-answers-past-the-mat-vec-bounda.patch` | op-level perf-mode only (rows in receipts); no default moves, no shipped path touched; served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0569-fix-vulkan-restore-the-admit-width-receipt-line-the-.patch` | one stderr line, emitted only when the switch is set; no default moves, no route change |
| `0570-docs-r57-fold-the-Fable-check-s-corrections-into-the.patch` | documentation only - no code path touched, no default moves; all numbers re-derived from the committed receipts |
| `0571-feat-r64-GGML_ARIFI_Q5K_MMVQ-lever-the-two-hoists-fi.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0572-fix-r64-the-q5_K-admit-is-n-5-not-n-2.8-the-measurem.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0573-feat-r64-the-q5_K-MMVQ-route-lands-proof-null-contro.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0574-fix-r64-withdraw-the-repeatable-at-every-width-claim.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0575-docs-r64-Fable-check-gates-corrected-counts-paired-c.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - the section 6b numbers are HQ's chain149 cells |
| `0576-feat-vulkan-r65-a-rows-per-workgroup-knob-on-the-pla.patch` | none yet - default unchanged (rows_from 4 = the inherited rule); no run behind this commit |
| `0577-test-r65-the-width-4-knee-is-width-locked-not-a-capa.patch` | op-level perf-mode only (rows in r65-evidence/22-ksweep-table.txt); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0578-test-r61-iq3_s-iq3_xxs-iq4_xs-rows-on-the-shapes-the.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0579-feat-r61-hoist-the-iq3-sign-application-out-of-the-N.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0580-fix-r61-the-dump-s-same-arm-control-FAILED-and-it-ca.patch` | none - test-harness determinism only, gated behind GGML_ARIFI_OP_DUMP and inert unset |
| `0581-docs-r61-three-registry-rows-for-the-R61-step-2-vari.patch` | documentation of already-measured rows; no new measurement in this commit |
| `0582-chore-r61-commit-every-raw-receipt-and-the-wrappers-.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0583-feat-vulkan-lane-254-R66-three-default-flips-q5_K-ro.patch` | UNMEASURED by the maker - integration; HQ measures the served chain |
| `0584-vulkan-measure-the-q6_K-q8_1-MMVQ-route-at-verify-wi.patch` | op-level on gfx1103, 6 interleaved counterbalanced rounds, paired; with GGML_ARIFI_Q6K_MMVQ=route n=7 is 1.400x on 248320x5120, 1.163x on 5120x17408, 1.122x on 17408x5120 and n=8 is 1.131x/1.050x/1.085x, all 6/6 rounds; the DEFAULT is unchanged so the shipped serve path is byte-identical and its served effect is zero by construction. |
| `0585-vulkan-q6_K-MMVQ-route-DEFAULT-ON-for-n-7-8-on-AMD-s.patch` | n=7,8 win 36/36 paired rounds on the three R71 shapes and all three occurrence picks (medians 1.400/1.122/1.163 at n=7, 1.131/1.085/1.050 at n=8, worst single round 1.003), plus G2 on the two previously untestable served shapes 5120x6144 (1.1254 6/6, 1.0576 5/6) and 1024x5120 (1.0876 6/6, 1.0087 5/6) with no measured loss anywhere; PPL inside one stderr at -b 8 (5.9932 vs 5.9945) and -b 7 (5.9897 vs 5.9947) |
| `0586-vulkan-q6_K-correct-the-R71b-route-admission-comment.patch` | none (comment only) |
| `0587-vulkan-q6_K-GGML_ARIFI_Q6K_F32_ROWS-R74-step-0-drive.patch` | none yet (diagnostic knob defaults to the shipped shape; timed decider pending) |
| `0588-vulkan-q6_K-GGML_ARIFI_Q6K_MMVQ_ROWS-the-q6_K-scoped.patch` | none (defaults to the shipped shape) |
| `0589-R74-report-the-q6_K-width-cliffs-are-REFUTED-at-the-.patch` | none served (nothing ships a default); decider 1536 round-cells, gates 261 targeted + 1004 MUL_MAT_ID cases EXECUTED |
| `0590-R74b-WIP-resumed-after-box-freeze-exact-n-q6_K-MMVQ-.patch` | *(no Measured-effect trailer)* |
| `0591-vulkan-q6_K-fence-the-R74b-n-6-route-cells-to-RDNA3-.patch` | *(no Measured-effect trailer)* |
| `0592-vulkan-q6_K-ship-the-R74b-n-6-MMVQ-route-cell-5120x6.patch` | *(no Measured-effect trailer)* |
| `0593-vulkan-q6_K-R74b-startup-route-line-names-RDNA3-text.patch` | *(no Measured-effect trailer)* |
| `0594-WIP-R85-q4_K-w5-arms-C1-next-slice-prefetch-C2-rows-.patch` | UNMEASURED - arms are default-off; the decider has not run |
| `0595-R85-ship-the-C3-4-1-column-split-for-q4_K-MMVQ-at-th.patch` | 248320x5120 n=5 12.75 -> 9.75 ms (x1.31, 6/6), n=6 13.64 -> 10.50 ms (x1.30, 6/6); 1024x5120 n=5 x1.06 (5/6), n=6 x1.09 (6/6); every other cell unchanged by construction |
| `0596-R87-ship-iq3_s-NUM_COLS-6-mat-vec-at-2-rows-per-work.patch` | served GSQ-RCO IQ3_S (logger ON, ABBA 3 rounds): iq3_s marg(5->6) +47.2 -> +23.8 ms per step; width-6 wall 522.8 -> 508.9 ms |
| `0597-R87-correction-iq3_s-n-6-rows-knob-narrowed-to-the-f.patch` | none on the served f32 path (same pipeline); f16-B iq3_s n=6 back to the inherited shape |
| `0598-R75-lane-262-WIP-iq4_xs-q8_1-MMVQ-shader-arm-registr.patch` | *(no Measured-effect trailer)* |
| `0599-vulkan-GGML_ARIFI_IQ4XS_MMVQ-legacy-route-all-switch.patch` | *(no Measured-effect trailer)* |
| `0600-tests-perf-rows-bs-1.9-at-the-served-attention-shape.patch` | *(no Measured-effect trailer)* |
| `0601-vulkan-iq4_xs-q8_1-MMVQ-admit-table-6-measured-cells.patch` | op-level perf-mode only (rows in receipts); served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0602-vulkan-GGML_ARIFI_IQ4XS_MMVQ-default-back-to-legacy-.patch` | none by default (legacy); route opt-in is op-level only, served t/s UNMEASURED by the maker - HQ pairs quiet |
| `0603-R86-lane-278-receipts-part-1-linearize-RED-x4-gate-b.patch` | *(no Measured-effect trailer)* |
| `0604-R86-lane-278-REPORT-receipts-part-2-timed-chain-dept.patch` | *(no Measured-effect trailer)* |
| `0605-R86-lane-278-report-correction-pre-registered-same-s.patch` | *(no Measured-effect trailer)* |
| `0606-R86-lane-278-report-summary-trimmed-under-300-words.patch` | *(no Measured-effect trailer)* |
| `0607-OW-014-lane-285-KV-precision-tail-trusts-ring-entrie.patch` | *(no Measured-effect trailer)* |
| `0608-W1-base-move-fixup-heapres-test-block-after-upstream.patch` | *(no Measured-effect trailer)* |
| `0609-W1-base-move-fixup-arifi_sync_check-expected-counts-.patch` | *(no Measured-effect trailer)* |
| `0610-W1-split-fixup-fork-buffer-layer-symbols-called-acro.patch` | *(no Measured-effect trailer)* |
| `0611-W1-base-move-fixup-DFlash-deferred-drafting-reads-po.patch` | *(no Measured-effect trailer)* |
| `0612-W1-base-move-fixup-ARIFI-SYNC-SOLO-on-upstream-s-coo.patch` | *(no Measured-effect trailer)* |
| `0613-arifi-sync-base-move-b10825-9e0e22059-b11178-f9af9be.patch` | UNMEASURED - register only |
| `0614-arifi-sync-re-key-provenance-ledgers-after-the-b1082.patch` | UNMEASURED - register only |
| `0615-OW-028-lane-279-ROCmFP4-FAST-q8_1-MMVQ-route-DEFAULT.patch` | see the lane-279 gates in the body; W1 re-runs the ROCmFP4 cells on b11178 |
| `0616-W1-split-fixup-Vulkan-test-fault-seam-atomics-are-in.patch` | UNMEASURED - test seam only; heapres re-run owed in the W1 battery |
| `0617-W1-collision-fixup-N3-MMQ-under-coopmat-opt-in-leave.patch` | none on the default path (opt-in OFF); runtime witness of the opt-in arm OWED (closed-lever launch gate refused it) |
| `0618-W1-collision-arms-N1-upstream-IQ4_XS-mat-vec-shader-.patch` | none on the default path (switches unset); arms receipts C:/ArifiLabs/cache/lane296/ev/B*-*.txt, ladder.txt |
| `0619-protected-wins-vulkan-uma-read-path-probe-symbol-re-.patch` | none - manifest only |
| `0620-W1-collisions-N11-N12-coopmat1-f16-B-guard-for-f32-B.patch` | pending run3 (C:/ArifiLabs/cache/lane296/ev3); run2 receipts C:/ArifiLabs/cache/lane296/ev2, ladder2.txt |
| `0621-W1-collisions-N12-RDNA3-device-probe-default-for-the.patch` | pending run4 (C:/ArifiLabs/cache/lane296/ev4); basis run3 C:/ArifiLabs/cache/lane296/ladder3.txt |
| `0622-W1-collisions-N12-RDNA3-f16-B-width-floor-24-for-the.patch` | pending run8 (C:/ArifiLabs/cache/lane296/ev8); basis run7 C:/ArifiLabs/cache/lane296/ladder7.txt |
| `0623-W1-collisions-N12-RDNA3-small-n-ffn_down-int8-except.patch` | *(no Measured-effect trailer)* |
| `0624-protected-wins-carry-registrations-across-the-W1-reb.patch` | none - registry only |
| `0625-metal-restore-upstream-s-dk-96-dv-64-sparse-FA-shape.patch` | none on this rig (Metal not compiled); restores an upstream shape admit |
| `0626-server-carry-the-tool-call-anchor-across-upstream-s-.patch` | *(no Measured-effect trailer)* |
| `0627-qwen35-chained-MTP-drafting-works-with-fused-QKV-GGU.patch` | *(no Measured-effect trailer)* |
| `0628-ggml-cpu-upstream-s-ARM-NEON-Q1_0-repack-arms-stay-d.patch` | *(no Measured-effect trailer)* |
| `0629-vulkan-graph_optimize-no-longer-hoists-a-UNARY-away-.patch` | none claimed; fused-op census + quality regate in lane-296 run 13 |
| `0630-arifi-sync-carry-spec-dflash-fused-inject-switch-to-.patch` | none - registry only |
| `0631-docs-GGML_ARIFI_VNNI_REPACK-scope-after-C019-ARM-NEO.patch` | none - documentation and a code comment only |
| `0632-arifi-sync-protected-win-resolutions.json-for-the-36.patch` | *(no Measured-effect trailer)* |
| `0633-tests-V023-asserting-regression-check-SILU-must-stay.patch` | *(no Measured-effect trailer)* |
| `0634-arifi-sync-protected-win-resolutions-B1-rows-measure.patch` | *(no Measured-effect trailer)* |
| `0635-arifi-sync-B1-rows-corrected-lane-296-248320-head-ce.patch` | *(no Measured-effect trailer)* |
| `0636-tests-SILU-MUL-perf-eval-cases-at-the-27B-FFN-width-.patch` | *(no Measured-effect trailer)* |
| `0637-lane-296-B1-run24-stage-h-close-in-protected-win-res.patch` | *(no Measured-effect trailer)* |
| `0638-lane-296-B1-correct-run24-order-effect-sign-record-k.patch` | *(no Measured-effect trailer)* |
| `0639-lane-296-B1-run25-close-of-OPEN-1-OPEN-2-protected-w.patch` | *(no Measured-effect trailer)* |
| `0640-lane-296-B1-OPEN-1-row-harness-sync-submit-pipeline-.patch` | *(no Measured-effect trailer)* |
| `0641-lane-296-B1-OPEN-1-not-yet-a-documented-loss-mechani.patch` | *(no Measured-effect trailer)* |
| `0642-night-sx8-numerics-revert-GDN-q-k-norm-to-ggml_l2_no.patch` | *(no Measured-effect trailer)* |
| `0643-Revert-night-sx8-numerics-revert-GDN-q-k-norm-to-ggm.patch` | *(no Measured-effect trailer)* |
| `0644-vulkan-keep-Q8_0-x-Q8_1-int8-coopmat1-under-GGML_ARI.patch` | *(no Measured-effect trailer)* |
| `0645-vulkan-GGML_ARIFI_Q8_0_CM1-off-on-min-rows-for-the-R.patch` | *(no Measured-effect trailer)* |
| `0646-vulkan-Q8_0-int8-cm1-default-FLOOR-int8-on-ssm_out-f.patch` | *(no Measured-effect trailer)* |
| `0647-vulkan-GGML_ARIFI_Q8_0_CM1_2D-roles-two-digit-int8-a.patch` | *(no Measured-effect trailer)* |
| `0648-lane-296-night-int8-accuracy-deliverable-floor-A-pas.patch` | *(no Measured-effect trailer)* |
| `0649-vulkan-two-digit-int8-second-digit-prescaled-by-2-GG.patch` | *(no Measured-effect trailer)* |
| `0650-vulkan-q8_1-residual-and-scale-f16-rounding-use-the-.patch` | *(no Measured-effect trailer)* |
| `0651-lane-296-int8-goal-INT8-MATH-derivation-op-level-err.patch` | *(no Measured-effect trailer)* |
| `0652-lane-296-int8-goal-conclusions-made-conditional-on-t.patch` | *(no Measured-effect trailer)* |
| `0653-vulkan-GGML_ARIFI_F32ACC-roles-all-forces-f32-accumu.patch` | *(no Measured-effect trailer)* |
| `0654-lane-296-int8-goal-stage-8-64-chunk-gap-is-real-2.7-.patch` | *(no Measured-effect trailer)* |
| `0655-lane-296-int8-goal-stage-8-pre-register-x16-pairing-.patch` | *(no Measured-effect trailer)* |
| `0656-lane-296-int8-goal-stage-9-F32ACC-takes-type-role-an.patch` | *(no Measured-effect trailer)* |
| `0657-vulkan-UMA-device-local-first-placement-on-the-Radeo.patch` | *(no Measured-effect trailer)* |
| `0658-sx8-int8-stage-10-S-X8-on-the-int8-coopmat1-MMQ-kern.patch` | *(no Measured-effect trailer)* |
| `0659-sx8-int8-stage-10-GGML_ARIFI_SX8_CM1_2D-fix-knob-two.patch` | *(no Measured-effect trailer)* |
| `0660-sx8-int8-stage-10-two-digit-activations-default-ON-f.patch` | *(no Measured-effect trailer)* |
| `0661-sx8-int8-attempt-E-16-value-activation-scales-for-th.patch` | *(no Measured-effect trailer)* |
| `0662-sx8-int8-attempt-E-quantize_q8_1-HALF-ds-store-via-v.patch` | *(no Measured-effect trailer)* |
| `0663-sx8-int8-attempt-D-two-digit-activations-only-for-ou.patch` | *(no Measured-effect trailer)* |
| `0664-sx8-int8-default-to-gate20-selective-two-digit-ffn_u.patch` | *(no Measured-effect trailer)* |
| `0665-vulkan-bound-SX8-residual-masks-and-exercise-gate-fi.patch` | *(no Measured-effect trailer)* |
| `0666-vulkan-use-float-below-S-X8-served-crossover.patch` | *(no Measured-effect trailer)* |
| `0667-vulkan-S-X8-width-gate-covers-prompt-widths-only-n-8.patch` | *(no Measured-effect trailer)* |
| `0668-S-X8-gate-comment-threshold-is-in-dispatch-columns-n.patch` | *(no Measured-effect trailer)* |
| `0669-server-keep-prompt-images-in-reusable-device-snapsho.patch` | *(no Measured-effect trailer)* |
| `0670-lane-298-skip-whole-file-mmap-prefetch-when-a-device.patch` | correctness fix (shipped-engine load drain), no speed claim: RED r1k-10060240 / GREEN r1l-10060355 (cache/mtp-x1-lane/cells) |
| `0671-lane-300-F-141-coopmat1-FA-zeroes-V-rows-masked-for-.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0672-lane-298-R1-checkpoint-fix-ssm-row-save-restore-maps.patch` | correctness fix (R1 checkpoint restore), no speed claim: RED r1h-10060107 / GREEN r1i-10060141, shipped base r1n-10060634 (cache/mtp-x1-lane/cells) |
| `0673-lane-298-qwen4exp-MTP-sidecar-load-graph_mtp-target-.patch` | part of win W5, MTP drafting for Qwen3.8-Flash-Next (qwen4exp): plain 10.73 -> MTP n-max 3 p-min 0.5 16.64 t/s short (+55.0%), long 10.38 -> 15.14 (+45.8%); accept 91.5% / 78.2%; MEANS of 2 rounds, -lm dio. cache/mtp-x1-lane/cells/gate2c-10050216-* (state head = 1c80315817). Lane grade: "pending strict 2a" [release ledger Table 1 W5] |
| `0674-lane-298-fix-Windows-lm-dio-probe-failure-unaligned-.patch` | part of win W6, Windows -lm dio direct-I/O load fixed: dio loads and serves uncapped: peak WS 0.95 GB, min available 5.4-9.7 GB, ids == reference (298 REPORT item 4); seat 77 reads the same cell as WS 1.0-1.4 GB, 15-17 s load. gate2c-10050216-state.json [release ledger Table 1 W6] |
| `0675-lane-298-unbuffered-read_raw_unsafe-throws-on-an-una.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W6, the number sits on that win's main commit |
| `0676-lane-298-qwen4exp-MTP-capture-is-a-real-graph-node-f.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W5, the number sits on that win's main commit |
| `0677-lane-298-server-emits-n_probs-on-the-speculative-pat.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W5, the number sits on that win's main commit |
| `0678-lane-298-spec-round-cost-timers-env-draft-recent-tok.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W8, the number sits on that win's main commit |
| `0679-lane-298-qwen4exp-MTP-draft-head-over-a-token-subset.patch` | part of win W7, dven40k draft subset head (Strata draft_vocab, 40,525 ids): +4.6..+8.7% decode on all 6 requests vs the MTP base, ids == base; cells/dvocab-10050535-* (state head = 7507baba3e) [release ledger Table 1 W7] |
| `0680-lane-298-MTP-eh_proj-as-one-2D-matmul.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 2 (lane 298), the number sits on that win's main commit |
| `0681-lane-298-OW-036-draft-penalty-default-on-ngram-mod-m.patch` | part of win W8, OW-036 draft recent-token penalty (pk4s3): long +2..+4.5%, other prompts flat, ids 12/12; cells/combo-10050615-*, ngcap* [release ledger Table 1 W8] |
| `0682-lane-298-ngram-mod-CAP0-gate-starts-at-full-width-ca.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0683-lane-298-item-5-drop-the-previous-ubatch-PLE-row-pag.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0684-lane-298-item-5-v2-PLE-release-one-whole-table-Virtu.patch` | part of win W12, PLE page release v2 (drop the previous ubatch's PLE rows from the working set): WS -67 vs +389 MiB per 8000 novel tokens, no repeat loss, ids 4/4; cells/pleio2-10051025-* (state head = e37036365e; default flip is the next commit) [release ledger Table 1 W12] |
| `0685-lane-298-item-5-PLE-release-default-ON-at-256-MiB-LL.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W12, the number sits on that win's main commit |
| `0686-lane-299-WIP-vulkan-HC_POST_W-fusion-SCALE-SIGMOID-S.patch` | part of win W11, HC_POST_W fusion (SCALE->SIGMOID->SCALE->DSV4_HC_POST in one dispatch): ABBA, 2 runs per arm: short 10.692 -> 10.828 (+1.2%), long 9.935 -> 10.599 (+6.6%), ids 8/8, -288 dispatches/token; fusion-x1-lane/cells/ab2-10050243-verdict.json (tps read and recomputed; state head 39d218e83d, which contains 6db5300cc1) [release ledger Table 1 W11] |
| `0687-lane-299-WIP-vulkan-GDN_BANK-fusion-GATED_DELTA_NET-.patch` | part of win W9, GDN_BANK fusion on the MTP verify path: +9.59% mean of four per-prompt MEDIANS of 3 rounds (short +13.65, long +8.88, copy +7.19, agent +8.65), ids 12/12, SET_ROWS 12.33 -> 0.08 ms; cells/gdnb-10051106-READ.txt (state head = 192127969f) [release ledger Table 1 W9] |
| `0688-lane-299-WIP-GDN_BANK-prepass-reads-SET_ROWS-operand.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W9, the number sits on that win's main commit |
| `0689-lane-298-item-5-prefetch-ahead-one-PrefetchVirtualMe.patch` | part of win W13, PLE prefetch-ahead (one PrefetchVirtualMemory per ubatch): novel 2,000-token prefill 126.01 -> 180.15 t/s (+42.9%), decode r1 +6.21%, repeat prefill +1.14%; MEANS of 4 requests; plant RED +2.02%; off2 tainted, off1 alone 126.00 -> 180.10; cells/pfa-10051313-* (state head = 6b0e35cf52) [release ledger Table 1 W13] |
| `0690-lane-298-item-5-prefetch-ahead-prefetch-after-the-PL.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W13, the number sits on that win's main commit |
| `0691-lane-298-item-5-prefetch-ahead-LLAMA_PLE_PREFETCH-de.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W13, the number sits on that win's main commit |
| `0692-lane-298-item-5-prefetch-ahead-keep-the-per-ubatch-W.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W13, the number sits on that win's main commit |
| `0693-lane-298-GDN-snapshot-byte-floor-diag-GGML_VK_GDN_SN.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0694-lane-298-GDN-replay-cost-bounding-diag-GGML_VK_GDN_R.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0695-lane-298-GDN-deferred-commit-replay-opt-in-LLAMA_GDN.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 2 (lane 298), the number sits on that win's main commit |
| `0696-lane-298-fix-two-log-format-strings-carried-raw-newl.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 2 (lane 298), the number sits on that win's main commit |
| `0697-lane-298-GDN-replay-verify-routes-an-unlogged-wide-b.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0698-lane-301-Vulkan-FA-sparse-gather-for-QSA-prefill-gqa.patch` | part of win W15, QSA sparse flash attention at prefill (Strata row 26): 32K prefill 105.8 -> 194.1 t/s (+83.4%), 16K 156.9 -> 205.7; 32K/2K ratio 0.49 -> 0.91; ids identical; FLASH_ATTN_EXT 21/21, RED fails the 9 prefill cases; -ub 4096 -b 4096 -c 33792, F16 KV, 2 rounds; qsa-x1-lane/cells/c3-10051215-* (bin-b source 1c80315817 dirty=2, Gap 7) [release ledger Table 1 W15] |
| `0699-lane-301-qwen4exp-row-25-persistent-pooled-indexer-k.patch` | part of win W16, Persistent pooled indexer-key cache for QSA decode (Strata row 25): decode at 32K 8.29 -> 10.35 t/s (+24.8%), 16K 9.41 -> 10.47; 32K/2K 0.79 -> 0.98; ids 16/16; RED=2 diverges; cells/c4-10051443-* (bin-d source 354ff5c9cc dirty=4, Gap 7) [release ledger Table 1 W16] |
| `0700-lane-301-row-25-gate-fix-M-RoPE-token-ubatches-carry.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W16, the number sits on that win's main commit |
| `0701-lane-301-protected-win-entry-qwen4exp-qsa-pooled-ind.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W16, the number sits on that win's main commit |
| `0702-lane-302-MUL_MAT_ID-expert-gather-for-small-batch-ma.patch` | part of win W17, MUL_MAT_ID slot-major expert gather at width 1: op level, real routing n4/n8: 0.73-0.94x of the old path per type (cells/cellA-10051633/); served perf split MUL_MAT_ID iq2_s 0.79, iq3_xxs 0.83, iq3_s 0.86 vs q2_0 control 0.88 (cells/cellB-10051832-read.json); served balanced ABBA x2 mean +0.34% (worst -0.69%), ids IDENT (cells/cellE-10052000-read.json, state head = 75b81abe13) [release ledger Table 1 W17] |
| `0703-lane-302-gather-leader-test-uses-a-mask-G-is-1-2-or-.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W17, the number sits on that win's main commit |
| `0704-lane-302-served-qwen4exp-MoE-op-test-shapes-n-1.8-ga.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W17, the number sits on that win's main commit |
| `0705-lane-302-served-MoE-op-test-cases-at-16-experts-only.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W17, the number sits on that win's main commit |
| `0706-lane-302-gather-pipelines-walk-slots-fastest-slot-ma.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W17, the number sits on that win's main commit |
| `0707-lane-302-protected-wins-entry-for-the-slot-major-MoE.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0708-lane-302-MoE-gather-default-ON-at-width-1-cellE-1005.patch` | UNMEASURED on its own - protected-wins registration commit; the number is in the entry it adds |
| `0709-lane-302-GGML_ARIFI_MOE_GATHER_ROWS-1-2-4-8-sets-row.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0710-lane-302-IQ3_S-gather-pipeline-defaults-to-2-rows-pe.patch` | as recorded in the subject: lane-302: IQ3_S gather pipeline defaults to 2 rows per workgroup (cellG-10052319: n4 -10%/-13% vs rm_iq in both rounds, harness 80/80 byte-identical); GGML_ARIFI_MOE_GATHER_ROWS still overrides every type |
| `0711-lane-302-protected-wins-entry-for-the-IQ3_S-gather-r.patch` | UNMEASURED on its own - protected-wins registration commit; the number is in the entry it adds |
| `0712-lane-302-IQ4_NL-gather-K-tail-redistribution-opt-in-.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0713-lane-302-IQ4_NL-q8_1-integer-dot-mat-vec-expert-gath.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0714-lane-302-IQ2_S-gather-body-arms-opt-in-GGML_ARIFI_IQ.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0715-lane-302-IQ2_S-gather-batched-grid-init-arms-opt-in-.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0716-lane-302-IQ2_S-gather-body-5-int8-sign-batched-grid-.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0717-lane-302-protected-wins-entry-vulkan-moe-gather-iq2s.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0718-llama-quant-same-type-tensors-are-byte-copied-so-ski.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W18, the number sits on that win's main commit |
| `0719-llama-quantize-read-via-slabbed-file-path-on-Windows.patch` | part of win W18, llama-quantize reads by slab on Windows (no mmap) + imatrix guard skip for byte-copied tensors: quantgate: R1 == R2 == R3 sha256 E60BC37C0A74DF39, planted R4 differs; quantize WS 1.18 -> 0.09 GB (seat 79 citing progress 19:11). The 27.5 GB tensor also needs --max-buffer-size 256 (CLI flag, no code). fusion-x1-lane/VERDICT.txt, cells/qgate1-10052023-* [release ledger Table 1 W18] |
| `0720-lane-299-protected-wins-entry-vulkan-qwen4exp-hc-pos.patch` | UNMEASURED on its own - protected-wins registration commit; the number is in the entry it adds |
| `0721-lane-299-protected-wins-entry-vulkan-qwen4exp-gdn-ba.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W10, the number sits on that win's main commit |
| `0722-lane-299-gdn-bank-protected-wins-entry-carries-the-i.patch` | UNMEASURED on its own - supporting commit; see release ledger Table 1 W10, the number sits on that win's main commit |
| `0723-lane-299-runbook-for-the-qwen4exp-decode-fusion-gate.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0724-moe-cache-x1-GLM-5.3-NVMe-expert-streaming-x12.x21-m.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0725-kimi-k3-read-the-ssm-bank-through-the-R1-rotated-ind.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0726-lane-298-integ-protected-win-registration-fixes-carr.patch` | UNMEASURED - no served number for this commit in the release ledger or the commit message |
| `0727-vulkan-scope-allocator-wide-shared-admission-to-MoE-.patch` | correctness fix (integ load regression), no speed claim: RED ismoke-10061217 arm ig2 / GREEN ismoke-10061337 (cache/mtp-x1-lane/cells) |
| `0728-lane-301-coopmat1-f16-B-staging-for-MUL_MAT_ID-on-no.patch` | prefill +2.3/+2.1/+1.6/+1.5% at 2K/8K/16K/32K, decode flat, ids identical (cache/qsa-x1-lane/cells/c14-10061050-state.json; quality cache/qsa-x1-lane/cells/c15-10061248-quality.json) |
| `0729-lane-301-protected-win-vulkan-cm1-f16b-staging-mul-m.patch` | none (registration only, no code change; the registered effect of 50b195beba is prefill +2.3/+2.1/+1.6/+1.5% at 2K/8K/16K/32K (cache/qsa-x1-lane/cells/c14-10061050-state.json)) |
| `0730-lane-302-IQ4_NL-gather-byte-pair-LUT-IQ3_S-gather-ba.patch` | OPT-IN, default off. Op level cells/cellQ-10061202-read.json (cache/moe-gather-x1-lane): IQ4_NL byte-pair LUT n2 +14.7/+15.8%, n4 +12.9/+15.6%, n8 +16.2/+19.5% slower; IQ3_S batched init n2 +10.2/+19.9%, n4 +3.7/+6.3% slower, n8 -1.8/-1.7%; both byte-identical (harness IDENT) with planted RED; default arm unchanged |
| `0731-lane-302-IQ3_XXS-gather-batched-grid-init-arm-opt-in.patch` | OPT-IN, default off. Op level cells/cellR-10061529-read.json (cache/moe-gather-x1-lane): IQ3_XXS batched init n2 +2.5/+2.1%, n4 +2.3/+8.3%, n8 +1.7/+1.9% slower, n1 +1.1/-1.3%; byte-identical (harness IDENT 3 arms), planted RED init=2 fails 9/10 |
| `0732-lane-300-arifi-det-probe-repeat-determinism-probe-wi.patch` | tooling only (tools/arifi-op-probe, no engine source); served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json |
| `0733-lane-300-det-probe-DETP_FA-FA-mask-live-column-dead-.patch` | tooling only (tools/arifi-op-probe, no engine source); served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json |
| `0734-lane-300-det-probe-DETP_FA_DUMP-FA-node-inputs-outpu.patch` | tooling only plus diagnostic env GGML_VK_FA_MASK_OPT_DISABLE (unset = unchanged condition); served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json |
| `0735-lane-300-arifi-fa-replay-model-free-replay-of-a-dump.patch` | tooling only (tools/arifi-op-probe, no engine source); served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json |
| `0736-lane-300-fa-replay-IGPU-backend-fallback-cell-10-dea.patch` | tooling only (tools/arifi-op-probe, no engine source); served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json |
| `0737-lane-300-F-141-coopmat1-FA-zeroes-V-rows-masked-for-.patch` | correctness fix: repeat-determinism 10/10 one sequence per key vs keep 2 distinct (cache/determinism-x1-lane/cells/c12-10051803-state.json, served ABA); teacher-forced cost TIE, CI spans 1.0 at w=1/3/4 (cells/c15-10052057, NOTES Cell 15 READ); planted RED GGML_VK_FA_DEADV_KEEP=1 |
| `0738-lane-300-det-probe-DETP_VERIFY-cost-mode-teacher-for.patch` | tooling only (tools/arifi-op-probe, no engine source); served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json |
| `0739-lane-300-det-probe-DETP_ROWS_OPS-2-full-node-dump-pe.patch` | tooling only (tools/arifi-op-probe, no engine source); served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json |
| `0740-lane-300-F-141-3b-LLAMA_ARIFI_MOE_SUMROWS_MAX-N-keep.patch` | correctness only, opt-in; default 1 = unchanged (served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json); 3b GREEN with the bundle (same file); opt-in cost not measured |
| `0741-lane-300-F-141-3b-FA-split_k-row-stable-under-GQA-wo.patch` | correctness only, opt-in after the default-0 commit (served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json); 3b GREEN with the bundle; opt-in cost not measured |
| `0742-lane-300-F-141-3b-row-stable-FA-split_k-also-on-the-.patch` | correctness only, opt-in after the default-0 commit (served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json); opt-in cost not measured |
| `0743-lane-300-F-141-3b-GGML_ARIFI_ADD_RMS_ROWSTABLE_N-N-k.patch` | correctness only, opt-in; default 0 (served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json); 3b GREEN with the bundle; opt-in cost not measured |
| `0744-lane-300-F-141-3b-arifi-mmv-width-model-free-MUL_MAT.patch` | tooling only (tools/arifi-op-probe, no engine source); served default ids == 96a6a8d976 binary, cache/determinism-x1-lane/cells/c27-10061355-state.json |
| `0745-lane-300-F-141-3b-GGML_ARIFI_MMV_ROWSTABLE_N-N-targe.patch` | correctness only, opt-in; ships ONLY with the next commit (alone it moves default Q4_K/Q5_K bits, cells/c25); pair default restored (cells/c26-10061224-state.json) |
| `0746-lane-300-F-141-3b-precise-Q4_K-Q5_K-MMVQ-dot-moves-t.patch` | correctness only, opt-in; default restored (cache/determinism-x1-lane/cells/c26-10061224-state.json Q-FX bed0e37284dd1839, O-FX 9ec22b32739bffed; served cache/determinism-x1-lane/cells/c27-10061355-state.json); opt-in cost not measured |
| `0747-lane-300-F-141-3b-GGML_ARIFI_FA_ROWSTABLE_N-default-.patch` | default-only change: default-OFF ids identical to the 96a6a8d976 binary served plain (cache/determinism-x1-lane/cells/c27-10061355-state.json vs c12-10051803, c13-10051909); opt-in cost not measured |
| `0748-lane-299-layout-copies-conv-tails-CPY-strided-no-CON.patch` | WIN +1.48% short / +1.19% long decode tok/s (all-on vs LLAMA_L299_LAYOUT_OFF=1, 10 rotated ABBA rounds, bootstrap CI [1.005, 1.0161] / [1.0061, 1.0155], ids exact 10/10 both prompts) cache/fusion-x1-lane/cells/ab11-10061754-verdict.json; dispatches 3027 -> 2868, plant RED first_diff 2/2 cells/ab7-10060555-verdict.json |
| `0749-lane-299-item-B-alpha-bias-joins-the-ssm_alpha-mat-v.patch` | LOSSLESS-TIE +0.09% short / +0.07% long decode tok/s (all-on vs LLAMA_L299_ALPHA_OFF=1, 10 rotated ABBA rounds, CI [0.9912, 1.0022] / [0.9943, 1.0035], ids exact 10/10 both prompts) cache/fusion-x1-lane/cells/ab11-10061754-verdict.json; dispatches 2868 -> 2833, plant RED first_diff 26/2 cells/ab8-10060930-verdict.json |
| `0750-lane-299-protected-wins-entry-qwen4exp-layout-copies.patch` | UNMEASURED (registration only; the win it registers is +1.48%/+1.19%, cache/fusion-x1-lane/cells/ab11-10061754-verdict.json) |
| `0751-lane-298-integ-GGML_VK_GDN_SNAP1_DIAG-prints-a-WARN-.patch` | none (diagnostic warning + docs only; default path unchanged) |
| `0752-lane-298-integ-round-2-protected-wins-commit-fields-.patch` | none (registration locator fix only; no engine change) |
| `0753-arifi-sync-port-the-sha-pinned-trailer-exemption-led.patch` | UNMEASURED - audit tooling only; no inference code path is touched. test_arifi_sync.py 169 tests OK. |
| `0754-arifi-sync-trailer-exemptions.json-rows-for-the-63-s.patch` | UNMEASURED - ledger only; no inference code path is touched. Offender classes on release/prep (tool predicates, classify.py): before 63 x1i rows unexempted, after 63/63 EXEMPT; 58 post-x1i remain for the reword. |
| `0755-arifi-sync-re-key-the-protected-win-ledgers-from-the.patch` | UNMEASURED - ledger keys and evidence copies only; no inference code path is touched. Re-key counts above CHECKED (07-b3-rekey.txt); protected-win validate --ref on a tagless clone is recorded in the release-prep MERGE-READY receipt. |
| `0756-arifi-sync-x1i-reword-trailer-table-and-replay-scrip.patch` | UNMEASURED - release tooling only; no inference code path is touched. Dry run on a scratch clone of release/prep: 60 commits replayed, 58 reworded, 60 shas changed, trees identical. |
| `0757-arifi-sync-x1i-reword-commits-the-ledger-re-key-as-t.patch` | UNMEASURED - release tooling only. Dry run on a scratch clone of release/prep @ 8c44efad0e: 62 commits replayed, 58 reworded, protected-wins.json re-keyed, ledger commit landed. |
| `0758-arifi-sync-x1i-reword-table-one-commit-per-win-carri.patch` | UNMEASURED - release tooling only. Dry run on a scratch clone of release/prep @ d3958ae2ed: 63 commits replayed, 58 reworded. |
| `0759-arifi-sync-x1i-reword-table-context-rows-carry-no-fi.patch` | UNMEASURED - release tooling only. effcheck.py over post-x1i-trailers.tsv: 13 rows classify as real, 45 as no effect. |
| `0760-lane-298-integ-round-2-freeze-prep-full-shas-on-the-.patch` | none (ledger keys and trailer-table text only; no engine change) |
| `0761-arifi-sync-re-key-the-sha-pinned-ledgers-after-the-p.patch` | UNMEASURED - ledger keys only; no inference code path is touched. |
| `0762-vulkan-IQ3_S-mat-vec-spec-id-clash-from-merge-round-.patch` | UNMEASURED (merge repair; restores the build, smoke run-ismoke2.ps1 is the receipt) |
| `0763-protected-wins-register-the-integ-wins-W5-W6-W7-W8-W.patch` | none (protected-win ledger registration; the numbers live in the entries) |
| `0764-common-fit-block-treats-draft-mtp-adaptive-as-MTP-lo.patch` | UNMEASURED (correctness fix; served-load receipt cells/afix-* from run-afix.ps1: integ crashes, fix loads, ids vs draft-mtp) |
| `0765-tools-qwen4exp-MTP-sidecar-builders-with-Strata-s-fe.patch` | none on the engine (tools only); the shipped scripts regenerate both served sidecars with identical tensors, cache/mtp-x1-lane/cells/sidecar-regen.txt |
| `0766-lane-300-F-141-producer-fix-zero-freed-KV-cells-befo.patch` | OPT-IN after cell 31 (default 0 from the commit 'KV_ZERO_FREED default 1 -> 0'): -fa off history == fresh bit-equal, S-NF F-nondet 4 -> 0 (cells/c31p-10070726-*); served ids Z0 == I every load (c31q-10070739, c31o-10070746, c31o-10070901); cost QM pooled +0.45% (floor 0.87%), OO pooled -0.31% / -3.23% (floors 0.38% / 7.33%) |
| `0767-lane-300-kv-zero-hardening-mid-decode-window-bound-t.patch` | OPT-IN after cell 31 (default 0 from the commit 'KV_ZERO_FREED default 1 -> 0'): -fa off history == fresh bit-equal, S-NF F-nondet 4 -> 0 (cells/c31p-10070726-*); served ids Z0 == I every load (c31q-10070739, c31o-10070746, c31o-10070901); cost QM pooled +0.45% (floor 0.87%), OO pooled -0.31% / -3.23% (floors 0.38% / 7.33%) |
| `0768-lane-300-LLAMA_ARIFI_KV_ZERO_FREED-default-1-0-cell-.patch` | OPT-IN; default restored to merge base (ids Z0 == I, cells/c31q-10070739-*, c31o-10070746-*, c31o-10070901-*); opt-in cost QM pooled +0.45% (floor 0.87%), OO pooled -0.31% / -3.23% (floors 0.38% / 7.33%) |
| `0769-build-info-version-prints-the-arifilabs-release-and-.patch` | none (string only); --version check in cache/mtp-x1-lane/cells/point-version.txt |
| `0770-docs-lane-299-runbook-NMSE-op-tests-miss-n_tokens-2..patch` | UNMEASURED (docs only; trap from cache/fusion-x1-lane/cells/ab14-10071210-verdict.json) |
| `0771-lane-302-lever-9-MUL_MAT_ID-expert-gather-for-Q2_0-a.patch` | UNMEASURED at commit; op gates + timing in cache/moe-gather-x1-lane/cells/cellT-*/SUMMARY.txt |
| `0772-lane-302-lever-9-GGML_ARIFI_MOE_GATHER_EXT-default-O.patch` | default OFF = integ route (cell T e0 32/32 IDENT to gather-off; cell U default-arm proof cache/moe-gather-x1-lane/cells/cellU-*/SUMMARY.txt); opt-in arm +8..+22% slower at n2..n8 on Q2_0 down, cache/moe-gather-x1-lane/cells/cellT-10070643/SUMMARY.txt |
| `0773-protected-wins-register-the-v0.1.3.1-opt-in-arms-KV_.patch` | none (protected-win ledger registration; the numbers live in the entries) |
| `0774-lane-300-LLAMA_ARIFI_KV_ZERO_FREED-default-0-1-HQ84-.patch` | default ON, cost QM (qwen4exp MTP n3) pooled +0.45% (floor 0.87%), OO (Ornith DFlash n2) pooled -0.31% / -3.23% (floors 0.38% / 7.33%, worst round -4.51%) (cells/c31q-10070739-*, c31o-10070746-*, c31o-10070901-*); -fa off history == fresh bit-equal, S-NF F-nondet 4 -> 0 (cells/c31p-10070726-*) |
| `0775-protected-wins-kv-zero-freed-cells-is-default-ON-in-.patch` | none (registry text; receipts are lane 300 cell 31: c31p-10070726, c31q-10070739, c31o-10070746, c31o-10070901) |
| `0776-tools-replace-local-C-ArifiLabs-paths-with-repo-rela.patch` | none (path handling only; proof cells/scrub-proof-*.txt: validate/check/series PASS in both root modes, every locator resolves) |
| `0777-vulkan-moe-cache-mask-the-IQ3_S-sub-block-scale-nibb.patch` | PENDING - functional cell r5d (IQ3_S NVMe path vs resident, ids compare) queued; no speed effect expected (one AND per sub-block). |
| `0778-tests-per-type-check-of-the-Vulkan-MoE-cache-matvec-.patch` | test only, no runtime change; receipt = iq3_s FAIL on 7156c61607 and PASS on 6fc7feeaa9 (cell queued). |
| `0779-tests-mul_mat-mul_mat_id-cases-on-a-slice-of-a-large.patch` | UNMEASURED (test only; RED/GREEN receipt in MERGE-READY.md row P1) |
| `0780-vulkan-read-the-batch-stride-of-an-in-place-src0-src.patch` | UNMEASURED (correctness; RED/GREEN on test-backend-ops MUL_MAT
  and MUL_MAT_ID in MERGE-READY.md row P1) |
| `0781-tests-matmul-wide-soft_max-matmul-on-the-same-input-.patch` | UNMEASURED (test only; RED/GREEN in MERGE-READY.md row P2) |
| `0782-vulkan-fix-stale-prealloc_y-reuse-across-flash-atten.patch` | UNMEASURED (correctness; RED/GREEN on MUL_MAT_PREALLOC_REUSE in
  MERGE-READY.md row P2) |
| `0783-vulkan-fix-flash-attention-Q8_1-shared-memory-write-.patch` | UNMEASURED (correctness; FLASH_ATTN_EXT gate in MERGE-READY.md row P3) |
| `0784-vulkan-null-vkEnumerateInstanceVersion-guard-argsort.patch` | UNMEASURED (robustness on other devices; ARGSORT gate stays green
  on the 890M, MERGE-READY.md row P6) |
| `0785-tests-MUL_MAT_PREALLOC_REUSE-quantized-tolerance-and.patch` | UNMEASURED (test only; RED/GREEN in MERGE-READY.md row P2) |
| `0786-tests-MUL_MAT_PREALLOC_REUSE-v2-strided-input-forces.patch` | none (test only; RED/GREEN in MERGE-READY.md row P2) |
| `0787-tests-gguf-handcrafted-file-whose-tensor-size-wraps-.patch` | none (test only; RED/GREEN in MERGE-READY.md row P5) |
| `0788-gguf-reject-crafted-files-that-hang-or-overflow-the-.patch` | UNMEASURED (correctness; test-gguf RED/GREEN in MERGE-READY.md row P5; load path, no decode effect) |
| `0789-tests-CLAMP-on-a-non-contiguous-view-RED-on-7156c616.patch` | none (test only; RED/GREEN in MERGE-READY.md row P7) |
| `0790-ggml-cpu-cuda-CPU-reference-fixes-CLAMP-views-soft_m.patch` | UNMEASURED (correctness; CLAMP RED/GREEN, SOFT_MAX_BACK and GET_ROWS_BACK no-regression in MERGE-READY.md row P7) |
| `0791-tests-MUL_MAT_PREALLOC_REUSE-runs-as-one-whole-graph.patch` | none (test only; RED/GREEN in MERGE-READY.md row P2) |
| `0792-tests-draft-acceptance-stops-at-EOG-RED-on-7156c6160.patch` | none (test only); RED rc=1 on 7156c61607, GREEN rc=0 with the fix (cache/upstream-x1-lane/cells/rg4-red-2-accept-eog.txt, rg4-green-2-accept-eog.txt) |
| `0793-common-stop-accepting-draft-tokens-at-EOG.patch` | test-spec-accept-eog RED on 7156c61607 (2 tokens accepted per overload), GREEN here (1); the served 2-turn replay effect is not measured. |
| `0794-llama-preserve-original-batch-order-for-speculative-.patch` | not yet measured; owed = -np 1 DFlash2 and MTP greedy ids identical vs integ, -np 2 DFlash2 acceptance per slot vs integ (upstream removed its regression test before merge). |
| `0795-tests-state-restore-must-refuse-a-different-KV-rotat.patch` | none (test only); RED rot FAIL (both mismatched restores accepted) on 7156c61607, GREEN rot PASS, other 12 columns PASS both arms (cache/upstream-x1-lane/cells/rg4-red-3-save-load-state.txt, rg4-green-3-save-load-state.txt) |
| `0796-kv-cache-save-the-exact-KV-rotation-with-the-state-r.patch` | not yet measured; check = test-save-load-state column "rot" RED on 7156c61607 (mismatched restores accepted), GREEN here (rejected), on a test-llama-archs llama model (head_k 128). |
| `0797-lane-301-lever-4-fuse-the-qwen4exp-QSA-mask-chain-in.patch` | prefill +0.2/+0.9/+0.4/+0.8% at 2K/8K/16K/32K, decode tie (-0.7..0.0%), greedy ids identical 128/128 at every length, served ABBA x2 vs integ 7156c61607 (cache/qsa-x1-lane/cells/c19-10080535-state.json); op RED FAIL 7/7 (cells/c18-10072033-ops-red.out.log); mechanism RGP compaction dispatch ~42 -> ~18 us at 32K (cells/c18-rgp-*) |
| `0798-lane-301-tests-QSA_MASK_FA-perf-mode-cases-RGP-targe.patch` | none (test shapes only) |
| `0799-lane-301-protected-win-vulkan-qsa-mask-fa-fusion-for.patch` | none (registration only; the measured effect is f42f16e38c's, cache/qsa-x1-lane/cells/c19-10080535-state.json) |
| `0800-ggml-collect-all-input-tensors-into-graph_inputs.patch` | none on the rig (n_copies 1 path unchanged by construction); multi-GPU pipeline-parallel correctness, not measurable here. |
| `0801-ggml-require-input-tensors-to-be-GGML_OP_NONE.patch` | guard only, no runtime cost; test-llama-archs over all archs builds every graph under the assert (redgreen-4). |
| `0802-llama-re-reserve-the-sched-when-the-nextn-extraction.patch` | not yet measured; check = MTP serve under GGML_SCHED_DEBUG_REALLOC (no mid-run realloc after the first decode), owed with the next MTP served cell. |
| `0803-glm5-next-give-dead-indexer-slots-unique-scatter-row.patch` | not yet measured; check = CPU greedy 2-sequence glm5-next run (test-llama-archs model) ids identical across 3 runs; decode cost on the GLM5-Next serve line owed to moe-x1-lane. |

## Unclassified

These commits fell through every grouping rule in `arifi_sync.py`. That is surfaced
rather than silently bucketed - add a rule when a new source appears.

- `7e7ec5b6a` patches: generate the full auditable 38-patch series from git
- `6860f6822` tools: add arifi-sync - the currency, series and upstream-bump driver
- `cd1866098` patches: move banked PrismML source patches out of the generated series directory
- `3fcab8134` patches: make the series generator implement its own convergence rule
- `380c0ba9e` patches: stop the manifest deriving anything from excluded commits
- `8da948373` port: Q1_0 repack scaffolding + Arm NEON/DP GEMV-GEMM kernels
- `9196f0b03` port: native x86 Q2_0 vec_dot (AVX512-VNNI / AVX-VNNI), drop scalar fallback alias
- `d65eb11c4` port: x86 AVX512-VNNI repack GEMV/GEMM for Q1_0 and Q2_0
- `5e0e1a101` fix(ggml-cpu): parameterize Q2_0 GEMM activation stride by QK2_0/QK8_0 - g64 NaN
- `723a68af5` fix(ggml-cpu): same g64 stride defect in the ported x86 Q2_0 vec_dot
- `fc46a74e3` ggml: reserve the ArifiLabs type-ID blocks at the enum itself
- `672273b1e` ggml: ROCmFP4/ROCmFPX weight formats behind GGML_ARIFI_ROCMFPX_FORMATS
- `f9224cefb` llama: ROCmFPX file types, quantizer wiring and the gguf-py registry
- `519204b59` tools: record GGML_ARIFI_ROCMFPX_FORMATS in the build-recipe snapshot
- `53c28a68c` feat: --arifi-profile and a CPU-only VNNI-repack startup advisory
- `3e8e15fa5` sync: currency GREEN — TurboQuant checkouts pinned at their lane-110C-reviewed shas (c26cbdffc, ba52ad107), rocmfpx pin advanced to db6844d9b after 6-commit review (HIP pinned-staging noted for the HIP-deltas slot)
- `055efbf0d` fix(ggml-cpu/x86): don't pass __m256i by value across the repack GEMV/GEMM ABI boundary (Windows 0xC0000005)
- `10adb5b3b` ggml: TurboQuant TQ3_1S/TQ4_1S weight formats behind GGML_ARIFI_TURBO_WEIGHT_QUANTS
- `17ac2969b` ggml-cpu/x86: use the row-stride parameter in repack GEMV stores instead of the row count
- `ee199085a` docs(FINDINGS): F-09 - the repack/GPU trade-off is a compute-placement choice, not a bug
- `a6daca42b` docs(FINDINGS): record why the repack auto-enable was NOT shipped
- `8024f23d7` provenance: catch the silent trailer-block break instead of only reporting it
- `16ac24b4e` gitattributes: pin .githooks to LF so the hook runs on Linux and macOS
- `f887a3fd5` githooks: force a normalized blob for the commit-msg hook
- `43ea0678b` docs(FINDINGS): F-09 exit 1 is the direction - dual residency, not a forced choice
- `d2574a071` repack: dual residency - GGML_ARIFI_VNNI_REPACK=2 keeps prefill on the GPU AND repacks decode
- `169bbd309` repack: dual residency measured - both wins held; fix the hot-path cost that ate the first one
- `7afc0e8a1` repack: fix the shadow tensor COUNT in the accounting line, and strengthen the claim it supports
- `524a1bbf3` repack: CPU kernels for the 128-group ternary format
- `44aa767aa` tests: direct equivalence test for the ternary repack kernels
- `0816b04dd` F-06: measure the GPU tier on one binary, and count the placement the prompt claim rested on
- `71b37680b` registry: the dual-residency prompt mechanism is measured now, not ASSUMED
- `8164d2b48` registry: our DSpark verdict was never ours -- the drafter cannot load on this fork
- `f19a3d829` speculative: DFlash/DSpark drafters have no vocab -- take the mask token from metadata
- `0c0cd9a57` expert cache: stride the bundle by the PADDED matrix size -- unblocks our own brain
- `3b6bec7f4` expert cache: POWERINFER_EXPERT_HEATMAP -- measure access skew before building a pin policy
- `ab0332a9e` ours/1163cb34939fe4a9cb07aec034c5954144497ae9.patch
- `79edc9c98` 1-core-types/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `7d42a5921` 1-core-types/0002-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `5cc1488b5` 2-cpu-reference/0001-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `fc9802e79` 2-cpu-reference/0007-ggml-cpu-declare-turbo3_cpu_wht_group_size-extern-no.patch
- `36f5faa97` 2-cpu-reference/0008-merge-fix-post-merge-build-and-crash-issues.patch
- `58179605c` 3-vulkan/0001-vulkan-metal-hip-add-TurboQuant-kernel-support.patch
- `e238783e1` 3-vulkan/0002-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch
- `780e81fbb` 3-vulkan/0003-fix-close-complete-audit-findings-7-port-regressions.patch
- `24a1643b4` 3-vulkan/0004-vulkan-reconstruct-supports_op-after-rebase-merge-da.patch
- `aba4835a5` 3-vulkan/0006-vulkan-restore-TQ4_1S-weight-type-wiring-lost-in-the.patch
- `9307230fd` 3-vulkan/0007-vulkan-add-the-TQ3_1S-weight-type.patch
- `24caccd26` 3-vulkan/0008-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch
- `7e7f885f8` 3-vulkan/0013-vulkan-TQ-rotated-matmul-shader-groundwork-A-side-lo.patch
- `e310fe9ee` 3-vulkan/0015-vulkan-create-the-TQ-activation-rotate-pipeline.patch
- `1af80c3e2` 3-vulkan/0017-vulkan-rotate-the-staged-activation-copy-in-place-dr.patch
- `87f6b4862` 3-vulkan/0018-vulkan-register-the-TQ-rotated-mul_mm_id-pipelines.patch
- `5633b6c81` 3-vulkan/0021-vulkan-fix-SET_ROWS-block-decomposition-for-turbo-we.patch
- `15305fe1d` 4-other-backends/0006-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch
- `e3887560f` 4-other-backends/0007-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch
- `84f20e818` 4-other-backends/0008-hip-add-mixed-f16-bf16-q8_0-fattn-vec-instances-to-t.patch
- `a67726d61` 4-other-backends/0009-cuda-revert-GQA-ratio-dispatch-to-comparison-form-fi.patch
- `1c4ddb0b4` 4-other-backends/0010-cuda-restore-Volta-GQA-modulo-dispatch-over-revert-i.patch
- `efaa788c6` 4-other-backends/0015-cuda-rework-MoE-expert-cache-execution.patch
- `a7468d449` 4-other-backends/0016-cuda-fix-MoE-cache-capacity-accounting.patch
- `2a49603c9` 4-other-backends/0017-cuda-fix-MoE-cache-scratch-budget-replacement.patch
- `26a5befeb` 4-other-backends/0018-cuda-bound-MoE-MMV-tail-row-reads.patch
- `cedcecc08` 4-other-backends/0019-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `a7342bb64` 4-other-backends/0020-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `a542082d7` 4-other-backends/0021-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `33cabbe6b` 4-other-backends/0022-cuda-prefer-generic-MMV-for-compatible-MoE-cache-nod.patch
- `0c125ebee` 4-other-backends/0023-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch
- `5eda84c8b` 4-other-backends/0024-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `3ec648746` 4-other-backends/0025-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `a60d46121` 4-other-backends/0026-cuda-bound-automatic-MoE-cache-admission.patch
- `96232ed24` 4-other-backends/0027-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `d7b5ef5ef` 4-other-backends/0028-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `2dfa46b0f` 4-other-backends/0029-common-surface-MoE-cache-activation.patch
- `403b70e20` 4-other-backends/0030-cuda-accelerate-complete-MoE-cache-pools.patch
- `ab3ec95d3` 4-other-backends/0031-cuda-report-oversized-MoE-cache-nodes.patch
- `39fdfb689` 4-other-backends/0032-cuda-clarify-MoE-cache-session-diagnostics.patch
- `847a02d4c` 4-other-backends/0033-cuda-report-MoE-cache-pair-residency.patch
- `aaee3b626` 4-other-backends/0034-cuda-pack-MoE-cache-dispatch-inputs.patch
- `87660fa66` 4-other-backends/0037-metal-port-TQ_NO_ROTATE-escape-hatch-from-stash-opt-.patch
- `51a001a57` 4-other-backends/0039-moe-cache-enable-HIP-backend-by-removing-no-op-stubs.patch
- `e42036adb` 4-other-backends/0040-moe-cache-count-dispatch-contention-bypasses-P1.patch
- `eca3f76f5` 4-other-backends/0041-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `918588bfb` 4-other-backends/0042-moe-cache-audit-fixes-H1-F1-F2-A1.patch
- `5e9cf1aa0` 4-other-backends/0043-moe-cache-fix-registry-build-I1-follow-up.patch
- `a16ad0072` 4-other-backends/0044-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `b4984db5b` 4-other-backends/0045-cuda-fix-HIP-MoE-cache-compatibility-and-q8_1-sums.patch
- `fb5fd21ed` 4-other-backends/0046-moe-cache-add-logging-at-silent-failure-points-acros.patch
- `3bd06c183` 4-other-backends/0048-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch
- `18ab077fc` 4-other-backends/0049-sycl-drop-the-duplicated-nthreads-declarations-in-fa.patch
- `7a3339536` 4-other-backends/0050-fattn-vec-split-the-turbo-K-dot-at-D-128-to-stop-the.patch
- `be9541074` 5-models-server/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `41448a06b` 5-models-server/0003-cli-break-interactive-loop-on-stdin-EOF-fixes-hot-sp.patch
- `37929a5d6` 5-models-server/0004-tools-expose-TQ3_1S-TQ4_1S-in-quantize-table-and-lla.patch
- `0af9f9be5` 5-models-server/0007-llama-support-DeepSeek-V4-tensor-split.patch
- `b630bb5a8` 5-models-server/0008-llama-mirror-DS4-q_a-kv-down-projections-in-tensor-s.patch
- `40b4bdb0a` 5-models-server/0009-glm-dsa-guard-lightning-indexer-Hadamard-rotation-wh.patch
- `c2260d79e` 5-models-server/0010-laguna-add-arch-tables-llm_arch-KV-keys-tensors-and-.patch
- `e3f4ad3a4` 5-models-server/0011-laguna-add-model-class-hparams-wiring-vocab-pre-toke.patch
- `9870e2ede` 5-models-server/0013-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch
- `a0a40906e` 5-models-server/0014-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch
- `d998e675a` 5-models-server/0015-cuda-sum-MoE-expert-outputs-on-decode-n_tokens-1.patch
- `6b6bc88a4` 5-models-server/0016-laguna-deduplicate-definitions-vs-the-early-snapshot.patch
- `3100ea638` 5-models-server/0017-dflash-fix-DSpark-tensor-meta-accessor-after-laguna-.patch
- `91eb46d0c` 5-models-server/0037-merge-fix-post-merge-build-and-crash-issues.patch
- `3f162dcd2` 5-models-server/0041-moe-cache-route-fit-probing-and-test-session-calls-t.patch
- `f12c5c9cf` 5-models-server/0048-llama-bench-document-pw-prefetch-weights-in-help-and.patch
- `6bc432494` 5-models-server/0050-llama-bench-remove-stale-prefetch-weights-documentat.patch
- `f0878e0ba` 5-models-server/0051-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch
- `ea5c2d260` 6-tests-build/0001-tests-re-add-test-turbo-quant.c-round-trip-test.patch
- `fc45de9d2` 6-tests-build/0002-tests-port-fork-turbo-backend-and-quantize-fns-test-.patch
- `cf6398926` 6-tests-build/0003-fix-close-complete-audit-findings-7-port-regressions.patch
- `b54c821d6` 6-tests-build/0004-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch
- `961be3ed3` 6-tests-build/0006-ggml-webgpu-add-support-for-f16-repeat-26307.patch
- `00e01961e` 6-tests-build/0010-tests-restore-DSV4_HC_COMB-eps-sweep-lost-in-the-244.patch
- `87b2e0bc8` 6-tests-build/0011-tests-cast-float-printf-args-in-test-turbo-quant-arm.patch
- `b0eb26776` 6-tests-build/0012-vulkan-add-the-TQ3_1S-weight-type.patch
- `a6f090b30` 6-tests-build/0013-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch
- `2b40e5de9` 6-tests-build/0014-ggml-cpu-remove-the-per-call-mallocs-from-the-TurboQ.patch
- `e73bc52e8` 6-tests-build/0015-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch
- `c16526481` 6-tests-build/0016-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch
- `4c810c716` 6-tests-build/0017-tests-port-11-DSv4-shaped-TQ-MUL_MAT-cases-from-sync.patch
- `7731084a4` 6-tests-build/0019-cuda-fix-MoE-cache-capacity-accounting.patch
- `38e89c1f7` 6-tests-build/0020-common-preserve-implicit-MoE-cache-provider-settings.patch
- `41c45982f` 6-tests-build/0021-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `99ca800ee` 6-tests-build/0022-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `786ab2d52` 6-tests-build/0023-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `db72c4193` 6-tests-build/0024-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch
- `17c7c07cd` 6-tests-build/0025-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `089040972` 6-tests-build/0026-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `c1794fdc9` 6-tests-build/0027-cuda-bound-automatic-MoE-cache-admission.patch
- `abe0313b9` 6-tests-build/0028-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `e049aa5dc` 6-tests-build/0029-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `3aeabc456` 6-tests-build/0030-common-surface-MoE-cache-activation.patch
- `b8b8445b9` 6-tests-build/0031-cuda-accelerate-complete-MoE-cache-pools.patch
- `f6f4793eb` 6-tests-build/0032-cuda-report-oversized-MoE-cache-nodes.patch
- `77200cb60` 6-tests-build/0033-tests-initialise-non-contiguous-tensors-row-by-row.patch
- `54d8576e7` 6-tests-build/0034-tests-cover-turbo-KV-flash-attention-at-head-dim-256.patch
- `42b1dc6da` 6-tests-build/0037-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `7b75d2d81` 6-tests-build/0038-moe-cache-route-fit-probing-and-test-session-calls-t.patch
- `352f820cc` 6-tests-build/0039-moe-cache-route-context-eligibility-probe-and-remain.patch
- `a129a4af8` 6-tests-build/0040-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `6845f5eef` 6-tests-build/0041-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch
- `5be7df7cb` 6-tests-build/0042-test-moe-cache-accept-IGPU-devices-not-just-GPU.patch
- `c63d600e1` 6-tests-build/0043-test-moe-cache-make-cache-shared-budget-UMA-aware-wi.patch
- `cbf7302e8` 6-tests-build/0044-test-moe-cache-save-and-restore-the-CUDA-device-arou.patch
- `90818e781` 6-tests-build/0045-tests-tell-ctest-that-77-means-skip-for-test-moe-cac.patch
- `896e01245` resolved/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch
- `de73817f4` resolved/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch
- `cc4cf49d4` resolved/0005-merge-close-TurboQuant-parity-gaps.patch
- `c45506904` resolved/0036-common-surface-MoE-cache-activation.patch
- `65379afea` resolved/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch
- `2e27fa2ac` resolved/0004-fix-close-complete-audit-findings-7-port-regressions.patch
- `c3b9f9607` resolved/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch
- `41b8d5f27` resolved/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch
- `f0535038d` resolved/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch
- `87718e709` resolved/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch
- `3213b5ae6` resolved/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch
- `e52ddca47` resolved/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch
- `c824d2526` resolved/0006-fix-close-complete-audit-findings-7-port-regressions.patch
- `271b69fee` resolved/0022-DeepseekV4-MTP-DSpark-25784.patch
- `6c26a9a2e` resolved/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch
- `40cf2c16d` resolved/0029-server-harden-shared-draft-device-placement.patch
- `985955a40` resolved/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch
- `178d136a7` resolved/0026-cuda-rework-MoE-expert-cache-execution.patch
- `97847f655` resolved/0033-server-account-for-MTP-placement-in-fit-reservations.patch
- `7acdcab79` resolved/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `939e27273` resolved/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch
- `178a18de4` resolved/0039-model-Muse-Glimmer-Support-26841.patch
- `5d3cee1ab` resolved/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch
- `6c5b1b31f` resolved/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch
- `6deea2ec3` resolved/0046-llama-remove-dead-MSA-indexer-scaffolding.patch
- `a8cb67290` resolved/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch
- `32b80fe16` resolved/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch
- `3c94f5827` resolved/0005-test-fix-some-CI-errors-26415.patch
- `11838d097` resolved/remaining-conflict-markers
- `43a7b1982` layer7/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `45dc09ac7` layer7/0002-Update-GGMLQuantizationType-and-LlamaFileType-enums-.patch
- `2627f7d80` layer7/0003-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `a9f8dab7f` layer7/0005-docs-add-rebase-plan-and-TurboQuant-recipes.patch
- `a55de4467` layer7/0007-gguf-py-dedupe-stacked-merge-artifacts-in-constants..patch
- `7bee8a8a3` layer7/0008-docs-add-KV-cache-quantization-guide-update-rebase-p.patch
- `77eaf7ec9` layer7/0009-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch
- `1247ae4dc` layer7/0010-merge-close-TurboQuant-parity-gaps.patch
- `a03eaa89d` layer7/0011-metal-restore-lost-kernel-templates-add-offline-shad.patch
- `4a3978b3d` layer7/0014-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch
- `722270795` layer7/0019-fix-apply-rebase-audit-fixes-stack-overflow-quality-.patch
- `6dbd2992c` layer7/0020-docs-add-TurboQuant-project-overview-to-AGENTS.md-an.patch
- `fd695760c` layer7/0021-cuda-rework-MoE-expert-cache-execution.patch
- `10728bd54` layer7/0022-docs-update-MoE-cache-validation.patch
- `15541d4a9` layer7/0023-common-preserve-implicit-MoE-cache-provider-settings.patch
- `85996f73c` layer7/0024-server-harden-shared-draft-device-placement.patch
- `1003100ac` layer7/0025-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `25b1a46be` layer7/0026-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `c481fab9a` layer7/0027-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `77d4dfd82` layer7/0028-docs-update-MoE-cache-validation.patch
- `ebec51f9e` layer7/0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `50099fe0d` layer7/0030-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `0672d5f09` layer7/0031-cuda-bound-automatic-MoE-cache-admission.patch
- `d59ec184e` layer7/0032-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `19d016fe6` layer7/0033-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `afca1f4e4` layer7/0034-common-surface-MoE-cache-activation.patch
- `dc21ee7fe` layer7/0035-docs-fix-MoE-cache-benchmark-verbosity.patch
- `2fb9ffe81` layer7/0036-cuda-accelerate-complete-MoE-cache-pools.patch
- `964e46ae1` layer7/0037-cuda-report-oversized-MoE-cache-nodes.patch
- `3d4ba04f7` layer7/0038-cuda-clarify-MoE-cache-session-diagnostics.patch
- `6426da001` layer7/0039-docs-record-MoE-cache-workload-convergence.patch
- `1927e6559` layer7/0040-cuda-report-MoE-cache-pair-residency.patch
- `deb396a84` layer7/0041-docs-document-what-the-test-suites-do-and-do-not-cov.patch
- `ece8a21a0` layer7/0044-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `bb807156c` layer7/0046-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `b223bd67e` layer7/0048-docs-document-INFO-level-MoE-cache-disable-reasons-i.patch
- `61e4d9d46` layer7/0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CACHE.md-and-re.patch
- `3a5a90712` layer7/0050-docs-correct-MOE-CACHE.md-against-the-implementation.patch
- `4e2473c6f` layer7/0051-ggml-split-the-turbo3-group-size-declaration-from-it.patch
- `b2abd491c` resolve 8 committed conflict markers on merit
- `7e987f524` fix the merge so it builds: 5 files, 6 defects
- `194fc0b68` merge repairs: seven defect classes, and one the compiler could never have reported
- `6c46ac5f0` ROCmFPX on Vulkan: the shader half, ported. NOT YET WORKING - it segfaults in execution.
- `7c75ae264` currency: register ALL SEVEN forks, so nothing can drift unseen again
- `dc23ffe2b` series: base and the branch it describes are ONE pair, and the linearity rule is now a gate
- `6727d0d79` hygiene: the code-review skill taught a type-ID map the arbitration deleted, and the UI was fetched from a moving tag
- `b415d09f6` contract: tq3 is the first family we must renumber, so write down why before anyone ports it
- `0778348ae` sources: honest pins - turboquant re-pinned off a dead lineage, tq3 main-understates note REFUTED, five type-id contract collisions recorded (lane-143)
- `81862eb23` sources: lane-143 self-refutation - the turbo3 ODR 'defect' does not exist in our tree (GGML_API already carries extern on all four branches); prisml pin REVERTED to keep the BEHIND signal live; FLAG E severity re-derived from loader code
- `1e35ace0c` sources r2 (checker BLOCK F1-F6): ciru 160 was 145 commits of rocmfpx history - corrected to a 7-15 bracket, no-ancestor premise refuted, GOAL-3 ciru review performed, rocmfpx.ref revert, prisml contradiction removed, FLAG E sensitivity added
- `e17ed8a5c` series: point base.ref at the LINEAR branch - recut-linear/2026-08-17 is the byte-identical flatten of the blessed union branch, so the series is replayable at last (lane-147)
- `5a799984b` series: disclose in the record itself that the flatten absorbed 17 empty commits - content unchanged, history count changed (lane-147 checker finding 4)
- `9658c1826` ggml: port the tq3 TQ3_4S family codec at ids 48-51 (TYPE-ID-ALLOCATION 3.1.1) - Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb
- `840064a30` tests: cover the tq3 family in test-quantize-fns and test-backend-ops (kilobyte-scale proof before the 13 GB run)
- `308878402` contract: tq3 rows 48-51 implemented behind GGML_ARIFI_TURBO_WEIGHT_QUANTS, plus the three corrections the port forces (their 200 = our TURBO2_0; six ids, four destinations; token_embd is Q6_K)
- `96ab60613` tests: tq3_4s/tq3_4se skipped in test-quantize-fns with the measured reason (E3M5 scale caps at 0.492, harness data needs 1.4); tq3_0 given documented 3.5bpw bounds
- `29cee8978` tools: gguf-retag-tq3 - lands LAST, after the coherence proof, per TYPE-ID-ALLOCATION 3.1.1. Decides ambiguous id 46 by measured byte span; five refusals, no in-place path
- `7906db777` checker fixes: choose_index restored verbatim (inverted tie-break + 2 drifted constants, encoder-only); tq3 numeric block gives TQ3_4S real proof and pins every dot mapping; drop redundant GGML_API redecl
- `091fa1ab5` retag: verify geometry per TENSOR for every mapped id, not a file-level vote on 46 only - fixes silent rewrite of mixed/ambiguous files and of unguarded 36/200; guard empty and overlapping tables
- `97ab4c1f9` retag README: per-tensor geometry check for every mapped id, ten refusals, byte-level assertion
- `c61e737ab` lane-144 GOAL 2 step 1: TQ3_4S (id 48) Vulkan port - dequant + mat-vec + mat-vec_id shaders, wired
- `595f513fe` lane-144: sweep ROCmFP4/ROCmFPX in test-backend-ops all_types (six identifiers, never listed before)
- `f2f0fa247` lane-144 CHECKER FIX: TQ3_4S was missing from the MUL_MAT_ID staging-overflow guard
- `28acc28ea` lane-145: Q6_0_ROCMFPX Vulkan fix - implement the 6-bit unpack (block struct was 34B vs CPU 26B; qs read as whole signed bytes). 60/60 MUL_MAT + 24/24 GET_ROWS on Vulkan0, was 50/60 + 20/24.
- `b22cf30bc` lane-145 checker finding 7: delete block_rocmfpx_fp6_packed16 + its A_TYPE_PACKED16 define (26B block has no valid uint16 view; nothing reads it). Re-verified 60/60 MUL_MAT, 24/24 GET_ROWS, 12/12 MUL_MAT_ID.
- `9645be4b4` test-backend-ops: cover the CPY types supports_op names that all_types omits
- `259c90e5a` lane-148 defect A ROOT FIX: Vulkan turbo3 centroid LUT was stale at 5 sites - CPY turbo3->f32 ERR 8.4302e-5 -> OK. CPU/CUDA/Metal/SYCL all carry CENTROIDS_3BIT (-0.190207,-0.118786,-0.066822,-0.021663); Vulkan alone carried a table whose midpoints do not match nearest_centroid_3bit(). copy_to_quant.comp's TM decision boundaries were derived from the stale table and are corrected too.
- `8ef93b515` lane-148 defect B ROOT FIX: FA_TYPE_TURBO2/3/4_0 GLSL spec-constant ids were stale (43/44/47) while enum ggml_type puts them at 200/201/202. Host passes the raw enum, so both switches fell through: fa_block_elems() returned its 1u default and dequantize4() returned vec4(0), zeroing V and the whole FA output. FLASH_ATTN_EXT -p turbo 0/1728 -> 1728/1728.
- `43d13a0ef` lane-148 F-69 sibling coverage: test-backend-ops FLASH_ATTN_EXT now covers TURBO2_0 too. Vulkan supports_op has always claimed turbo2 for FA but no test case existed, so its stale FA_TYPE id was invisible. 1728 -> 2592 cases, all PASS.
- `2ea7d28da` lane-148 checker findings 4+5 FIXED: turbo4 SET_ROWS was 0/21 declined behind a green 'Backend Vulkan0: OK' banner and turbo2 had no SET_ROWS case at all. Root cause: no cpy_turbo{2,4}_0_f32 copy-from-quant shader, so the tests' read-back leg declined. Added turbo2/turbo4 dequantize()/dequantize4()/get_dm(), the two spv+pipeline registrations, both supports_op and the DISPATCH switch (a third hand-synced pair - supports_op alone aborts with 'Missing CPY op'), and a test_set_rows_turbo2 class. SET_ROWS_TURBO2/3/4 now 21/21 EXECUTED each.
- `9554d9710` lane-149 F-110 guard (rung b): arifi_sync_check.py + CMake wiring + 16 ARIFI-SYNC markers. Checks 49 LUT pairs, 16 FA_TYPE ids, 35 QUANT_K block sizes, 4 type-list SETs, FA K/V coverage; unregistered mirrors FAIL
- `ac2a3f565` lane-149 checker-driven hardening of arifi-sync-check: FA_TYPE/QUANT_K literal spellings, ordinal (not line) pins, per-definition completeness, any array type, recursive shader walk, scalar-mirror class, expected-count assertions
- `6e8148b4d` lane-149 F-112 second half (checker finding 11): per-op tallies name every op whose cases were ALL declined, so a zero-coverage family cannot hide inside a green full sweep
- `40e796cd9` lane-150 step 1: TQ4_1S read-back leg wired - CPY(tq4_1s,f32) now EXECUTES, 291/291 -> 293/293
- `dd0d54208` lane-150 step 2: TQ4_1S write leg EXECUTES - SET_ROWS_TQ4_1S 17 cases/0 executed -> 17/17 PASS, and the inherited 5.0 waive is retired
- `9911ffe9e` lane-150 step 3 (root fix for the hole that hid the whole class): per-op x TYPE zero-coverage tally
- `f3909b07a` lane-150 checker-driven fixes: type_bucket prefix-boundary bug + dispatch block-size assert + the tallys limits named in code
- `a76cc84ed` lane-151 GOAL 6 hygiene: drop 648 tracked _lane-out debris files (incl. an embedded shadow.git with commit receipts) and gitignore the dir; genericize estate paths in docs/, licenses/, tools/gguf-retag-tq3
- `61f1a6a6e` lane-151 GOAL 9: reserve tq3's three destination-less serialized ids (37 TQ3_4SV -> 52, 31 TQ3_1S_AP1 -> 53, 44 TQ3_1S -> 54) as RESERVED-not-implemented in docs/TYPE-ID-ALLOCATION.md 3.1.2; geometry marked NOT DERIVED, no enumerators added, retag-tool refusals unchanged
- `84b5f5700` lane-151 GOALs 6+7: README refreshed to b10453 canonical arifi-main + the seven-ingested-sources table (two honest zeroes named); tq3 and ciru credits added; licenses/README.md gains the full Taken-from audit incl. the tq3 license GAP; UPDATE-RUNBOOK.md written (O(delta) recipe, THREE-flag configure, guards, the standard)
- `359b4d2f8` lane-151 GOAL 2: sources.json base.ref -> arifi/main (the canonical branch is now the series base) + series regen over the full canonical range: 323 patches, series check PASS, replay onto 4df29be4f identical outside patches/series
- `cc5b7dd2c` lane-151 GOAL 5: gitignore .tokensave/ - the fork repo now carries its own code graph (63344 nodes, 154451 edges over 3407 files) and the index is per-machine, never committed
- `891c58f0d` lane-151 CHECKER ROOT FIX: series check now asserts every patch is a COMMITTED BLOB, not just a file on disk. The byte-compare ran against the working directory, so untracked patches made it green while a fresh clone would die at git am - the defect that hid 33 missing patches twice. Help text corrected to match what the code does
- `3f9a7608c` lane-151 CHECKER FIXES (docs): UPDATE-RUNBOOK 4.5 no longer asserts a self-containment standard nothing on this estate meets - the four MinGW runtime DLLs are named and staged, and the measured stripped-PATH failure is recorded as identical for the raw build tree and the previous engine. licenses/README.md audit header stops claiming a purely mechanical trailer derivation: the tq3 row is a SUBJECT-line citation git never parses as a trailer, which is exactly why a trailer-only sweep would have missed the one real license gap
- `1de9526c5` lane-151 HQ DIRECTIVE: RETAIN the licenses instead of flagging them. tq3-MIT, prisml-MIT and ciru-MIT extracted verbatim from this repo's OWN fetched remotes (git show refs/remotes/<r>/<head>:LICENSE). REFUTES my own GOAL-6 finding and a year-old one: I wrote 'no clone remains on the estate' and Phase 0 wrote 'PrismML has no LICENSE' - both were absence claims checked against the wrong artifact while the remotes sat fetched in this very repository. All seven registered sources audited at once. thecodacus STAYS open: no remote, no text, nothing invented
- `14d598653` licenses: thecodacus-MIT retained from the public GitHub fork - the last open attribution CLOSED (no local remote != no source; President's correction 2026-08-18)
- `b6a4231ce` lane-152 GOAL 1: upstream bump b10453 -> b10481, the FIRST live execution of UPDATE-RUNBOOK 2. 348 patches replayed onto 25ae3a9b3, 4 conflicts resolved by hand, series regenerated to 329, zero merges.
- `3959ac94a` lane-152: what the FIRST live run of UPDATE-RUNBOOK section 2 corrected - one build break, two arifi_sync defects, the runbook itself, and the last license row
- `4f63c2198` lane-152 GOAL 2 batch 1: ciru's Vulkan SPIRV-Headers fallback, tq3's f32 hybrid-SSM state gates, rocmfpx's -ffast-math guard
- `7ad1fae5a` lane-152 GOAL 2 batch 2: three lfm2 pre-tokenizer hashes that only rocmfpx had - and the reason they must be pre-computed
- `fd64ae867` lane-152 pins: THREE advance with reviews (upstream, rocmfpx, tq3), TWO deliberately stay BEHIND (prisml, ciru)
- `568b35126` lane-152 checker F7: the old base is tag b10454, not b10453 - correction of record
- `9306ba0fb` repo-env.json (WI-1540 tail): the engine repo's env manifest - PowerShell-only law (F-107), three-flag configure (F-111), cmd.exe redirection (F-108), type-id FLAG E, series discipline, runbook pointer
- `52b068a53` WI-1580 sub-2: UPDATE-RUNBOOK section 7 - the 4.5 line was stale. lane-153 staged models/engines/arifi-b10481-union/ from a verified build of canonical, smoked from the staged copies, and flipped current-state.json + artifact-catalog.json, so 4.5 HAS now been executed once, by hand. The bump distinction is KEPT because it is still true: no bump has ever driven staging end to end (lane-152's bump refused at leg 2). Narrow edit - the section-2 and section-3 paragraphs around it are untouched. Note for the record: the lane-153 REPORT cites this text at UPDATE-RUNBOOK.md:238-244; it actually lives at :308-323
- `a98e992d2` graph: derive the V-unpad head count from the attention output tensor, not from hparams
- `edf49e037` sources: advance the turboquant pin f6124e914 -> 7ebcbb0b6 with pin_review_2026_08_18_lane154
- `baabb42eb` off-rig backends: land the 12 CUDA/Metal/SYCL/HIP rows unbuilt, per the HQ ruling of 2026-08-18
- `547cfecf6` sources: pin_review names the 12 off-rig shas and their BUILD-UNPROVEN status (HQ condition 3)
- `12bba67d5` sources: correct the pin_review - the contested 12/12 apply-conflict count is no longer the justification (checker F1)
- `1481633fb` sources: pin_review records the apply-count claim as WRONG, not disputed (checker F1, resolved against me)
- `4155f071f` base: b10481/25ae3a9b3 -> b10488/9d77fa172 + series regen 346 (lane-156 upstream bump)
- `118a60223` moe-cache: heat-protected eviction keeps hot experts resident (turboquant tail)
- `aa564eee3` tests: initialise non-contiguous tensors row by row (turboquant row f58ee0e97)
- `a9aee5843` sources: advance the turboquant pin 7ebcbb0b6 -> d14e36827 with a dated lane-156 pin_review
- `7f32b975b` sources: prisml pin HELD deliberately with a sharper reason + the v6 K-cache-mean-center verdict (lane-156 GOAL 4)
- `fc388cf2c` sources: ciru re-derived 315 -> 15 and HELD with all 15 reviewed (lane-156 GOAL 5)
- `392ae16a9` sources: advance remotes.upstream.pin 25ae3a9b3 -> 9d77fa172 (b10488) with its review
- `42cf661b3` sources: correct the turboquant pin_review's FALSE blocker for e130aef60 (checker F8, BLOCKING)
- `d6fbafdd7` sources: trim the e130aef60 blocker to the ONE item that survives (checker pass 2, advisory 1)
- `865814017` feat(kv-cache): port flag-gated mean centering
- `cdd7270be` feat(kv-cache): bind calibration to exact model
- `421e9dc97` fix(server): keep target calibration out of draft contexts
- `4436772c6` test(kv-cache): add Vulkan tensor probe and bench-pure A/B harness
- `94e8bfbde` sources: record lane-158 PrismML K-cache disposition
- `1259c9ec0` test(kv-cache): fail-close default-OFF binary identity
- `d88c0798f` test(kv-cache): assert both rotation-basis polarities against the resolved attn_rot_k
- `b22bc53f7` fix(kv-cache): make the A/B harness able to launch, and actually enforce the RAM floor it claims
- `7acf51c63` fix(kv-cache): floor-check the model named by -m, not the largest .gguf on the command line
- `bc0261903` fix(kv-cache): wait for the RAM floor between arms, and pin -fit off so the A/B arms match
- `e144e5df0` fix(kv-cache): probe the layer ids the calibration actually has, and let an existing calibration be reused
- `bf1e474bc` support DFlash2
- `82bc66c5f` dflash2: build fixes on the PR #27342 pick - dedupe DFLASH_BLOCK_SIZE enum/hparam/kv-map (fork DFlash1 originals keep authority, default 16) + ml. reference fix (the shadow the fork's own comment documents)
- `bdb9cc1c5` sources: register buun (spiritbuun/buun-llama-cpp) as a greedy-ingestion watch source - President ruling, VBR/KV + DFlash2-for-3.8 lineage
- `22f812d66` cuda: lift TQ3_4S kernel set from tq3/master (14 files)
- `5e675284d` metal: lift TQ3_4S kernel set from tq3/master (5 files + alloc hook)
- `d6725a4fa` metal: declare the tq3_rht pipeline getter in ggml-metal-device.h
- `0eb8604f1` sync: move base b10488 -> b10524, regenerate series (372 patches)
- `d83108b2b` spec: ingest turboquant MTP-boost wave + effective-KV bench reporting
- `0d2e5e3b6` sync: re-pin wave-3 sources + regenerate series (374 patches)
- `b6bf5fb31` arifi-sync: provenance native-class root fix - sha-pinned grandfather ledger + native Origin convention
- `ede218289` escha: native Escha-W2 types (ESCHA2=55/ESCHA3=56) + fused GGML_OP_ESCHA_MM, CPU + Vulkan
- `2a03445f4` escha: F-125 fix - full 64-bit pair sourcing + row-parity correction (K=2 AND K=3)
- `2f1b6f34f` escha: test-backend-ops — un-nest ESCHA_MM eval cases + multi-chunk ncols coverage + perf ncols curve
- `9cfd7e246` escha vulkan: bound every escha_mm dispatch to 32 columns (F-124 host-freeze guard)
- `835a6717c` escha vulkan: column-blocked escha_mm - one weight decode serves 4 columns (part 2)
- `29a2be66d` escha vulkan: column block C=8 + fall back to the one-column kernel below a full block
- `366e6fa09` escha tests: cover the column-blocked kernel AT MODEL DEPTH (ncols=9, real 27B shapes)
- `e73bcc519` escha vulkan: one hardware f32->f16 convert instead of the 15-instruction software RTE
- `f3195d818` escha vulkan: subgroup-shuffle Hadamard - 14 barriers per input block become 3
- `41ebd38ae` escha vulkan: the hardware f32->f16 convert must be float16_t, NOT packHalf2x16 (RTZ)
- `66493e606` escha vulkan: ESCHA_SG_HADAMARD defaults OFF - the lever is refuted by measurement
- `1ea7ec310` escha vulkan: f16-native generator - decode +18%, bit-exact
- `29e47ad01` escha vulkan: two more levers tried and REFUTED by measurement - occupancy and packed-pair
- `04ea349dd` escha vulkan: the multi-column threshold was disabling speculation - decode 3.96 -> 5.60 t/s
- `165c38e38` escha vulkan: column ladder C2/C4/C8/C16, and the generator table measured and REFUTED
- `b0d7e5307` dflash: ingest buun's DFlash2 adaptive controller - built, gated, and defaulted OFF by measurement
- `134e2448b` escha vulkan: prefill is OCCUPANCY-bound - coopmat and wider rungs both refuted, and the profile that said otherwise was lying
- `992fe54f7` escha vulkan: f16 activation staging - built, correct, and refuted; the real prefill win was the build
- `c3af00159` dflash: reject an out-of-extent draft depth instead of silently clamping it, and block the mask token
- `5a3fb20b2` spec: load the draft model from the same variable the log prints, and add an env-gated DFlash verify trace
- `cc1a2741c` spec: ON-DEVICE speculative checkpoints - drafter now BEATS plain (8.02 vs 4.5 t/s, byte-clean)
- `f75ce7aad` spec: ring rollback re-enabled - DFlash2 depth 3 = 8.80 t/s (2.0x plain), 0 degenerate
- `bb34f2996` tests: zero_gather + CPY/GET_ROWS Vulkan probe cases; server: depth>cap clamps with warning instead of load abort (F-136 arc)
- `9196ee9ce` rs zeroing hunt CLOSED: leak is backend-independent (CPU repro), in-graph rs_z zero never lands on the live cache tensor; zero-on-clear promoted belt->contract. Env-gated discriminators: PDH hash, plane0/sl/rl belt scoping, zero-gather-out, hybrid set_input trace
- `fe5e1e98f` arifi-sync: base move b10524/9ee9fc04c -> b10636/4d19b2876 (runbook 2.2 manual path); upstream pin advanced as the same pair; prisml + tq3 pins HELD with 3 reviews recorded
- `1d15ee76c` b10636 bump repair: restore upstream's DOTS3NOTE indexer arch + the two TAG_LLAMA_SEQ_ID_NEG TODOs that a whole-file --theirs resolution reverted. checkout --theirs takes stage 3 (the WHOLE file), not just the conflicting hunks - upstream's 4-line b10524..b10636 delta to this file was lost with it. GLM_DSA is absent here PRE-EXISTING (also absent on lane165-pre-bump-arifi-main), so it is NOT restored by this commit and stays a President item.
- `4d644b7d9` b10636 bump repair 2: close llm_graph_input_attn_k_dsa_iswa::can_reuse - the union resolution at ef642481d kept both sides' bodies but only one shared 'return res; }' tail, nesting every following definition inside it (build FAILED, 18 errors)
- `583e803bf` dflash: a rejected draft checkpoint image must not kill the server process
- `dc2fd361d` seat-46: MSVC portability root-fixes for the ROCm/HIP build (6 latent bugs: -fPIC on Windows, _MSC_VER fp16 macro parens, math.h/unistd.h guards, ssize_t->streamsize, dllimport on static lib) - build-rocm GREEN gfx1103, Vulkan build confirmed no-op
- `766ca387d` server: re-add the shared_draft_devices VRAM accounting on top of common_fit_extra_model (multi-device only)
- `97b5c4f76` arifi-sync: base move b10636/4d19b2876 -> b10680/d7bd3bfca (LATEST upstream tag, President mandate 2026-08-29); upstream pin advanced as one pair
- `75c4d67f9` vulkan sync-check: follow the FA_TYPE defines into b10680's new fa_types.glsl - reading only flash_attn_base.glsl found ZERO ids and silently turned checks 2 and 5 into no-ops (the exact F-110 class the guard exists for); now 16 FA_TYPE ids + 12 FA K/V types again
- `3ecc017f1` b10680 bump repair: common/fit.cpp still called llm_ffn_exps_block_regex(idx), which b10680 renamed to llm_ffn_block_regex(idx, ffn_regex) - the fork's moe_cache code auto-merged past the rename and the build failed. Same body, LLM_FFN_EXPS_REGEX passed explicitly, so behaviour is identical.
- `24750cb53` b10680 bump repair 2: my hunk-1 union in the 3aa479a04 (support DFlash2) resolution kept selector_top_k from BOTH sides - duplicate member, build FAILED. One copy kept. The union trap the runbook already names, this time on a declaration rather than a shared block terminator.
- `75212ca5a` runbook 4.1b: COLLISION MAP law (President 2026-08-29) - never lose our patches; A/B only where upstream delta intersects our changes; collision list = required bump-report line
- `693f9ee39` attribution gap-fix (seat-48): re-key the sha-dead provenance record + close the license/source-inventory gaps
- `f24b83d8a` runbook 2.2.1: RE-KEY the governed sha pointers after every base move - mechanize G1 so the class cannot silently return
- `d55298b75` attribution wave 2 (seat-48): zuijdwijk is a LIVE take source, not a dormant remote - promote it, and record the G7 trailer debt instead of amending
- `31bb9e21f` G12 re-audit (seat-48): 52 of the 180 are PROVEN not fork-authored - and none of them is unlicensed
- `43705bf3d` R1: the bank plane is a RUNTIME INDEX TENSOR, so the copies go AND graph reuse stays
- `369f2188c` R1 fix: guard the bank index tensors on ALLOCATION, not on rs_r1
- `ed7de13c2` lane-178: the ring's extra graph split is the SPLIT-INPUTS CAP, not a CPU fallback - runtime-selectable so one binary is both arms
- `4b7379f86` ggml-backend : adopt split-inputs cap 64 as the default
- `243bba4b7` lane-180: GATE 0 receipt - report eHostVisible in the vk memory logger, not just eDeviceLocal
- `1ef4b5a9b` lane-180 ADOPT: share ONE s_wrow view by default - 29 identical per-layer split inputs collapsed
- `2eb23e176` lane-188: SHIPPING KV precision tail (store-side) - quantized body + exact F16 ring
- `6d2a5ced5` lane-188: refuse the tail on MLA caches - the write-order detector cannot see the mla/lid idxs paths
- `45d9a8bae` lane-192 FIX (a): the precision tail survives a prefix-cache rewind
- `9bc2d1503` lane-194: q4_0 -> F16 Vulkan cpy shader + F16 KV tail compose (OPT-IN, default OFF)
- `32a781fac` lane-194: the CPU reference for quant -> F16 dup, which did not exist
- `e94314d34` lane-194: name the compose type in the tail banner - the receipt that stops a vacuous fidelity green
- `af5b7753c` lane-195: multi-segment flash attention - the ggml API + CPU reference + its gate
- `35785f324` lane-195: LEG 0 - the regression leg the branch was missing, plus the sinks condition stated
- `b160572e4` lane-196: Design B BUILT - heterogeneous split-k FA (per-query source selection), the Vulkan dispatch
- `bae77d0a2` lane-196: promote the tail-ON banner to WARN - llama-server drops INFO at default verbosity
- `0575ab003` lane-197: honor --spec-draft-p-min in the DFlash2 sampled selector path - the gate was greedy-branch-only, so the seated serve line (temp 0.7) ignored the flag entirely; hoisted to the shared site, greedy duplicate removed, flag-unset path untouched (p_min defaults 0.0)
- `28b56a336` lane-198: gate Q5_K (n>1) and Q4_K (n>=5) off the q8_1 MMVQ mat-vec shader on AMD - measured on the live speculative-verify graph (780M, proprietary driver): MMVQ per-dispatch cost steps 2.0-2.3x (Q4_K) / 1.55x (Q5_K) from NUM_COLS 4 to 5 while the f32 dequant shader rises 1.4-1.6x, and Q5_K is faster on the f32 shader at every measured n>1 (0.73-0.88x). GGML_VK_FORCE_MMVQ / GGML_VK_DISABLE_MMVQ still override; n==1 path untouched. Evidence: research/local-inference/lane-evidence/2026-09-01-lane-198/TABLE.json
- `54e25c113` runbook 4.1b duty 3: a named collision is decided by MECHANISM READ + THREE ARMS (theirs / ours / union), never by one number - President 2026-09-02
- `9ca43057e` runbook 4.1: warn that `series regen` is destructive-then-abort, and require a pre-landing baseline
- `c259de148` arifi-sync: root-fix the destructive series regen, and mechanize protected-win preservation
- `d51df1750` arifi-sync: name the incoming ref in full when the protected-win preflight refuses
- `00281ea43` arifi-sync: make an UNREGISTERED measured win refuse the update path
- `3d174e706` arifi-sync: close the two live false negatives in the protected-win gate, and mechanize the baseline boundary
- `974cc8ecd` arifi-sync: discharge eleven REGISTRATION OWED rows, and close two live holes in the guard itself
- `0639393e5` arifi-sync: search anchors in SOURCE only, so the manifest cannot vouch for itself
- `fe323a578` arifi-sync: bring the manifest's own `_schema.anchors` fact up to the source-only rule
- `d8b9904bd` arifi-sync: recognize `correctness only` as no-effect vocabulary
- `88413c9bf` speculative: size drafts from measured acceptance (--spec-draft-adaptive)
- `c3e065b4f` server: do not re-verify replayed draft tokens after a checkpoint restore
- `d29fcfe5f` arifi-sync: register the three lane-209 mechanisms this integration lands
- `64b0e3c35` test-backend-ops: adversarial MUL_MAT data patterns for the ROCmFP4 MMVQ widths
- `5f3d0de93` arifi-sync: register the rank-7 ROCmFP4 MMVQ mechanism, default-OFF and all
- `d068fcf51` arifi-sync: base move b10680/d7bd3bfca -> b10819/6a1a922d2, and the governed sha re-key that follows it
- `cd5ce0aa7` b10819 sync: repair six merge defects that stopped the tree from building
- `42ce1f0e9` metal: union the dropped upstream FA machinery back in - the tree did not compile
- `ffeb40523` cuda: union upstream's mul_mat_id_needs_sync predicate with our TQ arm
- `ebc3629cb` arifi-sync: restore the 2-space JSON indent the R17 register edits reformatted
- `cccea939e` cuda: restore the fused MoE-cache matvec on upstream's fusion kernel (WI-1699)
- `58c15eb90` arifi-sync: R13B's two b10819 repair commits become also_commits of the mechanisms they keep alive
- `14de968ab` sycl: the MMVQ launchers declared one pair of ranges and launched with another
- `de85e39cb` metal: replace the garbled library loader with upstream's per-kind loader plus an ArifiLabs library kind
- `79125644b` arifi-sync: protected-win learns a symbols class - a registered kernel entry point must have a DEFINITION, not a mention (WI-1700)
- `d2d95ed37` arifi-sync: the symbols guard ignores commented-out bodies, and the CUDA/Metal seed gaps the check found are closed (WI-1700 FIX-1/FIX-2)
- `dc3bc57b5` arifi-sync: sources.json post_move_tip re-keyed to the lane-214 integration tip
- `62014225f` ggml: flash-attention segment count moves to op_params[5] - it collided with upstream's n_kv_max in [4]
- `f83b62b29` arifi-sync: base move b10819/6a1a922d2 -> b10825/9e0e22059 (R22 read, R27 execution); 552/552 replayed clean, 0 merges, 0 hunk collisions, marker scan 0, protected wins PASS; R20 slot fix + R19 gate (default ON per R19D) carried Lane: lane-220
- `1a6925977` arifi-sync: rekey 3 protected-win manifests to the b10825 line (R27 check 05:1x: validate PASS only with this rekey; committed so the built line validates) Lane: lane-220
- `ce5d4046e` arifi-sync: scope subcommand (F-161) - the ingest A/B scope derived from the collision map (upstream files x our files x hunk overlap x speed paths) = sanity|named|full; UPDATE-RUNBOOK 4.1b.4 scope law; selfcheck Lane: lane-220
- `c9075b246` provenance route B (G7 append-only): R24's 58 classifications recorded in native-grandfather (55 ARIFI, remapped to the b10825 shas) + pending-trailers (2 FORK, 1 UPSTREAM); 46 fork-authored tooling/manifest commits grandfathered; provenance PASS; series regen 496 patches
- `4fea45e7e` spec: runtime switch for the DFlash encoder fusion (LLAMA_DFLASH_FUSED_INJECT), both injection paths kept, default LEGACY on this fork
- `7788c6102` kv-cache: the PLE n-gram history lookup becomes a runtime switch - seq_pos index by default, the cell scan behind LLAMA_KV_NGRAM_INDEX=0
- `cb36687a0` ggml: add GGML_TYPE_SX8 (57), the S-X8 v4.3 CPU decoder, retagged from the author's 41, plus its retag/cross-check tooling
- `e832c80a9` ggml: port jtrefon TBQ3_0/TBQ4_0 at ids 58/59 with the type-id arbitration, plus the SET_ROWS device guard (WI-1694 half A)
- `070c6c71d` docs + protected-wins: the R18 switch table - eight DOCUMENTED-TAKE runtime switches registered with their measured effect
- `77f825fd8` vulkan : add the S-X8 v4.3 kernel for GGML_TYPE_SX8 (57)
- `40347768b` vulkan : integer MMQ (Q8_1 activations) for GGML_TYPE_SX8 (57) - matmul_sx8_q8_1, CREATE_MMQ, per-8 activation sums hoisted into block_b_to_registers (8 dots + 8 FMAs per 32 weights)
- `4cfca125b` vulkan : mul_mm (prompt processing) for GGML_TYPE_SX8 (57) - the device said the gap is the missing mul_mm, not the missing MMQ
- `16f8ff2de` server : device-resident context checkpoint ring + tool-call anchor (R31 P1/P2, FreeToken M14/M15)
- `f85706ebb` llama : Windows unbuffered model reads (--load-mode direct_io) + PR #28223 host bufts under mmap (R31 M17 / M07)
- `89737a7db` tests: test-save-load-state Test 9 (device storage ring) - the first draft read logits the ring never stores and went RED on the ring code itself; the repaired test checks the restored state, GREEN on cdff32828 (R31 checker F1)
- `0621d6a4e` server/llama : device checkpoint storage-ID ownership + quantized device-view extent (R45, lane-229)
- `f61eb424d` server : LLAMA_CKPT_STORAGE_TRACE - one INFO line per checkpoint save and restore (R45, lane-229)
- `348f38ca2` test-backend-ops: R46 real 27B S-X8 decode shapes (17408x5120, 5120x17408, 248320x5120) for sx8/q8_0/q4_K at n=1..8
- `2f46d74ae` vulkan : GGML_VK_ALLOC_TRACE allocation/submit instrument + S-X8 mat-vec rows/workgroup switch (R47a, lane-232)
- `cbcbd447c` vulkan : alloc-trace reads per-heap live bytes under the lock (R47a, lane-232)
- `3181f30b8` test-backend-ops: S-X8 ragged-m, K-tail, f16-activation and real-27B-shape mat-vec correctness (R46b, checker F2)
- `f08dd0a9a` test-backend-ops: drop the S-X8 m=1 correctness cases, they are not comparable (R46b commit G)
- `11a85ec23` test(vulkan): add independent S-X8 route checks
- `8aa5ef3cf` feat(vulkan): account heap reservations transactionally
- `635df85aa` feat(backend): preflight Vulkan buffer-type batches
- `328385ebb` feat(alloc): loader-wide Vulkan allocation transaction (R46b B7b)
- `88b188b31` feat(vulkan): bounded host split with explicit staging reserve (R46b B7c)
- `f56b7451a` fix(checkpoint): make device finalization fallible
- `a0c408f8f` feat(updater): preserve fork work across rebases
- `3d64ac823` arifi-sync: regenerate the patch series at the R46i tip; grandfather the nine untrailered R46i-line commits
- `bf2ce0742` gitattributes: evidence receipt files are byte-exact (-text)
- `115f4e5a4` arifi-sync: grandfather the untrailered series-regen commit 833881db4
- `58de85ad0` arifi-sync: series replay applies with core.autocrlf=false so replayed bytes equal the committed blobs
- `42de753fe` arifi-sync: series replay keeps CR (--keep-cr); evidence receipts restored to their original LF bytes
- `c2b438cfa` feat(vulkan): MMVQ A-side decode hoist behind spec constant (R48b)
- `98cd65e48` docs(r48b): report section 10 (reconstruction) + rc receipts
- `42375fb51` docs(r48b): HQ planted-red receipts on the committed hoist (RED-1 both arms n=1..4, RED-2 ON-only n=2..4)
- `993ad050a` WIP(r48b phase 2): hoist type/width gate + MMVQ routing draft (maker output, unreviewed, unmeasured)
- `21b68eb74` docs(r48b): phase 2 gate + routing verified, correctness receipts, report section 11
- `5b9f31eec` WIP(r48c): Q6_K mat-vec x-fold + activation hoist draft (maker output, unreviewed, unmeasured)
- `d1b8aebaf` docs(r48c): clear the Q6_K shader of HQ finding 1; localise a pre-existing defect
- `1fe947413` docs(r48c): independent Fable LOW check PASS-GATED + its three receipts (upstream-path 3/60, S-X8 0/60, direct-scales arms 43/43)
- `ea298363e` docs(r48c): close the Fable LOW gates - measured registry rows, corrected coverage and failure-signature claims
- `8e7ac9e94` feat(vulkan): Q5_K mat-vec activation hoist behind spec constant (R48 phase 1)
- `40a526a19` docs(r48): Q5_K report section 10 (reconstruction) + HQ planted-red summary and rc receipts
- `3bd085b08` feat(vulkan): gate Q5_K mat-vec activation hoist to NUM_COLS<=3 (R48 phase 1b)
- `d43f74f7a` docs(r48): name the phase-1b commit SHA and downgrade the f16 gate claim
- `242a44a3b` arifi-sync: series replay checks out its worktree with autocrlf=false (index LF vs worktree CRLF killed patch 283/550)
- `2d5b6b2c9` vulkan : R52b — S-X8 mul_mm PACKED tile decode, and integer MMQ reachable under coopmat (opt-in)
- `1ff06a694` docs(r52b): the three-arm diagnostic — MMQ under coopmat LOSES, the packed decode wins 2.2%, and S-X8 mul_mm already beats q8_0 at this shape
- `50a9d4916` fix(r52b): perf-mode supersedes the single-dispatch numbers — MMQ under coopmat WINS for q8_0 and loses for S-X8, and the packed decode is a TIE
- `b3233140c` docs(r52b): correct the perf case-list line citation (12042-12049)
- `7d0c7ce27` docs(r52b): close the PASS-GATED docs gate — the coverage claim was overstated and the build log was never captured
- `89539e2ac` docs(r52b): re-point the two test-case-list citations that this lane's own comment growth shifted
- `8d8c8ca89` feat(sx8): recover the S-X8 v4.3 PCA correction that GGUF conversion drops (lane-242/R53)
- `112470efb` fix(sx8): close the R53 check gate -- same-run verifier receipt + companion identity guard (lane-242)
- `c43f09c9c` WIP r51: host-split order mechanism + plan-order trace; token_embd proven absent from the Vulkan plan
- `c0f6d3c08` feat(vulkan): choose the host-split memory type by measurement, not by index order
- `7d6e94b9f` docs(r58): name the regime and the receipt-line assertion the reorder needs
- `f4a0d6352` test(r58): phase B proof cells - the reorder runs, the output does not move
- `0232ea0c0` series: regen at the R56 tip (565 entries) + G7 rows for two untrailered lane commits
- `cddff4b48` test(lane-246 R60): paired S-X8/q8_0 mul_mm coverage across the small-n band, and the two refutations it produced
- `c3ed6236b` feat(vulkan): make the mat-vec admit width runtime-selectable, default inert
- `62a2e4531` test(r57): three k, two effects - the width-4 knee moves, the width-7 cliff does not
- `ac525a678` docs(r57): registry rows for the admit switch and for the n=7 cliff it uncovered
- `6064d93cc` test(r57): the decider answers - past the mat-vec boundary a verify column is ~46x cheaper
- `636558ae8` fix(vulkan): restore the admit-width receipt line the base check discarded
- `49961fdcd` docs(r57): fold the Fable check's corrections into the report and the registry
- `b9d0b4de8` feat(r64): GGML_ARIFI_Q5K_MMVQ lever + the two-hoists find + step-0(c) counts
- `b93613485` fix(r64): the q5_K admit is n>=5, not n=2..8 - the measurement narrowed it
- `d0edc90a9` feat(r64): the q5_K MMVQ route lands - proof, null control, registry rows
- `a0dd546da` fix(r64): withdraw the "repeatable at every width" claim on the k=17408 arm
- `bf27706b0` docs(r64): Fable-check gates - corrected counts, paired cites, generators, served pairing
- `3a49c5eb9` feat(vulkan,r65): a rows-per-workgroup knob on the plain MMVQ path, and the rows the knee test is read off
- `df6e77e2e` test(r65): the width-4 knee is width-locked, not a capacity - REFUTATION
- `e0a94115b` test(r61): iq3_s/iq3_xxs/iq4_xs rows on the shapes the GSQ file serves, plus a mat-vec pipeline-identity trace
- `35058539c` feat(r61): hoist the iq3 sign application out of the NUM_COLS loop, behind GGML_ARIFI_IQ3_MMVQ
- `85e321ae9` fix(r61): the dump's same-arm control FAILED, and it caught a real defect in the method
- `2edf5cac0` docs(r61): three registry rows for the R61 step-2 variables
- `cd1e2d3ae` chore(r61): commit every raw receipt and the wrappers that actually produced them
- `b6e9accfc` feat(vulkan,lane-254 R66): three default flips - q5_K route on AMD, iq3 sign-hoist everywhere, iq3 n=7 rows on RDNA3
- `15ecccad4` vulkan q6_K: correct the R71b route-admission comment to the CHECK-R73 record
- `b5f6f00f9` vulkan q6_K: GGML_ARIFI_Q6K_F32_ROWS + R74 step 0 driver statistics + pre-registration
- `65906d3b9` vulkan q6_K: GGML_ARIFI_Q6K_MMVQ_ROWS, the q6_K-scoped integer-dot rows knob
- `cb25daea8` R74 report: the q6_K width cliffs are REFUTED at the served width, with the mechanism named
- `7aed5a952` R74b WIP (resumed after box freeze): exact-n q6_K MMVQ rows knob, RDNA3 1@n6 default, route n=6 (m,k) cells, registry rows
- `eaf82069c` vulkan q6_K: fence the R74b n=6 route cells to RDNA3 (off RDNA3 rm_int_n is already 1 row)
- `316f2006d` vulkan q6_K: ship the R74b n=6 MMVQ route cell 5120x6144 ON (RDNA3, rows 1 at NUM_COLS 6 exactly)
- `6d3275f85` vulkan q6_K: R74b startup route line names RDNA3 (text only, no behavior change) + final-binary re-green + R49 side-finding
- `791bfcbfb` WIP R85: q4_K w5 arms C1 (next-slice prefetch), C2 (rows=2), C3 (4+1 column split) behind default-off env switches; decider + pre-screen scripts
- `bc5b0b3af` R85: ship the C3 4+1 column split for q4_K MMVQ at the measured-winning cells (n=5,6; m>=131072 or m<=1024; k<=8192), device-probed ON on RDNA3, GGML_ARIFI_Q4K_W5_SPLIT=0|1
- `31dade044` R87: ship iq3_s NUM_COLS=6 mat-vec at 2 rows per workgroup (was 4), device-probed on RDNA3, GGML_ARIFI_IQ3S_N6_ROWS=1|2|4 overrides
- `8a233ac96` R87 correction: iq3_s n=6 rows knob narrowed to the f32-B pipeline; mechanism comment reworded
- `b8ca07de6` R75 (lane-262) WIP: iq4_xs q8_1 MMVQ shader arm + registrations inherited from the stopped Opus 5 maker (unreviewed, unbuilt, default unrouted)
- `6887522f7` tests: perf rows bs 1..9 at the served attention shapes (10240x5120, 6144x5120, 12288x5120 for q4_K/q5_K/q6_K/iq4_xs/iq3_s/iq3_xxs; 5120x6144, 1024x5120 for q5_K/iq4_xs/iq3_s/iq3_xxs, bs=9 only for q4_K/q6_K)
- `40562e795` R86 (lane-278) receipts part 1: linearize, RED x4, gate battery, re-take, stage + REVERIFY, PPL runner
- `de6d71376` R86 (lane-278) REPORT + receipts part 2: timed chain, depth table, analysis; PROMOTE-READY yes
- `e91d62c86` R86 (lane-278) report correction: pre-registered same-session marg is the number of record (UNRESOLVED), host-load audit, same-day r73i anchor
- `664b7289d` R86 (lane-278) report: summary trimmed under 300 words
- `8f451c9c9` OW-014 (lane-285): KV precision tail trusts ring entries by exact ownership, not the high-water mark
- `c82b7d2cf` W1 base-move fixup: heapres test block after upstream's relocated llama_build(test-backend-ops.cpp)
- `fc48b46ba` W1 base-move fixup: arifi_sync_check expected counts on b11178 (QUANT_K 37, marked lists 16)
- `8379d345f` W1 split fixup: fork buffer-layer symbols called across the split are extern (declared in ggml-vulkan-common.h)
- `02c73feeb` W1 base-move fixup: DFlash deferred drafting reads pos0; RS zero audit reads gf_res_prev_active
- `4e739f4e0` W1 base-move fixup: ARIFI-SYNC-SOLO on upstream's coopmat1 int-MMQ shmem probe (marked type lists 16 -> 17)
- `e52a5b5d2` arifi-sync: base move b10825/9e0e22059 -> b11178/f9af9be21 (R55 read, W1 execution); 688/688 fork commits replayed via staged milestones S1..S7, hand stops carry Conflict-resolved trailers
- `80e47f738` arifi-sync: re-key provenance ledgers after the b10825 -> b11178 move (UPDATE-RUNBOOK 2.2.1) + G7 rows for 14 untrailered commits
- `d525d6670` OW-028 (lane-279): ROCmFP4-FAST q8_1 MMVQ route DEFAULT ON for FP4-format files
- `9678f7da1` W1 split fixup: Vulkan test-fault seam atomics are inline, not static (heapres mid-plan failure RED -> one definition)
- `38f348fab` W1 collision fixup (N3): MMQ-under-coopmat opt-in leaves Q8_0 to upstream int8 coopmat1 on RDNA3/4
- `df9ecc1f7` W1 collision arms (N1): upstream IQ4_XS mat-vec shader, MMVQ body and route as runtime switches beside ours, default ours
- `59c65e0be` protected-wins: vulkan-uma-read-path-probe symbol re-homed to ggml-vulkan-buffers.cpp after the f172be756 split
- `aa6ebfce2` W1 collisions N11/N12: coopmat1 f16-B guard for f32-B-only types; int8 coopmat1 MMQ min-n switch
- `e0d733dde` W1 collisions N12: RDNA3 device-probe default for the int8 coopmat1 MMQ width switch
- `11eef8565` W1 collisions N12: RDNA3 f16-B width floor 24 for the coopmat1 f32->f16 B conversion
- `5432990f3` W1 collisions N12: RDNA3 small-n ffn_down int8 exception defaults OFF (served -ub 32: q4kxl 0.920x vs 0.977x, GSQ 0.928x vs 0.958x of r86i); GGML_ARIFI_CM1_INT_SMALLN=down restores it
- `9bf09374d` protected-wins: carry registrations across the W1 rebase and register the unregistered measured wins (Sol REJECT fix 3)
- `0f50ef68d` metal: restore upstream's (dk=96, dv=64) sparse-FA shape lost in the W1 re-home
- `4eed112ad` server: carry the tool-call anchor across upstream's same-n_tokens checkpoint supersede (W1 collision C077)
- `ba3683034` qwen35: chained MTP drafting works with fused-QKV GGUFs (W1 collision C071)
- `0d62bfd1e` arifi-sync: protected-win-resolutions.json for the 36 wins the b10825 to b11178 range touches (lane-296 Sol-fix item 3, runbook 4.1c)
- `16f0ae402` tests: V023 asserting regression check - SILU must stay fused with its MUL on Vulkan (lane-296 B3)
- `d43dc95e7` arifi-sync: protected-win-resolutions B1 rows measured (lane-296 run15/17/23): 12 daily-dense rows carry r86i/U8 numbers, vanilla where it runs; open op re-pair cells listed per row
- `450b82a5b` arifi-sync: B1 rows corrected (lane-296): 248320 head cell is in the perf list and owed (run24 d), iq3 N7 off arm = registered legacy cliff, concat dispatch count wording, owed flags + run24 scope
- `feabf1a0b` tests: SILU*MUL perf/eval cases at the 27B FFN width (n_ff 17408, tokens 1/5/512) + ARIFI_PRINT_ERR error print (lane-296 B2b receipt source)
- `2bd33c42b` lane-296 B1: run24 + stage h close in protected-win resolutions
- `39e8e1ac2` lane-296 B1: correct run24 order-effect sign, record kernel identity R vs U8
- `c597892ae` lane-296 B1: run25 close of OPEN 1 + OPEN 2 (protected-win resolutions)
- `d70b85455` lane-296 B1: OPEN 1 row - harness/sync/submit/pipeline identity CHECKED, n=6 off the registered GSQ depth-4 line
- `ef308e5d8` lane-296 B1: OPEN 1 not yet a documented loss (mechanism unnamed), depth-5 width mapping, OPEN 2 q4_K closed at line level
- `ff323b02f` night/sx8-numerics: revert GDN q/k norm to ggml_l2_norm (5fdfa62829, qwen35 only) - throwaway numerics probe
- `8305fcd90` Revert "night/sx8-numerics: revert GDN q/k norm to ggml_l2_norm (5fdfa62829, qwen35 only) - throwaway numerics probe"
- `fe49d8b17` lane-296 night: int8 accuracy deliverable - floor A passes 3/3, goal B (int8 ssm_out) open
- `9a3783ec7` lane-296 int8 goal: INT8-MATH derivation + op-level error tables, INT8-ACCURACY update (B5 accurate at op level, 7.431166 vs bar 7.4163; instrument paired SE 0.006-0.012)
- `abbda3401` lane-296 int8 goal: conclusions made conditional on the 64-chunk check; B1-B3 pair (+0.0209 PPL at 3.4 SE with equal ssm_out op error); section renumber; magic-bias note
- `6813ffc87` lane-296 int8 goal stage 8: 64-chunk gap is real (+2.7 SE, broad); H-B and H-A falsified; float-path error named as f16 accumulation (random, 7.7e-3 -> 2.66e-4 with f32 acc); chain s8 pre-registered
- `a1eaa94a3` lane-296 int8 goal stage 8: pre-register x16 pairing and the c64 x x16 joint reading before c64 lands; smoke downgraded (2 seqs/pass); S-X8 f32acc route checked by code; price disclosed
- `1691b758c` lane-296 int8 goal stage 9: F32ACC takes <type>:<role> and <type>:all; op probe replays any weight type and times widths
- `d871a4fae` sx8-int8 stage 10: S-X8 on the int8 coopmat1 MMQ kernel (exact (d,m) remap, exact b sums, RED twin, role/min-n knobs, tests)
- `3f5c70921` sx8-int8 stage 10: GGML_ARIFI_SX8_CM1_2D fix knob (two-digit activations for listed S-X8 roles, lane-296 R3 route)
- `e650ca8a8` sx8-int8 stage 10: two-digit activations default ON for ffn_up (fix B)
- `7e8380ca6` sx8-int8 attempt E: 16-value activation scales for the S-X8 int8 cm1 kernel (env-only arm)
- `b43fe423b` sx8-int8 attempt E: quantize_q8_1 HALF ds store via vec2 (f16 locals need the arithmetic extension)
- `fb251eed0` sx8-int8 attempt D: two-digit activations only for outlier blocks, skip kernel for the second pass
- `16884e572` sx8-int8: default to gate20 selective two-digit ffn_up activations
- `926e2931d` S-X8 gate comment: threshold is in dispatch columns, not request tokens
- `c23bd8cc5` server: keep prompt images in reusable device snapshots
- `b651d7f98` lane-298: skip whole-file mmap prefetch when a device copies weights out of the mapping
- `8608d59a3` lane-300 F-141: coopmat1 FA zeroes V rows masked for every valid tile row (stale/foreign KV content no longer reaches the P*V WMMA sum); flags bit 32 default ON, GGML_VK_FA_DEADV_KEEP=1 = planted-RED switch; fa-replay sign-class arms. Lane: lane-300
- `1cfa48571` lane-298 R1 checkpoint fix: ssm row save/restore maps to the physical ring plane
- `8a721b95d` lane-298: qwen4exp MTP sidecar load + graph_mtp, target 4-stream capture, Windows unmap_fragment trim (env-gated)
- `a3cdce01b` lane-298: fix Windows -lm dio probe failure (unaligned file-pointer moves on NO_BUFFERING handles)
- `30347e2b6` lane-298: unbuffered read_raw_unsafe throws on an unaligned cursor/len/ptr instead of returning floor-sector bytes
- `52566ff2c` lane-298: qwen4exp MTP capture is a real graph node (fixes GGML_ASSERT backend_h != nullptr)
- `1980c2b73` lane-298: server emits n_probs on the speculative path (verify-row index per emitted token)
- `c0c175642` lane-298: spec round-cost timers + env draft recent-token penalty (OW-036)
- `99b1bd597` lane-298: qwen4exp MTP draft head over a token subset (d2t + gathered output rows, Strata draft_vocab)
- `6350b8f7e` lane-298: MTP eh_proj as one 2D matmul
- `c1f0071d8` lane-298: OW-036 draft penalty default on, ngram-mod miss-cost cap, MoE-union counter
- `e3ec87d92` lane-298: ngram-mod CAP0 gate starts at full width, caps only after a miss
- `e8f260b80` lane-298 item 5: drop the previous ubatch PLE row pages from the working set (LLAMA_PLE_RELEASE=1)
- `8291ce4ed` lane-298 item 5 v2: PLE release = one whole-table VirtualUnlock per row budget, not per row
- `671a9594e` lane-298 item 5: PLE release default ON at 256 MiB; LLAMA_PLE_RELEASE=0 turns it off
- `1ea9394f5` lane-299 WIP: vulkan HC_POST_W fusion (SCALE->SIGMOID->SCALE->DSV4_HC_POST, one dispatch)
- `df91200f0` lane-299 WIP: GDN_BANK prepass reads SET_ROWS operands in ggml order (src0 data, src1 row ids, src2 dst); the old order never matched, so the fused path never fired (optest3 reject 2). Adds GGML_VK_GDN_BANK_DEBUG reject-stage prints.
- `300d9d691` lane-298 item 5 prefetch-ahead: one PrefetchVirtualMemory over each ubatch's PLE row pages (LLAMA_PLE_PREFETCH=1 opt-in)
- `e30a7e459` lane-298 item 5 prefetch-ahead: prefetch after the PLE trim, not before
- `e2eb456e4` lane-298 item 5 prefetch-ahead: LLAMA_PLE_PREFETCH default 1 (pfa-10051313 PASS)
- `4295f4827` lane-298 item 5 prefetch-ahead: keep the per-ubatch WARN receipt on the default path
- `4b1fb8b39` lane-298 GDN snapshot-byte floor diag: GGML_VK_GDN_SNAP1_DIAG clamps GDN_BANK n_written to 1
- `0d511dd76` lane-298 GDN replay-cost bounding diag: GGML_VK_GDN_REPLAY_DIAG runs a replay prologue (n_rep push constant) in the GDN_BANK shader
- `f5276ee24` lane-298 fix: two log format strings carried raw newlines instead of \n (cf4f877c07 build break)
- `d42ee555f` lane-298 GDN replay: verify routes an unlogged wide-batch partial reject to the draft-time checkpoint
- `634bbd673` lane-301: Vulkan FA sparse gather for QSA prefill (gqa layout for sparse-hinted multi-row FA) + prefill sparse op cases
- `7d61001f0` lane-301: qwen4exp row 25 - persistent pooled indexer-key cache (pool+norm+rope once per full block, incremental fresh blocks), GGML_ARIFI_QSA_POOL_CACHE=0 / GGML_ARIFI_QSA_POOL_RED=1; test-backend-ops CPU census perf shapes + GGML_ARIFI_TBO_THREADS
- `49c4e1891` lane-301: protected-win entry qwen4exp-qsa-pooled-indexer-cache (row 25, c4 logger-off x2: decode 32K 8.29->10.35 tok/s, ids identical)
- `c49a7c15e` lane-302: MUL_MAT_ID expert gather for small-batch mat-vec (opt-in GGML_ARIFI_MOE_GATHER=1|2|4)
- `ba68186d9` lane-302: gather leader test uses a mask (G is 1, 2 or 4), comment points at the real pipeline block
- `fe35f4fa7` lane-302: served qwen4exp MoE op-test shapes n=1..8 + gather plant guard
- `afbe861ef` lane-302: served MoE op-test cases at 16 experts only
- `57c45ac84` lane-302: gather pipelines walk slots fastest (slot-major workgroup order) so shared experts hit L2
- `6350bcf72` lane-302: protected-wins entry for the slot-major MoE gather (op-level win, default off; served A/B not shown) Lane: lane-302
- `e8bf3e812` lane-302: MoE gather default ON at width 1 (cellE-10052000 served ABBA x2: mean +0.34%, ids IDENT 48/48); GGML_ARIFI_MOE_GATHER=0 opts out; protected-wins entry amended with served fields
- `8c913a9d2` lane-302: GGML_ARIFI_MOE_GATHER_ROWS (1|2|4|8) sets rows per workgroup of the gather pipelines; unset = rm_iq (no default moves)
- `7c9a6bc21` lane-302: IQ3_S gather pipeline defaults to 2 rows per workgroup (cellG-10052319: n4 -10%/-13% vs rm_iq in both rounds, harness 80/80 byte-identical); GGML_ARIFI_MOE_GATHER_ROWS still overrides every type
- `35841fffa` lane-302: protected-wins entry for the IQ3_S gather rows-2 default (cellH-10060008 served ABBA x2: mean +0.07%, worst prompt -0.54%, ids IDENT = KEEP; op -10%/-13% cellG-10052319)
- `d5deb283b` lane-302: IQ4_NL gather K-tail redistribution, opt-in GGML_ARIFI_MOE_GATHER_TAIL=1 (=2 planted wrong arm)
- `3d333a096` lane-302: IQ4_NL q8_1 integer-dot mat-vec + expert-gather twin, opt-in GGML_ARIFI_IQ4NL_MMVQ=1 (every n)
- `ad9f46e05` lane-302: IQ2_S gather body arms (opt-in GGML_ARIFI_IQ2S_BODY) + ISA dump under GGML_VK_PIPELINE_STATS
- `ad0ef50aa` lane-302: IQ2_S gather batched grid init arms (opt-in GGML_ARIFI_IQ2S_BODY=4|5), body stays opt-in
- `88eeef9a0` lane-302: IQ2_S gather body 5 (int8 sign + batched grid init) DEFAULT ON
- `32f06513d` lane-302: protected-wins entry vulkan-moe-gather-iq2s-body5 (cellO-10060849 KEEP) Lane: lane-302
- `b1c60b4e9` llama-quant: same-type tensors are byte-copied, so skip the imatrix guard for them (lets --tensor-type-file pin quantized tensors to their current type while requantizing others)
- `16557e772` llama-quantize: read via slabbed file path on Windows, not mmap
- `edc570ed3` lane-299: protected-wins entry vulkan-qwen4exp-hc-post-w (measured ABBA +1.28%/+6.69%, ids exact) Lane: lane-299
- `936985888` lane-299: protected-wins entry vulkan-qwen4exp-gdn-bank (measured 3-round medians +4.47%/+3.09%, ids exact 12/12)
- `ea904964e` lane-299: gdn-bank protected-wins entry carries the interleaved_ab bootstrap CI (short SIGNIFICANT, long TIED at n=3)
- `9ef9ad5b7` lane-299: runbook for the qwen4exp decode-fusion gates (build, op test with MATCH>0 rule, interleaved served A/B, noise floor)
- `b61ddcec3` moe-cache-x1: GLM-5.3 NVMe expert streaming, x12..x21 measured source
- `f144818c1` kimi-k3: read the ssm bank through the R1 rotated index (bank=true)
- `0130f822a` lane-298 integ: protected-win registration fixes carried in from lanes 301/302 + HQ80 f2c5fad421 resolution rows
- `1d7f47ec4` lane-301: coopmat1 f16-B staging for MUL_MAT_ID on non-Intel devices, default on
- `44450ec46` lane-301: protected-win vulkan-cm1-f16b-staging-mul-mat-id + row-26 workload 780M -> Radeon 890M (X1)
- `48468238d` lane-302: IQ4_NL gather byte-pair LUT + IQ3_S gather batched grid init arms (opt-in)
- `70e4f2303` lane-302: IQ3_XXS gather batched grid init arm (opt-in GGML_ARIFI_IQ3XXS_INIT=1|2)
- `9ca84f74a` lane-300: arifi-det-probe - repeat-determinism probe with history arms (DETP_CLEAR, DETP_PRE)
- `9ac5c5b4a` lane-300: det-probe DETP_FA (FA mask live-column + dead K/V report) and DETP_ROWS (GOAL 3b row-independence)
- `f0701495e` lane-300: det-probe DETP_FA_DUMP (FA node inputs/output per run) + DETP_ROWS_OPS (per-op last-row compare by width); GGML_VK_FA_MASK_OPT_DISABLE diagnosis switch Lane: lane-300
- `565bb1a2e` lane-300: arifi-fa-replay - model-free replay of a dumped FLASH_ATTN_EXT node (swap/constant/pad/row-width arms) for the F-141 coopmat FA locate
- `fa61e861f` lane-300: fa-replay IGPU backend fallback + cell-10 dead-V value-class/column-range arms; det-probe prefill ends at min(t-7,t-w+1) for widths > 8
- `b5ea4e3ee` lane-300 F-141: coopmat1 FA zeroes V rows masked for every valid tile row (stale/foreign KV content no longer reaches the P*V WMMA sum); flags bit 32 default ON, GGML_VK_FA_DEADV_KEEP=1 = planted-RED switch; fa-replay sign-class arms. Lane: lane-300
- `f0be30eb6` lane-300: det-probe DETP_VERIFY cost mode - teacher-forced w-row verify timing, fix vs planted-RED on identical ids (HQ79 Q7). Lane: lane-300
- `630ed8e76` lane-300: det-probe DETP_ROWS_OPS=2 full node dump per width (last-row hash, I32 nodes included); cell-16 name-walk compare was blind across width-dependent graphs. Lane: lane-300
- `41cc36d84` lane-300: F-141 3b: LLAMA_ARIFI_MOE_SUMROWS_MAX=<N> keeps the n=1 permute+sum_rows expert-sum order for n_tokens<=N (cell 17: n=1 SUM_ROWS vs n>1 ADD chain = first width carrier, ffn_moe_out layer 0). Default 1 = unchanged. Lane: lane-300
- `3bea6eff0` lane-300: F-141 3b: FA split_k row-stable under GQA - workgroups_x is the token count, so split_k (KV reduction split) moved with verify width; for neq1 <= GGML_ARIFI_FA_ROWSTABLE_N (default 8, 0 = old) split_k is chosen as for one token. Cell 18: SR8+MMVQ-force first diff = layer-3 attention. Lane: lane-300
- `4274c5f12` lane-300: F-141 3b: row-stable FA split_k also on the segmented body dispatch (one helper, same GGML_ARIFI_FA_ROWSTABLE_N rule); GGML_ARIFI_FA_TRACE=1 prints each distinct FA dispatch tuple; det-probe DETP_ROWS_OPS=3 hashes every token slice of every node keyed by absolute position (single steps = per-position reference). Cell 19: w=4/8 still first-differ at layer-3 FA. Lane: lane-300
- `d3c419a01` lane-300: F-141 3b: GGML_ARIFI_ADD_RMS_ROWSTABLE_N=<N> keeps the residual ADD -> RMS_NORM partials fusion for rows <= N (was one row only: a verify batch normalized every row with a different reduction order than n=1 decode). add/multi_add already write partials by flat index/512, so row r owns its n=1 partials; rms_norm_partials reads row = WorkGroupID.y; admission keeps the one-row budget so every width fuses the same layers. Default 0 = unchanged. Cell 21: fused RED, unfused GREEN. Lane: lane-300
- `56d0974bd` lane-300: F-141 3b: arifi-mmv-width - model-free MUL_MAT width replay: column 0 of an n-column mat-vec byte-compared with the n=1 result per (type, m, k). Cell 23: qwen4exp layer-0 z (q4_K 6144x2560) differs n=4 vs n=1 with hoist off; f32-dequant route moves the first diff to the q6_K qkv sibling. Lane: lane-300
- `e13cd0de8` lane-300: F-141 3b: GGML_ARIFI_MMV_ROWSTABLE_N=<N> targets mat-vec column j bit-equal to n=1 for n <= N. Cell 24 (arifi-mmv-width, model-free) showed three width carriers: (1) the MMVQ vs f32-dequant route moved with n (q5_K, q8_0, q6_K, small-k q4_K); now n <= N takes n=1's route. (2) the q6_K -32 fold (ARIFI_Q6K_XFOLD) applied only at NUM_COLS <= 3, so n >= 4 used other arithmetic (source CHECKED); spec constant 4 = 2 folds at every width. (3) Q4_K/Q5_K MMVQ column 0 differed n>1 vs n=1 at k = 2560/3072 only, same shader + pipeline by trace; mechanism ASSUMED = FMA contraction of the a*b*c - d*e dot varying per NUM_COLS specialization (the k pattern fits num_iters = 3 at wave64, wave size ASSUMED); spec constant 5 builds that dot `precise`. (3) moves the n=1 bits of Q4_K/Q5_K MMVQ under the flag only. Default 0 = unchanged (receipt arms O-FX/Q-FX in cell 25). Lane: lane-300
- `538198cd9` lane-300: F-141 3b: precise Q4_K/Q5_K MMVQ dot moves to its own SPIR-V (*_prec_q8_1_f32), iq3_s stock shape under GGML_ARIFI_MMV_ROWSTABLE_N
- `a26d71b8f` lane-300: F-141 3b: GGML_ARIFI_FA_ROWSTABLE_N default 8 -> 0 (opt-in). bf1de9485b shipped it ON before the default-OFF rule; its verify-width cost is unmeasured (cell 28 held), so per HQ82 the default returns to stock split_k. N=0 never selects the n=1 split (neq1 >= 1), so ggml_vk_fa_rowstable_wx returns workgroups_x on the single and the segmented dispatch = merge-base behaviour by construction. Served proof: cell 27 OP-0/QP-0 (env FA_ROWSTABLE_N=0, other flags unset) ids == the 96a6a8d976 binary's plain ids (cells 12 S-fix, 13 O-fix) on short/2k/long, both models.
- `35a67b322` lane-299: layout copies - conv tails CPY strided (no CONT, 4 per conv per token), PLE one transpose-copy + one kernel cast (replaces 4 CONT + 4 CONT + 4 CAST + 1 CONT). Same bytes. Off: LLAMA_L299_LAYOUT_OFF=1; RED plant: LLAMA_L299_LAYOUT_PLANT=1 (stale slot-0 history). Op test L299_LAYOUT.
- `23de61bab` lane-299: item B alpha bias joins the ssm_alpha mat-vec - ADD(ssm_dt) moves before the reshape so MUL_MAT + ADD are adjacent and the backend MUL_MAT_ADD fusion engages (36 ADD dispatches/token; same math). Off: LLAMA_L299_ALPHA_OFF=1; RED plant: LLAMA_L299_ALPHA_PLANT=1 (bias twice). Op test: MUL_MAT_VEC_FUSION bf16 m=1 n=48 k=2560 with bias.
- `063b87820` lane-299: protected-wins entry qwen4exp-layout-copies (measured +1.48%/+1.19%, ids exact 10/10)
- `40b6dc11a` lane-298 integ: GGML_VK_GDN_SNAP1_DIAG prints a WARN at first use; OPTIONS-REGISTRY rows for both GDN diagnostics
- `7013014ac` lane-298 integ round 2: protected-wins commit fields repointed to the integ cherry-pick shas
- `01cde6911` arifi-sync: port the sha-pinned trailer-exemption ledger (r86i 58187870f + a9187ded7) natively
- `0f112f59b` arifi-sync: trailer-exemptions.json rows for the 63 --strict offenders under the shipped engine c23bd8cc5
- `c00417ff7` arifi-sync: re-key the protected-win ledgers from the pre-rebase chain to the x1i commits
- `0e4f3c72b` arifi-sync: x1i-reword - trailer table and replay script for the 58 post-x1i commits
- `d6ad887f3` arifi-sync: x1i-reword commits the ledger re-key as the tip commit's identity
- `3c2a74ae8` arifi-sync: x1i-reword table - one commit per win carries the number
- `0dcbd357c` arifi-sync: x1i-reword table - context rows carry no figures
- `acf329085` lane-298 integ round 2 freeze prep: full shas on the round-2 win commit fields; Q39 Measured-effect cells
- `4476c4221` arifi-sync: re-key the sha-pinned ledgers after the post-x1i trailer reword
- `aa9cac23a` protected-wins: register the integ wins W5 W6 W7 W8 W12 W13 W18, join W9 W16, cover the round-2 non-wins
- `2adf5cc55` common: fit block treats draft-mtp-adaptive as MTP (load crash fix)
- `7dbdd0731` tools: qwen4exp MTP sidecar builders, with Strata's fetch tool and its MIT license
- `1ea866f86` lane-300: F-141 producer fix - zero freed KV cells before they are read (LLAMA_ARIFI_KV_ZERO_FREED, default on)
- `7ecb9a918` lane-300: kv-zero hardening - mid-decode window bound, transposed-V block assert, zeroing log
- `2fc868a68` lane-300: LLAMA_ARIFI_KV_ZERO_FREED default 1 -> 0 (cell 31 OO re-cut failed the pooled clause)
- `58d81805c` build-info: --version prints the arifilabs release and its upstream base
- `bfa4b73e2` lane-302 lever 9: MUL_MAT_ID expert gather for Q2_0 and every legacy / q8_1 MMVQ id type
- `5991bb1e3` lane-302 lever 9: GGML_ARIFI_MOE_GATHER_EXT default OFF (opt-in, measured slower on Q2_0)
- `a5ff6df2b` protected-wins: register the v0.1.3.1 opt-in arms (KV_ZERO_FREED entry, GATHER_EXT joins the slot-major gather)
- `b0b41ed54` lane-300: LLAMA_ARIFI_KV_ZERO_FREED default 0 -> 1 (HQ84 item 3: kv-zero ships default ON in the point release)
- `410f8e275` protected-wins: kv-zero-freed-cells is default ON in v0.1.3.1 (b0b41ed548), id drops -opt-in
- `66f36df66` tools: replace local C:/ArifiLabs paths with repo-relative ones in hand-written tools (HQ84 item 9)
- `86d8ca4c2` vulkan moe-cache: mask the IQ3_S sub-block scale nibble in moe_cache_mv
- `3ef4ebe6d` tests: per-type check of the Vulkan MoE cache matvec against the CPU
- `d2d304af9` tests: mul_mat / mul_mat_id cases on a slice of a larger cache (RED on 7156c61607)
- `d1e94cd73` tests: matmul -> wide soft_max -> matmul on the same input (prealloc_y reuse, RED before #29591)
- `ed182b41f` tests: MUL_MAT_PREALLOC_REUSE - quantized tolerance and forced matmul-before-soft_max order
- `d1bd56ba2` tests: MUL_MAT_PREALLOC_REUSE v2 - strided input forces staging, FA mask-opt arm, noise-free copy
- `484df5e38` tests: gguf handcrafted file whose tensor size wraps after padding (RED on 7156c61607)
- `504b41e20` gguf: reject crafted files that hang or overflow the loader
- `cdbd475a4` tests: CLAMP on a non-contiguous view (RED on 7156c61607)
- `999a1790f` ggml-cpu, cuda: CPU reference fixes (CLAMP views, soft_max_back alias, get_rows_back bounds, AVX512-FP16 dot)
- `23b8c36b5` tests: MUL_MAT_PREALLOC_REUSE runs as one whole graph
- `305502a07` tests: draft acceptance stops at EOG (RED on 7156c61607)
- `1632f134e` common : stop accepting draft tokens at EOG
- `10c7a2816` llama : preserve original batch order for speculative decoding layer inputs
- `8eac38cd7` tests: state restore must refuse a different KV rotation (RED on 7156c61607)
- `9905714c2` kv-cache: save the exact KV rotation with the state, reject a mismatched restore
- `5543c29e1` lane-301 lever 4: fuse the qwen4exp QSA mask chain into the Vulkan FA
- `03a822b6e` lane-301 tests: QSA_MASK_FA perf-mode cases (RGP target for lever 4)
- `8632241b9` lane-301: protected-win vulkan-qsa-mask-fa-fusion for lever 4 (f42f16e38c)
- `00cea8047` ggml : collect all input tensors into graph_inputs
- `dbf3364ad` ggml : require input tensors to be GGML_OP_NONE
- `81bf18af2` llama : re-reserve the sched when the nextn extraction flags change
- `f433dfe70` glm5-next: give dead indexer slots unique scatter rows

