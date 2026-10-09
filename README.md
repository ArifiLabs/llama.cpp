<p align="center"><img src="media/arifilabs-llama-cpp-banner.webp" alt="ArifiLabs llama.cpp" width="820"></p>

# ArifiLabs llama.cpp

**Ornith-1.5 35B at 34 tokens/s, Qwen3.8-Flash-Next at 20 tokens/s and Qwen3.8 27B at 13.6 tokens/s on an AMD Radeon
890M, the integrated graphics of a mini-PC. Vulkan, no discrete GPU.**

A llama.cpp fork built for AMD integrated GPUs. It runs MTP drafting on Qwen3.8-Flash-Next, gives Vulkan kernels to
quant formats that only CUDA or Metal users could run, makes the formats you already use faster, and keeps DFlash2 and
MTP speculative decoding stable and repeatable. Built on upstream llama.cpp `b11178`. Work on the fork started in
July 2026; this is its third published release.

**Version `v0.1.3.1`** (upstream `b11178`) · [Changelog](CHANGELOG.md) · versions are `v0.1.<release>.<patch>`: the
third number counts releases, the fourth counts fix releases on top of one.

## Speed: Radeon 890M (Minisforum AI X1 Pro-470)

| Model file | Mode | Draft depth | Drafter | Key server flags | Decode t/s, short / ~600-token prompt | Prompt t/s (~600 tokens) | Decode gain over plain |
|---|---|---|---|---|---|---|---|
| Qwen3.8-Flash-Next GSQ-RCO IQ3_S (ISTA-DASLab) | plain | - | - | `-ngl 999 -fa on --lazy-mode on -lm dio -c 4096` | 11.84 / 11.72 | 146.3 | - |
| same | MTP (sidecar head) | 3 | `mtp-sidecar-qwen4exp-q8_0.gguf` | plain flags + `--spec-type draft-mtp --spec-draft-p-min 0.5` | 19.44 / 17.62 | 139.3 | +64.1% / +50.3% |
| same | MTP + 40K draft vocabulary | 3 | `mtp-sidecar-qwen4exp-q8_0-dven40k.gguf` | same as the MTP row | **19.93 / 19.27** | 139.6 | **+68.3% / +64.4%** |
| Ornith-1.5 35B-A3B Q4_K_M | plain | - | - | `-ngl 999 -fa on -lm dio --lazy-mode on -c 4096 -b 2048 -ub 512` | 29.18 / 28.78 | **420.1** | - |
| same | MTP (built-in head) | 3 | head in the model file | plain flags + `--spec-type draft-mtp --spec-draft-p-min 0.5` | **33.74 / 34.45** | 413.2 | +15.6% / +19.7% |
| same | DFlash2 | 2 | `Ornith-1.5-35B-A3B-DFlash2-BF16.gguf` | plain flags + `--spec-type draft-dflash --dflash-defer-injection 0` | 31.97 / 33.43 | 392.8 | +9.5% / +16.1% |
| Qwen3.8 27B UD-Q4_K_XL (Huihui abliterated) | plain | - | - | `-ngl 999 -fa on -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0 --ctx-checkpoints 32` | 4.62 / 4.61 | 106.3 | - |
| same | MTP (built-in head) | 3 | head in the model file | plain flags + `--spec-type draft-mtp --spec-draft-p-min 0.5` | 10.16 / 9.90 | 103.3 | +119.9% / +114.7% |
| same | DFlash2 | **6** ¹ | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | plain flags + `--spec-type draft-dflash --dflash-defer-injection 0` | **13.62 / 11.55** | 100.9 | **+194.8% / +150.5%** |
| Qwen3.8 27B S-X8 v4.3, 7.5 bits per weight ([MarlaLabs](https://github.com/MarlaLabsAI/sx8-quantization)) | plain | - | - | `-ngl 999 -fa on -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0` | 2.96 / 2.95 | 92.6 | - |
| same | DFlash2 | **6** ¹ | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | plain flags + `--spec-type draft-dflash --dflash-defer-injection 0` | 9.54 / 8.57 | 88.7 | **+222.2% / +190.5%** |
| GLM-5.3 Flash GSQ-RCO 3.0-bit, experts streamed from NVMe ² | DFlash2 | 2 | `GLM-5.3-Flash-DFlash2-Q8_0.gguf` | `--moe-cache 1536 -fa off -c 2048 -b 8 -ub 8` + `GGML_ARIFI_MOE_NVME=1` | 2.96 (chat) | 4.4–4.6 | - |

Every row's full command line is in [How we run it](#how-we-run-it-radeon-890m). New in this release: **draft depth 6**
for both 27B DFlash2 lines, and a batch of correctness fixes (see [What is new](#what-is-new-in-this-release)). Greedy
output ids are **identical to v0.1.3.0 on every row**.

¹ Depth 6 is new for both 27B DFlash2 lines (3 and 4 before): Q4_K_XL short-prompt decode **12.09 → 13.62 t/s (+12.6%)**,
S-X8 **8.89 → 9.54 t/s (+7.3%)**, output identical. ² GLM-5.3 row: the v0.1.3.0 ship-gate number (8 launches ABBA,
output ids exact on all 8).

How we measure: each row is an A/B on one file against our previous release (v0.1.3.0), `llama-server`, both arms
alternated (ABBA), every round kept, means shown, greedy output ids compared on every row. Percent = (this ÷ plain - 1)
× 100 on the shown means, cut to one decimal, never rounded up. Receipts: [`evidence/v0.1.3.1/`](evidence/v0.1.3.1/).
Machine: Ryzen AI 9 HX 470, Radeon 890M, 96 GB DDR5-5600, 72 GB reserved for the GPU, Windows 11. Decode = 128 greedy
tokens after a ~12-token and a ~600-token prompt.

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
| plain | - | - | - | 11.84 / 11.72 |
| MTP | 3 | `mtp-sidecar-qwen4exp-q8_0.gguf` (MTP sidecar) | `-md mtp-sidecar-qwen4exp-q8_0.gguf -ngld 999 --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | 19.44 / 17.62 |
| MTP + 40K draft vocabulary | 3 | `mtp-sidecar-qwen4exp-q8_0-dven40k.gguf` | `-md mtp-sidecar-qwen4exp-q8_0-dven40k.gguf -ngld 999 --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | **19.93 / 19.27** |

Where the MTP sidecars come from: we built both, because the quantized GSQ-RCO files do not carry the MTP block.
`mtp-sidecar-qwen4exp-q8_0.gguf` is a small side GGUF holding only the MTP block: its head tensors come from
[ashbash's Q8_0 MTP drafter](https://huggingface.co/ashbash/Qwen3.8-Flash-Next-MTP-Drafter-GGUF), and our builder
checks every one of them against the 31 `mtp.*` tensors of Qwen's official BF16
[Qwen/Qwen3.8-Flash-Next](https://huggingface.co/Qwen/Qwen3.8-Flash-Next) checkpoint before it writes anything. The
`-dven40k` variant adds a 40,525-token English draft vocabulary, a design taken from
[Strata](https://github.com/Niko1221/Strata) and re-implemented here. To build them yourself: fetch the BF16 MTP tensors
with Strata's `mtp_fetch.py`, then run our sidecar builder. The builder, Strata's fetch tool (MIT, credited), pinned
inputs, step-by-step instructions and the sha256 of both files are in
[`tools/qwen4exp-mtp-sidecar/`](tools/qwen4exp-mtp-sidecar/README.md).

**Quality and speed of the sidecar choices.** The draft head never changes the answer: the main model checks every
drafted token and keeps only the ones it would have produced itself, so greedy output is token-for-token the same with
or without a draft (checked on every row of our ship gate). What the head changes is speed. We use the **Q8_0** head,
not BF16: each of its tensors is within 1.5% relative error of Qwen's BF16 weights (the builder refuses anything
worse), it is half the bytes the GPU reads on every draft step, and it already reaches 91.5% / 78.2% draft acceptance
(short / long prompt). A BF16 head has not been measured served yet; that A/B is queued. The **40K draft vocabulary**
makes each draft step cheaper (the head scores 40,525 tokens instead of the full vocabulary) and raised decode +4.6% to
+8.7% on all 6 test requests over the full-vocabulary sidecar, same output. A 106K-token subset gave no gain, so we ship
the 40K one.

**Ornith-1.5 35B-A3B MoE · Q4_K_M** (ornith-ai) · file `Ornith-1.5-35B-Q4_K_M.gguf`
Base: `-ngl 999 -np 1 -fa on -c 4096 -ub 512 -b 2048 -lm dio --lazy-mode on`

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | - | 29.18 / 28.78 |
| MTP | 3 | head inside the model file | `--spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | **33.74 / 34.45** |
| DFlash2 | 2 | `Ornith-1.5-35B-A3B-DFlash2-BF16.gguf` (jzinno) | `-md Ornith-1.5-35B-A3B-DFlash2-BF16.gguf -ngld 999 --spec-type draft-dflash --spec-draft-n-max 2 --dflash-defer-injection 0` | 31.97 / 33.43 |

**Qwen3.8 27B dense · UD-Q4_K_XL** (Huihui abliterated, huihui-ai) · file `Huihui-Qwen3.8-27B-abliterated-UD-Q4_K_XL.gguf`
Base: `-dev Vulkan0 -ngl 999 -fa on -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0 --parallel 1 --ctx-checkpoints 32 --ctx-checkpoints-device off --ctx-checkpoints-toolcall on`

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | - | 4.62 / 4.61 |
| MTP | 3 | head inside the model file | `--spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5` | 10.16 / 9.90 |
| DFlash2 | 6 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` (incoai) | `--spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --dflash-defer-injection 0 --spec-draft-n-max 6` | **13.62 / 11.55** |

**Qwen3.8 27B dense · S-X8 v4.3 format, 7.5 bits per weight** (MarlaLabs) · file `Qwen3.8-27B-SX8v43-id57.gguf` (retagged to our type 57 with `tools/gguf-retag-sx8/`)
Base: `-dev Vulkan0 -ngl 999 -fa on -c 8192 -b 1024 -ub 512 -ctk q8_0 -ctv q8_0 --parallel 1`

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s, short / long |
|---|---|---|---|---|
| plain | - | - | - | 2.96 / 2.95 |
| DFlash2 | 6 | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` (incoai) | `--spec-type draft-dflash -md Qwen3.8-27B-DFlash2-Q4_K_M.gguf --dflash-defer-injection 0 --spec-draft-n-max 6` | 9.54 / 8.57 |

**GLM-5.3 Flash MoE · GSQ-RCO 3.0-bit, routed experts streamed from NVMe** (pfeifferj) · file `GLM-5.3-Flash-GSQ-RCO-3.0bit.gguf`
Base: `-dev Vulkan0 -ngl 999 --no-host --no-op-offload --no-warmup -fit off -lm mmap -fa off -c 2048 -b 8 -ub 8 -np 1 --moe-cache 1536` plus the environment in note ⁵

| Mode | Draft depth | Drafter | Add to the base line | Decode t/s |
|---|---|---|---|---|
| DFlash2 | 2 | `GLM-5.3-Flash-DFlash2-Q8_0.gguf` (Anbeeld) | `-md GLM-5.3-Flash-DFlash2-Q8_0.gguf -devd Vulkan0 -ngld 99 --spec-type draft-dflash --spec-draft-n-max 2` | 2.96 (chat) |

⁵ The GLM line also needs these environment variables:
`GGML_ARIFI_MOE_NVME=1 GGML_ARIFI_MOE_NVME_CACHE_MIB=1536 GGML_ARIFI_MOE_NVME_IO_LANES=8 GGML_ARIFI_MOE_NVME_BATCH_READ=1
GGML_ARIFI_MOE_NVME_RESIDENT_MIB=61440 GGML_ARIFI_MOE_NVME_POLICY=heat GGML_ARIFI_MOE_DECODE_MAX_TOKENS=3
GGML_ARIFI_VK_MOE_CACHE=v1 GGML_ARIFI_VK_MOE_MV=coop GGML_ARIFI_VK_SHARED_BUDGET_MIB=1024 GGML_SCHED_PREFETCH_EXPERTS=0`,
and `GGML_ARIFI_MOE_NVME_REPLICAS` names a list of two identical copies of the model file on two drives. The measured
cell also set `GGML_ARIFI_VK_MOE_MV_MULTI=0` and `POWERINFER_NO_BUFFERING=1`; this engine reads neither on this path,
so they are left out. Each server line is copied from the measured cell's own record:
[`evidence/b11178-x1i2/`](evidence/b11178-x1i2/) `sg-*-state.json` (field `args`). The GLM cell's command record is not
in this repository; its result is [`glm-shipgate-VERDICT.txt`](evidence/b11178-x1i2/glm-shipgate-VERDICT.txt).

## Models used (download links)

Every model and drafter file in the speed tables, with where it comes from. "As downloaded" = we run the publisher's
file unchanged.

| Model | File | Download | Notes |
|---|---|---|---|
| Qwen3.8-Flash-Next, GSQ-RCO IQ3_S | `Qwen3.8-Flash-Next-GSQ-RCO-IQ3_S-00001-of-00002.gguf` (+ part 2) | [ISTA-DASLab/Qwen3.8-Flash-Next-GSQ-RCO-GGUF](https://huggingface.co/ISTA-DASLab/Qwen3.8-Flash-Next-GSQ-RCO-GGUF) | as downloaded |
| Qwen3.8-Flash-Next MTP sidecar | `mtp-sidecar-qwen4exp-q8_0.gguf`, `...-dven40k.gguf` | built by us from [ashbash's MTP drafter](https://huggingface.co/ashbash/Qwen3.8-Flash-Next-MTP-Drafter-GGUF), checked against [Qwen/Qwen3.8-Flash-Next](https://huggingface.co/Qwen/Qwen3.8-Flash-Next) | tools: [`tools/qwen4exp-mtp-sidecar/`](tools/qwen4exp-mtp-sidecar/README.md) |
| Ornith-1.5 35B-A3B, Q4_K_M | `Ornith-1.5-35B-Q4_K_M.gguf` | [ornith-ai/Ornith-1.5-35B-A3B-GGUF](https://huggingface.co/ornith-ai/Ornith-1.5-35B-A3B-GGUF) | as downloaded; MTP head inside |
| Ornith-1.5 DFlash2 drafter | `Ornith-1.5-35B-A3B-DFlash2-BF16.gguf` | [jzinno/Ornith-1.5-35B-A3B-DFlash2](https://huggingface.co/jzinno/Ornith-1.5-35B-A3B-DFlash2) | published as safetensors; converted by us to a BF16 GGUF |
| Qwen3.8 27B, UD-Q4_K_XL (Huihui abliterated) | `Huihui-Qwen3.8-27B-abliterated-UD-Q4_K_XL.gguf` | [huihui-ai/Huihui-Qwen3.8-27B-abliterated-GGUF](https://huggingface.co/huihui-ai/Huihui-Qwen3.8-27B-abliterated-GGUF) | as downloaded; MTP head inside |
| Qwen3.8 27B DFlash2 drafter | `Qwen3.8-27B-DFlash2-Q4_K_M.gguf` | [incoai/Qwen3.8-27B-DFlash2-GGUF](https://huggingface.co/incoai/Qwen3.8-27B-DFlash2-GGUF) | as downloaded |
| Qwen3.8 27B, S-X8 v4.3 | `Qwen3.8-27B-SX8v43.gguf` | [marlalabsAI/Qwen3.8-27B-SX8](https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8) | retag the type id once: `python tools/gguf-retag-sx8/retag_sx8.py` (writes the `-id57` file we run) |
| GLM-5.3 Flash, GSQ-RCO 3.0-bit | `GLM-5.3-Flash-GSQ-RCO-3.0bit.gguf` | [pfeifferj/GLM-5.3-Flash-GSQ-RCO-GGUF](https://huggingface.co/pfeifferj/GLM-5.3-Flash-GSQ-RCO-GGUF) | as downloaded |
| GLM-5.3 Flash DFlash2 drafter | `GLM-5.3-Flash-DFlash2-Q8_0.gguf` | [Anbeeld/GLM-5.3-Flash-DFlash2-GGUF](https://huggingface.co/Anbeeld/GLM-5.3-Flash-DFlash2-GGUF) | as downloaded |
| Qwen3.8 27B, ROCmFP4-FAST (780M table) | `Qwen3.8-27B-ROCmFP4-FAST.gguf` | [julianmb/Qwen-3.8-27B-ROCmFP4-FAST-GGUF](https://huggingface.co/julianmb/Qwen-3.8-27B-ROCmFP4-FAST-GGUF) | as downloaded |
| Qwen3.8 27B, UD-Q3_K_XL Unleashed (780M table) | `Qwen3.8-27B-Unleashed-UD-Q3_K_XL.gguf` | [outsourc-e/Qwen3.8-27B-Unleashed-GGUF](https://huggingface.co/outsourc-e/Qwen3.8-27B-Unleashed-GGUF) | as downloaded |
| Qwen3.8 27B, Escha-W2 (format page) | `Qwen3.8-27B-Escha-W2.escha-native.gguf` | [EschaLabs/Qwen3.8-27B-Escha-W2](https://huggingface.co/EschaLabs/Qwen3.8-27B-Escha-W2) | published for SGLang; converted by us to a native GGUF (types 55/56) |

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
These four files have not yet been measured on the Radeon 890M; that A/B is queued.

## What is new in this release

**v0.1.3.1** (receipts: [`evidence/v0.1.3.1/`](evidence/v0.1.3.1/), full list: [Changelog](CHANGELOG.md)):

| Change | Effect (Radeon 890M) |
|---|---|
| Draft depth 6 for both Qwen3.8 27B DFlash2 lines | Q4_K_XL short-prompt decode 12.09 → **13.62 t/s (+12.6%)**, S-X8 8.89 → **9.54 t/s (+7.3%)**, output identical |
| Sparse-prefill mask fused into flash attention (Qwen3.8-Flash-Next) | the mask chain runs in one kernel; prompt t/s up to +0.9% at 8K-32K tokens, same output |
| IQ3_S experts on the NVMe expert cache | output now matches the CPU exactly; a per-type test checks every cache weight type against the CPU |
| DeepSeek-3.2 and dots3note | load and run (the indexer rotation keeps the full-width tile) |
| Vulkan mat-mul and mat-vec on views and padded batches | correct rows for KV-cache views and odd-stride F16/BF16 batches |
| Speculative decoding | stops at a confirmed end-of-generation token; keeps batch order with several slots |
| Saved sessions | record the KV rotation, so a restore into a different setup is refused instead of computing wrong |
| GGUF loader | rejects crafted files that could hang or overflow it |
| Web UI | built in and on by default (pinned copy, works offline) |
| `--version` | names the release |

**In v0.1.3.0, still in this release.** Each effect below is a separate A/B on the Radeon 890M with
Qwen3.8-Flash-Next GSQ-RCO IQ3_S unless named, on the build where the change landed; receipts are in
[`evidence/b11178-x1i2/`](evidence/b11178-x1i2/).

| Change | Effect (Radeon 890M) | Default |
|---|---|---|
| MTP drafting for Qwen3.8-Flash-Next (`--spec-type draft-mtp -md <MTP sidecar>`) | 10.73 → 16.64 t/s short prompt (+55.0%), 10.38 → 15.14 long (+45.8%), draft acceptance 91.5% / 78.2%, means of 2 rounds | opt-in: needs the MTP sidecar GGUF, built by us from Qwen's checkpoint ([where it comes from](#how-we-run-it-radeon-890m)) |
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

## Quick start (Windows 11, AMD GPU, Vulkan)

Four steps: install the tools, build, download one model, run. Run every command in **PowerShell 7**.

**1. Install the tools (once).** You also need a current AMD graphics driver (AMD Software: Adrenalin Edition).

```powershell
winget install -e --id Git.Git
winget install -e --id Kitware.CMake
winget install -e --id Ninja-build.Ninja
winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
winget install -e --id KhronosGroup.VulkanSDK
winget install -e --id Python.Python.3.12
```

Open a **new** PowerShell window afterwards so the new tools are on `PATH`, then check: `gcc --version`,
`cmake --version`, `glslc --version`. We build with WinLibs GCC 14.2; winget currently installs a newer GCC, which we
have not tested.

**2. Get the code and build the server.**

```powershell
git clone -c core.longpaths=true https://github.com/ArifiLabs/llama.cpp arifilabs-llama.cpp
cd arifilabs-llama.cpp
cmake -S . -B build-vulkan -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON `
  -DGGML_ARIFI_ROCMFPX_FORMATS=ON -DGGML_ARIFI_TURBO_WEIGHT_QUANTS=ON `
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0A00" -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0A00" `
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"
cmake --build build-vulkan --target llama-server
build-vulkan\bin\llama-server.exe --version
```

The three Windows flags are required, not tuning: without them the build fails to link or the server dies at start
([why](docs/BUILDING.md)). `-c core.longpaths=true` is needed because one upstream file path is longer than Windows
allows by default.

**3. Download a model.** Our simplest fast line: Ornith-1.5 35B (one file, about 22 GB, with its own MTP draft head inside).

```powershell
pip install -U "huggingface_hub[cli]"
hf download ornith-ai/Ornith-1.5-35B-A3B-GGUF Ornith-1.5-35B-Q4_K_M.gguf --local-dir models
```

**4. Run it.** Open the web UI at `http://127.0.0.1:8080` in a browser, or use the OpenAI-compatible API at
`http://127.0.0.1:8080/v1`. The web UI is built in: the repository carries a pinned copy (`tools/ui/dist.tar.gz`,
sha256-checked), so the build needs no network and no Node.js for it.

```powershell
build-vulkan\bin\llama-server.exe -m models\Ornith-1.5-35B-Q4_K_M.gguf -ngl 999 -np 1 -fa on -c 4096 `
  -ub 512 -b 2048 -lm dio --lazy-mode on --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5
```

At startup the Vulkan backend prints one line for each tuned default it applied on your GPU. On our Radeon 890M this
line decodes at about 34–35 tokens per second. Every other model and mode, with its exact line and its drafter, is in
[How we run it](#how-we-run-it-radeon-890m).

## Test system (Radeon 890M numbers)

Every 890M number on this page was measured on this machine and software stack, read live on 2026-10-08.

| Part | What we run |
|---|---|
| Machine | Minisforum AI X1 Pro-470 (board GPBAC 1.0), BIOS 1.01 (2026-04-08, the latest Minisforum lists), performance mode |
| CPU / GPU | AMD Ryzen AI 9 HX 470, 12 cores / 24 threads; Radeon 890M (RDNA 3.5, gfx1150), integrated |
| Memory | 96 GB DDR5-5600 (2 x 48 GB, dual channel); 72 GB reserved for the GPU in the BIOS, so Windows sees about 23.6 GB |
| Storage | three NVMe drives on Windows' native NVMe driver (`nvmedisk.sys`, not the SCSI path): models on a Fanxiang S880 1 TB and a KingSpec XG7000 2 TB (PCIe Gen4 x4), Windows on a Crucial P3 Plus 1 TB. How: [native NVMe guide](docs/arifi/windows-native-nvme/README.md) |
| Operating system | Windows 11 Pro Insider Preview, Dev channel, build 29683.1000 (updated from 29680 on 2026-10-08; we diff 355 settings before and after every Windows update) |
| GPU driver | AMD Software (Adrenalin) 26.8.1, driver 32.0.31041.1004; Vulkan 1.4.349, AMD driver 2.0.395 (LLPC compiler) |
| AMD chipset | AMD Chipset Software 8.08.12.551 |
| Build tools | MinGW-w64 GCC (build of record, [`docs/BUILDING.md`](docs/BUILDING.md)), Vulkan SDK 1.4.357.0. ROCm / HIP SDK is not installed: this fork runs on Vulkan |
| Windows settings | power plan Balanced (fastest for the integrated GPU in our A/B on the previous machine, a Radeon 780M; not yet re-tested on this one); hypervisor and VBS off; hardware-accelerated GPU scheduling on; Windows Search indexer, SysMain, OneDrive and 12 background scheduled tasks off (the 29683 update turned Search and SysMain back on; we turned them off again); NVMe deep sleep off on mains power (Primary and Secondary NVMe Idle Timeout = 0: bursty 1 MB reads 3.0 ms -> about 1.0 ms average latency, [how](docs/arifi/windows-native-nvme/README.md)); disk sleep never; PCIe link power saving off; Defender exclusions on the model, build and run folders; Windows on-device AI off by policy. Full list with reasons and undo: [`docs/arifi/WINDOWS-SETTINGS.md`](docs/arifi/WINDOWS-SETTINGS.md) |

**All numbers here are from Windows.** We have not measured this fork on Linux; the drivers, scheduler and memory
handling there differ, so Linux numbers on the same machine may differ in either direction.

## Test system (Radeon 780M numbers)

Every 780M number on this page was measured on our previous machine, used until 30 September 2026. Read from our
equipment record and the rig stamp each A/B receipt carries.

| Part | What we ran |
|---|---|
| Machine | Beelink SER7 Pro, BIOS `SER7PRO_P5C8V38` (AMI, 2024-01-09): cTDP 54 W performance mode, fans max, SVM / AMD-V off, Wi-Fi off |
| CPU / GPU | AMD Ryzen 7 7840HS, 8 cores / 16 threads; Radeon 780M (RDNA3, gfx1103, 12 CU), integrated |
| Memory | DDR5-5600 with a 16 GB BIOS reservation for the GPU. Until 3 September 2026: 2 x 16 GB matched dual channel (32 GB). From 3 September: 32 GB + 16 GB (48 GB, asymmetric). Each receipt names its memory epoch |
| GPU driver | AMD Software (Adrenalin) driver 32.0.31041.1004 in the September runs (the same driver the 890M runs now) |
| Operating system | Windows 11 Pro Insider, build 29648 in the September runs |
| Power plan | Balanced. Measured on this machine: Balanced 29.12 > Ultimate 28.90 > High Performance 28.39 t/s; a "max performance" plan (minimum processor state 100% together with the AMD power slider at max) starved the integrated GPU to 11.07 t/s, because the CPU then takes the shared package power. The benchmark harness refuses to run off Balanced |
| Windows settings | Hyper-V, Virtual Machine Platform and Windows Hypervisor Platform removed; VBS / memory integrity and Credential Guard off (LSA protection kept on); SysMain off; Fast Startup and hibernation off; hardware-accelerated GPU scheduling on; Defender exclusions on the model, build and run folders |
| AMD software | Memory Optimizer "Productivity"; metrics tracking and overlay off |

On the 780M the hypervisor removal mattered most for long prompts (+22% on long context in our measurement). If you
reproduce our 780M numbers, match the BIOS reservation (16 GB), the power plan (Balanced) and the hypervisor state first.

## Radeon 780M vs Radeon 890M: what changes

If you run a Radeon 780M (Ryzen 7040 / 8040 series), you get the same tuned kernels as the 890M.

- **Same settings at every verify width.** Speculative decoding checks several drafted tokens at once; the "verify
  width" n is the number of tokens checked per step (draft depth + 1). The Vulkan backend's device probe reads the
  GPU's subgroup and wave properties, not its name, and classes both the 780M (gfx1103, RDNA3) and the 890M (gfx1150,
  RDNA 3.5) as RDNA3. Both therefore get the same mat-vec rows per workgroup, the same q8_1 route and the same width
  splits for n = 1 to 8. The startup log prints `device-probe (RDNA3)` for each one.
- **One difference:** flash-attention work splitting scales with the compute-unit count (12 on the 780M, 16 on the
  890M). Placement into the GPU reservation (`GGML_VK_UMA_PLACEMENT`) is enabled by device id for the 890M only.
- **Where each default was tuned:** the width defaults (IQ3_S / IQ3_XXS rows at n = 6 and 7, the Q4_K width-5 split,
  the Q5_K / Q6_K / ROCmFP4-FAST q8_1 routes) were tuned and measured on the 780M, so they are native to it. The int8
  coopmat1 prompt path and the slot-major MoE mat-vec were measured on the 890M only. A re-tune of the width defaults
  on the 890M is in progress.
- **Draft depth (`--spec-draft-n-max`) is where the two GPUs differ in practice.** On the 780M, Qwen3.8 27B with
  DFlash2 peaked at **depth 4**: depth 5 and 6 were slower per served step (September 2026 integration A/Bs on the
  780M kept `--spec-draft-n-max 4` for the 27B DFlash2 lines). On the 890M the same 27B goes further: **depth 6** is what
  v0.1.3.1 ships: Q4_K_XL short-prompt decode 12.09 -> 13.62 t/s (+12.6%) and S-X8 8.89 -> 9.54 t/s (+7.3%), output
  identical. On a 780M, start at depth 4 for the 27B; on an 890M, depth 6. The kernel table above used depth 2 on the 780M
  only to compare builds, not as the best setting.
- **Memory:** the 780M machine we measured had 16 GB reserved for the GPU in the BIOS; a 27B Q4_K_XL with its DFlash2
  drafter fits there. The Flash-Next, Ornith-1.5 35B and GLM-5.3 files are larger than 16 GB; those lines were
  measured with the 890M machine's 72 GB reservation. A 780M laptop or mini-PC can usually raise its reservation in
  the BIOS (look for "UMA frame buffer size").

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
| q5_K on the q8_1 MMVQ path at widths 5-8 | +6.7% to +20.4% per op at widths 5-8 (FFN shape 17408x5120) | on (AMD), `GGML_ARIFI_Q5K_MMVQ=legacy` reverts |
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
Release `b11178-v0.1.3.1` carries 886 fork commits on top of upstream `b11178`.

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

| Version | Release | Date | Upstream base | What it added | Headline served number | Receipts |
|---|---|---|---|---|---|---|
| `v0.1.1.0` | `b10825-r73i` | 2026-09-22 | `b10825` | First public release. A linear patch series that `series check` replays and verifies. Seven tracked sources: PowerInfer SSD expert streaming, PrismML Q2_0_G128 with its VNNI repack, the ROCmFPX formats, TQ3_4S, TurboQuant KV with TQ3_1S / TQ4_1S and their Vulkan kernels, plus three host-transfer prefetch patches | none: a capability release | that branch's `README.md` |
| `v0.1.2.0` | `b10825-r86i-x1` | 2026-10-01 | `b10825` | Vulkan kernels for every fork format (`test-backend-ops`, 0 failures), mat-vec kernels for verify widths 2-8, S-X8 kernels, the ROCmFP4-FAST q8_1 route (opt-in), stable DFlash2 and MTP on Qwen3.8, adaptive draft length, the Vulkan MoE expert cache, placement into the 890M's GPU reservation | Radeon 780M: Qwen3.8 27B ROCmFP4-FAST, DFlash2, **9.518 t/s** (from 7.498, +26.9%) | the 780M table above |
| - | `x1i` (not published) | 2026-10-03 | `b11178` | The base move to `b11178` with upstream's int8 coopmat1 path, S-X8 prompt reading on int8 coopmat1, prompt images in device snapshots, the ROCmFP4-FAST route on by default | Radeon 890M: Ornith-1.5 35B, DFlash2, **29.51 / 31.23 t/s**; Qwen3.8 27B Q4_K_XL, DFlash2, 10.46 / 10.61 t/s | the comparator arms (`x`) in [`evidence/b11178-x1i2/`](evidence/b11178-x1i2/) `sg-orn-*`, `sg-q27-*` |
| `v0.1.3.0` | `b11178-x1i2` | 2026-10-07 | `b11178` | MTP for Qwen3.8-Flash-Next with a 40K draft vocabulary, GDN state bank, fused hyper-connection step, sparse prefill attention and the pooled indexer cache, embedding-row prefetch and release, repeatable greedy decode, slot-major MoE mat-vec, GLM-5.3 Flash NVMe streaming, Windows direct-I/O load, quantize slab reads | Radeon 890M: Ornith-1.5 35B, MTP, **34.07 / 35.01 t/s**; Qwen3.8-Flash-Next, MTP + 40K vocabulary, 20.24 / 19.45; Qwen3.8 27B Q4_K_XL, DFlash2, 12.09 / 11.55 | [`evidence/b11178-x1i2/`](evidence/b11178-x1i2/) |
| `v0.1.3.1` | `b11178-v0.1.3.1` | 2026-10-09 | `b11178` | Fix release: draft depth 6 for both 27B DFlash2 lines, sparse-prefill mask fused into flash attention, IQ3_S NVMe expert-cache fix with a per-type test, DeepSeek-3.2 and dots3note load, Vulkan view and padded-batch mat-mul fixes, speculative-decoding and saved-state fixes, GGUF loader hardening, web UI on by default | Radeon 890M: Qwen3.8 27B Q4_K_XL DFlash2 **13.62 / 11.55 t/s**, S-X8 DFlash2 9.54 / 8.57, Ornith-1.5 MTP 33.74 / 34.45, Flash-Next MTP 40K 19.93 / 19.27; output ids identical to v0.1.3.0 | [`evidence/v0.1.3.1/`](evidence/v0.1.3.1/) `pg-*` |

On the 890M, from `x1i` to this release: Qwen3.8 27B Q4_K_XL with DFlash2 rose from 10.46 to 13.62 t/s on the short
prompt (+30.2%), and Ornith-1.5 35B rose from 29.51 / 31.23 t/s (DFlash2) to 33.74 / 34.45 t/s (MTP). Short / long = a ~12-token and
a ~600-token prompt, means of 4 rounds (8 for MTP).

## For format authors and researchers

You were sent this link because your format runs here with Vulkan kernels. Find your format below.

**S-X8 v4.3 (MarlaLabs).** S-X8 v4.3 is a weight format at 7.50 bits per weight (30 bytes per block), designed for
near-FP16 quality with a simple decoder (about 9-10 ALU operations per weight, no shared memory). Its author reports
wikitext-2 perplexity +0.17% over FP16 on Qwen3.5-4B, against +2.40% for Q8_0, at 11.6% fewer bytes than Q8_0. Format,
paper and CUDA kernels: [MarlaLabsAI/sx8-quantization](https://github.com/MarlaLabsAI/sx8-quantization) (Apache-2.0);
paper [doi:10.5281/zenodo.21922640](https://doi.org/10.5281/zenodo.21922640); the 27B file measured here:
[marlalabsAI/Qwen3.8-27B-SX8](https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8). This fork adds the Vulkan path.
Vulkan: `dequant_sx8.comp`, `mul_mat_vec_sx8.comp`, a q8_1 MMVQ path, a
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

**Escha-W2 (EschaLabs).** Escha-W2 is a 2-bit codebook weight format with mixed precision: on their 35B release the
gate and up projections are 2-bit, down projections 3-bit, attention projections, embeddings and LM head Int8 (12.3 GB
from a 71.9 GB BF16 checkpoint). EschaLabs publish it for a custom SGLang runtime on CUDA, Apache-2.0:
[eschalabs.com](https://www.eschalabs.com/), [Hugging Face](https://huggingface.co/EschaLabs), the 27B model
[EschaLabs/Qwen3.8-27B-Escha-W2](https://huggingface.co/EschaLabs/Qwen3.8-27B-Escha-W2). We found no public GitHub
repository for it. This fork runs the format in llama.cpp with Vulkan:
native types 55 and 56 with a fused `GGML_OP_ESCHA_MM`
(`escha_mm.comp`): Hadamard rotation, code decode and matmul in one dispatch. Decode is coherent on
2- and 3-bit files; column blocking makes the escha3 prefill shape 3.50x faster. The native GGUF
carries an `.escha_aux` tensor beside every code tensor (no separate file); `test-backend-ops -o ESCHA_MM` checks the
kernel against the CPU.

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
- [FlashML-org/FreeToken](https://github.com/FlashML-org/FreeToken): the design of the device-resident context checkpoint ring with pooled
  recurrent-state slots, the tool-call checkpoint anchor and unbuffered model reads, re-implemented here.
- [PrismML-Eng/llama.cpp](https://github.com/PrismML-Eng/llama.cpp): the Q2_0_G128 ternary format, the VNNI
  repack design and the f16 recurrent state.
- [MarlaLabsAI/sx8-quantization](https://github.com/MarlaLabsAI/sx8-quantization) (Martí Vidal Leandro): the S-X8
  v4.3 format, 7.50 bits per weight (Apache-2.0; paper [doi:10.5281/zenodo.21922640](https://doi.org/10.5281/zenodo.21922640)).
  The Vulkan kernels here are ours.
- [jtrefon/llama.cpp-turboq-mtp](https://github.com/jtrefon/llama.cpp-turboq-mtp): the TBQ3_0 and TBQ4_0
  KV cache types.
- [thecodacus/llama.cpp](https://github.com/thecodacus/llama.cpp): three host-transfer expert-prefetch
  patches.
- [EschaLabs](https://www.eschalabs.com/) ([Hugging Face](https://huggingface.co/EschaLabs)): the Escha-W2 code
  format (Apache-2.0).
- [Niko1221/Strata](https://github.com/Niko1221/Strata) (Niko1221 and the Strata contributors, MIT): the design of the
  sparse prefill attention, the pooled indexer-key cache and the 40,525-token draft vocabulary, re-implemented here;
  its `mtp_fetch.py` tool (shipped unchanged in `tools/qwen4exp-mtp-sidecar/`) fetches the Qwen3.8-Flash-Next BF16 MTP
  tensors our sidecar builder checks against.

### Models measured on this page

With thanks to every publisher. Each model link opens the exact model card we downloaded from.

| Model | Role here | Publisher | Model card | License (from the card) |
|---|---|---|---|---|
| Qwen3.8-Flash-Next GSQ-RCO (IQ3_S) | main model | [ISTA-DASLab](https://huggingface.co/ISTA-DASLab) | [Qwen3.8-Flash-Next-GSQ-RCO-GGUF](https://huggingface.co/ISTA-DASLab/Qwen3.8-Flash-Next-GSQ-RCO-GGUF) | Apache-2.0 |
| Qwen3.8-Flash-Next MTP drafter (Q8_0) | head tensors of our MTP sidecar | [ashbash](https://huggingface.co/ashbash) | [Qwen3.8-Flash-Next-MTP-Drafter-GGUF](https://huggingface.co/ashbash/Qwen3.8-Flash-Next-MTP-Drafter-GGUF) | Apache-2.0 |
| Qwen3.8-Flash-Next (BF16) | reference our sidecar head is checked against | [Qwen](https://huggingface.co/Qwen) | [Qwen3.8-Flash-Next](https://huggingface.co/Qwen/Qwen3.8-Flash-Next) | Qwen license (see card) |
| Ornith-1.5 35B-A3B | main model | [ornith-ai](https://huggingface.co/ornith-ai) | [Ornith-1.5-35B-A3B-GGUF](https://huggingface.co/ornith-ai/Ornith-1.5-35B-A3B-GGUF) | MIT |
| Ornith-1.5 DFlash2 drafter | drafter | [jzinno](https://huggingface.co/jzinno) | [Ornith-1.5-35B-A3B-DFlash2](https://huggingface.co/jzinno/Ornith-1.5-35B-A3B-DFlash2) | Apache-2.0 |
| Qwen3.8 27B abliterated (UD Q4_K_XL) | main model | [huihui-ai](https://huggingface.co/huihui-ai) | [Huihui-Qwen3.8-27B-abliterated-GGUF](https://huggingface.co/huihui-ai/Huihui-Qwen3.8-27B-abliterated-GGUF) | Apache-2.0 |
| Qwen3.8 27B DFlash2 drafter | drafter for the 27B lines | [incoai](https://huggingface.co/incoai) | [Qwen3.8-27B-DFlash2-GGUF](https://huggingface.co/incoai/Qwen3.8-27B-DFlash2-GGUF) | Apache-2.0 |
| Qwen3.8 27B S-X8 | main model | [marlalabsAI](https://huggingface.co/marlalabsAI) | [Qwen3.8-27B-SX8](https://huggingface.co/marlalabsAI/Qwen3.8-27B-SX8) | Apache-2.0 |
| Qwen3.8 27B ROCmFP4-FAST | main model | [julianmb](https://huggingface.co/julianmb) | [Qwen-3.8-27B-ROCmFP4-FAST-GGUF](https://huggingface.co/julianmb/Qwen-3.8-27B-ROCmFP4-FAST-GGUF) | Apache-2.0 |
| Qwen3.8 27B Unleashed (UD-Q3_K_XL) | main model | [outsourc-e](https://huggingface.co/outsourc-e) | [Qwen3.8-27B-Unleashed-GGUF](https://huggingface.co/outsourc-e/Qwen3.8-27B-Unleashed-GGUF) | Apache-2.0 |
| GLM-5.3 Flash GSQ-RCO 3.0-bit | main model (experts streamed from NVMe) | [pfeifferj](https://huggingface.co/pfeifferj) | [GLM-5.3-Flash-GSQ-RCO-GGUF](https://huggingface.co/pfeifferj/GLM-5.3-Flash-GSQ-RCO-GGUF) | MIT |
| GLM-5.3 Flash DFlash2 drafter | drafter | [Anbeeld](https://huggingface.co/Anbeeld) | [GLM-5.3-Flash-DFlash2-GGUF](https://huggingface.co/Anbeeld/GLM-5.3-Flash-DFlash2-GGUF) | not stated on the card |

## Requirements and limits

### Dependencies

| What | Needed for | Version we use | Notes |
|---|---|---|---|
| Windows 11 | everything | Insider Dev channel 29683 | all our numbers are from Windows; Linux is not measured |
| AMD graphics driver with Vulkan | running | AMD Software 26.8.1 (driver 32.0.31041.1004), Vulkan 1.4.349 | the fork targets AMD integrated GPUs (RDNA3 / RDNA3.5) |
| MinGW-w64 GCC (WinLibs, UCRT, POSIX threads) | building | GCC 14.2.0 | MSVC and clang-cl are not tested here |
| CMake + Ninja | building | CMake 4.3.3, Ninja 1.13.2 | |
| Vulkan SDK | building the Vulkan backend (shader compiler `glslc`) | 1.4.357.0 | not needed for a CPU-only build |
| Git | getting the code | 2.55 | clone with `-c core.longpaths=true` |
| Python 3 + `numpy` | the tools in `tools/` (MTP sidecar builder, S-X8 retag) | 3.12 | the tools use this repo's own `gguf-py`; per-tool requirements files are in [`requirements/`](requirements/) |
| `huggingface_hub` (the `hf` command) | downloading models | current | `pip install -U "huggingface_hub[cli]"` |

Install commands are in the [Quick start](#quick-start-windows-11-amd-gpu-vulkan). Every runtime switch and environment
variable, with its default: [`docs/arifi/ENVIRONMENT.md`](docs/arifi/ENVIRONMENT.md). Every model's command line on
both test machines, with the reason for each switch: [`docs/arifi/HOW-WE-RUN.md`](docs/arifi/HOW-WE-RUN.md). The
Windows settings we changed, why, and how to undo them: [`docs/arifi/WINDOWS-SETTINGS.md`](docs/arifi/WINDOWS-SETTINGS.md).

### Limits

- Tested on Windows 11 with Vulkan; MinGW-w64 GCC is the build of record ([`docs/BUILDING.md`](docs/BUILDING.md)).
- Tuned for AMD integrated GPUs; on other GPUs the probe-gated defaults stay off (see Hardware).
- `GGML_ARIFI_VNNI_REPACK` is opt-in.
- What did not work, and every protected win with its state: [`docs/arifi/STATUS.md`](docs/arifi/STATUS.md).

## License

[`LICENSE`](LICENSE) is upstream llama.cpp's MIT licence, verbatim. [`NOTICE`](NOTICE) names every third-party
component with its licence; the licence texts are in [`licenses/`](licenses/). S-X8 and turboquant_plus are
Apache-2.0, and their notices travel with this repository.
