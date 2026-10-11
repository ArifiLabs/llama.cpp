# qwen4exp MTP sidecar (Qwen3.8-Flash-Next)

The published Qwen3.8-Flash-Next GGUFs carry no MTP head. This folder builds a small side GGUF that holds only the MTP
block, so the engine drafts with it: `llama-server -m <main gguf> -md <sidecar> --spec-type draft-mtp`.

## Inputs (all pinned)

| input | where | pin |
|---|---|---|
| main model | Hugging Face `ISTA-DASLab/Qwen3.8-Flash-Next-GSQ-RCO-GGUF`, `IQ3_S/...-00001-of-00002.gguf` | hparams, tokenizer, `token_embd`, `output` are copied from shard 00001 |
| MTP block (Q8_0) | Hugging Face `ashbash/Qwen3.8-Flash-Next-MTP-Drafter-GGUF`, `Qwen3.8-Flash-Next-mtp-drafter-Q8_0.gguf` | revision `c82053e8db1eff3f5fcc219c3146fc2ed34258d6`, file sha256 `f2d0834148846c8e5d77ddb4898470936cf35153fac6519095d7e6836a80865a` |
| BF16 reference | `Qwen/Qwen3.8-Flash-Next` (131 safetensors shards), only the 31 `mtp.*` tensors, by HTTP range reads | revision `de4b8e4d43b917e7706784d8bb445c9af86a3540` (`PINNED_REVISION` in `mtp_fetch.py`), sha256 per tensor in `mtp_fetch.py` |
| 40K draft vocabulary (optional) | Strata v0.1.40.1 `data/draft_vocab_en.bin` (40,525 token ids) | sha256 `369151522226a5edaa5f12cfd1e2ae7db8f4fbdbd222f3dcf327dced9597fb25` |

The sidecar takes its tensors from the ashbash Q8_0 file. The BF16 fetch is the check: `build_sidecar.py` dequantizes
every head tensor and compares it with the BF16 truth before it writes anything (rel. error <= 1.5e-2 for Q8_0, norms
checked as 1+w). A head that does not match is refused.

## Why Q8_0, and what each file buys

| choice | quality | speed (Radeon 890M, IQ3_S main model, draft depth 3) |
|---|---|---|
| no draft (plain) | reference | 12.01 / 11.89 t/s (short / long prompt) |
| this sidecar, Q8_0 head | same greedy output: the main model verifies every drafted token | 19.68 / 17.70 t/s; draft acceptance 91.5% / 78.2% |
| `-dven40k` (40,525-token draft vocabulary) | same greedy output | 20.24 / 19.45 t/s; +4.6% to +8.7% over the full-vocabulary sidecar on all 6 test requests |
| 106K-token draft vocabulary | same greedy output | no gain (-8.9% to +3.5%); not shipped |
| BF16 head | same greedy output | not measured served yet (A/B queued); it reads twice the bytes per draft step |

The head only decides how many drafted tokens are accepted, so its precision moves speed, never the answer. Q8_0 is
within 1.5e-2 relative error of the BF16 weights on every tensor (checked by `build_sidecar.py`, which refuses a worse
head) and is half the size the GPU reads on each draft step.

## Steps

```
# 1. BF16 reference, ~5.2 GB of range reads (Strata's tool, unchanged)
python tools/qwen4exp-mtp-sidecar/mtp_fetch.py fetch  --out <bf16-dir>
python tools/qwen4exp-mtp-sidecar/mtp_fetch.py verify --out <bf16-dir>

# 2. the sidecar (ours): verify the Q8_0 head against BF16, then write
python tools/qwen4exp-mtp-sidecar/build_sidecar.py --main <IQ3_S-00001-of-00002.gguf> \
    --head <Qwen3.8-Flash-Next-mtp-drafter-Q8_0.gguf> --bf16 <bf16-dir> \
    --out mtp-sidecar-qwen4exp-q8_0.gguf

# 3. optional: draft logits over a 40K token subset (ours, Strata's draft-vocabulary design)
python tools/qwen4exp-mtp-sidecar/make_dvocab_sidecar.py mtp-sidecar-qwen4exp-q8_0.gguf \
    <strata>/data/draft_vocab_en.bin mtp-sidecar-qwen4exp-q8_0-dven40k.gguf
```

Both scripts need `numpy` and this repo's `gguf-py` (found relative to the script). Test of the fetch tool:
`python tools/qwen4exp-mtp-sidecar/test_mtp_fetch.py` (mocked, downloads nothing).

## Outputs

| file | size (bytes) | sha256 |
|---|---|---|
| `mtp-sidecar-qwen4exp-q8_0.gguf` | 3642719808 | `0bf164da24ec12f90c0d84c513b3c3c2c77bccf5f448b3a5d7bfa2e5c8940920` |
| `mtp-sidecar-qwen4exp-q8_0-dven40k.gguf` | 3206674592 | `b0203c9dfc4f4f7c8eeeef2224200c4d19860cdfb67b29e67d39beaa84d497ce` |

These scripts, run from this tree on 2026-10-07, regenerated both files with tensors identical to the files we serve.

## Serve

```
llama-server -m <IQ3_S-00001-of-00002.gguf> -md mtp-sidecar-qwen4exp-q8_0-dven40k.gguf -ngl 999 -ngld 999 \
    -fa on --lazy-mode on -lm dio --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-p-min 0.5
```

## Credits

`mtp_fetch.py` and `test_mtp_fetch.py` are from Strata (https://github.com/Niko1221/Strata, tag v0.1.40.1,
commit `82f46a8c8f475f001ad76d92f58f4a4f8ffb0253`), MIT, copied unchanged; license in `licenses/strata-MIT.txt`.
The draft-vocabulary subset design is also Strata's. The MTP head GGUF is ashbash's (Apache-2.0, from Qwen's
official BF16 release).
