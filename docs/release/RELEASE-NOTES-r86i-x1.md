# ArifiLabs llama.cpp — R86i + the Radeon 890M placement fix

> **Where the receipts live.** Every figure below is copied from a file in `evidence/`, named in the
> row; `evidence/MANIFEST.md` carries each file's sha256. Resolve the tree with `git rev-parse HEAD`
> and `git describe --tags`. The tag this tip is published under is named at publication;
> `r86i-public-2026-09-29` marks the R86i tree before this fix.

Base: upstream `ggml-org/llama.cpp` **b10825** (`9e0e220594af405a62835dc3a27495729fd8506b`), unchanged.
Engine: R86i (`docs/release/RELEASE-NOTES-r86i.md`, every figure there stands) plus **one** engine
commit. Nothing in R86i's defaults changed.

## What changed — one placement fix, scoped to one device

| Change | Switch | Devices it touches | Receipt |
|---|---|---|---|
| UMA placement: `DEVICE_LOCAL` first, `DEVICE_LOCAL\|HOST_VISIBLE\|HOST_COHERENT` dropped from the chain | `GGML_VK_UMA_PLACEMENT=auto` (default) / `legacy` / `device-local`; registry row `GGML_VK_UMA_PLACEMENT` | AMD device `0x150e` (Radeon 890M) only. Every other device keeps the upstream chain byte for byte | `evidence/x1-first-contact__placement-summary.txt` |

**Why.** On the 890M under Windows, the upstream first choice is billed by the driver to the WDDM
shared segment, which is capped near half of the system-visible RAM (about 11.8 GiB of 23.6 GiB). The
GSQ IQ3_S load placed 11.35 GB there and died at the first upload submit (`vk::Queue::submit: ErrorUnknown`)
with the BIOS reservation empty. Before this commit, on the 890M: GSQ IQ3_S and Q4_K_XL 27B failed to
load; IQ3_XXS 27B (10.44 GB file) loaded for
`llama-bench` but `llama-server -c 4096` died at init (`ErrorOutOfDeviceMemory`); S-X8 was never run
unfixed (its failure is inferred from the mechanism). Those receipts are not shipped in this repository.

## Verification on the 890M — correctness only

| Check | Result | Receipt |
|---|---|---|
| GSQ IQ3_S 27B / Q4_K_XL 27B / S-X8 v4.3 27B load, fix ON | **10.75 / 15.36 / 23.40 GiB in `DEVICE_LOCAL`** (the reservation), 0 failed allocations each | `evidence/x1-first-contact__placement-summary.txt` |
| `GGML_VK_UMA_PLACEMENT=legacy` control (IQ3_XXS 27B) | 9.19 GiB in the shared-billed type: the upstream chain, reproduced | same |
| `test-backend-ops test -o MUL_MAT` | **2305 executed / 2305 OK / 0 FAIL**, 876 not supported; 0 case statuses differ from the R86i 780M receipt | `evidence/x1-first-contact__chain2-summary.txt` |
| `test-backend-ops test -o MUL_MAT_ID` | **1004 / 1004 OK / 0 FAIL**, 10 not supported; the R86i 780M run has the same counts (receipt not shipped) | `evidence/x1-first-contact__chain1-executed-counts.txt` |
| Greedy identity, Qwen3.5-9B Q8_0, 4 prompts, temp 0 / top-k 1 | **4/4** in each of 3 pairs (unfixed vs fix ON, `legacy` vs ON, unfixed vs `legacy`) | `evidence/x1-first-contact__chain2-summary.txt` |

## What we could not show

- **No 890M speed number is published.** Every 890M throughput round so far ran under a harness we
  found disturbing the measurement; those rounds are indications, not results.
- The 780M-tuned defaults reach the 890M by feature probe (RDNA3-class, AMD vendor), not by an 890M
  measurement. Treat them as untuned there.
- Identity is shown on a 9B file because it is the only file we checked that loads on every arm,
  the unfixed engine included.

## Where it was checked

Minisforum AI X1 Pro-470 — Ryzen AI 9 HX 470, **Radeon 890M** (Vulkan device `0x150e`), driver
32.0.31041.1004 (AMD 26.8.1), Windows 11 (build 29671 at the same day's read), Balanced power plan, one pool of system RAM:
48 GB physical (32 + 16), **24 GB reserved in BIOS**, 23.6 GiB system-visible. 2026-09-30.
Every speed figure in the R86i notes stays a **Radeon 780M** figure (Beelink SER7 Pro).
