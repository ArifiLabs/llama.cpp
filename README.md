<p align="center"><img src="media/arifilabs-llama-cpp-banner.webp" alt="ArifiLabs llama.cpp" width="820"></p>

# ArifiLabs llama.cpp

**Ornith-1.5 35B at 35 tokens/s, Qwen3.8-Flash-Next at 20 tokens/s and Qwen3.8 27B at 12 tokens/s on an AMD Radeon
890M, the integrated graphics of a mini-PC. Vulkan, no discrete GPU.**

A llama.cpp fork built for AMD integrated GPUs. It runs MTP drafting on Qwen3.8-Flash-Next, gives Vulkan kernels to
quant formats that only CUDA or Metal users could run, makes the formats you already use faster, and keeps DFlash2 and
MTP speculative decoding stable and repeatable. Built on upstream llama.cpp `b11178`. Work on the fork started in
July 2026; this is its third published release.

## Speed: Radeon 890M (Minisforum AI X1 Pro-470)

| Model file | Mode | Draft depth | Drafter | Key server flags | Decode t/s, short / ~600-token prompt | Prompt t/s (~600 tokens) | Decode vs our previous release |
|---|---|---|---|---|---|---|---|
| Qwen3.8-Flash-Next GSQ-RCO IQ3_S (ISTA-DASLab) | plain | - | - | `-ngl 999 -fa on --lazy-mode on -lm mmap -c 2048` | 12.01 / 11.89 | 150.2 | +8.5% / +9.2% ¹ |
| same | MTP (sidecar head) | 3 | `mtp-sidecar-qwen4exp-q8_0.gguf` | `-fa on --lazy-mode on -lm dio -c 4096 --spec-type draft-mtp --spec-draft-p-min 0.5` | 19.68 / 17.70 | 144.0 | new: +65.4% / +50.4% over plain ² |
| same | MTP + 40K draft vocabulary | 3 | `mtp-sidecar-qwen4exp-q8_0-dven40k.gguf` | same as the MTP row | **20.24 / 19.45** | 143.5 | new: +70.1% / +65.3% over plain ² |
| Ornith-1.5 35B-A3B Q4_K_M | plain | - | - | `-ngl 999 -fa on -lm dio --lazy-mode on -c 4096 -b 2048 -ub 512` | 29.33 / 28.98 | 397.9 | +8.6% / +8.8% |
| same | MTP (built-in head) | 3 | head in the model file | plain flags + `--spec-type draft-mtp --spec-draft-p-min 0.5` | **34.07 / 35.01** | 415.2 | +16.1% / +20.8% over plain |
| same | DFlash2 | 2 | `Ornith-1.5-35B-A3B-DFlash2-BF16.gguf` | plain flags + `--spec-type draft-dflash --dflash-defer-injection 0` | 32.07 / 33.60 | 398.1 | +8.6% / +7.5% |
| Qwen3.8 27B UD-Q4_K_XL (Huihui abliterated) | plain | - | - | `-ngl 999 -fa on -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0 --ctx-checkpoints 32` | 4.57 / 4.60 | 104.4 | +5.9% / +6.0% |
| same | MTP (built-in head) | 3 | head in the model file | plain flags + `--spec-type draft-mtp --spec-draft-p-min 0.5` | 10.08 / 9.84 | 102.2 | +120.6% / +114.0% over plain |
| same | DFlash2 | 3 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | plain flags + `--spec-type draft-dflash --dflash-defer-injection 0` | **12.09 / 11.55** | 95.0 | +15.5% / +8.7% |
| Qwen3.8 27B S-X8 v4.3 (MarlaLabs) | plain | - | - | `-ngl 999 -fa on -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0` | 2.94 / 2.96 | 90.8 | +1.5% / +2.4% |
| same | DFlash2 | 4 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | plain flags + `--spec-type draft-dflash --dflash-defer-injection 0` | 8.89 / 8.29 | 88.5 | +5.5% / +6.9% |
| GLM-5.3 Flash GSQ-RCO 3.0-bit, experts streamed from NVMe ³ | DFlash2 | 2 | `GLM-5.3-Flash-DFlash2-Q8_0.gguf` | `--moe-cache 1536 -fa off -c 2048 -b 8 -ub 8` + `GGML_ARIFI_MOE_NVME=1` | 2.96 (chat) | 4.4–4.6 | tie (+0.3%), output identical |

Every row's full command line is in [How we run it](#how-we-run-it-radeon-890m). **Since this release** (separate
ABBA A/Bs on the same build, not yet in the table): draft depth 6 is faster on both 27B files - Q4_K_XL DFlash2 11.63 →
13.28 t/s short (+14.2%), long +1.2%; S-X8 DFlash2 short +8.4%, long +3.6%, output identical. The next release moves
both rows to depth 6.

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

## How we run it (Radeon 890M)

Every row of the 890M table above, with the exact server flags of the arm that produced it. One block per model:
the base line, then what each mode adds to it. Every line starts with `llama-server -m <model file>`; port, host and
CPU-thread flags (`-t`, `-tb`) are left out. "Draft depth" is `--spec-draft-n-max`: the most tokens the drafter
proposes per step. Every change marked "on" in [What is new](#what-is-new-in-this-release) is active in these lines with
no flag; its `=0` environment switch only turns it off, so none appear below.

**Qwen3.8-Flash-Next · GSQ-RCO IQ3_S** (ISTA-DASLab) · file `Qwen3.8-Flash-Next-GSQ-RCO-IQ3_S-00001-of-00002.gguf`
Base: `-ngl 999 -np 1 -fa on --lazy-mode on -c 4096 -lm dio`

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | - (A/B arm ⁴: `-c 2048 -lm mmap` instead of `-c 4096 -lm dio`) | 12.01 / 11.89 (in-cell arm: 11.89 / 11.77) |
| MTP | 3 | `mtp-sidecar-qwen4exp-q8_0.gguf` (MTP sidecar) | `-md mtp-sidecar-qwen4exp-q8_0.gguf -ngld 999 --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | 19.68 / 17.70 |
| MTP + 40K draft vocabulary | 3 | `mtp-sidecar-qwen4exp-q8_0-dven40k.gguf` | `-md mtp-sidecar-qwen4exp-q8_0-dven40k.gguf -ngld 999 --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | **20.24 / 19.45** |

Where the MTP sidecars come from: we built both. `mtp-sidecar-qwen4exp-q8_0.gguf` is the MTP block (31 tensors) of
Qwen's official BF16 Qwen3.8-Flash-Next checkpoint, extracted and converted by us to a Q8_0 GGUF sidecar, because the
quantized GSQ-RCO files do not carry it. The `-dven40k` variant adds a 40,525-token English draft vocabulary, a design
taken from Strata and re-implemented here.

**Ornith-1.5 35B-A3B MoE · Q4_K_M** (ornith-ai) · file `Ornith-1.5-35B-Q4_K_M.gguf`
Base: `-ngl 999 -np 1 -fa on -c 4096 -ub 512 -b 2048 -lm dio --lazy-mode on`

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | - | 29.33 / 28.98 |
| MTP | 3 | head inside the model file | `--spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | **34.07 / 35.01** |
| DFlash2 | 2 | `Ornith-1.5-35B-A3B-DFlash2-BF16.gguf` (jzinno) | `-md Ornith-1.5-35B-A3B-DFlash2-BF16.gguf -ngld 999 --spec-type draft-dflash --spec-draft-n-max 2 --dflash-defer-injection 0` | 32.07 / 33.60 |

**Qwen3.8 27B dense · UD-Q4_K_XL** (Huihui abliterated, huihui-ai) · file `Huihui-Qwen3.8-27B-abliterated-UD-Q4_K_XL.gguf`
Base: `-dev Vulkan0 -ngl 999 -fa on -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0 --parallel 1 --ctx-checkpoints 32 --ctx-checkpoints-device off --ctx-checkpoints-toolcall on`

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | - | 4.57 / 4.60 |
| MTP | 3 | head inside the model file | `--spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | 10.08 / 9.84 |
| DFlash2 | 3 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` (incoai) | `--spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --dflash-defer-injection 0 --spec-draft-n-max 3` | **12.09 / 11.55** |

**Qwen3.8 27B dense · S-X8 v4.3 format, 7.5 bits per weight** (MarlaLabs) · file `Qwen3.8-27B-SX8v43-id57.gguf` (retagged to our type 57 with `tools/gguf-retag-sx8/`)
Base: `-dev Vulkan0 -ngl 999 -fa on -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0 --parallel 1`

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | - | 2.94 / 2.96 |
| DFlash2 | 4 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` (incoai) | `--spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --dflash-defer-injection 0 --spec-draft-n-max 4` | 8.89 / 8.29 |

**GLM-5.3 Flash MoE · GSQ-RCO 3.0-bit, routed experts streamed from NVMe** (pfeifferj) · file `GLM-5.3-Flash-GSQ-RCO-3.0bit.gguf`
Base: `-dev Vulkan0 -ngl 999 --no-host --no-op-offload --no-warmup -fit off -lm mmap -fa off -c 2048 -b 8 -ub 8 -np 1 --moe-cache 1536` plus the environment in note ⁵

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s |
|---|---|---|---|---|
| DFlash2 | 2 | `GLM-5.3-Flash-DFlash2-Q8_0.gguf` (Anbeeld) | `-md GLM-5.3-Flash-DFlash2-Q8_0.gguf -devd Vulkan0 -ngld 99 --spec-type draft-dflash --spec-draft-n-max 2` | 2.96 (chat) |

⁴ In the A/B arms the test harness held the server's Windows working set to 4.29 GB. That is a job limit set
outside the server, not a server flag. ⁵ The GLM line also needs these environment variables:
`GGML_ARIFI_MOE_NVME=1 GGML_ARIFI_MOE_NVME_CACHE_MIB=1536 GGML_ARIFI_MOE_NVME_IO_LANES=8 GGML_ARIFI_MOE_NVME_BATCH_READ=1
GGML_ARIFI_MOE_NVME_RESIDENT_MIB=61440 GGML_ARIFI_MOE_NVME_POLICY=heat GGML_ARIFI_MOE_DECODE_MAX_TOKENS=3
GGML_ARIFI_VK_MOE_CACHE=v1 GGML_ARIFI_VK_MOE_MV=coop GGML_ARIFI_VK_MOE_MV_MULTI=0 GGML_ARIFI_VK_SHARED_BUDGET_MIB=1024
POWERINFER_NO_BUFFERING=1 GGML_SCHED_PREFETCH_EXPERTS=0`, and `GGML_ARIFI_MOE_NVME_REPLICAS` names a list of two
identical copies of the model file on two drives. Each server line is copied from the measured cell's own record:
[`evidence/b11178-x1i2/`](evidence/b11178-x1i2/) `sg-*-state.json` (field `args`). The GLM cell's command record is not
in this repository; its result is [`glm-shipgate-VERDICT.txt`](evidence/b11178-x1i2/glm-shipgate-VERDICT.txt).

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
  --ctx-checkpoints 32 --ctx-checkpoints-device off --ctx-checkpoints-toolcall on `
  --spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --dflash-defer-injection 0 --spec-draft-n-max 3
```

The two serve lines are the measured cells' command lines (`evidence/b11178-x1i2/sg-fn-*-state.json`,
`sg-q27-*-state.json`; port, host and thread flags left out). Every other model and mode is in
[How we run it](#how-we-run-it-radeon-890m). The 27B drafter is incoai's `Qwen3.8-27B-DFlash2-GGUF` (Q4_K_M).
Toolchain, flags and the MinGW runtime step are in [`docs/BUILDING.md`](docs/BUILDING.md). At startup the Vulkan
backend prints one line for each tuned default it applied on your GPU.

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
Vulkan backend against the CPU reference, with 0 failures.

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

One more KV cache format runs on the CPU only: **TBQ3_0 and TBQ4_0** (types 58/59, from jtrefon's
llama.cpp-turboq-mtp). A `tbq3_0` V cache is refused unless `LLAMA_ALLOW_TBQ3_KV=1` is set.

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
- **Draft recent-token penalty:** MTP drafting de-weights its most recent tokens (on by default;
  `LLAMA_SPEC_DRAFT_RECENT_PENALTY=0:0` turns it off).
- **DFlash2 options:** `--dflash-defer-injection` and the fused encoder-injection switch (`LLAMA_DFLASH_FUSED_INJECT`).
- **Context checkpoints:** `--ctx-checkpoints-device` keeps the checkpoint ring in the GPU reservation with pooled
  recurrent-state slots, and `--ctx-checkpoints-toolcall` anchors a checkpoint at each tool call.
- **GLM-5.3 Flash with NVMe expert streaming** (`GGML_ARIFI_MOE_NVME=1`): routed experts are read from the drive.
- **Bigger-than-RAM MoE:** PowerInfer expert streaming from SSD (`EXPERT_BUNDLE_PATH`) runs a 35B MoE
  model (Qwen3.6-35B-A3B) with its experts read from an on-disk bundle.
- **MoE expert cache on Vulkan:** `--moe-cache` keeps recently used experts in the GPU reservation when the routed
  experts stay in host memory, with a dedicated Vulkan shader and heat-protected eviction. It is the
  ggml-backend provider from TheTom's turboquant tree. See [`docs/backend/MOE-CACHE.md`](docs/backend/MOE-CACHE.md).
- **Placement into the GPU reservation** on the Radeon 890M (`GGML_VK_UMA_PLACEMENT`), plus a
  per-heap allocator that serves 27B drafted lines which used to fail at load.
- **K-cache mean-centering** for `q4_0` K caches: opt-in at build time (`-DGGML_ARIFI_KV_MEANCENTER=ON`), then
  `--kv-mean-center`. See [`docs/kv-mean-center.md`](docs/kv-mean-center.md).

## Where each improvement lives

One row per improvement, by the layer it changes. Paths are in this repository.

| Layer | Improvement | Where |
|---|---|---|
| Type | S-X8 v4.3 (type 57): CPU decoder, Vulkan kernels, packed prefill tile, int8 coopmat1 prompt path | [`dequant_sx8.comp`](ggml/src/ggml-vulkan/vulkan-shaders/dequant_sx8.comp), [`mul_mat_vec_sx8.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_sx8.comp), [`mul_mm_funcs.glsl`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mm_funcs.glsl), [`ggml-cpu/quants.c`](ggml/src/ggml-cpu/quants.c) |
| Type | ROCmFP4, ROCmFP4-FAST, ROCmFPX FP2/FP3/FP6/FP8 | [`ggml/rocmfp4/`](ggml/rocmfp4/), [`ggml/rocmfpx/`](ggml/rocmfpx/), [`dequant_rocmfp4_fast.comp`](ggml/src/ggml-vulkan/vulkan-shaders/dequant_rocmfp4_fast.comp), [`dequant_rocmfpx_fp8.comp`](ggml/src/ggml-vulkan/vulkan-shaders/dequant_rocmfpx_fp8.comp) and siblings |
| Type | TQ3_1S, TQ4_1S, TQ3_4S weights (Vulkan, CPU, CUDA, Metal) | [`mul_mat_vec_tq_sg.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_tq_sg.comp), [`tq_rotate_act.comp`](ggml/src/ggml-vulkan/vulkan-shaders/tq_rotate_act.comp), [`ggml-arifi-turbo-weights.c`](ggml/src/ggml-arifi-turbo-weights.c), [`tq3-native.cu`](ggml/src/ggml-cuda/tq3-native.cu), [`ggml-metal.metal`](ggml/src/ggml-metal/ggml-metal.metal) |
| Type | TurboQuant KV cache (turbo2/3/4) | [`dequant_turbo3_0.comp`](ggml/src/ggml-vulkan/vulkan-shaders/dequant_turbo3_0.comp), [`turbo_wht.comp`](ggml/src/ggml-vulkan/vulkan-shaders/turbo_wht.comp) |
| Type | TBQ3_0 / TBQ4_0 KV cache (types 58/59, CPU) | [`ggml-tbq-quant.c`](ggml/src/ggml-tbq-quant.c), [`ggml-cpu/ops.cpp`](ggml/src/ggml-cpu/ops.cpp) |
| Type | Q2_0_G128 ternary (type 43) and its CPU VNNI repack | [`dequant_q2_0_g128.comp`](ggml/src/ggml-vulkan/vulkan-shaders/dequant_q2_0_g128.comp), [`arch/x86/repack.cpp`](ggml/src/ggml-cpu/arch/x86/repack.cpp) |
| Type | Escha-W2 (types 55/56), fused `GGML_OP_ESCHA_MM` | [`escha_mm.comp`](ggml/src/ggml-vulkan/vulkan-shaders/escha_mm.comp), [`ggml-cpu/ops.cpp`](ggml/src/ggml-cpu/ops.cpp) |
| Kernel | IQ4_XS 16-thread mat-vec (upstream's kept as a switch) and its q8_1 MMVQ admit | [`mul_mat_vec_iq4_xs.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_iq4_xs.comp), [`mul_mat_vec_iq4_xs_up.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_iq4_xs_up.comp) |
| Kernel | IQ3_S / IQ3_XXS: 16 threads per superblock, sign hoist, width-6 and width-7 row counts | [`mul_mat_vec_iq3_s.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_iq3_s.comp), [`mul_mat_vec_iq3_xxs.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_iq3_xxs.comp) |
| Kernel | Q5_K activation hoist, Q6_K direct scales and x-fold | [`mul_mat_vec_q5_k.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_q5_k.comp), [`mul_mat_vec_q6_k.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_q6_k.comp) |
| Kernel | q8_1 MMVQ: A-side hoist, Q4_K width-5 split, Q5_K / Q6_K / ROCmFP4-FAST routes, admit width and wide rows | [`mul_mat_vecq.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vecq.comp), [`mul_mat_vecq_funcs.glsl`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vecq_funcs.glsl) |
| Kernel | MoE expert mat-vec in slot-major order, IQ3_S two rows, IQ2_S packed signs | [`mul_mat_vec_base.glsl`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_base.glsl), [`mul_mat_vec_iq2_s.comp`](ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec_iq2_s.comp) |
| Kernel | Recurrent-state update in place (GDN state bank) | [`gated_delta_net.comp`](ggml/src/ggml-vulkan/vulkan-shaders/gated_delta_net.comp), [`ggml-vulkan.cpp`](ggml/src/ggml-vulkan/ggml-vulkan.cpp) |
| Kernel | Tiled concat-transpose for the delta-net conv state | [`concat_transpose.comp`](ggml/src/ggml-vulkan/vulkan-shaders/concat_transpose.comp) |
| Kernel | Fused hyper-connection post step, sparse attention at prefill, masked-KV fix in coopmat1 flash attention, coopmat1 width rule and f16 staging, flash-attention dequant-KV switch, MoE mat-vec rows switch | [`ggml-vulkan.cpp`](ggml/src/ggml-vulkan/ggml-vulkan.cpp) |
| Memory | Placement into the GPU reservation, per-heap allocator, host-split memory type, UMA read path | [`ggml-vulkan.cpp`](ggml/src/ggml-vulkan/ggml-vulkan.cpp), [`ggml-vulkan-buffers.cpp`](ggml/src/ggml-vulkan/ggml-vulkan-buffers.cpp) |
| Model graph | Embedding-row prefetch and release, MTP sidecar head and 40K draft vocabulary, Flash-Next layout copies | [`src/models/qwen4exp.cpp`](src/models/qwen4exp.cpp) |
| Memory | Pooled indexer-key cache for long-context decode | [`llama-memory-hybrid-idx.cpp`](src/llama-memory-hybrid-idx.cpp) |
| Memory | f16 recurrent state (`GGML_RECURRENT_STATE_F16=1`), KV n-gram index switch | [`llama-model.cpp`](src/llama-model.cpp), [`llama-kv-cache.cpp`](src/llama-kv-cache.cpp) |
| MoE | Expert cache with heat-protected eviction (`--moe-cache`) | [`ggml-backend-moe-cache.h`](ggml/src/ggml-backend-moe-cache.h), [`ggml-vulkan-moe-cache.cpp`](ggml/src/ggml-vulkan/ggml-vulkan-moe-cache.cpp), [`moe_cache_mv.comp`](ggml/src/ggml-vulkan/vulkan-shaders/moe_cache_mv.comp) |
| MoE | GLM-5.3 Flash NVMe expert streaming | [`llama-moe-disk.h`](src/llama-moe-disk.h), [`ggml-vulkan-moe-cache.cpp`](ggml/src/ggml-vulkan/ggml-vulkan-moe-cache.cpp), [`ggml-backend.cpp`](ggml/src/ggml-backend.cpp) |
| MoE | PowerInfer SSD expert streaming, bundle writer, pipeline reuse, Windows port; host-transfer expert prefetch | [`llama-graph.cpp`](src/llama-graph.cpp), [`llama-model.cpp`](src/llama-model.cpp), [`ggml-cpu/repack.cpp`](ggml/src/ggml-cpu/repack.cpp), [`ggml-backend.cpp`](ggml/src/ggml-backend.cpp) |
| Loader | Direct-I/O load on Windows (`-lm dio`) | [`llama-mmap.cpp`](src/llama-mmap.cpp) |
| Loader | S-X8 PCA companion correction | [`llama-model-loader.cpp`](src/llama-model-loader.cpp), [`tools/sx8/`](tools/sx8/) |
| Loader | llama-quantize slab reads on Windows | [`llama-quant.cpp`](src/llama-quant.cpp) |
| Server | Adaptive draft length, draft recent-token penalty, DFlash2 fused-inject switch | [`common/speculative.cpp`](common/speculative.cpp) |
| Server | `--dflash-defer-injection`, `--spec-draft-adaptive`, `--moe-cache` flags | [`common/arg.cpp`](common/arg.cpp) |
| Server | Device context-checkpoint ring and tool-call anchor | [`server-context.cpp`](tools/server/server-context.cpp) |
| Tools | GGUF retag tools, K-cache mean-centering, op and determinism probes, patch-series tooling | [`tools/gguf-retag-sx8/`](tools/gguf-retag-sx8/), [`tools/gguf-retag-tq3/`](tools/gguf-retag-tq3/), [`tools/gguf-retag-g128/`](tools/gguf-retag-g128/), [`tools/kv-mean-center/`](tools/kv-mean-center/), [`tools/arifi-op-probe/`](tools/arifi-op-probe/), [`tools/arifi-sync/`](tools/arifi-sync/) |
| Diagnostics | Per-op profiling (`LANE110_PROF`), expert prefetch cap, mat-vec traces and row overrides (`GGML_ARIFI_G128_MMV_ROWS`, `GGML_ARIFI_Q6K_XFOLD`, `GGML_ARIFI_MMVQ_TRACE`), staging and host-split limits | [`llama-context.cpp`](src/llama-context.cpp), [`llama-graph.cpp`](src/llama-graph.cpp), [`ggml-vulkan.cpp`](ggml/src/ggml-vulkan/ggml-vulkan.cpp), [`ggml-vulkan-buffers.cpp`](ggml/src/ggml-vulkan/ggml-vulkan-buffers.cpp); all switches in [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md) |
| Build | CI that compiles CUDA, Metal, SYCL and Vulkan with the fork's formats on | [`arifi-backends-matrix.yml`](.github/workflows/arifi-backends-matrix.yml) |

## History

**The fork started in July 2026.** Its first commit, which sets up its identity and notices, is dated 22 July 2026.
Release `b11178-x1i2` carries 828 fork commits on top of upstream `b11178`.

**When each model ran here.** Qwen3.8-Flash-Next: served on our fork from late August 2026; our MTP drafting with a
sidecar head followed on 5 October. GLM-5.3 Flash: served on the Radeon 890M on 2 October 2026, routed experts streamed
from NVMe, built on upstream's model code (#27773). Upstream llama.cpp's v0.6.0 release announced both models on
5 October 2026. NVMe expert streaming for GLM-5.3 Flash on an integrated GPU is ours.

**Two machines.** Both are mini-PCs with an integrated GPU on one RAM pool.

| Machine | GPU | Memory | In use |
|---|---|---|---|
| Beelink SER7 Pro (Ryzen 7 7840HS) | Radeon 780M (RDNA3) | DDR5-5600, 16 GB reserved for the GPU | until 30 September 2026 |
| Minisforum AI X1 Pro-470 (Ryzen AI 9 HX 470) | Radeon 890M (RDNA 3.5) | 96 GB DDR5-5600, 72 GB reserved for the GPU | from 30 September 2026 |

**Releases.** The fork follows upstream: its served build moved to upstream `b10524` on 20 August, `b10680` by
2 September, `b10819` on 6 September, `b10825` on 7 September and `b11178` on 3 October. Each headline names its
machine; no number moves between the 780M and the 890M.

| Release | Date | Upstream base | What it added | Headline served number | Receipts |
|---|---|---|---|---|---|
| `b10825-r73i` | 2026-09-22 | `b10825` | First public release. A linear patch series that `series check` replays and verifies. Seven tracked sources: PowerInfer SSD expert streaming, PrismML Q2_0_G128 with its VNNI repack, the ROCmFPX formats, TQ3_4S, TurboQuant KV with TQ3_1S / TQ4_1S and their Vulkan kernels, plus three host-transfer prefetch patches | none: a capability release | that branch's `README.md` |
| `b10825-r86i-x1` | 2026-10-01 | `b10825` | Vulkan kernels for every fork format (`test-backend-ops`, 0 failures), mat-vec kernels for verify widths 2-8, S-X8 kernels, the ROCmFP4-FAST q8_1 route (opt-in), stable DFlash2 and MTP on Qwen3.8, adaptive draft length, the Vulkan MoE expert cache, placement into the 890M's GPU reservation | Radeon 780M: Qwen3.8 27B ROCmFP4-FAST, DFlash2, **9.518 t/s** (from 7.498, +26.9%) | the 780M table above |
| `x1i` (not published) | 2026-10-03 | `b11178` | The base move to `b11178` with upstream's int8 coopmat1 path, S-X8 prompt reading on int8 coopmat1, prompt images in device snapshots, the ROCmFP4-FAST route on by default | Radeon 890M: Ornith-1.5 35B, DFlash2, **29.51 / 31.23 t/s**; Qwen3.8 27B Q4_K_XL, DFlash2, 10.46 / 10.61 t/s | the comparator arms (`x`) in [`evidence/b11178-x1i2/`](evidence/b11178-x1i2/) `sg-orn-*`, `sg-q27-*` |
| `b11178-x1i2` | 2026-10-07 | `b11178` | MTP for Qwen3.8-Flash-Next with a 40K draft vocabulary, GDN state bank, fused hyper-connection step, sparse prefill attention and the pooled indexer cache, embedding-row prefetch and release, repeatable greedy decode, slot-major MoE mat-vec, GLM-5.3 Flash NVMe streaming, Windows direct-I/O load, quantize slab reads | Radeon 890M: Ornith-1.5 35B, MTP, **34.07 / 35.01 t/s**; Qwen3.8-Flash-Next, MTP + 40K vocabulary, 20.24 / 19.45; Qwen3.8 27B Q4_K_XL, DFlash2, 12.09 / 11.55 | [`evidence/b11178-x1i2/`](evidence/b11178-x1i2/) |

On the 890M, from `x1i` to this release: Qwen3.8 27B Q4_K_XL with DFlash2 rose 15.5% (short prompt) and 8.7% (long),
and Ornith-1.5 35B rose from 29.51 / 31.23 t/s (DFlash2) to 34.07 / 35.01 t/s (MTP). Short / long = a ~12-token and
a ~600-token prompt, means of 4 rounds (8 for MTP).

## For format authors and researchers

You were sent this link because your format runs here with Vulkan kernels. Find your format below.

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
- **Runtime switches.** Each tuned default has an environment switch that restores the old path. The tables on this
  page name each one; [`docs/OPTIONS-REGISTRY.md`](docs/OPTIONS-REGISTRY.md) holds the earlier ones with their evidence.
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
