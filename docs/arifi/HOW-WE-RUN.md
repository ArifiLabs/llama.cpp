# How we run each model

Every model and mode we measured, on both machines, with every argument, switch and environment
variable we used, and the reason for each one. Environment variables in full: [ENVIRONMENT.md](ENVIRONMENT.md).

## How to read this page

- Each block gives a **base line**; each mode row lists only what it **adds** to that line.
- **Draft depth** = `--spec-draft-n-max`: the most tokens the drafter proposes per step.
- **Decode t/s** = tokens per second while generating, after a short (~12-token) / long (~600-token) prompt.
- Drafted modes give the same greedy output as plain: the main model verifies every drafted token.

<!-- src: README.md "How we run it (Radeon 890M)"; common/arg.cpp (--spec-draft-n-max) -->

## Radeon 890M (current machine)

Numbers: the v0.1.3.1 gate, 2026-10-09, this release's arm ("point" column of
[`evidence/v0.1.3.1/pg-*-READ.txt`](../../evidence/v0.1.3.1/)).

Machine: Ryzen AI 9 HX 470, Radeon 890M (RDNA 3.5, gfx1150), 96 GB DDR5-5600 with 72 GB reserved for the GPU,
Windows 11. Each row is an A/B against v0.1.3.0 on the same file: 2 timed rounds per launch (short and long prompt),
2 launches per arm for Flash-Next and Ornith-1.5 (order ABBA), 1 per arm for the 27B lines; means shown.

The lines below are copied from each measured arm's own record (`evidence/v0.1.3.1/pg-*-state.json`,
field `args`). Each starts with `llama-server -m <model file>`; port and host are left out.
The arms in the tables below set **no environment variables** (`env` is empty); only GLM-5.3 needs some.

<!-- src: README.md "Speed: Radeon 890M"; evidence/v0.1.3.1/pg-fn-10082312-state.json, pg-orn-10090030-state.json, pg-q27-10090115-state.json, pg-sx8-10090129-state.json (args, env); evidence/v0.1.3.1/pg-*-READ.txt (point column) -->

### Qwen3.8-Flash-Next, GSQ-RCO IQ3_S (ISTA-DASLab)

- File: `Qwen3.8-Flash-Next-GSQ-RCO-IQ3_S-00001-of-00002.gguf` (+ part 2)
- Download: [ISTA-DASLab/Qwen3.8-Flash-Next-GSQ-RCO-GGUF](https://huggingface.co/ISTA-DASLab/Qwen3.8-Flash-Next-GSQ-RCO-GGUF)
- MTP sidecars: built by us, see [`tools/qwen4exp-mtp-sidecar/`](../../tools/qwen4exp-mtp-sidecar/README.md)
- Base: `-ngl 999 -np 1 -fa on --lazy-mode on -c 4096 -lm dio`

| Mode | Draft depth | Drafter file | Added flags | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | none | 11.84 / 11.72 |
| MTP | 3 | `mtp-sidecar-qwen4exp-q8_0.gguf` | `-md mtp-sidecar-qwen4exp-q8_0.gguf -ngld 999 --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | 19.44 / 17.62 |
| MTP + 40K draft vocabulary | 3 | `mtp-sidecar-qwen4exp-q8_0-dven40k.gguf` | `-md mtp-sidecar-qwen4exp-q8_0-dven40k.gguf -ngld 999 --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | 19.93 / 19.27 |

Environment variables: none.

<!-- src: README.md "How we run it (Radeon 890M)", "Models used"; evidence/v0.1.3.1/pg-fn-10082312-state.json (arms P, M, F), pg-fn-10082312-READ.txt -->

### Ornith-1.5 35B-A3B MoE, Q4_K_M (ornith-ai)

- File: `Ornith-1.5-35B-Q4_K_M.gguf` (MTP head inside the file)
- Download: [ornith-ai/Ornith-1.5-35B-A3B-GGUF](https://huggingface.co/ornith-ai/Ornith-1.5-35B-A3B-GGUF)
- DFlash2 drafter: [jzinno/Ornith-1.5-35B-A3B-DFlash2](https://huggingface.co/jzinno/Ornith-1.5-35B-A3B-DFlash2) (safetensors; we converted it to a BF16 GGUF)
- Base: `-ngl 999 -np 1 -fa on -c 4096 -ub 512 -b 2048 -lm dio --lazy-mode on`

| Mode | Draft depth | Drafter file | Added flags | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | none | 29.18 / 28.78 |
| MTP | 3 | head inside the model file | `--spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | 33.74 / 34.45 |
| DFlash2 | 2 | `Ornith-1.5-35B-A3B-DFlash2-BF16.gguf` | `-md Ornith-1.5-35B-A3B-DFlash2-BF16.gguf -ngld 999 --spec-type draft-dflash --spec-draft-n-max 2 --dflash-defer-injection 0` | 31.97 / 33.43 |

Environment variables: none.

<!-- src: README.md "How we run it (Radeon 890M)", "Models used"; evidence/v0.1.3.1/pg-orn-10090030-state.json (arms P, M, D), pg-orn-10090030-READ.txt -->

### Qwen3.8 27B dense, UD-Q4_K_XL (Huihui abliterated, huihui-ai)

- File: `Huihui-Qwen3.8-27B-abliterated-UD-Q4_K_XL.gguf` (MTP head inside the file)
- Download: [huihui-ai/Huihui-Qwen3.8-27B-abliterated-GGUF](https://huggingface.co/huihui-ai/Huihui-Qwen3.8-27B-abliterated-GGUF)
- DFlash2 drafter: [incoai/Qwen3.8-27B-DFlash2-GGUF](https://huggingface.co/incoai/Qwen3.8-27B-DFlash2-GGUF), file `Qwen3.8-27B-DFlash2-Q4_K_M.gguf`
- Base: `-dev Vulkan0 -ngl 999 -fa on -c 8192 -b 1024 -ub 512 -t 16 -tb 32 -ctk q8_0 -ctv q8_0 --parallel 1 --ctx-checkpoints 32 --ctx-checkpoints-device off --ctx-checkpoints-toolcall on`

| Mode | Draft depth | Drafter file | Added flags | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | none | 4.62 / 4.61 |
| MTP | 3 | head inside the model file | `--spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | 10.16 / 9.90 |
| DFlash2 | 6 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | `--spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --dflash-defer-injection 0 --spec-draft-n-max 6` | 13.62 / 11.55 |

Environment variables: none. The README leaves `-t 16 -tb 32` out of its lines; the measured records carry them.
This release moves the DFlash2 row from depth 3 to depth 6: short-prompt decode 12.09 → 13.62 t/s (+12.6%).
The depth-3 number is the mean of the v0.1.3.0 rounds 12.11 / 12.06 / 12.10 / 12.08 (DFlash2, short, the values
after the arrow) in [`sg-q27-10062103-shipgate-read.txt`](../../evidence/b11178-x1i2/sg-q27-10062103-shipgate-read.txt).

<!-- src: README.md "How we run it (Radeon 890M)", "Speed: Radeon 890M", "Models used"; evidence/v0.1.3.1/pg-q27-10090115-state.json (arms P, M, D), pg-q27-10090115-READ.txt; evidence/b11178-x1i2/sg-q27-10062103-shipgate-read.txt (depth 3: 12.11/12.06/12.10/12.08) -->

### Qwen3.8 27B dense, S-X8 v4.3, 7.5 bits per weight (MarlaLabs)

- File: `Qwen3.8-27B-SX8v43-id57.gguf` (the download is `Qwen3.8-27B-SX8v43.gguf`; retag it once with `python tools/gguf-retag-sx8/retag_sx8.py`)
- Download: [marlalabsAI/Qwen3.8-27B-SX8](https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8)
- DFlash2 drafter: `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` (incoai, link above)
- Base: `-dev Vulkan0 -ngl 999 -fa on -c 8192 -b 1024 -ub 512 -t 16 -tb 32 -ctk q8_0 -ctv q8_0 --parallel 1`

| Mode | Draft depth | Drafter file | Added flags | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | none | 2.96 / 2.95 |
| DFlash2 | 6 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | `--spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --dflash-defer-injection 0 --spec-draft-n-max 6` | 9.54 / 8.57 |

Environment variables: none. This release moves the DFlash2 row from depth 4 to depth 6: short-prompt decode
8.89 → 9.54 t/s (+7.3%). The depth-4 number is the mean of the v0.1.3.0 rounds 8.88 / 8.89 / 8.89 / 8.89 (DFlash2,
short, the values after the arrow) in
[`sg-sx8-10062125-shipgate-read.txt`](../../evidence/b11178-x1i2/sg-sx8-10062125-shipgate-read.txt).

<!-- src: README.md "How we run it (Radeon 890M)", "Speed: Radeon 890M", "Models used"; evidence/v0.1.3.1/pg-sx8-10090129-state.json (arms P, D), pg-sx8-10090129-READ.txt; evidence/b11178-x1i2/sg-sx8-10062125-shipgate-read.txt (depth 4: 8.88/8.89/8.89/8.89) -->

### GLM-5.3 Flash MoE, GSQ-RCO 3.0-bit, routed experts streamed from NVMe (pfeifferj)

- File: `GLM-5.3-Flash-GSQ-RCO-3.0bit.gguf`
- Download: [pfeifferj/GLM-5.3-Flash-GSQ-RCO-GGUF](https://huggingface.co/pfeifferj/GLM-5.3-Flash-GSQ-RCO-GGUF)
- DFlash2 drafter: [Anbeeld/GLM-5.3-Flash-DFlash2-GGUF](https://huggingface.co/Anbeeld/GLM-5.3-Flash-DFlash2-GGUF), file `GLM-5.3-Flash-DFlash2-Q8_0.gguf`
- Base: `-dev Vulkan0 -ngl 999 --no-host --no-op-offload --no-warmup -fit off -lm mmap -fa off -c 2048 -b 8 -ub 8 -np 1 --moe-cache 1536`

| Mode | Draft depth | Drafter file | Added flags | Decode t/s |
|---|---|---|---|---|
| DFlash2 | 2 | `GLM-5.3-Flash-DFlash2-Q8_0.gguf` | `-md GLM-5.3-Flash-DFlash2-Q8_0.gguf -devd Vulkan0 -ngld 99 --spec-type draft-dflash --spec-draft-n-max 2` | 2.96 (chat) |

Environment variables, as the README lists them for this line (what each does: [ENVIRONMENT.md](ENVIRONMENT.md)):

| Variable | Value |
|---|---|
| `GGML_ARIFI_MOE_NVME` | `1` |
| `GGML_ARIFI_MOE_NVME_CACHE_MIB` | `1536` (must equal `--moe-cache`) |
| `GGML_ARIFI_MOE_NVME_IO_LANES` | `8` |
| `GGML_ARIFI_MOE_NVME_BATCH_READ` | `1` |
| `GGML_ARIFI_MOE_NVME_RESIDENT_MIB` | `61440` |
| `GGML_ARIFI_MOE_NVME_POLICY` | `heat` |
| `GGML_ARIFI_MOE_DECODE_MAX_TOKENS` | `3` |
| `GGML_ARIFI_VK_MOE_CACHE` | `v1` |
| `GGML_ARIFI_VK_MOE_MV` | `coop` |
| `GGML_ARIFI_VK_MOE_MV_MULTI` | `0` (this build has no read of this name; it has no effect) |
| `GGML_ARIFI_VK_SHARED_BUDGET_MIB` | `1024` |
| `POWERINFER_NO_BUFFERING` | `1` (read only by the PowerInfer bundle reader, not the NVMe streaming path) |
| `GGML_SCHED_PREFETCH_EXPERTS` | `0` |
| `GGML_ARIFI_MOE_NVME_REPLICAS` | path to a manifest naming two identical copies of the model file on two drives |

The GLM cell's own command record is not in this repository. Its result is
[`glm-shipgate-VERDICT.txt`](../../evidence/b11178-x1i2/glm-shipgate-VERDICT.txt): a tie (+0.3%) against
the best earlier GLM build, 8 launches ABBA, output ids identical. Prompt reading: 4.4–4.6 t/s.

<!-- src: README.md "How we run it (Radeon 890M)" note 5, "Speed: Radeon 890M" note 3, "Models used"; evidence/b11178-x1i2/glm-shipgate-VERDICT.txt -->

## Radeon 780M (previous machine)

Machine: Radeon 780M (RDNA3, gfx1103, 12 CU) with a 16 GB BIOS reservation for the GPU, Windows 11, Balanced
power plan (the README's "Test system (Radeon 780M numbers)").

Most 780M A/Bs ran from a test harness whose full server line is not in our published records. Where
that is so, the block says **command line not recorded** and gives only what was recorded (drafter, depth).

<!-- src: README.md "Test system (Radeon 780M numbers)", "Speed: Radeon 780M", "Radeon 780M vs Radeon 890M" (12 CU, 16 GB); docs/arifi/STATUS.md "How the numbers were taken" -->

### Qwen3.8 27B dense, UD-Q4_K_XL (Huihui abliterated, huihui-ai), 17.38 GB

- File: `Huihui-Qwen3.8-27B-abliterated-UD-Q4_K_XL.gguf`
- Download: [huihui-ai/Huihui-Qwen3.8-27B-abliterated-GGUF](https://huggingface.co/huihui-ai/Huihui-Qwen3.8-27B-abliterated-GGUF)
- Drafter: `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` ([incoai](https://huggingface.co/incoai/Qwen3.8-27B-DFlash2-GGUF))

| Mode | Draft depth | Drafter file | Added flags | Decode t/s |
|---|---|---|---|---|
| plain | - | - | command line not recorded | 3.864 → 4.104 (build A/B, medians, +6.2%) |
| DFlash2 | 2 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | command line not recorded | 7.085 → 7.578 (build A/B, medians, +6.9%) |
| DFlash2 | 4 | same | command line not recorded | 7.197 (depth table); launch-level 7.208 → 7.192 |
| DFlash2 | 5 | same | command line not recorded | 6.535 / 6.785 (two legs) |
| DFlash2 | 6 | same | command line not recorded | 6.545 |

Environment variables: none recorded.

<!-- src: README.md "Speed: Radeon 780M"; evidence/r86-evidence__81-depth-table.txt, evidence/r86-evidence__80-launchlevel.txt, evidence/chain151__c29-q4kxl-27b-launchlevel-AB.json (drafter) -->

### Qwen3.8 27B dense, GSQ-RCO IQ3_S (ISTA-DASLab)

- File: `Qwen3.8-27B-GSQ-RCO-IQ3_S-mtp.gguf`
- Download: not in the README's download table (publisher repository `ISTA-DASLab/Qwen3.8-27B-GSQ-RCO-GGUF` per our A/B record)
- Drafter: `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` (incoai)
- Command line not recorded.

| Mode | Draft depth | Drafter file | Added flags | Decode t/s |
|---|---|---|---|---|
| plain | - | - | command line not recorded | 5.142 |
| DFlash2 | 4 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | command line not recorded | 8.118 (best depth); launch-level 7.988 → 7.925, tie |
| DFlash2 | 5 | same | command line not recorded | 7.255 / 7.252 (two legs) |
| DFlash2 | 6 | same | command line not recorded | 7.134 |

Environment variables: none recorded.

<!-- src: README.md "Radeon 780M vs Radeon 890M"; docs/arifi/STATUS.md "Results that showed no gain"; evidence/r86-evidence__81-depth-table.txt, evidence/r86-evidence__80-launchlevel.txt, evidence/chain151__c31-gsq3s-27b-launchlevel-AB.json -->

### Qwen3.8 27B dense, ROCmFP4-FAST (julianmb), 14.56 GB

- File: `Qwen3.8-27B-ROCmFP4-FAST.gguf`
- Download: [julianmb/Qwen-3.8-27B-ROCmFP4-FAST-GGUF](https://huggingface.co/julianmb/Qwen-3.8-27B-ROCmFP4-FAST-GGUF)
- Needs a build with `-DGGML_ARIFI_ROCMFPX_FORMATS=ON` (the quick-start build has it).

| Mode | Draft depth | Drafter file | Added flags | Decode t/s, build before → after |
|---|---|---|---|---|
| plain | - | - | command line not recorded | 5.084 → 5.086, tie |
| DFlash2 | 2 | not recorded | command line not recorded | 7.498 → 9.518 (means), +26.9% |

Environment variables: none recorded. The +26.9% is the q8_1 mat-vec route, now on by default
(`GGML_ARIFI_ROCMFP4_MMVQ=0` turns it off).

<!-- src: README.md "Speed: Radeon 780M", "Models used", "Faster kernels"; docs/arifi/STATUS.md protected wins (vulkan-rocmfp4-fast-q8_1-mmvq) -->

### Qwen3.8 27B dense, UD-Q3_K_XL Unleashed (outsourc-e), 13.22 GB

- File: `Qwen3.8-27B-Unleashed-UD-Q3_K_XL.gguf`
- Download: [outsourc-e/Qwen3.8-27B-Unleashed-GGUF](https://huggingface.co/outsourc-e/Qwen3.8-27B-Unleashed-GGUF)

| Mode | Draft depth | Drafter file | Added flags | Decode t/s, build before → after |
|---|---|---|---|---|
| plain | - | - | command line not recorded | 5.399 → 5.405, tie |
| DFlash2 | 2 | not recorded | command line not recorded | 8.367 → 9.268 (medians), +10.7% |

Environment variables: none recorded.

<!-- src: README.md "Speed: Radeon 780M", "Models used"; docs/arifi/STATUS.md protected wins (vulkan-iq3s-matvec-tpb16-union-gate) -->

### Qwen3.8 27B dense, S-X8 v4.3 (MarlaLabs), 26.14 GB

- File: `Qwen3.8-27B-SX8v43-id57.gguf` (retagged; see the 890M block)
- Download: [marlalabsAI/Qwen3.8-27B-SX8](https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8)
- Drafter: `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` (incoai)

| Mode | Draft depth | Drafter file | Added flags | Decode t/s |
|---|---|---|---|---|
| plain | - | - | command line not recorded | 1.831 → 2.247 (build A/B, medians), +22.7% |
| DFlash2 | 2 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | command line not recorded | 3.334 → 4.475 (build A/B, medians), +34.2% |
| DFlash2 | 4 | same | command line not recorded | 5.551 (2026-09-15 build) |

Environment variables: none recorded. On this file depth 6 ties depth 4 and depth 8 loses. The build
before these kernels could not load this file with a drafter; a per-heap allocator fix cured it.

<!-- src: README.md "Speed: Radeon 780M", "For format authors and researchers"; docs/arifi/STATUS.md "How the numbers were taken", "Results that showed no gain"; evidence/chain151__c30-sx8-27b-launchlevel-AB.json (drafter) -->

### Other 780M measurements (format kernels)

| Model / format | Mode | Result | Command line |
|---|---|---|---|
| Qwen3.8 27B IQ4_XS (file not named in the record) | DFlash2 | +0.6167 t/s mean paired difference, 8 of 8 cells (dedicated IQ4_XS mat-vec) | not recorded |
| Qwen3.8 27B TQ3_4S (file not named in the record) | `llama-bench` tg32 | 0.67 → 1.80 t/s | not recorded |
| 0.5B TQ4_1S / TQ3_1S (file not named in the record) | `llama-bench` tg64 | 44.69 → 126.65 / 45.46 → 107.10 t/s | not recorded |
| Ternary-Bonsai-8B, Q2_0_G128 (publisher not named in the record), CPU only | plain | 2.01 → 6.69 t/s decode with `GGML_ARIFI_VNNI_REPACK=1` (6.67) or `=2` (6.69) | `-ngl 0 --no-host -c 2048 -t 8` |

<!-- src: README.md "Faster kernels for formats you already use"; docs/arifi/STATUS.md protected wins; docs/OPTIONS-REGISTRY.md GGML_ARIFI_VNNI_REPACK rows -->

### What differs between the two machines

- **Same kernel settings.** The Vulkan device probe reads subgroup and wave properties, not the GPU
  name. It classes both the 780M and the 890M as RDNA3, so both get the same mat-vec rows, q8_1 route
  and width splits for n = 1 to 8. The startup log prints `device-probe (RDNA3)` on each.
- **Two device differences.** Flash-attention work splitting scales with the compute-unit count (12 on
  the 780M, 16 on the 890M). Placement into the GPU reservation (`GGML_VK_UMA_PLACEMENT`) is on by
  device id for the 890M only.
- **Where defaults were tuned.** The width defaults were tuned on the 780M. The int8 coopmat1 prompt
  path and the slot-major MoE mat-vec were measured on the 890M only.
- **Draft depth.** On the 780M the 27B DFlash2 lines peaked at depth 4; depths 5 and 6 were slower.
  On the 890M depth 6 beats depth 3: Q4_K_XL short-prompt decode 12.09 → 13.62 t/s (+12.6%).
  Start at 4 on a 780M, 6 on an 890M.
- **Memory.** The 780M machine reserved 16 GB for the GPU; a 27B Q4_K_XL with its drafter fits there.
  A 780M machine can usually raise its reservation in the BIOS ("UMA frame buffer size").

<!-- src: README.md "Radeon 780M vs Radeon 890M: what changes"; ggml/src/ggml-vulkan/ggml-vulkan.cpp:5242 (0x150e probe); evidence/b11178-x1i2/sg-q27-10062103-shipgate-read.txt (depth 3) -->

## Every switch we use, and why

"Function only" = we record what the switch does, but no A/B for its value is on record.

| Switch | What it does | Why we use it | Models |
|---|---|---|---|
| `-m <file>` | Main model file. | Required. | all |
| `-dev Vulkan0` | Runs on the first Vulkan device only. | Function only. | 27B lines, GLM |
| `-ngl 999` | Puts every layer on the GPU. | Function only (both GPUs share one RAM pool). | all GPU lines |
| `-np 1` / `--parallel 1` | One server slot. | Function only. | all |
| `-fa on` | Flash attention on. | Function only. Masked KV cells are kept out of the coopmat1 sum, which makes greedy decode repeatable (10 of 10 runs). | all but GLM |
| `-fa off` | Flash attention off. | Function only. | GLM |
| `-c N` (2048 / 4096 / 8192) | Context size. | Function only. | all |
| `-b N` (2048 / 1024 / 8) | Logical batch size. | Function only for 2048 and 1024. `-b 8` is required by NVMe expert streaming. | Ornith (2048), 27B (1024), GLM (8) |
| `-ub N` (512 / 8) | Physical batch size. | Function only for 512 (also the default). `-ub 8` is required by NVMe streaming. | Ornith, 27B, GLM |
| `-t 16 -tb 32` | CPU threads for decode / batch. | Function only. | 27B lines |
| `-lm dio` | Loads the model with direct I/O on Windows. | Flash-Next loads and serves with a 0.8–1.6 GB server working set, same output ids as mmap. | Flash-Next, Ornith |
| `-lm mmap` | Memory-maps the model. | Required by NVMe streaming (mapped expert addresses are identity keys). Flash-Next A/B arm. | GLM, Flash-Next A/B arm |
| `--lazy-mode on` | Reads rows of large tensors (per-layer embeddings) from disk on demand. | Function only. | Flash-Next, Ornith |
| `-ctk q8_0 -ctv q8_0` | KV cache in q8_0. | Function only: about half the KV memory of f16. | 27B lines |
| `--ctx-checkpoints 32` | Up to 32 context checkpoints per slot (also the code default). | Function only. | Q4_K_XL lines |
| `--ctx-checkpoints-device off` | Keeps checkpoints in host memory. The code default (`auto`) is on for hybrid models. | Function only. | Q4_K_XL lines |
| `--ctx-checkpoints-toolcall on` | Anchors a checkpoint at each tool call (also the code default). | Function only. | Q4_K_XL lines |
| `--spec-type draft-mtp` | Drafts with an MTP head (in the file, or a sidecar via `-md`). | 890M, over plain (README speed table): Flash-Next +64.1% / +50.3%, Ornith-1.5 +15.6% / +19.7%, 27B +119.9% / +114.7%. | Flash-Next, Ornith, 27B Q4_K_XL |
| `--spec-type draft-dflash` | Drafts with a DFlash2 drafter (`-md`). | 890M: Ornith-1.5 31.97 / 33.43 vs 29.18 / 28.78 plain; 27B 13.62 vs 4.62 (depth 6). | Ornith, all 27B, GLM |
| `--spec-draft-n-max N` | Draft depth. | Measured per GPU: 780M 27B peaks at 4; 890M depth 6 beats 3 by +12.6% (12.09 → 13.62). We always pin it so each line states its depth. | all drafted lines |
| `--spec-draft-p-min 0.5` | Stops a draft when the drafter's top probability falls below 0.5 (default 0.0). | No A/B of 0.5 by us. It is the best setting recorded in the MoE-cache source profile (GLM-5.2, RTX 3090). On the 780M 27B, 0.75 raised acceptance to 0.85 but lost t/s. | MTP lines |
| `--dflash-defer-injection 0` | Per-chunk encoder KV injection (default 1 = deferred). | Function only. The flag help says 0 gives higher acceptance on some models. | DFlash2 lines except GLM |
| `-md <file>` | Draft model or MTP sidecar file. | Required for DFlash2 and sidecar MTP. | drafted lines |
| `-ngld 999` / `-ngld 99` | Puts every draft-model layer on the GPU. | Function only. | drafted lines with `-md` |
| `-devd Vulkan0` | Draft model on the first Vulkan device. | Function only. | GLM |
| `--no-host` | Bypasses the host buffer. | Required by NVMe expert streaming. Also required for the CPU VNNI repack on a Vulkan build. | GLM, Q2_0_G128 CPU row |
| `--no-op-offload` | Keeps host tensor operations off the GPU. | Required by NVMe expert streaming. | GLM |
| `--no-warmup` | Skips the empty warm-up run. | Function only. | GLM |
| `-fit off` | Does not auto-adjust unset arguments to fit memory. | Function only. | GLM |
| `--moe-cache 1536` | 1536 MiB Vulkan cache for routed experts. | Required: must equal `GGML_ARIFI_MOE_NVME_CACHE_MIB`, or loading refuses. | GLM |
| `-ngl 0` | No layers on the GPU. | CPU-path measurement of the VNNI repack. | Q2_0_G128 CPU row |

<!-- src: README.md "How we run it", "What is new in this release", "Radeon 780M vs Radeon 890M"; common/arg.cpp (flag help); common/common.h:330,336,363,679,684,686 (defaults); docs/OPTIONS-REGISTRY.md "X1 NVMe expert streaming"; src/llama-context.cpp:833 (moe-cache check); docs/release/RELEASE-STORY-r73i.md (p-min 0.75); docs/backend/MOE-CACHE.md (GLM-5.2 profile); evidence/b11178-x1i2/sg-q27-10062103-shipgate-read.txt (depth 3) -->

## Build switches

The quick-start build (Windows 11, MinGW-w64 GCC, Vulkan SDK):

```powershell
cmake -S . -B build-vulkan -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON `
  -DGGML_ARIFI_ROCMFPX_FORMATS=ON -DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON `
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00" `
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"
```

| Switch | What it does | Why |
|---|---|---|
| `-G Ninja`, `-DCMAKE_BUILD_TYPE=Release` | Ninja generator, optimized build. | Standard release build. |
| `-DGGML_VULKAN=ON` | Builds the Vulkan backend. | The fork's GPU path on AMD. |
| `-DGGML_ARIFI_ROCMFPX_FORMATS=ON` | Compiles the ROCmFP4 / ROCmFPX weight formats (type ids 100-104, 107). Default OFF. | Needed to load ROCmFP4-FAST and the other ROCmFPX files. A build-time option, not an environment variable. |
| `-DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON` | Compiles the TQ3_1S / TQ4_1S weight formats (type ids 45/46). Default OFF. | Needed to load TQ weight files. |
| `-DCMAKE_C_FLAGS` / `-DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00"` | Targets Windows 10 headers. | Required: without it MinGW does not declare `CreateFile2` and the build stops at about 262 of 346 targets. |
| `-DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"` | Links the GCC runtime statically. | Required: otherwise another `libstdc++-6.dll` on `PATH` can stop the binary at start (`0xC0000139`). |
| (no UI flag) | The web UI is embedded from the pinned `tools/ui/dist.tar.gz` (sha256-checked), offline. | Open `http://127.0.0.1:8080`. `-DLLAMA_BUILD_UI` / `-DLLAMA_USE_PREBUILT_UI` only control the npm build and the network download; the pinned copy wins over both. Run with `--no-ui` for an API-only server. The UI touches no decode path. |
| `git clone -c core.longpaths=true` | Allows long paths in the checkout. | One upstream file path is longer than Windows allows by default. |

Optional, not in our serve lines: `-DGGML_ARIFI_KV_MEANCENTER=ON` (default OFF) enables K-cache
mean-centering with `--kv-mean-center`. Full build notes: [BUILDING.md](../BUILDING.md).

<!-- src: README.md "Quick start"; docs/BUILDING.md sections 0, 2, 4; ggml/CMakeLists.txt:144,151; CMakeLists.txt:144-145,158; tools/ui/CMakeLists.txt:7-13 -->
