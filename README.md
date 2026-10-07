<p align="center"><img src="media/arifilabs-llama-cpp-banner.webp" alt="ArifiLabs llama.cpp" width="820"></p>

# ArifiLabs llama.cpp

**Ornith-1.5 35B at 35 tokens/s, Qwen3.8-Flash-Next at 20 tokens/s and Qwen3.8 27B at 12 tokens/s on an AMD Radeon
890M, the integrated graphics of a mini-PC. Vulkan, no discrete GPU.**

A llama.cpp fork built for AMD integrated GPUs. It runs MTP drafting on Qwen3.8-Flash-Next, gives Vulkan kernels to
quant formats that only CUDA or Metal users could run, makes the formats you already use faster, and keeps DFlash2 and
MTP speculative decoding stable and repeatable. Built on upstream llama.cpp `b11178`.

## Speed: Radeon 890M (Minisforum AI X1 Pro-470)

| Model file | Mode | Decode t/s, short / ~600-token prompt | Prompt t/s (~600 tokens) | Decode vs our previous release |
|---|---|---|---|---|
| Qwen3.8-Flash-Next GSQ-RCO IQ3_S (ISTA-DASLab) | plain | 12.01 / 11.89 | 150.2 | +8.5% / +9.2% ¹ |
| same | MTP (sidecar head) | 19.68 / 17.70 | 144.0 | new: +65.4% / +50.4% over plain ² |
| same | MTP + 40K draft vocabulary | **20.24 / 19.45** | 143.5 | new: +70.1% / +65.3% over plain ² |
| Ornith-1.5 35B-A3B Q4_K_M | plain | 29.33 / 28.98 | 397.9 | +8.6% / +8.8% |
| same | MTP (built-in head) | **34.07 / 35.01** | 415.2 | +16.1% / +20.8% over plain |
| same | DFlash2, 2 tokens | 32.07 / 33.60 | 398.1 | +8.6% / +7.5% |
| Qwen3.8 27B UD-Q4_K_XL (Huihui abliterated) | plain | 4.57 / 4.60 | 104.4 | +5.9% / +6.0% |
| same | MTP (built-in head) | 10.08 / 9.84 | 102.2 | +120.6% / +114.0% over plain |
| same | DFlash2, 3 tokens | **12.09 / 11.55** | 95.0 | +15.5% / +8.7% |
| Qwen3.8 27B S-X8 v4.3 (MarlaLabs) | plain | 2.94 / 2.96 | 90.8 | +1.5% / +2.4% |
| same | DFlash2, 4 tokens | 8.89 / 8.29 | 88.5 | +5.5% / +6.9% |
| GLM-5.3 Flash GSQ-RCO 3.0-bit, experts streamed from NVMe ³ | DFlash2, 2 tokens | 2.96 (chat) | 4.4–4.6 | tie (+0.3%), output identical |

¹ Flash-Next's comparator is our previous release plus its three load/restore fixes: the previous release cannot load
this file inside the memory floor. ² The previous release had no MTP for these models, so MTP rows compare MTP against
plain decode on this build; for Flash-Next that plain arm ran in the same cell (11.89 / 11.77 t/s, 6 rounds).
³ GLM-5.3 is new in this release: its comparator is the best GLM build before this release, 8 launches ABBA, output ids
exact on all 8. Receipts: [`evidence/b11178-x1i2/`](evidence/b11178-x1i2/) (`sg-*` = these rows, `MANIFEST.md` = sha256 per file).
Plain-decode output ids differ from our previous release on three files, and in each case the previous release is
the one that moves: on S-X8 (token 5), the 27B Q4_K_XL (token 92, a near-tie of 0.0588 nats) and Ornith-1.5 (token 48)
its output changes with request history, while this release gives the same ids every round. On Ornith-1.5 this
release also differs at token 53 (short) and 58 (long) the same way every round: the masked-KV fix changes the last
bits of the attention sum. Details: [`docs/arifi/STATUS.md`](docs/arifi/STATUS.md).

Each row is one A/B on one file: our previous release (`x1i`) against this one, `llama-server`, both arms alternated
(ABBA, two passes), 4 rounds per arm (8 for MTP arms), every round kept, means shown. Greedy output ids are compared
on every row. Percent = (this ÷ previous - 1) × 100 on the shown means, cut to one decimal, never rounded up. Machine:
Ryzen AI 9 HX 470, Radeon 890M, 96 GB DDR5-5600, 72 GB reserved for the GPU, Windows 11. Decode = 128 greedy tokens
after a ~12-token and a ~600-token prompt.

## Speed: Radeon 780M (Beelink SER7 Pro), Qwen3.8 27B

| 27B model file | Plain decode, t/s: before → after | DFlash2 decode, t/s: before → after |
|---|---|---|
| ROCmFP4-FAST (julianmb, 14.56 GB) | 5.084 → 5.086, tie | 7.498 → **9.518**, +26.9% |
| UD-Q3_K_XL (outsourc-e Unleashed, 13.22 GB) | 5.399 → 5.405, tie | 8.367 → **9.268**, +10.7% |
| UD-Q4_K_XL (Huihui abliterated, 17.38 GB) | 3.864 → 4.104, +6.2% | 7.085 → **7.578**, +6.9% |
| S-X8 v4.3 (MarlaLabs, 26.14 GB) ¹ | 1.831 → 2.247, +22.7% | 3.334 → **4.475**, +34.2% |

Each before → after pair is one A/B on one file: this fork's build without the kernel, then the build with it
(September 2026, `llama-server`, DFlash2 drafting 2 tokens per step), 16 rounds per arm: **medians**, except
ROCmFP4-FAST, which shows **means**, as its protected-win record states. On the Q4_K_XL and S-X8 rows, the plain and
DFlash2 pairs are separate A/Bs. "tie" = the paired confidence interval includes zero. File sizes are in GB (10⁹ bytes).
¹ The build before these kernels could not load this file with a drafter; a per-heap allocator fix cured it.

## What is new in this release

Each effect below is a separate A/B on the Radeon 890M with Qwen3.8-Flash-Next GSQ-RCO IQ3_S unless named, on the
build where the change landed; receipts are in [`evidence/b11178-x1i2/`](evidence/b11178-x1i2/).

| Change | Effect (Radeon 890M) | Default |
|---|---|---|
| MTP drafting for Qwen3.8-Flash-Next (`--spec-type draft-mtp -md <MTP sidecar>`) | 10.73 → 16.64 t/s short prompt (+55.0%), 10.38 → 15.14 long (+45.8%), draft acceptance 91.5% / 78.2%, means of 2 rounds | opt-in (needs the MTP sidecar GGUF) |
| 40,525-token draft vocabulary head for MTP | +4.6% to +8.7% decode over the MTP head on all six test requests, same output ids | opt-in (sidecar) |
| Recurrent-state update in place (GDN state bank) on the MTP verify path and in plain decode | MTP decode +9.5% (mean of four prompts, 3-round medians, ids 12/12); plain decode +4.4% short, +3.0% long (3 rounds) | on; `GGML_VK_DISABLE_GDN_BANK` reverts |
| One fused dispatch per hyper-connection post step | 288 fewer dispatches per token; +1.2% short / +6.6% long decode in a 2-run ABBA, ids 8/8 | on; `GGML_VK_DISABLE_HC_POST_W` reverts |
| Sparse attention at prefill (QSA) | 32K-token prompt 105.8 → 194.1 t/s (+83.4%), 16K 156.9 → 205.7 (ubatch 4096, F16 KV, 2 rounds) | on; `GGML_ARIFI_FA_SPARSE_PREFILL=0` reverts |
| Pooled indexer-key cache for long-context decode | decode at 32K tokens 8.29 → 10.35 t/s (+24.8%), 2 rounds | on; `GGML_ARIFI_QSA_POOL_CACHE=0` reverts |
| Embedding-row prefetch per prompt chunk | novel 2,000-token prefill 126.0 → 180.1 t/s (+42.9%), decode +6.2%, same ids, means of 4 requests | on; `LLAMA_PLE_PREFETCH=0` reverts |
| Embedding-row release from the Windows working set | Windows-side growth over 8,000 novel tokens +389 MiB → -67 MiB (measured at a 64 MiB budget), same ids | on at a 256 MiB budget; `LLAMA_PLE_RELEASE=0` reverts |
| Repeatable greedy decode on Vulkan (masked KV cells kept out of the coopmat1 flash-attention sum) | 10 of 10 identical runs per prompt on Qwen3.8-Flash-Next and Ornith-1.5-35B-A3B; the old path gave two sequences | on; `GGML_VK_FA_DEADV_KEEP=1` = old path |
| MoE expert mat-vec in slot-major order (shared experts stay in cache) | served decode unchanged: +0.34% mean over four prompts (worst -0.69%), ABBA ×2, same ids | on at width 1; `GGML_ARIFI_MOE_GATHER=0` reverts |
| S-X8 v4.3 prompt reading on the int8 coopmat1 kernel | 782-token prompt 73.31 → 92.25 t/s (+25.8%, medians); KL divergence 0.000378 against the float path | on from 56 columns; `GGML_ARIFI_SX8_CM1=0` reverts |
| Prompt images kept in reusable device snapshots | a 512-token prompt: server peak working set 8.57 / 8.73 GB → 1.51 / 1.46 GB | always on |
| Direct-I/O model loading on Windows (`-lm dio`) | Qwen3.8-Flash-Next loads and serves with a 0.8–1.6 GB server working set, same ids as mmap | with `-lm dio` |
| GLM-5.3 Flash with routed experts streamed from NVMe | 2.96 t/s decode, sibling pre-read and cached readback on by default | `GGML_ARIFI_MOE_NVME=1` |
| llama-quantize reads its input in slabs on Windows | no file mapping of the input on Windows | always on (Windows) |
| ROCmFP4-FAST files take the q8_1 mat-vec route | +26.9% drafted (Radeon 780M, table above) | **now on**; `GGML_ARIFI_ROCMFP4_MMVQ=0` reverts |

Fixed since our previous release (both found on the previous release's own binary, with a failing test first):
loading Qwen3.8-Flash-Next no longer drains Windows memory to the alarm line (whole-file prefetch removed), and an
MTP checkpoint restore at a position not divisible by 4 no longer writes the wrong ring plane. Details:
[`docs/arifi/STATUS.md`](docs/arifi/STATUS.md).

## Vulkan support this fork adds

| Format | Upstream llama.cpp Vulkan (`b11178`) | This fork | What we added |
|---|---|---|---|
| S-X8 v4.3 (type 57) | none | `dequant_sx8.comp`, `mul_mat_vec_sx8.comp` | **NEW on Vulkan**: format plus kernels |
| ROCmFP4, ROCmFP4-FAST | none | `dequant_rocmfp4.comp`, `dequant_rocmfp4_fast.comp` | **NEW on Vulkan**: formats, q8_1 mat-vec |
| ROCmFPX FP2 / FP3 / FP6 / FP8 | none | `dequant_rocmfpx_fp2.comp`, `_fp3`, `_fp6`, `_fp8` | **NEW on Vulkan**: four new formats |
| TQ3_1S, TQ4_1S | none | `dequant_tq3_1s.comp`, `dequant_tq4_1s.comp`, `mul_mat_vec_tq3_1s.comp`, `mul_mat_vec_tq4_1s.comp`, `mul_mat_vec_tq_sg.comp` | **NEW on Vulkan**: formats, subgroup mat-vec |
| TQ3_4S (type 48) | none | `dequant_tq3_4s.comp`, `mul_mat_vec_tq3_4s.comp` | **NEW on Vulkan**: format plus kernels |
| Escha-W2 (types 55/56) | none | `escha_mm.comp` | **NEW on Vulkan**: fused decode-and-matmul kernel |
| TurboQuant KV cache (turbo2/3/4) | none | `dequant_turbo3_0.comp`, `turbo_wht.comp` | **NEW on Vulkan**: new KV cache types |
| Q2_0_G128 ternary (type 43) | none | `dequant_q2_0_g128.comp` | **NEW on Vulkan**: new ternary format |
| IQ4_XS | dedicated mat-vec, 8 threads per superblock | `mul_mat_vec_iq4_xs.comp`, 16 threads per superblock | ours stays the default; upstream's shader and route are switches (`GGML_ARIFI_IQ4XS_MV=upstream`) |
| IQ3_S, IQ3_XXS | dedicated mat-vec | `mul_mat_vec_iq3_s.comp`, `mul_mat_vec_iq3_xxs.comp` retuned | faster mat-vec at verify widths |
| Q5_K, Q6_K | dedicated mat-vec | `mul_mat_vec_q5_k.comp`, `mul_mat_vec_q6_k.comp` retuned | faster mat-vec, q8_1 route |
| Q4_K | dedicated mat-vec | q8_1 route in `mul_mat_vecq.comp`, width-5 split | faster mat-vec at widths 5-8 |

"none": upstream `b11178` has no such type in its Vulkan backend (re-checked against the `b11178` tree for this
release). Shaders live in [`ggml/src/ggml-vulkan/vulkan-shaders/`](ggml/src/ggml-vulkan/vulkan-shaders/).

## Quick start (Windows, Vulkan)

```powershell
git clone -c core.longpaths=true <this repo> arifilabs-llama.cpp; cd arifilabs-llama.cpp
cmake -S . -B build-vulkan -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON `
  -DGGML_ARIFI_ROCMFPX_FORMATS=ON -DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON -DLLAMA_USE_PREBUILT_UI=OFF `
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00" `
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" `
  -DLLAMA_BUILD_UI=OFF -DLLAMA_BUILD_WEBUI=OFF -DLLAMA_USE_PREBUILT_WEBUI=OFF
cmake --build build-vulkan --target llama-server -j 2
# Qwen3.8-Flash-Next with MTP drafting
build-vulkan\bin\llama-server.exe -m Qwen3.8-Flash-Next-GSQ-RCO-IQ3_S-00001-of-00002.gguf -ngl 999 -np 1 -fa on `
  --lazy-mode on -c 4096 -lm dio -md mtp-sidecar-qwen4exp-q8_0.gguf -ngld 999 `
  --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5
# Qwen3.8 27B with DFlash2 drafting
build-vulkan\bin\llama-server.exe -m Huihui-Qwen3.8-27B-abliterated-UD-Q4_K_XL.gguf -dev Vulkan0 -ngl 999 -fa on `
  -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0 --parallel 1 `
  --spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --dflash-defer-injection 0 --spec-draft-n-max 3
```

The two serve lines are the measured cells' command lines (`evidence/b11178-x1i2/sg-fn-*-state.json`,
`sg-q27-*-state.json`; port, host, thread and context-checkpoint flags left out). The 27B drafter is incoai's `Qwen3.8-27B-DFlash2-GGUF`
(Q4_K_M). Toolchain, flags and the MinGW runtime step are in [`docs/BUILDING.md`](docs/BUILDING.md). At startup the
Vulkan backend prints one line for each tuned default it applied on your GPU.

## Hardware

- **Measured on:** AMD Radeon 890M (Minisforum AI X1 Pro-470, the tables above marked 890M) and AMD Radeon 780M
  (Beelink SER7 Pro, the 780M table). Each table names its machine; no number moves between them.
- **Other GPUs:** many defaults switch on by device probe, on AMD or on AMD RDNA3 only, and other devices keep the
  upstream setting. Some changes run on every Vulkan device, including six mat-vec changes to upstream formats: the
  IQ3_S 16-thread layout, the dedicated IQ4_XS shader, the iq3 sign hoist (bit-identical output), the Q6_K direct
  scales and x-fold, and the Q5_K activation hoist. [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md) gives each
  switch's device scope. All upstream backends (CPU, CUDA, Metal, SYCL and others) stay in the tree.

## What it adds over upstream

### Quant formats with GPU kernels

Every format below has Vulkan shaders in this tree. The weight formats pass `test-backend-ops` on the
GPU against the CPU reference, with 0 failures.

| Format | Format author | Vulkan | CPU | CUDA | Metal |
|---|---|---|---|---|---|
| S-X8 v4.3, 7.5 bpw (type 57) | MarlaLabs | kernel | kernel | none | none |
| ROCmFP4, ROCmFP4-FAST | charlie12345 | kernel | kernel | none | none |
| ROCmFPX FP2 / FP3 / FP6 / FP8 | charlie12345 | kernel | kernel | none | none |
| TQ3_1S, TQ4_1S | TheTom (llama-cpp-turboquant) | kernel | kernel | kernel ² | kernel ² |
| TQ3_4S (type 48) | turbo-tan (llama.cpp-tq3) | kernel | kernel | kernel ² | kernel ² |
| TurboQuant KV cache (turbo2/3/4) | TheTom (llama-cpp-turboquant) | kernel | kernel | kernel ² | kernel ² |
| Q2_0_G128 ternary (type 43) | PrismML | kernel | kernel + VNNI repack | none | none |
| Escha-W2, 2 and 3 bpw (types 55/56) | EschaLabs | fused kernel | kernel | none | none |

² Carried from the format author's tree; we build and test on Vulkan and CPU.

### Faster kernels for formats you already use

Speculative decoding verifies several tokens per step, so mat-vec speed at widths 2-8 decides drafted
decode. These kernels target exactly those widths. Where a row names a switch, it restores the
upstream path.

| Kernel | Measured effect (Radeon 780M) | Default |
|---|---|---|
| IQ3_S mat-vec, 16 threads per superblock at verify widths | Q3_K_XL 27B drafted 8.367 → 9.268 t/s (medians), +10.7% | on, every device |
| IQ4_XS dedicated mat-vec shader | IQ4_XS 27B drafted +0.6167 t/s mean paired difference, 8 of 8 cells | on, every device |
| iq3 mat-vec at width 7: register spill removed | 465,399.5 → 20,890.3 µs per op, 22.2x | on (RDNA3), `GGML_ARIFI_IQ3_N7_ROWS=4` reverts |
| q6_K on the q8_1 MMVQ path at widths 7-8 | lm_head at width 7 20,046 → 14,313 µs, 1.40x; width 8 1.13x | on (AMD), `GGML_ARIFI_Q6K_MMVQ=legacy` reverts |
| q4_K width-5 split | lm_head 12,749.2 → 9,747.1 µs, 1.30x | on (RDNA3), `GGML_ARIFI_Q4K_W5_SPLIT=0` reverts |
| q5_K on the q8_1 MMVQ path at widths 5-8 | +6.7% to +20.4% per op at widths 5-8 (FFN shape 17408x5120) | on (AMD), `GGML_ARIFI_Q5K_MMVQ=0` reverts |
| iq3_s two rows per workgroup at width 6 | iq3_s mat-vec time of the 6th verify column 47.23 → 23.78 ms per step | on (RDNA3) |
| S-X8 decode kernel, A-hoist and MMVQ route | plain 1.831 → 2.247 t/s; drafted 3.334 → 4.475 t/s (medians); 71-73 GB/s at widths 4-8 | on |
| TQ subgroup mat-vec | `llama-bench`: TQ3_4S 27B tg32 0.67 → 1.80 t/s; TQ4_1S 0.5B tg64 44.69 → 126.65 t/s | on |
| ROCmFP4-FAST q8_1 MMVQ | drafted 7.498 → 9.518 t/s (means), +26.9% | on; `GGML_ARIFI_ROCMFP4_MMVQ=0` reverts |
| Escha-W2 column-blocked matmul | 3.50x on the escha3 prefill shape (17408x5120, 64 columns) | on |
| Q2_0_G128 VNNI repack (CPU) | decode 2.01 → 6.69 t/s, 3.3x | `GGML_ARIFI_VNNI_REPACK=2` |

### Speculative decoding, MoE and memory

- **MTP drafting for Qwen3.8-Flash-Next** with a sidecar MTP head, and an optional 40,525-token draft vocabulary.
- **Stable DFlash2 and MTP drafting on Qwen3.8.** Attaching a drafter no longer corrupts the recurrent
  state of hybrid models. Replayed draft tokens are not re-verified after a checkpoint restore, which
  stops a slot loop on Vulkan. A rejected draft checkpoint no longer stops the server.
- **Repeatable greedy decode on Vulkan:** masked KV cells no longer reach the coopmat1 flash-attention sum, so the same
  prompt gives the same output request after request.
- **Adaptive draft length:** `--spec-draft-adaptive` sizes each draft from the measured acceptance.
- **GLM-5.3 Flash with NVMe expert streaming** (`GGML_ARIFI_MOE_NVME=1`): routed experts are read from the drive.
- **Bigger-than-RAM MoE:** PowerInfer expert streaming from SSD (`EXPERT_BUNDLE_PATH`) runs a 35B MoE
  model (Qwen3.6-35B-A3B) with its experts read from an on-disk bundle.
- **MoE expert cache on Vulkan:** `--moe-cache` keeps recently used experts on the GPU when the routed
  experts stay in host memory, with a dedicated Vulkan shader and heat-protected eviction. It is the
  ggml-backend provider from TheTom's turboquant tree. See [`docs/backend/MOE-CACHE.md`](docs/backend/MOE-CACHE.md).
- **Placement into the GPU reservation** on the Radeon 890M (`GGML_VK_UMA_PLACEMENT`), plus a
  per-heap allocator that serves 27B drafted lines which used to fail at load.

## For format authors and researchers

You were sent this link because your format runs here on the GPU. Find your format below.

**S-X8 v4.3 (MarlaLabs).** Vulkan: `dequant_sx8.comp`, `mul_mat_vec_sx8.comp`, a q8_1 MMVQ path, a
packed cooperative-matrix tile for prefill, and (new) prompt reading on the int8 coopmat1 kernel from 56 columns.
Qwen3.8 27B S-X8: 5.551 t/s drafted at draft depth 4, 2.247 t/s plain (medians), mat-vec at 71-73 GB/s on a 780M,
close to the memory bus.
```powershell
python tools/gguf-retag-sx8/retag_sx8.py model-sx8.gguf model-sx8.57.gguf   # your type id 41 -> our 57
build-vulkan\bin\llama-server.exe -m model-sx8.57.gguf -dev Vulkan0 -ngl 999 -fa on -c 8192
```

**ROCmFP4 and ROCmFPX (charlie12345).** Vulkan: `dequant_rocmfp4.comp`, `dequant_rocmfp4_fast.comp`,
`dequant_rocmfpx_fp2.comp`, `_fp3`, `_fp6`, `_fp8`, mat-vec and get-rows pipelines for all six types, and a
q8_1 MMVQ path for ROCmFP4-FAST, on by default. Qwen3.8 27B ROCmFP4-FAST: 9.518 t/s drafted (from 7.498, means).
```powershell
cmake ... -DGGML_ARIFI_ROCMFPX_FORMATS=ON          # the quick-start build already has it
build-vulkan\bin\llama-server.exe -m Qwen3.8-27B-ROCmFP4-FAST.gguf -dev Vulkan0 -ngl 999 -fa on -c 8192
```

**TQ3_1S, TQ4_1S, TQ3_4S and TurboQuant KV (TheTom, turbo-tan).** Vulkan: `dequant_tq3_1s.comp`,
`dequant_tq4_1s.comp`, `dequant_tq3_4s.comp`, `mul_mat_vec_tq3_1s.comp`, `mul_mat_vec_tq4_1s.comp`,
`mul_mat_vec_tq3_4s.comp`, the subgroup mat-vec `mul_mat_vec_tq_sg.comp`, `tq_rotate_act.comp`,
`dequant_turbo3_0.comp` and `turbo_wht.comp`. Measured with `llama-bench`: TQ3_4S 27B tg32 0.67 → 1.80 t/s;
TQ4_1S 0.5B tg64 44.69 → 126.65 t/s.
```powershell
cmake ... -DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON       # the quick-start build already has it
python tools/gguf-retag-tq3/retag_tq3.py model-tq3_4s.gguf model-tq3_4s.arifi.gguf   # tq3 ids -> ours
build-vulkan\bin\llama-server.exe -m model-tq3_4s.arifi.gguf -dev Vulkan0 -ngl 999 -fa on
```

**Q2_0_G128 ternary (PrismML).** Vulkan: `dequant_q2_0_g128.comp` plus mat-vec. CPU: a VNNI repack
with GEMV and GEMM kernels, 2.01 → 6.69 t/s decode (3.3x) with `GGML_ARIFI_VNNI_REPACK=2`.
```powershell
python tools/gguf-retag-g128/retag_g128.py model-q2_0.gguf   # writes model-q2_0.g128.gguf
build-vulkan\bin\llama-server.exe -m model-q2_0.g128.gguf -dev Vulkan0 -ngl 999
```

**Escha-W2 (EschaLabs).** Native types 55 and 56 with a fused `GGML_OP_ESCHA_MM`
(`escha_mm.comp`): Hadamard rotation, code decode and matmul in one dispatch. Decode is coherent on
2- and 3-bit files; column blocking makes the escha3 prefill shape 3.50x faster. Load the GGUF with its
`.escha_aux` sidecar beside it; `test-backend-ops -o ESCHA_MM` checks it against the CPU.

**K-quants and i-quants (upstream formats).** Dedicated or retuned Vulkan mat-vec kernels for IQ4_XS
(`mul_mat_vec_iq4_xs.comp`), IQ3_S and IQ3_XXS, Q4_K, Q5_K and Q6_K. See the kernel table above. Every
switch and its default is in [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md).

## Built to rebase

- **A linear patch series** on upstream `b11178` (`f9af9be219ca647a59106f6201bf0d85fab00224`), zero merge commits.
  `python tools/arifi-sync/arifi_sync.py series check` regenerates it, replays it with `git am` and compares the result
  with this tree.
- **Runtime switches.** Each tuned default has an environment switch that restores the old path, listed in
  [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md).
- **Protected wins.** [`tools/arifi-sync/protected-wins.json`](tools/arifi-sync/protected-wins.json) names the code that
  61 measured wins depend on, and `protected-win validate` flags an upstream bump that drops one. It passes at this
  tip.
- **Provenance trailers.** Fork commits carry `Taken-from:` (source repository and commit) or `Origin:`, and
  `Measured-effect:`. 367 earlier commits are grandfathered and 63 are exempt, each pinned by sha with its reason.
  `python tools/arifi-sync/arifi_sync.py provenance --strict` passes.

## Credits

This fork stands on other people's work. [`NOTICE`](NOTICE) names each licensed source whose code this
tree carries, with its licence.

- [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp): the engine and the base of this fork.
- [LaurentZuijdwijk/llama.cpp](https://github.com/LaurentZuijdwijk/llama.cpp): the IQ3_S 16-thread mat-vec
  body, the tiled concat-transpose for the delta-net conv state, the draft replay-skip fix and adaptive
  draft sizing.
- [spiritbuun/buun-llama-cpp](https://github.com/spiritbuun/buun-llama-cpp): the DFlash2 adaptive
  controller and the checkpoint-rewind hardening.
- [charlie12345/ROCmFPX](https://github.com/charlie12345/ROCmFPX): the ROCmFP4 and ROCmFPX formats, their
  CPU paths, quantizer wiring and q8_1 mat-vec functions.
  [ciru-ai/ROCmFPX](https://github.com/ciru-ai/ROCmFPX): a Vulkan build fallback.
- [TheTom/llama-cpp-turboquant](https://github.com/TheTom/llama-cpp-turboquant): TurboQuant KV, the TQ3_1S
  and TQ4_1S weight formats and their kernels, and the MoE expert cache with its Vulkan provider.
  [TheTom/turboquant_plus](https://github.com/TheTom/turboquant_plus): KV-fidelity tooling (Apache-2.0).
- [turbo-tan/llama.cpp-tq3](https://github.com/turbo-tan/llama.cpp-tq3): the TQ3_4S format family with its
  CUDA and Metal kernels.
- [Tiiny-AI/PowerInfer](https://github.com/Tiiny-AI/PowerInfer): sparse execution and SSD expert streaming.
  The Windows IOCP read path is built on it.
- FreeToken (FlashML-org): the design of the device-resident context checkpoint ring with pooled
  recurrent-state slots, the tool-call checkpoint anchor and unbuffered model reads, re-implemented here.
- [PrismML-Eng/llama.cpp](https://github.com/PrismML-Eng/llama.cpp): the Q2_0_G128 ternary format, the VNNI
  repack design and the f16 recurrent state.
- MarlaLabs (Martí Vidal Leandro): the S-X8 v4.3 format (Apache-2.0). The Vulkan kernels here are ours.
- [jtrefon/llama.cpp-turboq-mtp](https://github.com/jtrefon/llama.cpp-turboq-mtp): the TBQ3_0 and TBQ4_0
  KV cache types.
- [thecodacus/llama.cpp](https://github.com/thecodacus/llama.cpp): three host-transfer expert-prefetch
  patches.
- EschaLabs: the Escha-W2 code format.
- ISTA-DASLab: the Qwen3.8-Flash-Next GSQ-RCO GGUF files measured here.
- Strata: the design of the sparse prefill attention, the pooled indexer-key cache and the 40,525-token draft
  vocabulary, re-implemented here.

## Requirements and limits

- Tested on Windows 11 with Vulkan; MinGW-w64 GCC is the build of record ([`docs/BUILDING.md`](docs/BUILDING.md)).
- Tuned for AMD integrated GPUs; on other GPUs the probe-gated defaults stay off (see Hardware).
- `GGML_ARIFI_VNNI_REPACK` is opt-in.
- What did not work, and every protected win with its state: [`docs/arifi/STATUS.md`](docs/arifi/STATUS.md).

## License

[`LICENSE`](LICENSE) is upstream llama.cpp's MIT licence, verbatim. [`NOTICE`](NOTICE) names every third-party
component with its licence; the licence texts are in [`licenses/`](licenses/). S-X8 and turboquant_plus are
Apache-2.0, and their notices travel with this repository.
