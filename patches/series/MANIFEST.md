# ArifiLabs patch-series MANIFEST

<!-- GENERATED FILE - DO NOT EDIT BY HAND.
     Regenerate:  python tools/arifi-sync/arifi_sync.py series regen
     Verify:      python tools/arifi-sync/arifi_sync.py series check  -->

## What this is

`patches/series/` is the complete, ordered, replayable definition of everything ArifiLabs
adds to upstream llama.cpp. It is generated from git, never hand-maintained, and an
integrity check refuses to pass if it has drifted from git by so much as a byte.

- Base: `ggml-org/llama.cpp` tag `b10488`, `9d77fa17254e1dee4b9e92504c91611a60b1359f`
- Patches: **353**, all non-merge, applied in filename order.

## Applying the series

```bash
git checkout -b my-rebuild 9d77fa172
git am patches/series/*.patch
```

The result is byte-identical to `master` everywhere outside `patches/series` itself:
same file contents, same 353 commit messages, same provenance trailers. Verified, not
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
git log --oneline 9d77fa172..master -- patches/series
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
| 1 | `0001-chore-establish-ArifiLabs-fork-identity-and-retained.patch` | arifi-fork-base | `7215b97d7` | ggml-org/llama.cpp@571d0d540df04f25298d0e159e520d9fc62ed121, Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4, charlie12345/ROCmFPX@a6a93765f7ce9779c13f9881164a65f7a9f31198 | - | chore: establish ArifiLabs fork identity and retained notices |
| 2 | `0002-docs-add-options-registry-and-unified-A-F-catalog.patch` | arifi-fork-base | `3923c7a77` | ArifiLabs local-inference catalog and trial evidence | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PROF`, `LANE110_PREFETCH_CAP` | docs: add options registry and unified A-F catalog |
| 3 | `0003-ci-add-Windows-Vulkan-and-bench-smoke-placeholders.patch` | arifi-fork-base | `661edcf24` | - | - | ci: add Windows Vulkan and bench-smoke placeholders |
| 4 | `0004-docs-require-loud-fail-upstream-currency-procedure.patch` | arifi-fork-base | `5a4e85ed3` | ggml-org/llama.cpp@571d0d540df04f25298d0e159e520d9fc62ed121, charlie12345/ROCmFPX@a6a93765f7ce9779c13f9881164a65f7a9f31198, PrismML-Eng/llama.cpp@ternary-Q2_0_g128-lineage, Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | docs: require loud-fail upstream currency procedure |
| 5 | `0005-powerinfer-add-MoE-pipeline-fused-sparse-ggml-ops-se.patch` | powerinfer-streaming | `14d777776` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: add MoE pipeline + fused-sparse ggml ops [series 0001] |
| 6 | `0006-powerinfer-CPU-kernels-dispatch-for-streamed-MoE-ops.patch` | powerinfer-streaming | `12ce69c54` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: CPU kernels + dispatch for streamed-MoE ops [series 0002] |
| 7 | `0007-powerinfer-vendor-streaming-library-incl.-lane-110-W.patch` | powerinfer-streaming | `485dcaf34` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED` | powerinfer: vendor streaming library incl. lane-110 Windows port [series 0003] |
| 8 | `0008-build-wire-powerinfer-lib-AVX-spill-cure-into-CMake-.patch` | powerinfer-streaming | `2a0f2eab5` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | build: wire powerinfer lib + AVX-spill cure into CMake [series 0004] |
| 9 | `0009-powerinfer-expert-bundle-loader-GENERATE_EXPERT_BUND.patch` | powerinfer-streaming | `c1ee9290e` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | powerinfer: expert-bundle loader + GENERATE_EXPERT_BUNDLE [series 0005] |
| 10 | `0010-powerinfer-generic-MoE-streaming-hook-SiLU-op-type-s.patch` | powerinfer-streaming | `8b4eed40e` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP` | powerinfer: generic MoE streaming hook, SiLU op-type, standalone expert prefetch [series 0006+0009] |
| 11 | `0011-powerinfer-per-ubatch-pipeline-init-guarded-reuse-PR.patch` | powerinfer-streaming | `0305229ab` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `LANE110_PROF` | powerinfer: per-ubatch pipeline init, guarded reuse, PROF field [series 0007+0010] |
| 12 | `0012-powerinfer-repack-carve-out-for-streamed-tensors-ser.patch` | powerinfer-streaming | `530a29f76` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | powerinfer: repack carve-out for streamed tensors [series 0008a] |
| 13 | `0013-powerinfer-down-projection-transpose-fix-series-0008.patch` | powerinfer-streaming | `75ac66769` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: down-projection transpose fix [series 0008c] |
| 14 | `0014-powerinfer-GPU-safe-staging-CPU-zero-copy-LANE110_PR.patch` | powerinfer-streaming | `7d51d7325` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `LANE110_PROF` | powerinfer: GPU-safe staging, CPU zero-copy, LANE110_PROF [series 0011] |
| 15 | `0015-lane-110-document-UI-ON-as-local-default-build-varia.patch` | arifi-fork-base | `b477ac879` | - | `LLAMA_USE_PREBUILT_UI` | lane-110: document UI-ON as local default build variant (static-runtime cure for ki-webui-0xC0000139) |
| 16 | `0016-community-llama-pin-mmap-backed-CPU-weights-for-fast.patch` | community-prefetch | `00196a5c8` | thecodacus/llama.cpp@20f5994bfeb91d24da328077c4b6095998cc9888 | - | community: llama : pin mmap-backed CPU weights for faster H2D uploads |
| 17 | `0017-community-ggml-overlap-offloaded-expert-weight-uploa.patch` | community-prefetch | `3c7bffea2` | thecodacus/llama.cpp@1163cb34939fe4a9cb07aec034c5954144497ae9 | - | community: ggml : overlap offloaded expert weight uploads with compute |
| 18 | `0018-community-ggml-size-prefetch-slots-per-layer-and-fix.patch` | community-prefetch | `7dcc63efa` | thecodacus/llama.cpp@5f83fbbe7c668c59912a1fe09e86a0ef580406c4 | - | community: ggml : size prefetch slots per layer and fix fallback use-after-free |
| 19 | `0019-tools-gguf-add-fail-closed-g128-retagger.patch` | ternary-g128 | `688ec290d` | - | - | tools(gguf): add fail-closed g128 retagger |
| 20 | `0020-tools-gguf-retagger-v2-structure-only-parser-no-tens.patch` | ternary-g128 | `5b1b76c48` | - | - | tools(gguf): retagger v2 — structure-only parser, no tensor materialization |
| 21 | `0021-g128-engine-port-applied-BUILD-BLOCKED-on-copy_to_qu.patch` | ternary-g128 | `8e64c99d3` | PrismML-Eng/llama.cpp@984bf9723, PrismML-Eng/llama.cpp@34dc5812c | - | g128 engine port applied — BUILD-BLOCKED on copy_to_quant.comp shader (Sol's fix owed) |
| 22 | `0022-vulkan-fix-f16-promotion-in-q2_0_g128-copy_to_quant.patch` | ternary-g128 | `bedf37061` | - | - | vulkan: fix f16 promotion in q2_0_g128 copy_to_quant |
| 23 | `0023-vulkan-g128-shader-gen-parity-MMQ-helper-completion.patch` | ternary-g128 | `4d0c642dd` | - | - | vulkan: g128 shader-gen parity + MMQ helper completion |
| 24 | `0024-vulkan-g128-DMMV-repack4-byte-oriented-rewrite.patch` | ternary-g128 | `cb41f97fb` | - | - | vulkan: g128 DMMV repack4 byte-oriented rewrite |
| 25 | `0025-loader-g128-include-get_next_tensor-ctx-arg.patch` | ternary-g128 | `8abedf7f5` | - | - | loader: g128 include + get_next_tensor ctx arg |
| 26 | `0026-loader-use-public-ggml_blck_size-for-g128-drop-inter.patch` | ternary-g128 | `9445a243a` | - | - | loader: use public ggml_blck_size for g128, drop internal include |
| 27 | `0027-tools-gguf-retag-v3-verified-type-id-rewrite-42-43.patch` | ternary-g128 | `b7d21c63a` | - | - | tools(gguf): retag v3 — verified type-id rewrite 42->43 |
| 28 | `0028-loader-accept-native-Q2_0_G128-tensors.patch` | ternary-g128 | `c9fd99d0a` | - | - | loader: accept native Q2_0_G128 tensors |
| 29 | `0029-ggml-cpu-dispatch-Q2_0_G128-at-the-7-Q2_0-sites.patch` | ternary-g128 | `7d8047404` | - | - | ggml-cpu: dispatch Q2_0_G128 at the 7 Q2_0 sites |
| 30 | `0030-port-f16-recurrent-S-state-env-gated-GGML_RECURRENT_.patch` | prismml-recurrent | `6eb6ef686` | PrismML-Eng/llama.cpp@3e0855571 | `GGML_RECURRENT_STATE_F16` | port: f16 recurrent S-state (env-gated GGML_RECURRENT_STATE_F16) |
| 31 | `0031-port-M-RoPE-embedded-batch-position-guard.patch` | rocmfpx-mrope | `3b5b914de` | charlie12345/ROCmFPX@rocmfpx-a6a9376 | - | port: M-RoPE embedded-batch position guard |
| 32 | `0032-docs-phase-3a-OPTIONS-REGISTRY-rows-M-RoPE-f16-recur.patch` | arifi-fork-base | `47066842a` | - | `GGML_RECURRENT_STATE_F16` | docs: phase-3a OPTIONS-REGISTRY rows (M-RoPE, f16-recurrent, DSpark-iGPU) |
| 33 | `0033-port-Windows-IOCP-async-expert-bundle-reads-M2b.patch` | iocp-async | `77a128543` | - | - | port: Windows IOCP async expert-bundle reads (M2b) |
| 34 | `0034-feat-POWERINFER_IOCP-runtime-toggle-for-the-Windows-.patch` | iocp-async | `95478851f` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | feat: POWERINFER_IOCP runtime toggle for the Windows expert-read path |
| 35 | `0035-docs-OPTIONS-REGISTRY-POWERINFER_IOCP-measured-row-s.patch` | iocp-async | `ce0d8e0e0` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | docs: OPTIONS-REGISTRY POWERINFER_IOCP measured row (same-binary A/B) |
| 36 | `0036-docs-correct-POWERINFER_IOCP-row-A-B-was-noise-direc.patch` | iocp-async | `63c5e0831` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | docs: correct POWERINFER_IOCP row - A/B was noise, direction UNRESOLVED |
| 37 | `0037-patches-generate-the-full-auditable-38-patch-series-.patch` | arifi-fork-base | `60fec85b1` | ArifiLabs lane-110C fork-maintainability slot | - | patches: generate the full auditable 38-patch series from git |
| 38 | `0038-tools-add-arifi-sync-the-currency-series-and-upstrea.patch` | arifi-fork-base | `cb03ea262` | ArifiLabs lane-110C fork-maintainability slot | `LLAMA_USE_PREBUILT_UI`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `ARIFI_TOOL_NVFP4_REMAP` | tools: add arifi-sync - the currency, series and upstream-bump driver |
| 39 | `0039-docs-README-section-a-GitHub-visitor-can-read-cold.patch` | arifi-fork-base | `98c763c94` | ArifiLabs lane-110C fork-maintainability slot | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP` | docs: README section a GitHub visitor can read cold |
| 40 | `0040-patches-move-banked-PrismML-source-patches-out-of-th.patch` | arifi-fork-base | `82bc104af` | PrismML-Eng/llama.cpp@ternary-Q2_0_g128-lineage | - | patches: move banked PrismML source patches out of the generated series directory |
| 41 | `0041-patches-make-the-series-generator-implement-its-own-.patch` | arifi-fork-base | `9fe49d715` | - | - | patches: make the series generator implement its own convergence rule |
| 42 | `0042-patches-stop-the-manifest-deriving-anything-from-exc.patch` | arifi-fork-base | `0a272bead` | - | - | patches: stop the manifest deriving anything from excluded commits |
| 43 | `0043-port-Q1_0-repack-scaffolding-Arm-NEON-DP-GEMV-GEMM-k.patch` | arifi-fork-base | `80e707de6` | PrismML-Eng/llama.cpp@720e06b16 | - | port: Q1_0 repack scaffolding + Arm NEON/DP GEMV-GEMM kernels |
| 44 | `0044-port-native-x86-Q2_0-vec_dot-AVX512-VNNI-AVX-VNNI-dr.patch` | arifi-fork-base | `daa30d545` | PrismML-Eng/llama.cpp@4d88cd4eb, PrismML-Eng/llama.cpp@79697f23a | - | port: native x86 Q2_0 vec_dot (AVX512-VNNI / AVX-VNNI), drop scalar fallback alias |
| 45 | `0045-port-x86-AVX512-VNNI-repack-GEMV-GEMM-for-Q1_0-and-Q.patch` | arifi-fork-base | `f26dc1227` | PrismML-Eng/llama.cpp@9fcaed763 | - | port: x86 AVX512-VNNI repack GEMV/GEMM for Q1_0 and Q2_0 |
| 46 | `0046-feat-GGML_ARIFI_VNNI_REPACK-runtime-toggle-explicit-.patch` | ternary-g128 | `33251d885` | - | `GGML_ARIFI_VNNI_REPACK` | feat: GGML_ARIFI_VNNI_REPACK runtime toggle + explicit g64/g128 tripwire |
| 47 | `0047-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_VNNI_REPACK.patch` | arifi-fork-base | `38779fe6d` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY row for GGML_ARIFI_VNNI_REPACK - UNMEASURED, artifact-blocked |
| 48 | `0048-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-CORRECT.patch` | arifi-fork-base | `cdc54b8f5` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - CORRECTNESS FAIL, GEMM returns NaN |
| 49 | `0049-fix-ggml-cpu-parameterize-Q2_0-GEMM-activation-strid.patch` | arifi-fork-base | `732d7ff2f` | PrismML-Eng/llama.cpp (Q2_0 4x8 VNNI GEMM design) | - | fix(ggml-cpu): parameterize Q2_0 GEMM activation stride by QK2_0/QK8_0 - g64 NaN |
| 50 | `0050-fix-ggml-cpu-same-g64-stride-defect-in-the-ported-x8.patch` | arifi-fork-base | `67d0a1392` | - | - | fix(ggml-cpu): same g64 stride defect in the ported x86 Q2_0 vec_dot |
| 51 | `0051-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-NaN-FIX.patch` | arifi-fork-base | `151c74f15` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - NaN FIXED, bit-identity NOT established |
| 52 | `0052-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-bit-div.patch` | arifi-fork-base | `4cf5d407d` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - bit-divergence ADJUDICATED, performance MEASURED |
| 53 | `0053-docs-correct-the-VNNI-row-s-base-commit-labelling-f7.patch` | arifi-fork-base | `2d9ca3a10` | - | `GGML_ARIFI_VNNI_REPACK` | docs: correct the VNNI row's base-commit labelling - f7c4d9d2c is NOT master |
| 54 | `0054-docs-fix-two-stray-table-breaking-pipes-in-the-VNNI-.patch` | arifi-fork-base | `18f4576ee` | - | `GGML_ARIFI_VNNI_REPACK` | docs: fix two stray table-breaking pipes in the VNNI registry row |
| 55 | `0055-docs-OPTIONS-REGISTRY-VNNI-row-topology-cell-now-ref.patch` | arifi-fork-base | `f68af59d5` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY VNNI row - topology cell now reflects the linear-master rebase |
| 56 | `0056-ggml-cpu-GGML_ARIFI_VNNI_REPACK-defaults-to-OFF-dive.patch` | ternary-g128 | `ef8a73c45` | - | `GGML_ARIFI_VNNI_REPACK` | ggml-cpu: GGML_ARIFI_VNNI_REPACK defaults to OFF (diverging from upstream, on measurement) |
| 57 | `0057-docs-fork-wide-GGUF-type-ID-allocation-contract-keys.patch` | arifi-fork-base | `c3b2fb752` | - | `ARIFI_TOOL_NVFP4_REMAP` | docs: fork-wide GGUF type-ID allocation contract (keystone, unblocks 4 slots) |
| 58 | `0058-ggml-reserve-the-ArifiLabs-type-ID-blocks-at-the-enu.patch` | arifi-fork-base | `ceab1af31` | - | - | ggml: reserve the ArifiLabs type-ID blocks at the enum itself |
| 59 | `0059-ggml-ROCmFP4-ROCmFPX-weight-formats-behind-GGML_ARIF.patch` | arifi-fork-base | `58ae9ab4c` | charlie12345/ROCmFPX@3edc3d31e | - | ggml: ROCmFP4/ROCmFPX weight formats behind GGML_ARIFI_ROCMFPX_FORMATS |
| 60 | `0060-llama-ROCmFPX-file-types-quantizer-wiring-and-the-gg.patch` | arifi-fork-base | `4fc75deea` | charlie12345/ROCmFPX@3edc3d31e | - | llama: ROCmFPX file types, quantizer wiring and the gguf-py registry |
| 61 | `0061-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_ROCMFPX_FOR.patch` | arifi-fork-base | `c4b768e5e` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY row for GGML_ARIFI_ROCMFPX_FORMATS |
| 62 | `0062-tools-record-GGML_ARIFI_ROCMFPX_FORMATS-in-the-build.patch` | arifi-fork-base | `235089712` | - | - | tools: record GGML_ARIFI_ROCMFPX_FORMATS in the build-recipe snapshot |
| 63 | `0063-docs-make-the-fork-buildable-by-someone-who-did-not-.patch` | arifi-fork-base | `f7314c051` | - | `LLAMA_USE_PREBUILT_UI`, `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `GGML_RECURRENT_STATE_F16` | docs: make the fork buildable by someone who did not build it |
| 64 | `0064-feat-arifi-profile-and-a-CPU-only-VNNI-repack-startu.patch` | arifi-fork-base | `736666548` | - | `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP`, `MAX_N_CACHED`, `GGML_RECURRENT_STATE_F16` | feat: --arifi-profile and a CPU-only VNNI-repack startup advisory |
| 65 | `0065-sync-bump-base-to-upstream-b10173-e9fa0781f-series-r.patch` | ternary-g128 | `b78babcd7` | upstream b10173 e9fa0781f | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PROF`, `LANE110_PREFETCH_CAP`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | sync: bump base to upstream b10173 (e9fa0781f) — series replayed (1 hand-resolved conflict: loader load-mode refactor e6dd0e29a), judge harness wired + exercised (F-085 llama-server, g128 ternary 27B PASS), pins updated, series regenerated |
| 66 | `0066-sync-currency-GREEN-TurboQuant-checkouts-pinned-at-t.patch` | arifi-fork-base | `49fdfdc47` | - | - | sync: currency GREEN — TurboQuant checkouts pinned at their lane-110C-reviewed shas (c26cbdffc, ba52ad107), rocmfpx pin advanced to db6844d9b after 6-commit review (HIP pinned-staging noted for the HIP-deltas slot) |
| 67 | `0067-fix-ggml-cpu-x86-don-t-pass-__m256i-by-value-across-.patch` | arifi-fork-base | `37b1196c9` | - | - | fix(ggml-cpu/x86): don't pass __m256i by value across the repack GEMV/GEMM ABI boundary (Windows 0xC0000005) |
| 68 | `0068-ggml-TurboQuant-TQ3_1S-TQ4_1S-weight-formats-behind-.patch` | arifi-fork-base | `8337e5de5` | - | - | ggml: TurboQuant TQ3_1S/TQ4_1S weight formats behind GGML_ARIFI_TURBO_WEIGHT_QUANTS |
| 69 | `0069-docs-CHANGELOG-and-FINDINGS-the-public-trail-a-stran.patch` | arifi-fork-base | `6a52ea773` | - | `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP` | docs: CHANGELOG and FINDINGS - the public trail a stranger can actually read |
| 70 | `0070-docs-USAGE-a-task-oriented-guide-for-humans-not-an-o.patch` | arifi-fork-base | `6a939866d` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `POWERINFER_IOCP` | docs: USAGE - a task-oriented guide for humans, not an option reference |
| 71 | `0071-ggml-cpu-x86-use-the-row-stride-parameter-in-repack-.patch` | arifi-fork-base | `5d4d52e8d` | - | - | ggml-cpu/x86: use the row-stride parameter in repack GEMV stores instead of the row count |
| 72 | `0072-ggml-cpu-fuse-the-TurboQuant-dot-product-transform-t.patch` | ternary-g128 | `4703edc27` | - | - | ggml-cpu: fuse the TurboQuant dot product - transform the activation, not the weights |
| 73 | `0073-docs-FINDINGS-F-09-the-repack-GPU-trade-off-is-a-com.patch` | arifi-fork-base | `6f27d465f` | - | - | docs(FINDINGS): F-09 - the repack/GPU trade-off is a compute-placement choice, not a bug |
| 74 | `0074-docs-neutralise-internal-governance-vocabulary-expla.patch` | arifi-fork-base | `e4c4ee7ae` | - | - | docs: neutralise internal governance vocabulary; explain the work-unit ids instead |
| 75 | `0075-docs-FINDINGS-record-why-the-repack-auto-enable-was-.patch` | arifi-fork-base | `848521ff7` | - | - | docs(FINDINGS): record why the repack auto-enable was NOT shipped |
| 76 | `0076-provenance-catch-the-silent-trailer-block-break-inst.patch` | arifi-fork-base | `8da0c5ab1` | - | - | provenance: catch the silent trailer-block break instead of only reporting it |
| 77 | `0077-gitattributes-pin-.githooks-to-LF-so-the-hook-runs-o.patch` | arifi-fork-base | `e3654bb06` | - | - | gitattributes: pin .githooks to LF so the hook runs on Linux and macOS |
| 78 | `0078-githooks-force-a-normalized-blob-for-the-commit-msg-.patch` | arifi-fork-base | `89ec1a675` | - | - | githooks: force a normalized blob for the commit-msg hook |
| 79 | `0079-docs-FINDINGS-F-09-exit-1-is-the-direction-dual-resi.patch` | arifi-fork-base | `2fdac0466` | - | - | docs(FINDINGS): F-09 exit 1 is the direction - dual residency, not a forced choice |
| 80 | `0080-repack-dual-residency-GGML_ARIFI_VNNI_REPACK-2-keeps.patch` | arifi-fork-base | `055c85479` | - | `GGML_ARIFI_VNNI_REPACK` | repack: dual residency - GGML_ARIFI_VNNI_REPACK=2 keeps prefill on the GPU AND repacks decode |
| 81 | `0081-repack-dual-residency-measured-both-wins-held-fix-th.patch` | arifi-fork-base | `ce6c5b7da` | - | `GGML_ARIFI_VNNI_REPACK` | repack: dual residency measured - both wins held; fix the hot-path cost that ate the first one |
| 82 | `0082-repack-fix-the-shadow-tensor-COUNT-in-the-accounting.patch` | arifi-fork-base | `962bd6492` | - | `GGML_ARIFI_VNNI_REPACK` | repack: fix the shadow tensor COUNT in the accounting line, and strengthen the claim it supports |
| 83 | `0083-repack-CPU-kernels-for-the-128-group-ternary-format.patch` | arifi-fork-base | `794f1303c` | - | `GGML_ARIFI_VNNI_REPACK` | repack: CPU kernels for the 128-group ternary format |
| 84 | `0084-tests-direct-equivalence-test-for-the-ternary-repack.patch` | arifi-fork-base | `dd67b9836` | - | - | tests: direct equivalence test for the ternary repack kernels |
| 85 | `0085-docs-the-g128-repack-SPEED-numbers-measured.patch` | arifi-fork-base | `cd7e63070` | - | `GGML_ARIFI_VNNI_REPACK` | docs: the g128 repack SPEED numbers, measured |
| 86 | `0086-docs-frame-the-g128-repack-numbers-as-a-CPU-tier-res.patch` | arifi-fork-base | `27ee3b405` | - | - | docs: frame the g128 repack numbers as a CPU-tier result |
| 87 | `0087-docs-publish-the-power-plan-finding-as-F-12-and-bind.patch` | arifi-fork-base | `400cf293a` | - | - | docs: publish the power-plan finding as F-12, and bind the bench protocol to it |
| 88 | `0088-F-06-measure-the-GPU-tier-on-one-binary-and-count-th.patch` | arifi-fork-base | `58d06a96e` | - | - | F-06: measure the GPU tier on one binary, and count the placement the prompt claim rested on |
| 89 | `0089-registry-the-dual-residency-prompt-mechanism-is-meas.patch` | arifi-fork-base | `ebbbc00f4` | - | `GGML_ARIFI_VNNI_REPACK` | registry: the dual-residency prompt mechanism is measured now, not ASSUMED |
| 90 | `0090-powerinfer-MAX_N_CACHED-admitted-two-values-that-bre.patch` | powerinfer-streaming | `e83865144` | - | `MAX_N_CACHED` | powerinfer: MAX_N_CACHED admitted two values that break the eviction invariant |
| 91 | `0091-rows-per-workgroup-on-g128-a-null-and-F-10-confirmed.patch` | ternary-g128 | `47fa9c26b` | - | - | rows-per-workgroup on g128: a null, and F-10 confirmed on a second file |
| 92 | `0092-registry-our-DSpark-verdict-was-never-ours-the-draft.patch` | arifi-fork-base | `3bbf41493` | - | - | registry: our DSpark verdict was never ours -- the drafter cannot load on this fork |
| 93 | `0093-speculative-DFlash-DSpark-drafters-have-no-vocab-tak.patch` | arifi-fork-base | `5c7291ec6` | - | - | speculative: DFlash/DSpark drafters have no vocab -- take the mask token from metadata |
| 94 | `0094-expert-cache-stride-the-bundle-by-the-PADDED-matrix-.patch` | arifi-fork-base | `62bd9e626` | - | - | expert cache: stride the bundle by the PADDED matrix size -- unblocks our own brain |
| 95 | `0095-powerinfer-POWERINFER_NO_BUFFERING-1-the-Windows-cou.patch` | powerinfer-streaming | `674af1dce` | - | `POWERINFER_IOCP` | powerinfer: POWERINFER_NO_BUFFERING=1 -- the Windows counterpart of the O_DIRECT Linux already uses |
| 96 | `0096-expert-cache-POWERINFER_EXPERT_HEATMAP-measure-acces.patch` | arifi-fork-base | `34898ba68` | - | - | expert cache: POWERINFER_EXPERT_HEATMAP -- measure access skew before building a pin policy |
| 97 | `0097-ours-1163cb34939fe4a9cb07aec034c5954144497ae9.patch.patch` | arifi-fork-base | `d796acb65` | - | - | ours/1163cb34939fe4a9cb07aec034c5954144497ae9.patch |
| 98 | `0098-1-core-types-0001-WIP-add-TurboQuant-KV-cache-types-.patch` | arifi-fork-base | `50e1325c0` | - | - | 1-core-types/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 99 | `0099-1-core-types-0002-ggml-port-TurboQuant-core-quant-ty.patch` | arifi-fork-base | `6aa91d8ae` | - | - | 1-core-types/0002-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 100 | `0100-2-cpu-reference-0001-ggml-port-TurboQuant-core-quant.patch` | arifi-fork-base | `baeb34385` | - | - | 2-cpu-reference/0001-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 101 | `0101-2-cpu-reference-0007-ggml-cpu-declare-turbo3_cpu_wht.patch` | arifi-fork-base | `9ae546228` | - | - | 2-cpu-reference/0007-ggml-cpu-declare-turbo3_cpu_wht_group_size-extern-no.patch |
| 102 | `0102-2-cpu-reference-0008-merge-fix-post-merge-build-and-.patch` | arifi-fork-base | `1fe1f2db4` | - | - | 2-cpu-reference/0008-merge-fix-post-merge-build-and-crash-issues.patch |
| 103 | `0103-3-vulkan-0001-vulkan-metal-hip-add-TurboQuant-kernel.patch` | arifi-fork-base | `3f6a399e8` | - | - | 3-vulkan/0001-vulkan-metal-hip-add-TurboQuant-kernel-support.patch |
| 104 | `0104-3-vulkan-0002-vulkan-port-fork-241-wave64-ballot-fix.patch` | arifi-fork-base | `373044cc5` | - | - | 3-vulkan/0002-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch |
| 105 | `0105-3-vulkan-0003-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `b09c77e5a` | - | - | 3-vulkan/0003-fix-close-complete-audit-findings-7-port-regressions.patch |
| 106 | `0106-3-vulkan-0004-vulkan-reconstruct-supports_op-after-r.patch` | arifi-fork-base | `738e567fd` | - | - | 3-vulkan/0004-vulkan-reconstruct-supports_op-after-rebase-merge-da.patch |
| 107 | `0107-3-vulkan-0006-vulkan-restore-TQ4_1S-weight-type-wiri.patch` | arifi-fork-base | `3b2aedbfb` | - | - | 3-vulkan/0006-vulkan-restore-TQ4_1S-weight-type-wiring-lost-in-the.patch |
| 108 | `0108-3-vulkan-0007-vulkan-add-the-TQ3_1S-weight-type.patc.patch` | arifi-fork-base | `1105ccf2d` | - | - | 3-vulkan/0007-vulkan-add-the-TQ3_1S-weight-type.patch |
| 109 | `0109-3-vulkan-0008-vulkan-reject-MUL_MAT_ID-for-the-Turbo.patch` | arifi-fork-base | `7e764805a` | - | - | 3-vulkan/0008-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch |
| 110 | `0110-3-vulkan-0013-vulkan-TQ-rotated-matmul-shader-ground.patch` | arifi-fork-base | `af8cea278` | - | - | 3-vulkan/0013-vulkan-TQ-rotated-matmul-shader-groundwork-A-side-lo.patch |
| 111 | `0111-3-vulkan-0015-vulkan-create-the-TQ-activation-rotate.patch` | arifi-fork-base | `06f635ff7` | - | - | 3-vulkan/0015-vulkan-create-the-TQ-activation-rotate-pipeline.patch |
| 112 | `0112-3-vulkan-0017-vulkan-rotate-the-staged-activation-co.patch` | arifi-fork-base | `dccba6879` | - | - | 3-vulkan/0017-vulkan-rotate-the-staged-activation-copy-in-place-dr.patch |
| 113 | `0113-3-vulkan-0018-vulkan-register-the-TQ-rotated-mul_mm_.patch` | arifi-fork-base | `5052ff7c3` | - | - | 3-vulkan/0018-vulkan-register-the-TQ-rotated-mul_mm_id-pipelines.patch |
| 114 | `0114-3-vulkan-0021-vulkan-fix-SET_ROWS-block-decompositio.patch` | arifi-fork-base | `63688cbc4` | - | - | 3-vulkan/0021-vulkan-fix-SET_ROWS-block-decomposition-for-turbo-we.patch |
| 115 | `0115-4-other-backends-0006-laguna-MoE-down-proj-f16-overf.patch` | arifi-fork-base | `a7748ab8f` | - | - | 4-other-backends/0006-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch |
| 116 | `0116-4-other-backends-0007-cuda-first-class-MoE-path-plai.patch` | arifi-fork-base | `26d4f72e8` | - | - | 4-other-backends/0007-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch |
| 117 | `0117-4-other-backends-0008-hip-add-mixed-f16-bf16-q8_0-fa.patch` | arifi-fork-base | `e9b0e81b6` | - | - | 4-other-backends/0008-hip-add-mixed-f16-bf16-q8_0-fattn-vec-instances-to-t.patch |
| 118 | `0118-4-other-backends-0009-cuda-revert-GQA-ratio-dispatch.patch` | arifi-fork-base | `7021b1938` | - | - | 4-other-backends/0009-cuda-revert-GQA-ratio-dispatch-to-comparison-form-fi.patch |
| 119 | `0119-4-other-backends-0010-cuda-restore-Volta-GQA-modulo-.patch` | arifi-fork-base | `f7958a645` | - | - | 4-other-backends/0010-cuda-restore-Volta-GQA-modulo-dispatch-over-revert-i.patch |
| 120 | `0120-4-other-backends-0015-cuda-rework-MoE-expert-cache-e.patch` | arifi-fork-base | `75763cb42` | - | - | 4-other-backends/0015-cuda-rework-MoE-expert-cache-execution.patch |
| 121 | `0121-4-other-backends-0016-cuda-fix-MoE-cache-capacity-ac.patch` | arifi-fork-base | `03651521a` | - | - | 4-other-backends/0016-cuda-fix-MoE-cache-capacity-accounting.patch |
| 122 | `0122-4-other-backends-0017-cuda-fix-MoE-cache-scratch-bud.patch` | arifi-fork-base | `e854b1b6b` | - | - | 4-other-backends/0017-cuda-fix-MoE-cache-scratch-budget-replacement.patch |
| 123 | `0123-4-other-backends-0018-cuda-bound-MoE-MMV-tail-row-re.patch` | arifi-fork-base | `4264f6065` | - | - | 4-other-backends/0018-cuda-bound-MoE-MMV-tail-row-reads.patch |
| 124 | `0124-4-other-backends-0019-cuda-tune-MoE-cache-CPU-overla.patch` | arifi-fork-base | `56c308ff0` | - | - | 4-other-backends/0019-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 125 | `0125-4-other-backends-0020-cuda-adapt-MoE-cache-admission.patch` | arifi-fork-base | `e3ed98713` | - | - | 4-other-backends/0020-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 126 | `0126-4-other-backends-0021-cuda-align-MoE-cache-pool-allo.patch` | arifi-fork-base | `0a45fbda4` | - | - | 4-other-backends/0021-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 127 | `0127-4-other-backends-0022-cuda-prefer-generic-MMV-for-co.patch` | arifi-fork-base | `68b183397` | - | - | 4-other-backends/0022-cuda-prefer-generic-MMV-for-compatible-MoE-cache-nod.patch |
| 128 | `0128-4-other-backends-0023-cuda-parallelize-MoE-cache-fil.patch` | arifi-fork-base | `035653447` | - | - | 4-other-backends/0023-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch |
| 129 | `0129-4-other-backends-0024-cuda-fuse-cached-MoE-SwiGLU-ro.patch` | arifi-fork-base | `5bbbe10c9` | - | - | 4-other-backends/0024-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 130 | `0130-4-other-backends-0025-cuda-aggregate-small-MoE-tenso.patch` | arifi-fork-base | `0b44f71ef` | - | - | 4-other-backends/0025-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 131 | `0131-4-other-backends-0026-cuda-bound-automatic-MoE-cache.patch` | arifi-fork-base | `b47516488` | - | - | 4-other-backends/0026-cuda-bound-automatic-MoE-cache-admission.patch |
| 132 | `0132-4-other-backends-0027-cuda-reject-undersized-automat.patch` | arifi-fork-base | `6177c12da` | - | - | 4-other-backends/0027-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 133 | `0133-4-other-backends-0028-cuda-enable-multi-token-MoE-ca.patch` | arifi-fork-base | `5328a90ed` | - | - | 4-other-backends/0028-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 134 | `0134-4-other-backends-0029-common-surface-MoE-cache-activ.patch` | arifi-fork-base | `29841da68` | - | - | 4-other-backends/0029-common-surface-MoE-cache-activation.patch |
| 135 | `0135-4-other-backends-0030-cuda-accelerate-complete-MoE-c.patch` | arifi-fork-base | `f7bbed3ce` | - | - | 4-other-backends/0030-cuda-accelerate-complete-MoE-cache-pools.patch |
| 136 | `0136-4-other-backends-0031-cuda-report-oversized-MoE-cach.patch` | arifi-fork-base | `b35898abb` | - | - | 4-other-backends/0031-cuda-report-oversized-MoE-cache-nodes.patch |
| 137 | `0137-4-other-backends-0032-cuda-clarify-MoE-cache-session.patch` | arifi-fork-base | `05f8c755e` | - | - | 4-other-backends/0032-cuda-clarify-MoE-cache-session-diagnostics.patch |
| 138 | `0138-4-other-backends-0033-cuda-report-MoE-cache-pair-res.patch` | arifi-fork-base | `0dc2d727a` | - | - | 4-other-backends/0033-cuda-report-MoE-cache-pair-residency.patch |
| 139 | `0139-4-other-backends-0034-cuda-pack-MoE-cache-dispatch-i.patch` | arifi-fork-base | `8ccf3ac8d` | - | - | 4-other-backends/0034-cuda-pack-MoE-cache-dispatch-inputs.patch |
| 140 | `0140-4-other-backends-0037-metal-port-TQ_NO_ROTATE-escape.patch` | arifi-fork-base | `050bfd213` | - | - | 4-other-backends/0037-metal-port-TQ_NO_ROTATE-escape-hatch-from-stash-opt-.patch |
| 141 | `0141-4-other-backends-0039-moe-cache-enable-HIP-backend-b.patch` | arifi-fork-base | `8719e54d5` | - | - | 4-other-backends/0039-moe-cache-enable-HIP-backend-by-removing-no-op-stubs.patch |
| 142 | `0142-4-other-backends-0040-moe-cache-count-dispatch-conte.patch` | arifi-fork-base | `1173da729` | - | - | 4-other-backends/0040-moe-cache-count-dispatch-contention-bypasses-P1.patch |
| 143 | `0143-4-other-backends-0041-moe-cache-provider-registry-wi.patch` | arifi-fork-base | `045c6b7bc` | - | - | 4-other-backends/0041-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 144 | `0144-4-other-backends-0042-moe-cache-audit-fixes-H1-F1-F2.patch` | arifi-fork-base | `7e52d3825` | - | - | 4-other-backends/0042-moe-cache-audit-fixes-H1-F1-F2-A1.patch |
| 145 | `0145-4-other-backends-0043-moe-cache-fix-registry-build-I.patch` | arifi-fork-base | `936c6dc49` | - | - | 4-other-backends/0043-moe-cache-fix-registry-build-I1-follow-up.patch |
| 146 | `0146-4-other-backends-0044-moe-cache-allow-automatic-mode.patch` | arifi-fork-base | `501dbc4cc` | - | - | 4-other-backends/0044-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 147 | `0147-4-other-backends-0045-cuda-fix-HIP-MoE-cache-compati.patch` | arifi-fork-base | `33fb6b8e1` | - | - | 4-other-backends/0045-cuda-fix-HIP-MoE-cache-compatibility-and-q8_1-sums.patch |
| 148 | `0148-4-other-backends-0046-moe-cache-add-logging-at-silen.patch` | arifi-fork-base | `5b5c5a6c8` | - | - | 4-other-backends/0046-moe-cache-add-logging-at-silent-failure-points-acros.patch |
| 149 | `0149-4-other-backends-0048-cuda-fit-fix-two-Werror-build-.patch` | arifi-fork-base | `1f5683b55` | - | - | 4-other-backends/0048-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch |
| 150 | `0150-4-other-backends-0049-sycl-drop-the-duplicated-nthre.patch` | arifi-fork-base | `f0a191b2c` | - | - | 4-other-backends/0049-sycl-drop-the-duplicated-nthreads-declarations-in-fa.patch |
| 151 | `0151-4-other-backends-0050-fattn-vec-split-the-turbo-K-do.patch` | arifi-fork-base | `3010d1d04` | - | - | 4-other-backends/0050-fattn-vec-split-the-turbo-K-dot-at-D-128-to-stop-the.patch |
| 152 | `0152-5-models-server-0001-WIP-add-TurboQuant-KV-cache-typ.patch` | arifi-fork-base | `ed4109a10` | - | - | 5-models-server/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 153 | `0153-5-models-server-0003-cli-break-interactive-loop-on-s.patch` | arifi-fork-base | `0fe1fd6c3` | - | - | 5-models-server/0003-cli-break-interactive-loop-on-stdin-EOF-fixes-hot-sp.patch |
| 154 | `0154-5-models-server-0004-tools-expose-TQ3_1S-TQ4_1S-in-q.patch` | arifi-fork-base | `70bca75a1` | - | - | 5-models-server/0004-tools-expose-TQ3_1S-TQ4_1S-in-quantize-table-and-lla.patch |
| 155 | `0155-5-models-server-0007-llama-support-DeepSeek-V4-tenso.patch` | arifi-fork-base | `e136f6bb2` | - | - | 5-models-server/0007-llama-support-DeepSeek-V4-tensor-split.patch |
| 156 | `0156-5-models-server-0008-llama-mirror-DS4-q_a-kv-down-pr.patch` | arifi-fork-base | `7e85e5d47` | - | - | 5-models-server/0008-llama-mirror-DS4-q_a-kv-down-projections-in-tensor-s.patch |
| 157 | `0157-5-models-server-0009-glm-dsa-guard-lightning-indexer.patch` | arifi-fork-base | `719158f46` | - | - | 5-models-server/0009-glm-dsa-guard-lightning-indexer-Hadamard-rotation-wh.patch |
| 158 | `0158-5-models-server-0010-laguna-add-arch-tables-llm_arch.patch` | arifi-fork-base | `d595325d3` | - | - | 5-models-server/0010-laguna-add-arch-tables-llm_arch-KV-keys-tensors-and-.patch |
| 159 | `0159-5-models-server-0011-laguna-add-model-class-hparams-.patch` | arifi-fork-base | `f4b36cec1` | - | - | 5-models-server/0011-laguna-add-model-class-hparams-wiring-vocab-pre-toke.patch |
| 160 | `0160-5-models-server-0013-laguna-MoE-down-proj-f16-overfl.patch` | arifi-fork-base | `d51893923` | - | - | 5-models-server/0013-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch |
| 161 | `0161-5-models-server-0014-cuda-first-class-MoE-path-plain.patch` | arifi-fork-base | `185a1120a` | - | - | 5-models-server/0014-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch |
| 162 | `0162-5-models-server-0015-cuda-sum-MoE-expert-outputs-on-.patch` | arifi-fork-base | `bfb1bee36` | - | - | 5-models-server/0015-cuda-sum-MoE-expert-outputs-on-decode-n_tokens-1.patch |
| 163 | `0163-5-models-server-0016-laguna-deduplicate-definitions-.patch` | arifi-fork-base | `5021f13f7` | - | - | 5-models-server/0016-laguna-deduplicate-definitions-vs-the-early-snapshot.patch |
| 164 | `0164-5-models-server-0017-dflash-fix-DSpark-tensor-meta-a.patch` | arifi-fork-base | `08e73374e` | - | - | 5-models-server/0017-dflash-fix-DSpark-tensor-meta-accessor-after-laguna-.patch |
| 165 | `0165-5-models-server-0021-common-support-the-DSpark-sidec.patch` | arifi-fork-base | `536d09d3e` | - | - | 5-models-server/0021-common-support-the-DSpark-sidecar-resolution-26458.patch |
| 166 | `0166-5-models-server-0037-merge-fix-post-merge-build-and-.patch` | arifi-fork-base | `a5786cfc7` | - | - | 5-models-server/0037-merge-fix-post-merge-build-and-crash-issues.patch |
| 167 | `0167-5-models-server-0041-moe-cache-route-fit-probing-and.patch` | arifi-fork-base | `1efb9d7ab` | - | - | 5-models-server/0041-moe-cache-route-fit-probing-and-test-session-calls-t.patch |
| 168 | `0168-5-models-server-0048-llama-bench-document-pw-prefetc.patch` | arifi-fork-base | `ca9c1267d` | - | - | 5-models-server/0048-llama-bench-document-pw-prefetch-weights-in-help-and.patch |
| 169 | `0169-5-models-server-0050-llama-bench-remove-stale-prefet.patch` | arifi-fork-base | `f2473ad09` | - | - | 5-models-server/0050-llama-bench-remove-stale-prefetch-weights-documentat.patch |
| 170 | `0170-5-models-server-0051-moe-cache-fix-auto-asymmetric-M.patch` | arifi-fork-base | `49c7450c6` | - | - | 5-models-server/0051-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch |
| 171 | `0171-6-tests-build-0001-tests-re-add-test-turbo-quant.c-r.patch` | arifi-fork-base | `1bb40c5d6` | - | - | 6-tests-build/0001-tests-re-add-test-turbo-quant.c-round-trip-test.patch |
| 172 | `0172-6-tests-build-0002-tests-port-fork-turbo-backend-and.patch` | arifi-fork-base | `a8d9de2ab` | - | - | 6-tests-build/0002-tests-port-fork-turbo-backend-and-quantize-fns-test-.patch |
| 173 | `0173-6-tests-build-0003-fix-close-complete-audit-findings.patch` | arifi-fork-base | `93134ab31` | - | - | 6-tests-build/0003-fix-close-complete-audit-findings-7-port-regressions.patch |
| 174 | `0174-6-tests-build-0004-laguna-tool-call-whitespace-toler.patch` | arifi-fork-base | `2018bbbb0` | - | - | 6-tests-build/0004-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch |
| 175 | `0175-6-tests-build-0006-ggml-webgpu-add-support-for-f16-r.patch` | arifi-fork-base | `77ac7a734` | - | - | 6-tests-build/0006-ggml-webgpu-add-support-for-f16-repeat-26307.patch |
| 176 | `0176-6-tests-build-0010-tests-restore-DSV4_HC_COMB-eps-sw.patch` | arifi-fork-base | `43255f274` | - | - | 6-tests-build/0010-tests-restore-DSV4_HC_COMB-eps-sweep-lost-in-the-244.patch |
| 177 | `0177-6-tests-build-0011-tests-cast-float-printf-args-in-t.patch` | arifi-fork-base | `ec9433760` | - | - | 6-tests-build/0011-tests-cast-float-printf-args-in-test-turbo-quant-arm.patch |
| 178 | `0178-6-tests-build-0012-vulkan-add-the-TQ3_1S-weight-type.patch` | arifi-fork-base | `48cb1d7d3` | - | - | 6-tests-build/0012-vulkan-add-the-TQ3_1S-weight-type.patch |
| 179 | `0179-6-tests-build-0013-vulkan-reject-MUL_MAT_ID-for-the-.patch` | arifi-fork-base | `a6d2dece6` | - | - | 6-tests-build/0013-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch |
| 180 | `0180-6-tests-build-0014-ggml-cpu-remove-the-per-call-mall.patch` | arifi-fork-base | `457a87e07` | - | - | 6-tests-build/0014-ggml-cpu-remove-the-per-call-mallocs-from-the-TurboQ.patch |
| 181 | `0181-6-tests-build-0015-vulkan-add-mul_mat_vec_id-for-the.patch` | arifi-fork-base | `f1650e0be` | - | - | 6-tests-build/0015-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch |
| 182 | `0182-6-tests-build-0016-feat-add-Vulkan-TQ4_1S-weight-pip.patch` | arifi-fork-base | `ea2ad4b72` | - | - | 6-tests-build/0016-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch |
| 183 | `0183-6-tests-build-0017-tests-port-11-DSv4-shaped-TQ-MUL_.patch` | arifi-fork-base | `175c26e7e` | - | - | 6-tests-build/0017-tests-port-11-DSv4-shaped-TQ-MUL_MAT-cases-from-sync.patch |
| 184 | `0184-6-tests-build-0019-cuda-fix-MoE-cache-capacity-accou.patch` | arifi-fork-base | `4918bea3a` | - | - | 6-tests-build/0019-cuda-fix-MoE-cache-capacity-accounting.patch |
| 185 | `0185-6-tests-build-0020-common-preserve-implicit-MoE-cach.patch` | arifi-fork-base | `f2d9aa5be` | - | - | 6-tests-build/0020-common-preserve-implicit-MoE-cache-provider-settings.patch |
| 186 | `0186-6-tests-build-0021-cuda-tune-MoE-cache-CPU-overlap-a.patch` | arifi-fork-base | `74dc5121f` | - | - | 6-tests-build/0021-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 187 | `0187-6-tests-build-0022-cuda-adapt-MoE-cache-admission-to.patch` | arifi-fork-base | `af75eec55` | - | - | 6-tests-build/0022-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 188 | `0188-6-tests-build-0023-cuda-align-MoE-cache-pool-allocat.patch` | arifi-fork-base | `7c0c9c7dd` | - | - | 6-tests-build/0023-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 189 | `0189-6-tests-build-0024-cuda-parallelize-MoE-cache-fills-.patch` | arifi-fork-base | `f348f49a7` | - | - | 6-tests-build/0024-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch |
| 190 | `0190-6-tests-build-0025-cuda-fuse-cached-MoE-SwiGLU-rows..patch` | arifi-fork-base | `88a9c58fc` | - | - | 6-tests-build/0025-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 191 | `0191-6-tests-build-0026-cuda-aggregate-small-MoE-tensors-.patch` | arifi-fork-base | `297073e8c` | - | - | 6-tests-build/0026-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 192 | `0192-6-tests-build-0027-cuda-bound-automatic-MoE-cache-ad.patch` | arifi-fork-base | `1e29d303b` | - | - | 6-tests-build/0027-cuda-bound-automatic-MoE-cache-admission.patch |
| 193 | `0193-6-tests-build-0028-cuda-reject-undersized-automatic-.patch` | arifi-fork-base | `729b634fd` | - | - | 6-tests-build/0028-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 194 | `0194-6-tests-build-0029-cuda-enable-multi-token-MoE-cache.patch` | arifi-fork-base | `bec4a67dd` | - | - | 6-tests-build/0029-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 195 | `0195-6-tests-build-0030-common-surface-MoE-cache-activati.patch` | arifi-fork-base | `7dc40864f` | - | - | 6-tests-build/0030-common-surface-MoE-cache-activation.patch |
| 196 | `0196-6-tests-build-0031-cuda-accelerate-complete-MoE-cach.patch` | arifi-fork-base | `e9b704e24` | - | - | 6-tests-build/0031-cuda-accelerate-complete-MoE-cache-pools.patch |
| 197 | `0197-6-tests-build-0032-cuda-report-oversized-MoE-cache-n.patch` | arifi-fork-base | `fab456a56` | - | - | 6-tests-build/0032-cuda-report-oversized-MoE-cache-nodes.patch |
| 198 | `0198-6-tests-build-0033-tests-initialise-non-contiguous-t.patch` | arifi-fork-base | `6cb1eb5e4` | - | - | 6-tests-build/0033-tests-initialise-non-contiguous-tensors-row-by-row.patch |
| 199 | `0199-6-tests-build-0034-tests-cover-turbo-KV-flash-attent.patch` | arifi-fork-base | `d2625cce1` | - | - | 6-tests-build/0034-tests-cover-turbo-KV-flash-attention-at-head-dim-256.patch |
| 200 | `0200-6-tests-build-0037-moe-cache-provider-registry-with-.patch` | arifi-fork-base | `c26532749` | - | - | 6-tests-build/0037-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 201 | `0201-6-tests-build-0038-moe-cache-route-fit-probing-and-t.patch` | arifi-fork-base | `c59d820b3` | - | - | 6-tests-build/0038-moe-cache-route-fit-probing-and-test-session-calls-t.patch |
| 202 | `0202-6-tests-build-0039-moe-cache-route-context-eligibili.patch` | arifi-fork-base | `67cdd73ef` | - | - | 6-tests-build/0039-moe-cache-route-context-eligibility-probe-and-remain.patch |
| 203 | `0203-6-tests-build-0040-moe-cache-allow-automatic-mode-on.patch` | arifi-fork-base | `d25966544` | - | - | 6-tests-build/0040-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 204 | `0204-6-tests-build-0041-moe-cache-fix-auto-asymmetric-MLA.patch` | arifi-fork-base | `990baa929` | - | - | 6-tests-build/0041-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch |
| 205 | `0205-6-tests-build-0042-test-moe-cache-accept-IGPU-device.patch` | arifi-fork-base | `b0eb2466f` | - | - | 6-tests-build/0042-test-moe-cache-accept-IGPU-devices-not-just-GPU.patch |
| 206 | `0206-6-tests-build-0043-test-moe-cache-make-cache-shared-.patch` | arifi-fork-base | `a66e2dc8d` | - | - | 6-tests-build/0043-test-moe-cache-make-cache-shared-budget-UMA-aware-wi.patch |
| 207 | `0207-6-tests-build-0044-test-moe-cache-save-and-restore-t.patch` | arifi-fork-base | `116ec29c8` | - | - | 6-tests-build/0044-test-moe-cache-save-and-restore-the-CUDA-device-arou.patch |
| 208 | `0208-6-tests-build-0045-tests-tell-ctest-that-77-means-sk.patch` | arifi-fork-base | `296f04704` | - | - | 6-tests-build/0045-tests-tell-ctest-that-77-means-skip-for-test-moe-cac.patch |
| 209 | `0209-resolved-0009-vulkan-add-mul_mat_vec_id-for-the-Turb.patch` | arifi-fork-base | `ce735ddae` | - | - | resolved/0009-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch |
| 210 | `0210-resolved-0011-fix-correct-Vulkan-turbo3-pipeline-wir.patch` | arifi-fork-base | `e8c09d32f` | - | - | resolved/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch |
| 211 | `0211-resolved-0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-ga.patch` | arifi-fork-base | `8560eeda2` | - | - | resolved/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch |
| 212 | `0212-resolved-0005-merge-close-TurboQuant-parity-gaps.pat.patch` | arifi-fork-base | `b0bbeda79` | - | - | resolved/0005-merge-close-TurboQuant-parity-gaps.patch |
| 213 | `0213-resolved-0036-common-surface-MoE-cache-activation.pa.patch` | arifi-fork-base | `53efca8a8` | - | - | resolved/0036-common-surface-MoE-cache-activation.patch |
| 214 | `0214-resolved-0019-vulkan-dispatch-the-TQ-rotated-mul_mm_.patch` | arifi-fork-base | `768d34d3c` | - | - | resolved/0019-vulkan-dispatch-the-TQ-rotated-mul_mm_id-path.patch |
| 215 | `0215-resolved-0010-feat-add-Vulkan-turbo3-KV-cache-pipeli.patch` | arifi-fork-base | `5ee7f3e46` | - | - | resolved/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch |
| 216 | `0216-resolved-0004-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `c0fe939fe` | - | - | resolved/0004-fix-close-complete-audit-findings-7-port-regressions.patch |
| 217 | `0217-resolved-0005-metal-restore-lost-kernel-templates-ad.patch` | arifi-fork-base | `db5c686ba` | - | - | resolved/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch |
| 218 | `0218-resolved-0012-metal-implement-DeepSeek-V4-hyper-conn.patch` | arifi-fork-base | `9b4c187d8` | - | - | resolved/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch |
| 219 | `0219-resolved-0035-cuda-gate-fused-TQ-mul_mat-paths-on-co.patch` | arifi-fork-base | `57418f7e6` | - | - | resolved/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch |
| 220 | `0220-resolved-0036-cuda-disable-fused-TQ3_1S-mul_mat-kern.patch` | arifi-fork-base | `c2149033d` | - | - | resolved/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch |
| 221 | `0221-resolved-0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HI.patch` | arifi-fork-base | `8044cce88` | - | - | resolved/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch |
| 222 | `0222-resolved-0047-sycl-hip-fix-two-build-breaks-in-fork-.patch` | arifi-fork-base | `b38144cb2` | - | - | resolved/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch |
| 223 | `0223-resolved-0006-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `4d31af45d` | - | - | resolved/0006-fix-close-complete-audit-findings-7-port-regressions.patch |
| 224 | `0224-resolved-0022-DeepseekV4-MTP-DSpark-25784.patch.patch` | arifi-fork-base | `0e724a154` | - | - | resolved/0022-DeepseekV4-MTP-DSpark-25784.patch |
| 225 | `0225-resolved-0023-dflash-fix-merge-artifacts-from-upstre.patch` | arifi-fork-base | `ef5d840d6` | - | - | resolved/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch |
| 226 | `0226-resolved-0029-server-harden-shared-draft-device-plac.patch` | arifi-fork-base | `d57b034fb` | - | - | resolved/0029-server-harden-shared-draft-device-placement.patch |
| 227 | `0227-resolved-0025-fix-server-batch-restored-checkpoint-p.patch` | arifi-fork-base | `85026041f` | - | - | resolved/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch |
| 228 | `0228-resolved-0026-cuda-rework-MoE-expert-cache-execution.patch` | arifi-fork-base | `843ca1953` | - | - | resolved/0026-cuda-rework-MoE-expert-cache-execution.patch |
| 229 | `0229-resolved-0033-server-account-for-MTP-placement-in-fi.patch` | arifi-fork-base | `b51ba6612` | - | - | resolved/0033-server-account-for-MTP-placement-in-fit-reservations.patch |
| 230 | `0230-resolved-0035-cuda-reject-undersized-automatic-MoE-c.patch` | arifi-fork-base | `f2054577a` | - | - | resolved/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 231 | `0231-resolved-0038-moe-cache-add-moe-cache-soft-mode-with.patch` | arifi-fork-base | `6535f5ad1` | - | - | resolved/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch |
| 232 | `0232-resolved-0039-model-Muse-Glimmer-Support-26841.patch.patch` | arifi-fork-base | `822ce0592` | - | - | resolved/0039-model-Muse-Glimmer-Support-26841.patch |
| 233 | `0233-resolved-0043-moe-cache-audit-fixes-H1-F1-F2-A1.patc.patch` | arifi-fork-base | `d3f2a53d0` | - | - | resolved/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch |
| 234 | `0234-resolved-0044-moe-cache-restore-moe_cache-params-on-.patch` | arifi-fork-base | `d8c788ed6` | - | - | resolved/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch |
| 235 | `0235-resolved-0046-llama-remove-dead-MSA-indexer-scaffold.patch` | arifi-fork-base | `5cc85c1d7` | - | - | resolved/0046-llama-remove-dead-MSA-indexer-scaffolding.patch |
| 236 | `0236-resolved-0047-moe-cache-add-logging-at-silent-failur.patch` | arifi-fork-base | `69b1eeb1a` | - | - | resolved/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch |
| 237 | `0237-resolved-0052-cuda-fit-fix-two-Werror-build-failures.patch` | arifi-fork-base | `d578c7318` | - | - | resolved/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch |
| 238 | `0238-resolved-0005-test-fix-some-CI-errors-26415.patch.patch` | arifi-fork-base | `93aac2b3e` | - | - | resolved/0005-test-fix-some-CI-errors-26415.patch |
| 239 | `0239-resolved-remaining-conflict-markers.patch` | arifi-fork-base | `9dac80c5d` | - | - | resolved/remaining-conflict-markers |
| 240 | `0240-layer7-0001-WIP-add-TurboQuant-KV-cache-types-turbo3.patch` | arifi-fork-base | `57b04f4b9` | - | - | layer7/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 241 | `0241-layer7-0002-Update-GGMLQuantizationType-and-LlamaFil.patch` | arifi-fork-base | `185d2a929` | - | - | layer7/0002-Update-GGMLQuantizationType-and-LlamaFileType-enums-.patch |
| 242 | `0242-layer7-0003-ggml-port-TurboQuant-core-quant-types-an.patch` | arifi-fork-base | `8d71a9919` | - | - | layer7/0003-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 243 | `0243-layer7-0005-docs-add-rebase-plan-and-TurboQuant-reci.patch` | arifi-fork-base | `13de91fc0` | - | - | layer7/0005-docs-add-rebase-plan-and-TurboQuant-recipes.patch |
| 244 | `0244-layer7-0007-gguf-py-dedupe-stacked-merge-artifacts-i.patch` | arifi-fork-base | `a6f37c812` | - | - | layer7/0007-gguf-py-dedupe-stacked-merge-artifacts-in-constants..patch |
| 245 | `0245-layer7-0008-docs-add-KV-cache-quantization-guide-upd.patch` | arifi-fork-base | `0b6d88259` | - | - | layer7/0008-docs-add-KV-cache-quantization-guide-update-rebase-p.patch |
| 246 | `0246-layer7-0009-vulkan-port-fork-241-wave64-ballot-fix-c.patch` | arifi-fork-base | `5ac489232` | - | - | layer7/0009-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch |
| 247 | `0247-layer7-0010-merge-close-TurboQuant-parity-gaps.patch.patch` | arifi-fork-base | `5907690b6` | - | - | layer7/0010-merge-close-TurboQuant-parity-gaps.patch |
| 248 | `0248-layer7-0011-metal-restore-lost-kernel-templates-add-.patch` | arifi-fork-base | `6de0593e6` | - | - | layer7/0011-metal-restore-lost-kernel-templates-add-offline-shad.patch |
| 249 | `0249-layer7-0014-laguna-tool-call-whitespace-tolerance-do.patch` | arifi-fork-base | `24b3987ad` | - | - | layer7/0014-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch |
| 250 | `0250-layer7-0019-fix-apply-rebase-audit-fixes-stack-overf.patch` | arifi-fork-base | `b08938206` | - | - | layer7/0019-fix-apply-rebase-audit-fixes-stack-overflow-quality-.patch |
| 251 | `0251-layer7-0020-docs-add-TurboQuant-project-overview-to-.patch` | arifi-fork-base | `e89622bea` | - | - | layer7/0020-docs-add-TurboQuant-project-overview-to-AGENTS.md-an.patch |
| 252 | `0252-layer7-0021-cuda-rework-MoE-expert-cache-execution.p.patch` | arifi-fork-base | `e6c5c71f6` | - | - | layer7/0021-cuda-rework-MoE-expert-cache-execution.patch |
| 253 | `0253-layer7-0022-docs-update-MoE-cache-validation.patch.patch` | arifi-fork-base | `24f05df43` | - | - | layer7/0022-docs-update-MoE-cache-validation.patch |
| 254 | `0254-layer7-0023-common-preserve-implicit-MoE-cache-provi.patch` | arifi-fork-base | `93316df89` | - | - | layer7/0023-common-preserve-implicit-MoE-cache-provider-settings.patch |
| 255 | `0255-layer7-0024-server-harden-shared-draft-device-placem.patch` | arifi-fork-base | `c8b4a8d1b` | - | - | layer7/0024-server-harden-shared-draft-device-placement.patch |
| 256 | `0256-layer7-0025-cuda-tune-MoE-cache-CPU-overlap-automati.patch` | arifi-fork-base | `b04954e60` | - | - | layer7/0025-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 257 | `0257-layer7-0026-cuda-adapt-MoE-cache-admission-to-device.patch` | arifi-fork-base | `086eadd2e` | - | - | layer7/0026-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 258 | `0258-layer7-0027-cuda-align-MoE-cache-pool-allocation-wit.patch` | arifi-fork-base | `b3498825b` | - | - | layer7/0027-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 259 | `0259-layer7-0028-docs-update-MoE-cache-validation.patch.patch` | arifi-fork-base | `e806fd06e` | - | - | layer7/0028-docs-update-MoE-cache-validation.patch |
| 260 | `0260-layer7-0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch.patch` | arifi-fork-base | `e6aba2b36` | - | - | layer7/0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 261 | `0261-layer7-0030-cuda-aggregate-small-MoE-tensors-into-ca.patch` | arifi-fork-base | `1576ca432` | - | - | layer7/0030-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 262 | `0262-layer7-0031-cuda-bound-automatic-MoE-cache-admission.patch` | arifi-fork-base | `ea9107aec` | - | - | layer7/0031-cuda-bound-automatic-MoE-cache-admission.patch |
| 263 | `0263-layer7-0032-cuda-reject-undersized-automatic-MoE-cac.patch` | arifi-fork-base | `1d090cfd8` | - | - | layer7/0032-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 264 | `0264-layer7-0033-cuda-enable-multi-token-MoE-cache-in-for.patch` | arifi-fork-base | `fdd44dacf` | - | - | layer7/0033-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 265 | `0265-layer7-0034-common-surface-MoE-cache-activation.patc.patch` | arifi-fork-base | `5400bed8f` | - | - | layer7/0034-common-surface-MoE-cache-activation.patch |
| 266 | `0266-layer7-0035-docs-fix-MoE-cache-benchmark-verbosity.p.patch` | arifi-fork-base | `d6dc57c77` | - | - | layer7/0035-docs-fix-MoE-cache-benchmark-verbosity.patch |
| 267 | `0267-layer7-0036-cuda-accelerate-complete-MoE-cache-pools.patch` | arifi-fork-base | `3f23a766d` | - | - | layer7/0036-cuda-accelerate-complete-MoE-cache-pools.patch |
| 268 | `0268-layer7-0037-cuda-report-oversized-MoE-cache-nodes.pa.patch` | arifi-fork-base | `0a77295f6` | - | - | layer7/0037-cuda-report-oversized-MoE-cache-nodes.patch |
| 269 | `0269-layer7-0038-cuda-clarify-MoE-cache-session-diagnosti.patch` | arifi-fork-base | `e0c3c542c` | - | - | layer7/0038-cuda-clarify-MoE-cache-session-diagnostics.patch |
| 270 | `0270-layer7-0039-docs-record-MoE-cache-workload-convergen.patch` | arifi-fork-base | `3eb5eaffd` | - | - | layer7/0039-docs-record-MoE-cache-workload-convergence.patch |
| 271 | `0271-layer7-0040-cuda-report-MoE-cache-pair-residency.pat.patch` | arifi-fork-base | `a1ad995a7` | - | - | layer7/0040-cuda-report-MoE-cache-pair-residency.patch |
| 272 | `0272-layer7-0041-docs-document-what-the-test-suites-do-an.patch` | arifi-fork-base | `d4e8d6264` | - | - | layer7/0041-docs-document-what-the-test-suites-do-and-do-not-cov.patch |
| 273 | `0273-layer7-0044-moe-cache-provider-registry-with-per-sch.patch` | arifi-fork-base | `48308188f` | - | - | layer7/0044-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 274 | `0274-layer7-0046-moe-cache-allow-automatic-mode-on-a-sing.patch` | arifi-fork-base | `9f7eadd29` | - | - | layer7/0046-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 275 | `0275-layer7-0048-docs-document-INFO-level-MoE-cache-disab.patch` | arifi-fork-base | `d6e294ca4` | - | - | layer7/0048-docs-document-INFO-level-MoE-cache-disable-reasons-i.patch |
| 276 | `0276-layer7-0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CAC.patch` | arifi-fork-base | `d18c1056f` | - | - | layer7/0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CACHE.md-and-re.patch |
| 277 | `0277-layer7-0050-docs-correct-MOE-CACHE.md-against-the-im.patch` | arifi-fork-base | `22e48ac3e` | - | - | layer7/0050-docs-correct-MOE-CACHE.md-against-the-implementation.patch |
| 278 | `0278-layer7-0051-ggml-split-the-turbo3-group-size-declara.patch` | arifi-fork-base | `c92b5fa46` | - | - | layer7/0051-ggml-split-the-turbo3-group-size-declaration-from-it.patch |
| 279 | `0279-resolve-8-committed-conflict-markers-on-merit.patch` | arifi-fork-base | `0de0de978` | - | - | resolve 8 committed conflict markers on merit |
| 280 | `0280-fix-the-merge-so-it-builds-5-files-6-defects.patch` | arifi-fork-base | `f461ceece` | - | - | fix the merge so it builds: 5 files, 6 defects |
| 281 | `0281-merge-repairs-seven-defect-classes-and-one-the-compi.patch` | arifi-fork-base | `9955b565f` | - | - | merge repairs: seven defect classes, and one the compiler could never have reported |
| 282 | `0282-ROCmFPX-on-Vulkan-the-shader-half-ported.-NOT-YET-WO.patch` | arifi-fork-base | `297dba4d9` | - | - | ROCmFPX on Vulkan: the shader half, ported. NOT YET WORKING - it segfaults in execution. |
| 283 | `0283-currency-register-ALL-SEVEN-forks-so-nothing-can-dri.patch` | arifi-fork-base | `c47812781` | - | - | currency: register ALL SEVEN forks, so nothing can drift unseen again |
| 284 | `0284-series-base-and-the-branch-it-describes-are-ONE-pair.patch` | arifi-fork-base | `06eb6e0c1` | - | - | series: base and the branch it describes are ONE pair, and the linearity rule is now a gate |
| 285 | `0285-hygiene-the-code-review-skill-taught-a-type-ID-map-t.patch` | arifi-fork-base | `4906d1dc1` | - | - | hygiene: the code-review skill taught a type-ID map the arbitration deleted, and the UI was fetched from a moving tag |
| 286 | `0286-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | ternary-g128 | `60e27b743` | - | - | vulkan: ask the pipelines whether they exist, not the type id |
| 287 | `0287-contract-tq3-is-the-first-family-we-must-renumber-so.patch` | arifi-fork-base | `54a8ffeb7` | - | - | contract: tq3 is the first family we must renumber, so write down why before anyone ports it |
| 288 | `0288-sources-honest-pins-turboquant-re-pinned-off-a-dead-.patch` | arifi-fork-base | `d327675e0` | - | - | sources: honest pins - turboquant re-pinned off a dead lineage, tq3 main-understates note REFUTED, five type-id contract collisions recorded (lane-143) |
| 289 | `0289-sources-lane-143-self-refutation-the-turbo3-ODR-defe.patch` | arifi-fork-base | `9d8ad1b84` | - | - | sources: lane-143 self-refutation - the turbo3 ODR 'defect' does not exist in our tree (GGML_API already carries extern on all four branches); prisml pin REVERTED to keep the BEHIND signal live; FLAG E severity re-derived from loader code |
| 290 | `0290-sources-r2-checker-BLOCK-F1-F6-ciru-160-was-145-comm.patch` | arifi-fork-base | `01d7826d7` | - | - | sources r2 (checker BLOCK F1-F6): ciru 160 was 145 commits of rocmfpx history - corrected to a 7-15 bracket, no-ancestor premise refuted, GOAL-3 ciru review performed, rocmfpx.ref revert, prisml contradiction removed, FLAG E sensitivity added |
| 291 | `0291-series-point-base.ref-at-the-LINEAR-branch-recut-lin.patch` | arifi-fork-base | `638f76a6b` | - | - | series: point base.ref at the LINEAR branch - recut-linear/2026-08-17 is the byte-identical flatten of the blessed union branch, so the series is replayable at last (lane-147) |
| 292 | `0292-series-disclose-in-the-record-itself-that-the-flatte.patch` | arifi-fork-base | `995fc1ef3` | - | - | series: disclose in the record itself that the flatten absorbed 17 empty commits - content unchanged, history count changed (lane-147 checker finding 4) |
| 293 | `0293-ggml-port-the-tq3-TQ3_4S-family-codec-at-ids-48-51-T.patch` | arifi-fork-base | `68d640f9a` | - | - | ggml: port the tq3 TQ3_4S family codec at ids 48-51 (TYPE-ID-ALLOCATION 3.1.1) - Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb |
| 294 | `0294-tests-cover-the-tq3-family-in-test-quantize-fns-and-.patch` | arifi-fork-base | `cf979b6f0` | - | - | tests: cover the tq3 family in test-quantize-fns and test-backend-ops (kilobyte-scale proof before the 13 GB run) |
| 295 | `0295-contract-tq3-rows-48-51-implemented-behind-GGML_ARIF.patch` | arifi-fork-base | `7d2d3d19c` | - | - | contract: tq3 rows 48-51 implemented behind GGML_ARIFI_TURBO_WEIGHT_QUANTS, plus the three corrections the port forces (their 200 = our TURBO2_0; six ids, four destinations; token_embd is Q6_K) |
| 296 | `0296-tests-tq3_4s-tq3_4se-skipped-in-test-quantize-fns-wi.patch` | arifi-fork-base | `f861bd989` | - | - | tests: tq3_4s/tq3_4se skipped in test-quantize-fns with the measured reason (E3M5 scale caps at 0.492, harness data needs 1.4); tq3_0 given documented 3.5bpw bounds |
| 297 | `0297-tools-gguf-retag-tq3-lands-LAST-after-the-coherence-.patch` | arifi-fork-base | `117a1a850` | - | - | tools: gguf-retag-tq3 - lands LAST, after the coherence proof, per TYPE-ID-ALLOCATION 3.1.1. Decides ambiguous id 46 by measured byte span; five refusals, no in-place path |
| 298 | `0298-checker-fixes-choose_index-restored-verbatim-inverte.patch` | arifi-fork-base | `74461237e` | - | - | checker fixes: choose_index restored verbatim (inverted tie-break + 2 drifted constants, encoder-only); tq3 numeric block gives TQ3_4S real proof and pins every dot mapping; drop redundant GGML_API redecl |
| 299 | `0299-retag-verify-geometry-per-TENSOR-for-every-mapped-id.patch` | arifi-fork-base | `b251596db` | - | - | retag: verify geometry per TENSOR for every mapped id, not a file-level vote on 46 only - fixes silent rewrite of mixed/ambiguous files and of unguarded 36/200; guard empty and overlapping tables |
| 300 | `0300-retag-README-per-tensor-geometry-check-for-every-map.patch` | arifi-fork-base | `014279c1d` | - | - | retag README: per-tensor geometry check for every mapped id, ten refusals, byte-level assertion |
| 301 | `0301-lane-144-GOAL-2-step-1-TQ3_4S-id-48-Vulkan-port-dequ.patch` | arifi-fork-base | `ac7e6c974` | - | - | lane-144 GOAL 2 step 1: TQ3_4S (id 48) Vulkan port - dequant + mat-vec + mat-vec_id shaders, wired |
| 302 | `0302-lane-144-sweep-ROCmFP4-ROCmFPX-in-test-backend-ops-a.patch` | arifi-fork-base | `2747114c3` | - | - | lane-144: sweep ROCmFP4/ROCmFPX in test-backend-ops all_types (six identifiers, never listed before) |
| 303 | `0303-lane-144-CHECKER-FIX-TQ3_4S-was-missing-from-the-MUL.patch` | arifi-fork-base | `382c609a6` | - | - | lane-144 CHECKER FIX: TQ3_4S was missing from the MUL_MAT_ID staging-overflow guard |
| 304 | `0304-lane-145-Q6_0_ROCMFPX-Vulkan-fix-implement-the-6-bit.patch` | arifi-fork-base | `be3333f95` | - | - | lane-145: Q6_0_ROCMFPX Vulkan fix - implement the 6-bit unpack (block struct was 34B vs CPU 26B; qs read as whole signed bytes). 60/60 MUL_MAT + 24/24 GET_ROWS on Vulkan0, was 50/60 + 20/24. |
| 305 | `0305-lane-145-checker-finding-7-delete-block_rocmfpx_fp6_.patch` | arifi-fork-base | `f9db6a754` | - | - | lane-145 checker finding 7: delete block_rocmfpx_fp6_packed16 + its A_TYPE_PACKED16 define (26B block has no valid uint16 view; nothing reads it). Re-verified 60/60 MUL_MAT, 24/24 GET_ROWS, 12/12 MUL_MAT_ID. |
| 306 | `0306-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | ternary-g128 | `7414afcd9` | - | - | vulkan: ask the pipelines whether they exist, not the type id (SET_ROWS + CPY) |
| 307 | `0307-test-backend-ops-cover-the-CPY-types-supports_op-nam.patch` | arifi-fork-base | `5b60bbc1c` | - | - | test-backend-ops: cover the CPY types supports_op names that all_types omits |
| 308 | `0308-lane-148-defect-A-ROOT-FIX-Vulkan-turbo3-centroid-LU.patch` | arifi-fork-base | `4af110b08` | - | - | lane-148 defect A ROOT FIX: Vulkan turbo3 centroid LUT was stale at 5 sites - CPY turbo3->f32 ERR 8.4302e-5 -> OK. CPU/CUDA/Metal/SYCL all carry CENTROIDS_3BIT (-0.190207,-0.118786,-0.066822,-0.021663); Vulkan alone carried a table whose midpoints do not match nearest_centroid_3bit(). copy_to_quant.comp's TM decision boundaries were derived from the stale table and are corrected too. |
| 309 | `0309-lane-148-defect-B-ROOT-FIX-FA_TYPE_TURBO2-3-4_0-GLSL.patch` | arifi-fork-base | `88e4d7086` | - | - | lane-148 defect B ROOT FIX: FA_TYPE_TURBO2/3/4_0 GLSL spec-constant ids were stale (43/44/47) while enum ggml_type puts them at 200/201/202. Host passes the raw enum, so both switches fell through: fa_block_elems() returned its 1u default and dequantize4() returned vec4(0), zeroing V and the whole FA output. FLASH_ATTN_EXT -p turbo 0/1728 -> 1728/1728. |
| 310 | `0310-lane-148-F-69-sibling-coverage-test-backend-ops-FLAS.patch` | arifi-fork-base | `54c3a4f0f` | - | - | lane-148 F-69 sibling coverage: test-backend-ops FLASH_ATTN_EXT now covers TURBO2_0 too. Vulkan supports_op has always claimed turbo2 for FA but no test case existed, so its stale FA_TYPE id was invisible. 1728 -> 2592 cases, all PASS. |
| 311 | `0311-lane-148-checker-findings-4-5-FIXED-turbo4-SET_ROWS-.patch` | arifi-fork-base | `0155b7a3d` | - | - | lane-148 checker findings 4+5 FIXED: turbo4 SET_ROWS was 0/21 declined behind a green 'Backend Vulkan0: OK' banner and turbo2 had no SET_ROWS case at all. Root cause: no cpy_turbo{2,4}_0_f32 copy-from-quant shader, so the tests' read-back leg declined. Added turbo2/turbo4 dequantize()/dequantize4()/get_dm(), the two spv+pipeline registrations, both supports_op and the DISPATCH switch (a third hand-synced pair - supports_op alone aborts with 'Missing CPY op'), and a test_set_rows_turbo2 class. SET_ROWS_TURBO2/3/4 now 21/21 EXECUTED each. |
| 312 | `0312-lane-149-F-110-guard-rung-b-arifi_sync_check.py-CMak.patch` | arifi-fork-base | `51ec2c700` | - | - | lane-149 F-110 guard (rung b): arifi_sync_check.py + CMake wiring + 16 ARIFI-SYNC markers. Checks 49 LUT pairs, 16 FA_TYPE ids, 35 QUANT_K block sizes, 4 type-list SETs, FA K/V coverage; unregistered mirrors FAIL |
| 313 | `0313-lane-149-F-112-F-109-harness-guards-NOT_SUPPORTED-ca.patch` | ternary-g128 | `d6d02b6a0` | - | - | lane-149 F-112 + F-109 harness guards: NOT_SUPPORTED cases counted and named, zero-executed run now FAILS; all_types coverage-parity assert with reason-bearing waivers; Q2_0_G128 added to all_types |
| 314 | `0314-lane-149-F-109-payoff-q2_0_g128-SET_ROWS-tie-roundin.patch` | ternary-g128 | `0a5c197c4` | - | - | lane-149 F-109 payoff: q2_0_g128 SET_ROWS tie-rounding fixed. GLSL round() breaks .5 ties implementation-chosen; CPU roundf is half-away-from-zero. 8 f16 failures -> 2, ERR 5.1e-4 -> 1.7e-5 |
| 315 | `0315-lane-149-checker-driven-hardening-of-arifi-sync-chec.patch` | arifi-fork-base | `f3f3bcc94` | - | - | lane-149 checker-driven hardening of arifi-sync-check: FA_TYPE/QUANT_K literal spellings, ordinal (not line) pins, per-definition completeness, any array type, recursive shader walk, scalar-mirror class, expected-count assertions |
| 316 | `0316-lane-149-F-112-second-half-checker-finding-11-per-op.patch` | arifi-fork-base | `3647036df` | - | - | lane-149 F-112 second half (checker finding 11): per-op tallies name every op whose cases were ALL declined, so a zero-coverage family cannot hide inside a green full sweep |
| 317 | `0317-lane-150-step-1-TQ4_1S-read-back-leg-wired-CPY-tq4_1.patch` | arifi-fork-base | `56b1e5599` | - | - | lane-150 step 1: TQ4_1S read-back leg wired - CPY(tq4_1s,f32) now EXECUTES, 291/291 -> 293/293 |
| 318 | `0318-lane-150-step-2-TQ4_1S-write-leg-EXECUTES-SET_ROWS_T.patch` | arifi-fork-base | `8e4583e31` | - | - | lane-150 step 2: TQ4_1S write leg EXECUTES - SET_ROWS_TQ4_1S 17 cases/0 executed -> 17/17 PASS, and the inherited 5.0 waive is retired |
| 319 | `0319-lane-150-step-3-root-fix-for-the-hole-that-hid-the-w.patch` | arifi-fork-base | `e77c2bb17` | - | - | lane-150 step 3 (root fix for the hole that hid the whole class): per-op x TYPE zero-coverage tally |
| 320 | `0320-lane-150-checker-driven-fixes-type_bucket-prefix-bou.patch` | arifi-fork-base | `a8e84919e` | - | - | lane-150 checker-driven fixes: type_bucket prefix-boundary bug + dispatch block-size assert + the tallys limits named in code |
| 321 | `0321-lane-151-GOAL-6-hygiene-drop-648-tracked-_lane-out-d.patch` | arifi-fork-base | `004c5388c` | - | `GGML_ARIFI_VNNI_REPACK` | lane-151 GOAL 6 hygiene: drop 648 tracked _lane-out debris files (incl. an embedded shadow.git with commit receipts) and gitignore the dir; genericize estate paths in docs/, licenses/, tools/gguf-retag-tq3 |
| 322 | `0322-lane-151-GOAL-9-reserve-tq3-s-three-destination-less.patch` | arifi-fork-base | `a4a87f1f9` | - | - | lane-151 GOAL 9: reserve tq3's three destination-less serialized ids (37 TQ3_4SV -> 52, 31 TQ3_1S_AP1 -> 53, 44 TQ3_1S -> 54) as RESERVED-not-implemented in docs/TYPE-ID-ALLOCATION.md 3.1.2; geometry marked NOT DERIVED, no enumerators added, retag-tool refusals unchanged |
| 323 | `0323-lane-151-GOALs-6-7-README-refreshed-to-b10453-canoni.patch` | arifi-fork-base | `7a0ab91dc` | - | - | lane-151 GOALs 6+7: README refreshed to b10453 canonical arifi-main + the seven-ingested-sources table (two honest zeroes named); tq3 and ciru credits added; licenses/README.md gains the full Taken-from audit incl. the tq3 license GAP; UPDATE-RUNBOOK.md written (O(delta) recipe, THREE-flag configure, guards, the standard) |
| 324 | `0324-lane-151-GOAL-2-sources.json-base.ref-arifi-main-the.patch` | arifi-fork-base | `485fe7c59` | - | `GGML_ARIFI_VNNI_REPACK` | lane-151 GOAL 2: sources.json base.ref -> arifi/main (the canonical branch is now the series base) + series regen over the full canonical range: 323 patches, series check PASS, replay onto 4df29be4f identical outside patches/series |
| 325 | `0325-lane-151-GOAL-5-gitignore-.tokensave-the-fork-repo-n.patch` | arifi-fork-base | `e6b1bd94c` | - | - | lane-151 GOAL 5: gitignore .tokensave/ - the fork repo now carries its own code graph (63344 nodes, 154451 edges over 3407 files) and the index is per-machine, never committed |
| 326 | `0326-lane-151-CHECKER-ROOT-FIX-series-check-now-asserts-e.patch` | arifi-fork-base | `913ac83c9` | - | - | lane-151 CHECKER ROOT FIX: series check now asserts every patch is a COMMITTED BLOB, not just a file on disk. The byte-compare ran against the working directory, so untracked patches made it green while a fresh clone would die at git am - the defect that hid 33 missing patches twice. Help text corrected to match what the code does |
| 327 | `0327-lane-151-CHECKER-FIXES-docs-UPDATE-RUNBOOK-4.5-no-lo.patch` | arifi-fork-base | `d24d3bc5c` | - | - | lane-151 CHECKER FIXES (docs): UPDATE-RUNBOOK 4.5 no longer asserts a self-containment standard nothing on this estate meets - the four MinGW runtime DLLs are named and staged, and the measured stripped-PATH failure is recorded as identical for the raw build tree and the previous engine. licenses/README.md audit header stops claiming a purely mechanical trailer derivation: the tq3 row is a SUBJECT-line citation git never parses as a trailer, which is exactly why a trailer-only sweep would have missed the one real license gap |
| 328 | `0328-lane-151-HQ-DIRECTIVE-RETAIN-the-licenses-instead-of.patch` | arifi-fork-base | `e5145724e` | - | - | lane-151 HQ DIRECTIVE: RETAIN the licenses instead of flagging them. tq3-MIT, prisml-MIT and ciru-MIT extracted verbatim from this repo's OWN fetched remotes (git show refs/remotes/<r>/<head>:LICENSE). REFUTES my own GOAL-6 finding and a year-old one: I wrote 'no clone remains on the estate' and Phase 0 wrote 'PrismML has no LICENSE' - both were absence claims checked against the wrong artifact while the remotes sat fetched in this very repository. All seven registered sources audited at once. thecodacus STAYS open: no remote, no text, nothing invented |
| 329 | `0329-licenses-thecodacus-MIT-retained-from-the-public-Git.patch` | arifi-fork-base | `b62a2a324` | - | - | licenses: thecodacus-MIT retained from the public GitHub fork - the last open attribution CLOSED (no local remote != no source; President's correction 2026-08-18) |
| 330 | `0330-lane-152-GOAL-1-upstream-bump-b10453-b10481-the-FIRS.patch` | arifi-fork-base | `b68aba7a2` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | lane-152 GOAL 1: upstream bump b10453 -> b10481, the FIRST live execution of UPDATE-RUNBOOK 2. 348 patches replayed onto 25ae3a9b3, 4 conflicts resolved by hand, series regenerated to 329, zero merges. |
| 331 | `0331-lane-152-what-the-FIRST-live-run-of-UPDATE-RUNBOOK-s.patch` | arifi-fork-base | `e958c8d1d` | - | - | lane-152: what the FIRST live run of UPDATE-RUNBOOK section 2 corrected - one build break, two arifi_sync defects, the runbook itself, and the last license row |
| 332 | `0332-lane-152-GOAL-2-batch-1-ciru-s-Vulkan-SPIRV-Headers-.patch` | arifi-fork-base | `34732cace` | ciru@8ffb2cd5a9b9e7eecc49335e0c79291a0df81d65, tq3@b1dd18410620bbf16909444d95ee2f9c10015b085, rocmfpx@8e6277f855df2a27ce072525ed19f3adc4138c47 | - | lane-152 GOAL 2 batch 1: ciru's Vulkan SPIRV-Headers fallback, tq3's f32 hybrid-SSM state gates, rocmfpx's -ffast-math guard |
| 333 | `0333-lane-152-GOAL-2-batch-2-three-lfm2-pre-tokenizer-has.patch` | arifi-fork-base | `0258abc2d` | rocmfpx@95bb4c798ee8c571e6b34ba4dd0ce15f4c2b9232 | - | lane-152 GOAL 2 batch 2: three lfm2 pre-tokenizer hashes that only rocmfpx had - and the reason they must be pre-computed |
| 334 | `0334-lane-152-pins-THREE-advance-with-reviews-upstream-ro.patch` | arifi-fork-base | `7beafc62b` | - | - | lane-152 pins: THREE advance with reviews (upstream, rocmfpx, tq3), TWO deliberately stay BEHIND (prisml, ciru) |
| 335 | `0335-lane-152-checker-F7-the-old-base-is-tag-b10454-not-b.patch` | arifi-fork-base | `613d58c6f` | - | - | lane-152 checker F7: the old base is tag b10454, not b10453 - correction of record |
| 336 | `0336-repo-env.json-WI-1540-tail-the-engine-repo-s-env-man.patch` | arifi-fork-base | `9a9e9a919` | - | - | repo-env.json (WI-1540 tail): the engine repo's env manifest - PowerShell-only law (F-107), three-flag configure (F-111), cmd.exe redirection (F-108), type-id FLAG E, series discipline, runbook pointer |
| 337 | `0337-WI-1580-sub-2-UPDATE-RUNBOOK-section-7-the-4.5-line-.patch` | arifi-fork-base | `cab3925b0` | - | - | WI-1580 sub-2: UPDATE-RUNBOOK section 7 - the 4.5 line was stale. lane-153 staged models/engines/arifi-b10481-union/ from a verified build of canonical, smoked from the staged copies, and flipped current-state.json + artifact-catalog.json, so 4.5 HAS now been executed once, by hand. The bump distinction is KEPT because it is still true: no bump has ever driven staging end to end (lane-152's bump refused at leg 2). Narrow edit - the section-2 and section-3 paragraphs around it are untouched. Note for the record: the lane-153 REPORT cites this text at UPDATE-RUNBOOK.md:238-244; it actually lives at :308-323 |
| 338 | `0338-vulkan-land-the-MoE-expert-cache-provider-files-VK-M.patch` | ternary-g128 | `8d0ecc7cf` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: land the MoE expert cache provider files (VK-MOE-CACHE step 1 of 2 - files, not yet wired) |
| 339 | `0339-graph-derive-the-V-unpad-head-count-from-the-attenti.patch` | arifi-fork-base | `bb7809f26` | TheTom/llama-cpp-turboquant@f0420e056 | - | graph: derive the V-unpad head count from the attention output tensor, not from hparams |
| 340 | `0340-vulkan-wire-the-MoE-expert-cache-provider-into-the-b.patch` | ternary-g128 | `49d451626` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: wire the MoE expert cache provider into the build (VK-MOE-CACHE step 2 of 2) |
| 341 | `0341-vulkan-complete-the-MoE-cache-port-the-56-provider-h.patch` | ternary-g128 | `5b117dc2d` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: complete the MoE-cache port - the +56 provider-header addition, the four handle accessors, and the include-order ROOT FIX the link error exposed |
| 342 | `0342-sources-advance-the-turboquant-pin-f6124e914-7ebcbb0.patch` | arifi-fork-base | `3eedb88f1` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: advance the turboquant pin f6124e914 -> 7ebcbb0b6 with pin_review_2026_08_18_lane154 |
| 343 | `0343-off-rig-backends-land-the-12-CUDA-Metal-SYCL-HIP-row.patch` | arifi-fork-base | `9c10c5aff` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | off-rig backends: land the 12 CUDA/Metal/SYCL/HIP rows unbuilt, per the HQ ruling of 2026-08-18 |
| 344 | `0344-sources-pin_review-names-the-12-off-rig-shas-and-the.patch` | arifi-fork-base | `209145e48` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: pin_review names the 12 off-rig shas and their BUILD-UNPROVEN status (HQ condition 3) |
| 345 | `0345-sources-correct-the-pin_review-the-contested-12-12-a.patch` | arifi-fork-base | `2dd9c0b69` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: correct the pin_review - the contested 12/12 apply-conflict count is no longer the justification (checker F1) |
| 346 | `0346-sources-pin_review-records-the-apply-count-claim-as-.patch` | arifi-fork-base | `c5ad74b1c` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: pin_review records the apply-count claim as WRONG, not disputed (checker F1, resolved against me) |
| 347 | `0347-base-b10481-25ae3a9b3-b10488-9d77fa172-series-regen-.patch` | arifi-fork-base | `172729a3b` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | base: b10481/25ae3a9b3 -> b10488/9d77fa172 + series regen 346 (lane-156 upstream bump) |
| 348 | `0348-moe-cache-heat-protected-eviction-keeps-hot-experts-.patch` | arifi-fork-base | `92e2f0c01` | llama-cpp-turboquant@730cc87ed (+ its doc follow-up cef75bf06) | - | moe-cache: heat-protected eviction keeps hot experts resident (turboquant tail) |
| 349 | `0349-tests-initialise-non-contiguous-tensors-row-by-row-t.patch` | arifi-fork-base | `c44d68931` | llama-cpp-turboquant@f58ee0e97 | - | tests: initialise non-contiguous tensors row by row (turboquant row f58ee0e97) |
| 350 | `0350-sources-advance-the-turboquant-pin-7ebcbb0b6-d14e368.patch` | arifi-fork-base | `5e402b5cf` | - | - | sources: advance the turboquant pin 7ebcbb0b6 -> d14e36827 with a dated lane-156 pin_review |
| 351 | `0351-sources-prisml-pin-HELD-deliberately-with-a-sharper-.patch` | arifi-fork-base | `0de7a647a` | - | - | sources: prisml pin HELD deliberately with a sharper reason + the v6 K-cache-mean-center verdict (lane-156 GOAL 4) |
| 352 | `0352-sources-ciru-re-derived-315-15-and-HELD-with-all-15-.patch` | arifi-fork-base | `afeaf5f1e` | - | - | sources: ciru re-derived 315 -> 15 and HELD with all 15 reviewed (lane-156 GOAL 5) |
| 353 | `0353-sources-advance-remotes.upstream.pin-25ae3a9b3-9d77f.patch` | arifi-fork-base | `65835bf99` | - | - | sources: advance remotes.upstream.pin 25ae3a9b3 -> 9d77fa172 (b10488) with its review |

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

## Unclassified

These commits fell through every grouping rule in `arifi_sync.py`. That is surfaced
rather than silently bucketed - add a rule when a new source appears.

- `60fec85b1` patches: generate the full auditable 38-patch series from git
- `cb03ea262` tools: add arifi-sync - the currency, series and upstream-bump driver
- `82bc104af` patches: move banked PrismML source patches out of the generated series directory
- `9fe49d715` patches: make the series generator implement its own convergence rule
- `0a272bead` patches: stop the manifest deriving anything from excluded commits
- `80e707de6` port: Q1_0 repack scaffolding + Arm NEON/DP GEMV-GEMM kernels
- `daa30d545` port: native x86 Q2_0 vec_dot (AVX512-VNNI / AVX-VNNI), drop scalar fallback alias
- `f26dc1227` port: x86 AVX512-VNNI repack GEMV/GEMM for Q1_0 and Q2_0
- `732d7ff2f` fix(ggml-cpu): parameterize Q2_0 GEMM activation stride by QK2_0/QK8_0 - g64 NaN
- `67d0a1392` fix(ggml-cpu): same g64 stride defect in the ported x86 Q2_0 vec_dot
- `ceab1af31` ggml: reserve the ArifiLabs type-ID blocks at the enum itself
- `58ae9ab4c` ggml: ROCmFP4/ROCmFPX weight formats behind GGML_ARIFI_ROCMFPX_FORMATS
- `4fc75deea` llama: ROCmFPX file types, quantizer wiring and the gguf-py registry
- `235089712` tools: record GGML_ARIFI_ROCMFPX_FORMATS in the build-recipe snapshot
- `736666548` feat: --arifi-profile and a CPU-only VNNI-repack startup advisory
- `49fdfdc47` sync: currency GREEN — TurboQuant checkouts pinned at their lane-110C-reviewed shas (c26cbdffc, ba52ad107), rocmfpx pin advanced to db6844d9b after 6-commit review (HIP pinned-staging noted for the HIP-deltas slot)
- `37b1196c9` fix(ggml-cpu/x86): don't pass __m256i by value across the repack GEMV/GEMM ABI boundary (Windows 0xC0000005)
- `8337e5de5` ggml: TurboQuant TQ3_1S/TQ4_1S weight formats behind GGML_ARIFI_TURBO_WEIGHT_QUANTS
- `5d4d52e8d` ggml-cpu/x86: use the row-stride parameter in repack GEMV stores instead of the row count
- `6f27d465f` docs(FINDINGS): F-09 - the repack/GPU trade-off is a compute-placement choice, not a bug
- `848521ff7` docs(FINDINGS): record why the repack auto-enable was NOT shipped
- `8da0c5ab1` provenance: catch the silent trailer-block break instead of only reporting it
- `e3654bb06` gitattributes: pin .githooks to LF so the hook runs on Linux and macOS
- `89ec1a675` githooks: force a normalized blob for the commit-msg hook
- `2fdac0466` docs(FINDINGS): F-09 exit 1 is the direction - dual residency, not a forced choice
- `055c85479` repack: dual residency - GGML_ARIFI_VNNI_REPACK=2 keeps prefill on the GPU AND repacks decode
- `ce6c5b7da` repack: dual residency measured - both wins held; fix the hot-path cost that ate the first one
- `962bd6492` repack: fix the shadow tensor COUNT in the accounting line, and strengthen the claim it supports
- `794f1303c` repack: CPU kernels for the 128-group ternary format
- `dd67b9836` tests: direct equivalence test for the ternary repack kernels
- `58d06a96e` F-06: measure the GPU tier on one binary, and count the placement the prompt claim rested on
- `ebbbc00f4` registry: the dual-residency prompt mechanism is measured now, not ASSUMED
- `3bbf41493` registry: our DSpark verdict was never ours -- the drafter cannot load on this fork
- `5c7291ec6` speculative: DFlash/DSpark drafters have no vocab -- take the mask token from metadata
- `62bd9e626` expert cache: stride the bundle by the PADDED matrix size -- unblocks our own brain
- `34898ba68` expert cache: POWERINFER_EXPERT_HEATMAP -- measure access skew before building a pin policy
- `d796acb65` ours/1163cb34939fe4a9cb07aec034c5954144497ae9.patch
- `50e1325c0` 1-core-types/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `6aa91d8ae` 1-core-types/0002-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `baeb34385` 2-cpu-reference/0001-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `9ae546228` 2-cpu-reference/0007-ggml-cpu-declare-turbo3_cpu_wht_group_size-extern-no.patch
- `1fe1f2db4` 2-cpu-reference/0008-merge-fix-post-merge-build-and-crash-issues.patch
- `3f6a399e8` 3-vulkan/0001-vulkan-metal-hip-add-TurboQuant-kernel-support.patch
- `373044cc5` 3-vulkan/0002-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch
- `b09c77e5a` 3-vulkan/0003-fix-close-complete-audit-findings-7-port-regressions.patch
- `738e567fd` 3-vulkan/0004-vulkan-reconstruct-supports_op-after-rebase-merge-da.patch
- `3b2aedbfb` 3-vulkan/0006-vulkan-restore-TQ4_1S-weight-type-wiring-lost-in-the.patch
- `1105ccf2d` 3-vulkan/0007-vulkan-add-the-TQ3_1S-weight-type.patch
- `7e764805a` 3-vulkan/0008-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch
- `af8cea278` 3-vulkan/0013-vulkan-TQ-rotated-matmul-shader-groundwork-A-side-lo.patch
- `06f635ff7` 3-vulkan/0015-vulkan-create-the-TQ-activation-rotate-pipeline.patch
- `dccba6879` 3-vulkan/0017-vulkan-rotate-the-staged-activation-copy-in-place-dr.patch
- `5052ff7c3` 3-vulkan/0018-vulkan-register-the-TQ-rotated-mul_mm_id-pipelines.patch
- `63688cbc4` 3-vulkan/0021-vulkan-fix-SET_ROWS-block-decomposition-for-turbo-we.patch
- `a7748ab8f` 4-other-backends/0006-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch
- `26d4f72e8` 4-other-backends/0007-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch
- `e9b0e81b6` 4-other-backends/0008-hip-add-mixed-f16-bf16-q8_0-fattn-vec-instances-to-t.patch
- `7021b1938` 4-other-backends/0009-cuda-revert-GQA-ratio-dispatch-to-comparison-form-fi.patch
- `f7958a645` 4-other-backends/0010-cuda-restore-Volta-GQA-modulo-dispatch-over-revert-i.patch
- `75763cb42` 4-other-backends/0015-cuda-rework-MoE-expert-cache-execution.patch
- `03651521a` 4-other-backends/0016-cuda-fix-MoE-cache-capacity-accounting.patch
- `e854b1b6b` 4-other-backends/0017-cuda-fix-MoE-cache-scratch-budget-replacement.patch
- `4264f6065` 4-other-backends/0018-cuda-bound-MoE-MMV-tail-row-reads.patch
- `56c308ff0` 4-other-backends/0019-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `e3ed98713` 4-other-backends/0020-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `0a45fbda4` 4-other-backends/0021-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `68b183397` 4-other-backends/0022-cuda-prefer-generic-MMV-for-compatible-MoE-cache-nod.patch
- `035653447` 4-other-backends/0023-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch
- `5bbbe10c9` 4-other-backends/0024-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `0b44f71ef` 4-other-backends/0025-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `b47516488` 4-other-backends/0026-cuda-bound-automatic-MoE-cache-admission.patch
- `6177c12da` 4-other-backends/0027-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `5328a90ed` 4-other-backends/0028-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `29841da68` 4-other-backends/0029-common-surface-MoE-cache-activation.patch
- `f7bbed3ce` 4-other-backends/0030-cuda-accelerate-complete-MoE-cache-pools.patch
- `b35898abb` 4-other-backends/0031-cuda-report-oversized-MoE-cache-nodes.patch
- `05f8c755e` 4-other-backends/0032-cuda-clarify-MoE-cache-session-diagnostics.patch
- `0dc2d727a` 4-other-backends/0033-cuda-report-MoE-cache-pair-residency.patch
- `8ccf3ac8d` 4-other-backends/0034-cuda-pack-MoE-cache-dispatch-inputs.patch
- `050bfd213` 4-other-backends/0037-metal-port-TQ_NO_ROTATE-escape-hatch-from-stash-opt-.patch
- `8719e54d5` 4-other-backends/0039-moe-cache-enable-HIP-backend-by-removing-no-op-stubs.patch
- `1173da729` 4-other-backends/0040-moe-cache-count-dispatch-contention-bypasses-P1.patch
- `045c6b7bc` 4-other-backends/0041-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `7e52d3825` 4-other-backends/0042-moe-cache-audit-fixes-H1-F1-F2-A1.patch
- `936c6dc49` 4-other-backends/0043-moe-cache-fix-registry-build-I1-follow-up.patch
- `501dbc4cc` 4-other-backends/0044-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `33fb6b8e1` 4-other-backends/0045-cuda-fix-HIP-MoE-cache-compatibility-and-q8_1-sums.patch
- `5b5c5a6c8` 4-other-backends/0046-moe-cache-add-logging-at-silent-failure-points-acros.patch
- `1f5683b55` 4-other-backends/0048-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch
- `f0a191b2c` 4-other-backends/0049-sycl-drop-the-duplicated-nthreads-declarations-in-fa.patch
- `3010d1d04` 4-other-backends/0050-fattn-vec-split-the-turbo-K-dot-at-D-128-to-stop-the.patch
- `ed4109a10` 5-models-server/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `0fe1fd6c3` 5-models-server/0003-cli-break-interactive-loop-on-stdin-EOF-fixes-hot-sp.patch
- `70bca75a1` 5-models-server/0004-tools-expose-TQ3_1S-TQ4_1S-in-quantize-table-and-lla.patch
- `e136f6bb2` 5-models-server/0007-llama-support-DeepSeek-V4-tensor-split.patch
- `7e85e5d47` 5-models-server/0008-llama-mirror-DS4-q_a-kv-down-projections-in-tensor-s.patch
- `719158f46` 5-models-server/0009-glm-dsa-guard-lightning-indexer-Hadamard-rotation-wh.patch
- `d595325d3` 5-models-server/0010-laguna-add-arch-tables-llm_arch-KV-keys-tensors-and-.patch
- `f4b36cec1` 5-models-server/0011-laguna-add-model-class-hparams-wiring-vocab-pre-toke.patch
- `d51893923` 5-models-server/0013-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch
- `185a1120a` 5-models-server/0014-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch
- `bfb1bee36` 5-models-server/0015-cuda-sum-MoE-expert-outputs-on-decode-n_tokens-1.patch
- `5021f13f7` 5-models-server/0016-laguna-deduplicate-definitions-vs-the-early-snapshot.patch
- `08e73374e` 5-models-server/0017-dflash-fix-DSpark-tensor-meta-accessor-after-laguna-.patch
- `536d09d3e` 5-models-server/0021-common-support-the-DSpark-sidecar-resolution-26458.patch
- `a5786cfc7` 5-models-server/0037-merge-fix-post-merge-build-and-crash-issues.patch
- `1efb9d7ab` 5-models-server/0041-moe-cache-route-fit-probing-and-test-session-calls-t.patch
- `ca9c1267d` 5-models-server/0048-llama-bench-document-pw-prefetch-weights-in-help-and.patch
- `f2473ad09` 5-models-server/0050-llama-bench-remove-stale-prefetch-weights-documentat.patch
- `49c7450c6` 5-models-server/0051-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch
- `1bb40c5d6` 6-tests-build/0001-tests-re-add-test-turbo-quant.c-round-trip-test.patch
- `a8d9de2ab` 6-tests-build/0002-tests-port-fork-turbo-backend-and-quantize-fns-test-.patch
- `93134ab31` 6-tests-build/0003-fix-close-complete-audit-findings-7-port-regressions.patch
- `2018bbbb0` 6-tests-build/0004-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch
- `77ac7a734` 6-tests-build/0006-ggml-webgpu-add-support-for-f16-repeat-26307.patch
- `43255f274` 6-tests-build/0010-tests-restore-DSV4_HC_COMB-eps-sweep-lost-in-the-244.patch
- `ec9433760` 6-tests-build/0011-tests-cast-float-printf-args-in-test-turbo-quant-arm.patch
- `48cb1d7d3` 6-tests-build/0012-vulkan-add-the-TQ3_1S-weight-type.patch
- `a6d2dece6` 6-tests-build/0013-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch
- `457a87e07` 6-tests-build/0014-ggml-cpu-remove-the-per-call-mallocs-from-the-TurboQ.patch
- `f1650e0be` 6-tests-build/0015-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch
- `ea2ad4b72` 6-tests-build/0016-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch
- `175c26e7e` 6-tests-build/0017-tests-port-11-DSv4-shaped-TQ-MUL_MAT-cases-from-sync.patch
- `4918bea3a` 6-tests-build/0019-cuda-fix-MoE-cache-capacity-accounting.patch
- `f2d9aa5be` 6-tests-build/0020-common-preserve-implicit-MoE-cache-provider-settings.patch
- `74dc5121f` 6-tests-build/0021-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `af75eec55` 6-tests-build/0022-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `7c0c9c7dd` 6-tests-build/0023-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `f348f49a7` 6-tests-build/0024-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch
- `88a9c58fc` 6-tests-build/0025-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `297073e8c` 6-tests-build/0026-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `1e29d303b` 6-tests-build/0027-cuda-bound-automatic-MoE-cache-admission.patch
- `729b634fd` 6-tests-build/0028-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `bec4a67dd` 6-tests-build/0029-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `7dc40864f` 6-tests-build/0030-common-surface-MoE-cache-activation.patch
- `e9b704e24` 6-tests-build/0031-cuda-accelerate-complete-MoE-cache-pools.patch
- `fab456a56` 6-tests-build/0032-cuda-report-oversized-MoE-cache-nodes.patch
- `6cb1eb5e4` 6-tests-build/0033-tests-initialise-non-contiguous-tensors-row-by-row.patch
- `d2625cce1` 6-tests-build/0034-tests-cover-turbo-KV-flash-attention-at-head-dim-256.patch
- `c26532749` 6-tests-build/0037-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `c59d820b3` 6-tests-build/0038-moe-cache-route-fit-probing-and-test-session-calls-t.patch
- `67cdd73ef` 6-tests-build/0039-moe-cache-route-context-eligibility-probe-and-remain.patch
- `d25966544` 6-tests-build/0040-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `990baa929` 6-tests-build/0041-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch
- `b0eb2466f` 6-tests-build/0042-test-moe-cache-accept-IGPU-devices-not-just-GPU.patch
- `a66e2dc8d` 6-tests-build/0043-test-moe-cache-make-cache-shared-budget-UMA-aware-wi.patch
- `116ec29c8` 6-tests-build/0044-test-moe-cache-save-and-restore-the-CUDA-device-arou.patch
- `296f04704` 6-tests-build/0045-tests-tell-ctest-that-77-means-skip-for-test-moe-cac.patch
- `ce735ddae` resolved/0009-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch
- `e8c09d32f` resolved/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch
- `8560eeda2` resolved/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch
- `b0bbeda79` resolved/0005-merge-close-TurboQuant-parity-gaps.patch
- `53efca8a8` resolved/0036-common-surface-MoE-cache-activation.patch
- `768d34d3c` resolved/0019-vulkan-dispatch-the-TQ-rotated-mul_mm_id-path.patch
- `5ee7f3e46` resolved/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch
- `c0fe939fe` resolved/0004-fix-close-complete-audit-findings-7-port-regressions.patch
- `db5c686ba` resolved/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch
- `9b4c187d8` resolved/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch
- `57418f7e6` resolved/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch
- `c2149033d` resolved/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch
- `8044cce88` resolved/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch
- `b38144cb2` resolved/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch
- `4d31af45d` resolved/0006-fix-close-complete-audit-findings-7-port-regressions.patch
- `0e724a154` resolved/0022-DeepseekV4-MTP-DSpark-25784.patch
- `ef5d840d6` resolved/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch
- `d57b034fb` resolved/0029-server-harden-shared-draft-device-placement.patch
- `85026041f` resolved/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch
- `843ca1953` resolved/0026-cuda-rework-MoE-expert-cache-execution.patch
- `b51ba6612` resolved/0033-server-account-for-MTP-placement-in-fit-reservations.patch
- `f2054577a` resolved/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `6535f5ad1` resolved/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch
- `822ce0592` resolved/0039-model-Muse-Glimmer-Support-26841.patch
- `d3f2a53d0` resolved/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch
- `d8c788ed6` resolved/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch
- `5cc85c1d7` resolved/0046-llama-remove-dead-MSA-indexer-scaffolding.patch
- `69b1eeb1a` resolved/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch
- `d578c7318` resolved/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch
- `93aac2b3e` resolved/0005-test-fix-some-CI-errors-26415.patch
- `9dac80c5d` resolved/remaining-conflict-markers
- `57b04f4b9` layer7/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `185d2a929` layer7/0002-Update-GGMLQuantizationType-and-LlamaFileType-enums-.patch
- `8d71a9919` layer7/0003-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `13de91fc0` layer7/0005-docs-add-rebase-plan-and-TurboQuant-recipes.patch
- `a6f37c812` layer7/0007-gguf-py-dedupe-stacked-merge-artifacts-in-constants..patch
- `0b6d88259` layer7/0008-docs-add-KV-cache-quantization-guide-update-rebase-p.patch
- `5ac489232` layer7/0009-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch
- `5907690b6` layer7/0010-merge-close-TurboQuant-parity-gaps.patch
- `6de0593e6` layer7/0011-metal-restore-lost-kernel-templates-add-offline-shad.patch
- `24b3987ad` layer7/0014-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch
- `b08938206` layer7/0019-fix-apply-rebase-audit-fixes-stack-overflow-quality-.patch
- `e89622bea` layer7/0020-docs-add-TurboQuant-project-overview-to-AGENTS.md-an.patch
- `e6c5c71f6` layer7/0021-cuda-rework-MoE-expert-cache-execution.patch
- `24f05df43` layer7/0022-docs-update-MoE-cache-validation.patch
- `93316df89` layer7/0023-common-preserve-implicit-MoE-cache-provider-settings.patch
- `c8b4a8d1b` layer7/0024-server-harden-shared-draft-device-placement.patch
- `b04954e60` layer7/0025-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `086eadd2e` layer7/0026-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `b3498825b` layer7/0027-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `e806fd06e` layer7/0028-docs-update-MoE-cache-validation.patch
- `e6aba2b36` layer7/0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `1576ca432` layer7/0030-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `ea9107aec` layer7/0031-cuda-bound-automatic-MoE-cache-admission.patch
- `1d090cfd8` layer7/0032-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `fdd44dacf` layer7/0033-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `5400bed8f` layer7/0034-common-surface-MoE-cache-activation.patch
- `d6dc57c77` layer7/0035-docs-fix-MoE-cache-benchmark-verbosity.patch
- `3f23a766d` layer7/0036-cuda-accelerate-complete-MoE-cache-pools.patch
- `0a77295f6` layer7/0037-cuda-report-oversized-MoE-cache-nodes.patch
- `e0c3c542c` layer7/0038-cuda-clarify-MoE-cache-session-diagnostics.patch
- `3eb5eaffd` layer7/0039-docs-record-MoE-cache-workload-convergence.patch
- `a1ad995a7` layer7/0040-cuda-report-MoE-cache-pair-residency.patch
- `d4e8d6264` layer7/0041-docs-document-what-the-test-suites-do-and-do-not-cov.patch
- `48308188f` layer7/0044-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `9f7eadd29` layer7/0046-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `d6e294ca4` layer7/0048-docs-document-INFO-level-MoE-cache-disable-reasons-i.patch
- `d18c1056f` layer7/0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CACHE.md-and-re.patch
- `22e48ac3e` layer7/0050-docs-correct-MOE-CACHE.md-against-the-implementation.patch
- `c92b5fa46` layer7/0051-ggml-split-the-turbo3-group-size-declaration-from-it.patch
- `0de0de978` resolve 8 committed conflict markers on merit
- `f461ceece` fix the merge so it builds: 5 files, 6 defects
- `9955b565f` merge repairs: seven defect classes, and one the compiler could never have reported
- `297dba4d9` ROCmFPX on Vulkan: the shader half, ported. NOT YET WORKING - it segfaults in execution.
- `c47812781` currency: register ALL SEVEN forks, so nothing can drift unseen again
- `06eb6e0c1` series: base and the branch it describes are ONE pair, and the linearity rule is now a gate
- `4906d1dc1` hygiene: the code-review skill taught a type-ID map the arbitration deleted, and the UI was fetched from a moving tag
- `54a8ffeb7` contract: tq3 is the first family we must renumber, so write down why before anyone ports it
- `d327675e0` sources: honest pins - turboquant re-pinned off a dead lineage, tq3 main-understates note REFUTED, five type-id contract collisions recorded (lane-143)
- `9d8ad1b84` sources: lane-143 self-refutation - the turbo3 ODR 'defect' does not exist in our tree (GGML_API already carries extern on all four branches); prisml pin REVERTED to keep the BEHIND signal live; FLAG E severity re-derived from loader code
- `01d7826d7` sources r2 (checker BLOCK F1-F6): ciru 160 was 145 commits of rocmfpx history - corrected to a 7-15 bracket, no-ancestor premise refuted, GOAL-3 ciru review performed, rocmfpx.ref revert, prisml contradiction removed, FLAG E sensitivity added
- `638f76a6b` series: point base.ref at the LINEAR branch - recut-linear/2026-08-17 is the byte-identical flatten of the blessed union branch, so the series is replayable at last (lane-147)
- `995fc1ef3` series: disclose in the record itself that the flatten absorbed 17 empty commits - content unchanged, history count changed (lane-147 checker finding 4)
- `68d640f9a` ggml: port the tq3 TQ3_4S family codec at ids 48-51 (TYPE-ID-ALLOCATION 3.1.1) - Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb
- `cf979b6f0` tests: cover the tq3 family in test-quantize-fns and test-backend-ops (kilobyte-scale proof before the 13 GB run)
- `7d2d3d19c` contract: tq3 rows 48-51 implemented behind GGML_ARIFI_TURBO_WEIGHT_QUANTS, plus the three corrections the port forces (their 200 = our TURBO2_0; six ids, four destinations; token_embd is Q6_K)
- `f861bd989` tests: tq3_4s/tq3_4se skipped in test-quantize-fns with the measured reason (E3M5 scale caps at 0.492, harness data needs 1.4); tq3_0 given documented 3.5bpw bounds
- `117a1a850` tools: gguf-retag-tq3 - lands LAST, after the coherence proof, per TYPE-ID-ALLOCATION 3.1.1. Decides ambiguous id 46 by measured byte span; five refusals, no in-place path
- `74461237e` checker fixes: choose_index restored verbatim (inverted tie-break + 2 drifted constants, encoder-only); tq3 numeric block gives TQ3_4S real proof and pins every dot mapping; drop redundant GGML_API redecl
- `b251596db` retag: verify geometry per TENSOR for every mapped id, not a file-level vote on 46 only - fixes silent rewrite of mixed/ambiguous files and of unguarded 36/200; guard empty and overlapping tables
- `014279c1d` retag README: per-tensor geometry check for every mapped id, ten refusals, byte-level assertion
- `ac7e6c974` lane-144 GOAL 2 step 1: TQ3_4S (id 48) Vulkan port - dequant + mat-vec + mat-vec_id shaders, wired
- `2747114c3` lane-144: sweep ROCmFP4/ROCmFPX in test-backend-ops all_types (six identifiers, never listed before)
- `382c609a6` lane-144 CHECKER FIX: TQ3_4S was missing from the MUL_MAT_ID staging-overflow guard
- `be3333f95` lane-145: Q6_0_ROCMFPX Vulkan fix - implement the 6-bit unpack (block struct was 34B vs CPU 26B; qs read as whole signed bytes). 60/60 MUL_MAT + 24/24 GET_ROWS on Vulkan0, was 50/60 + 20/24.
- `f9db6a754` lane-145 checker finding 7: delete block_rocmfpx_fp6_packed16 + its A_TYPE_PACKED16 define (26B block has no valid uint16 view; nothing reads it). Re-verified 60/60 MUL_MAT, 24/24 GET_ROWS, 12/12 MUL_MAT_ID.
- `5b60bbc1c` test-backend-ops: cover the CPY types supports_op names that all_types omits
- `4af110b08` lane-148 defect A ROOT FIX: Vulkan turbo3 centroid LUT was stale at 5 sites - CPY turbo3->f32 ERR 8.4302e-5 -> OK. CPU/CUDA/Metal/SYCL all carry CENTROIDS_3BIT (-0.190207,-0.118786,-0.066822,-0.021663); Vulkan alone carried a table whose midpoints do not match nearest_centroid_3bit(). copy_to_quant.comp's TM decision boundaries were derived from the stale table and are corrected too.
- `88e4d7086` lane-148 defect B ROOT FIX: FA_TYPE_TURBO2/3/4_0 GLSL spec-constant ids were stale (43/44/47) while enum ggml_type puts them at 200/201/202. Host passes the raw enum, so both switches fell through: fa_block_elems() returned its 1u default and dequantize4() returned vec4(0), zeroing V and the whole FA output. FLASH_ATTN_EXT -p turbo 0/1728 -> 1728/1728.
- `54c3a4f0f` lane-148 F-69 sibling coverage: test-backend-ops FLASH_ATTN_EXT now covers TURBO2_0 too. Vulkan supports_op has always claimed turbo2 for FA but no test case existed, so its stale FA_TYPE id was invisible. 1728 -> 2592 cases, all PASS.
- `0155b7a3d` lane-148 checker findings 4+5 FIXED: turbo4 SET_ROWS was 0/21 declined behind a green 'Backend Vulkan0: OK' banner and turbo2 had no SET_ROWS case at all. Root cause: no cpy_turbo{2,4}_0_f32 copy-from-quant shader, so the tests' read-back leg declined. Added turbo2/turbo4 dequantize()/dequantize4()/get_dm(), the two spv+pipeline registrations, both supports_op and the DISPATCH switch (a third hand-synced pair - supports_op alone aborts with 'Missing CPY op'), and a test_set_rows_turbo2 class. SET_ROWS_TURBO2/3/4 now 21/21 EXECUTED each.
- `51ec2c700` lane-149 F-110 guard (rung b): arifi_sync_check.py + CMake wiring + 16 ARIFI-SYNC markers. Checks 49 LUT pairs, 16 FA_TYPE ids, 35 QUANT_K block sizes, 4 type-list SETs, FA K/V coverage; unregistered mirrors FAIL
- `f3f3bcc94` lane-149 checker-driven hardening of arifi-sync-check: FA_TYPE/QUANT_K literal spellings, ordinal (not line) pins, per-definition completeness, any array type, recursive shader walk, scalar-mirror class, expected-count assertions
- `3647036df` lane-149 F-112 second half (checker finding 11): per-op tallies name every op whose cases were ALL declined, so a zero-coverage family cannot hide inside a green full sweep
- `56b1e5599` lane-150 step 1: TQ4_1S read-back leg wired - CPY(tq4_1s,f32) now EXECUTES, 291/291 -> 293/293
- `8e4583e31` lane-150 step 2: TQ4_1S write leg EXECUTES - SET_ROWS_TQ4_1S 17 cases/0 executed -> 17/17 PASS, and the inherited 5.0 waive is retired
- `e77c2bb17` lane-150 step 3 (root fix for the hole that hid the whole class): per-op x TYPE zero-coverage tally
- `a8e84919e` lane-150 checker-driven fixes: type_bucket prefix-boundary bug + dispatch block-size assert + the tallys limits named in code
- `004c5388c` lane-151 GOAL 6 hygiene: drop 648 tracked _lane-out debris files (incl. an embedded shadow.git with commit receipts) and gitignore the dir; genericize estate paths in docs/, licenses/, tools/gguf-retag-tq3
- `a4a87f1f9` lane-151 GOAL 9: reserve tq3's three destination-less serialized ids (37 TQ3_4SV -> 52, 31 TQ3_1S_AP1 -> 53, 44 TQ3_1S -> 54) as RESERVED-not-implemented in docs/TYPE-ID-ALLOCATION.md 3.1.2; geometry marked NOT DERIVED, no enumerators added, retag-tool refusals unchanged
- `7a0ab91dc` lane-151 GOALs 6+7: README refreshed to b10453 canonical arifi-main + the seven-ingested-sources table (two honest zeroes named); tq3 and ciru credits added; licenses/README.md gains the full Taken-from audit incl. the tq3 license GAP; UPDATE-RUNBOOK.md written (O(delta) recipe, THREE-flag configure, guards, the standard)
- `485fe7c59` lane-151 GOAL 2: sources.json base.ref -> arifi/main (the canonical branch is now the series base) + series regen over the full canonical range: 323 patches, series check PASS, replay onto 4df29be4f identical outside patches/series
- `e6b1bd94c` lane-151 GOAL 5: gitignore .tokensave/ - the fork repo now carries its own code graph (63344 nodes, 154451 edges over 3407 files) and the index is per-machine, never committed
- `913ac83c9` lane-151 CHECKER ROOT FIX: series check now asserts every patch is a COMMITTED BLOB, not just a file on disk. The byte-compare ran against the working directory, so untracked patches made it green while a fresh clone would die at git am - the defect that hid 33 missing patches twice. Help text corrected to match what the code does
- `d24d3bc5c` lane-151 CHECKER FIXES (docs): UPDATE-RUNBOOK 4.5 no longer asserts a self-containment standard nothing on this estate meets - the four MinGW runtime DLLs are named and staged, and the measured stripped-PATH failure is recorded as identical for the raw build tree and the previous engine. licenses/README.md audit header stops claiming a purely mechanical trailer derivation: the tq3 row is a SUBJECT-line citation git never parses as a trailer, which is exactly why a trailer-only sweep would have missed the one real license gap
- `e5145724e` lane-151 HQ DIRECTIVE: RETAIN the licenses instead of flagging them. tq3-MIT, prisml-MIT and ciru-MIT extracted verbatim from this repo's OWN fetched remotes (git show refs/remotes/<r>/<head>:LICENSE). REFUTES my own GOAL-6 finding and a year-old one: I wrote 'no clone remains on the estate' and Phase 0 wrote 'PrismML has no LICENSE' - both were absence claims checked against the wrong artifact while the remotes sat fetched in this very repository. All seven registered sources audited at once. thecodacus STAYS open: no remote, no text, nothing invented
- `b62a2a324` licenses: thecodacus-MIT retained from the public GitHub fork - the last open attribution CLOSED (no local remote != no source; President's correction 2026-08-18)
- `b68aba7a2` lane-152 GOAL 1: upstream bump b10453 -> b10481, the FIRST live execution of UPDATE-RUNBOOK 2. 348 patches replayed onto 25ae3a9b3, 4 conflicts resolved by hand, series regenerated to 329, zero merges.
- `e958c8d1d` lane-152: what the FIRST live run of UPDATE-RUNBOOK section 2 corrected - one build break, two arifi_sync defects, the runbook itself, and the last license row
- `34732cace` lane-152 GOAL 2 batch 1: ciru's Vulkan SPIRV-Headers fallback, tq3's f32 hybrid-SSM state gates, rocmfpx's -ffast-math guard
- `0258abc2d` lane-152 GOAL 2 batch 2: three lfm2 pre-tokenizer hashes that only rocmfpx had - and the reason they must be pre-computed
- `7beafc62b` lane-152 pins: THREE advance with reviews (upstream, rocmfpx, tq3), TWO deliberately stay BEHIND (prisml, ciru)
- `613d58c6f` lane-152 checker F7: the old base is tag b10454, not b10453 - correction of record
- `9a9e9a919` repo-env.json (WI-1540 tail): the engine repo's env manifest - PowerShell-only law (F-107), three-flag configure (F-111), cmd.exe redirection (F-108), type-id FLAG E, series discipline, runbook pointer
- `cab3925b0` WI-1580 sub-2: UPDATE-RUNBOOK section 7 - the 4.5 line was stale. lane-153 staged models/engines/arifi-b10481-union/ from a verified build of canonical, smoked from the staged copies, and flipped current-state.json + artifact-catalog.json, so 4.5 HAS now been executed once, by hand. The bump distinction is KEPT because it is still true: no bump has ever driven staging end to end (lane-152's bump refused at leg 2). Narrow edit - the section-2 and section-3 paragraphs around it are untouched. Note for the record: the lane-153 REPORT cites this text at UPDATE-RUNBOOK.md:238-244; it actually lives at :308-323
- `bb7809f26` graph: derive the V-unpad head count from the attention output tensor, not from hparams
- `3eedb88f1` sources: advance the turboquant pin f6124e914 -> 7ebcbb0b6 with pin_review_2026_08_18_lane154
- `9c10c5aff` off-rig backends: land the 12 CUDA/Metal/SYCL/HIP rows unbuilt, per the HQ ruling of 2026-08-18
- `209145e48` sources: pin_review names the 12 off-rig shas and their BUILD-UNPROVEN status (HQ condition 3)
- `2dd9c0b69` sources: correct the pin_review - the contested 12/12 apply-conflict count is no longer the justification (checker F1)
- `c5ad74b1c` sources: pin_review records the apply-count claim as WRONG, not disputed (checker F1, resolved against me)
- `172729a3b` base: b10481/25ae3a9b3 -> b10488/9d77fa172 + series regen 346 (lane-156 upstream bump)
- `92e2f0c01` moe-cache: heat-protected eviction keeps hot experts resident (turboquant tail)
- `c44d68931` tests: initialise non-contiguous tensors row by row (turboquant row f58ee0e97)
- `5e402b5cf` sources: advance the turboquant pin 7ebcbb0b6 -> d14e36827 with a dated lane-156 pin_review
- `0de7a647a` sources: prisml pin HELD deliberately with a sharper reason + the v6 K-cache-mean-center verdict (lane-156 GOAL 4)
- `afeaf5f1e` sources: ciru re-derived 315 -> 15 and HELD with all 15 reviewed (lane-156 GOAL 5)
- `65835bf99` sources: advance remotes.upstream.pin 25ae3a9b3 -> 9d77fa172 (b10488) with its review

