# Engine G conflict recovery

EXECUTOR-STAMP: provider=openai model=gpt-5.6-sol effort=high binding=validated-argv runtime=unverified doubt=RUNTIME_IDENTITY_UNVERIFIED .

## Disposition

CHECKED: All 51 entries in `<home>\AppData\Local\Temp\turbo.log` were classified. The result is 28 CASCADE, 23 UPSTREAM DRIFT, and 0 CROSS-FORK DEPENDENCY.

CHECKED: No feature represented by the 51 failed patch entries remains intentionally omitted from the recovered source. Patches that did not receive a new commit were already present, superseded by a later patch, a subset of a landed patch, or a duplicate entry.

CHECKED: The recovery is a HANDOFF, not a closed update of the original worktree ref. The sandbox denied writes to the linked worktree Git metadata. The 35 wrapper-created commits are on the writable shadow ref `recovery` at `6564107bef7e66bd2f2556aec86ad8b386600a1d`, while ordinary `git rev-parse HEAD` remains `11e91492aa5d1aab6371ed32f0e14f2641c763ed`. The source files in this worktree contain the cumulative recovery changes.

CHECKED: The verified transfer artifact is `_lane-out/engineG-conflicts.bundle`. `git bundle verify` reported it is OK, contains `refs/heads/recovery` at `6564107bef7e66bd2f2556aec86ad8b386600a1d`, and requires base `11e91492aa5d1aab6371ed32f0e14f2641c763ed`.

ASSUMED: An authorized coordinator can import or fast-forward from that bundle to make the recovery commits visible through the original linked-worktree Git metadata.

## Classification of all 51 failures

CHECKED: The classifications below come from patch contents, apply/reverse checks, three-way attempts, target-file inspection, and the wrapper receipts listed later.

| # | Failed patch | Class | Resolution |
|---:|---|---|---|
| 1 | `3-vulkan/0009-vulkan-add-mul_mat_vec_id-for-the-TurboQuant-weight-.patch` | UPSTREAM DRIFT | Landed, receipt 1 |
| 2 | `3-vulkan/0010-feat-add-Vulkan-turbo3-KV-cache-pipeline-support-a49.patch` | CASCADE | Landed after Vulkan prerequisites, receipt 7 |
| 3 | `3-vulkan/0011-fix-correct-Vulkan-turbo3-pipeline-wiring-after-ff8b.patch` | CASCADE | Landed, receipt 2 |
| 4 | `3-vulkan/0012-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch` | CASCADE | Equivalent wiring already present through the duplicate layer entry and later Vulkan work; no additional delta |
| 5 | `3-vulkan/0019-vulkan-dispatch-the-TQ-rotated-mul_mm_id-path.patch` | UPSTREAM DRIFT | Landed, receipt 6 |
| 6 | `3-vulkan/0020-vulkan-apply-the-TQ-MUL_MAT_ID-size-gate-only-to-the.patch` | UPSTREAM DRIFT | Landed, receipt 3 |
| 7 | `4-other-backends/0001-cuda-add-TurboQuant-MMVQ-WHT-inner-quant-CUDA-kernel.patch` | UPSTREAM DRIFT | Current CUDA side already contained the feature; three-way conflict selection produced no delta |
| 8 | `4-other-backends/0002-sycl-add-TurboQuant-WHT-and-vec-kernels.patch` | UPSTREAM DRIFT | Feature already present; remaining SYCL marker union finalized in receipt 35 |
| 9 | `4-other-backends/0003-vulkan-metal-hip-add-TurboQuant-kernel-support.patch` | UPSTREAM DRIFT | Feature already present; residual marker unions were cleaned in later receipts |
| 10 | `4-other-backends/0004-fix-close-complete-audit-findings-7-port-regressions.patch` | UPSTREAM DRIFT | Landed, receipt 8 |
| 11 | `4-other-backends/0005-metal-restore-lost-kernel-templates-add-offline-shad.patch` | UPSTREAM DRIFT | Landed, receipt 9 |
| 12 | `4-other-backends/0012-metal-implement-DeepSeek-V4-hyper-connections-26459.patch` | UPSTREAM DRIFT | Landed, receipt 10 |
| 13 | `4-other-backends/0013-metal-add-F16-support-for-bin-ops-26465.patch` | UPSTREAM DRIFT | Already present; reverse apply check succeeded |
| 14 | `4-other-backends/0014-metal-add-SILU_BACK-25982.patch` | UPSTREAM DRIFT | Already present; reverse apply check succeeded |
| 15 | `4-other-backends/0035-cuda-gate-fused-TQ-mul_mat-paths-on-contiguous-src1-.patch` | UPSTREAM DRIFT | Landed, receipt 11 |
| 16 | `4-other-backends/0036-cuda-disable-fused-TQ3_1S-mul_mat-kernel-fixes-DSv4-.patch` | CASCADE | Landed after 0035, receipt 12 |
| 17 | `4-other-backends/0038-cuda-enable-fused-TQ3_1S-mul_mat-on-HIP-gfx1100.patch` | CASCADE | Landed, receipt 13 |
| 18 | `4-other-backends/0047-sycl-hip-fix-two-build-breaks-in-fork-added-code.patch` | CASCADE | Landed, receipt 14 |
| 19 | `5-models-server/0002-llama-port-TurboQuant-KV-cache-models-and-common-lay.patch` | UPSTREAM DRIFT | TurboQuant KV-cache support already present; conflict selection produced no delta |
| 20 | `5-models-server/0005-merge-close-TurboQuant-parity-gaps.patch` | CASCADE | Landed, receipt 4 |
| 21 | `5-models-server/0006-fix-close-complete-audit-findings-7-port-regressions.patch` | UPSTREAM DRIFT | Parser/audit delta landed, receipt 15 |
| 22 | `5-models-server/0012-laguna-DFlash-drafter-Laguna-decoder-contract-NaN-sa.patch` | UPSTREAM DRIFT | Laguna behavior already present; artifact cleanup was included in receipt 17 |
| 23 | `5-models-server/0020-chat-add-qwen3-specialized-parser-26252.patch` | UPSTREAM DRIFT | Specialized parser already present; newer exact-tool trigger behavior retained |
| 24 | `5-models-server/0022-DeepseekV4-MTP-DSpark-25784.patch` | UPSTREAM DRIFT | Landed, receipt 16 |
| 25 | `5-models-server/0023-dflash-fix-merge-artifacts-from-upstream-cherry-pick.patch` | CASCADE | Landed after 0022 and removed duplicate/artifact code, receipt 17 |
| 26 | `5-models-server/0024-feat-add-Vulkan-TQ4_1S-weight-pipeline-wiring-f03d33.patch` | CASCADE | Duplicate layer emission of the Vulkan wiring; no second delta |
| 27 | `5-models-server/0025-fix-server-batch-restored-checkpoint-prompt-processi.patch` | UPSTREAM DRIFT | Landed, receipt 19 |
| 28 | `5-models-server/0026-cuda-rework-MoE-expert-cache-execution.patch` | UPSTREAM DRIFT | Landed, receipt 20 |
| 29 | `5-models-server/0027-server-fix-auto-selected-DSpark-device.patch` | CASCADE | Superseded by the stronger shared-draft placement patch in receipt 18 |
| 30 | `5-models-server/0028-common-preserve-implicit-MoE-cache-provider-settings.patch` | CASCADE | Already integrated by the MoE-cache recovery; reverse apply check succeeded |
| 31 | `5-models-server/0029-server-harden-shared-draft-device-placement.patch` | UPSTREAM DRIFT | Landed, receipt 18 |
| 32 | `5-models-server/0030-llama-skip-dormant-MoE-cache-sessions.patch` | CASCADE | Landed, receipt 21 |
| 33 | `5-models-server/0031-cuda-adapt-MoE-cache-admission-to-device-capability.patch` | CASCADE | Landed, receipt 22 |
| 34 | `5-models-server/0032-cuda-align-MoE-cache-pool-allocation-with-fit.patch` | CASCADE | Landed, receipt 23 |
| 35 | `5-models-server/0033-server-account-for-MTP-placement-in-fit-reservations.patch` | CASCADE | Landed, receipt 24 |
| 36 | `5-models-server/0034-cuda-aggregate-small-MoE-tensors-into-cache-pools.patch` | CASCADE | Already integrated by the recovered MoE-cache implementation; reverse apply check succeeded |
| 37 | `5-models-server/0035-cuda-reject-undersized-automatic-MoE-cache-slabs.patch` | CASCADE | Landed, receipt 25 |
| 38 | `5-models-server/0036-common-surface-MoE-cache-activation.patch` | CASCADE | Landed, receipt 5 |
| 39 | `5-models-server/0038-moe-cache-add-moe-cache-soft-mode-with-partial-exper.patch` | CASCADE | Landed, receipt 26 |
| 40 | `5-models-server/0039-model-Muse-Glimmer-Support-26841.patch` | UPSTREAM DRIFT | Landed, receipt 27 |
| 41 | `5-models-server/0040-model-Muse-Glimmer-Support-26841-290.patch` | CASCADE | Exact content duplicate of 0039 by stable patch-id `a4f27d97fa022a34433c9f7070c34a8254839552`; not landed twice |
| 42 | `5-models-server/0042-moe-cache-route-context-eligibility-probe-and-remain.patch` | CASCADE | Landed after MoE prerequisites, receipt 28 |
| 43 | `5-models-server/0043-moe-cache-audit-fixes-H1-F1-F2-A1.patch` | CASCADE | Landed, receipt 29 |
| 44 | `5-models-server/0044-moe-cache-restore-moe_cache-params-on-failed-fit-F1-.patch` | CASCADE | Landed, receipt 30 |
| 45 | `5-models-server/0045-llama-remove-stale-MSA-KV-indexer-references.patch` | CASCADE | Strict subset of the following cleanup; integrated by receipt 31, then reverse apply check succeeded |
| 46 | `5-models-server/0046-llama-remove-dead-MSA-indexer-scaffolding.patch` | CASCADE | Landed against the diverged KV-cache layout, receipt 31 |
| 47 | `5-models-server/0047-moe-cache-add-logging-at-silent-failure-points-acros.patch` | CASCADE | Landed, receipt 32 |
| 48 | `5-models-server/0049-chat-restore-upstream-DeepSeek-V4-handling-lost-in-t.patch` | UPSTREAM DRIFT | Its final behavior was already incorporated while resolving the chat conflicts; three-way apply then produced no delta |
| 49 | `5-models-server/0052-cuda-fit-fix-two-Werror-build-failures-in-the-new-ca.patch` | CASCADE | Landed, receipt 33 |
| 50 | `6-tests-build/0005-test-fix-some-CI-errors-26415.patch` | UPSTREAM DRIFT | WebGPU exclusion merged as a union of MiniMax variants, receipt 34 |
| 51 | `6-tests-build/0018-cuda-rework-MoE-expert-cache-execution.patch` | CASCADE | Duplicate test-layer emission against a newer expanded suite; current provider-registry tests and additional coverage were retained, producing no final delta |

## What is now in

CHECKED: Muse Glimmer projector/model support is present at `tools/mtmd/clip-impl.h:495` and `tools/mtmd/clip-impl.h:557`, with its model and image-processing changes in the same recovery commit.

CHECKED: The MoE expert-cache subsystem is present with provider sessions, per-device capability admission, route eligibility, aligned pool fitting, aggregate small-tensor accounting, minimum automatic slabs, soft partial-expert mode, dormant-session handling, restored fit parameters, MTP reservations, failure diagnostics, and the expanded cache/fit tests. Evidence includes `common/arg.cpp:2769`, `common/fit.h:47`, `tests/test-moe-cache-fit.cpp:39`, and `tests/test-moe-cache.cpp:97`.

CHECKED: Vulkan contains TurboQuant `mul_mat_vec_id`, turbo3 KV pipeline wiring, TQ4_1S weight and ID pipelines, the rotated `mul_mm_id` path, and the scoped MUL_MAT_ID gate. Evidence includes `ggml/src/ggml-vulkan/vulkan-shaders/vulkan-shaders-gen.cpp:842`, `:928`, and `ggml/src/ggml-vulkan/ggml-vulkan.cpp:4824`.

CHECKED: CUDA/HIP contains TurboQuant MMVQ/WHT and fused-path gates including gfx1100 enablement. SYCL contains upstream quantized SET_ROWS helpers plus Turbo2/3/4 cooperative kernels and F16/F32 dispatch. Metal contains restored templates, F16 binary operations, SILU_BACK, TurboQuant kernels, and DeepSeek V4 hyper-connections. Evidence includes `ggml/src/ggml-sycl/set_rows.cpp:905`, `ggml/src/ggml-metal/ggml-metal-ops.cpp:331`, `:1413`, and `ggml/src/ggml-cuda/ggml-cuda.cu:5166`.

CHECKED: DeepSeek V4 MTP/DSpark, DFlash Laguna behavior, specialized Qwen3 tool-call parsing, and TurboQuant KV-cache rotation support are present. Evidence includes `src/models/deepseek4.cpp:1409`, `src/models/dflash.cpp:124`, `common/speculative.cpp:1027`, `common/chat.cpp:1173`, and `src/llama-kv-cache.cpp:423`.

CHECKED: Features still out from these 51 entries: none identified in source review.

ASSUMED: Source presence does not prove buildability or runtime correctness.

## Refutations and constraints

CHECKED: The brief's counts reproduce: the log has exactly 51 FAIL lines and the initial branch had exactly 123 commits above upstream base `22b8e310b921d568e013e4533002be5a8fe53f17`.

CHECKED: The brief's implied clean landed state was false. Multiple source files contained literal committed conflict markers before this run. The final source marker scan is clean.

CHECKED: A failed apply did not always mean a missing feature. Several entries were already present, superseded, subsets, or duplicated into another layer.

CHECKED: No build, compilation, benchmark, test executable, or model launch was run, exactly as required. Therefore compilation and runtime behavior remain unverified.

CHECKED: No platform safeguard or model-switch banner was displayed during this run. Runtime identity was not observed and remains `RUNTIME_IDENTITY_UNVERIFIED`.

## Source-only verification

CHECKED: `rg --no-ignore-vcs -n '^<<<<<<< ours|^=======|^>>>>>>> theirs' -g '!_lane-out/**' -g '!*.patch' .` found no source conflict markers (excluding the known third-party miniaudio decorative match filter).

CHECKED: Shadow `git diff HEAD --check` produced no findings, and shadow `git status --short` showed only the untracked `_lane-out/` artifacts.

CHECKED: `git bundle verify _lane-out/engineG-conflicts.bundle` reported the bundle is OK.

## Wrapper receipt ledger

CHECKED: Each landed claim below is the wrapper's exact stdout proof, and every identifier was re-read with `git log`/`git rev-parse` from the shadow repository.

LANDED 7884ad9e11a6a2f1f701025470fa0cadbdee92c3
LANDED 331016f791a1c762ffb67648d49ec9fc3bafa561
LANDED 4ab5c2def343a86d3eddc3bb741d45fe069c4cbf
LANDED f47c33e991880e68282058856f483472a89dcafb
LANDED fce59816d9e9f507ce40c0b067483b79c36611ff
LANDED 285f9bae1605625038bc54e7ad99e137e40f46dd
LANDED 177adfbe5748b310209b22a1dbcacb95e1210f0c
LANDED b81c925af0da369d05191b6a4602ec46a83f7705
LANDED dd6b184a54efe597100e846bd51266d7e1d32b8a
LANDED 5d2d18398274ce7d04e548d245f0efa3191608ca
LANDED 813db83cabd3f3ae8dbf1bb0f7786c773247f248
LANDED 70bf70fe808867c1da63b591288692615a87bbde
LANDED 9f5eec0d39a6ffe66805429c3e29faf5ea76a1f3
LANDED d2898a29a3bceb472ff9ffc256416285e24aae16
LANDED 48bab33690f2287b129158b85663a30fd03818d2
LANDED 93fd694613a943b3e11335def835e20252bf29a1
LANDED 31e78facc4842b72c364227cdca83f38464542e4
LANDED 040b9d386429bb1e3ff187e36eaa87a871238c72
LANDED 13039955658d8d4bb63112ddac255325b913e0dc
LANDED 36e3483793170a74b3cbe3ebde98cad7b95fd048
LANDED 0435d3b023fc2d89e413296a02e7195754b97730
LANDED d4d586af427b89af91fb103b9f0f86a14fdc7e3b
LANDED 4899f2ae36adf8371a84bce3cf345548d44bc8ce
LANDED 64d73dade3b703f267f8668674ae7c8f644e66e2
LANDED 726916ddd6856b87ba277dc28d90ed80dfbbc2be
LANDED 12796219f9cd37fa02a9b7ae1e5cca481ef7b418
LANDED 5cb006de62a1c4a37aff87dd5a878307031aeaae
LANDED fadc0bf71478a3b581219a5629ae5dfa589e9a53
LANDED 6cde9eb834472bfe97709a6119197e89a908506d
LANDED a836030ff23ed5bb3fb4aa62e283a63615bfdb18
LANDED f3cd160c4cf9a0da552afceaa6a0559f6afe3d29
LANDED 4fad2b858fbebe55d4b3338f6a2d52b6d37d4e59
LANDED b5bff62e89d83d8d8bb49ffdced374c7f3b03c6b
LANDED 870eb2119ee48c24b5ee3a542796f6b1138c828c
LANDED 6564107bef7e66bd2f2556aec86ad8b386600a1d
