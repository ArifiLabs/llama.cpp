# Release notes: b11178-x1i2 (2026-10-07)

Branch `b11178-x1i2`, tag `v-b11178-x1i2-2026-10-07`. Built on upstream llama.cpp `b11178`
(`f9af9be219ca647a59106f6201bf0d85fab00224`), a linear patch series with zero merge commits. This branch is a new
release line: the previous published branch sits on `b10825` and is not an ancestor.

## Headline (Radeon 890M, `llama-server`, ABBA A/B against our previous release)

| Model file | Best mode | Decode t/s, short / ~600-token prompt |
|---|---|---|
| Ornith-1.5 35B-A3B Q4_K_M | MTP (built-in head) | 34.07 / 35.01 |
| Qwen3.8-Flash-Next GSQ-RCO IQ3_S | MTP + 40K draft vocabulary | 20.24 / 19.45 |
| Qwen3.8 27B UD-Q4_K_XL | DFlash2, 3 tokens | 12.09 / 11.55 |
| Qwen3.8 27B S-X8 v4.3 | DFlash2, 4 tokens | 8.89 / 8.29 |
| GLM-5.3 Flash GSQ-RCO 3.0-bit, NVMe expert streaming | DFlash2, 2 tokens | 2.96 (chat) |

Full table, method and footnotes: [`README.md`](../../README.md). Receipts: [`evidence/b11178-x1i2/`](../../evidence/b11178-x1i2/).

## New

- MTP drafting for Qwen3.8-Flash-Next with a sidecar head (`--spec-type draft-mtp -md <sidecar>`), and a 40,525-token
  draft vocabulary head.
- Recurrent-state update in place (GDN state bank) on the MTP verify path and in plain decode.
- One fused dispatch per hyper-connection post step.
- Sparse attention at prefill (QSA) and a pooled indexer-key cache for long-context decode.
- Embedding-row prefetch per prompt chunk and embedding-row release from the Windows working set.
- Repeatable greedy decode on Vulkan: masked KV cells are kept out of the coopmat1 flash-attention sum.
- MoE expert mat-vec in slot-major order, on at width 1.
- S-X8 v4.3 prompt reading on the int8 coopmat1 kernel, on from 56 columns.
- Prompt images kept in reusable device snapshots.
- Direct-I/O model loading on Windows (`-lm dio`) and slab reads in llama-quantize on Windows.
- GLM-5.3 Flash with routed experts streamed from NVMe (`GGML_ARIFI_MOE_NVME=1`).
- ROCmFP4-FAST q8_1 mat-vec route on by default (`GGML_ARIFI_ROCMFP4_MMVQ=0` reverts).

## Fixed (found on the previous release's own binary)

- Loading Qwen3.8-Flash-Next no longer drains Windows memory to the alarm line.
- An MTP checkpoint restore at a position not divisible by 4 no longer writes the wrong ring plane.

## Checks at this tip

- `arifi_sync.py provenance --strict`: pass (63 commits exempt and 367 grandfathered, each pinned by sha).
- `arifi_sync.py series check`: pass.
- `arifi_sync.py protected-win validate`: pass, 61 entries.

Every result that showed no gain, and what is still open: [`docs/arifi/STATUS.md`](../arifi/STATUS.md).
