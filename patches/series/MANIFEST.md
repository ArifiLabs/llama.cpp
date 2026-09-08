# ArifiLabs patch-series MANIFEST

<!-- GENERATED FILE - DO NOT EDIT BY HAND.
     Regenerate:  python tools/arifi-sync/arifi_sync.py series regen
     Verify:      python tools/arifi-sync/arifi_sync.py series check  -->

## What this is

`patches/series/` is the complete, ordered, replayable definition of everything ArifiLabs
adds to upstream llama.cpp. It is generated from git, never hand-maintained, and an
integrity check refuses to pass if it has drifted from git by so much as a byte.

- Base: `ggml-org/llama.cpp` tag `b10825`, `9e0e220594af405a62835dc3a27495729fd8506b`
- Patches: **505**, all non-merge, applied in filename order.

## Applying the series

```bash
git checkout -b my-rebuild 9e0e22059
git am patches/series/*.patch
```

The result is byte-identical to `master` everywhere outside `patches/series` itself:
same file contents, same 505 commit messages, same provenance trailers. Verified, not
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
git log --oneline 9e0e22059..master -- patches/series
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
| 1 | `0001-chore-establish-ArifiLabs-fork-identity-and-retained.patch` | arifi-fork-base | `d026313af` | ggml-org/llama.cpp@571d0d540df04f25298d0e159e520d9fc62ed121, Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4, charlie12345/ROCmFPX@a6a93765f7ce9779c13f9881164a65f7a9f31198 | - | chore: establish ArifiLabs fork identity and retained notices |
| 2 | `0002-docs-add-options-registry-and-unified-A-F-catalog.patch` | arifi-fork-base | `ba6329558` | ArifiLabs local-inference catalog and trial evidence | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PROF`, `LANE110_PREFETCH_CAP` | docs: add options registry and unified A-F catalog |
| 3 | `0003-ci-add-Windows-Vulkan-and-bench-smoke-placeholders.patch` | arifi-fork-base | `2d7e16ab5` | - | - | ci: add Windows Vulkan and bench-smoke placeholders |
| 4 | `0004-docs-require-loud-fail-upstream-currency-procedure.patch` | arifi-fork-base | `575fbb247` | ggml-org/llama.cpp@571d0d540df04f25298d0e159e520d9fc62ed121, charlie12345/ROCmFPX@a6a93765f7ce9779c13f9881164a65f7a9f31198, PrismML-Eng/llama.cpp@ternary-Q2_0_g128-lineage, Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | docs: require loud-fail upstream currency procedure |
| 5 | `0005-powerinfer-add-MoE-pipeline-fused-sparse-ggml-ops-se.patch` | powerinfer-streaming | `7fa458f85` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: add MoE pipeline + fused-sparse ggml ops [series 0001] |
| 6 | `0006-powerinfer-CPU-kernels-dispatch-for-streamed-MoE-ops.patch` | powerinfer-streaming | `d94f2f220` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: CPU kernels + dispatch for streamed-MoE ops [series 0002] |
| 7 | `0007-powerinfer-vendor-streaming-library-incl.-lane-110-W.patch` | powerinfer-streaming | `e258435da` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED` | powerinfer: vendor streaming library incl. lane-110 Windows port [series 0003] |
| 8 | `0008-build-wire-powerinfer-lib-AVX-spill-cure-into-CMake-.patch` | powerinfer-streaming | `da03505ec` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | build: wire powerinfer lib + AVX-spill cure into CMake [series 0004] |
| 9 | `0009-powerinfer-expert-bundle-loader-GENERATE_EXPERT_BUND.patch` | powerinfer-streaming | `e5b4c84e1` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | powerinfer: expert-bundle loader + GENERATE_EXPERT_BUNDLE [series 0005] |
| 10 | `0010-powerinfer-generic-MoE-streaming-hook-SiLU-op-type-s.patch` | powerinfer-streaming | `782a730b5` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP` | powerinfer: generic MoE streaming hook, SiLU op-type, standalone expert prefetch [series 0006+0009] |
| 11 | `0011-powerinfer-per-ubatch-pipeline-init-guarded-reuse-PR.patch` | powerinfer-streaming | `cfb977c93` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `LANE110_PROF` | powerinfer: per-ubatch pipeline init, guarded reuse, PROF field [series 0007+0010] |
| 12 | `0012-powerinfer-repack-carve-out-for-streamed-tensors-ser.patch` | powerinfer-streaming | `87dd24346` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE` | powerinfer: repack carve-out for streamed tensors [series 0008a] |
| 13 | `0013-powerinfer-down-projection-transpose-fix-series-0008.patch` | powerinfer-streaming | `f55f9ea75` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | - | powerinfer: down-projection transpose fix [series 0008c] |
| 14 | `0014-powerinfer-GPU-safe-staging-CPU-zero-copy-LANE110_PR.patch` | powerinfer-streaming | `a6324c47c` | Tiiny-AI/PowerInfer@8bd56d69906c9d2dba4d3bf6899763401e01a9a4 | `LANE110_PROF` | powerinfer: GPU-safe staging, CPU zero-copy, LANE110_PROF [series 0011] |
| 15 | `0015-lane-110-document-UI-ON-as-local-default-build-varia.patch` | arifi-fork-base | `c6a4defbd` | - | `LLAMA_USE_PREBUILT_UI` | lane-110: document UI-ON as local default build variant (static-runtime cure for ki-webui-0xC0000139) |
| 16 | `0016-community-llama-pin-mmap-backed-CPU-weights-for-fast.patch` | community-prefetch | `6a022922f` | thecodacus/llama.cpp@20f5994bfeb91d24da328077c4b6095998cc9888 | - | community: llama : pin mmap-backed CPU weights for faster H2D uploads |
| 17 | `0017-community-ggml-overlap-offloaded-expert-weight-uploa.patch` | community-prefetch | `cc296d507` | thecodacus/llama.cpp@1163cb34939fe4a9cb07aec034c5954144497ae9 | - | community: ggml : overlap offloaded expert weight uploads with compute |
| 18 | `0018-community-ggml-size-prefetch-slots-per-layer-and-fix.patch` | community-prefetch | `cba2e9538` | thecodacus/llama.cpp@5f83fbbe7c668c59912a1fe09e86a0ef580406c4 | - | community: ggml : size prefetch slots per layer and fix fallback use-after-free |
| 19 | `0019-tools-gguf-add-fail-closed-g128-retagger.patch` | ternary-g128 | `889a25884` | - | - | tools(gguf): add fail-closed g128 retagger |
| 20 | `0020-tools-gguf-retagger-v2-structure-only-parser-no-tens.patch` | ternary-g128 | `b4592e6c3` | - | - | tools(gguf): retagger v2 — structure-only parser, no tensor materialization |
| 21 | `0021-g128-engine-port-applied-BUILD-BLOCKED-on-copy_to_qu.patch` | ternary-g128 | `a3cddd4de` | PrismML-Eng/llama.cpp@984bf9723, PrismML-Eng/llama.cpp@34dc5812c | - | g128 engine port applied — BUILD-BLOCKED on copy_to_quant.comp shader (Sol's fix owed) |
| 22 | `0022-vulkan-fix-f16-promotion-in-q2_0_g128-copy_to_quant.patch` | ternary-g128 | `3b039c683` | - | - | vulkan: fix f16 promotion in q2_0_g128 copy_to_quant |
| 23 | `0023-vulkan-g128-shader-gen-parity-MMQ-helper-completion.patch` | ternary-g128 | `eaaceefa3` | - | - | vulkan: g128 shader-gen parity + MMQ helper completion |
| 24 | `0024-vulkan-g128-DMMV-repack4-byte-oriented-rewrite.patch` | ternary-g128 | `79773a3ee` | - | - | vulkan: g128 DMMV repack4 byte-oriented rewrite |
| 25 | `0025-loader-g128-include-get_next_tensor-ctx-arg.patch` | ternary-g128 | `3aae71a45` | - | - | loader: g128 include + get_next_tensor ctx arg |
| 26 | `0026-loader-use-public-ggml_blck_size-for-g128-drop-inter.patch` | ternary-g128 | `ba767f428` | - | - | loader: use public ggml_blck_size for g128, drop internal include |
| 27 | `0027-tools-gguf-retag-v3-verified-type-id-rewrite-42-43.patch` | ternary-g128 | `015128b82` | - | - | tools(gguf): retag v3 — verified type-id rewrite 42->43 |
| 28 | `0028-loader-accept-native-Q2_0_G128-tensors.patch` | ternary-g128 | `5bd55af20` | - | - | loader: accept native Q2_0_G128 tensors |
| 29 | `0029-ggml-cpu-dispatch-Q2_0_G128-at-the-7-Q2_0-sites.patch` | ternary-g128 | `f569b47cc` | - | - | ggml-cpu: dispatch Q2_0_G128 at the 7 Q2_0 sites |
| 30 | `0030-port-f16-recurrent-S-state-env-gated-GGML_RECURRENT_.patch` | prismml-recurrent | `77f8e7dde` | PrismML-Eng/llama.cpp@3e0855571 | `GGML_RECURRENT_STATE_F16` | port: f16 recurrent S-state (env-gated GGML_RECURRENT_STATE_F16) |
| 31 | `0031-port-M-RoPE-embedded-batch-position-guard.patch` | rocmfpx-mrope | `3c6322b81` | charlie12345/ROCmFPX@rocmfpx-a6a9376 | - | port: M-RoPE embedded-batch position guard |
| 32 | `0032-docs-phase-3a-OPTIONS-REGISTRY-rows-M-RoPE-f16-recur.patch` | arifi-fork-base | `1797838f0` | - | `GGML_RECURRENT_STATE_F16` | docs: phase-3a OPTIONS-REGISTRY rows (M-RoPE, f16-recurrent, DSpark-iGPU) |
| 33 | `0033-port-Windows-IOCP-async-expert-bundle-reads-M2b.patch` | iocp-async | `b117c4856` | - | - | port: Windows IOCP async expert-bundle reads (M2b) |
| 34 | `0034-feat-POWERINFER_IOCP-runtime-toggle-for-the-Windows-.patch` | iocp-async | `a578f05dd` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | feat: POWERINFER_IOCP runtime toggle for the Windows expert-read path |
| 35 | `0035-docs-OPTIONS-REGISTRY-POWERINFER_IOCP-measured-row-s.patch` | iocp-async | `7be86e346` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | docs: OPTIONS-REGISTRY POWERINFER_IOCP measured row (same-binary A/B) |
| 36 | `0036-docs-correct-POWERINFER_IOCP-row-A-B-was-noise-direc.patch` | iocp-async | `953a96219` | - | `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH` | docs: correct POWERINFER_IOCP row - A/B was noise, direction UNRESOLVED |
| 37 | `0037-patches-generate-the-full-auditable-38-patch-series-.patch` | arifi-fork-base | `6827c9462` | ArifiLabs lane-110C fork-maintainability slot | - | patches: generate the full auditable 38-patch series from git |
| 38 | `0038-tools-add-arifi-sync-the-currency-series-and-upstrea.patch` | arifi-fork-base | `4de985763` | ArifiLabs lane-110C fork-maintainability slot | `LLAMA_USE_PREBUILT_UI`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `ARIFI_TOOL_NVFP4_REMAP` | tools: add arifi-sync - the currency, series and upstream-bump driver |
| 39 | `0039-docs-README-section-a-GitHub-visitor-can-read-cold.patch` | arifi-fork-base | `3971660b6` | ArifiLabs lane-110C fork-maintainability slot | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP` | docs: README section a GitHub visitor can read cold |
| 40 | `0040-patches-move-banked-PrismML-source-patches-out-of-th.patch` | arifi-fork-base | `24b2c17c2` | PrismML-Eng/llama.cpp@ternary-Q2_0_g128-lineage | - | patches: move banked PrismML source patches out of the generated series directory |
| 41 | `0041-patches-make-the-series-generator-implement-its-own-.patch` | arifi-fork-base | `57cddcfd3` | - | - | patches: make the series generator implement its own convergence rule |
| 42 | `0042-patches-stop-the-manifest-deriving-anything-from-exc.patch` | arifi-fork-base | `43f38b822` | - | - | patches: stop the manifest deriving anything from excluded commits |
| 43 | `0043-port-Q1_0-repack-scaffolding-Arm-NEON-DP-GEMV-GEMM-k.patch` | arifi-fork-base | `d9f1a2117` | PrismML-Eng/llama.cpp@720e06b16 | - | port: Q1_0 repack scaffolding + Arm NEON/DP GEMV-GEMM kernels |
| 44 | `0044-port-native-x86-Q2_0-vec_dot-AVX512-VNNI-AVX-VNNI-dr.patch` | arifi-fork-base | `38232d775` | PrismML-Eng/llama.cpp@4d88cd4eb, PrismML-Eng/llama.cpp@79697f23a | - | port: native x86 Q2_0 vec_dot (AVX512-VNNI / AVX-VNNI), drop scalar fallback alias |
| 45 | `0045-port-x86-AVX512-VNNI-repack-GEMV-GEMM-for-Q1_0-and-Q.patch` | arifi-fork-base | `a2fa737a6` | PrismML-Eng/llama.cpp@9fcaed763 | - | port: x86 AVX512-VNNI repack GEMV/GEMM for Q1_0 and Q2_0 |
| 46 | `0046-feat-GGML_ARIFI_VNNI_REPACK-runtime-toggle-explicit-.patch` | ternary-g128 | `9bf54af26` | - | `GGML_ARIFI_VNNI_REPACK` | feat: GGML_ARIFI_VNNI_REPACK runtime toggle + explicit g64/g128 tripwire |
| 47 | `0047-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_VNNI_REPACK.patch` | arifi-fork-base | `bde65d3f1` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY row for GGML_ARIFI_VNNI_REPACK - UNMEASURED, artifact-blocked |
| 48 | `0048-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-CORRECT.patch` | arifi-fork-base | `b8060d46b` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - CORRECTNESS FAIL, GEMM returns NaN |
| 49 | `0049-fix-ggml-cpu-parameterize-Q2_0-GEMM-activation-strid.patch` | arifi-fork-base | `56ac77b01` | PrismML-Eng/llama.cpp (Q2_0 4x8 VNNI GEMM design) | - | fix(ggml-cpu): parameterize Q2_0 GEMM activation stride by QK2_0/QK8_0 - g64 NaN |
| 50 | `0050-fix-ggml-cpu-same-g64-stride-defect-in-the-ported-x8.patch` | arifi-fork-base | `a01ed569b` | - | - | fix(ggml-cpu): same g64 stride defect in the ported x86 Q2_0 vec_dot |
| 51 | `0051-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-NaN-FIX.patch` | arifi-fork-base | `81f5fae50` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - NaN FIXED, bit-identity NOT established |
| 52 | `0052-docs-OPTIONS-REGISTRY-GGML_ARIFI_VNNI_REPACK-bit-div.patch` | arifi-fork-base | `ef4792cab` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY GGML_ARIFI_VNNI_REPACK - bit-divergence ADJUDICATED, performance MEASURED |
| 53 | `0053-docs-correct-the-VNNI-row-s-base-commit-labelling-f7.patch` | arifi-fork-base | `7fd82d5f0` | - | `GGML_ARIFI_VNNI_REPACK` | docs: correct the VNNI row's base-commit labelling - f7c4d9d2c is NOT master |
| 54 | `0054-docs-fix-two-stray-table-breaking-pipes-in-the-VNNI-.patch` | arifi-fork-base | `d9862e98e` | - | `GGML_ARIFI_VNNI_REPACK` | docs: fix two stray table-breaking pipes in the VNNI registry row |
| 55 | `0055-docs-OPTIONS-REGISTRY-VNNI-row-topology-cell-now-ref.patch` | arifi-fork-base | `a6890ff47` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY VNNI row - topology cell now reflects the linear-master rebase |
| 56 | `0056-ggml-cpu-GGML_ARIFI_VNNI_REPACK-defaults-to-OFF-dive.patch` | ternary-g128 | `43f2d29b1` | - | `GGML_ARIFI_VNNI_REPACK` | ggml-cpu: GGML_ARIFI_VNNI_REPACK defaults to OFF (diverging from upstream, on measurement) |
| 57 | `0057-docs-fork-wide-GGUF-type-ID-allocation-contract-keys.patch` | arifi-fork-base | `ab831f49b` | - | `ARIFI_TOOL_NVFP4_REMAP` | docs: fork-wide GGUF type-ID allocation contract (keystone, unblocks 4 slots) |
| 58 | `0058-ggml-reserve-the-ArifiLabs-type-ID-blocks-at-the-enu.patch` | arifi-fork-base | `d1aed38fc` | - | - | ggml: reserve the ArifiLabs type-ID blocks at the enum itself |
| 59 | `0059-ggml-ROCmFP4-ROCmFPX-weight-formats-behind-GGML_ARIF.patch` | arifi-fork-base | `667ee75c4` | charlie12345/ROCmFPX@3edc3d31e | - | ggml: ROCmFP4/ROCmFPX weight formats behind GGML_ARIFI_ROCMFPX_FORMATS |
| 60 | `0060-llama-ROCmFPX-file-types-quantizer-wiring-and-the-gg.patch` | arifi-fork-base | `2c27c91a9` | charlie12345/ROCmFPX@3edc3d31e | - | llama: ROCmFPX file types, quantizer wiring and the gguf-py registry |
| 61 | `0061-docs-OPTIONS-REGISTRY-row-for-GGML_ARIFI_ROCMFPX_FOR.patch` | arifi-fork-base | `cd7869d35` | - | `GGML_ARIFI_VNNI_REPACK` | docs: OPTIONS-REGISTRY row for GGML_ARIFI_ROCMFPX_FORMATS |
| 62 | `0062-tools-record-GGML_ARIFI_ROCMFPX_FORMATS-in-the-build.patch` | arifi-fork-base | `392a34232` | - | - | tools: record GGML_ARIFI_ROCMFPX_FORMATS in the build-recipe snapshot |
| 63 | `0063-docs-make-the-fork-buildable-by-someone-who-did-not-.patch` | arifi-fork-base | `01ad15f29` | - | `LLAMA_USE_PREBUILT_UI`, `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `GGML_RECURRENT_STATE_F16` | docs: make the fork buildable by someone who did not build it |
| 64 | `0064-feat-arifi-profile-and-a-CPU-only-VNNI-repack-startu.patch` | arifi-fork-base | `65bd34a31` | - | `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP`, `MAX_N_CACHED`, `GGML_RECURRENT_STATE_F16` | feat: --arifi-profile and a CPU-only VNNI-repack startup advisory |
| 65 | `0065-sync-bump-base-to-upstream-b10173-e9fa0781f-series-r.patch` | ternary-g128 | `39940ceb6` | upstream b10173 e9fa0781f | `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `LANE110_PROF`, `LANE110_PREFETCH_CAP`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | sync: bump base to upstream b10173 (e9fa0781f) — series replayed (1 hand-resolved conflict: loader load-mode refactor e6dd0e29a), judge harness wired + exercised (F-085 llama-server, g128 ternary 27B PASS), pins updated, series regenerated |
| 66 | `0066-sync-currency-GREEN-TurboQuant-checkouts-pinned-at-t.patch` | arifi-fork-base | `8c51d177c` | - | - | sync: currency GREEN — TurboQuant checkouts pinned at their lane-110C-reviewed shas (c26cbdffc, ba52ad107), rocmfpx pin advanced to db6844d9b after 6-commit review (HIP pinned-staging noted for the HIP-deltas slot) |
| 67 | `0067-fix-ggml-cpu-x86-don-t-pass-__m256i-by-value-across-.patch` | arifi-fork-base | `c16526b3b` | - | - | fix(ggml-cpu/x86): don't pass __m256i by value across the repack GEMV/GEMM ABI boundary (Windows 0xC0000005) |
| 68 | `0068-ggml-TurboQuant-TQ3_1S-TQ4_1S-weight-formats-behind-.patch` | arifi-fork-base | `3d9668cdc` | - | - | ggml: TurboQuant TQ3_1S/TQ4_1S weight formats behind GGML_ARIFI_TURBO_WEIGHT_QUANTS |
| 69 | `0069-docs-CHANGELOG-and-FINDINGS-the-public-trail-a-stran.patch` | arifi-fork-base | `5e84221f0` | - | `GGML_ARIFI_VNNI_REPACK`, `POWERINFER_IOCP` | docs: CHANGELOG and FINDINGS - the public trail a stranger can actually read |
| 70 | `0070-docs-USAGE-a-task-oriented-guide-for-humans-not-an-o.patch` | arifi-fork-base | `a10d81dd4` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `MAX_N_CACHED`, `POWERINFER_IOCP` | docs: USAGE - a task-oriented guide for humans, not an option reference |
| 71 | `0071-ggml-cpu-x86-use-the-row-stride-parameter-in-repack-.patch` | arifi-fork-base | `c77c324ab` | - | - | ggml-cpu/x86: use the row-stride parameter in repack GEMV stores instead of the row count |
| 72 | `0072-ggml-cpu-fuse-the-TurboQuant-dot-product-transform-t.patch` | ternary-g128 | `8f7895381` | - | - | ggml-cpu: fuse the TurboQuant dot product - transform the activation, not the weights |
| 73 | `0073-docs-FINDINGS-F-09-the-repack-GPU-trade-off-is-a-com.patch` | arifi-fork-base | `b6a36788e` | - | - | docs(FINDINGS): F-09 - the repack/GPU trade-off is a compute-placement choice, not a bug |
| 74 | `0074-docs-neutralise-internal-governance-vocabulary-expla.patch` | arifi-fork-base | `3d1b1ac45` | - | - | docs: neutralise internal governance vocabulary; explain the work-unit ids instead |
| 75 | `0075-docs-FINDINGS-record-why-the-repack-auto-enable-was-.patch` | arifi-fork-base | `38096996f` | - | - | docs(FINDINGS): record why the repack auto-enable was NOT shipped |
| 76 | `0076-provenance-catch-the-silent-trailer-block-break-inst.patch` | arifi-fork-base | `68e47b9fc` | - | - | provenance: catch the silent trailer-block break instead of only reporting it |
| 77 | `0077-gitattributes-pin-.githooks-to-LF-so-the-hook-runs-o.patch` | arifi-fork-base | `9fd35df85` | - | - | gitattributes: pin .githooks to LF so the hook runs on Linux and macOS |
| 78 | `0078-githooks-force-a-normalized-blob-for-the-commit-msg-.patch` | arifi-fork-base | `009a8da6c` | - | - | githooks: force a normalized blob for the commit-msg hook |
| 79 | `0079-docs-FINDINGS-F-09-exit-1-is-the-direction-dual-resi.patch` | arifi-fork-base | `db6df2b50` | - | - | docs(FINDINGS): F-09 exit 1 is the direction - dual residency, not a forced choice |
| 80 | `0080-repack-dual-residency-GGML_ARIFI_VNNI_REPACK-2-keeps.patch` | arifi-fork-base | `46aca7c0c` | - | `GGML_ARIFI_VNNI_REPACK` | repack: dual residency - GGML_ARIFI_VNNI_REPACK=2 keeps prefill on the GPU AND repacks decode |
| 81 | `0081-repack-dual-residency-measured-both-wins-held-fix-th.patch` | arifi-fork-base | `eae07ca1c` | - | `GGML_ARIFI_VNNI_REPACK` | repack: dual residency measured - both wins held; fix the hot-path cost that ate the first one |
| 82 | `0082-repack-fix-the-shadow-tensor-COUNT-in-the-accounting.patch` | arifi-fork-base | `222946b67` | - | `GGML_ARIFI_VNNI_REPACK` | repack: fix the shadow tensor COUNT in the accounting line, and strengthen the claim it supports |
| 83 | `0083-repack-CPU-kernels-for-the-128-group-ternary-format.patch` | arifi-fork-base | `50132f88c` | - | `GGML_ARIFI_VNNI_REPACK` | repack: CPU kernels for the 128-group ternary format |
| 84 | `0084-tests-direct-equivalence-test-for-the-ternary-repack.patch` | arifi-fork-base | `be8dc6239` | - | - | tests: direct equivalence test for the ternary repack kernels |
| 85 | `0085-docs-the-g128-repack-SPEED-numbers-measured.patch` | arifi-fork-base | `ac294e8f1` | - | `GGML_ARIFI_VNNI_REPACK` | docs: the g128 repack SPEED numbers, measured |
| 86 | `0086-docs-frame-the-g128-repack-numbers-as-a-CPU-tier-res.patch` | arifi-fork-base | `c264a72ca` | - | - | docs: frame the g128 repack numbers as a CPU-tier result |
| 87 | `0087-docs-publish-the-power-plan-finding-as-F-12-and-bind.patch` | arifi-fork-base | `a6056fb66` | - | - | docs: publish the power-plan finding as F-12, and bind the bench protocol to it |
| 88 | `0088-F-06-measure-the-GPU-tier-on-one-binary-and-count-th.patch` | arifi-fork-base | `f932903cd` | - | - | F-06: measure the GPU tier on one binary, and count the placement the prompt claim rested on |
| 89 | `0089-registry-the-dual-residency-prompt-mechanism-is-meas.patch` | arifi-fork-base | `8d9bb0275` | - | `GGML_ARIFI_VNNI_REPACK` | registry: the dual-residency prompt mechanism is measured now, not ASSUMED |
| 90 | `0090-powerinfer-MAX_N_CACHED-admitted-two-values-that-bre.patch` | powerinfer-streaming | `5bb165467` | - | `MAX_N_CACHED` | powerinfer: MAX_N_CACHED admitted two values that break the eviction invariant |
| 91 | `0091-rows-per-workgroup-on-g128-a-null-and-F-10-confirmed.patch` | ternary-g128 | `edcf89be5` | - | - | rows-per-workgroup on g128: a null, and F-10 confirmed on a second file |
| 92 | `0092-registry-our-DSpark-verdict-was-never-ours-the-draft.patch` | arifi-fork-base | `69472e927` | - | - | registry: our DSpark verdict was never ours -- the drafter cannot load on this fork |
| 93 | `0093-speculative-DFlash-DSpark-drafters-have-no-vocab-tak.patch` | arifi-fork-base | `3d5aaa619` | - | - | speculative: DFlash/DSpark drafters have no vocab -- take the mask token from metadata |
| 94 | `0094-expert-cache-stride-the-bundle-by-the-PADDED-matrix-.patch` | arifi-fork-base | `3e9c41c50` | - | - | expert cache: stride the bundle by the PADDED matrix size -- unblocks our own brain |
| 95 | `0095-powerinfer-POWERINFER_NO_BUFFERING-1-the-Windows-cou.patch` | powerinfer-streaming | `20332a0ef` | - | `POWERINFER_IOCP` | powerinfer: POWERINFER_NO_BUFFERING=1 -- the Windows counterpart of the O_DIRECT Linux already uses |
| 96 | `0096-expert-cache-POWERINFER_EXPERT_HEATMAP-measure-acces.patch` | arifi-fork-base | `4191dcb9e` | - | - | expert cache: POWERINFER_EXPERT_HEATMAP -- measure access skew before building a pin policy |
| 97 | `0097-ours-1163cb34939fe4a9cb07aec034c5954144497ae9.patch.patch` | arifi-fork-base | `b7a23f113` | - | - | ours/1163cb34939fe4a9cb07aec034c5954144497ae9.patch |
| 98 | `0098-1-core-types-0001-WIP-add-TurboQuant-KV-cache-types-.patch` | arifi-fork-base | `94342d355` | - | - | 1-core-types/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 99 | `0099-1-core-types-0002-ggml-port-TurboQuant-core-quant-ty.patch` | arifi-fork-base | `b44ed2c15` | - | - | 1-core-types/0002-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 100 | `0100-2-cpu-reference-0001-ggml-port-TurboQuant-core-quant.patch` | arifi-fork-base | `73f446772` | - | - | 2-cpu-reference/0001-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 101 | `0101-2-cpu-reference-0007-ggml-cpu-declare-turbo3_cpu_wht.patch` | arifi-fork-base | `b3bbb9bfa` | - | - | 2-cpu-reference/0007-ggml-cpu-declare-turbo3_cpu_wht_group_size-extern-no.patch |
| 102 | `0102-2-cpu-reference-0008-merge-fix-post-merge-build-and-.patch` | arifi-fork-base | `589a7c403` | - | - | 2-cpu-reference/0008-merge-fix-post-merge-build-and-crash-issues.patch |
| 103 | `0103-3-vulkan-0001-vulkan-metal-hip-add-TurboQuant-kernel.patch` | arifi-fork-base | `272997fe5` | - | - | 3-vulkan/0001-vulkan-metal-hip-add-TurboQuant-kernel-support.patch |
| 104 | `0104-3-vulkan-0002-vulkan-port-fork-241-wave64-ballot-fix.patch` | arifi-fork-base | `e55c1c656` | - | - | 3-vulkan/0002-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch |
| 105 | `0105-3-vulkan-0003-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `9bcb0e98b` | - | - | 3-vulkan/0003-fix-close-complete-audit-findings-7-port-regressions.patch |
| 106 | `0106-3-vulkan-0004-vulkan-reconstruct-supports_op-after-r.patch` | arifi-fork-base | `299db0849` | - | - | 3-vulkan/0004-vulkan-reconstruct-supports_op-after-rebase-merge-da.patch |
| 107 | `0107-3-vulkan-0006-vulkan-restore-TQ4_1S-weight-type-wiri.patch` | arifi-fork-base | `a03716427` | - | - | 3-vulkan/0006-vulkan-restore-TQ4_1S-weight-type-wiring-lost-in-the.patch |
| 108 | `0108-3-vulkan-0007-vulkan-add-the-TQ3_1S-weight-type.patc.patch` | arifi-fork-base | `f8543b7cc` | - | - | 3-vulkan/0007-vulkan-add-the-TQ3_1S-weight-type.patch |
| 109 | `0109-3-vulkan-0008-vulkan-reject-MUL_MAT_ID-for-the-Turbo.patch` | arifi-fork-base | `71ffdc04e` | - | - | 3-vulkan/0008-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch |
| 110 | `0110-3-vulkan-0013-vulkan-TQ-rotated-matmul-shader-ground.patch` | arifi-fork-base | `9dd1c2bbb` | - | - | 3-vulkan/0013-vulkan-TQ-rotated-matmul-shader-groundwork-A-side-lo.patch |
| 111 | `0111-3-vulkan-0015-vulkan-create-the-TQ-activation-rotate.patch` | arifi-fork-base | `b9dc4aa27` | - | - | 3-vulkan/0015-vulkan-create-the-TQ-activation-rotate-pipeline.patch |
| 112 | `0112-3-vulkan-0017-vulkan-rotate-the-staged-activation-co.patch` | arifi-fork-base | `ea05afd41` | - | - | 3-vulkan/0017-vulkan-rotate-the-staged-activation-copy-in-place-dr.patch |
| 113 | `0113-3-vulkan-0018-vulkan-register-the-TQ-rotated-mul_mm_.patch` | arifi-fork-base | `d07741312` | - | - | 3-vulkan/0018-vulkan-register-the-TQ-rotated-mul_mm_id-pipelines.patch |
| 114 | `0114-3-vulkan-0021-vulkan-fix-SET_ROWS-block-decompositio.patch` | arifi-fork-base | `832c023e9` | - | - | 3-vulkan/0021-vulkan-fix-SET_ROWS-block-decomposition-for-turbo-we.patch |
| 115 | `0115-4-other-backends-0006-laguna-MoE-down-proj-f16-overf.patch` | arifi-fork-base | `03c278de6` | - | - | 4-other-backends/0006-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch |
| 116 | `0116-4-other-backends-0007-cuda-first-class-MoE-path-plai.patch` | arifi-fork-base | `310ae9400` | - | - | 4-other-backends/0007-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch |
| 117 | `0117-4-other-backends-0008-hip-add-mixed-f16-bf16-q8_0-fa.patch` | arifi-fork-base | `d88e8cb6b` | - | - | 4-other-backends/0008-hip-add-mixed-f16-bf16-q8_0-fattn-vec-instances-to-t.patch |
| 118 | `0118-4-other-backends-0009-cuda-revert-GQA-ratio-dispatch.patch` | arifi-fork-base | `dbcf8548b` | - | - | 4-other-backends/0009-cuda-revert-GQA-ratio-dispatch-to-comparison-form-fi.patch |
| 119 | `0119-4-other-backends-0010-cuda-restore-Volta-GQA-modulo-.patch` | arifi-fork-base | `87df12365` | - | - | 4-other-backends/0010-cuda-restore-Volta-GQA-modulo-dispatch-over-revert-i.patch |
| 120 | `0120-4-other-backends-0015-cuda-rework-MoE-expert-cache-e.patch` | arifi-fork-base | `5f5e1ab00` | - | - | 4-other-backends/0015-cuda-rework-MoE-expert-cache-execution.patch |
| 121 | `0121-4-other-backends-0016-cuda-fix-MoE-cache-capacity-ac.patch` | arifi-fork-base | `809460ff4` | - | - | 4-other-backends/0016-cuda-fix-MoE-cache-capacity-accounting.patch |
| 122 | `0122-4-other-backends-0017-cuda-fix-MoE-cache-scratch-bud.patch` | arifi-fork-base | `21d90bf23` | - | - | 4-other-backends/0017-cuda-fix-MoE-cache-scratch-budget-replacement.patch |
| 123 | `0123-4-other-backends-0018-cuda-bound-MoE-MMV-tail-row-re.patch` | arifi-fork-base | `5864d15fa` | - | - | 4-other-backends/0018-cuda-bound-MoE-MMV-tail-row-reads.patch |
| 124 | `0124-4-other-backends-0019-cuda-tune-MoE-cache-CPU-overla.patch` | arifi-fork-base | `c147d10b3` | - | - | 4-other-backends/0019-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 125 | `0125-4-other-backends-0020-cuda-adapt-MoE-cache-admission.patch` | arifi-fork-base | `08939b5d4` | - | - | 4-other-backends/0020-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 126 | `0126-4-other-backends-0021-cuda-align-MoE-cache-pool-allo.patch` | arifi-fork-base | `6557fa761` | - | - | 4-other-backends/0021-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 127 | `0127-4-other-backends-0022-cuda-prefer-generic-MMV-for-co.patch` | arifi-fork-base | `bc361c40f` | - | - | 4-other-backends/0022-cuda-prefer-generic-MMV-for-compatible-MoE-cache-nod.patch |
| 128 | `0128-4-other-backends-0023-cuda-parallelize-MoE-cache-fil.patch` | arifi-fork-base | `45ab61bfb` | - | - | 4-other-backends/0023-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch |
| 129 | `0129-4-other-backends-0024-cuda-fuse-cached-MoE-SwiGLU-ro.patch` | arifi-fork-base | `fb5b90a3c` | - | - | 4-other-backends/0024-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 130 | `0130-4-other-backends-0025-cuda-aggregate-small-MoE-tenso.patch` | arifi-fork-base | `332a072ef` | - | - | 4-other-backends/0025-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 131 | `0131-4-other-backends-0026-cuda-bound-automatic-MoE-cache.patch` | arifi-fork-base | `9c317c8ba` | - | - | 4-other-backends/0026-cuda-bound-automatic-MoE-cache-admission.patch |
| 132 | `0132-4-other-backends-0027-cuda-reject-undersized-automat.patch` | arifi-fork-base | `0913e1e2a` | - | - | 4-other-backends/0027-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 133 | `0133-4-other-backends-0028-cuda-enable-multi-token-MoE-ca.patch` | arifi-fork-base | `b8b367e35` | - | - | 4-other-backends/0028-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 134 | `0134-4-other-backends-0029-common-surface-MoE-cache-activ.patch` | arifi-fork-base | `eb626764a` | - | - | 4-other-backends/0029-common-surface-MoE-cache-activation.patch |
| 135 | `0135-4-other-backends-0030-cuda-accelerate-complete-MoE-c.patch` | arifi-fork-base | `d85aad250` | - | - | 4-other-backends/0030-cuda-accelerate-complete-MoE-cache-pools.patch |
| 136 | `0136-4-other-backends-0031-cuda-report-oversized-MoE-cach.patch` | arifi-fork-base | `e82403113` | - | - | 4-other-backends/0031-cuda-report-oversized-MoE-cache-nodes.patch |
| 137 | `0137-4-other-backends-0032-cuda-clarify-MoE-cache-session.patch` | arifi-fork-base | `01ffa9432` | - | - | 4-other-backends/0032-cuda-clarify-MoE-cache-session-diagnostics.patch |
| 138 | `0138-4-other-backends-0033-cuda-report-MoE-cache-pair-res.patch` | arifi-fork-base | `46feff51d` | - | - | 4-other-backends/0033-cuda-report-MoE-cache-pair-residency.patch |
| 139 | `0139-4-other-backends-0034-cuda-pack-MoE-cache-dispatch-i.patch` | arifi-fork-base | `f4378080d` | - | - | 4-other-backends/0034-cuda-pack-MoE-cache-dispatch-inputs.patch |
| 140 | `0140-4-other-backends-0037-metal-port-TQ_NO_ROTATE-escape.patch` | arifi-fork-base | `533945073` | - | - | 4-other-backends/0037-metal-port-TQ_NO_ROTATE-escape-hatch-from-stash-opt-.patch |
| 141 | `0141-4-other-backends-0039-moe-cache-enable-HIP-backend-b.patch` | arifi-fork-base | `015dceb55` | - | - | 4-other-backends/0039-moe-cache-enable-HIP-backend-by-removing-no-op-stubs.patch |
| 142 | `0142-4-other-backends-0040-moe-cache-count-dispatch-conte.patch` | arifi-fork-base | `04c4b6861` | - | - | 4-other-backends/0040-moe-cache-count-dispatch-contention-bypasses-P1.patch |
| 143 | `0143-4-other-backends-0041-moe-cache-provider-registry-wi.patch` | arifi-fork-base | `427a576f7` | - | - | 4-other-backends/0041-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 144 | `0144-4-other-backends-0042-moe-cache-audit-fixes-H1-F1-F2.patch` | arifi-fork-base | `bd09adac9` | - | - | 4-other-backends/0042-moe-cache-audit-fixes-H1-F1-F2-A1.patch |
| 145 | `0145-4-other-backends-0043-moe-cache-fix-registry-build-I.patch` | arifi-fork-base | `9eeea2669` | - | - | 4-other-backends/0043-moe-cache-fix-registry-build-I1-follow-up.patch |
| 146 | `0146-4-other-backends-0044-moe-cache-allow-automatic-mode.patch` | arifi-fork-base | `0ba03a42c` | - | - | 4-other-backends/0044-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 147 | `0147-4-other-backends-0045-cuda-fix-HIP-MoE-cache-compati.patch` | arifi-fork-base | `c9c6cf39a` | - | - | 4-other-backends/0045-cuda-fix-HIP-MoE-cache-compatibility-and-q8_1-sums.patch |
| 148 | `0148-4-other-backends-0046-moe-cache-add-logging-at-silen.patch` | arifi-fork-base | `847374ac3` | - | - | 4-other-backends/0046-moe-cache-add-logging-at-silent-failure-points-acros.patch |
| 149 | `0149-4-other-backends-0048-cuda-fit-fix-two-Werror-build-.patch` | arifi-fork-base | `210a1bcb1` | - | - | 4-other-backends/0048-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch |
| 150 | `0150-4-other-backends-0049-sycl-drop-the-duplicated-nthre.patch` | arifi-fork-base | `224612665` | - | - | 4-other-backends/0049-sycl-drop-the-duplicated-nthreads-declarations-in-fa.patch |
| 151 | `0151-4-other-backends-0050-fattn-vec-split-the-turbo-K-do.patch` | arifi-fork-base | `28d179679` | - | - | 4-other-backends/0050-fattn-vec-split-the-turbo-K-dot-at-D-128-to-stop-the.patch |
| 152 | `0152-5-models-server-0001-WIP-add-TurboQuant-KV-cache-typ.patch` | arifi-fork-base | `60d681354` | - | - | 5-models-server/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 153 | `0153-5-models-server-0003-cli-break-interactive-loop-on-s.patch` | arifi-fork-base | `260a9ed02` | - | - | 5-models-server/0003-cli-break-interactive-loop-on-stdin-EOF-fixes-hot-sp.patch |
| 154 | `0154-5-models-server-0004-tools-expose-TQ3_1S-TQ4_1S-in-q.patch` | arifi-fork-base | `9f80737fb` | - | - | 5-models-server/0004-tools-expose-TQ3_1S-TQ4_1S-in-quantize-table-and-lla.patch |
| 155 | `0155-5-models-server-0007-llama-support-DeepSeek-V4-tenso.patch` | arifi-fork-base | `08ee85906` | - | - | 5-models-server/0007-llama-support-DeepSeek-V4-tensor-split.patch |
| 156 | `0156-5-models-server-0008-llama-mirror-DS4-q_a-kv-down-pr.patch` | arifi-fork-base | `46560bd09` | - | - | 5-models-server/0008-llama-mirror-DS4-q_a-kv-down-projections-in-tensor-s.patch |
| 157 | `0157-5-models-server-0009-glm-dsa-guard-lightning-indexer.patch` | arifi-fork-base | `b276a9b1c` | - | - | 5-models-server/0009-glm-dsa-guard-lightning-indexer-Hadamard-rotation-wh.patch |
| 158 | `0158-5-models-server-0010-laguna-add-arch-tables-llm_arch.patch` | arifi-fork-base | `7b53e6bce` | - | - | 5-models-server/0010-laguna-add-arch-tables-llm_arch-KV-keys-tensors-and-.patch |
| 159 | `0159-5-models-server-0011-laguna-add-model-class-hparams-.patch` | arifi-fork-base | `7404be191` | - | - | 5-models-server/0011-laguna-add-model-class-hparams-wiring-vocab-pre-toke.patch |
| 160 | `0160-5-models-server-0013-laguna-MoE-down-proj-f16-overfl.patch` | arifi-fork-base | `a4bd003d3` | - | - | 5-models-server/0013-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch |
| 161 | `0161-5-models-server-0014-cuda-first-class-MoE-path-plain.patch` | arifi-fork-base | `07020aa30` | - | - | 5-models-server/0014-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch |
| 162 | `0162-5-models-server-0015-cuda-sum-MoE-expert-outputs-on-.patch` | arifi-fork-base | `44f2b42b7` | - | - | 5-models-server/0015-cuda-sum-MoE-expert-outputs-on-decode-n_tokens-1.patch |
| 163 | `0163-5-models-server-0016-laguna-deduplicate-definitions-.patch` | arifi-fork-base | `cafe4dd4a` | - | - | 5-models-server/0016-laguna-deduplicate-definitions-vs-the-early-snapshot.patch |
| 164 | `0164-5-models-server-0017-dflash-fix-DSpark-tensor-meta-a.patch` | arifi-fork-base | `94ac9ae9f` | - | - | 5-models-server/0017-dflash-fix-DSpark-tensor-meta-accessor-after-laguna-.patch |
| 165 | `0165-5-models-server-0021-common-support-the-DSpark-sidec.patch` | arifi-fork-base | `905841018` | - | - | 5-models-server/0021-common-support-the-DSpark-sidecar-resolution-26458.patch |
| 166 | `0166-5-models-server-0037-merge-fix-post-merge-build-and-.patch` | arifi-fork-base | `a3788c9b5` | - | - | 5-models-server/0037-merge-fix-post-merge-build-and-crash-issues.patch |
| 167 | `0167-5-models-server-0041-moe-cache-route-fit-probing-and.patch` | arifi-fork-base | `c5dfc46e2` | - | - | 5-models-server/0041-moe-cache-route-fit-probing-and-test-session-calls-t.patch |
| 168 | `0168-5-models-server-0048-llama-bench-document-pw-prefetc.patch` | arifi-fork-base | `6a83305b8` | - | - | 5-models-server/0048-llama-bench-document-pw-prefetch-weights-in-help-and.patch |
| 169 | `0169-5-models-server-0050-llama-bench-remove-stale-prefet.patch` | arifi-fork-base | `23097bb01` | - | - | 5-models-server/0050-llama-bench-remove-stale-prefetch-weights-documentat.patch |
| 170 | `0170-5-models-server-0051-moe-cache-fix-auto-asymmetric-M.patch` | arifi-fork-base | `8f14a5bf8` | - | - | 5-models-server/0051-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch |
| 171 | `0171-6-tests-build-0001-tests-re-add-test-turbo-quant.c-r.patch` | arifi-fork-base | `b2d9654bf` | - | - | 6-tests-build/0001-tests-re-add-test-turbo-quant.c-round-trip-test.patch |
| 172 | `0172-6-tests-build-0002-tests-port-fork-turbo-backend-and.patch` | arifi-fork-base | `61dfa65d9` | - | - | 6-tests-build/0002-tests-port-fork-turbo-backend-and-quantize-fns-test-.patch |
| 173 | `0173-6-tests-build-0003-fix-close-complete-audit-findings.patch` | arifi-fork-base | `a96866f7e` | - | - | 6-tests-build/0003-fix-close-complete-audit-findings-7-port-regressions.patch |
| 174 | `0174-6-tests-build-0004-laguna-tool-call-whitespace-toler.patch` | arifi-fork-base | `b7413eca0` | - | - | 6-tests-build/0004-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch |
| 175 | `0175-6-tests-build-0006-ggml-webgpu-add-support-for-f16-r.patch` | arifi-fork-base | `c3dc63f66` | - | - | 6-tests-build/0006-ggml-webgpu-add-support-for-f16-repeat-26307.patch |
| 176 | `0176-6-tests-build-0010-tests-restore-DSV4_HC_COMB-eps-sw.patch` | arifi-fork-base | `01f443747` | - | - | 6-tests-build/0010-tests-restore-DSV4_HC_COMB-eps-sweep-lost-in-the-244.patch |
| 177 | `0177-6-tests-build-0011-tests-cast-float-printf-args-in-t.patch` | arifi-fork-base | `9beb5eac6` | - | - | 6-tests-build/0011-tests-cast-float-printf-args-in-test-turbo-quant-arm.patch |
| 178 | `0178-6-tests-build-0012-vulkan-add-the-TQ3_1S-weight-type.patch` | arifi-fork-base | `45ba6c62c` | - | - | 6-tests-build/0012-vulkan-add-the-TQ3_1S-weight-type.patch |
| 179 | `0179-6-tests-build-0013-vulkan-reject-MUL_MAT_ID-for-the-.patch` | arifi-fork-base | `937a37fcd` | - | - | 6-tests-build/0013-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch |
| 180 | `0180-6-tests-build-0014-ggml-cpu-remove-the-per-call-mall.patch` | arifi-fork-base | `fbd324a4b` | - | - | 6-tests-build/0014-ggml-cpu-remove-the-per-call-mallocs-from-the-TurboQ.patch |
| 181 | `0181-6-tests-build-0015-vulkan-add-mul_mat_vec_id-for-the.patch` | arifi-fork-base | `a68de0674` | - | - | 6-tests-build/0015-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch |
| 182 | `0182-6-tests-build-0016-feat-add-Vulkan-TQ4_1S-weight-pip.patch` | arifi-fork-base | `2809bc8f8` | - | - | 6-tests-build/0016-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch |
| 183 | `0183-6-tests-build-0017-tests-port-11-DSv4-shaped-TQ-MUL_.patch` | arifi-fork-base | `edb277526` | - | - | 6-tests-build/0017-tests-port-11-DSv4-shaped-TQ-MUL_MAT-cases-from-sync.patch |
| 184 | `0184-6-tests-build-0019-cuda-fix-MoE-cache-capacity-accou.patch` | arifi-fork-base | `813569dbd` | - | - | 6-tests-build/0019-cuda-fix-MoE-cache-capacity-accounting.patch |
| 185 | `0185-6-tests-build-0020-common-preserve-implicit-MoE-cach.patch` | arifi-fork-base | `dbd475c03` | - | - | 6-tests-build/0020-common-preserve-implicit-MoE-cache-provider-settings.patch |
| 186 | `0186-6-tests-build-0021-cuda-tune-MoE-cache-CPU-overlap-a.patch` | arifi-fork-base | `1d04b0084` | - | - | 6-tests-build/0021-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 187 | `0187-6-tests-build-0022-cuda-adapt-MoE-cache-admission-to.patch` | arifi-fork-base | `c670e80f2` | - | - | 6-tests-build/0022-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 188 | `0188-6-tests-build-0023-cuda-align-MoE-cache-pool-allocat.patch` | arifi-fork-base | `8e1434a27` | - | - | 6-tests-build/0023-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 189 | `0189-6-tests-build-0024-cuda-parallelize-MoE-cache-fills-.patch` | arifi-fork-base | `beb67774f` | - | - | 6-tests-build/0024-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch |
| 190 | `0190-6-tests-build-0025-cuda-fuse-cached-MoE-SwiGLU-rows..patch` | arifi-fork-base | `0efbf18c0` | - | - | 6-tests-build/0025-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 191 | `0191-6-tests-build-0026-cuda-aggregate-small-MoE-tensors-.patch` | arifi-fork-base | `94d0dc9e1` | - | - | 6-tests-build/0026-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 192 | `0192-6-tests-build-0027-cuda-bound-automatic-MoE-cache-ad.patch` | arifi-fork-base | `d86ae83e9` | - | - | 6-tests-build/0027-cuda-bound-automatic-MoE-cache-admission.patch |
| 193 | `0193-6-tests-build-0028-cuda-reject-undersized-automatic-.patch` | arifi-fork-base | `04e547feb` | - | - | 6-tests-build/0028-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 194 | `0194-6-tests-build-0029-cuda-enable-multi-token-MoE-cache.patch` | arifi-fork-base | `9c9678f3a` | - | - | 6-tests-build/0029-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 195 | `0195-6-tests-build-0030-common-surface-MoE-cache-activati.patch` | arifi-fork-base | `128843895` | - | - | 6-tests-build/0030-common-surface-MoE-cache-activation.patch |
| 196 | `0196-6-tests-build-0031-cuda-accelerate-complete-MoE-cach.patch` | arifi-fork-base | `58186f250` | - | - | 6-tests-build/0031-cuda-accelerate-complete-MoE-cache-pools.patch |
| 197 | `0197-6-tests-build-0032-cuda-report-oversized-MoE-cache-n.patch` | arifi-fork-base | `937f8e689` | - | - | 6-tests-build/0032-cuda-report-oversized-MoE-cache-nodes.patch |
| 198 | `0198-6-tests-build-0033-tests-initialise-non-contiguous-t.patch` | arifi-fork-base | `2311225e9` | - | - | 6-tests-build/0033-tests-initialise-non-contiguous-tensors-row-by-row.patch |
| 199 | `0199-6-tests-build-0034-tests-cover-turbo-KV-flash-attent.patch` | arifi-fork-base | `a883472a2` | - | - | 6-tests-build/0034-tests-cover-turbo-KV-flash-attention-at-head-dim-256.patch |
| 200 | `0200-6-tests-build-0037-moe-cache-provider-registry-with-.patch` | arifi-fork-base | `4f82d7209` | - | - | 6-tests-build/0037-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 201 | `0201-6-tests-build-0038-moe-cache-route-fit-probing-and-t.patch` | arifi-fork-base | `149f50068` | - | - | 6-tests-build/0038-moe-cache-route-fit-probing-and-test-session-calls-t.patch |
| 202 | `0202-6-tests-build-0039-moe-cache-route-context-eligibili.patch` | arifi-fork-base | `39e772360` | - | - | 6-tests-build/0039-moe-cache-route-context-eligibility-probe-and-remain.patch |
| 203 | `0203-6-tests-build-0040-moe-cache-allow-automatic-mode-on.patch` | arifi-fork-base | `919d9ac2c` | - | - | 6-tests-build/0040-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 204 | `0204-6-tests-build-0041-moe-cache-fix-auto-asymmetric-MLA.patch` | arifi-fork-base | `9146eb967` | - | - | 6-tests-build/0041-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch |
| 205 | `0205-6-tests-build-0042-test-moe-cache-accept-IGPU-device.patch` | arifi-fork-base | `804513bb8` | - | - | 6-tests-build/0042-test-moe-cache-accept-IGPU-devices-not-just-GPU.patch |
| 206 | `0206-6-tests-build-0043-test-moe-cache-make-cache-shared-.patch` | arifi-fork-base | `1211a72c3` | - | - | 6-tests-build/0043-test-moe-cache-make-cache-shared-budget-UMA-aware-wi.patch |
| 207 | `0207-6-tests-build-0044-test-moe-cache-save-and-restore-t.patch` | arifi-fork-base | `3ef5882f3` | - | - | 6-tests-build/0044-test-moe-cache-save-and-restore-the-CUDA-device-arou.patch |
| 208 | `0208-6-tests-build-0045-tests-tell-ctest-that-77-means-sk.patch` | arifi-fork-base | `59704c358` | - | - | 6-tests-build/0045-tests-tell-ctest-that-77-means-skip-for-test-moe-cac.patch |
| 209 | `0209-resolved-0011-fix-correct-Vulkan-turbo3-pipeline-wir.patch` | arifi-fork-base | `9cac10cf7` | - | - | resolved/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch |
| 210 | `0210-resolved-0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-ga.patch` | arifi-fork-base | `3521632c9` | - | - | resolved/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch |
| 211 | `0211-resolved-0005-merge-close-TurboQuant-parity-gaps.pat.patch` | arifi-fork-base | `5b27d7f80` | - | - | resolved/0005-merge-close-TurboQuant-parity-gaps.patch |
| 212 | `0212-resolved-0036-common-surface-MoE-cache-activation.pa.patch` | arifi-fork-base | `f833d6f09` | - | - | resolved/0036-common-surface-MoE-cache-activation.patch |
| 213 | `0213-resolved-0010-feat-add-Vulkan-turbo3-KV-cache-pipeli.patch` | arifi-fork-base | `679adff17` | - | - | resolved/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch |
| 214 | `0214-resolved-0004-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `f2b5185c3` | - | - | resolved/0004-fix-close-complete-audit-findings-7-port-regressions.patch |
| 215 | `0215-resolved-0005-metal-restore-lost-kernel-templates-ad.patch` | arifi-fork-base | `816008089` | - | - | resolved/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch |
| 216 | `0216-resolved-0012-metal-implement-DeepSeek-V4-hyper-conn.patch` | arifi-fork-base | `1165056f3` | - | - | resolved/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch |
| 217 | `0217-resolved-0035-cuda-gate-fused-TQ-mul_mat-paths-on-co.patch` | arifi-fork-base | `88ab9f2e5` | - | - | resolved/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch |
| 218 | `0218-resolved-0036-cuda-disable-fused-TQ3_1S-mul_mat-kern.patch` | arifi-fork-base | `cb1f93bae` | - | - | resolved/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch |
| 219 | `0219-resolved-0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HI.patch` | arifi-fork-base | `a981a981e` | - | - | resolved/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch |
| 220 | `0220-resolved-0047-sycl-hip-fix-two-build-breaks-in-fork-.patch` | arifi-fork-base | `2382acd55` | - | - | resolved/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch |
| 221 | `0221-resolved-0006-fix-close-complete-audit-findings-7-po.patch` | arifi-fork-base | `a9b7a2c68` | - | - | resolved/0006-fix-close-complete-audit-findings-7-port-regressions.patch |
| 222 | `0222-resolved-0022-DeepseekV4-MTP-DSpark-25784.patch.patch` | arifi-fork-base | `da16d9e37` | - | - | resolved/0022-DeepseekV4-MTP-DSpark-25784.patch |
| 223 | `0223-resolved-0023-dflash-fix-merge-artifacts-from-upstre.patch` | arifi-fork-base | `3670aa599` | - | - | resolved/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch |
| 224 | `0224-resolved-0029-server-harden-shared-draft-device-plac.patch` | arifi-fork-base | `646b6276b` | - | - | resolved/0029-server-harden-shared-draft-device-placement.patch |
| 225 | `0225-resolved-0025-fix-server-batch-restored-checkpoint-p.patch` | arifi-fork-base | `93f370d47` | - | - | resolved/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch |
| 226 | `0226-resolved-0026-cuda-rework-MoE-expert-cache-execution.patch` | arifi-fork-base | `4fa19b037` | - | - | resolved/0026-cuda-rework-MoE-expert-cache-execution.patch |
| 227 | `0227-resolved-0033-server-account-for-MTP-placement-in-fi.patch` | arifi-fork-base | `4419e16d3` | - | - | resolved/0033-server-account-for-MTP-placement-in-fit-reservations.patch |
| 228 | `0228-resolved-0035-cuda-reject-undersized-automatic-MoE-c.patch` | arifi-fork-base | `e2cf94791` | - | - | resolved/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 229 | `0229-resolved-0038-moe-cache-add-moe-cache-soft-mode-with.patch` | arifi-fork-base | `08bf89d53` | - | - | resolved/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch |
| 230 | `0230-resolved-0039-model-Muse-Glimmer-Support-26841.patch.patch` | arifi-fork-base | `975016049` | - | - | resolved/0039-model-Muse-Glimmer-Support-26841.patch |
| 231 | `0231-resolved-0043-moe-cache-audit-fixes-H1-F1-F2-A1.patc.patch` | arifi-fork-base | `b360f9016` | - | - | resolved/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch |
| 232 | `0232-resolved-0044-moe-cache-restore-moe_cache-params-on-.patch` | arifi-fork-base | `53a283c41` | - | - | resolved/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch |
| 233 | `0233-resolved-0046-llama-remove-dead-MSA-indexer-scaffold.patch` | arifi-fork-base | `fe6868c43` | - | - | resolved/0046-llama-remove-dead-MSA-indexer-scaffolding.patch |
| 234 | `0234-resolved-0047-moe-cache-add-logging-at-silent-failur.patch` | arifi-fork-base | `b1003d7fc` | - | - | resolved/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch |
| 235 | `0235-resolved-0052-cuda-fit-fix-two-Werror-build-failures.patch` | arifi-fork-base | `83d1eaf74` | - | - | resolved/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch |
| 236 | `0236-resolved-0005-test-fix-some-CI-errors-26415.patch.patch` | arifi-fork-base | `9347b0638` | - | - | resolved/0005-test-fix-some-CI-errors-26415.patch |
| 237 | `0237-resolved-remaining-conflict-markers.patch` | arifi-fork-base | `2b2b070a4` | - | - | resolved/remaining-conflict-markers |
| 238 | `0238-layer7-0001-WIP-add-TurboQuant-KV-cache-types-turbo3.patch` | arifi-fork-base | `f8bcd8e5d` | - | - | layer7/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch |
| 239 | `0239-layer7-0002-Update-GGMLQuantizationType-and-LlamaFil.patch` | arifi-fork-base | `2a48cb5a0` | - | - | layer7/0002-Update-GGMLQuantizationType-and-LlamaFileType-enums-.patch |
| 240 | `0240-layer7-0003-ggml-port-TurboQuant-core-quant-types-an.patch` | arifi-fork-base | `678ffff4e` | - | - | layer7/0003-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch |
| 241 | `0241-layer7-0005-docs-add-rebase-plan-and-TurboQuant-reci.patch` | arifi-fork-base | `10fb9f724` | - | - | layer7/0005-docs-add-rebase-plan-and-TurboQuant-recipes.patch |
| 242 | `0242-layer7-0007-gguf-py-dedupe-stacked-merge-artifacts-i.patch` | arifi-fork-base | `cc1e0274f` | - | - | layer7/0007-gguf-py-dedupe-stacked-merge-artifacts-in-constants..patch |
| 243 | `0243-layer7-0008-docs-add-KV-cache-quantization-guide-upd.patch` | arifi-fork-base | `79c7b3139` | - | - | layer7/0008-docs-add-KV-cache-quantization-guide-update-rebase-p.patch |
| 244 | `0244-layer7-0009-vulkan-port-fork-241-wave64-ballot-fix-c.patch` | arifi-fork-base | `1639cbdba` | - | - | layer7/0009-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch |
| 245 | `0245-layer7-0010-merge-close-TurboQuant-parity-gaps.patch.patch` | arifi-fork-base | `5982381ab` | - | - | layer7/0010-merge-close-TurboQuant-parity-gaps.patch |
| 246 | `0246-layer7-0011-metal-restore-lost-kernel-templates-add-.patch` | arifi-fork-base | `0be8bc913` | - | - | layer7/0011-metal-restore-lost-kernel-templates-add-offline-shad.patch |
| 247 | `0247-layer7-0014-laguna-tool-call-whitespace-tolerance-do.patch` | arifi-fork-base | `b44787449` | - | - | layer7/0014-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch |
| 248 | `0248-layer7-0019-fix-apply-rebase-audit-fixes-stack-overf.patch` | arifi-fork-base | `e8aa54219` | - | - | layer7/0019-fix-apply-rebase-audit-fixes-stack-overflow-quality-.patch |
| 249 | `0249-layer7-0020-docs-add-TurboQuant-project-overview-to-.patch` | arifi-fork-base | `636fd40ab` | - | - | layer7/0020-docs-add-TurboQuant-project-overview-to-AGENTS.md-an.patch |
| 250 | `0250-layer7-0021-cuda-rework-MoE-expert-cache-execution.p.patch` | arifi-fork-base | `75fefde2f` | - | - | layer7/0021-cuda-rework-MoE-expert-cache-execution.patch |
| 251 | `0251-layer7-0022-docs-update-MoE-cache-validation.patch.patch` | arifi-fork-base | `c80a85f3b` | - | - | layer7/0022-docs-update-MoE-cache-validation.patch |
| 252 | `0252-layer7-0023-common-preserve-implicit-MoE-cache-provi.patch` | arifi-fork-base | `2905c4c26` | - | - | layer7/0023-common-preserve-implicit-MoE-cache-provider-settings.patch |
| 253 | `0253-layer7-0024-server-harden-shared-draft-device-placem.patch` | arifi-fork-base | `aed527bac` | - | - | layer7/0024-server-harden-shared-draft-device-placement.patch |
| 254 | `0254-layer7-0025-cuda-tune-MoE-cache-CPU-overlap-automati.patch` | arifi-fork-base | `6c6760aa3` | - | - | layer7/0025-cuda-tune-MoE-cache-CPU-overlap-automatically.patch |
| 255 | `0255-layer7-0026-cuda-adapt-MoE-cache-admission-to-device.patch` | arifi-fork-base | `41d862b38` | - | - | layer7/0026-cuda-adapt-MoE-cache-admission-to-device-capability.patch |
| 256 | `0256-layer7-0027-cuda-align-MoE-cache-pool-allocation-wit.patch` | arifi-fork-base | `564cc862d` | - | - | layer7/0027-cuda-align-MoE-cache-pool-allocation-with-fit.patch |
| 257 | `0257-layer7-0028-docs-update-MoE-cache-validation.patch.patch` | arifi-fork-base | `357a5efd2` | - | - | layer7/0028-docs-update-MoE-cache-validation.patch |
| 258 | `0258-layer7-0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch.patch` | arifi-fork-base | `c9c7035be` | - | - | layer7/0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch |
| 259 | `0259-layer7-0030-cuda-aggregate-small-MoE-tensors-into-ca.patch` | arifi-fork-base | `12a3d36de` | - | - | layer7/0030-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch |
| 260 | `0260-layer7-0031-cuda-bound-automatic-MoE-cache-admission.patch` | arifi-fork-base | `ef6e6fd45` | - | - | layer7/0031-cuda-bound-automatic-MoE-cache-admission.patch |
| 261 | `0261-layer7-0032-cuda-reject-undersized-automatic-MoE-cac.patch` | arifi-fork-base | `ee88e1a42` | - | - | layer7/0032-cuda-reject-undersized-automatic-MoE-cache-slabs.patch |
| 262 | `0262-layer7-0033-cuda-enable-multi-token-MoE-cache-in-for.patch` | arifi-fork-base | `e3c652d07` | - | - | layer7/0033-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch |
| 263 | `0263-layer7-0034-common-surface-MoE-cache-activation.patc.patch` | arifi-fork-base | `db701a958` | - | - | layer7/0034-common-surface-MoE-cache-activation.patch |
| 264 | `0264-layer7-0035-docs-fix-MoE-cache-benchmark-verbosity.p.patch` | arifi-fork-base | `45db02335` | - | - | layer7/0035-docs-fix-MoE-cache-benchmark-verbosity.patch |
| 265 | `0265-layer7-0036-cuda-accelerate-complete-MoE-cache-pools.patch` | arifi-fork-base | `b3891c20d` | - | - | layer7/0036-cuda-accelerate-complete-MoE-cache-pools.patch |
| 266 | `0266-layer7-0037-cuda-report-oversized-MoE-cache-nodes.pa.patch` | arifi-fork-base | `d71db0a6b` | - | - | layer7/0037-cuda-report-oversized-MoE-cache-nodes.patch |
| 267 | `0267-layer7-0038-cuda-clarify-MoE-cache-session-diagnosti.patch` | arifi-fork-base | `f7a9260e5` | - | - | layer7/0038-cuda-clarify-MoE-cache-session-diagnostics.patch |
| 268 | `0268-layer7-0039-docs-record-MoE-cache-workload-convergen.patch` | arifi-fork-base | `9566c98bc` | - | - | layer7/0039-docs-record-MoE-cache-workload-convergence.patch |
| 269 | `0269-layer7-0040-cuda-report-MoE-cache-pair-residency.pat.patch` | arifi-fork-base | `f87f4ee26` | - | - | layer7/0040-cuda-report-MoE-cache-pair-residency.patch |
| 270 | `0270-layer7-0041-docs-document-what-the-test-suites-do-an.patch` | arifi-fork-base | `05c8996e3` | - | - | layer7/0041-docs-document-what-the-test-suites-do-and-do-not-cov.patch |
| 271 | `0271-layer7-0044-moe-cache-provider-registry-with-per-sch.patch` | arifi-fork-base | `03955dd69` | - | - | layer7/0044-moe-cache-provider-registry-with-per-scheduler-selec.patch |
| 272 | `0272-layer7-0046-moe-cache-allow-automatic-mode-on-a-sing.patch` | arifi-fork-base | `f54d46959` | - | - | layer7/0046-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch |
| 273 | `0273-layer7-0048-docs-document-INFO-level-MoE-cache-disab.patch` | arifi-fork-base | `34740bfc5` | - | - | layer7/0048-docs-document-INFO-level-MoE-cache-disable-reasons-i.patch |
| 274 | `0274-layer7-0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CAC.patch` | arifi-fork-base | `f847582f0` | - | - | layer7/0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CACHE.md-and-re.patch |
| 275 | `0275-layer7-0050-docs-correct-MOE-CACHE.md-against-the-im.patch` | arifi-fork-base | `84705341b` | - | - | layer7/0050-docs-correct-MOE-CACHE.md-against-the-implementation.patch |
| 276 | `0276-layer7-0051-ggml-split-the-turbo3-group-size-declara.patch` | arifi-fork-base | `c3424dbb6` | - | - | layer7/0051-ggml-split-the-turbo3-group-size-declaration-from-it.patch |
| 277 | `0277-resolve-8-committed-conflict-markers-on-merit.patch` | arifi-fork-base | `3ea6d9a71` | - | - | resolve 8 committed conflict markers on merit |
| 278 | `0278-fix-the-merge-so-it-builds-5-files-6-defects.patch` | arifi-fork-base | `6304b0545` | - | - | fix the merge so it builds: 5 files, 6 defects |
| 279 | `0279-merge-repairs-seven-defect-classes-and-one-the-compi.patch` | arifi-fork-base | `8bc896908` | - | - | merge repairs: seven defect classes, and one the compiler could never have reported |
| 280 | `0280-ROCmFPX-on-Vulkan-the-shader-half-ported.-NOT-YET-WO.patch` | arifi-fork-base | `7b892ad44` | - | - | ROCmFPX on Vulkan: the shader half, ported. NOT YET WORKING - it segfaults in execution. |
| 281 | `0281-currency-register-ALL-SEVEN-forks-so-nothing-can-dri.patch` | arifi-fork-base | `9cc15b897` | - | - | currency: register ALL SEVEN forks, so nothing can drift unseen again |
| 282 | `0282-series-base-and-the-branch-it-describes-are-ONE-pair.patch` | arifi-fork-base | `1e49fd129` | - | - | series: base and the branch it describes are ONE pair, and the linearity rule is now a gate |
| 283 | `0283-hygiene-the-code-review-skill-taught-a-type-ID-map-t.patch` | arifi-fork-base | `4360cbd2c` | - | - | hygiene: the code-review skill taught a type-ID map the arbitration deleted, and the UI was fetched from a moving tag |
| 284 | `0284-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | ternary-g128 | `4c5b247b8` | - | - | vulkan: ask the pipelines whether they exist, not the type id |
| 285 | `0285-contract-tq3-is-the-first-family-we-must-renumber-so.patch` | arifi-fork-base | `f34e0452e` | - | - | contract: tq3 is the first family we must renumber, so write down why before anyone ports it |
| 286 | `0286-sources-honest-pins-turboquant-re-pinned-off-a-dead-.patch` | arifi-fork-base | `194368dc8` | - | - | sources: honest pins - turboquant re-pinned off a dead lineage, tq3 main-understates note REFUTED, five type-id contract collisions recorded (lane-143) |
| 287 | `0287-sources-lane-143-self-refutation-the-turbo3-ODR-defe.patch` | arifi-fork-base | `f5843df9a` | - | - | sources: lane-143 self-refutation - the turbo3 ODR 'defect' does not exist in our tree (GGML_API already carries extern on all four branches); prisml pin REVERTED to keep the BEHIND signal live; FLAG E severity re-derived from loader code |
| 288 | `0288-sources-r2-checker-BLOCK-F1-F6-ciru-160-was-145-comm.patch` | arifi-fork-base | `4de1f6b22` | - | - | sources r2 (checker BLOCK F1-F6): ciru 160 was 145 commits of rocmfpx history - corrected to a 7-15 bracket, no-ancestor premise refuted, GOAL-3 ciru review performed, rocmfpx.ref revert, prisml contradiction removed, FLAG E sensitivity added |
| 289 | `0289-series-point-base.ref-at-the-LINEAR-branch-recut-lin.patch` | arifi-fork-base | `e0829d363` | - | - | series: point base.ref at the LINEAR branch - recut-linear/2026-08-17 is the byte-identical flatten of the blessed union branch, so the series is replayable at last (lane-147) |
| 290 | `0290-series-disclose-in-the-record-itself-that-the-flatte.patch` | arifi-fork-base | `e1b3aa2f6` | - | - | series: disclose in the record itself that the flatten absorbed 17 empty commits - content unchanged, history count changed (lane-147 checker finding 4) |
| 291 | `0291-ggml-port-the-tq3-TQ3_4S-family-codec-at-ids-48-51-T.patch` | arifi-fork-base | `8dd605718` | - | - | ggml: port the tq3 TQ3_4S family codec at ids 48-51 (TYPE-ID-ALLOCATION 3.1.1) - Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb |
| 292 | `0292-tests-cover-the-tq3-family-in-test-quantize-fns-and-.patch` | arifi-fork-base | `fdf2df3ee` | - | - | tests: cover the tq3 family in test-quantize-fns and test-backend-ops (kilobyte-scale proof before the 13 GB run) |
| 293 | `0293-contract-tq3-rows-48-51-implemented-behind-GGML_ARIF.patch` | arifi-fork-base | `1f430eb40` | - | - | contract: tq3 rows 48-51 implemented behind GGML_ARIFI_TURBO_WEIGHT_QUANTS, plus the three corrections the port forces (their 200 = our TURBO2_0; six ids, four destinations; token_embd is Q6_K) |
| 294 | `0294-tests-tq3_4s-tq3_4se-skipped-in-test-quantize-fns-wi.patch` | arifi-fork-base | `dff6e4b12` | - | - | tests: tq3_4s/tq3_4se skipped in test-quantize-fns with the measured reason (E3M5 scale caps at 0.492, harness data needs 1.4); tq3_0 given documented 3.5bpw bounds |
| 295 | `0295-tools-gguf-retag-tq3-lands-LAST-after-the-coherence-.patch` | arifi-fork-base | `90dd9cb1a` | - | - | tools: gguf-retag-tq3 - lands LAST, after the coherence proof, per TYPE-ID-ALLOCATION 3.1.1. Decides ambiguous id 46 by measured byte span; five refusals, no in-place path |
| 296 | `0296-checker-fixes-choose_index-restored-verbatim-inverte.patch` | arifi-fork-base | `61e5d9db5` | - | - | checker fixes: choose_index restored verbatim (inverted tie-break + 2 drifted constants, encoder-only); tq3 numeric block gives TQ3_4S real proof and pins every dot mapping; drop redundant GGML_API redecl |
| 297 | `0297-retag-verify-geometry-per-TENSOR-for-every-mapped-id.patch` | arifi-fork-base | `764e4c4e4` | - | - | retag: verify geometry per TENSOR for every mapped id, not a file-level vote on 46 only - fixes silent rewrite of mixed/ambiguous files and of unguarded 36/200; guard empty and overlapping tables |
| 298 | `0298-retag-README-per-tensor-geometry-check-for-every-map.patch` | arifi-fork-base | `a669040c5` | - | - | retag README: per-tensor geometry check for every mapped id, ten refusals, byte-level assertion |
| 299 | `0299-lane-144-GOAL-2-step-1-TQ3_4S-id-48-Vulkan-port-dequ.patch` | arifi-fork-base | `ed06c5a0b` | - | - | lane-144 GOAL 2 step 1: TQ3_4S (id 48) Vulkan port - dequant + mat-vec + mat-vec_id shaders, wired |
| 300 | `0300-lane-144-sweep-ROCmFP4-ROCmFPX-in-test-backend-ops-a.patch` | arifi-fork-base | `06e52de42` | - | - | lane-144: sweep ROCmFP4/ROCmFPX in test-backend-ops all_types (six identifiers, never listed before) |
| 301 | `0301-lane-144-CHECKER-FIX-TQ3_4S-was-missing-from-the-MUL.patch` | arifi-fork-base | `b82d7b86b` | - | - | lane-144 CHECKER FIX: TQ3_4S was missing from the MUL_MAT_ID staging-overflow guard |
| 302 | `0302-lane-145-Q6_0_ROCMFPX-Vulkan-fix-implement-the-6-bit.patch` | arifi-fork-base | `455c5f5c5` | - | - | lane-145: Q6_0_ROCMFPX Vulkan fix - implement the 6-bit unpack (block struct was 34B vs CPU 26B; qs read as whole signed bytes). 60/60 MUL_MAT + 24/24 GET_ROWS on Vulkan0, was 50/60 + 20/24. |
| 303 | `0303-lane-145-checker-finding-7-delete-block_rocmfpx_fp6_.patch` | arifi-fork-base | `82683a20f` | - | - | lane-145 checker finding 7: delete block_rocmfpx_fp6_packed16 + its A_TYPE_PACKED16 define (26B block has no valid uint16 view; nothing reads it). Re-verified 60/60 MUL_MAT, 24/24 GET_ROWS, 12/12 MUL_MAT_ID. |
| 304 | `0304-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | ternary-g128 | `8a95a6c41` | - | - | vulkan: ask the pipelines whether they exist, not the type id (SET_ROWS + CPY) |
| 305 | `0305-test-backend-ops-cover-the-CPY-types-supports_op-nam.patch` | arifi-fork-base | `0a9838950` | - | - | test-backend-ops: cover the CPY types supports_op names that all_types omits |
| 306 | `0306-lane-148-defect-A-ROOT-FIX-Vulkan-turbo3-centroid-LU.patch` | arifi-fork-base | `4fc3ab415` | - | - | lane-148 defect A ROOT FIX: Vulkan turbo3 centroid LUT was stale at 5 sites - CPY turbo3->f32 ERR 8.4302e-5 -> OK. CPU/CUDA/Metal/SYCL all carry CENTROIDS_3BIT (-0.190207,-0.118786,-0.066822,-0.021663); Vulkan alone carried a table whose midpoints do not match nearest_centroid_3bit(). copy_to_quant.comp's TM decision boundaries were derived from the stale table and are corrected too. |
| 307 | `0307-lane-148-defect-B-ROOT-FIX-FA_TYPE_TURBO2-3-4_0-GLSL.patch` | arifi-fork-base | `397d241e3` | - | - | lane-148 defect B ROOT FIX: FA_TYPE_TURBO2/3/4_0 GLSL spec-constant ids were stale (43/44/47) while enum ggml_type puts them at 200/201/202. Host passes the raw enum, so both switches fell through: fa_block_elems() returned its 1u default and dequantize4() returned vec4(0), zeroing V and the whole FA output. FLASH_ATTN_EXT -p turbo 0/1728 -> 1728/1728. |
| 308 | `0308-lane-148-F-69-sibling-coverage-test-backend-ops-FLAS.patch` | arifi-fork-base | `e6b62374d` | - | - | lane-148 F-69 sibling coverage: test-backend-ops FLASH_ATTN_EXT now covers TURBO2_0 too. Vulkan supports_op has always claimed turbo2 for FA but no test case existed, so its stale FA_TYPE id was invisible. 1728 -> 2592 cases, all PASS. |
| 309 | `0309-lane-148-checker-findings-4-5-FIXED-turbo4-SET_ROWS-.patch` | arifi-fork-base | `a61547983` | - | - | lane-148 checker findings 4+5 FIXED: turbo4 SET_ROWS was 0/21 declined behind a green 'Backend Vulkan0: OK' banner and turbo2 had no SET_ROWS case at all. Root cause: no cpy_turbo{2,4}_0_f32 copy-from-quant shader, so the tests' read-back leg declined. Added turbo2/turbo4 dequantize()/dequantize4()/get_dm(), the two spv+pipeline registrations, both supports_op and the DISPATCH switch (a third hand-synced pair - supports_op alone aborts with 'Missing CPY op'), and a test_set_rows_turbo2 class. SET_ROWS_TURBO2/3/4 now 21/21 EXECUTED each. |
| 310 | `0310-lane-149-F-110-guard-rung-b-arifi_sync_check.py-CMak.patch` | arifi-fork-base | `582b5e281` | - | - | lane-149 F-110 guard (rung b): arifi_sync_check.py + CMake wiring + 16 ARIFI-SYNC markers. Checks 49 LUT pairs, 16 FA_TYPE ids, 35 QUANT_K block sizes, 4 type-list SETs, FA K/V coverage; unregistered mirrors FAIL |
| 311 | `0311-lane-149-F-112-F-109-harness-guards-NOT_SUPPORTED-ca.patch` | ternary-g128 | `f2267a6c7` | - | - | lane-149 F-112 + F-109 harness guards: NOT_SUPPORTED cases counted and named, zero-executed run now FAILS; all_types coverage-parity assert with reason-bearing waivers; Q2_0_G128 added to all_types |
| 312 | `0312-lane-149-F-109-payoff-q2_0_g128-SET_ROWS-tie-roundin.patch` | ternary-g128 | `d8e90fdd9` | - | - | lane-149 F-109 payoff: q2_0_g128 SET_ROWS tie-rounding fixed. GLSL round() breaks .5 ties implementation-chosen; CPU roundf is half-away-from-zero. 8 f16 failures -> 2, ERR 5.1e-4 -> 1.7e-5 |
| 313 | `0313-lane-149-checker-driven-hardening-of-arifi-sync-chec.patch` | arifi-fork-base | `a1e795f2f` | - | - | lane-149 checker-driven hardening of arifi-sync-check: FA_TYPE/QUANT_K literal spellings, ordinal (not line) pins, per-definition completeness, any array type, recursive shader walk, scalar-mirror class, expected-count assertions |
| 314 | `0314-lane-149-F-112-second-half-checker-finding-11-per-op.patch` | arifi-fork-base | `9a046618c` | - | - | lane-149 F-112 second half (checker finding 11): per-op tallies name every op whose cases were ALL declined, so a zero-coverage family cannot hide inside a green full sweep |
| 315 | `0315-lane-150-step-1-TQ4_1S-read-back-leg-wired-CPY-tq4_1.patch` | arifi-fork-base | `88b0d6834` | - | - | lane-150 step 1: TQ4_1S read-back leg wired - CPY(tq4_1s,f32) now EXECUTES, 291/291 -> 293/293 |
| 316 | `0316-lane-150-step-2-TQ4_1S-write-leg-EXECUTES-SET_ROWS_T.patch` | arifi-fork-base | `c2d57011b` | - | - | lane-150 step 2: TQ4_1S write leg EXECUTES - SET_ROWS_TQ4_1S 17 cases/0 executed -> 17/17 PASS, and the inherited 5.0 waive is retired |
| 317 | `0317-lane-150-step-3-root-fix-for-the-hole-that-hid-the-w.patch` | arifi-fork-base | `bb71dcd05` | - | - | lane-150 step 3 (root fix for the hole that hid the whole class): per-op x TYPE zero-coverage tally |
| 318 | `0318-lane-150-checker-driven-fixes-type_bucket-prefix-bou.patch` | arifi-fork-base | `2afa34d24` | - | - | lane-150 checker-driven fixes: type_bucket prefix-boundary bug + dispatch block-size assert + the tallys limits named in code |
| 319 | `0319-lane-151-GOAL-6-hygiene-drop-648-tracked-_lane-out-d.patch` | arifi-fork-base | `78c9eabd2` | - | `GGML_ARIFI_VNNI_REPACK` | lane-151 GOAL 6 hygiene: drop 648 tracked _lane-out debris files (incl. an embedded shadow.git with commit receipts) and gitignore the dir; genericize estate paths in docs/, licenses/, tools/gguf-retag-tq3 |
| 320 | `0320-lane-151-GOAL-9-reserve-tq3-s-three-destination-less.patch` | arifi-fork-base | `31f755192` | - | - | lane-151 GOAL 9: reserve tq3's three destination-less serialized ids (37 TQ3_4SV -> 52, 31 TQ3_1S_AP1 -> 53, 44 TQ3_1S -> 54) as RESERVED-not-implemented in docs/TYPE-ID-ALLOCATION.md 3.1.2; geometry marked NOT DERIVED, no enumerators added, retag-tool refusals unchanged |
| 321 | `0321-lane-151-GOALs-6-7-README-refreshed-to-b10453-canoni.patch` | arifi-fork-base | `4a4ea78fb` | - | - | lane-151 GOALs 6+7: README refreshed to b10453 canonical arifi-main + the seven-ingested-sources table (two honest zeroes named); tq3 and ciru credits added; licenses/README.md gains the full Taken-from audit incl. the tq3 license GAP; UPDATE-RUNBOOK.md written (O(delta) recipe, THREE-flag configure, guards, the standard) |
| 322 | `0322-lane-151-GOAL-2-sources.json-base.ref-arifi-main-the.patch` | arifi-fork-base | `e65e4b365` | - | `GGML_ARIFI_VNNI_REPACK` | lane-151 GOAL 2: sources.json base.ref -> arifi/main (the canonical branch is now the series base) + series regen over the full canonical range: 323 patches, series check PASS, replay onto 4df29be4f identical outside patches/series |
| 323 | `0323-lane-151-GOAL-5-gitignore-.tokensave-the-fork-repo-n.patch` | arifi-fork-base | `e501e049a` | - | - | lane-151 GOAL 5: gitignore .tokensave/ - the fork repo now carries its own code graph (63344 nodes, 154451 edges over 3407 files) and the index is per-machine, never committed |
| 324 | `0324-lane-151-CHECKER-ROOT-FIX-series-check-now-asserts-e.patch` | arifi-fork-base | `920408c8b` | - | - | lane-151 CHECKER ROOT FIX: series check now asserts every patch is a COMMITTED BLOB, not just a file on disk. The byte-compare ran against the working directory, so untracked patches made it green while a fresh clone would die at git am - the defect that hid 33 missing patches twice. Help text corrected to match what the code does |
| 325 | `0325-lane-151-CHECKER-FIXES-docs-UPDATE-RUNBOOK-4.5-no-lo.patch` | arifi-fork-base | `a41a73f79` | - | - | lane-151 CHECKER FIXES (docs): UPDATE-RUNBOOK 4.5 no longer asserts a self-containment standard nothing on this estate meets - the four MinGW runtime DLLs are named and staged, and the measured stripped-PATH failure is recorded as identical for the raw build tree and the previous engine. licenses/README.md audit header stops claiming a purely mechanical trailer derivation: the tq3 row is a SUBJECT-line citation git never parses as a trailer, which is exactly why a trailer-only sweep would have missed the one real license gap |
| 326 | `0326-lane-151-HQ-DIRECTIVE-RETAIN-the-licenses-instead-of.patch` | arifi-fork-base | `8fb9910a9` | - | - | lane-151 HQ DIRECTIVE: RETAIN the licenses instead of flagging them. tq3-MIT, prisml-MIT and ciru-MIT extracted verbatim from this repo's OWN fetched remotes (git show refs/remotes/<r>/<head>:LICENSE). REFUTES my own GOAL-6 finding and a year-old one: I wrote 'no clone remains on the estate' and Phase 0 wrote 'PrismML has no LICENSE' - both were absence claims checked against the wrong artifact while the remotes sat fetched in this very repository. All seven registered sources audited at once. thecodacus STAYS open: no remote, no text, nothing invented |
| 327 | `0327-licenses-thecodacus-MIT-retained-from-the-public-Git.patch` | arifi-fork-base | `a8c59ea7b` | - | - | licenses: thecodacus-MIT retained from the public GitHub fork - the last open attribution CLOSED (no local remote != no source; President's correction 2026-08-18) |
| 328 | `0328-lane-152-GOAL-1-upstream-bump-b10453-b10481-the-FIRS.patch` | arifi-fork-base | `b555f90cc` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | lane-152 GOAL 1: upstream bump b10453 -> b10481, the FIRST live execution of UPDATE-RUNBOOK 2. 348 patches replayed onto 25ae3a9b3, 4 conflicts resolved by hand, series regenerated to 329, zero merges. |
| 329 | `0329-lane-152-what-the-FIRST-live-run-of-UPDATE-RUNBOOK-s.patch` | arifi-fork-base | `90a032846` | - | - | lane-152: what the FIRST live run of UPDATE-RUNBOOK section 2 corrected - one build break, two arifi_sync defects, the runbook itself, and the last license row |
| 330 | `0330-lane-152-GOAL-2-batch-1-ciru-s-Vulkan-SPIRV-Headers-.patch` | arifi-fork-base | `2e62d54f7` | ciru@8ffb2cd5a9b9e7eecc49335e0c79291a0df81d65, tq3@b1dd18410620bbf16909444d95ee2f9c10015b085, rocmfpx@8e6277f855df2a27ce072525ed19f3adc4138c47 | - | lane-152 GOAL 2 batch 1: ciru's Vulkan SPIRV-Headers fallback, tq3's f32 hybrid-SSM state gates, rocmfpx's -ffast-math guard |
| 331 | `0331-lane-152-GOAL-2-batch-2-three-lfm2-pre-tokenizer-has.patch` | arifi-fork-base | `978af8507` | rocmfpx@95bb4c798ee8c571e6b34ba4dd0ce15f4c2b9232 | - | lane-152 GOAL 2 batch 2: three lfm2 pre-tokenizer hashes that only rocmfpx had - and the reason they must be pre-computed |
| 332 | `0332-lane-152-pins-THREE-advance-with-reviews-upstream-ro.patch` | arifi-fork-base | `327d64620` | - | - | lane-152 pins: THREE advance with reviews (upstream, rocmfpx, tq3), TWO deliberately stay BEHIND (prisml, ciru) |
| 333 | `0333-lane-152-checker-F7-the-old-base-is-tag-b10454-not-b.patch` | arifi-fork-base | `c706b58a0` | - | - | lane-152 checker F7: the old base is tag b10454, not b10453 - correction of record |
| 334 | `0334-repo-env.json-WI-1540-tail-the-engine-repo-s-env-man.patch` | arifi-fork-base | `51b205a44` | - | - | repo-env.json (WI-1540 tail): the engine repo's env manifest - PowerShell-only law (F-107), three-flag configure (F-111), cmd.exe redirection (F-108), type-id FLAG E, series discipline, runbook pointer |
| 335 | `0335-WI-1580-sub-2-UPDATE-RUNBOOK-section-7-the-4.5-line-.patch` | arifi-fork-base | `379c08ed3` | - | - | WI-1580 sub-2: UPDATE-RUNBOOK section 7 - the 4.5 line was stale. lane-153 staged models/engines/arifi-b10481-union/ from a verified build of canonical, smoked from the staged copies, and flipped current-state.json + artifact-catalog.json, so 4.5 HAS now been executed once, by hand. The bump distinction is KEPT because it is still true: no bump has ever driven staging end to end (lane-152's bump refused at leg 2). Narrow edit - the section-2 and section-3 paragraphs around it are untouched. Note for the record: the lane-153 REPORT cites this text at UPDATE-RUNBOOK.md:238-244; it actually lives at :308-323 |
| 336 | `0336-vulkan-land-the-MoE-expert-cache-provider-files-VK-M.patch` | ternary-g128 | `6c8702ca5` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: land the MoE expert cache provider files (VK-MOE-CACHE step 1 of 2 - files, not yet wired) |
| 337 | `0337-graph-derive-the-V-unpad-head-count-from-the-attenti.patch` | arifi-fork-base | `0c462d769` | TheTom/llama-cpp-turboquant@f0420e056 | - | graph: derive the V-unpad head count from the attention output tensor, not from hparams |
| 338 | `0338-vulkan-wire-the-MoE-expert-cache-provider-into-the-b.patch` | ternary-g128 | `c0157fdb0` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: wire the MoE expert cache provider into the build (VK-MOE-CACHE step 2 of 2) |
| 339 | `0339-vulkan-complete-the-MoE-cache-port-the-56-provider-h.patch` | ternary-g128 | `5a1cbe8cb` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | vulkan: complete the MoE-cache port - the +56 provider-header addition, the four handle accessors, and the include-order ROOT FIX the link error exposed |
| 340 | `0340-sources-advance-the-turboquant-pin-f6124e914-7ebcbb0.patch` | arifi-fork-base | `41cef48ff` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: advance the turboquant pin f6124e914 -> 7ebcbb0b6 with pin_review_2026_08_18_lane154 |
| 341 | `0341-off-rig-backends-land-the-12-CUDA-Metal-SYCL-HIP-row.patch` | arifi-fork-base | `1c8854991` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | off-rig backends: land the 12 CUDA/Metal/SYCL/HIP rows unbuilt, per the HQ ruling of 2026-08-18 |
| 342 | `0342-sources-pin_review-names-the-12-off-rig-shas-and-the.patch` | arifi-fork-base | `e578d5fb7` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: pin_review names the 12 off-rig shas and their BUILD-UNPROVEN status (HQ condition 3) |
| 343 | `0343-sources-correct-the-pin_review-the-contested-12-12-a.patch` | arifi-fork-base | `8663db6f5` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: correct the pin_review - the contested 12/12 apply-conflict count is no longer the justification (checker F1) |
| 344 | `0344-sources-pin_review-records-the-apply-count-claim-as-.patch` | arifi-fork-base | `533db1837` | TheTom/llama-cpp-turboquant@7ebcbb0b6 | - | sources: pin_review records the apply-count claim as WRONG, not disputed (checker F1, resolved against me) |
| 345 | `0345-base-b10481-25ae3a9b3-b10488-9d77fa172-series-regen-.patch` | arifi-fork-base | `393c4936f` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | base: b10481/25ae3a9b3 -> b10488/9d77fa172 + series regen 346 (lane-156 upstream bump) |
| 346 | `0346-moe-cache-heat-protected-eviction-keeps-hot-experts-.patch` | arifi-fork-base | `4e32775f4` | llama-cpp-turboquant@730cc87ed (+ its doc follow-up cef75bf06) | - | moe-cache: heat-protected eviction keeps hot experts resident (turboquant tail) |
| 347 | `0347-tests-initialise-non-contiguous-tensors-row-by-row-t.patch` | arifi-fork-base | `7f0850d21` | llama-cpp-turboquant@f58ee0e97 | - | tests: initialise non-contiguous tensors row by row (turboquant row f58ee0e97) |
| 348 | `0348-sources-advance-the-turboquant-pin-7ebcbb0b6-d14e368.patch` | arifi-fork-base | `d162c8674` | - | - | sources: advance the turboquant pin 7ebcbb0b6 -> d14e36827 with a dated lane-156 pin_review |
| 349 | `0349-sources-prisml-pin-HELD-deliberately-with-a-sharper-.patch` | arifi-fork-base | `64fb40705` | - | - | sources: prisml pin HELD deliberately with a sharper reason + the v6 K-cache-mean-center verdict (lane-156 GOAL 4) |
| 350 | `0350-sources-ciru-re-derived-315-15-and-HELD-with-all-15-.patch` | arifi-fork-base | `eb8eb8c23` | - | - | sources: ciru re-derived 315 -> 15 and HELD with all 15 reviewed (lane-156 GOAL 5) |
| 351 | `0351-sources-advance-remotes.upstream.pin-25ae3a9b3-9d77f.patch` | arifi-fork-base | `9fedd180a` | - | - | sources: advance remotes.upstream.pin 25ae3a9b3 -> 9d77fa172 (b10488) with its review |
| 352 | `0352-sources-correct-the-turboquant-pin_review-s-FALSE-bl.patch` | arifi-fork-base | `0c5469b59` | - | - | sources: correct the turboquant pin_review's FALSE blocker for e130aef60 (checker F8, BLOCKING) |
| 353 | `0353-sources-trim-the-e130aef60-blocker-to-the-ONE-item-t.patch` | arifi-fork-base | `1e4449451` | - | - | sources: trim the e130aef60 blocker to the ONE item that survives (checker pass 2, advisory 1) |
| 354 | `0354-feat-kv-cache-port-flag-gated-mean-centering.patch` | arifi-fork-base | `df4902760` | prisml/prism-v6@b075a360c | - | feat(kv-cache): port flag-gated mean centering |
| 355 | `0355-feat-kv-cache-bind-calibration-to-exact-model.patch` | arifi-fork-base | `69fe7e3bb` | prisml/prism-v6@b075a360c | - | feat(kv-cache): bind calibration to exact model |
| 356 | `0356-fix-server-keep-target-calibration-out-of-draft-cont.patch` | arifi-fork-base | `6c4f7e148` | prisml/prism@4dd165625 | - | fix(server): keep target calibration out of draft contexts |
| 357 | `0357-test-kv-cache-add-Vulkan-tensor-probe-and-bench-pure.patch` | arifi-fork-base | `7088baef0` | - | - | test(kv-cache): add Vulkan tensor probe and bench-pure A/B harness |
| 358 | `0358-sources-record-lane-158-PrismML-K-cache-disposition.patch` | arifi-fork-base | `ba9d0ab82` | - | - | sources: record lane-158 PrismML K-cache disposition |
| 359 | `0359-test-kv-cache-fail-close-default-OFF-binary-identity.patch` | arifi-fork-base | `f30e9e964` | - | - | test(kv-cache): fail-close default-OFF binary identity |
| 360 | `0360-test-kv-cache-assert-both-rotation-basis-polarities-.patch` | arifi-fork-base | `a410fc9b3` | - | - | test(kv-cache): assert both rotation-basis polarities against the resolved attn_rot_k |
| 361 | `0361-fix-kv-cache-make-the-A-B-harness-able-to-launch-and.patch` | arifi-fork-base | `2772ec9e7` | - | - | fix(kv-cache): make the A/B harness able to launch, and actually enforce the RAM floor it claims |
| 362 | `0362-fix-kv-cache-floor-check-the-model-named-by-m-not-th.patch` | arifi-fork-base | `3024bf175` | - | - | fix(kv-cache): floor-check the model named by -m, not the largest .gguf on the command line |
| 363 | `0363-fix-kv-cache-wait-for-the-RAM-floor-between-arms-and.patch` | arifi-fork-base | `6be16f3e2` | - | - | fix(kv-cache): wait for the RAM floor between arms, and pin -fit off so the A/B arms match |
| 364 | `0364-fix-kv-cache-probe-the-layer-ids-the-calibration-act.patch` | arifi-fork-base | `3a7f42685` | - | - | fix(kv-cache): probe the layer ids the calibration actually has, and let an existing calibration be reused |
| 365 | `0365-support-DFlash2.patch` | arifi-fork-base | `b526efa91` | - | - | support DFlash2 |
| 366 | `0366-dflash2-build-fixes-on-the-PR-27342-pick-dedupe-DFLA.patch` | arifi-fork-base | `44102a64f` | - | - | dflash2: build fixes on the PR #27342 pick - dedupe DFLASH_BLOCK_SIZE enum/hparam/kv-map (fork DFlash1 originals keep authority, default 16) + ml. reference fix (the shadow the fork's own comment documents) |
| 367 | `0367-sources-register-buun-spiritbuun-buun-llama-cpp-as-a.patch` | arifi-fork-base | `2ec737cc9` | - | - | sources: register buun (spiritbuun/buun-llama-cpp) as a greedy-ingestion watch source - President ruling, VBR/KV + DFlash2-for-3.8 lineage |
| 368 | `0368-cuda-lift-TQ3_4S-kernel-set-from-tq3-master-14-files.patch` | arifi-fork-base | `8225dbd0e` | turbo-tan/llama.cpp-tq3@854516439 | - | cuda: lift TQ3_4S kernel set from tq3/master (14 files) |
| 369 | `0369-metal-lift-TQ3_4S-kernel-set-from-tq3-master-5-files.patch` | arifi-fork-base | `541fa6374` | turbo-tan/llama.cpp-tq3@854516439 | - | metal: lift TQ3_4S kernel set from tq3/master (5 files + alloc hook) |
| 370 | `0370-metal-declare-the-tq3_rht-pipeline-getter-in-ggml-me.patch` | arifi-fork-base | `4d38e0eb7` | turbo-tan/llama.cpp-tq3@854516439 | - | metal: declare the tq3_rht pipeline getter in ggml-metal-device.h |
| 371 | `0371-sync-move-base-b10488-b10524-regenerate-series-372-p.patch` | arifi-fork-base | `190d0ca04` | upstream@9ee9fc04c (tag b10524) | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | sync: move base b10488 -> b10524, regenerate series (372 patches) |
| 372 | `0372-spec-ingest-turboquant-MTP-boost-wave-effective-KV-b.patch` | arifi-fork-base | `9705d1c38` | llama-cpp-turboquant@bd1bf025f (524531e57, f9e04f5d7, 4be91d62b, 5b105dfb7, 4c4131bf8, b20e97012, 7544b18cf, 275963f50, cd638bc13, e82fe159b) | - | spec: ingest turboquant MTP-boost wave + effective-KV bench reporting |
| 373 | `0373-sync-re-pin-wave-3-sources-regenerate-series-374-pat.patch` | arifi-fork-base | `31a5f37d3` | - | `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `LLAMA_USE_PREBUILT_UI`, `ARIFI_TOOL_NVFP4_REMAP` | sync: re-pin wave-3 sources + regenerate series (374 patches) |
| 374 | `0374-arifi-sync-provenance-native-class-root-fix-sha-pinn.patch` | arifi-fork-base | `feb038bdc` | - | - | arifi-sync: provenance native-class root fix - sha-pinned grandfather ledger + native Origin convention |
| 375 | `0375-vulkan-subgroup-cooperative-TQ-mat-vec-tq3_1s-tq4_1s.patch` | ternary-g128 | `3c5ac214e` | - | - | vulkan: subgroup-cooperative TQ mat-vec (tq3_1s/tq4_1s/tq3_4s) |
| 376 | `0376-vulkan-register-blocked-4-elem-lane-TQ-subgroup-mat-.patch` | ternary-g128 | `f59532673` | - | - | vulkan: register-blocked 4-elem/lane TQ subgroup mat-vec (packed32 loads, in-register WHT stages 1-2, rows=4) |
| 377 | `0377-escha-native-Escha-W2-types-ESCHA2-55-ESCHA3-56-fuse.patch` | arifi-fork-base | `44aef02d2` | - | - | escha: native Escha-W2 types (ESCHA2=55/ESCHA3=56) + fused GGML_OP_ESCHA_MM, CPU + Vulkan |
| 378 | `0378-escha-F-125-fix-full-64-bit-pair-sourcing-row-parity.patch` | arifi-fork-base | `c5982591f` | - | - | escha: F-125 fix - full 64-bit pair sourcing + row-parity correction (K=2 AND K=3) |
| 379 | `0379-escha-test-backend-ops-un-nest-ESCHA_MM-eval-cases-m.patch` | arifi-fork-base | `a39174005` | - | - | escha: test-backend-ops — un-nest ESCHA_MM eval cases + multi-chunk ncols coverage + perf ncols curve |
| 380 | `0380-escha-vulkan-bound-every-escha_mm-dispatch-to-32-col.patch` | arifi-fork-base | `1b6bfaf58` | - | - | escha vulkan: bound every escha_mm dispatch to 32 columns (F-124 host-freeze guard) |
| 381 | `0381-escha-vulkan-column-blocked-escha_mm-one-weight-deco.patch` | arifi-fork-base | `abca2e782` | - | - | escha vulkan: column-blocked escha_mm - one weight decode serves 4 columns (part 2) |
| 382 | `0382-escha-vulkan-column-block-C-8-fall-back-to-the-one-c.patch` | arifi-fork-base | `f8716ab56` | - | - | escha vulkan: column block C=8 + fall back to the one-column kernel below a full block |
| 383 | `0383-escha-tests-cover-the-column-blocked-kernel-AT-MODEL.patch` | arifi-fork-base | `1f1864aba` | - | - | escha tests: cover the column-blocked kernel AT MODEL DEPTH (ncols=9, real 27B shapes) |
| 384 | `0384-escha-vulkan-one-hardware-f32-f16-convert-instead-of.patch` | arifi-fork-base | `e85e34b95` | - | - | escha vulkan: one hardware f32->f16 convert instead of the 15-instruction software RTE |
| 385 | `0385-escha-vulkan-subgroup-shuffle-Hadamard-14-barriers-p.patch` | arifi-fork-base | `d0bd8fe5e` | - | - | escha vulkan: subgroup-shuffle Hadamard - 14 barriers per input block become 3 |
| 386 | `0386-escha-vulkan-the-hardware-f32-f16-convert-must-be-fl.patch` | arifi-fork-base | `753d0eb64` | - | - | escha vulkan: the hardware f32->f16 convert must be float16_t, NOT packHalf2x16 (RTZ) |
| 387 | `0387-escha-vulkan-ESCHA_SG_HADAMARD-defaults-OFF-the-leve.patch` | arifi-fork-base | `6d3bd166c` | - | - | escha vulkan: ESCHA_SG_HADAMARD defaults OFF - the lever is refuted by measurement |
| 388 | `0388-escha-vulkan-f16-native-generator-decode-18-bit-exac.patch` | arifi-fork-base | `5c3582287` | - | - | escha vulkan: f16-native generator - decode +18%, bit-exact |
| 389 | `0389-escha-vulkan-two-more-levers-tried-and-REFUTED-by-me.patch` | arifi-fork-base | `450b8cfef` | - | - | escha vulkan: two more levers tried and REFUTED by measurement - occupancy and packed-pair |
| 390 | `0390-escha-vulkan-the-multi-column-threshold-was-disablin.patch` | arifi-fork-base | `49fc8b3d3` | - | - | escha vulkan: the multi-column threshold was disabling speculation - decode 3.96 -> 5.60 t/s |
| 391 | `0391-escha-vulkan-column-ladder-C2-C4-C8-C16-and-the-gene.patch` | arifi-fork-base | `b9ce31520` | - | - | escha vulkan: column ladder C2/C4/C8/C16, and the generator table measured and REFUTED |
| 392 | `0392-dflash-ingest-buun-s-DFlash2-adaptive-controller-bui.patch` | arifi-fork-base | `170d8c627` | - | - | dflash: ingest buun's DFlash2 adaptive controller - built, gated, and defaulted OFF by measurement |
| 393 | `0393-escha-vulkan-prefill-is-OCCUPANCY-bound-coopmat-and-.patch` | arifi-fork-base | `3cc0c7913` | - | - | escha vulkan: prefill is OCCUPANCY-bound - coopmat and wider rungs both refuted, and the profile that said otherwise was lying |
| 394 | `0394-escha-vulkan-f16-activation-staging-built-correct-an.patch` | arifi-fork-base | `8d5a484c4` | - | - | escha vulkan: f16 activation staging - built, correct, and refuted; the real prefill win was the build |
| 395 | `0395-dflash-reject-an-out-of-extent-draft-depth-instead-o.patch` | arifi-fork-base | `a1351b4b0` | - | - | dflash: reject an out-of-extent draft depth instead of silently clamping it, and block the mask token |
| 396 | `0396-spec-load-the-draft-model-from-the-same-variable-the.patch` | arifi-fork-base | `3d3bc4fd4` | - | - | spec: load the draft model from the same variable the log prints, and add an env-gated DFlash verify trace |
| 397 | `0397-F-136-ROOT-CAUSE-FIX-the-recurrent-snapshot-bank-n_r.patch` | prismml-recurrent | `2ef0bd659` | - | - | F-136 ROOT CAUSE + FIX: the recurrent snapshot-bank (n_rs_seq) machinery corrupts hybrid targets; drafters no longer flip the target onto it |
| 398 | `0398-spec-ON-DEVICE-speculative-checkpoints-drafter-now-B.patch` | arifi-fork-base | `2974512cb` | - | - | spec: ON-DEVICE speculative checkpoints - drafter now BEATS plain (8.02 vs 4.5 t/s, byte-clean) |
| 399 | `0399-spec-ring-rollback-re-enabled-DFlash2-depth-3-8.80-t.patch` | arifi-fork-base | `c030c10e2` | - | - | spec: ring rollback re-enabled - DFlash2 depth 3 = 8.80 t/s (2.0x plain), 0 degenerate |
| 400 | `0400-rs-zero-on-clear-ROOT-FIX-cross-request-recurrent-st.patch` | prismml-recurrent | `378ddb526` | - | - | rs: zero-on-clear ROOT FIX - cross-request recurrent-state leak was the residual F-136 mechanism |
| 401 | `0401-tests-zero_gather-CPY-GET_ROWS-Vulkan-probe-cases-se.patch` | arifi-fork-base | `7c2a235ec` | - | - | tests: zero_gather + CPY/GET_ROWS Vulkan probe cases; server: depth>cap clamps with warning instead of load abort (F-136 arc) |
| 402 | `0402-rs-zeroing-hunt-CLOSED-leak-is-backend-independent-C.patch` | arifi-fork-base | `8d7876f51` | - | - | rs zeroing hunt CLOSED: leak is backend-independent (CPU repro), in-graph rs_z zero never lands on the live cache tensor; zero-on-clear promoted belt->contract. Env-gated discriminators: PDH hash, plane0/sl/rl belt scoping, zero-gather-out, hybrid set_input trace |
| 403 | `0403-vulkan-F-136-residual-ROOT-FIX-in-graph_optimize-is_.patch` | ternary-g128 | `db0cd3814` | - | - | vulkan: F-136 residual ROOT FIX in graph_optimize is_src_of - full view-chain roots + RAW/WAR hazards through node sources; reorder no longer hoists reads over aliased inplace writes (repeat repro: belt-off now 3/3 byte-identical) |
| 404 | `0404-arifi-sync-base-move-b10524-9ee9fc04c-b10636-4d19b28.patch` | arifi-fork-base | `44d9f0bfe` | - | - | arifi-sync: base move b10524/9ee9fc04c -> b10636/4d19b2876 (runbook 2.2 manual path); upstream pin advanced as the same pair; prisml + tq3 pins HELD with 3 reviews recorded |
| 405 | `0405-b10636-bump-repair-restore-upstream-s-DOTS3NOTE-inde.patch` | arifi-fork-base | `7a40b423e` | - | - | b10636 bump repair: restore upstream's DOTS3NOTE indexer arch + the two TAG_LLAMA_SEQ_ID_NEG TODOs that a whole-file --theirs resolution reverted. checkout --theirs takes stage 3 (the WHOLE file), not just the conflicting hunks - upstream's 4-line b10524..b10636 delta to this file was lost with it. GLM_DSA is absent here PRE-EXISTING (also absent on lane165-pre-bump-arifi-main), so it is NOT restored by this commit and stays a President item. |
| 406 | `0406-b10636-bump-repair-2-close-llm_graph_input_attn_k_ds.patch` | arifi-fork-base | `0bccc45a3` | - | - | b10636 bump repair 2: close llm_graph_input_attn_k_dsa_iswa::can_reuse - the union resolution at ef642481d kept both sides' bodies but only one shared 'return res; }' tail, nesting every following definition inside it (build FAILED, 18 errors) |
| 407 | `0407-dflash-a-rejected-draft-checkpoint-image-must-not-ki.patch` | arifi-fork-base | `c41e4eb61` | github.com/spiritbuun/buun-llama-cpp@e332b2494 | - | dflash: a rejected draft checkpoint image must not kill the server process |
| 408 | `0408-seat-46-MSVC-portability-root-fixes-for-the-ROCm-HIP.patch` | arifi-fork-base | `df731fd27` | - | - | seat-46: MSVC portability root-fixes for the ROCm/HIP build (6 latent bugs: -fPIC on Windows, _MSC_VER fp16 macro parens, math.h/unistd.h guards, ssize_t->streamsize, dllimport on static lib) - build-rocm GREEN gfx1103, Vulkan build confirmed no-op |
| 409 | `0409-server-re-add-the-shared_draft_devices-VRAM-accounti.patch` | arifi-fork-base | `197f8f343` | - | - | server: re-add the shared_draft_devices VRAM accounting on top of common_fit_extra_model (multi-device only) |
| 410 | `0410-arifi-sync-base-move-b10636-4d19b2876-b10680-d7bd3bf.patch` | arifi-fork-base | `e7c4ffd7a` | - | - | arifi-sync: base move b10636/4d19b2876 -> b10680/d7bd3bfca (LATEST upstream tag, President mandate 2026-08-29); upstream pin advanced as one pair |
| 411 | `0411-vulkan-sync-check-follow-the-FA_TYPE-defines-into-b1.patch` | arifi-fork-base | `cc31c8aa1` | - | - | vulkan sync-check: follow the FA_TYPE defines into b10680's new fa_types.glsl - reading only flash_attn_base.glsl found ZERO ids and silently turned checks 2 and 5 into no-ops (the exact F-110 class the guard exists for); now 16 FA_TYPE ids + 12 FA K/V types again |
| 412 | `0412-b10680-bump-repair-common-fit.cpp-still-called-llm_f.patch` | arifi-fork-base | `53270c9b1` | - | - | b10680 bump repair: common/fit.cpp still called llm_ffn_exps_block_regex(idx), which b10680 renamed to llm_ffn_block_regex(idx, ffn_regex) - the fork's moe_cache code auto-merged past the rename and the build failed. Same body, LLM_FFN_EXPS_REGEX passed explicitly, so behaviour is identical. |
| 413 | `0413-b10680-bump-repair-2-my-hunk-1-union-in-the-3aa479a0.patch` | arifi-fork-base | `047f211f8` | - | - | b10680 bump repair 2: my hunk-1 union in the 3aa479a04 (support DFlash2) resolution kept selector_top_k from BOTH sides - duplicate member, build FAILED. One copy kept. The union trap the runbook already names, this time on a declaration rather than a shared block terminator. |
| 414 | `0414-runbook-4.1b-COLLISION-MAP-law-President-2026-08-29-.patch` | arifi-fork-base | `5020dfb1e` | - | - | runbook 4.1b: COLLISION MAP law (President 2026-08-29) - never lose our patches; A/B only where upstream delta intersects our changes; collision list = required bump-report line |
| 415 | `0415-attribution-gap-fix-seat-48-re-key-the-sha-dead-prov.patch` | arifi-fork-base | `7d300807c` | - | - | attribution gap-fix (seat-48): re-key the sha-dead provenance record + close the license/source-inventory gaps |
| 416 | `0416-runbook-2.2.1-RE-KEY-the-governed-sha-pointers-after.patch` | arifi-fork-base | `9fa4df299` | - | - | runbook 2.2.1: RE-KEY the governed sha pointers after every base move - mechanize G1 so the class cannot silently return |
| 417 | `0417-attribution-wave-2-seat-48-zuijdwijk-is-a-LIVE-take-.patch` | arifi-fork-base | `e93e5d437` | - | - | attribution wave 2 (seat-48): zuijdwijk is a LIVE take source, not a dormant remote - promote it, and record the G7 trailer debt instead of amending |
| 418 | `0418-G12-re-audit-seat-48-52-of-the-180-are-PROVEN-not-fo.patch` | arifi-fork-base | `ec4946ed6` | - | - | G12 re-audit (seat-48): 52 of the 180 are PROVEN not fork-authored - and none of them is unlicensed |
| 419 | `0419-R1-the-bank-plane-is-a-RUNTIME-INDEX-TENSOR-so-the-c.patch` | arifi-fork-base | `6b2a7943b` | - | - | R1: the bank plane is a RUNTIME INDEX TENSOR, so the copies go AND graph reuse stays |
| 420 | `0420-R1-fix-guard-the-bank-index-tensors-on-ALLOCATION-no.patch` | arifi-fork-base | `68979d342` | - | - | R1 fix: guard the bank index tensors on ALLOCATION, not on rs_r1 |
| 421 | `0421-lane-178-the-ring-s-extra-graph-split-is-the-SPLIT-I.patch` | arifi-fork-base | `b40d34455` | - | - | lane-178: the ring's extra graph split is the SPLIT-INPUTS CAP, not a CPU fallback - runtime-selectable so one binary is both arms |
| 422 | `0422-ggml-backend-adopt-split-inputs-cap-64-as-the-defaul.patch` | arifi-fork-base | `d7109fafe` | - | - | ggml-backend : adopt split-inputs cap 64 as the default |
| 423 | `0423-q2_0_g128-the-tie-predicate-is-exact-on-both-sides-n.patch` | ternary-g128 | `83c82af96` | - | - | q2_0_g128: the tie predicate is exact on both sides, not a reciprocal |
| 424 | `0424-lane-180-the-ring-s-26-extra-graph-inputs-are-23-IDE.patch` | prismml-recurrent | `532932a6f` | - | - | lane-180: the ring's 26 extra graph inputs are 23 IDENTICAL s_wrow views - one per recurrent layer (delta-net-base.cpp:849), 4 bytes each; share one view behind LLAMA_RS_WROW_SHARE=1 |
| 425 | `0425-lane-180-GATE-0-receipt-report-eHostVisible-in-the-v.patch` | arifi-fork-base | `381951370` | - | - | lane-180: GATE 0 receipt - report eHostVisible in the vk memory logger, not just eDeviceLocal |
| 426 | `0426-lane-180-ADOPT-share-ONE-s_wrow-view-by-default-29-i.patch` | arifi-fork-base | `a1a138000` | - | - | lane-180 ADOPT: share ONE s_wrow view by default - 29 identical per-layer split inputs collapsed |
| 427 | `0427-vulkan-perf-logger-must-not-abort-when-an-async-copy.patch` | ternary-g128 | `e7608c075` | - | - | vulkan: perf logger must not abort when an async copy left a compute context alive |
| 428 | `0428-lane-188-SHIPPING-KV-precision-tail-store-side-quant.patch` | arifi-fork-base | `0be0c0508` | 52c9acec Implement exact tails for quantized standard KV caches, a0a884b9 Optimize fully covered exact KV groups, 9f5850a0 Implement exact standard KV tail storage planning | - | lane-188: SHIPPING KV precision tail (store-side) - quantized body + exact F16 ring |
| 429 | `0429-lane-188-refuse-the-tail-on-MLA-caches-the-write-ord.patch` | arifi-fork-base | `ad42d9028` | - | - | lane-188: refuse the tail on MLA caches - the write-order detector cannot see the mla/lid idxs paths |
| 430 | `0430-lane-192-FIX-a-the-precision-tail-survives-a-prefix-.patch` | arifi-fork-base | `049697c79` | - | - | lane-192 FIX (a): the precision tail survives a prefix-cache rewind |
| 431 | `0431-lane-194-q4_0-F16-Vulkan-cpy-shader-F16-KV-tail-comp.patch` | arifi-fork-base | `f4dcd3f19` | - | - | lane-194: q4_0 -> F16 Vulkan cpy shader + F16 KV tail compose (OPT-IN, default OFF) |
| 432 | `0432-lane-194-the-CPU-reference-for-quant-F16-dup-which-d.patch` | arifi-fork-base | `ba591ac56` | - | - | lane-194: the CPU reference for quant -> F16 dup, which did not exist |
| 433 | `0433-lane-194-name-the-compose-type-in-the-tail-banner-th.patch` | arifi-fork-base | `4c7ce3f62` | - | - | lane-194: name the compose type in the tail banner - the receipt that stops a vacuous fidelity green |
| 434 | `0434-lane-195-multi-segment-flash-attention-the-ggml-API-.patch` | arifi-fork-base | `a04638027` | - | - | lane-195: multi-segment flash attention - the ggml API + CPU reference + its gate |
| 435 | `0435-lane-195-LEG-0-the-regression-leg-the-branch-was-mis.patch` | arifi-fork-base | `34f4553d0` | - | - | lane-195: LEG 0 - the regression leg the branch was missing, plus the sinks condition stated |
| 436 | `0436-lane-196-Design-B-BUILT-heterogeneous-split-k-FA-per.patch` | arifi-fork-base | `dc0d0a9bf` | - | - | lane-196: Design B BUILT - heterogeneous split-k FA (per-query source selection), the Vulkan dispatch |
| 437 | `0437-lane-196-promote-the-tail-ON-banner-to-WARN-llama-se.patch` | arifi-fork-base | `50e38d7ec` | - | - | lane-196: promote the tail-ON banner to WARN - llama-server drops INFO at default verbosity |
| 438 | `0438-lane-197-honor-spec-draft-p-min-in-the-DFlash2-sampl.patch` | arifi-fork-base | `abebad0c6` | - | - | lane-197: honor --spec-draft-p-min in the DFlash2 sampled selector path - the gate was greedy-branch-only, so the seated serve line (temp 0.7) ignored the flag entirely; hoisted to the shared site, greedy duplicate removed, flag-unset path untouched (p_min defaults 0.0) |
| 439 | `0439-lane-198-gate-Q5_K-n-1-and-Q4_K-n-5-off-the-q8_1-MMV.patch` | arifi-fork-base | `111973f5d` | - | - | lane-198: gate Q5_K (n>1) and Q4_K (n>=5) off the q8_1 MMVQ mat-vec shader on AMD - measured on the live speculative-verify graph (780M, proprietary driver): MMVQ per-dispatch cost steps 2.0-2.3x (Q4_K) / 1.55x (Q5_K) from NUM_COLS 4 to 5 while the f32 dequant shader rises 1.4-1.6x, and Q5_K is faster on the f32 shader at every measured n>1 (0.73-0.88x). GGML_VK_FORCE_MMVQ / GGML_VK_DISABLE_MMVQ still override; n==1 path untouched. Evidence: research/local-inference/lane-evidence/2026-09-01-lane-198/TABLE.json |
| 440 | `0440-runbook-4.1b-duty-3-a-named-collision-is-decided-by-.patch` | arifi-fork-base | `58df3f895` | - | - | runbook 4.1b duty 3: a named collision is decided by MECHANISM READ + THREE ARMS (theirs / ours / union), never by one number - President 2026-09-02 |
| 441 | `0441-vulkan-tiled-concat-transpose-for-the-delta-net-conv.patch` | ternary-g128 | `3caa602ad` | LaurentZuijdwijk/llama.cpp@1674aa4b4b70b1482c21a1b3881c4506191c4f6c | - | vulkan: tiled concat-transpose for the delta-net conv state |
| 442 | `0442-runbook-4.1-warn-that-series-regen-is-destructive-th.patch` | arifi-fork-base | `c07ed9c7c` | - | - | runbook 4.1: warn that `series regen` is destructive-then-abort, and require a pre-landing baseline |
| 443 | `0443-arifi-sync-root-fix-the-destructive-series-regen-and.patch` | arifi-fork-base | `93177b4d9` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `LANE110_PROF` | arifi-sync: root-fix the destructive series regen, and mechanize protected-win preservation |
| 444 | `0444-vulkan-label-the-concat-transpose-prior-art-numbers-.patch` | ternary-g128 | `c34d40317` | - | - | vulkan: label the concat-transpose prior-art numbers as upstream-reported, not ours |
| 445 | `0445-arifi-sync-name-the-incoming-ref-in-full-when-the-pr.patch` | arifi-fork-base | `d2433f1c9` | - | - | arifi-sync: name the incoming ref in full when the protected-win preflight refuses |
| 446 | `0446-arifi-sync-make-an-UNREGISTERED-measured-win-refuse-.patch` | arifi-fork-base | `df835d571` | - | `GENERATE_EXPERT_BUNDLE`, `LANE110_PROF`, `GGML_ARIFI_VNNI_REPACK`, `MAX_N_CACHED` | arifi-sync: make an UNREGISTERED measured win refuse the update path |
| 447 | `0447-arifi-sync-close-the-two-live-false-negatives-in-the.patch` | arifi-fork-base | `41668a312` | - | `GGML_ARIFI_VNNI_REPACK` | arifi-sync: close the two live false negatives in the protected-win gate, and mechanize the baseline boundary |
| 448 | `0448-arifi-sync-discharge-eleven-REGISTRATION-OWED-rows-a.patch` | arifi-fork-base | `57795cc38` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `GENERATE_EXPERT_BUNDLE`, `LANE110_PROF` | arifi-sync: discharge eleven REGISTRATION OWED rows, and close two live holes in the guard itself |
| 449 | `0449-arifi-sync-search-anchors-in-SOURCE-only-so-the-mani.patch` | arifi-fork-base | `0ebb9cc08` | - | - | arifi-sync: search anchors in SOURCE only, so the manifest cannot vouch for itself |
| 450 | `0450-arifi-sync-bring-the-manifest-s-own-_schema.anchors-.patch` | arifi-fork-base | `4511d79f7` | - | - | arifi-sync: bring the manifest's own `_schema.anchors` fact up to the source-only rule |
| 451 | `0451-arifi-sync-recognize-correctness-only-as-no-effect-v.patch` | arifi-fork-base | `725a5ff97` | - | - | arifi-sync: recognize `correctness only` as no-effect vocabulary |
| 452 | `0452-vulkan-create-the-ROCmFP4-FAST-mat-mat-pipelines-and.patch` | ternary-g128 | `f10f3a95e` | - | - | vulkan: create the ROCmFP4-FAST mat-mat pipelines and dequantize once per row in the generic mat-vec |
| 453 | `0453-vulkan-dedicated-IQ4_XS-mat-vec-shader-K-quant-style.patch` | ternary-g128 | `7062c6794` | - | - | vulkan: dedicated IQ4_XS mat-vec shader (K-quant style, 16 threads per superblock) + the 17408x5120 perf rows for test-backend-ops (lane-209 rank 5; gates run next) |
| 454 | `0454-vulkan-IQ3_S-mat-vec-16-invocations-per-superblock-a.patch` | ternary-g128 | `74ea96ac6` | - | - | vulkan: IQ3_S mat-vec 16 invocations per superblock at NUM_COLS >= 2 (zuijdwijk 211abf6a9 via lane-166, gate extended from >4; lane-209 rank 4; gates run next) |
| 455 | `0455-vulkan-IQ3_S-mat-vec-split-gate-becomes-NUM_COLS-3-N.patch` | ternary-g128 | `32a480d54` | LaurentZuijdwijk/llama.cpp 211abf6a9 (via lane-166/matvec-vgpr-fix, never merged), gate widened. | - | vulkan: IQ3_S mat-vec split gate becomes (NUM_COLS == 3 \|\| NUM_COLS > 4); the stock widths keep the upstream body bit for bit |
| 456 | `0456-vulkan-create-the-ROCmFP4-FAST-q8_1-MMVQ-mat-vec-pip.patch` | ternary-g128 | `d094f7bef` | rocmfpx/main mul_mat_vecq_funcs.glsl + its :4867/:4931 registrations and shaders-gen :752/:624 | - | vulkan: create the ROCmFP4-FAST q8_1 MMVQ mat-vec pipelines and gate them to n <= 5 on AMD |
| 457 | `0457-vulkan-keep-the-ROCmFP4-FAST-q8_1-MMVQ-path-to-n-2.5.patch` | ternary-g128 | `81dcb48fb` | - | - | vulkan: keep the ROCmFP4-FAST q8_1 MMVQ path to n=2..5 only |
| 458 | `0458-vulkan-enforce-ROCmFP4-MMVQ-width-gate-before-batch-.patch` | ternary-g128 | `f63bc66f8` | - | - | vulkan: enforce ROCmFP4 MMVQ width gate before batch routing |
| 459 | `0459-vulkan-exclude-ROCmFP4-MMVQ-n-4-tie-width.patch` | ternary-g128 | `80bff6e98` | - | - | vulkan: exclude ROCmFP4 MMVQ n=4 tie width |
| 460 | `0460-vulkan-isolate-ROCmFP4-MMVQ-to-n-3-and-n-5.patch` | ternary-g128 | `618c894d9` | - | - | vulkan: isolate ROCmFP4 MMVQ to n=3 and n=5 |
| 461 | `0461-vulkan-correct-lane-209-backend-coverage-comment.patch` | ternary-g128 | `e23edc305` | - | - | vulkan: correct lane-209 backend coverage comment |
| 462 | `0462-speculative-size-drafts-from-measured-acceptance-spe.patch` | arifi-fork-base | `ccd09f526` | github.com/LaurentZuijdwijk/llama.cpp@ca2616991 | - | speculative: size drafts from measured acceptance (--spec-draft-adaptive) |
| 463 | `0463-server-do-not-re-verify-replayed-draft-tokens-after-.patch` | arifi-fork-base | `e1b6361f5` | github.com/LaurentZuijdwijk/llama.cpp@aeca25573 | - | server: do not re-verify replayed draft tokens after a checkpoint restore |
| 464 | `0464-vulkan-q6_k-mat-vec-reads-its-scales-from-the-block-.patch` | ternary-g128 | `10b7c2350` | - | - | vulkan: q6_k mat-vec reads its scales from the block, not through LDS - MEASURED NEUTRAL on gfx1103 |
| 465 | `0465-arifi-sync-register-the-three-lane-209-mechanisms-th.patch` | arifi-fork-base | `f425b2c79` | - | - | arifi-sync: register the three lane-209 mechanisms this integration lands |
| 466 | `0466-test-backend-ops-adversarial-MUL_MAT-data-patterns-f.patch` | arifi-fork-base | `b3f61e883` | - | - | test-backend-ops: adversarial MUL_MAT data patterns for the ROCmFP4 MMVQ widths |
| 467 | `0467-vulkan-ROCmFP4-MMVQ-becomes-opt-in-GGML_ARIFI_ROCMFP.patch` | ternary-g128 | `2ac18271f` | - | - | vulkan: ROCmFP4 MMVQ becomes opt-in (GGML_ARIFI_ROCMFP4_MMVQ=1) - kept in full, not yet cleared |
| 468 | `0468-arifi-sync-register-the-rank-7-ROCmFP4-MMVQ-mechanis.patch` | arifi-fork-base | `b4080d300` | - | - | arifi-sync: register the rank-7 ROCmFP4 MMVQ mechanism, default-OFF and all |
| 469 | `0469-docs-the-public-trail-for-the-b10680-kernel-wave-lan.patch` | arifi-fork-base | `f1105c9d7` | - | - | docs: the public trail for the b10680 kernel wave - lanes 202, 204 and 209 |
| 470 | `0470-docs-scope-the-power-plan-claim-to-the-runs-that-ban.patch` | arifi-fork-base | `3eb7bd419` | - | - | docs: scope the power-plan claim to the runs that banked a receipt, and fix two counts |
| 471 | `0471-docs-apply-the-six-independent-check-fixes-to-the-b1.patch` | arifi-fork-base | `a1c706b54` | - | - | docs: apply the six independent-check fixes to the b10680 kernel-wave trail |
| 472 | `0472-arifi-sync-base-move-b10680-d7bd3bfca-b10819-6a1a922.patch` | arifi-fork-base | `23035e5b1` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `LANE110_PROF`, `GENERATE_EXPERT_BUNDLE` | arifi-sync: base move b10680/d7bd3bfca -> b10819/6a1a922d2, and the governed sha re-key that follows it |
| 473 | `0473-b10819-sync-repair-six-merge-defects-that-stopped-th.patch` | arifi-fork-base | `aa8f8feca` | - | - | b10819 sync: repair six merge defects that stopped the tree from building |
| 474 | `0474-vulkan-tq_rotate-is-a-src1-reformat-restore-the-TQ-M.patch` | ternary-g128 | `cb2bd09a7` | - | - | vulkan: tq_rotate is a src1 reformat - restore the TQ MUL_MAT_ID path |
| 475 | `0475-metal-union-the-dropped-upstream-FA-machinery-back-i.patch` | arifi-fork-base | `02893efe4` | - | - | metal: union the dropped upstream FA machinery back in - the tree did not compile |
| 476 | `0476-cuda-union-upstream-s-mul_mat_id_needs_sync-predicat.patch` | arifi-fork-base | `f16d2b3b4` | - | - | cuda: union upstream's mul_mat_id_needs_sync predicate with our TQ arm |
| 477 | `0477-ci-a-backend-matrix-that-compiles-what-this-rig-cann.patch` | arifi-fork-base | `badb275c4` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `LANE110_PROF`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI` | ci: a backend matrix that compiles what this rig cannot, with our formats ON |
| 478 | `0478-arifi-sync-restore-the-2-space-JSON-indent-the-R17-r.patch` | arifi-fork-base | `68be5f571` | - | `GGML_ARIFI_VNNI_REPACK`, `EXPERT_BUNDLE_PATH`, `LANE110_PROF`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI` | arifi-sync: restore the 2-space JSON indent the R17 register edits reformatted |
| 479 | `0479-ci-the-test-quantize-fns-gate-was-decorative-and-Met.patch` | arifi-fork-base | `b7b174a67` | - | - | ci: the test-quantize-fns gate was decorative, and Metal needs both library modes |
| 480 | `0480-cuda-restore-the-fused-MoE-cache-matvec-on-upstream-.patch` | arifi-fork-base | `9dcd2e8a7` | - | - | cuda: restore the fused MoE-cache matvec on upstream's fusion kernel (WI-1699) |
| 481 | `0481-ci-make-the-link-contract-real-and-run-test-backend-.patch` | arifi-fork-base | `b7b2751dc` | - | - | ci: make the link contract real, and run test-backend-ops on the CPU job |
| 482 | `0482-arifi-sync-R13B-s-two-b10819-repair-commits-become-a.patch` | arifi-fork-base | `cbe00c86f` | - | - | arifi-sync: R13B's two b10819 repair commits become also_commits of the mechanisms they keep alive |
| 483 | `0483-sycl-the-MMVQ-launchers-declared-one-pair-of-ranges-.patch` | arifi-fork-base | `618e0769a` | - | - | sycl: the MMVQ launchers declared one pair of ranges and launched with another |
| 484 | `0484-ci-each-job-now-proves-the-unioned-file-itself-produ.patch` | arifi-fork-base | `8a4b5a1d4` | - | - | ci: each job now proves the unioned file itself produced an object |
| 485 | `0485-metal-replace-the-garbled-library-loader-with-upstre.patch` | arifi-fork-base | `085d2065f` | - | - | metal: replace the garbled library loader with upstream's per-kind loader plus an ArifiLabs library kind |
| 486 | `0486-ci-the-cpu-ubuntu-job-runs-protected-win-validate-so.patch` | arifi-fork-base | `6139dfff9` | - | - | ci: the cpu-ubuntu job runs protected-win validate, so the manifest is gated where the code is |
| 487 | `0487-arifi-sync-protected-win-learns-a-symbols-class-a-re.patch` | arifi-fork-base | `eaf341d27` | - | - | arifi-sync: protected-win learns a symbols class - a registered kernel entry point must have a DEFINITION, not a mention (WI-1700) |
| 488 | `0488-arifi-sync-the-symbols-guard-ignores-commented-out-b.patch` | arifi-fork-base | `fe53a790f` | - | - | arifi-sync: the symbols guard ignores commented-out bodies, and the CUDA/Metal seed gaps the check found are closed (WI-1700 FIX-1/FIX-2) |
| 489 | `0489-docs-put-the-integration-A-B-numbers-into-the-public.patch` | arifi-fork-base | `5b66adad9` | - | - | docs: put the integration A/B numbers into the public trail, and record the b10819 base move |
| 490 | `0490-docs-state-the-power-plan-and-RAM-as-pre-registered-.patch` | arifi-fork-base | `4e0b7e262` | - | - | docs: state the power plan and RAM as pre-registered conditions, not per-launch receipts |
| 491 | `0491-arifi-sync-sources.json-post_move_tip-re-keyed-to-th.patch` | arifi-fork-base | `283a58842` | - | - | arifi-sync: sources.json post_move_tip re-keyed to the lane-214 integration tip |
| 492 | `0492-ggml-flash-attention-segment-count-moves-to-op_param.patch` | arifi-fork-base | `1d9c22856` | - | - | ggml: flash-attention segment count moves to op_params[5] - it collided with upstream's n_kv_max in [4] |
| 493 | `0493-vulkan-FA-dequant-KV-f16-scratch-is-a-runtime-gate-d.patch` | ternary-g128 | `9d26dc5ce` | - | - | vulkan: FA dequant-KV f16 scratch is a runtime gate, default ON on every device (upstream behaviour) - GGML_ARIFI_FA_DEQUANT_KV overrides both ways |
| 494 | `0494-arifi-sync-base-move-b10819-6a1a922d2-b10825-9e0e220.patch` | arifi-fork-base | `568d60751` | - | - | arifi-sync: base move b10819/6a1a922d2 -> b10825/9e0e22059 (R22 read, R27 execution); 552/552 replayed clean, 0 merges, 0 hunk collisions, marker scan 0, protected wins PASS; R20 slot fix + R19 gate (default ON per R19D) carried Lane: lane-220 |
| 495 | `0495-arifi-sync-rekey-3-protected-win-manifests-to-the-b1.patch` | arifi-fork-base | `44897b55e` | - | - | arifi-sync: rekey 3 protected-win manifests to the b10825 line (R27 check 05:1x: validate PASS only with this rekey; committed so the built line validates) Lane: lane-220 |
| 496 | `0496-arifi-sync-scope-subcommand-F-161-the-ingest-A-B-sco.patch` | arifi-fork-base | `b7c2d98ba` | - | - | arifi-sync: scope subcommand (F-161) - the ingest A/B scope derived from the collision map (upstream files x our files x hunk overlap x speed paths) = sanity\|named\|full; UPDATE-RUNBOOK 4.1b.4 scope law; selfcheck Lane: lane-220 |
| 497 | `0497-provenance-route-B-G7-append-only-R24-s-58-classific.patch` | arifi-fork-base | `7e2e3782c` | - | `EXPERT_BUNDLE_PATH`, `LANE110_PREFETCH_CAP`, `LANE110_PROF`, `MAX_N_CACHED`, `GENERATE_EXPERT_BUNDLE`, `LLAMA_USE_PREBUILT_UI`, `GGML_RECURRENT_STATE_F16`, `POWERINFER_IOCP`, `GGML_ARIFI_VNNI_REPACK`, `GGML_ARIFI_TURBO_KV`, `ARIFI_TOOL_NVFP4_REMAP` | provenance route B (G7 append-only): R24's 58 classifications recorded in native-grandfather (55 ARIFI, remapped to the b10825 shas) + pending-trailers (2 FORK, 1 UPSTREAM); 46 fork-authored tooling/manifest commits grandfathered; provenance PASS; series regen 496 patches |
| 498 | `0498-spec-runtime-switch-for-the-DFlash-encoder-fusion-LL.patch` | arifi-fork-base | `f9746db7b` | - | - | spec: runtime switch for the DFlash encoder fusion (LLAMA_DFLASH_FUSED_INJECT), both injection paths kept, default LEGACY on this fork |
| 499 | `0499-vulkan-RDNA3-mul_mat_vec_id-rows-runtime-selectable-.patch` | ternary-g128 | `0c4b9444d` | - | - | vulkan: RDNA3 mul_mat_vec_id rows runtime-selectable (GGML_ARIFI_MMV_ID_ROWS) + Ornith expert-shape perf rows |
| 500 | `0500-kv-cache-the-PLE-n-gram-history-lookup-becomes-a-run.patch` | arifi-fork-base | `0fe8c7f49` | - | - | kv-cache: the PLE n-gram history lookup becomes a runtime switch - seq_pos index by default, the cell scan behind LLAMA_KV_NGRAM_INDEX=0 |
| 501 | `0501-vulkan-MUL_MAT_ID-B-staging-receipt-and-a-loud-asser.patch` | ternary-g128 | `abdb77478` | - | - | vulkan: MUL_MAT_ID B-staging receipt, and a loud assert on the tq_rotate x K-padding hazard |
| 502 | `0502-vulkan-UMA-read-back-takes-the-direct-memcpy-only-wh.patch` | ternary-g128 | `2f241c57d` | - | - | vulkan: UMA read-back takes the direct memcpy only when the mapping is HOST_CACHED (WI-1693 fix A) |
| 503 | `0503-ggml-add-GGML_TYPE_SX8-57-the-S-X8-v4.3-CPU-decoder-.patch` | arifi-fork-base | `174ead5ca` | MarlaLabs llama-cpp-sx8.patch (github.com/MarlaLabs, Apache-2.0), cloned at cache/r16-sx8 | - | ggml: add GGML_TYPE_SX8 (57), the S-X8 v4.3 CPU decoder, retagged from the author's 41, plus its retag/cross-check tooling |
| 504 | `0504-ggml-port-jtrefon-TBQ3_0-TBQ4_0-at-ids-58-59-with-th.patch` | arifi-fork-base | `3e3eb64d0` | jtrefon/llama.cpp-turboq-mtp@6a02d0494 (ggml/src/ggml-turboq.c, ggml-turboq-tables.h, ggml-common.h, ggml.c, ggml-cpu, src/llama-kv-cache.cpp, src/llama-graph.cpp, src/llama-context.cpp, common/arg.cpp, tests) | - | ggml: port jtrefon TBQ3_0/TBQ4_0 at ids 58/59 with the type-id arbitration, plus the SET_ROWS device guard (WI-1694 half A) |
| 505 | `0505-docs-protected-wins-the-R18-switch-table-eight-DOCUM.patch` | arifi-fork-base | `4a8c1536e` | - | - | docs + protected-wins: the R18 switch table - eight DOCUMENTED-TAKE runtime switches registered with their measured effect |

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
| `0209-resolved-0011-fix-correct-Vulkan-turbo3-pipeline-wir.patch` | *(no Measured-effect trailer)* |
| `0210-resolved-0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-ga.patch` | *(no Measured-effect trailer)* |
| `0211-resolved-0005-merge-close-TurboQuant-parity-gaps.pat.patch` | *(no Measured-effect trailer)* |
| `0212-resolved-0036-common-surface-MoE-cache-activation.pa.patch` | *(no Measured-effect trailer)* |
| `0213-resolved-0010-feat-add-Vulkan-turbo3-KV-cache-pipeli.patch` | *(no Measured-effect trailer)* |
| `0214-resolved-0004-fix-close-complete-audit-findings-7-po.patch` | *(no Measured-effect trailer)* |
| `0215-resolved-0005-metal-restore-lost-kernel-templates-ad.patch` | *(no Measured-effect trailer)* |
| `0216-resolved-0012-metal-implement-DeepSeek-V4-hyper-conn.patch` | *(no Measured-effect trailer)* |
| `0217-resolved-0035-cuda-gate-fused-TQ-mul_mat-paths-on-co.patch` | *(no Measured-effect trailer)* |
| `0218-resolved-0036-cuda-disable-fused-TQ3_1S-mul_mat-kern.patch` | *(no Measured-effect trailer)* |
| `0219-resolved-0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HI.patch` | *(no Measured-effect trailer)* |
| `0220-resolved-0047-sycl-hip-fix-two-build-breaks-in-fork-.patch` | *(no Measured-effect trailer)* |
| `0221-resolved-0006-fix-close-complete-audit-findings-7-po.patch` | *(no Measured-effect trailer)* |
| `0222-resolved-0022-DeepseekV4-MTP-DSpark-25784.patch.patch` | *(no Measured-effect trailer)* |
| `0223-resolved-0023-dflash-fix-merge-artifacts-from-upstre.patch` | *(no Measured-effect trailer)* |
| `0224-resolved-0029-server-harden-shared-draft-device-plac.patch` | *(no Measured-effect trailer)* |
| `0225-resolved-0025-fix-server-batch-restored-checkpoint-p.patch` | *(no Measured-effect trailer)* |
| `0226-resolved-0026-cuda-rework-MoE-expert-cache-execution.patch` | *(no Measured-effect trailer)* |
| `0227-resolved-0033-server-account-for-MTP-placement-in-fi.patch` | *(no Measured-effect trailer)* |
| `0228-resolved-0035-cuda-reject-undersized-automatic-MoE-c.patch` | *(no Measured-effect trailer)* |
| `0229-resolved-0038-moe-cache-add-moe-cache-soft-mode-with.patch` | *(no Measured-effect trailer)* |
| `0230-resolved-0039-model-Muse-Glimmer-Support-26841.patch.patch` | *(no Measured-effect trailer)* |
| `0231-resolved-0043-moe-cache-audit-fixes-H1-F1-F2-A1.patc.patch` | *(no Measured-effect trailer)* |
| `0232-resolved-0044-moe-cache-restore-moe_cache-params-on-.patch` | *(no Measured-effect trailer)* |
| `0233-resolved-0046-llama-remove-dead-MSA-indexer-scaffold.patch` | *(no Measured-effect trailer)* |
| `0234-resolved-0047-moe-cache-add-logging-at-silent-failur.patch` | *(no Measured-effect trailer)* |
| `0235-resolved-0052-cuda-fit-fix-two-Werror-build-failures.patch` | *(no Measured-effect trailer)* |
| `0236-resolved-0005-test-fix-some-CI-errors-26415.patch.patch` | *(no Measured-effect trailer)* |
| `0237-resolved-remaining-conflict-markers.patch` | *(no Measured-effect trailer)* |
| `0238-layer7-0001-WIP-add-TurboQuant-KV-cache-types-turbo3.patch` | *(no Measured-effect trailer)* |
| `0239-layer7-0002-Update-GGMLQuantizationType-and-LlamaFil.patch` | *(no Measured-effect trailer)* |
| `0240-layer7-0003-ggml-port-TurboQuant-core-quant-types-an.patch` | *(no Measured-effect trailer)* |
| `0241-layer7-0005-docs-add-rebase-plan-and-TurboQuant-reci.patch` | *(no Measured-effect trailer)* |
| `0242-layer7-0007-gguf-py-dedupe-stacked-merge-artifacts-i.patch` | *(no Measured-effect trailer)* |
| `0243-layer7-0008-docs-add-KV-cache-quantization-guide-upd.patch` | *(no Measured-effect trailer)* |
| `0244-layer7-0009-vulkan-port-fork-241-wave64-ballot-fix-c.patch` | *(no Measured-effect trailer)* |
| `0245-layer7-0010-merge-close-TurboQuant-parity-gaps.patch.patch` | *(no Measured-effect trailer)* |
| `0246-layer7-0011-metal-restore-lost-kernel-templates-add-.patch` | *(no Measured-effect trailer)* |
| `0247-layer7-0014-laguna-tool-call-whitespace-tolerance-do.patch` | *(no Measured-effect trailer)* |
| `0248-layer7-0019-fix-apply-rebase-audit-fixes-stack-overf.patch` | *(no Measured-effect trailer)* |
| `0249-layer7-0020-docs-add-TurboQuant-project-overview-to-.patch` | *(no Measured-effect trailer)* |
| `0250-layer7-0021-cuda-rework-MoE-expert-cache-execution.p.patch` | *(no Measured-effect trailer)* |
| `0251-layer7-0022-docs-update-MoE-cache-validation.patch.patch` | *(no Measured-effect trailer)* |
| `0252-layer7-0023-common-preserve-implicit-MoE-cache-provi.patch` | *(no Measured-effect trailer)* |
| `0253-layer7-0024-server-harden-shared-draft-device-placem.patch` | *(no Measured-effect trailer)* |
| `0254-layer7-0025-cuda-tune-MoE-cache-CPU-overlap-automati.patch` | *(no Measured-effect trailer)* |
| `0255-layer7-0026-cuda-adapt-MoE-cache-admission-to-device.patch` | *(no Measured-effect trailer)* |
| `0256-layer7-0027-cuda-align-MoE-cache-pool-allocation-wit.patch` | *(no Measured-effect trailer)* |
| `0257-layer7-0028-docs-update-MoE-cache-validation.patch.patch` | *(no Measured-effect trailer)* |
| `0258-layer7-0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch.patch` | *(no Measured-effect trailer)* |
| `0259-layer7-0030-cuda-aggregate-small-MoE-tensors-into-ca.patch` | *(no Measured-effect trailer)* |
| `0260-layer7-0031-cuda-bound-automatic-MoE-cache-admission.patch` | *(no Measured-effect trailer)* |
| `0261-layer7-0032-cuda-reject-undersized-automatic-MoE-cac.patch` | *(no Measured-effect trailer)* |
| `0262-layer7-0033-cuda-enable-multi-token-MoE-cache-in-for.patch` | *(no Measured-effect trailer)* |
| `0263-layer7-0034-common-surface-MoE-cache-activation.patc.patch` | *(no Measured-effect trailer)* |
| `0264-layer7-0035-docs-fix-MoE-cache-benchmark-verbosity.p.patch` | *(no Measured-effect trailer)* |
| `0265-layer7-0036-cuda-accelerate-complete-MoE-cache-pools.patch` | *(no Measured-effect trailer)* |
| `0266-layer7-0037-cuda-report-oversized-MoE-cache-nodes.pa.patch` | *(no Measured-effect trailer)* |
| `0267-layer7-0038-cuda-clarify-MoE-cache-session-diagnosti.patch` | *(no Measured-effect trailer)* |
| `0268-layer7-0039-docs-record-MoE-cache-workload-convergen.patch` | *(no Measured-effect trailer)* |
| `0269-layer7-0040-cuda-report-MoE-cache-pair-residency.pat.patch` | *(no Measured-effect trailer)* |
| `0270-layer7-0041-docs-document-what-the-test-suites-do-an.patch` | *(no Measured-effect trailer)* |
| `0271-layer7-0044-moe-cache-provider-registry-with-per-sch.patch` | *(no Measured-effect trailer)* |
| `0272-layer7-0046-moe-cache-allow-automatic-mode-on-a-sing.patch` | *(no Measured-effect trailer)* |
| `0273-layer7-0048-docs-document-INFO-level-MoE-cache-disab.patch` | *(no Measured-effect trailer)* |
| `0274-layer7-0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CAC.patch` | *(no Measured-effect trailer)* |
| `0275-layer7-0050-docs-correct-MOE-CACHE.md-against-the-im.patch` | *(no Measured-effect trailer)* |
| `0276-layer7-0051-ggml-split-the-turbo3-group-size-declara.patch` | *(no Measured-effect trailer)* |
| `0277-resolve-8-committed-conflict-markers-on-merit.patch` | *(no Measured-effect trailer)* |
| `0278-fix-the-merge-so-it-builds-5-files-6-defects.patch` | *(no Measured-effect trailer)* |
| `0279-merge-repairs-seven-defect-classes-and-one-the-compi.patch` | *(no Measured-effect trailer)* |
| `0280-ROCmFPX-on-Vulkan-the-shader-half-ported.-NOT-YET-WO.patch` | *(no Measured-effect trailer)* |
| `0281-currency-register-ALL-SEVEN-forks-so-nothing-can-dri.patch` | *(no Measured-effect trailer)* |
| `0282-series-base-and-the-branch-it-describes-are-ONE-pair.patch` | *(no Measured-effect trailer)* |
| `0283-hygiene-the-code-review-skill-taught-a-type-ID-map-t.patch` | *(no Measured-effect trailer)* |
| `0284-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | *(no Measured-effect trailer)* |
| `0285-contract-tq3-is-the-first-family-we-must-renumber-so.patch` | *(no Measured-effect trailer)* |
| `0286-sources-honest-pins-turboquant-re-pinned-off-a-dead-.patch` | *(no Measured-effect trailer)* |
| `0287-sources-lane-143-self-refutation-the-turbo3-ODR-defe.patch` | *(no Measured-effect trailer)* |
| `0288-sources-r2-checker-BLOCK-F1-F6-ciru-160-was-145-comm.patch` | *(no Measured-effect trailer)* |
| `0289-series-point-base.ref-at-the-LINEAR-branch-recut-lin.patch` | *(no Measured-effect trailer)* |
| `0290-series-disclose-in-the-record-itself-that-the-flatte.patch` | *(no Measured-effect trailer)* |
| `0291-ggml-port-the-tq3-TQ3_4S-family-codec-at-ids-48-51-T.patch` | *(no Measured-effect trailer)* |
| `0292-tests-cover-the-tq3-family-in-test-quantize-fns-and-.patch` | *(no Measured-effect trailer)* |
| `0293-contract-tq3-rows-48-51-implemented-behind-GGML_ARIF.patch` | *(no Measured-effect trailer)* |
| `0294-tests-tq3_4s-tq3_4se-skipped-in-test-quantize-fns-wi.patch` | *(no Measured-effect trailer)* |
| `0295-tools-gguf-retag-tq3-lands-LAST-after-the-coherence-.patch` | *(no Measured-effect trailer)* |
| `0296-checker-fixes-choose_index-restored-verbatim-inverte.patch` | *(no Measured-effect trailer)* |
| `0297-retag-verify-geometry-per-TENSOR-for-every-mapped-id.patch` | *(no Measured-effect trailer)* |
| `0298-retag-README-per-tensor-geometry-check-for-every-map.patch` | *(no Measured-effect trailer)* |
| `0299-lane-144-GOAL-2-step-1-TQ3_4S-id-48-Vulkan-port-dequ.patch` | *(no Measured-effect trailer)* |
| `0300-lane-144-sweep-ROCmFP4-ROCmFPX-in-test-backend-ops-a.patch` | *(no Measured-effect trailer)* |
| `0301-lane-144-CHECKER-FIX-TQ3_4S-was-missing-from-the-MUL.patch` | *(no Measured-effect trailer)* |
| `0302-lane-145-Q6_0_ROCMFPX-Vulkan-fix-implement-the-6-bit.patch` | *(no Measured-effect trailer)* |
| `0303-lane-145-checker-finding-7-delete-block_rocmfpx_fp6_.patch` | *(no Measured-effect trailer)* |
| `0304-vulkan-ask-the-pipelines-whether-they-exist-not-the-.patch` | *(no Measured-effect trailer)* |
| `0305-test-backend-ops-cover-the-CPY-types-supports_op-nam.patch` | *(no Measured-effect trailer)* |
| `0306-lane-148-defect-A-ROOT-FIX-Vulkan-turbo3-centroid-LU.patch` | *(no Measured-effect trailer)* |
| `0307-lane-148-defect-B-ROOT-FIX-FA_TYPE_TURBO2-3-4_0-GLSL.patch` | *(no Measured-effect trailer)* |
| `0308-lane-148-F-69-sibling-coverage-test-backend-ops-FLAS.patch` | *(no Measured-effect trailer)* |
| `0309-lane-148-checker-findings-4-5-FIXED-turbo4-SET_ROWS-.patch` | *(no Measured-effect trailer)* |
| `0310-lane-149-F-110-guard-rung-b-arifi_sync_check.py-CMak.patch` | *(no Measured-effect trailer)* |
| `0311-lane-149-F-112-F-109-harness-guards-NOT_SUPPORTED-ca.patch` | *(no Measured-effect trailer)* |
| `0312-lane-149-F-109-payoff-q2_0_g128-SET_ROWS-tie-roundin.patch` | *(no Measured-effect trailer)* |
| `0313-lane-149-checker-driven-hardening-of-arifi-sync-chec.patch` | *(no Measured-effect trailer)* |
| `0314-lane-149-F-112-second-half-checker-finding-11-per-op.patch` | *(no Measured-effect trailer)* |
| `0315-lane-150-step-1-TQ4_1S-read-back-leg-wired-CPY-tq4_1.patch` | *(no Measured-effect trailer)* |
| `0316-lane-150-step-2-TQ4_1S-write-leg-EXECUTES-SET_ROWS_T.patch` | *(no Measured-effect trailer)* |
| `0317-lane-150-step-3-root-fix-for-the-hole-that-hid-the-w.patch` | *(no Measured-effect trailer)* |
| `0318-lane-150-checker-driven-fixes-type_bucket-prefix-bou.patch` | *(no Measured-effect trailer)* |
| `0319-lane-151-GOAL-6-hygiene-drop-648-tracked-_lane-out-d.patch` | *(no Measured-effect trailer)* |
| `0320-lane-151-GOAL-9-reserve-tq3-s-three-destination-less.patch` | *(no Measured-effect trailer)* |
| `0321-lane-151-GOALs-6-7-README-refreshed-to-b10453-canoni.patch` | *(no Measured-effect trailer)* |
| `0322-lane-151-GOAL-2-sources.json-base.ref-arifi-main-the.patch` | *(no Measured-effect trailer)* |
| `0323-lane-151-GOAL-5-gitignore-.tokensave-the-fork-repo-n.patch` | *(no Measured-effect trailer)* |
| `0324-lane-151-CHECKER-ROOT-FIX-series-check-now-asserts-e.patch` | *(no Measured-effect trailer)* |
| `0325-lane-151-CHECKER-FIXES-docs-UPDATE-RUNBOOK-4.5-no-lo.patch` | *(no Measured-effect trailer)* |
| `0326-lane-151-HQ-DIRECTIVE-RETAIN-the-licenses-instead-of.patch` | *(no Measured-effect trailer)* |
| `0327-licenses-thecodacus-MIT-retained-from-the-public-Git.patch` | *(no Measured-effect trailer)* |
| `0328-lane-152-GOAL-1-upstream-bump-b10453-b10481-the-FIRS.patch` | *(no Measured-effect trailer)* |
| `0329-lane-152-what-the-FIRST-live-run-of-UPDATE-RUNBOOK-s.patch` | *(no Measured-effect trailer)* |
| `0330-lane-152-GOAL-2-batch-1-ciru-s-Vulkan-SPIRV-Headers-.patch` | UNMEASURED,UNMEASURED - the source reports reasoning depth 17k-64k to 33k-120k chars and
  structural codegen bugs eliminated on Qwen3.8-27B at tq3_4s, plus about 12 MiB per family of
  size cost. Theirs, not ours.,UNMEASURED - the source measured 60 to 14 test-backend-ops failures on
  gfx1151 / ROCm 7.14 at about 2 percent decode cost. We do not build HIP in the configuration
  any number in this repository was measured on. |
| `0331-lane-152-GOAL-2-batch-2-three-lfm2-pre-tokenizer-has.patch` | UNMEASURED - no conversion of an LFM2.5 model was run, and the pre-tokenizer
  workflow was not executed. Syntax-checked with ast.parse, nothing more. |
| `0332-lane-152-pins-THREE-advance-with-reviews-upstream-ro.patch` | *(no Measured-effect trailer)* |
| `0333-lane-152-checker-F7-the-old-base-is-tag-b10454-not-b.patch` | *(no Measured-effect trailer)* |
| `0334-repo-env.json-WI-1540-tail-the-engine-repo-s-env-man.patch` | *(no Measured-effect trailer)* |
| `0335-WI-1580-sub-2-UPDATE-RUNBOOK-section-7-the-4.5-line-.patch` | *(no Measured-effect trailer)* |
| `0336-vulkan-land-the-MoE-expert-cache-provider-files-VK-M.patch` | UNMEASURED at this step - the files are not wired into the build yet; step 2 carries the build and guard proofs. |
| `0337-graph-derive-the-V-unpad-head-count-from-the-attenti.patch` | UNMEASURED on this rig. Their report is a live abort in ggml_reshape_3d during graph reserve for gpt-oss-20b (V head_dim 64 padded to 128, 64 q heads over 8 kv heads) with -ctk turbo3 -ctv turbo3; we do not run that model, so the fix is taken as a correctness guard and the claim is NOT restated as ours. |
| `0338-vulkan-wire-the-MoE-expert-cache-provider-into-the-b.patch` | UNMEASURED for throughput - this lane makes NO timed claim about the cache. The proofs carried are correctness proofs only: baseline exe mtimes banked before the ingest, then a rebuild with fresh mtimes, test-backend-ops and test-quantize-fns compared against the banked baseline, and arifi_sync_check.py green. |
| `0339-vulkan-complete-the-MoE-cache-port-the-56-provider-h.patch` | UNMEASURED for throughput; this lane makes NO timed claim about the cache. Correctness only: F-107 exe mtimes moved for all five targets (10:52 to 11:30), llama-cli grew 97090161 to 97433306 bytes, test-backend-ops 26213/26260 identical to the banked baseline, test-quantize-fns 6 failures identical, arifi_sync_check.py OK on the same 49/7/16/35/16/12 tallies. |
| `0340-sources-advance-the-turboquant-pin-f6124e914-7ebcbb0.patch` | UNMEASURED - a pin and its review change no runtime behaviour. |
| `0341-off-rig-backends-land-the-12-CUDA-Metal-SYCL-HIP-row.patch` | UNMEASURED AND UNPROVABLE ON THIS RIG - that is the point of the ruling. This code never compiles here (no CUDA/Metal/SYCL/HIP device or toolchain), so no F-107 exe proof and no backend-ops execution proof exists for it. A future lane with the hardware owes roughly 30 min to verify it; the debt is a named OWED row in the REPORT. |
| `0342-sources-pin_review-names-the-12-off-rig-shas-and-the.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0343-sources-correct-the-pin_review-the-contested-12-12-a.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0344-sources-pin_review-records-the-apply-count-claim-as-.patch` | UNMEASURED - a pin review changes no runtime behaviour. |
| `0345-base-b10481-25ae3a9b3-b10488-9d77fa172-series-regen-.patch` | *(no Measured-effect trailer)* |
| `0346-moe-cache-heat-protected-eviction-keeps-hot-experts-.patch` | UNMEASURED here. The source reports +12% TG (soft) / +7.5%
  (auto) on an RTX 5090 with Laguna S-2.1 - a CUDA measurement on hardware this
  estate does not have, quoted, NOT adopted as our number. |
| `0347-tests-initialise-non-contiguous-tensors-row-by-row-t.patch` | UNMEASURED as a delta - the sweep baseline 26213/26260 is
  re-run after this lane's build and compared case-for-case. The source notes
  the fix does NOT make their TQ4_1S k_v=1600 case pass; it unmasks a separate
  CUDA defect, which is off-rig for us. |
| `0348-sources-advance-the-turboquant-pin-7ebcbb0b6-d14e368.patch` | *(no Measured-effect trailer)* |
| `0349-sources-prisml-pin-HELD-deliberately-with-a-sharper-.patch` | *(no Measured-effect trailer)* |
| `0350-sources-ciru-re-derived-315-15-and-HELD-with-all-15-.patch` | *(no Measured-effect trailer)* |
| `0351-sources-advance-remotes.upstream.pin-25ae3a9b3-9d77f.patch` | *(no Measured-effect trailer)* |
| `0352-sources-correct-the-turboquant-pin_review-s-FALSE-bl.patch` | *(no Measured-effect trailer)* |
| `0353-sources-trim-the-e130aef60-blocker-to-the-ONE-item-t.patch` | *(no Measured-effect trailer)* |
| `0354-feat-kv-cache-port-flag-gated-mean-centering.patch` | UNMEASURED |
| `0355-feat-kv-cache-bind-calibration-to-exact-model.patch` | UNMEASURED (native toolchain execution denied in managed seat) |
| `0356-fix-server-keep-target-calibration-out-of-draft-cont.patch` | UNMEASURED (native toolchain execution denied in managed seat) |
| `0357-test-kv-cache-add-Vulkan-tensor-probe-and-bench-pure.patch` | harness selftest PASS; native build/model effect UNMEASURED (HQ-OWED) |
| `0358-sources-record-lane-158-PrismML-K-cache-disposition.patch` | source census corrected; native model effect UNMEASURED (HQ-OWED) |
| `0359-test-kv-cache-fail-close-default-OFF-binary-identity.patch` | clean fixture PASS; planted byte mismatch RED; native identity UNMEASURED (HQ-OWED) |
| `0360-test-kv-cache-assert-both-rotation-basis-polarities-.patch` | *(no Measured-effect trailer)* |
| `0361-fix-kv-cache-make-the-A-B-harness-able-to-launch-and.patch` | *(no Measured-effect trailer)* |
| `0362-fix-kv-cache-floor-check-the-model-named-by-m-not-th.patch` | *(no Measured-effect trailer)* |
| `0363-fix-kv-cache-wait-for-the-RAM-floor-between-arms-and.patch` | *(no Measured-effect trailer)* |
| `0364-fix-kv-cache-probe-the-layer-ids-the-calibration-act.patch` | *(no Measured-effect trailer)* |
| `0365-support-DFlash2.patch` | *(no Measured-effect trailer)* |
| `0366-dflash2-build-fixes-on-the-PR-27342-pick-dedupe-DFLA.patch` | *(no Measured-effect trailer)* |
| `0367-sources-register-buun-spiritbuun-buun-llama-cpp-as-a.patch` | *(no Measured-effect trailer)* |
| `0368-cuda-lift-TQ3_4S-kernel-set-from-tq3-master-14-files.patch` | UNMEASURED ON THIS RIG (no CUDA silicon); tq3-side references: RTX 3090 tg128 43.1 -> 47.4 t/s (PRMT vec_dot), Ampere decode +35% (3d3888c9f) |
| `0369-metal-lift-TQ3_4S-kernel-set-from-tq3-master-5-files.patch` | UNMEASURED ON THIS RIG (no Metal silicon); tq3-side reference: M3 Pro 27B decode ~4 t/s, ~27x over CPU (02d51f048) |
| `0370-metal-declare-the-tq3_rht-pipeline-getter-in-ggml-me.patch` | UNMEASURED ON THIS RIG (no Metal toolchain) |
| `0371-sync-move-base-b10488-b10524-regenerate-series-372-p.patch` | UNMEASURED (build+sweep follow in this lane) |
| `0372-spec-ingest-turboquant-MTP-boost-wave-effective-KV-b.patch` | UNMEASURED (their mtp-boost numbers are theirs; no arm run on this rig yet) |
| `0373-sync-re-pin-wave-3-sources-regenerate-series-374-pat.patch` | UNMEASURED (build+sweep follow) |
| `0374-arifi-sync-provenance-native-class-root-fix-sha-pinn.patch` | provenance audit flips ambient-red to enforcing; 289 grandfathered, 0 missing, new untrailered commits fail again |
| `0375-vulkan-subgroup-cooperative-TQ-mat-vec-tq3_1s-tq4_1s.patch` | 27B tq3_4s tg32 0.67->1.80 t/s; 0.5b tq4_1s tg64 44.7->126.7, tq3_1s 45.5->107.1; mat-vec kernel n=1 3.1-3.4x (780M, evidence lane-163) |
| `0376-vulkan-register-blocked-4-elem-lane-TQ-subgroup-mat-.patch` | *(no Measured-effect trailer)* |
| `0377-escha-native-Escha-W2-types-ESCHA2-55-ESCHA3-56-fuse.patch` | *(no Measured-effect trailer)* |
| `0378-escha-F-125-fix-full-64-bit-pair-sourcing-row-parity.patch` | Escha-W2 native decode goes from token salad to coherent; worst-projection weight correlation 0.9358, K=2 and K=3 both correct |
| `0379-escha-test-backend-ops-un-nest-ESCHA_MM-eval-cases-m.patch` | none yet (test registration only); unlocks the ncols duration curve |
| `0380-escha-vulkan-bound-every-escha_mm-dispatch-to-32-col.patch` | per-dispatch column count capped at 32, was ncols up to 2048 |
| `0381-escha-vulkan-column-blocked-escha_mm-one-weight-deco.patch` | pending on this commit, before-numbers are 1.15 and 2.12 ms per column |
| `0382-escha-vulkan-column-block-C-8-fall-back-to-the-one-c.patch` | 3.51x on escha3 17408x5120 at ncols=64, decode unchanged |
| `0383-escha-tests-cover-the-column-blocked-kernel-AT-MODEL.patch` | shipped C=8 prefill path now covered at real 27B shapes, 10/10 GREEN |
| `0384-escha-vulkan-one-hardware-f32-f16-convert-instead-of.patch` | UNMEASURED at commit time - per-weight instruction count drops ~21 to ~7 by inspection; decode/prefill deltas and the exhaustive rounding sweep are owed |
| `0385-escha-vulkan-subgroup-shuffle-Hadamard-14-barriers-p.patch` | UNMEASURED at commit time - barrier count per 128-input block drops 14 to 3 at subgroup 64 by inspection; correctness gate and speed delta owed |
| `0386-escha-vulkan-the-hardware-f32-f16-convert-must-be-fl.patch` | CORRECTNESS ONLY at this commit - removes a one-ulp truncation error affecting 25.6% of codewords that 1ee8e9ad7 would have shipped; speed deltas for this lever alone are measured next and reported separately. |
| `0387-escha-vulkan-ESCHA_SG_HADAMARD-defaults-OFF-the-leve.patch` | SG lever OFF by default; no speed change vs the shipped RTE-only configuration, and ~3% of prefill recovered versus having it on |
| `0388-escha-vulkan-f16-native-generator-decode-18-bit-exac.patch` | *(no Measured-effect trailer)* |
| `0389-escha-vulkan-two-more-levers-tried-and-REFUTED-by-me.patch` | *(no Measured-effect trailer)* |
| `0390-escha-vulkan-the-multi-column-threshold-was-disablin.patch` | *(no Measured-effect trailer)* |
| `0391-escha-vulkan-column-ladder-C2-C4-C8-C16-and-the-gene.patch` | *(no Measured-effect trailer)* |
| `0392-dflash-ingest-buun-s-DFlash2-adaptive-controller-bui.patch` | *(no Measured-effect trailer)* |
| `0393-escha-vulkan-prefill-is-OCCUPANCY-bound-coopmat-and-.patch` | *(no Measured-effect trailer)* |
| `0394-escha-vulkan-f16-activation-staging-built-correct-an.patch` | *(no Measured-effect trailer)* |
| `0395-dflash-reject-an-out-of-extent-draft-depth-instead-o.patch` | *(no Measured-effect trailer)* |
| `0396-spec-load-the-draft-model-from-the-same-variable-the.patch` | *(no Measured-effect trailer)* |
| `0397-F-136-ROOT-CAUSE-FIX-the-recurrent-snapshot-bank-n_r.patch` | *(no Measured-effect trailer)* |
| `0398-spec-ON-DEVICE-speculative-checkpoints-drafter-now-B.patch` | *(no Measured-effect trailer)* |
| `0399-spec-ring-rollback-re-enabled-DFlash2-depth-3-8.80-t.patch` | *(no Measured-effect trailer)* |
| `0400-rs-zero-on-clear-ROOT-FIX-cross-request-recurrent-st.patch` | *(no Measured-effect trailer)* |
| `0401-tests-zero_gather-CPY-GET_ROWS-Vulkan-probe-cases-se.patch` | *(no Measured-effect trailer)* |
| `0402-rs-zeroing-hunt-CLOSED-leak-is-backend-independent-C.patch` | *(no Measured-effect trailer)* |
| `0403-vulkan-F-136-residual-ROOT-FIX-in-graph_optimize-is_.patch` | *(no Measured-effect trailer)* |
| `0404-arifi-sync-base-move-b10524-9ee9fc04c-b10636-4d19b28.patch` | *(no Measured-effect trailer)* |
| `0405-b10636-bump-repair-restore-upstream-s-DOTS3NOTE-inde.patch` | *(no Measured-effect trailer)* |
| `0406-b10636-bump-repair-2-close-llm_graph_input_attn_k_ds.patch` | *(no Measured-effect trailer)* |
| `0407-dflash-a-rejected-draft-checkpoint-image-must-not-ki.patch` | UNMEASURED (no benchmark arm; the guard replaces a process abort on a path that needs a mismatched draft image to reach) |
| `0408-seat-46-MSVC-portability-root-fixes-for-the-ROCm-HIP.patch` | *(no Measured-effect trailer)* |
| `0409-server-re-add-the-shared_draft_devices-VRAM-accounti.patch` | *(no Measured-effect trailer)* |
| `0410-arifi-sync-base-move-b10636-4d19b2876-b10680-d7bd3bf.patch` | *(no Measured-effect trailer)* |
| `0411-vulkan-sync-check-follow-the-FA_TYPE-defines-into-b1.patch` | *(no Measured-effect trailer)* |
| `0412-b10680-bump-repair-common-fit.cpp-still-called-llm_f.patch` | *(no Measured-effect trailer)* |
| `0413-b10680-bump-repair-2-my-hunk-1-union-in-the-3aa479a0.patch` | *(no Measured-effect trailer)* |
| `0414-runbook-4.1b-COLLISION-MAP-law-President-2026-08-29-.patch` | *(no Measured-effect trailer)* |
| `0415-attribution-gap-fix-seat-48-re-key-the-sha-dead-prov.patch` | *(no Measured-effect trailer)* |
| `0416-runbook-2.2.1-RE-KEY-the-governed-sha-pointers-after.patch` | *(no Measured-effect trailer)* |
| `0417-attribution-wave-2-seat-48-zuijdwijk-is-a-LIVE-take-.patch` | *(no Measured-effect trailer)* |
| `0418-G12-re-audit-seat-48-52-of-the-180-are-PROVEN-not-fo.patch` | *(no Measured-effect trailer)* |
| `0419-R1-the-bank-plane-is-a-RUNTIME-INDEX-TENSOR-so-the-c.patch` | *(no Measured-effect trailer)* |
| `0420-R1-fix-guard-the-bank-index-tensors-on-ALLOCATION-no.patch` | *(no Measured-effect trailer)* |
| `0421-lane-178-the-ring-s-extra-graph-split-is-the-SPLIT-I.patch` | *(no Measured-effect trailer)* |
| `0422-ggml-backend-adopt-split-inputs-cap-64-as-the-defaul.patch` | *(no Measured-effect trailer)* |
| `0423-q2_0_g128-the-tie-predicate-is-exact-on-both-sides-n.patch` | *(no Measured-effect trailer)* |
| `0424-lane-180-the-ring-s-26-extra-graph-inputs-are-23-IDE.patch` | *(no Measured-effect trailer)* |
| `0425-lane-180-GATE-0-receipt-report-eHostVisible-in-the-v.patch` | *(no Measured-effect trailer)* |
| `0426-lane-180-ADOPT-share-ONE-s_wrow-view-by-default-29-i.patch` | *(no Measured-effect trailer)* |
| `0427-vulkan-perf-logger-must-not-abort-when-an-async-copy.patch` | *(no Measured-effect trailer)* |
| `0428-lane-188-SHIPPING-KV-precision-tail-store-side-quant.patch` | *(no Measured-effect trailer)* |
| `0429-lane-188-refuse-the-tail-on-MLA-caches-the-write-ord.patch` | *(no Measured-effect trailer)* |
| `0430-lane-192-FIX-a-the-precision-tail-survives-a-prefix-.patch` | *(no Measured-effect trailer)* |
| `0431-lane-194-q4_0-F16-Vulkan-cpy-shader-F16-KV-tail-comp.patch` | *(no Measured-effect trailer)* |
| `0432-lane-194-the-CPU-reference-for-quant-F16-dup-which-d.patch` | *(no Measured-effect trailer)* |
| `0433-lane-194-name-the-compose-type-in-the-tail-banner-th.patch` | *(no Measured-effect trailer)* |
| `0434-lane-195-multi-segment-flash-attention-the-ggml-API-.patch` | *(no Measured-effect trailer)* |
| `0435-lane-195-LEG-0-the-regression-leg-the-branch-was-mis.patch` | *(no Measured-effect trailer)* |
| `0436-lane-196-Design-B-BUILT-heterogeneous-split-k-FA-per.patch` | *(no Measured-effect trailer)* |
| `0437-lane-196-promote-the-tail-ON-banner-to-WARN-llama-se.patch` | *(no Measured-effect trailer)* |
| `0438-lane-197-honor-spec-draft-p-min-in-the-DFlash2-sampl.patch` | *(no Measured-effect trailer)* |
| `0439-lane-198-gate-Q5_K-n-1-and-Q4_K-n-5-off-the-q8_1-MMV.patch` | *(no Measured-effect trailer)* |
| `0440-runbook-4.1b-duty-3-a-named-collision-is-decided-by-.patch` | *(no Measured-effect trailer)* |
| `0441-vulkan-tiled-concat-transpose-for-the-delta-net-conv.patch` | CONCAT dispatch -12.79% (-15.66 ms per ub512 prefill graph);
  end-to-end ceiling +0.18%, below serve resolution on this box |
| `0442-runbook-4.1-warn-that-series-regen-is-destructive-th.patch` | none (documentation) |
| `0443-arifi-sync-root-fix-the-destructive-series-regen-and.patch` | no engine behaviour changed and no shipped default moved; tooling and
  documentation only, verified by the 14-case suite including four fail-closed negative cases. |
| `0444-vulkan-label-the-concat-transpose-prior-art-numbers-.patch` | none - comment text only, no generated code differs. |
| `0445-arifi-sync-name-the-incoming-ref-in-full-when-the-pr.patch` | none - one diagnostic message string; 14/14 tests still green. |
| `0446-arifi-sync-make-an-UNREGISTERED-measured-win-refuse-.patch` | tooling and documentation only; no engine behaviour changed and no shipped default moved |
| `0447-arifi-sync-close-the-two-live-false-negatives-in-the.patch` | tooling and documentation only; no engine behaviour changed and no shipped default moved |
| `0448-arifi-sync-discharge-eleven-REGISTRATION-OWED-rows-a.patch` | tooling and documentation only; no engine behaviour changed and no shipped default moved |
| `0449-arifi-sync-search-anchors-in-SOURCE-only-so-the-mani.patch` | correctness only, no throughput claim |
| `0450-arifi-sync-bring-the-manifest-s-own-_schema.anchors-.patch` | tooling and documentation only; a stale schema fact corrected, no code path changed |
| `0451-arifi-sync-recognize-correctness-only-as-no-effect-v.patch` | tooling and documentation only; no engine behaviour changed and no shipped
  default moved. |
| `0452-vulkan-create-the-ROCmFP4-FAST-mat-mat-pipelines-and.patch` | *(no Measured-effect trailer)* |
| `0453-vulkan-dedicated-IQ4_XS-mat-vec-shader-K-quant-style.patch` | *(no Measured-effect trailer)* |
| `0454-vulkan-IQ3_S-mat-vec-16-invocations-per-superblock-a.patch` | *(no Measured-effect trailer)* |
| `0455-vulkan-IQ3_S-mat-vec-split-gate-becomes-NUM_COLS-3-N.patch` | *(no Measured-effect trailer)* |
| `0456-vulkan-create-the-ROCmFP4-FAST-q8_1-MMVQ-mat-vec-pip.patch` | *(no Measured-effect trailer)* |
| `0457-vulkan-keep-the-ROCmFP4-FAST-q8_1-MMVQ-path-to-n-2.5.patch` | *(no Measured-effect trailer)* |
| `0458-vulkan-enforce-ROCmFP4-MMVQ-width-gate-before-batch-.patch` | *(no Measured-effect trailer)* |
| `0459-vulkan-exclude-ROCmFP4-MMVQ-n-4-tie-width.patch` | *(no Measured-effect trailer)* |
| `0460-vulkan-isolate-ROCmFP4-MMVQ-to-n-3-and-n-5.patch` | *(no Measured-effect trailer)* |
| `0461-vulkan-correct-lane-209-backend-coverage-comment.patch` | *(no Measured-effect trailer)* |
| `0462-speculative-size-drafts-from-measured-acceptance-spe.patch` | *(no Measured-effect trailer)* |
| `0463-server-do-not-re-verify-replayed-draft-tokens-after-.patch` | *(no Measured-effect trailer)* |
| `0464-vulkan-q6_k-mat-vec-reads-its-scales-from-the-block-.patch` | NEUTRAL on gfx1103 - op-level q6_k median ON/OFF 0.9796 inside a
  78-row untouched control at 0.9990, p10-p90 [0.952, 1.053]; serve TIED on all
  six prefill/decode cells; output byte-identical, 1 sha per prompt over 12
  launches. No speed win is claimed. |
| `0465-arifi-sync-register-the-three-lane-209-mechanisms-th.patch` | *(no Measured-effect trailer)* |
| `0466-test-backend-ops-adversarial-MUL_MAT-data-patterns-f.patch` | none - test coverage only |
| `0467-vulkan-ROCmFP4-MMVQ-becomes-opt-in-GGML_ARIFI_ROCMFP.patch` | none on the seated line (no ROCmFP4 tensors); on ROCmFP4 files
  this defers a measured +26.9% drafted decode behind an opt-in env var |
| `0468-arifi-sync-register-the-rank-7-ROCmFP4-MMVQ-mechanis.patch` | none - manifest registration only |
| `0469-docs-the-public-trail-for-the-b10680-kernel-wave-lan.patch` | UNMEASURED - documentation only; no source, shader, default or
  build input is modified, and every performance figure is transcribed from a
  cited prior measurement rather than taken here |
| `0470-docs-scope-the-power-plan-claim-to-the-runs-that-ban.patch` | UNMEASURED |
| `0471-docs-apply-the-six-independent-check-fixes-to-the-b1.patch` | UNMEASURED |
| `0472-arifi-sync-base-move-b10680-d7bd3bfca-b10819-6a1a922.patch` | UNMEASURED - tooling and governed records only; no engine behaviour changed by this commit. |
| `0473-b10819-sync-repair-six-merge-defects-that-stopped-th.patch` | build FAIL at 6/305 -> RC=0 with all five targets linked (llama-cli, llama-server, llama-quantize, test-backend-ops, test-quantize-fns); runtime effect of the speculative.cpp union is measured by the F-141 content gate and the served DFlash2 arm, not by this build. |
| `0474-vulkan-tq_rotate-is-a-src1-reformat-restore-the-TQ-M.patch` | test-backend-ops goes from ABORT at case 12343 (GGML_ASSERT y_needs_reformat, rc 0xC0000409, no result at all) to a completed sweep; build stays RC=0 on all five targets. |
| `0475-metal-union-the-dropped-upstream-FA-machinery-back-i.patch` | *(no Measured-effect trailer)* |
| `0476-cuda-union-upstream-s-mul_mat_id_needs_sync-predicat.patch` | *(no Measured-effect trailer)* |
| `0477-ci-a-backend-matrix-that-compiles-what-this-rig-cann.patch` | *(no Measured-effect trailer)* |
| `0478-arifi-sync-restore-the-2-space-JSON-indent-the-R17-r.patch` | *(no Measured-effect trailer)* |
| `0479-ci-the-test-quantize-fns-gate-was-decorative-and-Met.patch` | *(no Measured-effect trailer)* |
| `0480-cuda-restore-the-fused-MoE-cache-matvec-on-upstream-.patch` | *(no Measured-effect trailer)* |
| `0481-ci-make-the-link-contract-real-and-run-test-backend-.patch` | *(no Measured-effect trailer)* |
| `0482-arifi-sync-R13B-s-two-b10819-repair-commits-become-a.patch` | *(no Measured-effect trailer)* |
| `0483-sycl-the-MMVQ-launchers-declared-one-pair-of-ranges-.patch` | *(no Measured-effect trailer)* |
| `0484-ci-each-job-now-proves-the-unioned-file-itself-produ.patch` | *(no Measured-effect trailer)* |
| `0485-metal-replace-the-garbled-library-loader-with-upstre.patch` | UNMEASURED - off-rig backend, union-by-mechanism-read; compile verification PENDING the first metal-macos run |
| `0486-ci-the-cpu-ubuntu-job-runs-protected-win-validate-so.patch` | UNMEASURED - CI workflow only; never run, the branch is not pushed |
| `0487-arifi-sync-protected-win-learns-a-symbols-class-a-re.patch` | UNMEASURED - tooling and governed records only; no engine behaviour changed by this commit. |
| `0488-arifi-sync-the-symbols-guard-ignores-commented-out-b.patch` | UNMEASURED - tooling and governed records only; no engine behaviour changed by this commit. |
| `0489-docs-put-the-integration-A-B-numbers-into-the-public.patch` | UNMEASURED - documentation of measurements already banked; no engine behaviour changed by this commit. |
| `0490-docs-state-the-power-plan-and-RAM-as-pre-registered-.patch` | UNMEASURED - documentation wording; no engine behaviour changed by this commit. |
| `0491-arifi-sync-sources.json-post_move_tip-re-keyed-to-th.patch` | UNMEASURED - register only |
| `0492-ggml-flash-attention-segment-count-moves-to-op_param.patch` | *(no Measured-effect trailer)* |
| `0493-vulkan-FA-dequant-KV-f16-scratch-is-a-runtime-gate-d.patch` | R19D-ENGAGEMENT-LEGS.md D1 (ON vs OFF, one binary): prefill clean +8.08% [+1.63%, +14.52%]; this commit = the ON default, expected 0 vs the engine of record (same path) |
| `0494-arifi-sync-base-move-b10819-6a1a922d2-b10825-9e0e220.patch` | *(no Measured-effect trailer)* |
| `0495-arifi-sync-rekey-3-protected-win-manifests-to-the-b1.patch` | *(no Measured-effect trailer)* |
| `0496-arifi-sync-scope-subcommand-F-161-the-ingest-A-B-sco.patch` | *(no Measured-effect trailer)* |
| `0497-provenance-route-B-G7-append-only-R24-s-58-classific.patch` | *(no Measured-effect trailer)* |
| `0498-spec-runtime-switch-for-the-DFlash-encoder-fusion-LL.patch` | DOCUMENTED-TAKE. Seat DFlash2 decode -0.98% CI95 [-1.16%, -0.80%] fused vs legacy; U-IQ4XS +0.05% [-0.56%, +0.67%]; U-Q3KXL -0.87% [-2.50%, +0.77%]. The wave's INERT control on the same pair moved +3.71% [+0.80%, +6.61%] on decode, so none of these clears the build floor. |
| `0499-vulkan-RDNA3-mul_mat_vec_id-rows-runtime-selectable-.patch` | DOCUMENTED-TAKE. 20 op rows, 3 rounds each: x0.979 to x1.031 against the upstream default of 4 - no setting separates from the ladder's own spread. Correctness 80/80 per setting, TBOID GREEN. No serve cell ran (no MoE file that fits this box was staged). |
| `0500-kv-cache-the-PLE-n-gram-history-lookup-becomes-a-run.patch` | DOCUMENTED-TAKE, mechanism UNMEASURED. Receipt grep: 0 hits in 10 server logs - the mechanism never ran. The same engine pair still moved DFlash2 prefill +8.03% [+5.56%, +10.49%] and plain decode -2.20% [-2.79%, -1.60%], which is therefore pure build floor, not this switch. Witness owed: test-llama-archs -a qwen4exp under both envs (PREREG-r15c4-kv-ngram-index.md). |
| `0501-vulkan-MUL_MAT_ID-B-staging-receipt-and-a-loud-asser.patch` | DOCUMENTED-TAKE. Receipt proved the path cannot engage on this GPU: 0 K-padded rows in 7 logs (no coopmat2 on gfx1103). Op ladder 48 MUL_MAT_ID ops, 3 rounds per arm, x0.976 to x1.032, no CI claimed at n=3. No default changed, so there is nothing to lose here. |
| `0502-vulkan-UMA-read-back-takes-the-direct-memcpy-only-wh.patch` | DOCUMENTED-TAKE. Mechanism cell (ONE binary, direct vs auto) read decode +0.21% CI95 [-0.26%, +0.69%] - flat, and one of the two quietest cells in the whole wave. The four c3 files land within -1.88% and -0.03% on DFlash2 decode. Cell c1 NOT BANKED (resumed across the 21:30 job). Still owed: a host-cached unified-memory device, where the probe would actually select direct. |
| `0503-ggml-add-GGML_TYPE_SX8-57-the-S-X8-v4.3-CPU-decoder-.patch` | DOCUMENTED-TAKE. The harmless cell (the port present but unreachable) read DFlash2 decode +0.43% CI95 [-0.47%, +1.33%] and plain decode -5.33% [-12.26%, +1.60%]; the same pair moved plain PREFILL -8.28% [-11.00%, -5.56%], the wave's largest inert move, which is the build floor and not this port. TBO ladder x0.991 to x1.033. The S-X8 file cell never ran: no S-X8 file exists and the decoder is CPU-only. |
| `0504-ggml-port-jtrefon-TBQ3_0-TBQ4_0-at-ids-58-59-with-th.patch` | DOCUMENTED-TAKE. The harmless cell (types present, no launch names them) read DFlash2 decode +0.60% CI95 [-1.28%, +2.47%] and DFlash2 prefill +8.15% [+5.38%, +10.92%], the latter pure build floor. The KV MECHANISM WAS NEVER MEASURED: the baseline arm refused the type ("Unsupported cache type: tbq4_0") and the candidate arm was aborted by the schedule horizon. What ran is engine-vs-engine at KV-on-host, +1.60% [+1.30%, +1.90%] on a 0.515 t/s crawl. Owed: candidate-engine tbq4_0 vs candidate-engine q8_0; the baseline-engine design is impossible because the base engine does not know the type. |
| `0505-docs-protected-wins-the-R18-switch-table-eight-DOCUM.patch` | none - registry and manifest only, no code path touched |

## Unclassified

These commits fell through every grouping rule in `arifi_sync.py`. That is surfaced
rather than silently bucketed - add a rule when a new source appears.

- `6827c9462` patches: generate the full auditable 38-patch series from git
- `4de985763` tools: add arifi-sync - the currency, series and upstream-bump driver
- `24b2c17c2` patches: move banked PrismML source patches out of the generated series directory
- `57cddcfd3` patches: make the series generator implement its own convergence rule
- `43f38b822` patches: stop the manifest deriving anything from excluded commits
- `d9f1a2117` port: Q1_0 repack scaffolding + Arm NEON/DP GEMV-GEMM kernels
- `38232d775` port: native x86 Q2_0 vec_dot (AVX512-VNNI / AVX-VNNI), drop scalar fallback alias
- `a2fa737a6` port: x86 AVX512-VNNI repack GEMV/GEMM for Q1_0 and Q2_0
- `56ac77b01` fix(ggml-cpu): parameterize Q2_0 GEMM activation stride by QK2_0/QK8_0 - g64 NaN
- `a01ed569b` fix(ggml-cpu): same g64 stride defect in the ported x86 Q2_0 vec_dot
- `d1aed38fc` ggml: reserve the ArifiLabs type-ID blocks at the enum itself
- `667ee75c4` ggml: ROCmFP4/ROCmFPX weight formats behind GGML_ARIFI_ROCMFPX_FORMATS
- `2c27c91a9` llama: ROCmFPX file types, quantizer wiring and the gguf-py registry
- `392a34232` tools: record GGML_ARIFI_ROCMFPX_FORMATS in the build-recipe snapshot
- `65bd34a31` feat: --arifi-profile and a CPU-only VNNI-repack startup advisory
- `8c51d177c` sync: currency GREEN — TurboQuant checkouts pinned at their lane-110C-reviewed shas (c26cbdffc, ba52ad107), rocmfpx pin advanced to db6844d9b after 6-commit review (HIP pinned-staging noted for the HIP-deltas slot)
- `c16526b3b` fix(ggml-cpu/x86): don't pass __m256i by value across the repack GEMV/GEMM ABI boundary (Windows 0xC0000005)
- `3d9668cdc` ggml: TurboQuant TQ3_1S/TQ4_1S weight formats behind GGML_ARIFI_TURBO_WEIGHT_QUANTS
- `c77c324ab` ggml-cpu/x86: use the row-stride parameter in repack GEMV stores instead of the row count
- `b6a36788e` docs(FINDINGS): F-09 - the repack/GPU trade-off is a compute-placement choice, not a bug
- `38096996f` docs(FINDINGS): record why the repack auto-enable was NOT shipped
- `68e47b9fc` provenance: catch the silent trailer-block break instead of only reporting it
- `9fd35df85` gitattributes: pin .githooks to LF so the hook runs on Linux and macOS
- `009a8da6c` githooks: force a normalized blob for the commit-msg hook
- `db6df2b50` docs(FINDINGS): F-09 exit 1 is the direction - dual residency, not a forced choice
- `46aca7c0c` repack: dual residency - GGML_ARIFI_VNNI_REPACK=2 keeps prefill on the GPU AND repacks decode
- `eae07ca1c` repack: dual residency measured - both wins held; fix the hot-path cost that ate the first one
- `222946b67` repack: fix the shadow tensor COUNT in the accounting line, and strengthen the claim it supports
- `50132f88c` repack: CPU kernels for the 128-group ternary format
- `be8dc6239` tests: direct equivalence test for the ternary repack kernels
- `f932903cd` F-06: measure the GPU tier on one binary, and count the placement the prompt claim rested on
- `8d9bb0275` registry: the dual-residency prompt mechanism is measured now, not ASSUMED
- `69472e927` registry: our DSpark verdict was never ours -- the drafter cannot load on this fork
- `3d5aaa619` speculative: DFlash/DSpark drafters have no vocab -- take the mask token from metadata
- `3e9c41c50` expert cache: stride the bundle by the PADDED matrix size -- unblocks our own brain
- `4191dcb9e` expert cache: POWERINFER_EXPERT_HEATMAP -- measure access skew before building a pin policy
- `b7a23f113` ours/1163cb34939fe4a9cb07aec034c5954144497ae9.patch
- `94342d355` 1-core-types/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `b44ed2c15` 1-core-types/0002-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `73f446772` 2-cpu-reference/0001-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `b3bbb9bfa` 2-cpu-reference/0007-ggml-cpu-declare-turbo3_cpu_wht_group_size-extern-no.patch
- `589a7c403` 2-cpu-reference/0008-merge-fix-post-merge-build-and-crash-issues.patch
- `272997fe5` 3-vulkan/0001-vulkan-metal-hip-add-TurboQuant-kernel-support.patch
- `e55c1c656` 3-vulkan/0002-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch
- `9bcb0e98b` 3-vulkan/0003-fix-close-complete-audit-findings-7-port-regressions.patch
- `299db0849` 3-vulkan/0004-vulkan-reconstruct-supports_op-after-rebase-merge-da.patch
- `a03716427` 3-vulkan/0006-vulkan-restore-TQ4_1S-weight-type-wiring-lost-in-the.patch
- `f8543b7cc` 3-vulkan/0007-vulkan-add-the-TQ3_1S-weight-type.patch
- `71ffdc04e` 3-vulkan/0008-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch
- `9dd1c2bbb` 3-vulkan/0013-vulkan-TQ-rotated-matmul-shader-groundwork-A-side-lo.patch
- `b9dc4aa27` 3-vulkan/0015-vulkan-create-the-TQ-activation-rotate-pipeline.patch
- `ea05afd41` 3-vulkan/0017-vulkan-rotate-the-staged-activation-copy-in-place-dr.patch
- `d07741312` 3-vulkan/0018-vulkan-register-the-TQ-rotated-mul_mm_id-pipelines.patch
- `832c023e9` 3-vulkan/0021-vulkan-fix-SET_ROWS-block-decomposition-for-turbo-we.patch
- `03c278de6` 4-other-backends/0006-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch
- `310ae9400` 4-other-backends/0007-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch
- `d88e8cb6b` 4-other-backends/0008-hip-add-mixed-f16-bf16-q8_0-fattn-vec-instances-to-t.patch
- `dbcf8548b` 4-other-backends/0009-cuda-revert-GQA-ratio-dispatch-to-comparison-form-fi.patch
- `87df12365` 4-other-backends/0010-cuda-restore-Volta-GQA-modulo-dispatch-over-revert-i.patch
- `5f5e1ab00` 4-other-backends/0015-cuda-rework-MoE-expert-cache-execution.patch
- `809460ff4` 4-other-backends/0016-cuda-fix-MoE-cache-capacity-accounting.patch
- `21d90bf23` 4-other-backends/0017-cuda-fix-MoE-cache-scratch-budget-replacement.patch
- `5864d15fa` 4-other-backends/0018-cuda-bound-MoE-MMV-tail-row-reads.patch
- `c147d10b3` 4-other-backends/0019-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `08939b5d4` 4-other-backends/0020-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `6557fa761` 4-other-backends/0021-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `bc361c40f` 4-other-backends/0022-cuda-prefer-generic-MMV-for-compatible-MoE-cache-nod.patch
- `45ab61bfb` 4-other-backends/0023-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch
- `fb5b90a3c` 4-other-backends/0024-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `332a072ef` 4-other-backends/0025-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `9c317c8ba` 4-other-backends/0026-cuda-bound-automatic-MoE-cache-admission.patch
- `0913e1e2a` 4-other-backends/0027-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `b8b367e35` 4-other-backends/0028-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `eb626764a` 4-other-backends/0029-common-surface-MoE-cache-activation.patch
- `d85aad250` 4-other-backends/0030-cuda-accelerate-complete-MoE-cache-pools.patch
- `e82403113` 4-other-backends/0031-cuda-report-oversized-MoE-cache-nodes.patch
- `01ffa9432` 4-other-backends/0032-cuda-clarify-MoE-cache-session-diagnostics.patch
- `46feff51d` 4-other-backends/0033-cuda-report-MoE-cache-pair-residency.patch
- `f4378080d` 4-other-backends/0034-cuda-pack-MoE-cache-dispatch-inputs.patch
- `533945073` 4-other-backends/0037-metal-port-TQ_NO_ROTATE-escape-hatch-from-stash-opt-.patch
- `015dceb55` 4-other-backends/0039-moe-cache-enable-HIP-backend-by-removing-no-op-stubs.patch
- `04c4b6861` 4-other-backends/0040-moe-cache-count-dispatch-contention-bypasses-P1.patch
- `427a576f7` 4-other-backends/0041-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `bd09adac9` 4-other-backends/0042-moe-cache-audit-fixes-H1-F1-F2-A1.patch
- `9eeea2669` 4-other-backends/0043-moe-cache-fix-registry-build-I1-follow-up.patch
- `0ba03a42c` 4-other-backends/0044-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `c9c6cf39a` 4-other-backends/0045-cuda-fix-HIP-MoE-cache-compatibility-and-q8_1-sums.patch
- `847374ac3` 4-other-backends/0046-moe-cache-add-logging-at-silent-failure-points-acros.patch
- `210a1bcb1` 4-other-backends/0048-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch
- `224612665` 4-other-backends/0049-sycl-drop-the-duplicated-nthreads-declarations-in-fa.patch
- `28d179679` 4-other-backends/0050-fattn-vec-split-the-turbo-K-dot-at-D-128-to-stop-the.patch
- `60d681354` 5-models-server/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `260a9ed02` 5-models-server/0003-cli-break-interactive-loop-on-stdin-EOF-fixes-hot-sp.patch
- `9f80737fb` 5-models-server/0004-tools-expose-TQ3_1S-TQ4_1S-in-quantize-table-and-lla.patch
- `08ee85906` 5-models-server/0007-llama-support-DeepSeek-V4-tensor-split.patch
- `46560bd09` 5-models-server/0008-llama-mirror-DS4-q_a-kv-down-projections-in-tensor-s.patch
- `b276a9b1c` 5-models-server/0009-glm-dsa-guard-lightning-indexer-Hadamard-rotation-wh.patch
- `7b53e6bce` 5-models-server/0010-laguna-add-arch-tables-llm_arch-KV-keys-tensors-and-.patch
- `7404be191` 5-models-server/0011-laguna-add-model-class-hparams-wiring-vocab-pre-toke.patch
- `a4bd003d3` 5-models-server/0013-laguna-MoE-down-proj-f16-overflow-guard-CUDA-GQA-rat.patch
- `07020aa30` 5-models-server/0014-cuda-first-class-MoE-path-plain-f16-FA-hygiene-on-GB.patch
- `44f2b42b7` 5-models-server/0015-cuda-sum-MoE-expert-outputs-on-decode-n_tokens-1.patch
- `cafe4dd4a` 5-models-server/0016-laguna-deduplicate-definitions-vs-the-early-snapshot.patch
- `94ac9ae9f` 5-models-server/0017-dflash-fix-DSpark-tensor-meta-accessor-after-laguna-.patch
- `905841018` 5-models-server/0021-common-support-the-DSpark-sidecar-resolution-26458.patch
- `a3788c9b5` 5-models-server/0037-merge-fix-post-merge-build-and-crash-issues.patch
- `c5dfc46e2` 5-models-server/0041-moe-cache-route-fit-probing-and-test-session-calls-t.patch
- `6a83305b8` 5-models-server/0048-llama-bench-document-pw-prefetch-weights-in-help-and.patch
- `23097bb01` 5-models-server/0050-llama-bench-remove-stale-prefetch-weights-documentat.patch
- `8f14a5bf8` 5-models-server/0051-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch
- `b2d9654bf` 6-tests-build/0001-tests-re-add-test-turbo-quant.c-round-trip-test.patch
- `61dfa65d9` 6-tests-build/0002-tests-port-fork-turbo-backend-and-quantize-fns-test-.patch
- `a96866f7e` 6-tests-build/0003-fix-close-complete-audit-findings-7-port-regressions.patch
- `b7413eca0` 6-tests-build/0004-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch
- `c3dc63f66` 6-tests-build/0006-ggml-webgpu-add-support-for-f16-repeat-26307.patch
- `01f443747` 6-tests-build/0010-tests-restore-DSV4_HC_COMB-eps-sweep-lost-in-the-244.patch
- `9beb5eac6` 6-tests-build/0011-tests-cast-float-printf-args-in-test-turbo-quant-arm.patch
- `45ba6c62c` 6-tests-build/0012-vulkan-add-the-TQ3_1S-weight-type.patch
- `937a37fcd` 6-tests-build/0013-vulkan-reject-MUL_MAT_ID-for-the-TurboQuant-weight-t.patch
- `fbd324a4b` 6-tests-build/0014-ggml-cpu-remove-the-per-call-mallocs-from-the-TurboQ.patch
- `a68de0674` 6-tests-build/0015-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch
- `2809bc8f8` 6-tests-build/0016-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch
- `edb277526` 6-tests-build/0017-tests-port-11-DSv4-shaped-TQ-MUL_MAT-cases-from-sync.patch
- `813569dbd` 6-tests-build/0019-cuda-fix-MoE-cache-capacity-accounting.patch
- `dbd475c03` 6-tests-build/0020-common-preserve-implicit-MoE-cache-provider-settings.patch
- `1d04b0084` 6-tests-build/0021-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `c670e80f2` 6-tests-build/0022-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `8e1434a27` 6-tests-build/0023-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `beb67774f` 6-tests-build/0024-cuda-parallelize-MoE-cache-fills-on-capable-multi-GP.patch
- `0efbf18c0` 6-tests-build/0025-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `94d0dc9e1` 6-tests-build/0026-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `d86ae83e9` 6-tests-build/0027-cuda-bound-automatic-MoE-cache-admission.patch
- `04e547feb` 6-tests-build/0028-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `9c9678f3a` 6-tests-build/0029-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `128843895` 6-tests-build/0030-common-surface-MoE-cache-activation.patch
- `58186f250` 6-tests-build/0031-cuda-accelerate-complete-MoE-cache-pools.patch
- `937f8e689` 6-tests-build/0032-cuda-report-oversized-MoE-cache-nodes.patch
- `2311225e9` 6-tests-build/0033-tests-initialise-non-contiguous-tensors-row-by-row.patch
- `a883472a2` 6-tests-build/0034-tests-cover-turbo-KV-flash-attention-at-head-dim-256.patch
- `4f82d7209` 6-tests-build/0037-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `149f50068` 6-tests-build/0038-moe-cache-route-fit-probing-and-test-session-calls-t.patch
- `39e772360` 6-tests-build/0039-moe-cache-route-context-eligibility-probe-and-remain.patch
- `919d9ac2c` 6-tests-build/0040-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `9146eb967` 6-tests-build/0041-moe-cache-fix-auto-asymmetric-MLA-guard-and-test-ski.patch
- `804513bb8` 6-tests-build/0042-test-moe-cache-accept-IGPU-devices-not-just-GPU.patch
- `1211a72c3` 6-tests-build/0043-test-moe-cache-make-cache-shared-budget-UMA-aware-wi.patch
- `3ef5882f3` 6-tests-build/0044-test-moe-cache-save-and-restore-the-CUDA-device-arou.patch
- `59704c358` 6-tests-build/0045-tests-tell-ctest-that-77-means-skip-for-test-moe-cac.patch
- `9cac10cf7` resolved/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch
- `3521632c9` resolved/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch
- `5b27d7f80` resolved/0005-merge-close-TurboQuant-parity-gaps.patch
- `f833d6f09` resolved/0036-common-surface-MoE-cache-activation.patch
- `679adff17` resolved/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch
- `f2b5185c3` resolved/0004-fix-close-complete-audit-findings-7-port-regressions.patch
- `816008089` resolved/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch
- `1165056f3` resolved/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch
- `88ab9f2e5` resolved/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch
- `cb1f93bae` resolved/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch
- `a981a981e` resolved/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch
- `2382acd55` resolved/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch
- `a9b7a2c68` resolved/0006-fix-close-complete-audit-findings-7-port-regressions.patch
- `da16d9e37` resolved/0022-DeepseekV4-MTP-DSpark-25784.patch
- `3670aa599` resolved/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch
- `646b6276b` resolved/0029-server-harden-shared-draft-device-placement.patch
- `93f370d47` resolved/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch
- `4fa19b037` resolved/0026-cuda-rework-MoE-expert-cache-execution.patch
- `4419e16d3` resolved/0033-server-account-for-MTP-placement-in-fit-reservations.patch
- `e2cf94791` resolved/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `08bf89d53` resolved/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch
- `975016049` resolved/0039-model-Muse-Glimmer-Support-26841.patch
- `b360f9016` resolved/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch
- `53a283c41` resolved/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch
- `fe6868c43` resolved/0046-llama-remove-dead-MSA-indexer-scaffolding.patch
- `b1003d7fc` resolved/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch
- `83d1eaf74` resolved/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch
- `9347b0638` resolved/0005-test-fix-some-CI-errors-26415.patch
- `2b2b070a4` resolved/remaining-conflict-markers
- `f8bcd8e5d` layer7/0001-WIP-add-TurboQuant-KV-cache-types-turbo3-turbo4.patch
- `2a48cb5a0` layer7/0002-Update-GGMLQuantizationType-and-LlamaFileType-enums-.patch
- `678ffff4e` layer7/0003-ggml-port-TurboQuant-core-quant-types-and-CPU-kernel.patch
- `10fb9f724` layer7/0005-docs-add-rebase-plan-and-TurboQuant-recipes.patch
- `cc1e0274f` layer7/0007-gguf-py-dedupe-stacked-merge-artifacts-in-constants..patch
- `79c7b3139` layer7/0008-docs-add-KV-cache-quantization-guide-update-rebase-p.patch
- `1639cbdba` layer7/0009-vulkan-port-fork-241-wave64-ballot-fix-correct-rpc-o.patch
- `5982381ab` layer7/0010-merge-close-TurboQuant-parity-gaps.patch
- `0be8bc913` layer7/0011-metal-restore-lost-kernel-templates-add-offline-shad.patch
- `b44787449` layer7/0014-laguna-tool-call-whitespace-tolerance-docs-and-test-.patch
- `e8aa54219` layer7/0019-fix-apply-rebase-audit-fixes-stack-overflow-quality-.patch
- `636fd40ab` layer7/0020-docs-add-TurboQuant-project-overview-to-AGENTS.md-an.patch
- `75fefde2f` layer7/0021-cuda-rework-MoE-expert-cache-execution.patch
- `c80a85f3b` layer7/0022-docs-update-MoE-cache-validation.patch
- `2905c4c26` layer7/0023-common-preserve-implicit-MoE-cache-provider-settings.patch
- `aed527bac` layer7/0024-server-harden-shared-draft-device-placement.patch
- `6c6760aa3` layer7/0025-cuda-tune-MoE-cache-CPU-overlap-automatically.patch
- `41d862b38` layer7/0026-cuda-adapt-MoE-cache-admission-to-device-capability.patch
- `564cc862d` layer7/0027-cuda-align-MoE-cache-pool-allocation-with-fit.patch
- `357a5efd2` layer7/0028-docs-update-MoE-cache-validation.patch
- `c9c7035be` layer7/0029-cuda-fuse-cached-MoE-SwiGLU-rows.patch
- `12a3d36de` layer7/0030-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch
- `ef6e6fd45` layer7/0031-cuda-bound-automatic-MoE-cache-admission.patch
- `ee88e1a42` layer7/0032-cuda-reject-undersized-automatic-MoE-cache-slabs.patch
- `e3c652d07` layer7/0033-cuda-enable-multi-token-MoE-cache-in-forced-modes.patch
- `db701a958` layer7/0034-common-surface-MoE-cache-activation.patch
- `45db02335` layer7/0035-docs-fix-MoE-cache-benchmark-verbosity.patch
- `b3891c20d` layer7/0036-cuda-accelerate-complete-MoE-cache-pools.patch
- `d71db0a6b` layer7/0037-cuda-report-oversized-MoE-cache-nodes.patch
- `f7a9260e5` layer7/0038-cuda-clarify-MoE-cache-session-diagnostics.patch
- `9566c98bc` layer7/0039-docs-record-MoE-cache-workload-convergence.patch
- `f87f4ee26` layer7/0040-cuda-report-MoE-cache-pair-residency.patch
- `05c8996e3` layer7/0041-docs-document-what-the-test-suites-do-and-do-not-cov.patch
- `03955dd69` layer7/0044-moe-cache-provider-registry-with-per-scheduler-selec.patch
- `f54d46959` layer7/0046-moe-cache-allow-automatic-mode-on-a-single-device-wh.patch
- `34740bfc5` layer7/0048-docs-document-INFO-level-MoE-cache-disable-reasons-i.patch
- `f847582f0` layer7/0049-docs-rename-CUDA-MOE-CACHE.md-to-MOE-CACHE.md-and-re.patch
- `84705341b` layer7/0050-docs-correct-MOE-CACHE.md-against-the-implementation.patch
- `c3424dbb6` layer7/0051-ggml-split-the-turbo3-group-size-declaration-from-it.patch
- `3ea6d9a71` resolve 8 committed conflict markers on merit
- `6304b0545` fix the merge so it builds: 5 files, 6 defects
- `8bc896908` merge repairs: seven defect classes, and one the compiler could never have reported
- `7b892ad44` ROCmFPX on Vulkan: the shader half, ported. NOT YET WORKING - it segfaults in execution.
- `9cc15b897` currency: register ALL SEVEN forks, so nothing can drift unseen again
- `1e49fd129` series: base and the branch it describes are ONE pair, and the linearity rule is now a gate
- `4360cbd2c` hygiene: the code-review skill taught a type-ID map the arbitration deleted, and the UI was fetched from a moving tag
- `f34e0452e` contract: tq3 is the first family we must renumber, so write down why before anyone ports it
- `194368dc8` sources: honest pins - turboquant re-pinned off a dead lineage, tq3 main-understates note REFUTED, five type-id contract collisions recorded (lane-143)
- `f5843df9a` sources: lane-143 self-refutation - the turbo3 ODR 'defect' does not exist in our tree (GGML_API already carries extern on all four branches); prisml pin REVERTED to keep the BEHIND signal live; FLAG E severity re-derived from loader code
- `4de1f6b22` sources r2 (checker BLOCK F1-F6): ciru 160 was 145 commits of rocmfpx history - corrected to a 7-15 bracket, no-ancestor premise refuted, GOAL-3 ciru review performed, rocmfpx.ref revert, prisml contradiction removed, FLAG E sensitivity added
- `e0829d363` series: point base.ref at the LINEAR branch - recut-linear/2026-08-17 is the byte-identical flatten of the blessed union branch, so the series is replayable at last (lane-147)
- `e1b3aa2f6` series: disclose in the record itself that the flatten absorbed 17 empty commits - content unchanged, history count changed (lane-147 checker finding 4)
- `8dd605718` ggml: port the tq3 TQ3_4S family codec at ids 48-51 (TYPE-ID-ALLOCATION 3.1.1) - Taken-from turbo-tan/llama.cpp-tq3@58ad80ffb
- `fdf2df3ee` tests: cover the tq3 family in test-quantize-fns and test-backend-ops (kilobyte-scale proof before the 13 GB run)
- `1f430eb40` contract: tq3 rows 48-51 implemented behind GGML_ARIFI_TURBO_WEIGHT_QUANTS, plus the three corrections the port forces (their 200 = our TURBO2_0; six ids, four destinations; token_embd is Q6_K)
- `dff6e4b12` tests: tq3_4s/tq3_4se skipped in test-quantize-fns with the measured reason (E3M5 scale caps at 0.492, harness data needs 1.4); tq3_0 given documented 3.5bpw bounds
- `90dd9cb1a` tools: gguf-retag-tq3 - lands LAST, after the coherence proof, per TYPE-ID-ALLOCATION 3.1.1. Decides ambiguous id 46 by measured byte span; five refusals, no in-place path
- `61e5d9db5` checker fixes: choose_index restored verbatim (inverted tie-break + 2 drifted constants, encoder-only); tq3 numeric block gives TQ3_4S real proof and pins every dot mapping; drop redundant GGML_API redecl
- `764e4c4e4` retag: verify geometry per TENSOR for every mapped id, not a file-level vote on 46 only - fixes silent rewrite of mixed/ambiguous files and of unguarded 36/200; guard empty and overlapping tables
- `a669040c5` retag README: per-tensor geometry check for every mapped id, ten refusals, byte-level assertion
- `ed06c5a0b` lane-144 GOAL 2 step 1: TQ3_4S (id 48) Vulkan port - dequant + mat-vec + mat-vec_id shaders, wired
- `06e52de42` lane-144: sweep ROCmFP4/ROCmFPX in test-backend-ops all_types (six identifiers, never listed before)
- `b82d7b86b` lane-144 CHECKER FIX: TQ3_4S was missing from the MUL_MAT_ID staging-overflow guard
- `455c5f5c5` lane-145: Q6_0_ROCMFPX Vulkan fix - implement the 6-bit unpack (block struct was 34B vs CPU 26B; qs read as whole signed bytes). 60/60 MUL_MAT + 24/24 GET_ROWS on Vulkan0, was 50/60 + 20/24.
- `82683a20f` lane-145 checker finding 7: delete block_rocmfpx_fp6_packed16 + its A_TYPE_PACKED16 define (26B block has no valid uint16 view; nothing reads it). Re-verified 60/60 MUL_MAT, 24/24 GET_ROWS, 12/12 MUL_MAT_ID.
- `0a9838950` test-backend-ops: cover the CPY types supports_op names that all_types omits
- `4fc3ab415` lane-148 defect A ROOT FIX: Vulkan turbo3 centroid LUT was stale at 5 sites - CPY turbo3->f32 ERR 8.4302e-5 -> OK. CPU/CUDA/Metal/SYCL all carry CENTROIDS_3BIT (-0.190207,-0.118786,-0.066822,-0.021663); Vulkan alone carried a table whose midpoints do not match nearest_centroid_3bit(). copy_to_quant.comp's TM decision boundaries were derived from the stale table and are corrected too.
- `397d241e3` lane-148 defect B ROOT FIX: FA_TYPE_TURBO2/3/4_0 GLSL spec-constant ids were stale (43/44/47) while enum ggml_type puts them at 200/201/202. Host passes the raw enum, so both switches fell through: fa_block_elems() returned its 1u default and dequantize4() returned vec4(0), zeroing V and the whole FA output. FLASH_ATTN_EXT -p turbo 0/1728 -> 1728/1728.
- `e6b62374d` lane-148 F-69 sibling coverage: test-backend-ops FLASH_ATTN_EXT now covers TURBO2_0 too. Vulkan supports_op has always claimed turbo2 for FA but no test case existed, so its stale FA_TYPE id was invisible. 1728 -> 2592 cases, all PASS.
- `a61547983` lane-148 checker findings 4+5 FIXED: turbo4 SET_ROWS was 0/21 declined behind a green 'Backend Vulkan0: OK' banner and turbo2 had no SET_ROWS case at all. Root cause: no cpy_turbo{2,4}_0_f32 copy-from-quant shader, so the tests' read-back leg declined. Added turbo2/turbo4 dequantize()/dequantize4()/get_dm(), the two spv+pipeline registrations, both supports_op and the DISPATCH switch (a third hand-synced pair - supports_op alone aborts with 'Missing CPY op'), and a test_set_rows_turbo2 class. SET_ROWS_TURBO2/3/4 now 21/21 EXECUTED each.
- `582b5e281` lane-149 F-110 guard (rung b): arifi_sync_check.py + CMake wiring + 16 ARIFI-SYNC markers. Checks 49 LUT pairs, 16 FA_TYPE ids, 35 QUANT_K block sizes, 4 type-list SETs, FA K/V coverage; unregistered mirrors FAIL
- `a1e795f2f` lane-149 checker-driven hardening of arifi-sync-check: FA_TYPE/QUANT_K literal spellings, ordinal (not line) pins, per-definition completeness, any array type, recursive shader walk, scalar-mirror class, expected-count assertions
- `9a046618c` lane-149 F-112 second half (checker finding 11): per-op tallies name every op whose cases were ALL declined, so a zero-coverage family cannot hide inside a green full sweep
- `88b0d6834` lane-150 step 1: TQ4_1S read-back leg wired - CPY(tq4_1s,f32) now EXECUTES, 291/291 -> 293/293
- `c2d57011b` lane-150 step 2: TQ4_1S write leg EXECUTES - SET_ROWS_TQ4_1S 17 cases/0 executed -> 17/17 PASS, and the inherited 5.0 waive is retired
- `bb71dcd05` lane-150 step 3 (root fix for the hole that hid the whole class): per-op x TYPE zero-coverage tally
- `2afa34d24` lane-150 checker-driven fixes: type_bucket prefix-boundary bug + dispatch block-size assert + the tallys limits named in code
- `78c9eabd2` lane-151 GOAL 6 hygiene: drop 648 tracked _lane-out debris files (incl. an embedded shadow.git with commit receipts) and gitignore the dir; genericize estate paths in docs/, licenses/, tools/gguf-retag-tq3
- `31f755192` lane-151 GOAL 9: reserve tq3's three destination-less serialized ids (37 TQ3_4SV -> 52, 31 TQ3_1S_AP1 -> 53, 44 TQ3_1S -> 54) as RESERVED-not-implemented in docs/TYPE-ID-ALLOCATION.md 3.1.2; geometry marked NOT DERIVED, no enumerators added, retag-tool refusals unchanged
- `4a4ea78fb` lane-151 GOALs 6+7: README refreshed to b10453 canonical arifi-main + the seven-ingested-sources table (two honest zeroes named); tq3 and ciru credits added; licenses/README.md gains the full Taken-from audit incl. the tq3 license GAP; UPDATE-RUNBOOK.md written (O(delta) recipe, THREE-flag configure, guards, the standard)
- `e65e4b365` lane-151 GOAL 2: sources.json base.ref -> arifi/main (the canonical branch is now the series base) + series regen over the full canonical range: 323 patches, series check PASS, replay onto 4df29be4f identical outside patches/series
- `e501e049a` lane-151 GOAL 5: gitignore .tokensave/ - the fork repo now carries its own code graph (63344 nodes, 154451 edges over 3407 files) and the index is per-machine, never committed
- `920408c8b` lane-151 CHECKER ROOT FIX: series check now asserts every patch is a COMMITTED BLOB, not just a file on disk. The byte-compare ran against the working directory, so untracked patches made it green while a fresh clone would die at git am - the defect that hid 33 missing patches twice. Help text corrected to match what the code does
- `a41a73f79` lane-151 CHECKER FIXES (docs): UPDATE-RUNBOOK 4.5 no longer asserts a self-containment standard nothing on this estate meets - the four MinGW runtime DLLs are named and staged, and the measured stripped-PATH failure is recorded as identical for the raw build tree and the previous engine. licenses/README.md audit header stops claiming a purely mechanical trailer derivation: the tq3 row is a SUBJECT-line citation git never parses as a trailer, which is exactly why a trailer-only sweep would have missed the one real license gap
- `8fb9910a9` lane-151 HQ DIRECTIVE: RETAIN the licenses instead of flagging them. tq3-MIT, prisml-MIT and ciru-MIT extracted verbatim from this repo's OWN fetched remotes (git show refs/remotes/<r>/<head>:LICENSE). REFUTES my own GOAL-6 finding and a year-old one: I wrote 'no clone remains on the estate' and Phase 0 wrote 'PrismML has no LICENSE' - both were absence claims checked against the wrong artifact while the remotes sat fetched in this very repository. All seven registered sources audited at once. thecodacus STAYS open: no remote, no text, nothing invented
- `a8c59ea7b` licenses: thecodacus-MIT retained from the public GitHub fork - the last open attribution CLOSED (no local remote != no source; President's correction 2026-08-18)
- `b555f90cc` lane-152 GOAL 1: upstream bump b10453 -> b10481, the FIRST live execution of UPDATE-RUNBOOK 2. 348 patches replayed onto 25ae3a9b3, 4 conflicts resolved by hand, series regenerated to 329, zero merges.
- `90a032846` lane-152: what the FIRST live run of UPDATE-RUNBOOK section 2 corrected - one build break, two arifi_sync defects, the runbook itself, and the last license row
- `2e62d54f7` lane-152 GOAL 2 batch 1: ciru's Vulkan SPIRV-Headers fallback, tq3's f32 hybrid-SSM state gates, rocmfpx's -ffast-math guard
- `978af8507` lane-152 GOAL 2 batch 2: three lfm2 pre-tokenizer hashes that only rocmfpx had - and the reason they must be pre-computed
- `327d64620` lane-152 pins: THREE advance with reviews (upstream, rocmfpx, tq3), TWO deliberately stay BEHIND (prisml, ciru)
- `c706b58a0` lane-152 checker F7: the old base is tag b10454, not b10453 - correction of record
- `51b205a44` repo-env.json (WI-1540 tail): the engine repo's env manifest - PowerShell-only law (F-107), three-flag configure (F-111), cmd.exe redirection (F-108), type-id FLAG E, series discipline, runbook pointer
- `379c08ed3` WI-1580 sub-2: UPDATE-RUNBOOK section 7 - the 4.5 line was stale. lane-153 staged models/engines/arifi-b10481-union/ from a verified build of canonical, smoked from the staged copies, and flipped current-state.json + artifact-catalog.json, so 4.5 HAS now been executed once, by hand. The bump distinction is KEPT because it is still true: no bump has ever driven staging end to end (lane-152's bump refused at leg 2). Narrow edit - the section-2 and section-3 paragraphs around it are untouched. Note for the record: the lane-153 REPORT cites this text at UPDATE-RUNBOOK.md:238-244; it actually lives at :308-323
- `0c462d769` graph: derive the V-unpad head count from the attention output tensor, not from hparams
- `41cef48ff` sources: advance the turboquant pin f6124e914 -> 7ebcbb0b6 with pin_review_2026_08_18_lane154
- `1c8854991` off-rig backends: land the 12 CUDA/Metal/SYCL/HIP rows unbuilt, per the HQ ruling of 2026-08-18
- `e578d5fb7` sources: pin_review names the 12 off-rig shas and their BUILD-UNPROVEN status (HQ condition 3)
- `8663db6f5` sources: correct the pin_review - the contested 12/12 apply-conflict count is no longer the justification (checker F1)
- `533db1837` sources: pin_review records the apply-count claim as WRONG, not disputed (checker F1, resolved against me)
- `393c4936f` base: b10481/25ae3a9b3 -> b10488/9d77fa172 + series regen 346 (lane-156 upstream bump)
- `4e32775f4` moe-cache: heat-protected eviction keeps hot experts resident (turboquant tail)
- `7f0850d21` tests: initialise non-contiguous tensors row by row (turboquant row f58ee0e97)
- `d162c8674` sources: advance the turboquant pin 7ebcbb0b6 -> d14e36827 with a dated lane-156 pin_review
- `64fb40705` sources: prisml pin HELD deliberately with a sharper reason + the v6 K-cache-mean-center verdict (lane-156 GOAL 4)
- `eb8eb8c23` sources: ciru re-derived 315 -> 15 and HELD with all 15 reviewed (lane-156 GOAL 5)
- `9fedd180a` sources: advance remotes.upstream.pin 25ae3a9b3 -> 9d77fa172 (b10488) with its review
- `0c5469b59` sources: correct the turboquant pin_review's FALSE blocker for e130aef60 (checker F8, BLOCKING)
- `1e4449451` sources: trim the e130aef60 blocker to the ONE item that survives (checker pass 2, advisory 1)
- `df4902760` feat(kv-cache): port flag-gated mean centering
- `69fe7e3bb` feat(kv-cache): bind calibration to exact model
- `6c4f7e148` fix(server): keep target calibration out of draft contexts
- `7088baef0` test(kv-cache): add Vulkan tensor probe and bench-pure A/B harness
- `ba9d0ab82` sources: record lane-158 PrismML K-cache disposition
- `f30e9e964` test(kv-cache): fail-close default-OFF binary identity
- `a410fc9b3` test(kv-cache): assert both rotation-basis polarities against the resolved attn_rot_k
- `2772ec9e7` fix(kv-cache): make the A/B harness able to launch, and actually enforce the RAM floor it claims
- `3024bf175` fix(kv-cache): floor-check the model named by -m, not the largest .gguf on the command line
- `6be16f3e2` fix(kv-cache): wait for the RAM floor between arms, and pin -fit off so the A/B arms match
- `3a7f42685` fix(kv-cache): probe the layer ids the calibration actually has, and let an existing calibration be reused
- `b526efa91` support DFlash2
- `44102a64f` dflash2: build fixes on the PR #27342 pick - dedupe DFLASH_BLOCK_SIZE enum/hparam/kv-map (fork DFlash1 originals keep authority, default 16) + ml. reference fix (the shadow the fork's own comment documents)
- `2ec737cc9` sources: register buun (spiritbuun/buun-llama-cpp) as a greedy-ingestion watch source - President ruling, VBR/KV + DFlash2-for-3.8 lineage
- `8225dbd0e` cuda: lift TQ3_4S kernel set from tq3/master (14 files)
- `541fa6374` metal: lift TQ3_4S kernel set from tq3/master (5 files + alloc hook)
- `4d38e0eb7` metal: declare the tq3_rht pipeline getter in ggml-metal-device.h
- `190d0ca04` sync: move base b10488 -> b10524, regenerate series (372 patches)
- `9705d1c38` spec: ingest turboquant MTP-boost wave + effective-KV bench reporting
- `31a5f37d3` sync: re-pin wave-3 sources + regenerate series (374 patches)
- `feb038bdc` arifi-sync: provenance native-class root fix - sha-pinned grandfather ledger + native Origin convention
- `44aef02d2` escha: native Escha-W2 types (ESCHA2=55/ESCHA3=56) + fused GGML_OP_ESCHA_MM, CPU + Vulkan
- `c5982591f` escha: F-125 fix - full 64-bit pair sourcing + row-parity correction (K=2 AND K=3)
- `a39174005` escha: test-backend-ops — un-nest ESCHA_MM eval cases + multi-chunk ncols coverage + perf ncols curve
- `1b6bfaf58` escha vulkan: bound every escha_mm dispatch to 32 columns (F-124 host-freeze guard)
- `abca2e782` escha vulkan: column-blocked escha_mm - one weight decode serves 4 columns (part 2)
- `f8716ab56` escha vulkan: column block C=8 + fall back to the one-column kernel below a full block
- `1f1864aba` escha tests: cover the column-blocked kernel AT MODEL DEPTH (ncols=9, real 27B shapes)
- `e85e34b95` escha vulkan: one hardware f32->f16 convert instead of the 15-instruction software RTE
- `d0bd8fe5e` escha vulkan: subgroup-shuffle Hadamard - 14 barriers per input block become 3
- `753d0eb64` escha vulkan: the hardware f32->f16 convert must be float16_t, NOT packHalf2x16 (RTZ)
- `6d3bd166c` escha vulkan: ESCHA_SG_HADAMARD defaults OFF - the lever is refuted by measurement
- `5c3582287` escha vulkan: f16-native generator - decode +18%, bit-exact
- `450b8cfef` escha vulkan: two more levers tried and REFUTED by measurement - occupancy and packed-pair
- `49fc8b3d3` escha vulkan: the multi-column threshold was disabling speculation - decode 3.96 -> 5.60 t/s
- `b9ce31520` escha vulkan: column ladder C2/C4/C8/C16, and the generator table measured and REFUTED
- `170d8c627` dflash: ingest buun's DFlash2 adaptive controller - built, gated, and defaulted OFF by measurement
- `3cc0c7913` escha vulkan: prefill is OCCUPANCY-bound - coopmat and wider rungs both refuted, and the profile that said otherwise was lying
- `8d5a484c4` escha vulkan: f16 activation staging - built, correct, and refuted; the real prefill win was the build
- `a1351b4b0` dflash: reject an out-of-extent draft depth instead of silently clamping it, and block the mask token
- `3d3bc4fd4` spec: load the draft model from the same variable the log prints, and add an env-gated DFlash verify trace
- `2974512cb` spec: ON-DEVICE speculative checkpoints - drafter now BEATS plain (8.02 vs 4.5 t/s, byte-clean)
- `c030c10e2` spec: ring rollback re-enabled - DFlash2 depth 3 = 8.80 t/s (2.0x plain), 0 degenerate
- `7c2a235ec` tests: zero_gather + CPY/GET_ROWS Vulkan probe cases; server: depth>cap clamps with warning instead of load abort (F-136 arc)
- `8d7876f51` rs zeroing hunt CLOSED: leak is backend-independent (CPU repro), in-graph rs_z zero never lands on the live cache tensor; zero-on-clear promoted belt->contract. Env-gated discriminators: PDH hash, plane0/sl/rl belt scoping, zero-gather-out, hybrid set_input trace
- `44d9f0bfe` arifi-sync: base move b10524/9ee9fc04c -> b10636/4d19b2876 (runbook 2.2 manual path); upstream pin advanced as the same pair; prisml + tq3 pins HELD with 3 reviews recorded
- `7a40b423e` b10636 bump repair: restore upstream's DOTS3NOTE indexer arch + the two TAG_LLAMA_SEQ_ID_NEG TODOs that a whole-file --theirs resolution reverted. checkout --theirs takes stage 3 (the WHOLE file), not just the conflicting hunks - upstream's 4-line b10524..b10636 delta to this file was lost with it. GLM_DSA is absent here PRE-EXISTING (also absent on lane165-pre-bump-arifi-main), so it is NOT restored by this commit and stays a President item.
- `0bccc45a3` b10636 bump repair 2: close llm_graph_input_attn_k_dsa_iswa::can_reuse - the union resolution at ef642481d kept both sides' bodies but only one shared 'return res; }' tail, nesting every following definition inside it (build FAILED, 18 errors)
- `c41e4eb61` dflash: a rejected draft checkpoint image must not kill the server process
- `df731fd27` seat-46: MSVC portability root-fixes for the ROCm/HIP build (6 latent bugs: -fPIC on Windows, _MSC_VER fp16 macro parens, math.h/unistd.h guards, ssize_t->streamsize, dllimport on static lib) - build-rocm GREEN gfx1103, Vulkan build confirmed no-op
- `197f8f343` server: re-add the shared_draft_devices VRAM accounting on top of common_fit_extra_model (multi-device only)
- `e7c4ffd7a` arifi-sync: base move b10636/4d19b2876 -> b10680/d7bd3bfca (LATEST upstream tag, President mandate 2026-08-29); upstream pin advanced as one pair
- `cc31c8aa1` vulkan sync-check: follow the FA_TYPE defines into b10680's new fa_types.glsl - reading only flash_attn_base.glsl found ZERO ids and silently turned checks 2 and 5 into no-ops (the exact F-110 class the guard exists for); now 16 FA_TYPE ids + 12 FA K/V types again
- `53270c9b1` b10680 bump repair: common/fit.cpp still called llm_ffn_exps_block_regex(idx), which b10680 renamed to llm_ffn_block_regex(idx, ffn_regex) - the fork's moe_cache code auto-merged past the rename and the build failed. Same body, LLM_FFN_EXPS_REGEX passed explicitly, so behaviour is identical.
- `047f211f8` b10680 bump repair 2: my hunk-1 union in the 3aa479a04 (support DFlash2) resolution kept selector_top_k from BOTH sides - duplicate member, build FAILED. One copy kept. The union trap the runbook already names, this time on a declaration rather than a shared block terminator.
- `5020dfb1e` runbook 4.1b: COLLISION MAP law (President 2026-08-29) - never lose our patches; A/B only where upstream delta intersects our changes; collision list = required bump-report line
- `7d300807c` attribution gap-fix (seat-48): re-key the sha-dead provenance record + close the license/source-inventory gaps
- `9fa4df299` runbook 2.2.1: RE-KEY the governed sha pointers after every base move - mechanize G1 so the class cannot silently return
- `e93e5d437` attribution wave 2 (seat-48): zuijdwijk is a LIVE take source, not a dormant remote - promote it, and record the G7 trailer debt instead of amending
- `ec4946ed6` G12 re-audit (seat-48): 52 of the 180 are PROVEN not fork-authored - and none of them is unlicensed
- `6b2a7943b` R1: the bank plane is a RUNTIME INDEX TENSOR, so the copies go AND graph reuse stays
- `68979d342` R1 fix: guard the bank index tensors on ALLOCATION, not on rs_r1
- `b40d34455` lane-178: the ring's extra graph split is the SPLIT-INPUTS CAP, not a CPU fallback - runtime-selectable so one binary is both arms
- `d7109fafe` ggml-backend : adopt split-inputs cap 64 as the default
- `381951370` lane-180: GATE 0 receipt - report eHostVisible in the vk memory logger, not just eDeviceLocal
- `a1a138000` lane-180 ADOPT: share ONE s_wrow view by default - 29 identical per-layer split inputs collapsed
- `0be0c0508` lane-188: SHIPPING KV precision tail (store-side) - quantized body + exact F16 ring
- `ad42d9028` lane-188: refuse the tail on MLA caches - the write-order detector cannot see the mla/lid idxs paths
- `049697c79` lane-192 FIX (a): the precision tail survives a prefix-cache rewind
- `f4dcd3f19` lane-194: q4_0 -> F16 Vulkan cpy shader + F16 KV tail compose (OPT-IN, default OFF)
- `ba591ac56` lane-194: the CPU reference for quant -> F16 dup, which did not exist
- `4c7ce3f62` lane-194: name the compose type in the tail banner - the receipt that stops a vacuous fidelity green
- `a04638027` lane-195: multi-segment flash attention - the ggml API + CPU reference + its gate
- `34f4553d0` lane-195: LEG 0 - the regression leg the branch was missing, plus the sinks condition stated
- `dc0d0a9bf` lane-196: Design B BUILT - heterogeneous split-k FA (per-query source selection), the Vulkan dispatch
- `50e38d7ec` lane-196: promote the tail-ON banner to WARN - llama-server drops INFO at default verbosity
- `abebad0c6` lane-197: honor --spec-draft-p-min in the DFlash2 sampled selector path - the gate was greedy-branch-only, so the seated serve line (temp 0.7) ignored the flag entirely; hoisted to the shared site, greedy duplicate removed, flag-unset path untouched (p_min defaults 0.0)
- `111973f5d` lane-198: gate Q5_K (n>1) and Q4_K (n>=5) off the q8_1 MMVQ mat-vec shader on AMD - measured on the live speculative-verify graph (780M, proprietary driver): MMVQ per-dispatch cost steps 2.0-2.3x (Q4_K) / 1.55x (Q5_K) from NUM_COLS 4 to 5 while the f32 dequant shader rises 1.4-1.6x, and Q5_K is faster on the f32 shader at every measured n>1 (0.73-0.88x). GGML_VK_FORCE_MMVQ / GGML_VK_DISABLE_MMVQ still override; n==1 path untouched. Evidence: research/local-inference/lane-evidence/2026-09-01-lane-198/TABLE.json
- `58df3f895` runbook 4.1b duty 3: a named collision is decided by MECHANISM READ + THREE ARMS (theirs / ours / union), never by one number - President 2026-09-02
- `c07ed9c7c` runbook 4.1: warn that `series regen` is destructive-then-abort, and require a pre-landing baseline
- `93177b4d9` arifi-sync: root-fix the destructive series regen, and mechanize protected-win preservation
- `d2433f1c9` arifi-sync: name the incoming ref in full when the protected-win preflight refuses
- `df835d571` arifi-sync: make an UNREGISTERED measured win refuse the update path
- `41668a312` arifi-sync: close the two live false negatives in the protected-win gate, and mechanize the baseline boundary
- `57795cc38` arifi-sync: discharge eleven REGISTRATION OWED rows, and close two live holes in the guard itself
- `0ebb9cc08` arifi-sync: search anchors in SOURCE only, so the manifest cannot vouch for itself
- `4511d79f7` arifi-sync: bring the manifest's own `_schema.anchors` fact up to the source-only rule
- `725a5ff97` arifi-sync: recognize `correctness only` as no-effect vocabulary
- `ccd09f526` speculative: size drafts from measured acceptance (--spec-draft-adaptive)
- `e1b6361f5` server: do not re-verify replayed draft tokens after a checkpoint restore
- `f425b2c79` arifi-sync: register the three lane-209 mechanisms this integration lands
- `b3f61e883` test-backend-ops: adversarial MUL_MAT data patterns for the ROCmFP4 MMVQ widths
- `b4080d300` arifi-sync: register the rank-7 ROCmFP4 MMVQ mechanism, default-OFF and all
- `23035e5b1` arifi-sync: base move b10680/d7bd3bfca -> b10819/6a1a922d2, and the governed sha re-key that follows it
- `aa8f8feca` b10819 sync: repair six merge defects that stopped the tree from building
- `02893efe4` metal: union the dropped upstream FA machinery back in - the tree did not compile
- `f16d2b3b4` cuda: union upstream's mul_mat_id_needs_sync predicate with our TQ arm
- `68be5f571` arifi-sync: restore the 2-space JSON indent the R17 register edits reformatted
- `9dcd2e8a7` cuda: restore the fused MoE-cache matvec on upstream's fusion kernel (WI-1699)
- `cbe00c86f` arifi-sync: R13B's two b10819 repair commits become also_commits of the mechanisms they keep alive
- `618e0769a` sycl: the MMVQ launchers declared one pair of ranges and launched with another
- `085d2065f` metal: replace the garbled library loader with upstream's per-kind loader plus an ArifiLabs library kind
- `eaf341d27` arifi-sync: protected-win learns a symbols class - a registered kernel entry point must have a DEFINITION, not a mention (WI-1700)
- `fe53a790f` arifi-sync: the symbols guard ignores commented-out bodies, and the CUDA/Metal seed gaps the check found are closed (WI-1700 FIX-1/FIX-2)
- `283a58842` arifi-sync: sources.json post_move_tip re-keyed to the lane-214 integration tip
- `1d9c22856` ggml: flash-attention segment count moves to op_params[5] - it collided with upstream's n_kv_max in [4]
- `568d60751` arifi-sync: base move b10819/6a1a922d2 -> b10825/9e0e22059 (R22 read, R27 execution); 552/552 replayed clean, 0 merges, 0 hunk collisions, marker scan 0, protected wins PASS; R20 slot fix + R19 gate (default ON per R19D) carried Lane: lane-220
- `44897b55e` arifi-sync: rekey 3 protected-win manifests to the b10825 line (R27 check 05:1x: validate PASS only with this rekey; committed so the built line validates) Lane: lane-220
- `b7c2d98ba` arifi-sync: scope subcommand (F-161) - the ingest A/B scope derived from the collision map (upstream files x our files x hunk overlap x speed paths) = sanity|named|full; UPDATE-RUNBOOK 4.1b.4 scope law; selfcheck Lane: lane-220
- `7e2e3782c` provenance route B (G7 append-only): R24's 58 classifications recorded in native-grandfather (55 ARIFI, remapped to the b10825 shas) + pending-trailers (2 FORK, 1 UPSTREAM); 46 fork-authored tooling/manifest commits grandfathered; provenance PASS; series regen 496 patches
- `f9746db7b` spec: runtime switch for the DFlash encoder fusion (LLAMA_DFLASH_FUSED_INJECT), both injection paths kept, default LEGACY on this fork
- `0fe8c7f49` kv-cache: the PLE n-gram history lookup becomes a runtime switch - seq_pos index by default, the cell scan behind LLAMA_KV_NGRAM_INDEX=0
- `174ead5ca` ggml: add GGML_TYPE_SX8 (57), the S-X8 v4.3 CPU decoder, retagged from the author's 41, plus its retag/cross-check tooling
- `3e3eb64d0` ggml: port jtrefon TBQ3_0/TBQ4_0 at ids 58/59 with the type-id arbitration, plus the SET_ROWS device guard (WI-1694 half A)
- `4a8c1536e` docs + protected-wins: the R18 switch table - eight DOCUMENTED-TAKE runtime switches registered with their measured effect

