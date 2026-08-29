# ArifiLabs patch-series MANIFEST

<!-- GENERATED FILE - DO NOT EDIT BY HAND.
     Regenerate:  python tools/arifi-sync/arifi_sync.py series regen
     Verify:      python tools/arifi-sync/arifi_sync.py series check  -->

## What this is

`patches/series/` is the complete, ordered, replayable definition of everything ArifiLabs
adds to upstream llama.cpp. It is generated from git, never hand-maintained, and an
integrity check refuses to pass if it has drifted from git by so much as a byte.

- Base: `ggml-org/llama.cpp` tag `b10636`, `4d19b287691e8f47fc303be420f630c40ec45684`
- Patches: **407**, all non-merge, applied in filename order.

## Applying the series

```bash
git checkout -b my-rebuild 4d19b2876
git am patches/series/*.patch
```

The result is byte-identical to `master` everywhere outside `patches/series` itself:
same file contents, same 407 commit messages, same provenance trailers. Verified, not
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
git log --oneline 4d19b2876..master -- patches/series
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
| 1 | `0001-chore-establish-ArifiLabs-fork-identity-and-retained.patch` | arifi-fork-base | `c31d9bd53` | ggml-org/llama.cpp@571d0d540df04f25298d0e159e520d9fc62ed121, Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4, charlie12345/ROCmFPX@a6a93765f7ce9779c13f9881164a65f7a9f31198 | - | chore: establish ArifiLabs fork identity and retained notices |
| 2 | `0002-docs-add-options-registry-and-unified-A-F-catalog.patch` | arifi-fork-base | `ed76d6009` | ArifiLabs local-inference catalog and trial evidence | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PROF`, `LANE110_PREFETCH_CAP` | docs: add options registry and unified A-F catalog |
| 3 | `0003-ci-add-Windows-Vulkan-and-bench-smoke-placeholders.patch` | arifi-fork-base | `0d7c99675` | - | - | ci: add Windows Vulkan and bench-smoke placeholders |
| 4 | `0004-docs-require-loud-fail-upstream-currency-procedure.patch` | arifi-fork-base | `8ac8fb473` | ggml-org/llama.cpp@571d0d540df04f25298d0e159e520d9fc62ed121, charlie12345/ROCmFPX@a6a93765f7ce9779c13f9881164a65f7a9f31198, PrismML-Eng/llama.cpp@ternary-Q2_0_g128-lineage, Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | docs: require loud-fail upstream currency procedure |
| 5 | `0005-powerinfer-add-MoE-pipeline-fused-sparse-ggml-ops-se.patch` | powerinfer-streaming | `a9ae10439` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: add MoE pipeline + fused-sparse ggml ops [series 0001] |
| 6 | `0006-powerinfer-CPU-kernels-dispatch-for-streamed-MoE-ops.patch` | powerinfer-streaming | `225d9da24` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: CPU kernels + dispatch for streamed-MoE ops [series 0002] |
| 7 | `0007-powerinfer-vendor-streaming-library-incl.-lane-110-W.patch` | powerinfer-streaming | `463656128` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED` | powerinfer: vendor streaming library incl. lane-110 Windows port [series 0003] |
| 8 | `0008-build-wire-powerinfer-lib-AVX-spill-cure-into-CMake-.patch` | powerinfer-streaming | `30b6c6668` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | build: wire powerinfer lib + AVX-spill cure into CMake [series 0004] |
| 9 | `0009-powerinfer-expert-bundle-loader-GENERATE_EXPERT_BUND.patch` | powerinfer-streaming | `939873fd0` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | powerinfer: expert-bundle loader + GENERATE_EXPERT_BUNDLE [series 0005] |
| 10 | `0010-powerinfer-generic-MoE-streaming-hook-SiLU-op-type-s.patch` | powerinfer-streaming | `c7684a3b4` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP` | powerinfer: generic MoE streaming hook, SiLU op-type, standalone expert prefetch [series 0006+0009] |
| 11 | `0011-powerinfer-per-ubatch-pipeline-init-guarded-reuse-PR.patch` | powerinfer-streaming | `a75484820` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `LANE110_PROF` | powerinfer: per-ubatch pipeline init, guarded reuse, PROF field [series 0007+0010] |
| 12 | `0012-powerinfer-repack-carve-out-for-streamed-tensors-ser.patch` | powerinfer-streaming | `ba6f1e51a` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | powerinfer: repack carve-out for streamed tensors [series 0008a] |
| 13 | `0013-powerinfer-down-projection-transpose-fix-series-0008.patch` | powerinfer-streaming | `6aae7ede0` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: down-projection transpose fix [series 0008c] |
| 14 | `0014-powerinfer-GPU-safe-staging-CPU-zero-copy-LANE110_PR.patch` | powerinfer-streaming | `e63a775bc` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `LANE110_PROF` | powerinfer: GPU-safe staging, CPU zero-copy, LANE110_PROF [series 0011] |
| 15 | `0015-lane-110-document-UI-ON-as-local-default-build-varia.patch` | arifi-fork-base | `fd58e6a33` | - | `LLAMA_USE_PREBUILT_UI` | lane-110: document UI-ON as local default build variant (static-runtime cure for ki-webui-0xC0000139) |
| 16 | `0016-community-llama-pin-mmap-backed-CPU-weights-for-fast.patch` | community-prefetch | `b5ebf17a4` | thecodacus/llama.cpp@20f5994bfeb91d24da328077c4b6095998cc9888 | - | community: llama : pin mmap-backed CPU weights for faster H2D uploads |
| 17 | `0017-community-ggml-overlap-offloaded-expert-weight-uploa.patch` | community-prefetch | `564910a6c` | thecodacus/llama.cpp@1163cb34939fe4a9cb07aec034c5954144497ae9 | - | community: ggml : overlap offloaded expert weight uploads with compute |
| 18 | `0018-community-ggml-size-prefetch-slots-per-layer-and-fix.patch` | community-prefetch | `e57cf7085` | thecodacus/llama.cpp@5f83fbbe7c668c59912a1fe09e86a0ef580406c4 | - | community: ggml : size prefetch slots per layer and fix fallback use-after-free |
| 19 | `0019-tools-gguf-add-fail-closed-g128-retagger.patch` | ternary-g128 | `8f09804ff` | - | - | tools(gguf): add fail-closed g128 retagger |
| 20 | `0020-tools-gguf-retagger-v2-structure-only-parser-no-tens.patch` | ternary-g128 | `a2d985394` | - | - | tools(gguf): retagger v2 — structure-only parser, no tensor materialization |
| 21 | `0021-g128-engine-port-applied-BUILD-BLOCKED-on-copy_to_qu.patch` | ternary-g128 | `277b9189e` | PrismML-Eng/llama.cpp@984bf9723, PrismML-Eng/llama.cpp@34dc5812c | - | g128 engine port applied — BUILD-BLOCKED on copy_to_quant.comp shader (Sol's fix owed) |
| 22 | `0022-vulkan-fix-f16-promotion-in-q2_0_g128-copy_to_quant.patch` | ternary-g128 | `70f91f02c` | - | - | vulkan: fix f16 promotion in q2_0_g128 copy_to_quant |
| 23 | `0023-vulkan-g128-shader-gen-parity-MMQ-helper-completion.patch` | ternary-g128 | `62f207f49` | - | - | vulkan: g128 shader-gen parity + MMQ helper completion |
| 24 | `0024-vulkan-g128-DMMV-repack4-byte-oriented-rewrite.patch` | ternary-g128 | `f04568666` | - | - | vulkan: g128 DMMV repack4 byte-oriented rewrite |
| 25 | `0025-loader-g128-include-get_next_tensor-ctx-arg.patch` | ternary-g128 | `25a111b1a` | - | - | loader: g128 include + get_next_tensor ctx arg |
| 26 | `0026-loader-use-public-ggml_blck_size-for-g128-drop-inter.patch` | ternary-g128 | `5d127e2c7` | - | - | loader: use public ggml_blck_size for g128, drop internal include |
| 27 | `0027-tools-gguf-retag-v3-verified-type-id-rewrite-42-43.patch` | ternary-g128 | `3275805f2` | - | - | tools(gguf): retag v3 — verified type-id rewrite 42->43 |
| 28 | `0028-loader-accept-native-Q2_0_G128-tensors.patch` | ternary-g128 | `33e3daa06` | - | - | loader: accept native Q2_0_G128 tensors |
| 29 | `0029-ggml-cpu-dispatch-Q2_0_G128-at-the-7-Q2_0-sites.patch` | ternary-g128 | `8768b5c40` | - | - | ggml-cpu: dispatch Q2_0_G128 at the 7 Q2_0 sites |
| 30 | `0030-port-f16-recurrent-S-state-env-gated-GGML_RECURRENT_.patch` | prismml-recurrent | `f713f1829` | PrismML-Eng/llama.cpp@3e0855571 | `GGML_RECURRENT_STATE_F16` | port: f16 recurrent S-state (env-gated GGML_RECURRENT_STATE_F16) |
| 31 | `0031-port-M-RoPE-embedded-batch-position-guard.patch` | rocmfpx-mrope | `65a897205` | charlie12345/ROCmFPX@rocmfpx-a6a9376 | - | port: M-RoPE embedded-batch position guard |
| 32 | `0032-docs-phase-3a-OPTIONS-REGISTRY-rows-M-RoPE-f16-recur.patch` | arifi-fork-base | `5ee4e9dca` | - | `GGML_RECURRENT_STATE_F16` | docs: phase-3a OPTIONS-REGISTRY rows (M-RoPE, f16-recurrent, DSpark-iGPU) |
| 33 | `0033-port-Windows-IOCP-async-expert-bundle-reads-M2b.patch` | iocp-async | `cb97f455e` | - | - | port: Windows IOCP async expert-bundle reads (M2b) |
| 34 | `0034-feat-POWERINFER_IOCP-runtime-toggle-for-the-Windows-.patch` | iocp-async | `cf79edd63` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | feat: POWERINFER_IOCP runtime toggle for the Windows expert-read path |
| 35 | `0035-docs-OPTIONS-REGISTRY-POWERINFER_IOCP-measured-row-s.patch` | iocp-async | `2faaf1fc4` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | docs: OPTIONS-REGISTRY POWERINFER_IOCP measured row (same-binary A/B) |
| 36 | `0036-docs-correct-POWERINFER_IOCP-row-A-B-was-noise-direc.patch` | iocp-async | `7ae4ca3c5` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | docs: correct POWERINFER_IOCP row - A/B was noise, direction UNRESOLVED |
| 37 | `0037-patches-generate-the-full-auditable-38-patch-series-.patch` | arifi-fork-base | `715c433c3` | ArifiLabs lane-110C fork-maintainability slot | - | patches: generate the full auditable 38-patch series from git |
| 38 | `0038-tools-add-arifi-sync-the-currency-series-and-upstrea.patch` | arifi-fork-base | `a22db15c2` | ArifiLabs lane-110C fork-maintainability slot | `LLAMA_USE_PREBUILT_UI`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `ARIFI_TOOL_NVFP4_REMAP` | tools: add arifi-sync - the currency, series and upstream-bump driver |
| 39 | `0039-docs-README-section-a-GitHub-visitor-can-read-cold.patch` | arifi-fork-base | `fa43f7c7b` | ArifiLabs lane-110C fork-maintainability slot | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP` | docs: README section a GitHub visitor can read cold |
| 40 | `0040-patches-move-banked-PrismML-source-patches-out-of-th.patch` | arifi-fork-base | `5bb391cc5` | PrismML-Eng/llama.cpp@ternary-Q2_0_g128-lineage | - | patches: move banked PrismML source patches out of the generated series directory |
| 41 | `0041-patches-make-the-series-generator-implement-its-own-.patch` | arifi-fork-base | `ce32525d0` | - | - | patches: make the series generator implement its own convergence rule |
| 42 | `0042-patches-stop-the-manifest-deriving-anything-from-exc.patch` | arifi-fork-base | `fc39077ec` | - | - | patches: stop the manifest deriving anything from excluded commits |
| 43 | `0043-port-Q1_0-repack-scaffolding-Arm-NEON-DP-GEMV-GEMM-k.patch` | arifi-fork-base | `c64d968b9` | PrismML-Eng/llama.cpp@720e06b16 | - | port: Q1_0 repack scaffolding + Arm NEON/DP GEMV-GEMM kernels |
| 44 | `0044-port-native-x86-Q2_0-vec_dot-AVX512-VNNI-AVX-VNNI-dr.patch` | arifi-fork-base | `6d77a587d` | PrismML-Eng/llama.cpp@4d88cd4eb, PrismML-Eng/llama.cpp@79697f23a | - | port: native x86 Q2_0 vec_dot (AVX512-VNNI / AVX-VNNI), drop scalar fallback alias |
| 45 | `0045-port-x86-AVX512-VNNI-repack-GEMV-GEMM-for-Q1_0-and-Q.patch` | arifi-fork-base | `9251864bc` | PrismML-Eng/llama.cpp@9fcaed763 | - | port: x86 AVX512-VNNI repack GEMV/GEMM for Q1_0 and Q2_0 |
| 46 | `0046-feat-GGML_ARIFI_VNNI_REPACK-runtime-toggle-explicit-.patch` | ternary-g128 | `6a1f2c092` | - | `GGML_ARIFI_VNNI_REPACK` | feat: GGML_ARIFI_VNNI_REPACK runtime toggle + explicit g64/g128 tripwire |
| 47 | `0047-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_VNNI_REPACK.patch` | arifi-fork-base | `a9794d296` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY row for GGML_ARIFI_VNNI_REPACK - UNMEASURED, artifact-blocked |
| 48 | `0048-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-CORRECT.patch` | arifi-fork-base | `5236e4e0d` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - CORRECTNESS FAIL, GEMM returns NaN |
| 49 | `0049-fix-ggml-cpu-parameterize-Q2_0-GEMM-activation-strid.patch` | arifi-fork-base | `d39f16e7b` | PrismML-Eng/llama.cpp (Q2_0 4x8 VNNI GEMM design) | - | fix(ggml-cpu): parameterize Q2_0 GEMM activation stride by QK2_0/QK8_0 - g64 NaN |
| 50 | `0050-fix-ggml-cpu-same-g64-stride-defect-in-the-ported-x8.patch` | arifi-fork-base | `bc62a6fe7` | - | - | fix(ggml-cpu): same g64 stride defect in the ported x86 Q2_0 vec_dot |
| 51 | `0051-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-NaN-FIX.patch` | arifi-fork-base | `bea5f3954` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - NaN FIXED, bit-identity NOT established |
| 52 | `0052-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-bit-div.patch` | arifi-fork-base | `92dc53eb7` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - bit-divergence ADJUDICATED, performance MEASURED |
| 53 | `0053-docs-correct-the-VNNI-row-s-base-commit-labelling-f7.patch` | arifi-fork-base | `2d2c17637` | - | `GGML_ARIFI_VNNI_REPACK` | docs: correct the VNNI row's base-commit labelling - f7c4d9d2c is NOT master |
| 54 | `0054-docs-fix-two-stray-table-breaking-pipes-in-the-VNNI-.patch` | arifi-fork-base | `979da859a` | - | `GGML_ARIFI_VNNI_REPACK` | docs: fix two stray table-breaking pipes in the VNNI registry row |
| 55 | `0055-docs-OPTIONS-REGISTRY-VNNI-row-topology-cell-now-ref.patch` | arifi-fork-base | `a2d836a70` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY VNNI row - topology cell now reflects the linear-master rebase |
| 56 | `0056-ggml-cpu-GGML_ARIFI_VNNI_REPACK-defaults-to-OFF-dive.patch` | ternary-g128 | `b236b4e55` | - | `GGML_ARIFI_VNNI_REPACK` | ggml-cpu: GGML_ARIFI_VNNI_REPACK defaults to OFF (diverging from upstream, on measurement) |
| 57 | `0057-docs-fork-wide-GGUF-type-ID-allocation-contract-keys.patch` | arifi-fork-base | `eb7ea57eb` | - | `ARIFI_TOOL_NVFP4_REMAP` | docs: fork-wide GGUF type-ID allocation contract (keystone, unblocks 4 slots) |
| 58 | `0058-ggml-reserve-the-ArifiLabs-type-ID-blocks-at-the-enu.patch` | arifi-fork-base | `401eae47a` | - | - | ggml: reserve the ArifiLabs type-ID blocks at the enum itself |
| 59 | `0059-ggml-ROCmFP4-ROCmFPX-weight-formats-behind-GGML_ARIF.patch` | arifi-fork-base | `312b7b3e8` | charlie12345/ROCmFPX@3edc3d31e | - | ggml: ROCmFP4/ROCmFPX weight formats behind GGML_ARIFI_ROCMFPX_FORMATS |
| 60 | `0060-llama-ROCmFPX-file-types-quantizer-wiring-and-the-gg.patch` | arifi-fork-base | `4bc3ac170` | charlie12345/ROCmFPX@3edc3d31e | - | llama: ROCmFPX file types, quantizer wiring and the gguf-py registry |
| 61 | `0061-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_ROCMFPX_FOR.patch` | arifi-fork-base | `29f22d320` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY row for GGML_ARIFI_ROCMFPX_FORMATS |
| 62 | `0062-tools-record-GGML_ARIFI_ROCMFPX_FORMATS-in-the-build.patch` | arifi-fork-base | `b968f70d3` | - | - | tools: record GGML_ARIFI_ROCMFPX_FORMATS in the build-recipe snapshot |
| 63 | `0063-docs-make-the-fork-buildable-by-someone-who-did-not-.patch` | arifi-fork-base | `73362713a` | - | `LLAMA_USE_PREBUILT_UI`, `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `GGML_RECURRENT_STATE_F16` | docs: make the fork buildable by someone who did not build it |
| 64 | `0064-feat-arifi-profile-and-a-CPU-only-VNNI-repack-startu.patch` | arifi-fork-base | `ddde547df` | - | `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP`, `MAX_N_CACHED`, `GGML_RECURRENT_STATE_F16` | feat: --arifi-profile and a CPU-only VNNI-repack startup advisory |
| 65 | `0065-sync-bump-base-to-upstream-b10173-e9fa0781f-series-r.patch` | ternary-g128 | `081511473` | upstream b10173 e9fa0781f | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PROF`, `LANE110_PREFETCH_CAP`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | sync: bump base to upstream b10173 (e9fa0781f) — series replayed (1 hand-resolved conflict: loader load-mode refactor e6dd0e29a), judge harness wired + exercised (F-085 llama-server, g128 ternary 27B PASS), pins updated, series regenerated |
| 66 | `0066-sync-currency-GREEN-TurboQuant-checkouts-pinned-at-t.patch` | arifi-fork-base | `420ccfc17` | - | - | sync: currency GREEN — TurboQuant checkouts pinned at their lane-110C-reviewed shas (c26cbdffc, ba52ad107), rocmfpx pin advanced to db6844d9b after 6-commit review (HIP pinned-staging noted for the HIP-deltas slot) |
| 67 | `0067-fix-ggml-cpu-x86-don-t-pass-__m256i-by-value-across-.patch` | arifi-fork-base | `67737ceaa` | - | - | fix(ggml-cpu/x86): don't pass __m256i by value across the repack GEMV/GEMM ABI boundary (Windows 0xC0000005) |
| 68 | `0068-ggml-TurboQuant-TQ3_1S-TQ4_1S-weight-formats-behind-.patch` | arifi-fork-base | `1f67e7d7d` | - | - | ggml: TurboQuant TQ3_1S/TQ4_1S weight formats behind GGML_ARIFI_TURBO_WEIGHT_QUANTS |
| 69 | `0069-docs-CHANGELOG-and-FINDINGS-the-public-trail-a-stran.patch` | arifi-fork-base | `a76624e33` | - | `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP` | docs: CHANGELOG and FINDINGS - the public trail a stranger can actually read |
| 70 | `0070-docs-USAGE-a-task-oriented-guide-for-humans-not-an-o.patch` | arifi-fork-base | `dfaf2b168` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `POWERINFER_IOCP` | docs: USAGE - a task-oriented guide for humans, not an option reference |
| 71 | `0071-ggml-cpu-x86-use-the-row-stride-parameter-in-repack-.patch` | arifi-fork-base | `15cb1fbe4` | - | - | ggml-cpu/x86: use the row-stride parameter in repack GEMV stores instead of the row count |
| 72 | `0072-ggml-cpu-fuse-the-TurboQuant-dot-product-transform-t.patch` | ternary-g128 | `51aa7bdde` | - | - | ggml-cpu: fuse the TurboQuant dot product - transform the activation, not the weights |
| 73 | `0073-docs-FINDINGS-F-09-the-repack-GPU-trade-off-is-a-com.patch` | arifi-fork-base | `bddafc077` | - | - | docs(FINDINGS): F-09 - the repack/GPU trade-off is a compute-placement choice, not a bug |
| 74 | `0074-docs-neutralise-internal-governance-vocabulary-expla.patch` | arifi-fork-base | `288b13b2f` | - | - | docs: neutralise internal governance vocabulary; explain the work-unit ids instead |
| 75 | `0075-docs-FINDINGS-record-why-the-repack-auto-enable-was-.patch` | arifi-fork-base | `0e52f3374` | - | - | docs(FINDINGS): record why the repack auto-enable was NOT shipped |
| 76 | `0076-provenance-catch-the-silent-trailer-block-break-inst.patch` | arifi-fork-base | `83eea6cbb` | - | - | provenance: catch the silent trailer-block break instead of only reporting it |
| 77 | `0077-gitattributes-pin-.githooks-to-LF-so-the-hook-runs-o.patch` | arifi-fork-base | `abf1860d0` | - | - | gitattributes: pin .githooks to LF so the hook runs on Linux and macOS |
| 78 | `0078-githooks-force-a-normalized-blob-for-the-commit-msg-.patch` | arifi-fork-base | `ae58bf43c` | - | - | githooks: force a normalized blob for the commit-msg hook |
| 79 | `0079-docs-FINDINGS-F-09-exit-1-is-the-direction-dual-resi.patch` | arifi-fork-base | `3e5dbdceb` | - | - | docs(FINDINGS): F-09 exit 1 is the direction - dual residency, not a forced choice |
| 80 | `0080-repack-dual-residency-GGML_ARIFI_VNNI_REPACK-2-keeps.patch` | arifi-fork-base | `b756121c7` | - | `GGML_ARIFI_VNNI_REPACK` | repack: dual residency - GGML_ARIFI_VNNI_REPACK=2 keeps prefill on the GPU AND repacks decode |
| 81 | `0081-repack-dual-residency-measured-both-wins-held-fix-th.patch` | arifi-fork-base | `e197c8e90` | - | `GGML_ARIFI_VNNI_REPACK` | repack: dual residency measured - both wins held; fix the hot-path cost that ate the first one |
| 82 | `0082-repack-fix-the-shadow-tensor-COUNT-in-the-accounting.patch` | arifi-fork-base | `c12ced72c` | - | `GGML_ARIFI_VNNI_REPACK` | repack: fix the shadow tensor COUNT in the accounting line, and strengthen the claim it supports |
| 83 | `0083-repack-CPU-kernels-for-the-128-group-ternary-format.patch` | arifi-fork-base | `8ceac6e1a` | - | `GGML_ARIFI_VNNI_REPACK` | repack: CPU kernels for the 128-group ternary format |
| 84 | `0084-tests-direct-equivalence-test-for-the-ternary-repack.patch` | arifi-fork-base | `adcf40c8e` | - | - | tests: direct equivalence test for the ternary repack kernels |
| 85 | `0085-docs-the-g128-repack-SPEED-numbers-measured.patch` | arifi-fork-base | `1ea435780` | - | `GGML_ARIFI_VNNI_REPACK` | docs: the g128 repack SPEED numbers, measured |
| 86 | `0086-docs-frame-the-g128-repack-numbers-as-a-CPU-tier-res.patch` | arifi-fork-base | `a27586519` | - | - | docs: frame the g128 repack numbers as a CPU-tier result |
| 87 | `0087-docs-publish-the-power-plan-finding-as-F-12-and-bind.patch` | arifi-fork-base | `a42578bf7` | - | - | docs: publish the power-plan finding as F-12, and bind the bench protocol to it |
| 88 | `0088-F-06-measure-the-GPU-tier-on-one-binary-and-count-th.patch` | arifi-fork-base | `4376421fc` | - | - | F-06: measure the GPU tier on one binary, and count the placement the prompt claim rested on |
| 89 | `0089-registry-the-dual-residency-prompt-mechanism-is-meas.patch` | arifi-fork-base | `de367c13c` | - | `GGML_ARIFI_VNNI_REPACK` | registry: the dual-residency prompt mechanism is measured now, not ASSUMED |
| 90 | `0090-powerinfer-MAX_N_CACHED-admitted-two-values-that-bre.patch` | powerinfer-streaming | `0711ff7c7` | - | `MAX_N_CACHED` | powerinfer: MAX_N_CACHED admitted two values that break the eviction invariant |
| 91 | `0091-rows-per-workgroup-on-g128-a-null-and-F-10-confirmed.patch` | ternary-g128 | `489d95de9` | - | - | rows-per-workgroup on g128: a null, and F-10 confirmed on a second file |
| 92 | `0092-registry-our-DSpark-verdict-was-never-ours-the-draft.patch` | arifi-fork-base | `8e80c0cf7` | - | - | registry: our DSpark verdict was never ours -- the drafter cannot load on this fork |
| 93 | `0093-speculative-DFlash-DSpark-drafters-have-no-vocab-tak.patch` | arifi-fork-base | `f85196125` | - | - | speculative: DFlash/DSpark drafters have no vocab -- take the mask token from metadata |
| 94 | `0094-expert-cache-stride-the-bundle-by-the-PADDED-matrix-.patch` | arifi-fork-base | `e15cde987` | - | - | expert cache: stride the bundle by the PADDED matrix size -- unblocks our own brain |
| 95 | `0095-powerinfer-POWERINFER_NO_BUFFERING-1-the-Windows-cou.patch` | powerinfer-streaming | `07e9d62ac` | - | `POWERINFER_IOCP` | powerinfer: POWERINFER_NO_BUFFERING=1 -- the Windows counterpart of the O_DIRECT Linux already uses |
| 96 | `0096-expert-cache-POWERINFER_EXPERT_HEATMAP-measure-acces.patch` | arifi-fork-base | `a47109eb3` | - | - | expert cache: POWERINFER_EXPERT_HEATMAP -- measure access skew before building a pin policy |
| 97 | `0097-ours-1163cb34939fe4a9cb07aec034c5954144497ae9.patch.patch` | arifi-fork-base | `b7c313dcc` | - | - | ours/1163cb34939fe4a9cb07aec034c5954144497ae9.patch |
| 98 | `0098-1-core-types-0001-WIP-add-TurboQuant-KV-cache-types-.patch` | arifi-fork-base | `95a4a9cd4` | - | - | 1-core-types/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 99 | `0099-1-core-types-0002-ggml-port-TurboQuant-core-quant-ty.patch` | arifi-fork-base | `c57b624b9` | - | - | 1-core-types/0002-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 100 | `0100-2-cpu-reference-0001-ggml-port-TurboQuant-core-quant.patch` | arifi-fork-base | `6e6fffce4` | - | - | 2-cpu-reference/0001-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 101 | `0101-2-cpu-reference-0007-ggml-cpu-declare-turbo3_cpu_wht.patch` | arifi-fork-base | `8d3e64baf` | - | - | 2-cpu-reference/0007-ggml-cpu-declare-turbo3_cpu_wht_group_size-extern-no.patch |
| 102 | `0102-2-cpu-reference-0008-merge-fix-post-merge-build-and-.patch` | arifi-fork-base | `a0ea176ea` | - | - | 2-cpu-reference/0008-merge-fix-post-merge-build-and-crash-issues.patch |
| 103 | `0103-3-vulkan-0001-vulkan-metal-hip-add-TurboQuant-kernel.patch` | arifi-fork-base | `bef8e4fc9` | - | - | 3-vulkan/0001-vulkan-metal-hip-add-TurboQuant-kernel-support.patch |
| 104 | `0104-3-vulkan-0002-vulkan-port-fork-241-wave64-ballot-fix.patch` | arifi-fork-base | `f87714b3b` | - | - | 3-vulkan/0002-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch |
| 105 | `0105-3-vulkan-0003-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `1011cbe0c` | - | - | 3-vulkan/0003-fix-close-complete-audit-findings-7-port-regressions.patch |
| 106 | `0106-3-vulkan-0004-vulkan-reconstruct-supports_op-after-r.patch` | arifi-fork-base | `665194368` | - | - | 3-vulkan/0004-vulkan-reconstruct-supports_op-after-rebase-merge-da.patch |
| 107 | `0107-3-vulkan-0006-vulkan-restore-TQ4_1S-weight-type-wiri.patch` | arifi-fork-base | `61d2a5b4d` | - | - | 3-vulkan/0006-vulkan-restore-TQ4_1S-weight-type-wiring-lost-in-the.patch |
| 108 | `0108-3-vulkan-0007-vulkan-add-the-TQ3_1S-weight-type.patc.patch` | arifi-fork-base | `e49188d7d` | - | - | 3-vulkan/0007-vulkan-add-the-TQ3_1S-weight-type.patch |
| 109 | `0109-3-vulkan-0008-vulkan-reject-MUL_MAT_ID-for-the-Turbo.patch` | arifi-fork-base | `4b2bad742` | - | - | 3-vulkan/0008-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch |
| 110 | `0110-3-vulkan-0013-vulkan-TQ-rotated-matmul-shader-ground.patch` | arifi-fork-base | `ed0811236` | - | - | 3-vulkan/0013-vulkan-TQ-rotated-matmul-shader-groundwork-A-side-lo.patch |
| 111 | `0111-3-vulkan-0015-vulkan-create-the-TQ-activation-rotate.patch` | arifi-fork-base | `47e9ed806` | - | - | 3-vulkan/0015-vulkan-create-the-TQ-activation-rotate-pipeline.patch |
| 112 | `0112-3-vulkan-0017-vulkan-rotate-the-staged-activation-co.patch` | arifi-fork-base | `cacdbf32d` | - | - | 3-vulkan/0017-vulkan-rotate-the-staged-activation-copy-in-place-dr.patch |
| 113 | `0113-3-vulkan-0018-vulkan-register-the-TQ-rotated-mul_mm_.patch` | arifi-fork-base | `f591884cb` | - | - | 3-vulkan/0018-vulkan-register-the-TQ-rotated-mul_mm_id-pipelines.patch |
| 114 | `0114-3-vulkan-0021-vulkan-fix-SET_ROWS-block-decompositio.patch` | arifi-fork-base | `26f8aae4f` | - | - | 3-vulkan/0021-vulkan-fix-SET_ROWS-block-decomposition-for-turbo-we.patch |
| 115 | `0115-4-other-backends-0006-laguna-MoE-down-proj-f16-overf.patch` | arifi-fork-base | `c1a9f9912` | - | - | 4-other-backends/0006-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch |
| 116 | `0116-4-other-backends-0007-cuda-first-class-MoE-path-plai.patch` | arifi-fork-base | `3d3580de7` | - | - | 4-other-backends/0007-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch |
| 117 | `0117-4-other-backends-0008-hip-add-mixed-f16-bf16-q8_0-fa.patch` | arifi-fork-base | `98cc91268` | - | - | 4-other-backends/0008-hip-add-mixed-f16-bf16-q8_0-fattn-vec-instances-to-t.patch |
| 118 | `0118-4-other-backends-0009-cuda-revert-GQA-ratio-dispatch.patch` | arifi-fork-base | `042e9b82e` | - | - | 4-other-backends/0009-cuda-revert-GQA-ratio-dispatch-to-comparison-form-fi.patch |
| 119 | `0119-4-other-backends-0010-cuda-restore-Volta-GQA-modulo-.patch` | arifi-fork-base | `7dbbea15f` | - | - | 4-other-backends/0010-cuda-restore-Volta-GQA-modulo-dispatch-over-revert-i.patch |
| 120 | `0120-4-other-backends-0015-cuda-rework-MoE-expert-cache-e.patch` | arifi-fork-base | `54978e401` | - | - | 4-other-backends/0015-cuda-rework-MoE-expert-cache-execution.patch |
| 121 | `0121-4-other-backends-0016-cuda-fix-MoE-cache-capacity-ac.patch` | arifi-fork-base | `db786092f` | - | - | 4-other-backends/0016-cuda-fix-MoE-cache-capacity-accounting.patch |
| 122 | `0122-4-other-backends-0017-cuda-fix-MoE-cache-scratch-bud.patch` | arifi-fork-base | `52a5cfb6e` | - | - | 4-other-backends/0017-cuda-fix-MoE-cache-scratch-budget-replacement.patch |
| 123 | `0123-4-other-backends-0018-cuda-bound-MoE-MMV-tail-row-re.patch` | arifi-fork-base | `6b325964a` | - | - | 4-other-backends/0018-cuda-bound-MoE-MMV-tail-row-reads.patch |
| 124 | `0124-4-other-backends-0019-cuda-tune-MoE-cache-CPU-overla.patch` | arifi-fork-base | `5fc3a0ed9` | - | - | 4-other-backends/0019-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 125 | `0125-4-other-backends-0020-cuda-adapt-MoE-cache-admission.patch` | arifi-fork-base | `76dffada3` | - | - | 4-other-backends/0020-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 126 | `0126-4-other-backends-0021-cuda-align-MoE-cache-pool-allo.patch` | arifi-fork-base | `ee8db7983` | - | - | 4-other-backends/0021-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 127 | `0127-4-other-backends-0022-cuda-prefer-generic-MMV-for-co.patch` | arifi-fork-base | `f365fb75f` | - | - | 4-other-backends/0022-cuda-prefer-generic-MMV-for-compatible-MoE-cache-nod.patch |
| 128 | `0128-4-other-backends-0023-cuda-parallelize-MoE-cache-fil.patch` | arifi-fork-base | `6c19040f9` | - | - | 4-other-backends/0023-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch |
| 129 | `0129-4-other-backends-0024-cuda-fuse-cached-MoE-SwiGLU-ro.patch` | arifi-fork-base | `2119e81a0` | - | - | 4-other-backends/0024-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 130 | `0130-4-other-backends-0025-cuda-aggregate-small-MoE-tenso.patch` | arifi-fork-base | `8989261c6` | - | - | 4-other-backends/0025-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 131 | `0131-4-other-backends-0026-cuda-bound-automatic-MoE-cache.patch` | arifi-fork-base | `97c79b5e5` | - | - | 4-other-backends/0026-cuda-bound-automatic-MoE-cache-admission.patch |
| 132 | `0132-4-other-backends-0027-cuda-reject-undersized-automat.patch` | arifi-fork-base | `fa26043e3` | - | - | 4-other-backends/0027-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 133 | `0133-4-other-backends-0028-cuda-enable-multi-token-MoE-ca.patch` | arifi-fork-base | `2fd6b3167` | - | - | 4-other-backends/0028-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 134 | `0134-4-other-backends-0029-common-surface-MoE-cache-activ.patch` | arifi-fork-base | `262c414b4` | - | - | 4-other-backends/0029-common-surface-MoE-cache-activation.patch |
| 135 | `0135-4-other-backends-0030-cuda-accelerate-complete-MoE-c.patch` | arifi-fork-base | `833c5eab9` | - | - | 4-other-backends/0030-cuda-accelerate-complete-MoE-cache-pools.patch |
| 136 | `0136-4-other-backends-0031-cuda-report-oversized-MoE-cach.patch` | arifi-fork-base | `3d0889ad9` | - | - | 4-other-backends/0031-cuda-report-oversized-MoE-cache-nodes.patch |
| 137 | `0137-4-other-backends-0032-cuda-clarify-MoE-cache-session.patch` | arifi-fork-base | `4f1b210dc` | - | - | 4-other-backends/0032-cuda-clarify-MoE-cache-session-diagnostics.patch |
| 138 | `0138-4-other-backends-0033-cuda-report-MoE-cache-pair-res.patch` | arifi-fork-base | `a97cf9c5c` | - | - | 4-other-backends/0033-cuda-report-MoE-cache-pair-residency.patch |
| 139 | `0139-4-other-backends-0034-cuda-pack-MoE-cache-dispatch-i.patch` | arifi-fork-base | `7950475e8` | - | - | 4-other-backends/0034-cuda-pack-MoE-cache-dispatch-inputs.patch |
| 140 | `0140-4-other-backends-0037-metal-port-TQ_NO_ROTATE-escape.patch` | arifi-fork-base | `fcb0143db` | - | - | 4-other-backends/0037-metal-port-TQ_NO_ROTATE-escape-hatch-from-stash-opt-.patch |
| 141 | `0141-4-other-backends-0039-moe-cache-enable-HIP-backend-b.patch` | arifi-fork-base | `93b221406` | - | - | 4-other-backends/0039-moe-cache-enable-HIP-backend-by-removing-no-op-stubs.patch |
| 142 | `0142-4-other-backends-0040-moe-cache-count-dispatch-conte.patch` | arifi-fork-base | `af8b6262d` | - | - | 4-other-backends/0040-moe-cache-count-dispatch-contention-bypasses-P1.patch |
| 143 | `0143-4-other-backends-0041-moe-cache-provider-registry-wi.patch` | arifi-fork-base | `c4a21f972` | - | - | 4-other-backends/0041-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 144 | `0144-4-other-backends-0042-moe-cache-audit-fixes-H1-F1-F2.patch` | arifi-fork-base | `86475b3a5` | - | - | 4-other-backends/0042-moe-cache-audit-fixes-H1-F1-F2-A1.patch |
| 145 | `0145-4-other-backends-0043-moe-cache-fix-registry-build-I.patch` | arifi-fork-base | `638f12227` | - | - | 4-other-backends/0043-moe-cache-fix-registry-build-I1-follow-up.patch |
| 146 | `0146-4-other-backends-0044-moe-cache-allow-automatic-mode.patch` | arifi-fork-base | `484a71de8` | - | - | 4-other-backends/0044-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 147 | `0147-4-other-backends-0045-cuda-fix-HIP-MoE-cache-compati.patch` | arifi-fork-base | `9ec3cd7c8` | - | - | 4-other-backends/0045-cuda-fix-HIP-MoE-cache-compatibility-and-q8_1-sums.patch |
| 148 | `0148-4-other-backends-0046-moe-cache-add-logging-at-silen.patch` | arifi-fork-base | `2d7d47351` | - | - | 4-other-backends/0046-moe-cache-add-logging-at-silent-failure-points-acros.patch |
| 149 | `0149-4-other-backends-0048-cuda-fit-fix-two-Werror-build-.patch` | arifi-fork-base | `5aca05ef5` | - | - | 4-other-backends/0048-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch |
| 150 | `0150-4-other-backends-0049-sycl-drop-the-duplicated-nthre.patch` | arifi-fork-base | `47e151678` | - | - | 4-other-backends/0049-sycl-drop-the-duplicated-nthreads-declarations-in-fa.patch |
| 151 | `0151-4-other-backends-0050-fattn-vec-split-the-turbo-K-do.patch` | arifi-fork-base | `ecb2e5fc7` | - | - | 4-other-backends/0050-fattn-vec-split-the-turbo-K-dot-at-D-128-to-stop-the.patch |
| 152 | `0152-5-models-server-0001-WIP-add-TurboQuant-KV-cache-typ.patch` | arifi-fork-base | `5c7fb6396` | - | - | 5-models-server/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 153 | `0153-5-models-server-0003-cli-break-interactive-loop-on-s.patch` | arifi-fork-base | `7cc7134e5` | - | - | 5-models-server/0003-cli-break-interactive-loop-on-stdin-EOF-fixes-hot-sp.patch |
| 154 | `0154-5-models-server-0004-tools-expose-TQ3_1S-TQ4_1S-in-q.patch` | arifi-fork-base | `692472db0` | - | - | 5-models-server/0004-tools-expose-TQ3_1S-TQ4_1S-in-quantize-table-and-lla.patch |
| 155 | `0155-5-models-server-0007-llama-support-DeepSeek-V4-tenso.patch` | arifi-fork-base | `6cd8dddda` | - | - | 5-models-server/0007-llama-support-DeepSeek-V4-tensor-split.patch |
| 156 | `0156-5-models-server-0008-llama-mirror-DS4-q_a-kv-down-pr.patch` | arifi-fork-base | `3f80542b5` | - | - | 5-models-server/0008-llama-mirror-DS4-q_a-kv-down-projections-in-tensor-s.patch |
| 157 | `0157-5-models-server-0009-glm-dsa-guard-lightning-indexer.patch` | arifi-fork-base | `b81705266` | - | - | 5-models-server/0009-glm-dsa-guard-lightning-indexer-Hadamard-rotation-wh.patch |
| 158 | `0158-5-models-server-0010-laguna-add-arch-tables-llm_arch.patch` | arifi-fork-base | `2f10753d4` | - | - | 5-models-server/0010-laguna-add-arch-tables-llm_arch-KV-keys-tensors-and-.patch |
| 159 | `0159-5-models-server-0011-laguna-add-model-class-hparams-.patch` | arifi-fork-base | `c3cbe241b` | - | - | 5-models-server/0011-laguna-add-model-class-hparams-wiring-vocab-pre-toke.patch |
| 160 | `0160-5-models-server-0013-laguna-MoE-down-proj-f16-overfl.patch` | arifi-fork-base | `c784b11be` | - | - | 5-models-server/0013-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch |
| 161 | `0161-5-models-server-0014-cuda-first-class-MoE-path-plain.patch` | arifi-fork-base | `0af30cf91` | - | - | 5-models-server/0014-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch |
| 162 | `0162-5-models-server-0015-cuda-sum-MoE-expert-outputs-on-.patch` | arifi-fork-base | `bd5b182cb` | - | - | 5-models-server/0015-cuda-sum-MoE-expert-outputs-on-decode-n_tokens-1.patch |
| 163 | `0163-5-models-server-0016-laguna-deduplicate-definitions-.patch` | arifi-fork-base | `49fa51144` | - | - | 5-models-server/0016-laguna-deduplicate-definitions-vs-the-early-snapshot.patch |
| 164 | `0164-5-models-server-0017-dflash-fix-DSpark-tensor-meta-a.patch` | arifi-fork-base | `6829f6f2c` | - | - | 5-models-server/0017-dflash-fix-DSpark-tensor-meta-accessor-after-laguna-.patch |
| 165 | `0165-5-models-server-0021-common-support-the-DSpark-sidec.patch` | arifi-fork-base | `93ee8e5e1` | - | - | 5-models-server/0021-common-support-the-DSpark-sidecar-resolution-26458.patch |
| 166 | `0166-5-models-server-0037-merge-fix-post-merge-build-and-.patch` | arifi-fork-base | `9037b2844` | - | - | 5-models-server/0037-merge-fix-post-merge-build-and-crash-issues.patch |
| 167 | `0167-5-models-server-0041-moe-cache-route-fit-probing-and.patch` | arifi-fork-base | `145f90a69` | - | - | 5-models-server/0041-moe-cache-route-fit-probing-and-test-session-calls-t.patch |
| 168 | `0168-5-models-server-0048-llama-bench-document-pw-prefetc.patch` | arifi-fork-base | `f4571d0ee` | - | - | 5-models-server/0048-llama-bench-document-pw-prefetch-weights-in-help-and.patch |
| 169 | `0169-5-models-server-0050-llama-bench-remove-stale-prefet.patch` | arifi-fork-base | `89633f681` | - | - | 5-models-server/0050-llama-bench-remove-stale-prefetch-weights-documentat.patch |
| 170 | `0170-5-models-server-0051-moe-cache-fix-auto-asymmetric-M.patch` | arifi-fork-base | `7fd149a1e` | - | - | 5-models-server/0051-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch |
| 171 | `0171-6-tests-build-0001-tests-re-add-test-turbo-quant.c-r.patch` | arifi-fork-base | `289d1bcf1` | - | - | 6-tests-build/0001-tests-re-add-test-turbo-quant.c-round-trip-test.patch |
| 172 | `0172-6-tests-build-0002-tests-port-fork-turbo-backend-and.patch` | arifi-fork-base | `149230524` | - | - | 6-tests-build/0002-tests-port-fork-turbo-backend-and-quantize-fns-test-.patch |
| 173 | `0173-6-tests-build-0003-fix-close-complete-audit-findings.patch` | arifi-fork-base | `486b73741` | - | - | 6-tests-build/0003-fix-close-complete-audit-findings-7-port-regressions.patch |
| 174 | `0174-6-tests-build-0004-laguna-tool-call-whitespace-toler.patch` | arifi-fork-base | `a421e3822` | - | - | 6-tests-build/0004-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch |
| 175 | `0175-6-tests-build-0006-ggml-webgpu-add-support-for-f16-r.patch` | arifi-fork-base | `f76034db4` | - | - | 6-tests-build/0006-ggml-webgpu-add-support-for-f16-repeat-26307.patch |
| 176 | `0176-6-tests-build-0010-tests-restore-DSV4_HC_COMB-eps-sw.patch` | arifi-fork-base | `045ef93e0` | - | - | 6-tests-build/0010-tests-restore-DSV4_HC_COMB-eps-sweep-lost-in-the-244.patch |
| 177 | `0177-6-tests-build-0011-tests-cast-float-printf-args-in-t.patch` | arifi-fork-base | `0b046454e` | - | - | 6-tests-build/0011-tests-cast-float-printf-args-in-test-turbo-quant-arm.patch |
| 178 | `0178-6-tests-build-0012-vulkan-add-the-TQ3_1S-weight-type.patch` | arifi-fork-base | `e706bb5b5` | - | - | 6-tests-build/0012-vulkan-add-the-TQ3_1S-weight-type.patch |
| 179 | `0179-6-tests-build-0013-vulkan-reject-MUL_MAT_ID-for-the-.patch` | arifi-fork-base | `060bebd7f` | - | - | 6-tests-build/0013-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch |
| 180 | `0180-6-tests-build-0014-ggml-cpu-remove-the-per-call-mall.patch` | arifi-fork-base | `711810814` | - | - | 6-tests-build/0014-ggml-cpu-remove-the-per-call-mallocs-from-the-TurboQ.patch |
| 181 | `0181-6-tests-build-0015-vulkan-add-mul_mat_vec_id-for-the.patch` | arifi-fork-base | `e88c632d1` | - | - | 6-tests-build/0015-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch |
| 182 | `0182-6-tests-build-0016-feat-add-Vulkan-TQ4_1S-weight-pip.patch` | arifi-fork-base | `97449e612` | - | - | 6-tests-build/0016-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch |
| 183 | `0183-6-tests-build-0017-tests-port-11-DSv4-shaped-TQ-MUL_.patch` | arifi-fork-base | `06fdbf323` | - | - | 6-tests-build/0017-tests-port-11-DSv4-shaped-TQ-MUL_MAT-cases-from-sync.patch |
| 184 | `0184-6-tests-build-0019-cuda-fix-MoE-cache-capacity-accou.patch` | arifi-fork-base | `8f1fa85f7` | - | - | 6-tests-build/0019-cuda-fix-MoE-cache-capacity-accounting.patch |
| 185 | `0185-6-tests-build-0020-common-preserve-implicit-MoE-cach.patch` | arifi-fork-base | `30e53401b` | - | - | 6-tests-build/0020-common-preserve-implicit-MoE-cache-provider-settings.patch |
| 186 | `0186-6-tests-build-0021-cuda-tune-MoE-cache-CPU-overlap-a.patch` | arifi-fork-base | `a60a9534c` | - | - | 6-tests-build/0021-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 187 | `0187-6-tests-build-0022-cuda-adapt-MoE-cache-admission-to.patch` | arifi-fork-base | `7ee740275` | - | - | 6-tests-build/0022-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 188 | `0188-6-tests-build-0023-cuda-align-MoE-cache-pool-allocat.patch` | arifi-fork-base | `94cba10d7` | - | - | 6-tests-build/0023-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 189 | `0189-6-tests-build-0024-cuda-parallelize-MoE-cache-fills-.patch` | arifi-fork-base | `476394bed` | - | - | 6-tests-build/0024-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch |
| 190 | `0190-6-tests-build-0025-cuda-fuse-cached-MoE-SwiGLU-rows..patch` | arifi-fork-base | `b27983cf3` | - | - | 6-tests-build/0025-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 191 | `0191-6-tests-build-0026-cuda-aggregate-small-MoE-tensors-.patch` | arifi-fork-base | `b6628df21` | - | - | 6-tests-build/0026-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 192 | `0192-6-tests-build-0027-cuda-bound-automatic-MoE-cache-ad.patch` | arifi-fork-base | `45bacf628` | - | - | 6-tests-build/0027-cuda-bound-automatic-MoE-cache-admission.patch |
| 193 | `0193-6-tests-build-0028-cuda-reject-undersized-automatic-.patch` | arifi-fork-base | `28563af4c` | - | - | 6-tests-build/0028-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 194 | `0194-6-tests-build-0029-cuda-enable-multi-token-MoE-cache.patch` | arifi-fork-base | `9efa7d6fb` | - | - | 6-tests-build/0029-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 195 | `0195-6-tests-build-0030-common-surface-MoE-cache-activati.patch` | arifi-fork-base | `b0bae2980` | - | - | 6-tests-build/0030-common-surface-MoE-cache-activation.patch |
| 196 | `0196-6-tests-build-0031-cuda-accelerate-complete-MoE-cach.patch` | arifi-fork-base | `17e56bc51` | - | - | 6-tests-build/0031-cuda-accelerate-complete-MoE-cache-pools.patch |
| 197 | `0197-6-tests-build-0032-cuda-report-oversized-MoE-cache-n.patch` | arifi-fork-base | `311ae06c2` | - | - | 6-tests-build/0032-cuda-report-oversized-MoE-cache-nodes.patch |
| 198 | `0198-6-tests-build-0033-tests-initialise-non-contiguous-t.patch` | arifi-fork-base | `af6a0471d` | - | - | 6-tests-build/0033-tests-initialise-non-contiguous-tensors-row-by-row.patch |
| 199 | `0199-6-tests-build-0034-tests-cover-turbo-KV-flash-attent.patch` | arifi-fork-base | `4fd14bb50` | - | - | 6-tests-build/0034-tests-cover-turbo-KV-flash-attention-at-head-dim-256.patch |
| 200 | `0200-6-tests-build-0037-moe-cache-provider-registry-with-.patch` | arifi-fork-base | `53ba14cc7` | - | - | 6-tests-build/0037-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 201 | `0201-6-tests-build-0038-moe-cache-route-fit-probing-and-t.patch` | arifi-fork-base | `4861e56e1` | - | - | 6-tests-build/0038-moe-cache-route-fit-probing-and-test-session-calls-t.patch |
| 202 | `0202-6-tests-build-0039-moe-cache-route-context-eligibili.patch` | arifi-fork-base | `845dca09c` | - | - | 6-tests-build/0039-moe-cache-route-context-eligibility-probe-and-remain.patch |
| 203 | `0203-6-tests-build-0040-moe-cache-allow-automatic-mode-on.patch` | arifi-fork-base | `dc43cc88d` | - | - | 6-tests-build/0040-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 204 | `0204-6-tests-build-0041-moe-cache-fix-auto-asymmetric-MLA.patch` | arifi-fork-base | `4f2e9e899` | - | - | 6-tests-build/0041-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch |
| 205 | `0205-6-tests-build-0042-test-moe-cache-accept-IGPU-device.patch` | arifi-fork-base | `a333dc741` | - | - | 6-tests-build/0042-test-moe-cache-accept-IGPU-devices-not-just-GPU.patch |
| 206 | `0206-6-tests-build-0043-test-moe-cache-make-cache-shared-.patch` | arifi-fork-base | `133569377` | - | - | 6-tests-build/0043-test-moe-cache-make-cache-shared-budget-UMA-aware-wi.patch |
| 207 | `0207-6-tests-build-0044-test-moe-cache-save-and-restore-t.patch` | arifi-fork-base | `71d6e40b3` | - | - | 6-tests-build/0044-test-moe-cache-save-and-restore-the-CUDA-device-arou.patch |
| 208 | `0208-6-tests-build-0045-tests-tell-ctest-that-77-means-sk.patch` | arifi-fork-base | `94d3be281` | - | - | 6-tests-build/0045-tests-tell-ctest-that-77-means-skip-for-test-moe-cac.patch |
| 209 | `0209-resolved-0009-vulkan-add-mul_mat_vec_id-for-the-Turb.patch` | arifi-fork-base | `097f49a79` | - | - | resolved/0009-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch |
| 210 | `0210-resolved-0011-fix-correct-Vulkan-turbo3-pipeline-wir.patch` | arifi-fork-base | `1c9195f82` | - | - | resolved/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch |
| 211 | `0211-resolved-0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-ga.patch` | arifi-fork-base | `5204b4ac0` | - | - | resolved/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch |
| 212 | `0212-resolved-0005-merge-close-TurboQuant-parity-gaps.pat.patch` | arifi-fork-base | `f1be65178` | - | - | resolved/0005-merge-close-TurboQuant-parity-gaps.patch |
| 213 | `0213-resolved-0036-common-surface-MoE-cache-activation.pa.patch` | arifi-fork-base | `e2398f694` | - | - | resolved/0036-common-surface-MoE-cache-activation.patch |
| 214 | `0214-resolved-0019-vulkan-dispatch-the-TQ-rotated-mul_mm_.patch` | arifi-fork-base | `5e9f9ac0d` | - | - | resolved/0019-vulkan-dispatch-the-TQ-rotated-mul_mm_id-path.patch |
| 215 | `0215-resolved-0010-feat-add-Vulkan-turbo3-KV-cache-pipeli.patch` | arifi-fork-base | `88014c397` | - | - | resolved/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch |
| 216 | `0216-resolved-0004-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `9568085fa` | - | - | resolved/0004-fix-close-complete-audit-findings-7-port-regressions.patch |
| 217 | `0217-resolved-0005-metal-restore-lost-kernel-templates-ad.patch` | arifi-fork-base | `379934ec8` | - | - | resolved/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch |
| 218 | `0218-resolved-0012-metal-implement-DeepSeek-V4-hyper-conn.patch` | arifi-fork-base | `4b3fedf7f` | - | - | resolved/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch |
| 219 | `0219-resolved-0035-cuda-gate-fused-TQ-mul_mat-paths-on-co.patch` | arifi-fork-base | `c13f985dc` | - | - | resolved/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch |
| 220 | `0220-resolved-0036-cuda-disable-fused-TQ3_1S-mul_mat-kern.patch` | arifi-fork-base | `b9715ac05` | - | - | resolved/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch |
| 221 | `0221-resolved-0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HI.patch` | arifi-fork-base | `50c177811` | - | - | resolved/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch |
| 222 | `0222-resolved-0047-sycl-hip-fix-two-build-breaks-in-fork-.patch` | arifi-fork-base | `5f0a08aa7` | - | - | resolved/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch |
| 223 | `0223-resolved-0006-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `04b1dcb74` | - | - | resolved/0006-fix-close-complete-audit-findings-7-port-regressions.patch |
| 224 | `0224-resolved-0022-DeepseekV4-MTP-DSpark-25784.patch.patch` | arifi-fork-base | `19fac938f` | - | - | resolved/0022-DeepseekV4-MTP-DSpark-25784.patch |
| 225 | `0225-resolved-0023-dflash-fix-merge-artifacts-from-upstre.patch` | arifi-fork-base | `a642aee7b` | - | - | resolved/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch |
| 226 | `0226-resolved-0029-server-harden-shared-draft-device-plac.patch` | arifi-fork-base | `c3f6c993c` | - | - | resolved/0029-server-harden-shared-draft-device-placement.patch |
| 227 | `0227-resolved-0025-fix-server-batch-restored-checkpoint-p.patch` | arifi-fork-base | `e78b3ca2e` | - | - | resolved/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch |
| 228 | `0228-resolved-0026-cuda-rework-MoE-expert-cache-execution.patch` | arifi-fork-base | `9da482fd6` | - | - | resolved/0026-cuda-rework-MoE-expert-cache-execution.patch |
| 229 | `0229-resolved-0033-server-account-for-MTP-placement-in-fi.patch` | arifi-fork-base | `bb312d715` | - | - | resolved/0033-server-account-for-MTP-placement-in-fit-reservations.patch |
| 230 | `0230-resolved-0035-cuda-reject-undersized-automatic-MoE-c.patch` | arifi-fork-base | `f6588b8e2` | - | - | resolved/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 231 | `0231-resolved-0038-moe-cache-add-moe-cache-soft-mode-with.patch` | arifi-fork-base | `f42422d44` | - | - | resolved/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch |
| 232 | `0232-resolved-0039-model-Muse-Glimmer-Support-26841.patch.patch` | arifi-fork-base | `6e10fc420` | - | - | resolved/0039-model-Muse-Glimmer-Support-26841.patch |
| 233 | `0233-resolved-0043-moe-cache-audit-fixes-H1-F1-F2-A1.patc.patch` | arifi-fork-base | `e4f8f028a` | - | - | resolved/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch |
| 234 | `0234-resolved-0044-moe-cache-restore-moe_cache-params-on-.patch` | arifi-fork-base | `d7b3dbf7d` | - | - | resolved/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch |
| 235 | `0235-resolved-0046-llama-remove-dead-MSA-indexer-scaffold.patch` | arifi-fork-base | `2dd362d5a` | - | - | resolved/0046-llama-remove-dead-MSA-indexer-scaffolding.patch |
| 236 | `0236-resolved-0047-moe-cache-add-logging-at-silent-failur.patch` | arifi-fork-base | `9311a7acc` | - | - | resolved/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch |
| 237 | `0237-resolved-0052-cuda-fit-fix-two-Werror-build-failures.patch` | arifi-fork-base | `0f966e10c` | - | - | resolved/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch |
| 238 | `0238-resolved-0005-test-fix-some-CI-errors-26415.patch.patch` | arifi-fork-base | `a7793fb24` | - | - | resolved/0005-test-fix-some-CI-errors-26415.patch |
| 239 | `0239-resolved-remaining-conflict-markers.patch` | arifi-fork-base | `693bf2e98` | - | - | resolved/remaining-conflict-markers |
| 240 | `0240-layer7-0001-WIP-add-TurboQuant-KV-cache-types-turbo3.patch` | arifi-fork-base | `951d4afda` | - | - | layer7/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 241 | `0241-layer7-0002-Update-GGMLQuantizationType-and-LlamaFil.patch` | arifi-fork-base | `c5f7aa4cd` | - | - | layer7/0002-Update-GGMLQuantizationType-and-LlamaFileType-enums-.patch |
| 242 | `0242-layer7-0003-ggml-port-TurboQuant-core-quant-types-an.patch` | arifi-fork-base | `2b8222b7b` | - | - | layer7/0003-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 243 | `0243-layer7-0005-docs-add-rebase-plan-and-TurboQuant-reci.patch` | arifi-fork-base | `a9b6fe8c6` | - | - | layer7/0005-docs-add-rebase-plan-and-TurboQuant-recipes.patch |
| 244 | `0244-layer7-0007-gguf-py-dedupe-stacked-merge-artifacts-i.patch` | arifi-fork-base | `e3b10a63c` | - | - | layer7/0007-gguf-py-dedupe-stacked-merge-artifacts-in-constants..patch |
| 245 | `0245-layer7-0008-docs-add-KV-cache-quantization-guide-upd.patch` | arifi-fork-base | `0a6c2b275` | - | - | layer7/0008-docs-add-KV-cache-quantization-guide-update-rebase-p.patch |
| 246 | `0246-layer7-0009-vulkan-port-fork-241-wave64-ballot-fix-c.patch` | arifi-fork-base | `c42b56de5` | - | - | layer7/0009-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch |
| 247 | `0247-layer7-0010-merge-close-TurboQuant-parity-gaps.patch.patch` | arifi-fork-base | `286cbbf2d` | - | - | layer7/0010-merge-close-TurboQuant-parity-gaps.patch |
| 248 | `0248-layer7-0011-metal-restore-lost-kernel-templates-add-.patch` | arifi-fork-base | `3872af19a` | - | - | layer7/0011-metal-restore-lost-kernel-templates-add-offline-shad.patch |
| 249 | `0249-layer7-0014-laguna-tool-call-whitespace-tolerance-do.patch` | arifi-fork-base | `70aa80189` | - | - | layer7/0014-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch |
| 250 | `0250-layer7-0019-fix-apply-rebase-audit-fixes-stack-overf.patch` | arifi-fork-base | `65049862f` | - | - | layer7/0019-fix-apply-rebase-audit-fixes-stack-overflow-quality-.patch |
| 251 | `0251-layer7-0020-docs-add-TurboQuant-project-overview-to-.patch` | arifi-fork-base | `d91a64dec` | - | - | layer7/0020-docs-add-TurboQuant-project-overview-to-AGENTS.md-an.patch |
| 252 | `0252-layer7-0021-cuda-rework-MoE-expert-cache-execution.p.patch` | arifi-fork-base | `fe235ab50` | - | - | layer7/0021-cuda-rework-MoE-expert-cache-execution.patch |
| 253 | `0253-layer7-0022-docs-update-MoE-cache-validation.patch.patch` | arifi-fork-base | `2ddaf21ff` | - | - | layer7/0022-docs-update-MoE-cache-validation.patch |
| 254 | `0254-layer7-0023-common-preserve-implicit-MoE-cache-provi.patch` | arifi-fork-base | `2a0ed2605` | - | - | layer7/0023-common-preserve-implicit-MoE-cache-provider-settings.patch |
| 255 | `0255-layer7-0024-server-harden-shared-draft-device-placem.patch` | arifi-fork-base | `62156922b` | - | - | layer7/0024-server-harden-shared-draft-device-placement.patch |
| 256 | `0256-layer7-0025-cuda-tune-MoE-cache-CPU-overlap-automati.patch` | arifi-fork-base | `7dd14d193` | - | - | layer7/0025-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 257 | `0257-layer7-0026-cuda-adapt-MoE-cache-admission-to-device.patch` | arifi-fork-base | `998a4679b` | - | - | layer7/0026-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 258 | `0258-layer7-0027-cuda-align-MoE-cache-pool-allocation-wit.patch` | arifi-fork-base | `b9c77c22f` | - | - | layer7/0027-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 259 | `0259-layer7-0028-docs-update-MoE-cache-validation.patch.patch` | arifi-fork-base | `f5d663faa` | - | - | layer7/0028-docs-update-MoE-cache-validation.patch |
| 260 | `0260-layer7-0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch.patch` | arifi-fork-base | `f6e292523` | - | - | layer7/0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 261 | `0261-layer7-0030-cuda-aggregate-small-MoE-tensors-into-ca.patch` | arifi-fork-base | `5e0a9ecfb` | - | - | layer7/0030-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 262 | `0262-layer7-0031-cuda-bound-automatic-MoE-cache-admission.patch` | arifi-fork-base | `04c7f4e48` | - | - | layer7/0031-cuda-bound-automatic-MoE-cache-admission.patch |
| 263 | `0263-layer7-0032-cuda-reject-undersized-automatic-MoE-cac.patch` | arifi-fork-base | `ca10593ee` | - | - | layer7/0032-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 264 | `0264-layer7-0033-cuda-enable-multi-token-MoE-cache-in-for.patch` | arifi-fork-base | `4c3c4dd32` | - | - | layer7/0033-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 265 | `0265-layer7-0034-common-surface-MoE-cache-activation.patc.patch` | arifi-fork-base | `26d6b3d94` | - | - | layer7/0034-common-surface-MoE-cache-activation.patch |
| 266 | `0266-layer7-0035-docs-fix-MoE-cache-benchmark-verbosity.p.patch` | arifi-fork-base | `fee15f5ba` | - | - | layer7/0035-docs-fix-MoE-cache-benchmark-verbosity.patch |
| 267 | `0267-layer7-0036-cuda-accelerate-complete-MoE-cache-pools.patch` | arifi-fork-base | `6f6428621` | - | - | layer7/0036-cuda-accelerate-complete-MoE-cache-pools.patch |
| 268 | `0268-layer7-0037-cuda-report-oversized-MoE-cache-nodes.pa.patch` | arifi-fork-base | `d20fd194f` | - | - | layer7/0037-cuda-report-oversized-MoE-cache-nodes.patch |
| 269 | `0269-layer7-0038-cuda-clarify-MoE-cache-session-diagnosti.patch` | arifi-fork-base | `9245f9689` | - | - | layer7/0038-cuda-clarify-MoE-cache-session-diagnostics.patch |
| 270 | `0270-layer7-0039-docs-record-MoE-cache-workload-convergen.patch` | arifi-fork-base | `72ea4f65b` | - | - | layer7/0039-docs-record-MoE-cache-workload-convergence.patch |
| 271 | `0271-layer7-0040-cuda-report-MoE-cache-pair-residency.pat.patch` | arifi-fork-base | `3039f50b9` | - | - | layer7/0040-cuda-report-MoE-cache-pair-residency.patch |
| 272 | `0272-layer7-0041-docs-document-what-the-test-suites-do-an.patch` | arifi-fork-base | `7357c811c` | - | - | layer7/0041-docs-document-what-the-test-suites-do-and-do-not-cov.patch |
| 273 | `0273-layer7-0044-moe-cache-provider-registry-with-per-sch.patch` | arifi-fork-base | `df36d27e6` | - | - | layer7/0044-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 274 | `0274-layer7-0046-moe-cache-allow-automatic-mode-on-a-sing.patch` | arifi-fork-base | `ac1566425` | - | - | layer7/0046-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 275 | `0275-layer7-0048-docs-document-INFO-level-MoE-cache-disab.patch` | arifi-fork-base | `f0e9b1573` | - | - | layer7/0048-docs-document-INFO-level-MoE-cache-disable-reasons-i.patch |
| 276 | `0276-layer7-0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CAC.patch` | arifi-fork-base | `5d470b2b1` | - | - | layer7/0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CACHE.md-and-re.patch |
| 277 | `0277-layer7-0050-docs-correct-MOE-CACHE.md-against-the-im.patch` | arifi-fork-base | `2b9175384` | - | - | layer7/0050-docs-correct-MOE-CACHE.md-against-the-implementation.patch |
| 278 | `0278-layer7-0051-ggml-split-the-turbo3-group-size-declara.patch` | arifi-fork-base | `ff72da4fe` | - | - | layer7/0051-ggml-split-the-turbo3-group-size-declaration-from-it.patch |
| 279 | `0279-resolve-8-committed-conflict-markers-on-merit.patch` | arifi-fork-base | `a66d44da4` | - | - | resolve 8 committed conflict markers on merit |
| 280 | `0280-fix-the-merge-so-it-builds-5-files-6-defects.patch` | arifi-fork-base | `02cc66ec9` | - | - | fix the merge so it builds: 5 files, 6 defects |
| 281 | `0281-merge-repairs-seven-defect-classes-and-one-the-compi.patch` | arifi-fork-base | `3345c796d` | - | - | merge repairs: seven defect classes, and one the compiler could never have reported |
| 282 | `0282-ROCmFPX-on-Vulkan-the-shader-half-ported.-NOT-YET-WO.patch` | arifi-fork-base | `f20594cd1` | - | - | ROCmFPX on Vulkan: the shader half, ported. NOT YET WORKING - it segfaults in execution. |
| 283 | `0283-currency-register-ALL-SEVEN-forks-so-nothing-can-dri.patch` | arifi-fork-base | `0e58a39f9` | - | - | currency: register ALL SEVEN forks, so nothing can drift unseen again |
| 284 | `0284-series-base-and-the-branch-it-describes-are-ONE-pair.patch` | arifi-fork-base | `52ea962c4` | - | - | series: base and the branch it describes are ONE pair, and the linearity rule is now a gate |
| 285 | `0285-hygiene-the-code-review-skill-taught-a-type-ID-map-t.patch` | arifi-fork-base | `1167b0d39` | - | - | hygiene: the code-review skill taught a type-ID map the arbitration deleted, and the UI was fetched from a moving tag |
| 286 | `0286-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | ternary-g128 | `838828ebe` | - | - | vulkan: ask the pipelines whether they exist, not the type id |
| 287 | `0287-contract-tq3-is-the-first-family-we-must-renumber-so.patch` | arifi-fork-base | `3ff3caded` | - | - | contract: tq3 is the first family we must renumber, so write down why before anyone ports it |
| 288 | `0288-sources-honest-pins-turboquant-re-pinned-off-a-dead-.patch` | arifi-fork-base | `3b089d8d6` | - | - | sources: honest pins - turboquant re-pinned off a dead lineage, tq3 main-understates note REFUTED, five type-id contract collisions recorded (lane-143) |
| 289 | `0289-sources-lane-143-self-refutation-the-turbo3-ODR-defe.patch` | arifi-fork-base | `5390271f3` | - | - | sources: lane-143 self-refutation - the turbo3 ODR 'defect' does not exist in our tree (GGML_API already carries extern on all four branches); prisml pin REVERTED to keep the BEHIND signal live; FLAG E severity re-derived from loader code |
| 290 | `0290-sources-r2-checker-BLOCK-F1-F6-ciru-160-was-145-comm.patch` | arifi-fork-base | `4ec84e216` | - | - | sources r2 (checker BLOCK F1-F6): ciru 160 was 145 commits of rocmfpx history - corrected to a 7-15 bracket, no-ancestor premise refuted, GOAL-3 ciru review performed, rocmfpx.ref revert, prisml contradiction removed, FLAG E sensitivity added |
| 291 | `0291-series-point-base.ref-at-the-LINEAR-branch-recut-lin.patch` | arifi-fork-base | `bcb0fc6ce` | - | - | series: point base.ref at the LINEAR branch - recut-linear/2026-08-17 is the byte-identical flatten of the blessed union branch, so the series is replayable at last (lane-147) |
| 292 | `0292-series-disclose-in-the-record-itself-that-the-flatte.patch` | arifi-fork-base | `75663c142` | - | - | series: disclose in the record itself that the flatten absorbed 17 empty commits - content unchanged, history count changed (lane-147 checker finding 4) |
| 293 | `0293-ggml-port-the-tq3-TQ3_4S-family-codec-at-ids-48-51-T.patch` | arifi-fork-base | `9eeb8fddd` | - | - | ggml: port the tq3 TQ3_4S family codec at ids 48-51 (TYPE-ID-ALLOCATION 3.1.1) - Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb |
| 294 | `0294-tests-cover-the-tq3-family-in-test-quantize-fns-and-.patch` | arifi-fork-base | `f9184f428` | - | - | tests: cover the tq3 family in test-quantize-fns and test-backend-ops (kilobyte-scale proof before the 13 GB run) |
| 295 | `0295-contract-tq3-rows-48-51-implemented-behind-GGML_ARIF.patch` | arifi-fork-base | `48c09a1d2` | - | - | contract: tq3 rows 48-51 implemented behind GGML_ARIFI_TURBO_WEIGHT_QUANTS, plus the three corrections the port forces (their 200 = our TURBO2_0; six ids, four destinations; token_embd is Q6_K) |
| 296 | `0296-tests-tq3_4s-tq3_4se-skipped-in-test-quantize-fns-wi.patch` | arifi-fork-base | `17bb3e8c2` | - | - | tests: tq3_4s/tq3_4se skipped in test-quantize-fns with the measured reason (E3M5 scale caps at 0.492, harness data needs 1.4); tq3_0 given documented 3.5bpw bounds |
| 297 | `0297-tools-gguf-retag-tq3-lands-LAST-after-the-coherence-.patch` | arifi-fork-base | `2a7537571` | - | - | tools: gguf-retag-tq3 - lands LAST, after the coherence proof, per TYPE-ID-ALLOCATION 3.1.1. Decides ambiguous id 46 by measured byte span; five refusals, no in-place path |
| 298 | `0298-checker-fixes-choose_index-restored-verbatim-inverte.patch` | arifi-fork-base | `8e2ae2fd1` | - | - | checker fixes: choose_index restored verbatim (inverted tie-break + 2 drifted constants, encoder-only); tq3 numeric block gives TQ3_4S real proof and pins every dot mapping; drop redundant GGML_API redecl |
| 299 | `0299-retag-verify-geometry-per-TENSOR-for-every-mapped-id.patch` | arifi-fork-base | `d1fe739cd` | - | - | retag: verify geometry per TENSOR for every mapped id, not a file-level vote on 46 only - fixes silent rewrite of mixed/ambiguous files and of unguarded 36/200; guard empty and overlapping tables |
| 300 | `0300-retag-README-per-tensor-geometry-check-for-every-map.patch` | arifi-fork-base | `762dd9ca8` | - | - | retag README: per-tensor geometry check for every mapped id, ten refusals, byte-level assertion |
| 301 | `0301-lane-144-GOAL-2-step-1-TQ3_4S-id-48-Vulkan-port-dequ.patch` | arifi-fork-base | `85c6500a6` | - | - | lane-144 GOAL 2 step 1: TQ3_4S (id 48) Vulkan port - dequant + mat-vec + mat-vec_id shaders, wired |
| 302 | `0302-lane-144-sweep-ROCmFP4-ROCmFPX-in-test-backend-ops-a.patch` | arifi-fork-base | `38cb15b5a` | - | - | lane-144: sweep ROCmFP4/ROCmFPX in test-backend-ops all_types (six identifiers, never listed before) |
| 303 | `0303-lane-144-CHECKER-FIX-TQ3_4S-was-missing-from-the-MUL.patch` | arifi-fork-base | `ba9e60dea` | - | - | lane-144 CHECKER FIX: TQ3_4S was missing from the MUL_MAT_ID staging-overflow guard |
| 304 | `0304-lane-145-Q6_0_ROCMFPX-Vulkan-fix-implement-the-6-bit.patch` | arifi-fork-base | `1f5683957` | - | - | lane-145: Q6_0_ROCMFPX Vulkan fix - implement the 6-bit unpack (block struct was 34B vs CPU 26B; qs read as whole signed bytes). 60/60 MUL_MAT + 24/24 GET_ROWS on Vulkan0, was 50/60 + 20/24. |
| 305 | `0305-lane-145-checker-finding-7-delete-block_rocmfpx_fp6_.patch` | arifi-fork-base | `56af5c640` | - | - | lane-145 checker finding 7: delete block_rocmfpx_fp6_packed16 + its A_TYPE_PACKED16 define (26B block has no valid uint16 view; nothing reads it). Re-verified 60/60 MUL_MAT, 24/24 GET_ROWS, 12/12 MUL_MAT_ID. |
| 306 | `0306-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | ternary-g128 | `834917952` | - | - | vulkan: ask the pipelines whether they exist, not the type id (SET_ROWS + CPY) |
| 307 | `0307-test-backend-ops-cover-the-CPY-types-supports_op-nam.patch` | arifi-fork-base | `8421a64d7` | - | - | test-backend-ops: cover the CPY types supports_op names that all_types omits |
| 308 | `0308-lane-148-defect-A-ROOT-FIX-Vulkan-turbo3-centroid-LU.patch` | arifi-fork-base | `c6c2bd2f3` | - | - | lane-148 defect A ROOT FIX: Vulkan turbo3 centroid LUT was stale at 5 sites - CPY turbo3->f32 ERR 8.4302e-5 -> OK. CPU/CUDA/Metal/SYCL all carry CENTROIDS_3BIT (-0.190207,-0.118786,-0.066822,-0.021663); Vulkan alone carried a table whose midpoints do not match nearest_centroid_3bit(). copy_to_quant.comp's TM decision boundaries were derived from the stale table and are corrected too. |
| 309 | `0309-lane-148-defect-B-ROOT-FIX-FA_TYPE_TURBO2-3-4_0-GLSL.patch` | arifi-fork-base | `a384a6d2c` | - | - | lane-148 defect B ROOT FIX: FA_TYPE_TURBO2/3/4_0 GLSL spec-constant ids were stale (43/44/47) while enum ggml_type puts them at 200/201/202. Host passes the raw enum, so both switches fell through: fa_block_elems() returned its 1u default and dequantize4() returned vec4(0), zeroing V and the whole FA output. FLASH_ATTN_EXT -p turbo 0/1728 -> 1728/1728. |
| 310 | `0310-lane-148-F-69-sibling-coverage-test-backend-ops-FLAS.patch` | arifi-fork-base | `53a0cf264` | - | - | lane-148 F-69 sibling coverage: test-backend-ops FLASH_ATTN_EXT now covers TURBO2_0 too. Vulkan supports_op has always claimed turbo2 for FA but no test case existed, so its stale FA_TYPE id was invisible. 1728 -> 2592 cases, all PASS. |
| 311 | `0311-lane-148-checker-findings-4-5-FIXED-turbo4-SET_ROWS-.patch` | arifi-fork-base | `26d061cc8` | - | - | lane-148 checker findings 4+5 FIXED: turbo4 SET_ROWS was 0/21 declined behind a green 'Backend Vulkan0: OK' banner and turbo2 had no SET_ROWS case at all. Root cause: no cpy_turbo{2,4}_0_f32 copy-from-quant shader, so the tests' read-back leg declined. Added turbo2/turbo4 dequantize()/dequantize4()/get_dm(), the two spv+pipeline registrations, both supports_op and the DISPATCH switch (a third hand-synced pair - supports_op alone aborts with 'Missing CPY op'), and a test_set_rows_turbo2 class. SET_ROWS_TURBO2/3/4 now 21/21 EXECUTED each. |
| 312 | `0312-lane-149-F-110-guard-rung-b-arifi_sync_check.py-CMak.patch` | arifi-fork-base | `c107494bd` | - | - | lane-149 F-110 guard (rung b): arifi_sync_check.py + CMake wiring + 16 ARIFI-SYNC markers. Checks 49 LUT pairs, 16 FA_TYPE ids, 35 QUANT_K block sizes, 4 type-list SETs, FA K/V coverage; unregistered mirrors FAIL |
| 313 | `0313-lane-149-F-112-F-109-harness-guards-NOT_SUPPORTED-ca.patch` | ternary-g128 | `7248dfe7e` | - | - | lane-149 F-112 + F-109 harness guards: NOT_SUPPORTED cases counted and named, zero-executed run now FAILS; all_types coverage-parity assert with reason-bearing waivers; Q2_0_G128 added to all_types |
| 314 | `0314-lane-149-F-109-payoff-q2_0_g128-SET_ROWS-tie-roundin.patch` | ternary-g128 | `329642ca7` | - | - | lane-149 F-109 payoff: q2_0_g128 SET_ROWS tie-rounding fixed. GLSL round() breaks .5 ties implementation-chosen; CPU roundf is half-away-from-zero. 8 f16 failures -> 2, ERR 5.1e-4 -> 1.7e-5 |
| 315 | `0315-lane-149-checker-driven-hardening-of-arifi-sync-chec.patch` | arifi-fork-base | `7f9fd898f` | - | - | lane-149 checker-driven hardening of arifi-sync-check: FA_TYPE/QUANT_K literal spellings, ordinal (not line) pins, per-definition completeness, any array type, recursive shader walk, scalar-mirror class, expected-count assertions |
| 316 | `0316-lane-149-F-112-second-half-checker-finding-11-per-op.patch` | arifi-fork-base | `b277882c7` | - | - | lane-149 F-112 second half (checker finding 11): per-op tallies name every op whose cases were ALL declined, so a zero-coverage family cannot hide inside a green full sweep |
| 317 | `0317-lane-150-step-1-TQ4_1S-read-back-leg-wired-CPY-tq4_1.patch` | arifi-fork-base | `a48d3e5c5` | - | - | lane-150 step 1: TQ4_1S read-back leg wired - CPY(tq4_1s,f32) now EXECUTES, 291/291 -> 293/293 |
| 318 | `0318-lane-150-step-2-TQ4_1S-write-leg-EXECUTES-SET_ROWS_T.patch` | arifi-fork-base | `491d1dac8` | - | - | lane-150 step 2: TQ4_1S write leg EXECUTES - SET_ROWS_TQ4_1S 17 cases/0 executed -> 17/17 PASS, and the inherited 5.0 waive is retired |
| 319 | `0319-lane-150-step-3-root-fix-for-the-hole-that-hid-the-w.patch` | arifi-fork-base | `32cdb2b40` | - | - | lane-150 step 3 (root fix for the hole that hid the whole class): per-op x TYPE zero-coverage tally |
| 320 | `0320-lane-150-checker-driven-fixes-type_bucket-prefix-bou.patch` | arifi-fork-base | `3865f3e52` | - | - | lane-150 checker-driven fixes: type_bucket prefix-boundary bug + dispatch block-size assert + the tallys limits named in code |
| 321 | `0321-lane-151-GOAL-6-hygiene-drop-648-tracked-_lane-out-d.patch` | arifi-fork-base | `0dddb2321` | - | `GGML_ARIFI_VNNI_REPACK` | lane-151 GOAL 6 hygiene: drop 648 tracked _lane-out debris files (incl. an embedded shadow.git with commit receipts) and gitignore the dir; genericize estate paths in docs/, licenses/, tools/gguf-retag-tq3 |
| 322 | `0322-lane-151-GOAL-9-reserve-tq3-s-three-destination-less.patch` | arifi-fork-base | `3337f1ef8` | - | - | lane-151 GOAL 9: reserve tq3's three destination-less serialized ids (37 TQ3_4SV -> 52, 31 TQ3_1S_AP1 -> 53, 44 TQ3_1S -> 54) as RESERVED-not-implemented in docs/TYPE-ID-ALLOCATION.md 3.1.2; geometry marked NOT DERIVED, no enumerators added, retag-tool refusals unchanged |
| 323 | `0323-lane-151-GOALs-6-7-README-refreshed-to-b10453-canoni.patch` | arifi-fork-base | `6ffd79fd6` | - | - | lane-151 GOALs 6+7: README refreshed to b10453 canonical arifi-main + the seven-ingested-sources table (two honest zeroes named); tq3 and ciru credits added; licenses/README.md gains the full Taken-from audit incl. the tq3 license GAP; UPDATE-RUNBOOK.md written (O(delta) recipe, THREE-flag configure, guards, the standard) |
| 324 | `0324-lane-151-GOAL-2-sources.json-base.ref-arifi-main-the.patch` | arifi-fork-base | `471532254` | - | `GGML_ARIFI_VNNI_REPACK` | lane-151 GOAL 2: sources.json base.ref -> arifi/main (the canonical branch is now the series base) + series regen over the full canonical range: 323 patches, series check PASS, replay onto 4df29be4f identical outside patches/series |
| 325 | `0325-lane-151-GOAL-5-gitignore-.tokensave-the-fork-repo-n.patch` | arifi-fork-base | `55806de75` | - | - | lane-151 GOAL 5: gitignore .tokensave/ - the fork repo now carries its own code graph (63344 nodes, 154451 edges over 3407 files) and the index is per-machine, never committed |
| 326 | `0326-lane-151-CHECKER-ROOT-FIX-series-check-now-asserts-e.patch` | arifi-fork-base | `e46ec904a` | - | - | lane-151 CHECKER ROOT FIX: series check now asserts every patch is a COMMITTED BLOB, not just a file on disk. The byte-compare ran against the working directory, so untracked patches made it green while a fresh clone would die at git am - the defect that hid 33 missing patches twice. Help text corrected to match what the code does |
| 327 | `0327-lane-151-CHECKER-FIXES-docs-UPDATE-RUNBOOK-4.5-no-lo.patch` | arifi-fork-base | `eb60ac543` | - | - | lane-151 CHECKER FIXES (docs): UPDATE-RUNBOOK 4.5 no longer asserts a self-containment standard nothing on this estate meets - the four MinGW runtime DLLs are named and staged, and the measured stripped-PATH failure is recorded as identical for the raw build tree and the previous engine. licenses/README.md audit header stops claiming a purely mechanical trailer derivation: the tq3 row is a SUBJECT-line citation git never parses as a trailer, which is exactly why a trailer-only sweep would have missed the one real license gap |
| 328 | `0328-lane-151-HQ-DIRECTIVE-RETAIN-the-licenses-instead-of.patch` | arifi-fork-base | `f7eaf83a4` | - | - | lane-151 HQ DIRECTIVE: RETAIN the licenses instead of flagging them. tq3-MIT, prisml-MIT and ciru-MIT extracted verbatim from this repo's OWN fetched remotes (git show refs/remotes/<r>/<head>:LICENSE). REFUTES my own GOAL-6 finding and a year-old one: I wrote 'no clone remains on the estate' and Phase 0 wrote 'PrismML has no LICENSE' - both were absence claims checked against the wrong artifact while the remotes sat fetched in this very repository. All seven registered sources audited at once. thecodacus STAYS open: no remote, no text, nothing invented |
| 329 | `0329-licenses-thecodacus-MIT-retained-from-the-public-Git.patch` | arifi-fork-base | `679ceb044` | - | - | licenses: thecodacus-MIT retained from the public GitHub fork - the last open attribution CLOSED (no local remote != no source; President's correction 2026-08-18) |
| 330 | `0330-lane-152-GOAL-1-upstream-bump-b10453-b10481-the-FIRS.patch` | arifi-fork-base | `800ce0e39` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | lane-152 GOAL 1: upstream bump b10453 -> b10481, the FIRST live execution of UPDATE-RUNBOOK 2. 348 patches replayed onto 25ae3a9b3, 4 conflicts resolved by hand, series regenerated to 329, zero merges. |
| 331 | `0331-lane-152-what-the-FIRST-live-run-of-UPDATE-RUNBOOK-s.patch` | arifi-fork-base | `6a96b5620` | - | - | lane-152: what the FIRST live run of UPDATE-RUNBOOK section 2 corrected - one build break, two arifi_sync defects, the runbook itself, and the last license row |
| 332 | `0332-lane-152-GOAL-2-batch-1-ciru-s-Vulkan-SPIRV-Headers-.patch` | arifi-fork-base | `709879d52` | ciru@8ffb2cd5a9b9e7eecc49335e0c79291a0df81d65, tq3@b1dd18410620bbf16909444d95ee2f9c10015b085, rocmfpx@8e6277f855df2a27ce072525ed19f3adc4138c47 | - | lane-152 GOAL 2 batch 1: ciru's Vulkan SPIRV-Headers fallback, tq3's f32 hybrid-SSM state gates, rocmfpx's -ffast-math guard |
| 333 | `0333-lane-152-GOAL-2-batch-2-three-lfm2-pre-tokenizer-has.patch` | arifi-fork-base | `cf45b4a38` | rocmfpx@95bb4c798ee8c571e6b34ba4dd0ce15f4c2b9232 | - | lane-152 GOAL 2 batch 2: three lfm2 pre-tokenizer hashes that only rocmfpx had - and the reason they must be pre-computed |
| 334 | `0334-lane-152-pins-THREE-advance-with-reviews-upstream-ro.patch` | arifi-fork-base | `5118405f4` | - | - | lane-152 pins: THREE advance with reviews (upstream, rocmfpx, tq3), TWO deliberately stay BEHIND (prisml, ciru) |
| 335 | `0335-lane-152-checker-F7-the-old-base-is-tag-b10454-not-b.patch` | arifi-fork-base | `5e05d75a1` | - | - | lane-152 checker F7: the old base is tag b10454, not b10453 - correction of record |
| 336 | `0336-repo-env.json-WI-1540-tail-the-engine-repo-s-env-man.patch` | arifi-fork-base | `2682ce141` | - | - | repo-env.json (WI-1540 tail): the engine repo's env manifest - PowerShell-only law (F-107), three-flag configure (F-111), cmd.exe redirection (F-108), type-id FLAG E, series discipline, runbook pointer |
| 337 | `0337-WI-1580-sub-2-UPDATE-RUNBOOK-section-7-the-4.5-line-.patch` | arifi-fork-base | `40b2af9c8` | - | - | WI-1580 sub-2: UPDATE-RUNBOOK section 7 - the 4.5 line was stale. lane-153 staged models/engines/arifi-b10481-union/ from a verified build of canonical, smoked from the staged copies, and flipped current-state.json + artifact-catalog.json, so 4.5 HAS now been executed once, by hand. The bump distinction is KEPT because it is still true: no bump has ever driven staging end to end (lane-152's bump refused at leg 2). Narrow edit - the section-2 and section-3 paragraphs around it are untouched. Note for the record: the lane-153 REPORT cites this text at UPDATE-RUNBOOK.md:238-244; it actually lives at :308-323 |
| 338 | `0338-vulkan-land-the-MoE-expert-cache-provider-files-VK-M.patch` | ternary-g128 | `85c1df0af` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: land the MoE expert cache provider files (VK-MOE-CACHE step 1 of 2 - files, not yet wired) |
| 339 | `0339-graph-derive-the-V-unpad-head-count-from-the-attenti.patch` | arifi-fork-base | `547cd7bd3` | TheTom/llama-cpp-turboquant@f0420e056 | - | graph: derive the V-unpad head count from the attention output tensor, not from hparams |
| 340 | `0340-vulkan-wire-the-MoE-expert-cache-provider-into-the-b.patch` | ternary-g128 | `56ce45f4b` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: wire the MoE expert cache provider into the build (VK-MOE-CACHE step 2 of 2) |
| 341 | `0341-vulkan-complete-the-MoE-cache-port-the-56-provider-h.patch` | ternary-g128 | `e948f7ea3` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: complete the MoE-cache port - the +56 provider-header addition, the four handle accessors, and the include-order ROOT FIX the link error exposed |
| 342 | `0342-sources-advance-the-turboquant-pin-f6124e914-7ebcbb0.patch` | arifi-fork-base | `faebd6c1b` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: advance the turboquant pin f6124e914 -> 7ebcbb0b6 with pin_review_2026_08_18_lane154 |
| 343 | `0343-off-rig-backends-land-the-12-CUDA-Metal-SYCL-HIP-row.patch` | arifi-fork-base | `9b7316610` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | off-rig backends: land the 12 CUDA/Metal/SYCL/HIP rows unbuilt, per the HQ ruling of 2026-08-18 |
| 344 | `0344-sources-pin_review-names-the-12-off-rig-shas-and-the.patch` | arifi-fork-base | `bf37bea8e` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: pin_review names the 12 off-rig shas and their BUILD-UNPROVEN status (HQ condition 3) |
| 345 | `0345-sources-correct-the-pin_review-the-contested-12-12-a.patch` | arifi-fork-base | `c5a4450f6` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: correct the pin_review - the contested 12/12 apply-conflict count is no longer the justification (checker F1) |
| 346 | `0346-sources-pin_review-records-the-apply-count-claim-as-.patch` | arifi-fork-base | `5a004c1f6` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: pin_review records the apply-count claim as WRONG, not disputed (checker F1, resolved against me) |
| 347 | `0347-base-b10481-25ae3a9b3-b10488-9d77fa172-series-regen-.patch` | arifi-fork-base | `d8c294b6f` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | base: b10481/25ae3a9b3 -> b10488/9d77fa172 + series regen 346 (lane-156 upstream bump) |
| 348 | `0348-moe-cache-heat-protected-eviction-keeps-hot-experts-.patch` | arifi-fork-base | `7156bd797` | llama-cpp-turboquant@730cc87ed (+ its doc follow-up cef75bf06) | - | moe-cache: heat-protected eviction keeps hot experts resident (turboquant tail) |
| 349 | `0349-tests-initialise-non-contiguous-tensors-row-by-row-t.patch` | arifi-fork-base | `0cbc6238d` | llama-cpp-turboquant@f58ee0e97 | - | tests: initialise non-contiguous tensors row by row (turboquant row f58ee0e97) |
| 350 | `0350-sources-advance-the-turboquant-pin-7ebcbb0b6-d14e368.patch` | arifi-fork-base | `28dabf813` | - | - | sources: advance the turboquant pin 7ebcbb0b6 -> d14e36827 with a dated lane-156 pin_review |
| 351 | `0351-sources-prisml-pin-HELD-deliberately-with-a-sharper-.patch` | arifi-fork-base | `a2f2cdd27` | - | - | sources: prisml pin HELD deliberately with a sharper reason + the v6 K-cache-mean-center verdict (lane-156 GOAL 4) |
| 352 | `0352-sources-ciru-re-derived-315-15-and-HELD-with-all-15-.patch` | arifi-fork-base | `04ea0f3fc` | - | - | sources: ciru re-derived 315 -> 15 and HELD with all 15 reviewed (lane-156 GOAL 5) |
| 353 | `0353-sources-advance-remotes.upstream.pin-25ae3a9b3-9d77f.patch` | arifi-fork-base | `2afdab5a0` | - | - | sources: advance remotes.upstream.pin 25ae3a9b3 -> 9d77fa172 (b10488) with its review |
| 354 | `0354-sources-correct-the-turboquant-pin_review-s-FALSE-bl.patch` | arifi-fork-base | `57e2d896a` | - | - | sources: correct the turboquant pin_review's FALSE blocker for e130aef60 (checker F8, BLOCKING) |
| 355 | `0355-sources-trim-the-e130aef60-blocker-to-the-ONE-item-t.patch` | arifi-fork-base | `3acc2af7e` | - | - | sources: trim the e130aef60 blocker to the ONE item that survives (checker pass 2, advisory 1) |
| 356 | `0356-feat-kv-cache-port-flag-gated-mean-centering.patch` | arifi-fork-base | `aaf20fde1` | prisml/prism-v6@b075a360c | - | feat(kv-cache): port flag-gated mean centering |
| 357 | `0357-feat-kv-cache-bind-calibration-to-exact-model.patch` | arifi-fork-base | `38378c66e` | prisml/prism-v6@b075a360c | - | feat(kv-cache): bind calibration to exact model |
| 358 | `0358-fix-server-keep-target-calibration-out-of-draft-cont.patch` | arifi-fork-base | `663a8f4d0` | prisml/prism@4dd165625 | - | fix(server): keep target calibration out of draft contexts |
| 359 | `0359-test-kv-cache-add-Vulkan-tensor-probe-and-bench-pure.patch` | arifi-fork-base | `07c98aaab` | - | - | test(kv-cache): add Vulkan tensor probe and bench-pure A/B harness |
| 360 | `0360-sources-record-lane-158-PrismML-K-cache-disposition.patch` | arifi-fork-base | `de65a7008` | - | - | sources: record lane-158 PrismML K-cache disposition |
| 361 | `0361-test-kv-cache-fail-close-default-OFF-binary-identity.patch` | arifi-fork-base | `043fb3a4a` | - | - | test(kv-cache): fail-close default-OFF binary identity |
| 362 | `0362-test-kv-cache-assert-both-rotation-basis-polarities-.patch` | arifi-fork-base | `4010317b1` | - | - | test(kv-cache): assert both rotation-basis polarities against the resolved attn_rot_k |
| 363 | `0363-fix-kv-cache-make-the-A-B-harness-able-to-launch-and.patch` | arifi-fork-base | `812febcd1` | - | - | fix(kv-cache): make the A/B harness able to launch, and actually enforce the RAM floor it claims |
| 364 | `0364-fix-kv-cache-floor-check-the-model-named-by-m-not-th.patch` | arifi-fork-base | `15b9d2eb1` | - | - | fix(kv-cache): floor-check the model named by -m, not the largest .gguf on the command line |
| 365 | `0365-fix-kv-cache-wait-for-the-RAM-floor-between-arms-and.patch` | arifi-fork-base | `b6a66f917` | - | - | fix(kv-cache): wait for the RAM floor between arms, and pin -fit off so the A/B arms match |
| 366 | `0366-fix-kv-cache-probe-the-layer-ids-the-calibration-act.patch` | arifi-fork-base | `409b7f4c6` | - | - | fix(kv-cache): probe the layer ids the calibration actually has, and let an existing calibration be reused |
| 367 | `0367-support-DFlash2.patch` | arifi-fork-base | `3aa479a04` | - | - | support DFlash2 |
| 368 | `0368-dflash2-build-fixes-on-the-PR-27342-pick-dedupe-DFLA.patch` | arifi-fork-base | `93bfccc4c` | - | - | dflash2: build fixes on the PR #27342 pick - dedupe DFLASH_BLOCK_SIZE enum/hparam/kv-map (fork DFlash1 originals keep authority, default 16) + ml. reference fix (the shadow the fork's own comment documents) |
| 369 | `0369-sources-register-buun-spiritbuun-buun-llama-cpp-as-a.patch` | arifi-fork-base | `0ac817851` | - | - | sources: register buun (spiritbuun/buun-llama-cpp) as a greedy-ingestion watch source - President ruling, VBR/KV + DFlash2-for-3.8 lineage |
| 370 | `0370-cuda-lift-TQ3_4S-kernel-set-from-tq3-master-14-files.patch` | arifi-fork-base | `e7ef1fe3f` | turbo-tan/llama.cpp-tq3@854516439 | - | cuda: lift TQ3_4S kernel set from tq3/master (14 files) |
| 371 | `0371-metal-lift-TQ3_4S-kernel-set-from-tq3-master-5-files.patch` | arifi-fork-base | `e1706011f` | turbo-tan/llama.cpp-tq3@854516439 | - | metal: lift TQ3_4S kernel set from tq3/master (5 files + alloc hook) |
| 372 | `0372-metal-declare-the-tq3_rht-pipeline-getter-in-ggml-me.patch` | arifi-fork-base | `3846d1754` | turbo-tan/llama.cpp-tq3@854516439 | - | metal: declare the tq3_rht pipeline getter in ggml-metal-device.h |
| 373 | `0373-sync-move-base-b10488-b10524-regenerate-series-372-p.patch` | arifi-fork-base | `a5f8b0e36` | upstream@9ee9fc04c (tag b10524) | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | sync: move base b10488 -> b10524, regenerate series (372 patches) |
| 374 | `0374-spec-ingest-turboquant-MTP-boost-wave-effective-KV-b.patch` | arifi-fork-base | `708163476` | llama-cpp-turboquant@bd1bf025f (524531e57, f9e04f5d7, 4be91d62b, 5b105dfb7, 4c4131bf8, b20e97012, 7544b18cf, 275963f50, cd638bc13, e82fe159b) | - | spec: ingest turboquant MTP-boost wave + effective-KV bench reporting |
| 375 | `0375-sync-re-pin-wave-3-sources-regenerate-series-374-pat.patch` | arifi-fork-base | `cadea7a8d` | - | `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `LLAMA_USE_PREBUILT_UI`, `ARIFI_TOOL_NVFP4_REMAP` | sync: re-pin wave-3 sources + regenerate series (374 patches) |
| 376 | `0376-arifi-sync-provenance-native-class-root-fix-sha-pinn.patch` | arifi-fork-base | `3cecbd959` | - | - | arifi-sync: provenance native-class root fix - sha-pinned grandfather ledger + native Origin convention |
| 377 | `0377-vulkan-subgroup-cooperative-TQ-mat-vec-tq3_1s-tq4_1s.patch` | ternary-g128 | `bb5663d0c` | - | - | vulkan: subgroup-cooperative TQ mat-vec (tq3_1s/tq4_1s/tq3_4s) |
| 378 | `0378-vulkan-register-blocked-4-elem-lane-TQ-subgroup-mat-.patch` | ternary-g128 | `3fb632865` | - | - | vulkan: register-blocked 4-elem/lane TQ subgroup mat-vec (packed32 loads, in-register WHT stages 1-2, rows=4) |
| 379 | `0379-escha-native-Escha-W2-types-ESCHA2-55-ESCHA3-56-fuse.patch` | arifi-fork-base | `37672b33a` | - | - | escha: native Escha-W2 types (ESCHA2=55/ESCHA3=56) + fused GGML_OP_ESCHA_MM, CPU + Vulkan |
| 380 | `0380-escha-F-125-fix-full-64-bit-pair-sourcing-row-parity.patch` | arifi-fork-base | `6c5915aea` | - | - | escha: F-125 fix - full 64-bit pair sourcing + row-parity correction (K=2 AND K=3) |
| 381 | `0381-escha-test-backend-ops-un-nest-ESCHA_MM-eval-cases-m.patch` | arifi-fork-base | `aa2b60177` | - | - | escha: test-backend-ops — un-nest ESCHA_MM eval cases + multi-chunk ncols coverage + perf ncols curve |
| 382 | `0382-escha-vulkan-bound-every-escha_mm-dispatch-to-32-col.patch` | arifi-fork-base | `68f315573` | - | - | escha vulkan: bound every escha_mm dispatch to 32 columns (F-124 host-freeze guard) |
| 383 | `0383-escha-vulkan-column-blocked-escha_mm-one-weight-deco.patch` | arifi-fork-base | `8fb570d79` | - | - | escha vulkan: column-blocked escha_mm - one weight decode serves 4 columns (part 2) |
| 384 | `0384-escha-vulkan-column-block-C-8-fall-back-to-the-one-c.patch` | arifi-fork-base | `3724953ff` | - | - | escha vulkan: column block C=8 + fall back to the one-column kernel below a full block |
| 385 | `0385-escha-tests-cover-the-column-blocked-kernel-AT-MODEL.patch` | arifi-fork-base | `649326e52` | - | - | escha tests: cover the column-blocked kernel AT MODEL DEPTH (ncols=9, real 27B shapes) |
| 386 | `0386-escha-vulkan-one-hardware-f32-f16-convert-instead-of.patch` | arifi-fork-base | `eebdb62d5` | - | - | escha vulkan: one hardware f32->f16 convert instead of the 15-instruction software RTE |
| 387 | `0387-escha-vulkan-subgroup-shuffle-Hadamard-14-barriers-p.patch` | arifi-fork-base | `b4d497529` | - | - | escha vulkan: subgroup-shuffle Hadamard - 14 barriers per input block become 3 |
| 388 | `0388-escha-vulkan-the-hardware-f32-f16-convert-must-be-fl.patch` | arifi-fork-base | `09a755f81` | - | - | escha vulkan: the hardware f32->f16 convert must be float16_t, NOT packHalf2x16 (RTZ) |
| 389 | `0389-escha-vulkan-ESCHA_SG_HADAMARD-defaults-OFF-the-leve.patch` | arifi-fork-base | `da609f065` | - | - | escha vulkan: ESCHA_SG_HADAMARD defaults OFF - the lever is refuted by measurement |
| 390 | `0390-escha-vulkan-f16-native-generator-decode-18-bit-exac.patch` | arifi-fork-base | `fc725bb08` | - | - | escha vulkan: f16-native generator - decode +18%, bit-exact |
| 391 | `0391-escha-vulkan-two-more-levers-tried-and-REFUTED-by-me.patch` | arifi-fork-base | `7d74886d1` | - | - | escha vulkan: two more levers tried and REFUTED by measurement - occupancy and packed-pair |
| 392 | `0392-escha-vulkan-the-multi-column-threshold-was-disablin.patch` | arifi-fork-base | `e7d56f975` | - | - | escha vulkan: the multi-column threshold was disabling speculation - decode 3.96 -> 5.60 t/s |
| 393 | `0393-escha-vulkan-column-ladder-C2-C4-C8-C16-and-the-gene.patch` | arifi-fork-base | `33718bdd1` | - | - | escha vulkan: column ladder C2/C4/C8/C16, and the generator table measured and REFUTED |
| 394 | `0394-dflash-ingest-buun-s-DFlash2-adaptive-controller-bui.patch` | arifi-fork-base | `5bc3068e1` | - | - | dflash: ingest buun's DFlash2 adaptive controller - built, gated, and defaulted OFF by measurement |
| 395 | `0395-escha-vulkan-prefill-is-OCCUPANCY-bound-coopmat-and-.patch` | arifi-fork-base | `cdae587c0` | - | - | escha vulkan: prefill is OCCUPANCY-bound - coopmat and wider rungs both refuted, and the profile that said otherwise was lying |
| 396 | `0396-escha-vulkan-f16-activation-staging-built-correct-an.patch` | arifi-fork-base | `55961be81` | - | - | escha vulkan: f16 activation staging - built, correct, and refuted; the real prefill win was the build |
| 397 | `0397-dflash-reject-an-out-of-extent-draft-depth-instead-o.patch` | arifi-fork-base | `19c1174cb` | - | - | dflash: reject an out-of-extent draft depth instead of silently clamping it, and block the mask token |
| 398 | `0398-spec-load-the-draft-model-from-the-same-variable-the.patch` | arifi-fork-base | `c66454aa4` | - | - | spec: load the draft model from the same variable the log prints, and add an env-gated DFlash verify trace |
| 399 | `0399-F-136-ROOT-CAUSE-FIX-the-recurrent-snapshot-bank-n_r.patch` | prismml-recurrent | `b454aa9a3` | - | - | F-136 ROOT CAUSE + FIX: the recurrent snapshot-bank (n_rs_seq) machinery corrupts hybrid targets; drafters no longer flip the target onto it |
| 400 | `0400-spec-ON-DEVICE-speculative-checkpoints-drafter-now-B.patch` | arifi-fork-base | `2321e601d` | - | - | spec: ON-DEVICE speculative checkpoints - drafter now BEATS plain (8.02 vs 4.5 t/s, byte-clean) |
| 401 | `0401-spec-ring-rollback-re-enabled-DFlash2-depth-3-8.80-t.patch` | arifi-fork-base | `f77dbab5f` | - | - | spec: ring rollback re-enabled - DFlash2 depth 3 = 8.80 t/s (2.0x plain), 0 degenerate |
| 402 | `0402-rs-zero-on-clear-ROOT-FIX-cross-request-recurrent-st.patch` | prismml-recurrent | `6eec8fbfa` | - | - | rs: zero-on-clear ROOT FIX - cross-request recurrent-state leak was the residual F-136 mechanism |
| 403 | `0403-tests-zero_gather-CPY-GET_ROWS-Vulkan-probe-cases-se.patch` | arifi-fork-base | `4dcf0a27e` | - | - | tests: zero_gather + CPY/GET_ROWS Vulkan probe cases; server: depth>cap clamps with warning instead of load abort (F-136 arc) |
| 404 | `0404-rs-zeroing-hunt-CLOSED-leak-is-backend-independent-C.patch` | arifi-fork-base | `6a46f775c` | - | - | rs zeroing hunt CLOSED: leak is backend-independent (CPU repro), in-graph rs_z zero never lands on the live cache tensor; zero-on-clear promoted belt->contract. Env-gated discriminators: PDH hash, plane0/sl/rl belt scoping, zero-gather-out, hybrid set_input trace |
| 405 | `0405-vulkan-F-136-residual-ROOT-FIX-in-graph_optimize-is_.patch` | ternary-g128 | `dfc476bc8` | - | - | vulkan: F-136 residual ROOT FIX in graph_optimize is_src_of - full view-chain roots + RAW/WAR hazards through node sources; reorder no longer hoists reads over aliased inplace writes (repeat repro: belt-off now 3/3 byte-identical) |
| 406 | `0406-arifi-sync-base-move-b10524-9ee9fc04c-b10636-4d19b28.patch` | arifi-fork-base | `767cb29a6` | - | - | arifi-sync: base move b10524/9ee9fc04c -> b10636/4d19b2876 (runbook 2.2 manual path); upstream pin advanced as the same pair; prisml + tq3 pins HELD with 3 reviews recorded |
| 407 | `0407-b10636-bump-repair-restore-upstream-s-DOTS3NOTE-inde.patch` | arifi-fork-base | `80c4f85d9` | - | - | b10636 bump repair: restore upstream's DOTS3NOTE indexer arch + the two TAG_LLAMA_SEQ_ID_NEG TODOs that a whole-file --theirs resolution reverted. checkout --theirs takes stage 3 (the WHOLE file), not just the conflicting hunks - upstream's 4-line b10524..b10636 delta to this file was lost with it. GLM_DSA is absent here PRE-EXISTING (also absent on lane165-pre-bump-arifi-main), so it is NOT restored by this commit and stays a President item. |

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
| `0165-5-models-server-0021-common-support-the-DSpark-sidec.patch` | *(no Measured-effect trailer)* |
| `0166-5-models-server-0037-merge-fix-post-merge-build-and-.patch` | *(no Measured-effect trailer)* |
| `0167-5-models-server-0041-moe-cache-route-fit-probing-and.patch` | *(no Measured-effect trailer)* |
| `0168-5-models-server-0048-llama-bench-document-pw-prefetc.patch` | *(no Measured-effect trailer)* |
| `0169-5-models-server-0050-llama-bench-remove-stale-prefet.patch` | *(no Measured-effect trailer)* |
| `0170-5-models-server-0051-moe-cache-fix-auto-asymmetric-M.patch` | *(no Measured-effect trailer)* |
| `0171-6-tests-build-0001-tests-re-add-test-turbo-quant.c-r.patch` | *(no Measured-effect trailer)* |
| `0172-6-tests-build-0002-tests-port-fork-turbo-backend-and.patch` | *(no Measured-effect trailer)* |
| `0173-6-tests-build-0003-fix-close-complete-audit-findings.patch` | *(no Measured-effect trailer)* |
| `0174-6-tests-build-0004-laguna-tool-call-whitespace-toler.patch` | *(no Measured-effect trailer)* |
| `0175-6-tests-build-0006-ggml-webgpu-add-support-for-f16-r.patch` | *(no Measured-effect trailer)* |
| `0176-6-tests-build-0010-tests-restore-DSV4_HC_COMB-eps-sw.patch` | *(no Measured-effect trailer)* |
| `0177-6-tests-build-0011-tests-cast-float-printf-args-in-t.patch` | *(no Measured-effect trailer)* |
| `0178-6-tests-build-0012-vulkan-add-the-TQ3_1S-weight-type.patch` | *(no Measured-effect trailer)* |
| `0179-6-tests-build-0013-vulkan-reject-MUL_MAT_ID-for-the-.patch` | *(no Measured-effect trailer)* |
| `0180-6-tests-build-0014-ggml-cpu-remove-the-per-call-mall.patch` | *(no Measured-effect trailer)* |
| `0181-6-tests-build-0015-vulkan-add-mul_mat_vec_id-for-the.patch` | *(no Measured-effect trailer)* |
| `0182-6-tests-build-0016-feat-add-Vulkan-TQ4_1S-weight-pip.patch` | *(no Measured-effect trailer)* |
| `0183-6-tests-build-0017-tests-port-11-DSv4-shaped-TQ-MUL_.patch` | *(no Measured-effect trailer)* |
| `0184-6-tests-build-0019-cuda-fix-MoE-cache-capacity-accou.patch` | *(no Measured-effect trailer)* |
| `0185-6-tests-build-0020-common-preserve-implicit-MoE-cach.patch` | *(no Measured-effect trailer)* |
| `0186-6-tests-build-0021-cuda-tune-MoE-cache-CPU-overlap-a.patch` | *(no Measured-effect trailer)* |
| `0187-6-tests-build-0022-cuda-adapt-MoE-cache-admission-to.patch` | *(no Measured-effect trailer)* |
| `0188-6-tests-build-0023-cuda-align-MoE-cache-pool-allocat.patch` | *(no Measured-effect trailer)* |
| `0189-6-tests-build-0024-cuda-parallelize-MoE-cache-fills-.patch` | *(no Measured-effect trailer)* |
| `0190-6-tests-build-0025-cuda-fuse-cached-MoE-SwiGLU-rows..patch` | *(no Measured-effect trailer)* |
| `0191-6-tests-build-0026-cuda-aggregate-small-MoE-tensors-.patch` | *(no Measured-effect trailer)* |
| `0192-6-tests-build-0027-cuda-bound-automatic-MoE-cache-ad.patch` | *(no Measured-effect trailer)* |
| `0193-6-tests-build-0028-cuda-reject-undersized-automatic-.patch` | *(no Measured-effect trailer)* |
| `0194-6-tests-build-0029-cuda-enable-multi-token-MoE-cache.patch` | *(no Measured-effect trailer)* |
| `0195-6-tests-build-0030-common-surface-MoE-cache-activati.patch` | *(no Measured-effect trailer)* |
| `0196-6-tests-build-0031-cuda-accelerate-complete-MoE-cach.patch` | *(no Measured-effect trailer)* |
| `0197-6-tests-build-0032-cuda-report-oversized-MoE-cache-n.patch` | *(no Measured-effect trailer)* |
| `0198-6-tests-build-0033-tests-initialise-non-contiguous-t.patch` | *(no Measured-effect trailer)* |
| `0199-6-tests-build-0034-tests-cover-turbo-KV-flash-attent.patch` | *(no Measured-effect trailer)* |
| `0200-6-tests-build-0037-moe-cache-provider-registry-with-.patch` | *(no Measured-effect trailer)* |
| `0201-6-tests-build-0038-moe-cache-route-fit-probing-and-t.patch` | *(no Measured-effect trailer)* |
| `0202-6-tests-build-0039-moe-cache-route-context-eligibili.patch` | *(no Measured-effect trailer)* |
| `0203-6-tests-build-0040-moe-cache-allow-automatic-mode-on.patch` | *(no Measured-effect trailer)* |
| `0204-6-tests-build-0041-moe-cache-fix-auto-asymmetric-MLA.patch` | *(no Measured-effect trailer)* |
| `0205-6-tests-build-0042-test-moe-cache-accept-IGPU-device.patch` | *(no Measured-effect trailer)* |
| `0206-6-tests-build-0043-test-moe-cache-make-cache-shared-.patch` | *(no Measured-effect trailer)* |
| `0207-6-tests-build-0044-test-moe-cache-save-and-restore-t.patch` | *(no Measured-effect trailer)* |
| `0208-6-tests-build-0045-tests-tell-ctest-that-77-means-sk.patch` | *(no Measured-effect trailer)* |
| `0209-resolved-0009-vulkan-add-mul_mat_vec_id-for-the-Turb.patch` | *(no Measured-effect trailer)* |
| `0210-resolved-0011-fix-correct-Vulkan-turbo3-pipeline-wir.patch` | *(no Measured-effect trailer)* |
| `0211-resolved-0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-ga.patch` | *(no Measured-effect trailer)* |
| `0212-resolved-0005-merge-close-TurboQuant-parity-gaps.pat.patch` | *(no Measured-effect trailer)* |
| `0213-resolved-0036-common-surface-MoE-cache-activation.pa.patch` | *(no Measured-effect trailer)* |
| `0214-resolved-0019-vulkan-dispatch-the-TQ-rotated-mul_mm_.patch` | *(no Measured-effect trailer)* |
| `0215-resolved-0010-feat-add-Vulkan-turbo3-KV-cache-pipeli.patch` | *(no Measured-effect trailer)* |
| `0216-resolved-0004-fix-close-complete-audit-findings-7-po.patch` | *(no Measured-effect trailer)* |
| `0217-resolved-0005-metal-restore-lost-kernel-templates-ad.patch` | *(no Measured-effect trailer)* |
| `0218-resolved-0012-metal-implement-DeepSeek-V4-hyper-conn.patch` | *(no Measured-effect trailer)* |
| `0219-resolved-0035-cuda-gate-fused-TQ-mul_mat-paths-on-co.patch` | *(no Measured-effect trailer)* |
| `0220-resolved-0036-cuda-disable-fused-TQ3_1S-mul_mat-kern.patch` | *(no Measured-effect trailer)* |
| `0221-resolved-0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HI.patch` | *(no Measured-effect trailer)* |
| `0222-resolved-0047-sycl-hip-fix-two-build-breaks-in-fork-.patch` | *(no Measured-effect trailer)* |
| `0223-resolved-0006-fix-close-complete-audit-findings-7-po.patch` | *(no Measured-effect trailer)* |
| `0224-resolved-0022-DeepseekV4-MTP-DSpark-25784.patch.patch` | *(no Measured-effect trailer)* |
| `0225-resolved-0023-dflash-fix-merge-artifacts-from-upstre.patch` | *(no Measured-effect trailer)* |
| `0226-resolved-0029-server-harden-shared-draft-device-plac.patch` | *(no Measured-effect trailer)* |
| `0227-resolved-0025-fix-server-batch-restored-checkpoint-p.patch` | *(no Measured-effect trailer)* |
| `0228-resolved-0026-cuda-rework-MoE-expert-cache-execution.patch` | *(no Measured-effect trailer)* |
| `0229-resolved-0033-server-account-for-MTP-placement-in-fi.patch` | *(no Measured-effect trailer)* |
| `0230-resolved-0035-cuda-reject-undersized-automatic-MoE-c.patch` | *(no Measured-effect trailer)* |
| `0231-resolved-0038-moe-cache-add-moe-cache-soft-mode-with.patch` | *(no Measured-effect trailer)* |
| `0232-resolved-0039-model-Muse-Glimmer-Support-26841.patch.patch` | *(no Measured-effect trailer)* |
| `0233-resolved-0043-moe-cache-audit-fixes-H1-F1-F2-A1.patc.patch` | *(no Measured-effect trailer)* |
| `0234-resolved-0044-moe-cache-restore-moe_cache-params-on-.patch` | *(no Measured-effect trailer)* |
| `0235-resolved-0046-llama-remove-dead-MSA-indexer-scaffold.patch` | *(no Measured-effect trailer)* |
| `0236-resolved-0047-moe-cache-add-logging-at-silent-failur.patch` | *(no Measured-effect trailer)* |
| `0237-resolved-0052-cuda-fit-fix-two-Werror-build-failures.patch` | *(no Measured-effect trailer)* |
| `0238-resolved-0005-test-fix-some-CI-errors-26415.patch.patch` | *(no Measured-effect trailer)* |
| `0239-resolved-remaining-conflict-markers.patch` | *(no Measured-effect trailer)* |
| `0240-layer7-0001-WIP-add-TurboQuant-KV-cache-types-turbo3.patch` | *(no Measured-effect trailer)* |
| `0241-layer7-0002-Update-GGMLQuantizationType-and-LlamaFil.patch` | *(no Measured-effect trailer)* |
| `0242-layer7-0003-ggml-port-TurboQuant-core-quant-types-an.patch` | *(no Measured-effect trailer)* |
| `0243-layer7-0005-docs-add-rebase-plan-and-TurboQuant-reci.patch` | *(no Measured-effect trailer)* |
| `0244-layer7-0007-gguf-py-dedupe-stacked-merge-artifacts-i.patch` | *(no Measured-effect trailer)* |
| `0245-layer7-0008-docs-add-KV-cache-quantization-guide-upd.patch` | *(no Measured-effect trailer)* |
| `0246-layer7-0009-vulkan-port-fork-241-wave64-ballot-fix-c.patch` | *(no Measured-effect trailer)* |
| `0247-layer7-0010-merge-close-TurboQuant-parity-gaps.patch.patch` | *(no Measured-effect trailer)* |
| `0248-layer7-0011-metal-restore-lost-kernel-templates-add-.patch` | *(no Measured-effect trailer)* |
| `0249-layer7-0014-laguna-tool-call-whitespace-tolerance-do.patch` | *(no Measured-effect trailer)* |
| `0250-layer7-0019-fix-apply-rebase-audit-fixes-stack-overf.patch` | *(no Measured-effect trailer)* |
| `0251-layer7-0020-docs-add-TurboQuant-project-overview-to-.patch` | *(no Measured-effect trailer)* |
| `0252-layer7-0021-cuda-rework-MoE-expert-cache-execution.p.patch` | *(no Measured-effect trailer)* |
| `0253-layer7-0022-docs-update-MoE-cache-validation.patch.patch` | *(no Measured-effect trailer)* |
| `0254-layer7-0023-common-preserve-implicit-MoE-cache-provi.patch` | *(no Measured-effect trailer)* |
| `0255-layer7-0024-server-harden-shared-draft-device-placem.patch` | *(no Measured-effect trailer)* |
| `0256-layer7-0025-cuda-tune-MoE-cache-CPU-overlap-automati.patch` | *(no Measured-effect trailer)* |
| `0257-layer7-0026-cuda-adapt-MoE-cache-admission-to-device.patch` | *(no Measured-effect trailer)* |
| `0258-layer7-0027-cuda-align-MoE-cache-pool-allocation-wit.patch` | *(no Measured-effect trailer)* |
| `0259-layer7-0028-docs-update-MoE-cache-validation.patch.patch` | *(no Measured-effect trailer)* |
| `0260-layer7-0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch.patch` | *(no Measured-effect trailer)* |
| `0261-layer7-0030-cuda-aggregate-small-MoE-tensors-into-ca.patch` | *(no Measured-effect trailer)* |
| `0262-layer7-0031-cuda-bound-automatic-MoE-cache-admission.patch` | *(no Measured-effect trailer)* |
| `0263-layer7-0032-cuda-reject-undersized-automatic-MoE-cac.patch` | *(no Measured-effect trailer)* |
| `0264-layer7-0033-cuda-enable-multi-token-MoE-cache-in-for.patch` | *(no Measured-effect trailer)* |
| `0265-layer7-0034-common-surface-MoE-cache-activation.patc.patch` | *(no Measured-effect trailer)* |
| `0266-layer7-0035-docs-fix-MoE-cache-benchmark-verbosity.p.patch` | *(no Measured-effect trailer)* |
| `0267-layer7-0036-cuda-accelerate-complete-MoE-cache-pools.patch` | *(no Measured-effect trailer)* |
| `0268-layer7-0037-cuda-report-oversized-MoE-cache-nodes.pa.patch` | *(no Measured-effect trailer)* |
| `0269-layer7-0038-cuda-clarify-MoE-cache-session-diagnosti.patch` | *(no Measured-effect trailer)* |
| `0270-layer7-0039-docs-record-MoE-cache-workload-convergen.patch` | *(no Measured-effect trailer)* |
| `0271-layer7-0040-cuda-report-MoE-cache-pair-residency.pat.patch` | *(no Measured-effect trailer)* |
| `0272-layer7-0041-docs-document-what-the-test-suites-do-an.patch` | *(no Measured-effect trailer)* |
| `0273-layer7-0044-moe-cache-provider-registry-with-per-sch.patch` | *(no Measured-effect trailer)* |
| `0274-layer7-0046-moe-cache-allow-automatic-mode-on-a-sing.patch` | *(no Measured-effect trailer)* |
| `0275-layer7-0048-docs-document-INFO-level-MoE-cache-disab.patch` | *(no Measured-effect trailer)* |
| `0276-layer7-0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CAC.patch` | *(no Measured-effect trailer)* |
| `0277-layer7-0050-docs-correct-MOE-CACHE.md-against-the-im.patch` | *(no Measured-effect trailer)* |
| `0278-layer7-0051-ggml-split-the-turbo3-group-size-declara.patch` | *(no Measured-effect trailer)* |
| `0279-resolve-8-committed-conflict-markers-on-merit.patch` | *(no Measured-effect trailer)* |
| `0280-fix-the-merge-so-it-builds-5-files-6-defects.patch` | *(no Measured-effect trailer)* |
| `0281-merge-repairs-seven-defect-classes-and-one-the-compi.patch` | *(no Measured-effect trailer)* |
| `0282-ROCmFPX-on-Vulkan-the-shader-half-ported.-NOT-YET-WO.patch` | *(no Measured-effect trailer)* |
| `0283-currency-register-ALL-SEVEN-forks-so-nothing-can-dri.patch` | *(no Measured-effect trailer)* |
| `0284-series-base-and-the-branch-it-describes-are-ONE-pair.patch` | *(no Measured-effect trailer)* |
| `0285-hygiene-the-code-review-skill-taught-a-type-ID-map-t.patch` | *(no Measured-effect trailer)* |
| `0286-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | *(no Measured-effect trailer)* |
| `0287-contract-tq3-is-the-first-family-we-must-renumber-so.patch` | *(no Measured-effect trailer)* |
| `0288-sources-honest-pins-turboquant-re-pinned-off-a-dead-.patch` | *(no Measured-effect trailer)* |
| `0289-sources-lane-143-self-refutation-the-turbo3-ODR-defe.patch` | *(no Measured-effect trailer)* |
| `0290-sources-r2-checker-BLOCK-F1-F6-ciru-160-was-145-comm.patch` | *(no Measured-effect trailer)* |
| `0291-series-point-base.ref-at-the-LINEAR-branch-recut-lin.patch` | *(no Measured-effect trailer)* |
| `0292-series-disclose-in-the-record-itself-that-the-flatte.patch` | *(no Measured-effect trailer)* |
| `0293-ggml-port-the-tq3-TQ3_4S-family-codec-at-ids-48-51-T.patch` | *(no Measured-effect trailer)* |
| `0294-tests-cover-the-tq3-family-in-test-quantize-fns-and-.patch` | *(no Measured-effect trailer)* |
| `0295-contract-tq3-rows-48-51-implemented-behind-GGML_ARIF.patch` | *(no Measured-effect trailer)* |
| `0296-tests-tq3_4s-tq3_4se-skipped-in-test-quantize-fns-wi.patch` | *(no Measured-effect trailer)* |
| `0297-tools-gguf-retag-tq3-lands-LAST-after-the-coherence-.patch` | *(no Measured-effect trailer)* |
| `0298-checker-fixes-choose_index-restored-verbatim-inverte.patch` | *(no Measured-effect trailer)* |
| `0299-retag-verify-geometry-per-TENSOR-for-every-mapped-id.patch` | *(no Measured-effect trailer)* |
| `0300-retag-README-per-tensor-geometry-check-for-every-map.patch` | *(no Measured-effect trailer)* |
| `0301-lane-144-GOAL-2-step-1-TQ3_4S-id-48-Vulkan-port-dequ.patch` | *(no Measured-effect trailer)* |
| `0302-lane-144-sweep-ROCmFP4-ROCmFPX-in-test-backend-ops-a.patch` | *(no Measured-effect trailer)* |
| `0303-lane-144-CHECKER-FIX-TQ3_4S-was-missing-from-the-MUL.patch` | *(no Measured-effect trailer)* |
| `0304-lane-145-Q6_0_ROCMFPX-Vulkan-fix-implement-the-6-bit.patch` | *(no Measured-effect trailer)* |
| `0305-lane-145-checker-finding-7-delete-block_rocmfpx_fp6_.patch` | *(no Measured-effect trailer)* |
| `0306-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | *(no Measured-effect trailer)* |
| `0307-test-backend-ops-cover-the-CPY-types-supports_op-nam.patch` | *(no Measured-effect trailer)* |
| `0308-lane-148-defect-A-ROOT-FIX-Vulkan-turbo3-centroid-LU.patch` | *(no Measured-effect trailer)* |
| `0309-lane-148-defect-B-ROOT-FIX-FA_TYPE_TURBO2-3-4_0-GLSL.patch` | *(no Measured-effect trailer)* |
| `0310-lane-148-F-69-sibling-coverage-test-backend-ops-FLAS.patch` | *(no Measured-effect trailer)* |
| `0311-lane-148-checker-findings-4-5-FIXED-turbo4-SET_ROWS-.patch` | *(no Measured-effect trailer)* |
| `0312-lane-149-F-110-guard-rung-b-arifi_sync_check.py-CMak.patch` | *(no Measured-effect trailer)* |
| `0313-lane-149-F-112-F-109-harness-guards-NOT_SUPPORTED-ca.patch` | *(no Measured-effect trailer)* |
| `0314-lane-149-F-109-payoff-q2_0_g128-SET_ROWS-tie-roundin.patch` | *(no Measured-effect trailer)* |
| `0315-lane-149-checker-driven-hardening-of-arifi-sync-chec.patch` | *(no Measured-effect trailer)* |
| `0316-lane-149-F-112-second-half-checker-finding-11-per-op.patch` | *(no Measured-effect trailer)* |
| `0317-lane-150-step-1-TQ4_1S-read-back-leg-wired-CPY-tq4_1.patch` | *(no Measured-effect trailer)* |
| `0318-lane-150-step-2-TQ4_1S-write-leg-EXECUTES-SET_ROWS_T.patch` | *(no Measured-effect trailer)* |
| `0319-lane-150-step-3-root-fix-for-the-hole-that-hid-the-w.patch` | *(no Measured-effect trailer)* |
| `0320-lane-150-checker-driven-fixes-type_bucket-prefix-bou.patch` | *(no Measured-effect trailer)* |
| `0321-lane-151-GOAL-6-hygiene-drop-648-tracked-_lane-out-d.patch` | *(no Measured-effect trailer)* |
| `0322-lane-151-GOAL-9-reserve-tq3-s-three-destination-less.patch` | *(no Measured-effect trailer)* |
| `0323-lane-151-GOALs-6-7-README-refreshed-to-b10453-canoni.patch` | *(no Measured-effect trailer)* |
| `0324-lane-151-GOAL-2-sources.json-base.ref-arifi-main-the.patch` | *(no Measured-effect trailer)* |
| `0325-lane-151-GOAL-5-gitignore-.tokensave-the-fork-repo-n.patch` | *(no Measured-effect trailer)* |
| `0326-lane-151-CHECKER-ROOT-FIX-series-check-now-asserts-e.patch` | *(no Measured-effect trailer)* |
| `0327-lane-151-CHECKER-FIXES-docs-UPDATE-RUNBOOK-4.5-no-lo.patch` | *(no Measured-effect trailer)* |
| `0328-lane-151-HQ-DIRECTIVE-RETAIN-the-licenses-instead-of.patch` | *(no Measured-effect trailer)* |
| `0329-licenses-thecodacus-MIT-retained-from-the-public-Git.patch` | *(no Measured-effect trailer)* |
| `0330-lane-152-GOAL-1-upstream-bump-b10453-b10481-the-FIRS.patch` | *(no Measured-effect trailer)* |
| `0331-lane-152-what-the-FIRST-live-run-of-UPDATE-RUNBOOK-s.patch` | *(no Measured-effect trailer)* |
| `0332-lane-152-GOAL-2-batch-1-ciru-s-Vulkan-SPIRV-Headers-.patch` | UNMEASURED,UNMEASURED - the source reports reasoning depth 17k-64k to 33k-120k chars and
  structural codegen bugs eliminated on Qwen3.8-27B at tq3_4s, plus about 12 MiB per family of
  size cost. Theirs, not ours.,UNMEASURED - the source measured 60 to 14 test-backend-ops failures on
  gfx1151 / ROCm 7.14 at about 2 percent decode cost. We do not build HIP in the configuration
  any number in this repository was measured on. |
| `0333-lane-152-GOAL-2-batch-2-three-lfm2-pre-tokenizer-has.patch` | UNMEASURED - no conversion of an LFM2.5 model was run, and the pre-tokenizer
  workflow was not executed. Syntax-checked with ast.parse, nothing more. |
| `0334-lane-152-pins-THREE-advance-with-reviews-upstream-ro.patch` | *(no Measured-effect trailer)* |
| `0335-lane-152-checker-F7-the-old-base-is-tag-b10454-not-b.patch` | *(no Measured-effect trailer)* |
| `0336-repo-env.json-WI-1540-tail-the-engine-repo-s-env-man.patch` | *(no Measured-effect trailer)* |
| `0337-WI-1580-sub-2-UPDATE-RUNBOOK-section-7-the-4.5-line-.patch` | *(no Measured-effect trailer)* |
| `0338-vulkan-land-the-MoE-expert-cache-provider-files-VK-M.patch` | UNMEASURED at this step - the files are not wired into the build yet; step 2 carries the build and guard proofs. |
| `0339-graph-derive-the-V-unpad-head-count-from-the-attenti.patch` | UNMEASURED on this rig. Their report is a live abort in ggml_reshape_3d during graph reserve for gpt-oss-20b (V head_dim 64 padded to 128, 64 q heads over 8 kv heads) with -ctk turbo3 -ctv turbo3; we do not run that model, so the fix is taken as a correctness guard and the claim is NOT restated as ours. |
| `0340-vulkan-wire-the-MoE-expert-cache-provider-into-the-b.patch` | UNMEASURED for throughput - this lane makes NO timed claim about the cache. The proofs carried are correctness proofs only: baseline exe mtimes banked before the ingest, then a rebuild with fresh mtimes, test-backend-ops and test-quantize-fns compared against the banked baseline, and arifi_sync_check.py green. |
| `0341-vulkan-complete-the-MoE-cache-port-the-56-provider-h.patch` | UNMEASURED for throughput; this lane makes NO timed claim about the cache. Correctness only: F-107 exe mtimes moved for all five targets (10:52 to 11:30), llama-cli grew 97090161 to 97433306 bytes, test-backend-ops 26213/26260 identical to the banked baseline, test-quantize-fns 6 failures identical, arifi_sync_check.py OK on the same 49/7/16/35/16/12 tallies. |
| `0342-sources-advance-the-turboquant-pin-f6124e914-7ebcbb0.patch` | UNMEASURED - a pin and its review change no runtime behaviour. |
| `0343-off-rig-backends-land-the-12-CUDA-Metal-SYCL-HIP-row.patch` | UNMEASURED AND UNPROVABLE ON THIS RIG - that is the point of the ruling. This code never compiles here (no CUDA/Metal/SYCL/HIP device or toolchain), so no F-107 exe proof and no backend-ops execution proof exists for it. A future lane with the hardware owes roughly 30 min to verify it; the debt is a named OWED row in the REPORT. |
| `0344-sources-pin_review-names-the-12-off-rig-shas-and-the.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0345-sources-correct-the-pin_review-the-contested-12-12-a.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0346-sources-pin_review-records-the-apply-count-claim-as-.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0347-base-b10481-25ae3a9b3-b10488-9d77fa172-series-regen-.patch` | *(no Measured-effect trailer)* |
| `0348-moe-cache-heat-protected-eviction-keeps-hot-experts-.patch` | UNMEASURED here. The source reports +12% TG (soft) / +7.5%
  (auto) on an RTX 5090 with Laguna S-2.1 - a CUDA measurement on hardware this
  estate does not have, quoted, NOT adopted as our number. |
| `0349-tests-initialise-non-contiguous-tensors-row-by-row-t.patch` | UNMEASURED as a delta - the sweep baseline 26213/26260 is
  re-run after this lane's build and compared case-for-case. The source notes
  the fix does NOT make their TQ4_1S k_v=1600 case pass; it unmasks a separate
  CUDA defect, which is off-rig for us. |
| `0350-sources-advance-the-turboquant-pin-7ebcbb0b6-d14e368.patch` | *(no Measured-effect trailer)* |
| `0351-sources-prisml-pin-HELD-deliberately-with-a-sharper-.patch` | *(no Measured-effect trailer)* |
| `0352-sources-ciru-re-derived-315-15-and-HELD-with-all-15-.patch` | *(no Measured-effect trailer)* |
| `0353-sources-advance-remotes.upstream.pin-25ae3a9b3-9d77f.patch` | *(no Measured-effect trailer)* |
| `0354-sources-correct-the-turboquant-pin_review-s-FALSE-bl.patch` | *(no Measured-effect trailer)* |
| `0355-sources-trim-the-e130aef60-blocker-to-the-ONE-item-t.patch` | *(no Measured-effect trailer)* |
| `0356-feat-kv-cache-port-flag-gated-mean-centering.patch` | UNMEASURED |
| `0357-feat-kv-cache-bind-calibration-to-exact-model.patch` | UNMEASURED (native toolchain execution denied in managed seat) |
| `0358-fix-server-keep-target-calibration-out-of-draft-cont.patch` | UNMEASURED (native toolchain execution denied in managed seat) |
| `0359-test-kv-cache-add-Vulkan-tensor-probe-and-bench-pure.patch` | harness selftest PASS; native build/model effect UNMEASURED (HQ-OWED) |
| `0360-sources-record-lane-158-PrismML-K-cache-disposition.patch` | source census corrected; native model effect UNMEASURED (HQ-OWED) |
| `0361-test-kv-cache-fail-close-default-OFF-binary-identity.patch` | clean fixture PASS; planted byte mismatch RED; native identity UNMEASURED (HQ-OWED) |
| `0362-test-kv-cache-assert-both-rotation-basis-polarities-.patch` | *(no Measured-effect trailer)* |
| `0363-fix-kv-cache-make-the-A-B-harness-able-to-launch-and.patch` | *(no Measured-effect trailer)* |
| `0364-fix-kv-cache-floor-check-the-model-named-by-m-not-th.patch` | *(no Measured-effect trailer)* |
| `0365-fix-kv-cache-wait-for-the-RAM-floor-between-arms-and.patch` | *(no Measured-effect trailer)* |
| `0366-fix-kv-cache-probe-the-layer-ids-the-calibration-act.patch` | *(no Measured-effect trailer)* |
| `0367-support-DFlash2.patch` | *(no Measured-effect trailer)* |
| `0368-dflash2-build-fixes-on-the-PR-27342-pick-dedupe-DFLA.patch` | *(no Measured-effect trailer)* |
| `0369-sources-register-buun-spiritbuun-buun-llama-cpp-as-a.patch` | *(no Measured-effect trailer)* |
| `0370-cuda-lift-TQ3_4S-kernel-set-from-tq3-master-14-files.patch` | UNMEASURED ON THIS RIG (no CUDA silicon); tq3-side references: RTX 3090 tg128 43.1 -> 47.4 t/s (PRMT vec_dot), Ampere decode +35% (3d3888c9f) |
| `0371-metal-lift-TQ3_4S-kernel-set-from-tq3-master-5-files.patch` | UNMEASURED ON THIS RIG (no Metal silicon); tq3-side reference: M3 Pro 27B decode ~4 t/s, ~27x over CPU (02d51f048) |
| `0372-metal-declare-the-tq3_rht-pipeline-getter-in-ggml-me.patch` | UNMEASURED ON THIS RIG (no Metal toolchain) |
| `0373-sync-move-base-b10488-b10524-regenerate-series-372-p.patch` | UNMEASURED (build+sweep follow in this lane) |
| `0374-spec-ingest-turboquant-MTP-boost-wave-effective-KV-b.patch` | UNMEASURED (their mtp-boost numbers are theirs; no arm run on this rig yet) |
| `0375-sync-re-pin-wave-3-sources-regenerate-series-374-pat.patch` | UNMEASURED (build+sweep follow) |
| `0376-arifi-sync-provenance-native-class-root-fix-sha-pinn.patch` | provenance audit flips ambient-red to enforcing; 289 grandfathered, 0 missing, new untrailered commits fail again |
| `0377-vulkan-subgroup-cooperative-TQ-mat-vec-tq3_1s-tq4_1s.patch` | 27B tq3_4s tg32 0.67->1.80 t/s; 0.5b tq4_1s tg64 44.7->126.7, tq3_1s 45.5->107.1; mat-vec kernel n=1 3.1-3.4x (780M, evidence lane-163) |
| `0378-vulkan-register-blocked-4-elem-lane-TQ-subgroup-mat-.patch` | *(no Measured-effect trailer)* |
| `0379-escha-native-Escha-W2-types-ESCHA2-55-ESCHA3-56-fuse.patch` | *(no Measured-effect trailer)* |
| `0380-escha-F-125-fix-full-64-bit-pair-sourcing-row-parity.patch` | Escha-W2 native decode goes from token salad to coherent; worst-projection weight correlation 0.9358, K=2 and K=3 both correct |
| `0381-escha-test-backend-ops-un-nest-ESCHA_MM-eval-cases-m.patch` | none yet (test registration only); unlocks the ncols duration curve |
| `0382-escha-vulkan-bound-every-escha_mm-dispatch-to-32-col.patch` | per-dispatch column count capped at 32, was ncols up to 2048 |
| `0383-escha-vulkan-column-blocked-escha_mm-one-weight-deco.patch` | pending on this commit, before-numbers are 1.15 and 2.12 ms per column |
| `0384-escha-vulkan-column-block-C-8-fall-back-to-the-one-c.patch` | 3.51x on escha3 17408x5120 at ncols=64, decode unchanged |
| `0385-escha-tests-cover-the-column-blocked-kernel-AT-MODEL.patch` | shipped C=8 prefill path now covered at real 27B shapes, 10/10 GREEN |
| `0386-escha-vulkan-one-hardware-f32-f16-convert-instead-of.patch` | UNMEASURED at commit time - per-weight instruction count drops ~21 to ~7 by inspection; decode/prefill deltas and the exhaustive rounding sweep are owed |
| `0387-escha-vulkan-subgroup-shuffle-Hadamard-14-barriers-p.patch` | UNMEASURED at commit time - barrier count per 128-input block drops 14 to 3 at subgroup 64 by inspection; correctness gate and speed delta owed |
| `0388-escha-vulkan-the-hardware-f32-f16-convert-must-be-fl.patch` | CORRECTNESS ONLY at this commit - removes a one-ulp truncation error affecting 25.6% of codewords that 1ee8e9ad7 would have shipped; speed deltas for this lever alone are measured next and reported separately. |
| `0389-escha-vulkan-ESCHA_SG_HADAMARD-defaults-OFF-the-leve.patch` | SG lever OFF by default; no speed change vs the shipped RTE-only configuration, and ~3% of prefill recovered versus having it on |
| `0390-escha-vulkan-f16-native-generator-decode-18-bit-exac.patch` | *(no Measured-effect trailer)* |
| `0391-escha-vulkan-two-more-levers-tried-and-REFUTED-by-me.patch` | *(no Measured-effect trailer)* |
| `0392-escha-vulkan-the-multi-column-threshold-was-disablin.patch` | *(no Measured-effect trailer)* |
| `0393-escha-vulkan-column-ladder-C2-C4-C8-C16-and-the-gene.patch` | *(no Measured-effect trailer)* |
| `0394-dflash-ingest-buun-s-DFlash2-adaptive-controller-bui.patch` | *(no Measured-effect trailer)* |
| `0395-escha-vulkan-prefill-is-OCCUPANCY-bound-coopmat-and-.patch` | *(no Measured-effect trailer)* |
| `0396-escha-vulkan-f16-activation-staging-built-correct-an.patch` | *(no Measured-effect trailer)* |
| `0397-dflash-reject-an-out-of-extent-draft-depth-instead-o.patch` | *(no Measured-effect trailer)* |
| `0398-spec-load-the-draft-model-from-the-same-variable-the.patch` | *(no Measured-effect trailer)* |
| `0399-F-136-ROOT-CAUSE-FIX-the-recurrent-snapshot-bank-n_r.patch` | *(no Measured-effect trailer)* |
| `0400-spec-ON-DEVICE-speculative-checkpoints-drafter-now-B.patch` | *(no Measured-effect trailer)* |
| `0401-spec-ring-rollback-re-enabled-DFlash2-depth-3-8.80-t.patch` | *(no Measured-effect trailer)* |
| `0402-rs-zero-on-clear-ROOT-FIX-cross-request-recurrent-st.patch` | *(no Measured-effect trailer)* |
| `0403-tests-zero_gather-CPY-GET_ROWS-Vulkan-probe-cases-se.patch` | *(no Measured-effect trailer)* |
| `0404-rs-zeroing-hunt-CLOSED-leak-is-backend-independent-C.patch` | *(no Measured-effect trailer)* |
| `0405-vulkan-F-136-residual-ROOT-FIX-in-graph_optimize-is_.patch` | *(no Measured-effect trailer)* |
| `0406-arifi-sync-base-move-b10524-9ee9fc04c-b10636-4d19b28.patch` | *(no Measured-effect trailer)* |
| `0407-b10636-bump-repair-restore-upstream-s-DOTS3NOTE-inde.patch` | *(no Measured-effect trailer)* |

## Unclassified

These commits fell through every grouping rule in `arifi_sync.py`. That is surfaced
rather than silently bucketed - add a rule when a new source appears.

- `715c433c3` patches: generate the full auditable 38-patch series from git
- `a22db15c2` tools: add arifi-sync - the currency, series and upstream-bump driver
- `5bb391cc5` patches: move banked PrismML source patches out of the generated series directory
- `ce32525d0` patches: make the series generator implement its own convergence rule
- `fc39077ec` patches: stop the manifest deriving anything from excluded commits
- `c64d968b9` port: Q1_0 repack scaffolding + Arm NEON/DP GEMV-GEMM kernels
- `6d77a587d` port: native x86 Q2_0 vec_dot (AVX512-VNNI / AVX-VNNI), drop scalar fallback alias
- `9251864bc` port: x86 AVX512-VNNI repack GEMV/GEMM for Q1_0 and Q2_0
- `d39f16e7b` fix(ggml-cpu): parameterize Q2_0 GEMM activation stride by QK2_0/QK8_0 - g64 NaN
- `bc62a6fe7` fix(ggml-cpu): same g64 stride defect in the ported x86 Q2_0 vec_dot
- `401eae47a` ggml: reserve the ArifiLabs type-ID blocks at the enum itself
- `312b7b3e8` ggml: ROCmFP4/ROCmFPX weight formats behind GGML_ARIFI_ROCMFPX_FORMATS
- `4bc3ac170` llama: ROCmFPX file types, quantizer wiring and the gguf-py registry
- `b968f70d3` tools: record GGML_ARIFI_ROCMFPX_FORMATS in the build-recipe snapshot
- `ddde547df` feat: --arifi-profile and a CPU-only VNNI-repack startup advisory
- `420ccfc17` sync: currency GREEN — TurboQuant checkouts pinned at their lane-110C-reviewed shas (c26cbdffc, ba52ad107), rocmfpx pin advanced to db6844d9b after 6-commit review (HIP pinned-staging noted for the HIP-deltas slot)
- `67737ceaa` fix(ggml-cpu/x86): don't pass __m256i by value across the repack GEMV/GEMM ABI boundary (Windows 0xC0000005)
- `1f67e7d7d` ggml: TurboQuant TQ3_1S/TQ4_1S weight formats behind GGML_ARIFI_TURBO_WEIGHT_QUANTS
- `15cb1fbe4` ggml-cpu/x86: use the row-stride parameter in repack GEMV stores instead of the row count
- `bddafc077` docs(FINDINGS): F-09 - the repack/GPU trade-off is a compute-placement choice, not a bug
- `0e52f3374` docs(FINDINGS): record why the repack auto-enable was NOT shipped
- `83eea6cbb` provenance: catch the silent trailer-block break instead of only reporting it
- `abf1860d0` gitattributes: pin .githooks to LF so the hook runs on Linux and macOS
- `ae58bf43c` githooks: force a normalized blob for the commit-msg hook
- `3e5dbdceb` docs(FINDINGS): F-09 exit 1 is the direction - dual residency, not a forced choice
- `b756121c7` repack: dual residency - GGML_ARIFI_VNNI_REPACK=2 keeps prefill on the GPU AND repacks decode
- `e197c8e90` repack: dual residency measured - both wins held; fix the hot-path cost that ate the first one
- `c12ced72c` repack: fix the shadow tensor COUNT in the accounting line, and strengthen the claim it supports
- `8ceac6e1a` repack: CPU kernels for the 128-group ternary format
- `adcf40c8e` tests: direct equivalence test for the ternary repack kernels
- `4376421fc` F-06: measure the GPU tier on one binary, and count the placement the prompt claim rested on
- `de367c13c` registry: the dual-residency prompt mechanism is measured now, not ASSUMED
- `8e80c0cf7` registry: our DSpark verdict was never ours -- the drafter cannot load on this fork
- `f85196125` speculative: DFlash/DSpark drafters have no vocab -- take the mask token from metadata
- `e15cde987` expert cache: stride the bundle by the PADDED matrix size -- unblocks our own brain
- `a47109eb3` expert cache: POWERINFER_EXPERT_HEATMAP -- measure access skew before building a pin policy
- `b7c313dcc` ours/1163cb34939fe4a9cb07aec034c5954144497ae9.patch
- `95a4a9cd4` 1-core-types/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `c57b624b9` 1-core-types/0002-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `6e6fffce4` 2-cpu-reference/0001-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `8d3e64baf` 2-cpu-reference/0007-ggml-cpu-declare-turbo3_cpu_wht_group_size-extern-no.patch
- `a0ea176ea` 2-cpu-reference/0008-merge-fix-post-merge-build-and-crash-issues.patch
- `bef8e4fc9` 3-vulkan/0001-vulkan-metal-hip-add-TurboQuant-kernel-support.patch
- `f87714b3b` 3-vulkan/0002-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch
- `1011cbe0c` 3-vulkan/0003-fix-close-complete-audit-findings-7-port-regressions.patch
- `665194368` 3-vulkan/0004-vulkan-reconstruct-supports_op-after-rebase-merge-da.patch
- `61d2a5b4d` 3-vulkan/0006-vulkan-restore-TQ4_1S-weight-type-wiring-lost-in-the.patch
- `e49188d7d` 3-vulkan/0007-vulkan-add-the-TQ3_1S-weight-type.patch
- `4b2bad742` 3-vulkan/0008-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch
- `ed0811236` 3-vulkan/0013-vulkan-TQ-rotated-matmul-shader-groundwork-A-side-lo.patch
- `47e9ed806` 3-vulkan/0015-vulkan-create-the-TQ-activation-rotate-pipeline.patch
- `cacdbf32d` 3-vulkan/0017-vulkan-rotate-the-staged-activation-copy-in-place-dr.patch
- `f591884cb` 3-vulkan/0018-vulkan-register-the-TQ-rotated-mul_mm_id-pipelines.patch
- `26f8aae4f` 3-vulkan/0021-vulkan-fix-SET_ROWS-block-decomposition-for-turbo-we.patch
- `c1a9f9912` 4-other-backends/0006-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch
- `3d3580de7` 4-other-backends/0007-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch
- `98cc91268` 4-other-backends/0008-hip-add-mixed-f16-bf16-q8_0-fattn-vec-instances-to-t.patch
- `042e9b82e` 4-other-backends/0009-cuda-revert-GQA-ratio-dispatch-to-comparison-form-fi.patch
- `7dbbea15f` 4-other-backends/0010-cuda-restore-Volta-GQA-modulo-dispatch-over-revert-i.patch
- `54978e401` 4-other-backends/0015-cuda-rework-MoE-expert-cache-execution.patch
- `db786092f` 4-other-backends/0016-cuda-fix-MoE-cache-capacity-accounting.patch
- `52a5cfb6e` 4-other-backends/0017-cuda-fix-MoE-cache-scratch-budget-replacement.patch
- `6b325964a` 4-other-backends/0018-cuda-bound-MoE-MMV-tail-row-reads.patch
- `5fc3a0ed9` 4-other-backends/0019-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `76dffada3` 4-other-backends/0020-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `ee8db7983` 4-other-backends/0021-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `f365fb75f` 4-other-backends/0022-cuda-prefer-generic-MMV-for-compatible-MoE-cache-nod.patch
- `6c19040f9` 4-other-backends/0023-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch
- `2119e81a0` 4-other-backends/0024-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `8989261c6` 4-other-backends/0025-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `97c79b5e5` 4-other-backends/0026-cuda-bound-automatic-MoE-cache-admission.patch
- `fa26043e3` 4-other-backends/0027-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `2fd6b3167` 4-other-backends/0028-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `262c414b4` 4-other-backends/0029-common-surface-MoE-cache-activation.patch
- `833c5eab9` 4-other-backends/0030-cuda-accelerate-complete-MoE-cache-pools.patch
- `3d0889ad9` 4-other-backends/0031-cuda-report-oversized-MoE-cache-nodes.patch
- `4f1b210dc` 4-other-backends/0032-cuda-clarify-MoE-cache-session-diagnostics.patch
- `a97cf9c5c` 4-other-backends/0033-cuda-report-MoE-cache-pair-residency.patch
- `7950475e8` 4-other-backends/0034-cuda-pack-MoE-cache-dispatch-inputs.patch
- `fcb0143db` 4-other-backends/0037-metal-port-TQ_NO_ROTATE-escape-hatch-from-stash-opt-.patch
- `93b221406` 4-other-backends/0039-moe-cache-enable-HIP-backend-by-removing-no-op-stubs.patch
- `af8b6262d` 4-other-backends/0040-moe-cache-count-dispatch-contention-bypasses-P1.patch
- `c4a21f972` 4-other-backends/0041-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `86475b3a5` 4-other-backends/0042-moe-cache-audit-fixes-H1-F1-F2-A1.patch
- `638f12227` 4-other-backends/0043-moe-cache-fix-registry-build-I1-follow-up.patch
- `484a71de8` 4-other-backends/0044-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `9ec3cd7c8` 4-other-backends/0045-cuda-fix-HIP-MoE-cache-compatibility-and-q8_1-sums.patch
- `2d7d47351` 4-other-backends/0046-moe-cache-add-logging-at-silent-failure-points-acros.patch
- `5aca05ef5` 4-other-backends/0048-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch
- `47e151678` 4-other-backends/0049-sycl-drop-the-duplicated-nthreads-declarations-in-fa.patch
- `ecb2e5fc7` 4-other-backends/0050-fattn-vec-split-the-turbo-K-dot-at-D-128-to-stop-the.patch
- `5c7fb6396` 5-models-server/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `7cc7134e5` 5-models-server/0003-cli-break-interactive-loop-on-stdin-EOF-fixes-hot-sp.patch
- `692472db0` 5-models-server/0004-tools-expose-TQ3_1S-TQ4_1S-in-quantize-table-and-lla.patch
- `6cd8dddda` 5-models-server/0007-llama-support-DeepSeek-V4-tensor-split.patch
- `3f80542b5` 5-models-server/0008-llama-mirror-DS4-q_a-kv-down-projections-in-tensor-s.patch
- `b81705266` 5-models-server/0009-glm-dsa-guard-lightning-indexer-Hadamard-rotation-wh.patch
- `2f10753d4` 5-models-server/0010-laguna-add-arch-tables-llm_arch-KV-keys-tensors-and-.patch
- `c3cbe241b` 5-models-server/0011-laguna-add-model-class-hparams-wiring-vocab-pre-toke.patch
- `c784b11be` 5-models-server/0013-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch
- `0af30cf91` 5-models-server/0014-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch
- `bd5b182cb` 5-models-server/0015-cuda-sum-MoE-expert-outputs-on-decode-n_tokens-1.patch
- `49fa51144` 5-models-server/0016-laguna-deduplicate-definitions-vs-the-early-snapshot.patch
- `6829f6f2c` 5-models-server/0017-dflash-fix-DSpark-tensor-meta-accessor-after-laguna-.patch
- `93ee8e5e1` 5-models-server/0021-common-support-the-DSpark-sidecar-resolution-26458.patch
- `9037b2844` 5-models-server/0037-merge-fix-post-merge-build-and-crash-issues.patch
- `145f90a69` 5-models-server/0041-moe-cache-route-fit-probing-and-test-session-calls-t.patch
- `f4571d0ee` 5-models-server/0048-llama-bench-document-pw-prefetch-weights-in-help-and.patch
- `89633f681` 5-models-server/0050-llama-bench-remove-stale-prefetch-weights-documentat.patch
- `7fd149a1e` 5-models-server/0051-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch
- `289d1bcf1` 6-tests-build/0001-tests-re-add-test-turbo-quant.c-round-trip-test.patch
- `149230524` 6-tests-build/0002-tests-port-fork-turbo-backend-and-quantize-fns-test-.patch
- `486b73741` 6-tests-build/0003-fix-close-complete-audit-findings-7-port-regressions.patch
- `a421e3822` 6-tests-build/0004-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch
- `f76034db4` 6-tests-build/0006-ggml-webgpu-add-support-for-f16-repeat-26307.patch
- `045ef93e0` 6-tests-build/0010-tests-restore-DSV4_HC_COMB-eps-sweep-lost-in-the-244.patch
- `0b046454e` 6-tests-build/0011-tests-cast-float-printf-args-in-test-turbo-quant-arm.patch
- `e706bb5b5` 6-tests-build/0012-vulkan-add-the-TQ3_1S-weight-type.patch
- `060bebd7f` 6-tests-build/0013-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch
- `711810814` 6-tests-build/0014-ggml-cpu-remove-the-per-call-mallocs-from-the-TurboQ.patch
- `e88c632d1` 6-tests-build/0015-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch
- `97449e612` 6-tests-build/0016-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch
- `06fdbf323` 6-tests-build/0017-tests-port-11-DSv4-shaped-TQ-MUL_MAT-cases-from-sync.patch
- `8f1fa85f7` 6-tests-build/0019-cuda-fix-MoE-cache-capacity-accounting.patch
- `30e53401b` 6-tests-build/0020-common-preserve-implicit-MoE-cache-provider-settings.patch
- `a60a9534c` 6-tests-build/0021-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `7ee740275` 6-tests-build/0022-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `94cba10d7` 6-tests-build/0023-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `476394bed` 6-tests-build/0024-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch
- `b27983cf3` 6-tests-build/0025-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `b6628df21` 6-tests-build/0026-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `45bacf628` 6-tests-build/0027-cuda-bound-automatic-MoE-cache-admission.patch
- `28563af4c` 6-tests-build/0028-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `9efa7d6fb` 6-tests-build/0029-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `b0bae2980` 6-tests-build/0030-common-surface-MoE-cache-activation.patch
- `17e56bc51` 6-tests-build/0031-cuda-accelerate-complete-MoE-cache-pools.patch
- `311ae06c2` 6-tests-build/0032-cuda-report-oversized-MoE-cache-nodes.patch
- `af6a0471d` 6-tests-build/0033-tests-initialise-non-contiguous-tensors-row-by-row.patch
- `4fd14bb50` 6-tests-build/0034-tests-cover-turbo-KV-flash-attention-at-head-dim-256.patch
- `53ba14cc7` 6-tests-build/0037-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `4861e56e1` 6-tests-build/0038-moe-cache-route-fit-probing-and-test-session-calls-t.patch
- `845dca09c` 6-tests-build/0039-moe-cache-route-context-eligibility-probe-and-remain.patch
- `dc43cc88d` 6-tests-build/0040-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `4f2e9e899` 6-tests-build/0041-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch
- `a333dc741` 6-tests-build/0042-test-moe-cache-accept-IGPU-devices-not-just-GPU.patch
- `133569377` 6-tests-build/0043-test-moe-cache-make-cache-shared-budget-UMA-aware-wi.patch
- `71d6e40b3` 6-tests-build/0044-test-moe-cache-save-and-restore-the-CUDA-device-arou.patch
- `94d3be281` 6-tests-build/0045-tests-tell-ctest-that-77-means-skip-for-test-moe-cac.patch
- `097f49a79` resolved/0009-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch
- `1c9195f82` resolved/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch
- `5204b4ac0` resolved/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch
- `f1be65178` resolved/0005-merge-close-TurboQuant-parity-gaps.patch
- `e2398f694` resolved/0036-common-surface-MoE-cache-activation.patch
- `5e9f9ac0d` resolved/0019-vulkan-dispatch-the-TQ-rotated-mul_mm_id-path.patch
- `88014c397` resolved/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch
- `9568085fa` resolved/0004-fix-close-complete-audit-findings-7-port-regressions.patch
- `379934ec8` resolved/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch
- `4b3fedf7f` resolved/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch
- `c13f985dc` resolved/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch
- `b9715ac05` resolved/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch
- `50c177811` resolved/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch
- `5f0a08aa7` resolved/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch
- `04b1dcb74` resolved/0006-fix-close-complete-audit-findings-7-port-regressions.patch
- `19fac938f` resolved/0022-DeepseekV4-MTP-DSpark-25784.patch
- `a642aee7b` resolved/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch
- `c3f6c993c` resolved/0029-server-harden-shared-draft-device-placement.patch
- `e78b3ca2e` resolved/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch
- `9da482fd6` resolved/0026-cuda-rework-MoE-expert-cache-execution.patch
- `bb312d715` resolved/0033-server-account-for-MTP-placement-in-fit-reservations.patch
- `f6588b8e2` resolved/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `f42422d44` resolved/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch
- `6e10fc420` resolved/0039-model-Muse-Glimmer-Support-26841.patch
- `e4f8f028a` resolved/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch
- `d7b3dbf7d` resolved/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch
- `2dd362d5a` resolved/0046-llama-remove-dead-MSA-indexer-scaffolding.patch
- `9311a7acc` resolved/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch
- `0f966e10c` resolved/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch
- `a7793fb24` resolved/0005-test-fix-some-CI-errors-26415.patch
- `693bf2e98` resolved/remaining-conflict-markers
- `951d4afda` layer7/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `c5f7aa4cd` layer7/0002-Update-GGMLQuantizationType-and-LlamaFileType-enums-.patch
- `2b8222b7b` layer7/0003-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `a9b6fe8c6` layer7/0005-docs-add-rebase-plan-and-TurboQuant-recipes.patch
- `e3b10a63c` layer7/0007-gguf-py-dedupe-stacked-merge-artifacts-in-constants..patch
- `0a6c2b275` layer7/0008-docs-add-KV-cache-quantization-guide-update-rebase-p.patch
- `c42b56de5` layer7/0009-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch
- `286cbbf2d` layer7/0010-merge-close-TurboQuant-parity-gaps.patch
- `3872af19a` layer7/0011-metal-restore-lost-kernel-templates-add-offline-shad.patch
- `70aa80189` layer7/0014-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch
- `65049862f` layer7/0019-fix-apply-rebase-audit-fixes-stack-overflow-quality-.patch
- `d91a64dec` layer7/0020-docs-add-TurboQuant-project-overview-to-AGENTS.md-an.patch
- `fe235ab50` layer7/0021-cuda-rework-MoE-expert-cache-execution.patch
- `2ddaf21ff` layer7/0022-docs-update-MoE-cache-validation.patch
- `2a0ed2605` layer7/0023-common-preserve-implicit-MoE-cache-provider-settings.patch
- `62156922b` layer7/0024-server-harden-shared-draft-device-placement.patch
- `7dd14d193` layer7/0025-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `998a4679b` layer7/0026-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `b9c77c22f` layer7/0027-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `f5d663faa` layer7/0028-docs-update-MoE-cache-validation.patch
- `f6e292523` layer7/0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `5e0a9ecfb` layer7/0030-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `04c7f4e48` layer7/0031-cuda-bound-automatic-MoE-cache-admission.patch
- `ca10593ee` layer7/0032-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `4c3c4dd32` layer7/0033-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `26d6b3d94` layer7/0034-common-surface-MoE-cache-activation.patch
- `fee15f5ba` layer7/0035-docs-fix-MoE-cache-benchmark-verbosity.patch
- `6f6428621` layer7/0036-cuda-accelerate-complete-MoE-cache-pools.patch
- `d20fd194f` layer7/0037-cuda-report-oversized-MoE-cache-nodes.patch
- `9245f9689` layer7/0038-cuda-clarify-MoE-cache-session-diagnostics.patch
- `72ea4f65b` layer7/0039-docs-record-MoE-cache-workload-convergence.patch
- `3039f50b9` layer7/0040-cuda-report-MoE-cache-pair-residency.patch
- `7357c811c` layer7/0041-docs-document-what-the-test-suites-do-and-do-not-cov.patch
- `df36d27e6` layer7/0044-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `ac1566425` layer7/0046-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `f0e9b1573` layer7/0048-docs-document-INFO-level-MoE-cache-disable-reasons-i.patch
- `5d470b2b1` layer7/0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CACHE.md-and-re.patch
- `2b9175384` layer7/0050-docs-correct-MOE-CACHE.md-against-the-implementation.patch
- `ff72da4fe` layer7/0051-ggml-split-the-turbo3-group-size-declaration-from-it.patch
- `a66d44da4` resolve 8 committed conflict markers on merit
- `02cc66ec9` fix the merge so it builds: 5 files, 6 defects
- `3345c796d` merge repairs: seven defect classes, and one the compiler could never have reported
- `f20594cd1` ROCmFPX on Vulkan: the shader half, ported. NOT YET WORKING - it segfaults in execution.
- `0e58a39f9` currency: register ALL SEVEN forks, so nothing can drift unseen again
- `52ea962c4` series: base and the branch it describes are ONE pair, and the linearity rule is now a gate
- `1167b0d39` hygiene: the code-review skill taught a type-ID map the arbitration deleted, and the UI was fetched from a moving tag
- `3ff3caded` contract: tq3 is the first family we must renumber, so write down why before anyone ports it
- `3b089d8d6` sources: honest pins - turboquant re-pinned off a dead lineage, tq3 main-understates note REFUTED, five type-id contract collisions recorded (lane-143)
- `5390271f3` sources: lane-143 self-refutation - the turbo3 ODR 'defect' does not exist in our tree (GGML_API already carries extern on all four branches); prisml pin REVERTED to keep the BEHIND signal live; FLAG E severity re-derived from loader code
- `4ec84e216` sources r2 (checker BLOCK F1-F6): ciru 160 was 145 commits of rocmfpx history - corrected to a 7-15 bracket, no-ancestor premise refuted, GOAL-3 ciru review performed, rocmfpx.ref revert, prisml contradiction removed, FLAG E sensitivity added
- `bcb0fc6ce` series: point base.ref at the LINEAR branch - recut-linear/2026-08-17 is the byte-identical flatten of the blessed union branch, so the series is replayable at last (lane-147)
- `75663c142` series: disclose in the record itself that the flatten absorbed 17 empty commits - content unchanged, history count changed (lane-147 checker finding 4)
- `9eeb8fddd` ggml: port the tq3 TQ3_4S family codec at ids 48-51 (TYPE-ID-ALLOCATION 3.1.1) - Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb
- `f9184f428` tests: cover the tq3 family in test-quantize-fns and test-backend-ops (kilobyte-scale proof before the 13 GB run)
- `48c09a1d2` contract: tq3 rows 48-51 implemented behind GGML_ARIFI_TURBO_WEIGHT_QUANTS, plus the three corrections the port forces (their 200 = our TURBO2_0; six ids, four destinations; token_embd is Q6_K)
- `17bb3e8c2` tests: tq3_4s/tq3_4se skipped in test-quantize-fns with the measured reason (E3M5 scale caps at 0.492, harness data needs 1.4); tq3_0 given documented 3.5bpw bounds
- `2a7537571` tools: gguf-retag-tq3 - lands LAST, after the coherence proof, per TYPE-ID-ALLOCATION 3.1.1. Decides ambiguous id 46 by measured byte span; five refusals, no in-place path
- `8e2ae2fd1` checker fixes: choose_index restored verbatim (inverted tie-break + 2 drifted constants, encoder-only); tq3 numeric block gives TQ3_4S real proof and pins every dot mapping; drop redundant GGML_API redecl
- `d1fe739cd` retag: verify geometry per TENSOR for every mapped id, not a file-level vote on 46 only - fixes silent rewrite of mixed/ambiguous files and of unguarded 36/200; guard empty and overlapping tables
- `762dd9ca8` retag README: per-tensor geometry check for every mapped id, ten refusals, byte-level assertion
- `85c6500a6` lane-144 GOAL 2 step 1: TQ3_4S (id 48) Vulkan port - dequant + mat-vec + mat-vec_id shaders, wired
- `38cb15b5a` lane-144: sweep ROCmFP4/ROCmFPX in test-backend-ops all_types (six identifiers, never listed before)
- `ba9e60dea` lane-144 CHECKER FIX: TQ3_4S was missing from the MUL_MAT_ID staging-overflow guard
- `1f5683957` lane-145: Q6_0_ROCMFPX Vulkan fix - implement the 6-bit unpack (block struct was 34B vs CPU 26B; qs read as whole signed bytes). 60/60 MUL_MAT + 24/24 GET_ROWS on Vulkan0, was 50/60 + 20/24.
- `56af5c640` lane-145 checker finding 7: delete block_rocmfpx_fp6_packed16 + its A_TYPE_PACKED16 define (26B block has no valid uint16 view; nothing reads it). Re-verified 60/60 MUL_MAT, 24/24 GET_ROWS, 12/12 MUL_MAT_ID.
- `8421a64d7` test-backend-ops: cover the CPY types supports_op names that all_types omits
- `c6c2bd2f3` lane-148 defect A ROOT FIX: Vulkan turbo3 centroid LUT was stale at 5 sites - CPY turbo3->f32 ERR 8.4302e-5 -> OK. CPU/CUDA/Metal/SYCL all carry CENTROIDS_3BIT (-0.190207,-0.118786,-0.066822,-0.021663); Vulkan alone carried a table whose midpoints do not match nearest_centroid_3bit(). copy_to_quant.comp's TM decision boundaries were derived from the stale table and are corrected too.
- `a384a6d2c` lane-148 defect B ROOT FIX: FA_TYPE_TURBO2/3/4_0 GLSL spec-constant ids were stale (43/44/47) while enum ggml_type puts them at 200/201/202. Host passes the raw enum, so both switches fell through: fa_block_elems() returned its 1u default and dequantize4() returned vec4(0), zeroing V and the whole FA output. FLASH_ATTN_EXT -p turbo 0/1728 -> 1728/1728.
- `53a0cf264` lane-148 F-69 sibling coverage: test-backend-ops FLASH_ATTN_EXT now covers TURBO2_0 too. Vulkan supports_op has always claimed turbo2 for FA but no test case existed, so its stale FA_TYPE id was invisible. 1728 -> 2592 cases, all PASS.
- `26d061cc8` lane-148 checker findings 4+5 FIXED: turbo4 SET_ROWS was 0/21 declined behind a green 'Backend Vulkan0: OK' banner and turbo2 had no SET_ROWS case at all. Root cause: no cpy_turbo{2,4}_0_f32 copy-from-quant shader, so the tests' read-back leg declined. Added turbo2/turbo4 dequantize()/dequantize4()/get_dm(), the two spv+pipeline registrations, both supports_op and the DISPATCH switch (a third hand-synced pair - supports_op alone aborts with 'Missing CPY op'), and a test_set_rows_turbo2 class. SET_ROWS_TURBO2/3/4 now 21/21 EXECUTED each.
- `c107494bd` lane-149 F-110 guard (rung b): arifi_sync_check.py + CMake wiring + 16 ARIFI-SYNC markers. Checks 49 LUT pairs, 16 FA_TYPE ids, 35 QUANT_K block sizes, 4 type-list SETs, FA K/V coverage; unregistered mirrors FAIL
- `7f9fd898f` lane-149 checker-driven hardening of arifi-sync-check: FA_TYPE/QUANT_K literal spellings, ordinal (not line) pins, per-definition completeness, any array type, recursive shader walk, scalar-mirror class, expected-count assertions
- `b277882c7` lane-149 F-112 second half (checker finding 11): per-op tallies name every op whose cases were ALL declined, so a zero-coverage family cannot hide inside a green full sweep
- `a48d3e5c5` lane-150 step 1: TQ4_1S read-back leg wired - CPY(tq4_1s,f32) now EXECUTES, 291/291 -> 293/293
- `491d1dac8` lane-150 step 2: TQ4_1S write leg EXECUTES - SET_ROWS_TQ4_1S 17 cases/0 executed -> 17/17 PASS, and the inherited 5.0 waive is retired
- `32cdb2b40` lane-150 step 3 (root fix for the hole that hid the whole class): per-op x TYPE zero-coverage tally
- `3865f3e52` lane-150 checker-driven fixes: type_bucket prefix-boundary bug + dispatch block-size assert + the tallys limits named in code
- `0dddb2321` lane-151 GOAL 6 hygiene: drop 648 tracked _lane-out debris files (incl. an embedded shadow.git with commit receipts) and gitignore the dir; genericize estate paths in docs/, licenses/, tools/gguf-retag-tq3
- `3337f1ef8` lane-151 GOAL 9: reserve tq3's three destination-less serialized ids (37 TQ3_4SV -> 52, 31 TQ3_1S_AP1 -> 53, 44 TQ3_1S -> 54) as RESERVED-not-implemented in docs/TYPE-ID-ALLOCATION.md 3.1.2; geometry marked NOT DERIVED, no enumerators added, retag-tool refusals unchanged
- `6ffd79fd6` lane-151 GOALs 6+7: README refreshed to b10453 canonical arifi-main + the seven-ingested-sources table (two honest zeroes named); tq3 and ciru credits added; licenses/README.md gains the full Taken-from audit incl. the tq3 license GAP; UPDATE-RUNBOOK.md written (O(delta) recipe, THREE-flag configure, guards, the standard)
- `471532254` lane-151 GOAL 2: sources.json base.ref -> arifi/main (the canonical branch is now the series base) + series regen over the full canonical range: 323 patches, series check PASS, replay onto 4df29be4f identical outside patches/series
- `55806de75` lane-151 GOAL 5: gitignore .tokensave/ - the fork repo now carries its own code graph (63344 nodes, 154451 edges over 3407 files) and the index is per-machine, never committed
- `e46ec904a` lane-151 CHECKER ROOT FIX: series check now asserts every patch is a COMMITTED BLOB, not just a file on disk. The byte-compare ran against the working directory, so untracked patches made it green while a fresh clone would die at git am - the defect that hid 33 missing patches twice. Help text corrected to match what the code does
- `eb60ac543` lane-151 CHECKER FIXES (docs): UPDATE-RUNBOOK 4.5 no longer asserts a self-containment standard nothing on this estate meets - the four MinGW runtime DLLs are named and staged, and the measured stripped-PATH failure is recorded as identical for the raw build tree and the previous engine. licenses/README.md audit header stops claiming a purely mechanical trailer derivation: the tq3 row is a SUBJECT-line citation git never parses as a trailer, which is exactly why a trailer-only sweep would have missed the one real license gap
- `f7eaf83a4` lane-151 HQ DIRECTIVE: RETAIN the licenses instead of flagging them. tq3-MIT, prisml-MIT and ciru-MIT extracted verbatim from this repo's OWN fetched remotes (git show refs/remotes/<r>/<head>:LICENSE). REFUTES my own GOAL-6 finding and a year-old one: I wrote 'no clone remains on the estate' and Phase 0 wrote 'PrismML has no LICENSE' - both were absence claims checked against the wrong artifact while the remotes sat fetched in this very repository. All seven registered sources audited at once. thecodacus STAYS open: no remote, no text, nothing invented
- `679ceb044` licenses: thecodacus-MIT retained from the public GitHub fork - the last open attribution CLOSED (no local remote != no source; President's correction 2026-08-18)
- `800ce0e39` lane-152 GOAL 1: upstream bump b10453 -> b10481, the FIRST live execution of UPDATE-RUNBOOK 2. 348 patches replayed onto 25ae3a9b3, 4 conflicts resolved by hand, series regenerated to 329, zero merges.
- `6a96b5620` lane-152: what the FIRST live run of UPDATE-RUNBOOK section 2 corrected - one build break, two arifi_sync defects, the runbook itself, and the last license row
- `709879d52` lane-152 GOAL 2 batch 1: ciru's Vulkan SPIRV-Headers fallback, tq3's f32 hybrid-SSM state gates, rocmfpx's -ffast-math guard
- `cf45b4a38` lane-152 GOAL 2 batch 2: three lfm2 pre-tokenizer hashes that only rocmfpx had - and the reason they must be pre-computed
- `5118405f4` lane-152 pins: THREE advance with reviews (upstream, rocmfpx, tq3), TWO deliberately stay BEHIND (prisml, ciru)
- `5e05d75a1` lane-152 checker F7: the old base is tag b10454, not b10453 - correction of record
- `2682ce141` repo-env.json (WI-1540 tail): the engine repo's env manifest - PowerShell-only law (F-107), three-flag configure (F-111), cmd.exe redirection (F-108), type-id FLAG E, series discipline, runbook pointer
- `40b2af9c8` WI-1580 sub-2: UPDATE-RUNBOOK section 7 - the 4.5 line was stale. lane-153 staged models/engines/arifi-b10481-union/ from a verified build of canonical, smoked from the staged copies, and flipped current-state.json + artifact-catalog.json, so 4.5 HAS now been executed once, by hand. The bump distinction is KEPT because it is still true: no bump has ever driven staging end to end (lane-152's bump refused at leg 2). Narrow edit - the section-2 and section-3 paragraphs around it are untouched. Note for the record: the lane-153 REPORT cites this text at UPDATE-RUNBOOK.md:238-244; it actually lives at :308-323
- `547cd7bd3` graph: derive the V-unpad head count from the attention output tensor, not from hparams
- `faebd6c1b` sources: advance the turboquant pin f6124e914 -> 7ebcbb0b6 with pin_review_2026_08_18_lane154
- `9b7316610` off-rig backends: land the 12 CUDA/Metal/SYCL/HIP rows unbuilt, per the HQ ruling of 2026-08-18
- `bf37bea8e` sources: pin_review names the 12 off-rig shas and their BUILD-UNPROVEN status (HQ condition 3)
- `c5a4450f6` sources: correct the pin_review - the contested 12/12 apply-conflict count is no longer the justification (checker F1)
- `5a004c1f6` sources: pin_review records the apply-count claim as WRONG, not disputed (checker F1, resolved against me)
- `d8c294b6f` base: b10481/25ae3a9b3 -> b10488/9d77fa172 + series regen 346 (lane-156 upstream bump)
- `7156bd797` moe-cache: heat-protected eviction keeps hot experts resident (turboquant tail)
- `0cbc6238d` tests: initialise non-contiguous tensors row by row (turboquant row f58ee0e97)
- `28dabf813` sources: advance the turboquant pin 7ebcbb0b6 -> d14e36827 with a dated lane-156 pin_review
- `a2f2cdd27` sources: prisml pin HELD deliberately with a sharper reason + the v6 K-cache-mean-center verdict (lane-156 GOAL 4)
- `04ea0f3fc` sources: ciru re-derived 315 -> 15 and HELD with all 15 reviewed (lane-156 GOAL 5)
- `2afdab5a0` sources: advance remotes.upstream.pin 25ae3a9b3 -> 9d77fa172 (b10488) with its review
- `57e2d896a` sources: correct the turboquant pin_review's FALSE blocker for e130aef60 (checker F8, BLOCKING)
- `3acc2af7e` sources: trim the e130aef60 blocker to the ONE item that survives (checker pass 2, advisory 1)
- `aaf20fde1` feat(kv-cache): port flag-gated mean centering
- `38378c66e` feat(kv-cache): bind calibration to exact model
- `663a8f4d0` fix(server): keep target calibration out of draft contexts
- `07c98aaab` test(kv-cache): add Vulkan tensor probe and bench-pure A/B harness
- `de65a7008` sources: record lane-158 PrismML K-cache disposition
- `043fb3a4a` test(kv-cache): fail-close default-OFF binary identity
- `4010317b1` test(kv-cache): assert both rotation-basis polarities against the resolved attn_rot_k
- `812febcd1` fix(kv-cache): make the A/B harness able to launch, and actually enforce the RAM floor it claims
- `15b9d2eb1` fix(kv-cache): floor-check the model named by -m, not the largest .gguf on the command line
- `b6a66f917` fix(kv-cache): wait for the RAM floor between arms, and pin -fit off so the A/B arms match
- `409b7f4c6` fix(kv-cache): probe the layer ids the calibration actually has, and let an existing calibration be reused
- `3aa479a04` support DFlash2
- `93bfccc4c` dflash2: build fixes on the PR #27342 pick - dedupe DFLASH_BLOCK_SIZE enum/hparam/kv-map (fork DFlash1 originals keep authority, default 16) + ml. reference fix (the shadow the fork's own comment documents)
- `0ac817851` sources: register buun (spiritbuun/buun-llama-cpp) as a greedy-ingestion watch source - President ruling, VBR/KV + DFlash2-for-3.8 lineage
- `e7ef1fe3f` cuda: lift TQ3_4S kernel set from tq3/master (14 files)
- `e1706011f` metal: lift TQ3_4S kernel set from tq3/master (5 files + alloc hook)
- `3846d1754` metal: declare the tq3_rht pipeline getter in ggml-metal-device.h
- `a5f8b0e36` sync: move base b10488 -> b10524, regenerate series (372 patches)
- `708163476` spec: ingest turboquant MTP-boost wave + effective-KV bench reporting
- `cadea7a8d` sync: re-pin wave-3 sources + regenerate series (374 patches)
- `3cecbd959` arifi-sync: provenance native-class root fix - sha-pinned grandfather ledger + native Origin convention
- `37672b33a` escha: native Escha-W2 types (ESCHA2=55/ESCHA3=56) + fused GGML_OP_ESCHA_MM, CPU + Vulkan
- `6c5915aea` escha: F-125 fix - full 64-bit pair sourcing + row-parity correction (K=2 AND K=3)
- `aa2b60177` escha: test-backend-ops — un-nest ESCHA_MM eval cases + multi-chunk ncols coverage + perf ncols curve
- `68f315573` escha vulkan: bound every escha_mm dispatch to 32 columns (F-124 host-freeze guard)
- `8fb570d79` escha vulkan: column-blocked escha_mm - one weight decode serves 4 columns (part 2)
- `3724953ff` escha vulkan: column block C=8 + fall back to the one-column kernel below a full block
- `649326e52` escha tests: cover the column-blocked kernel AT MODEL DEPTH (ncols=9, real 27B shapes)
- `eebdb62d5` escha vulkan: one hardware f32->f16 convert instead of the 15-instruction software RTE
- `b4d497529` escha vulkan: subgroup-shuffle Hadamard - 14 barriers per input block become 3
- `09a755f81` escha vulkan: the hardware f32->f16 convert must be float16_t, NOT packHalf2x16 (RTZ)
- `da609f065` escha vulkan: ESCHA_SG_HADAMARD defaults OFF - the lever is refuted by measurement
- `fc725bb08` escha vulkan: f16-native generator - decode +18%, bit-exact
- `7d74886d1` escha vulkan: two more levers tried and REFUTED by measurement - occupancy and packed-pair
- `e7d56f975` escha vulkan: the multi-column threshold was disabling speculation - decode 3.96 -> 5.60 t/s
- `33718bdd1` escha vulkan: column ladder C2/C4/C8/C16, and the generator table measured and REFUTED
- `5bc3068e1` dflash: ingest buun's DFlash2 adaptive controller - built, gated, and defaulted OFF by measurement
- `cdae587c0` escha vulkan: prefill is OCCUPANCY-bound - coopmat and wider rungs both refuted, and the profile that said otherwise was lying
- `55961be81` escha vulkan: f16 activation staging - built, correct, and refuted; the real prefill win was the build
- `19c1174cb` dflash: reject an out-of-extent draft depth instead of silently clamping it, and block the mask token
- `c66454aa4` spec: load the draft model from the same variable the log prints, and add an env-gated DFlash verify trace
- `2321e601d` spec: ON-DEVICE speculative checkpoints - drafter now BEATS plain (8.02 vs 4.5 t/s, byte-clean)
- `f77dbab5f` spec: ring rollback re-enabled - DFlash2 depth 3 = 8.80 t/s (2.0x plain), 0 degenerate
- `4dcf0a27e` tests: zero_gather + CPY/GET_ROWS Vulkan probe cases; server: depth>cap clamps with warning instead of load abort (F-136 arc)
- `6a46f775c` rs zeroing hunt CLOSED: leak is backend-independent (CPU repro), in-graph rs_z zero never lands on the live cache tensor; zero-on-clear promoted belt->contract. Env-gated discriminators: PDH hash, plane0/sl/rl belt scoping, zero-gather-out, hybrid set_input trace
- `767cb29a6` arifi-sync: base move b10524/9ee9fc04c -> b10636/4d19b2876 (runbook 2.2 manual path); upstream pin advanced as the same pair; prisml + tq3 pins HELD with 3 reviews recorded
- `80c4f85d9` b10636 bump repair: restore upstream's DOTS3NOTE indexer arch + the two TAG_LLAMA_SEQ_ID_NEG TODOs that a whole-file --theirs resolution reverted. checkout --theirs takes stage 3 (the WHOLE file), not just the conflicting hunks - upstream's 4-line b10524..b10636 delta to this file was lost with it. GLM_DSA is absent here PRE-EXISTING (also absent on lane165-pre-bump-arifi-main), so it is NOT restored by this commit and stays a President item.

