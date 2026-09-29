# ArifiLabs llama.cpp — R86i (`r86i-public-2026-09-29`)

> **Where the receipts live.** Every figure below is copied from a file in `evidence/`, named in the
> row. `evidence/MANIFEST.md` carries each file's sha256. Short SHAs and engine labels in this
> document (`6fb7a425a`, `5a7434218`, `037b433a9`) are studio-internal and **do not resolve in this
> repository**; resolve the tree with `git rev-parse HEAD` and `git describe --tags`.

Base: upstream `ggml-org/llama.cpp` **b10825** (`9e0e220594af405a62835dc3a27495729fd8506b`), upstream's
own commit, byte for byte. Engine label `arifi-b10825-r86i-6fb7a425a` (supersedes
`arifi-b10825-r73i-5a7434218`). Count the fork commits and the series yourself:
`git rev-list --count 9e0e22059..HEAD`, `python tools/arifi-sync/arifi_sync.py series check`.

## What changed — three RDNA3 mat-vec defaults ON, one route opt-in

A speculative decoder verifies at **width = 1 + draft depth**. All three new defaults act at widths
5 and 6, which a draft depth of 4 or 5 reaches.

| # | Change (lane) | Switch to revert | Before → after | Rounds | Receipt |
|---|---|---|---|---|---|
| 1 | q4_K width-5/6 mat-vec split 4+1 (R85), lm_head 248320×5120 | `GGML_ARIFI_Q4K_W5_SPLIT=0` | n=5 **12,749.2 → 9,747.1 us** (1.3082); n=6 **13,636.0 → 10,496.4 us** (1.3011) | 6/6, 6/6 | `evidence/r85-evidence__13-paired-c3.txt` |
| 2 | iq3_s 2 rows per workgroup at width 6 (R87), served IQ3_S 27B | `GGML_ARIFI_IQ3S_N6_ROWS` | iq3_s cost of the 6th verify column **+47.23 ± 1.33 → +23.78 ± 2.10 ms/step** | 3 vs 3 launches | `evidence/r87-evidence__34-served-after-n6r2.txt` |
| 3 | q6_K MMVQ admit at width 6 for ONE shape (R74b), 5120×6144 | `GGML_ARIFI_Q6K_MMVQ_ROWS` | **713.3 → 691.1 us** (1.0363) | 6/6 | `evidence/r74b-evidence__21-paired.txt` |
| 4 | iq4_xs MMVQ route (R75), opt-in | `GGML_ARIFI_IQ4XS_MMVQ=route` | none claimed: greedy output diverged on a third-party iq4_xs file, so the default stays `legacy` | — | `docs/OPTIONS-REGISTRY.md` row |

Row 3 is per-shape on purpose: the same route on lm_head measured pick-dependent and was refused;
the other shapes in the receipt show why.

## The number of the week

The whole-step depth table predicted that rows 1 + 2 make the GSQ depth 5→6 marginal **−23.0 ms**
cheaper. Measured cross-session: **−23.21 ± 16.06 ms** (CI excludes 0). The same-session number of
record is **−28.66 ± 34.46 ms** — same sign and size, CI too wide to decide on its own, so it is
banked UNRESOLVED. Receipt: `evidence/r86-evidence__82-marginals.txt`.

## Verification against r73i, same rig

| Check | Result | Receipt |
|---|---|---|
| `MUL_MAT` correctness | **2305 executed / 2305 OK / 0 FAIL**, 876 not supported | `evidence/r86-evidence__19-executed-counts.txt` |
| Greedy bit-identity | **6/6** identical (3 prompts × Q4_K_XL and S-X8 27B) | `evidence/r86-evidence__31-bitid-summary.txt` |
| Perplexity `-b 6`, GSQ IQ3_S 27B | **6.0281 ± 0.14257** both engines | `evidence/r86-evidence__50-ppl-b6-gsq3s-r73.txt`, `…-r86.txt` |
| Perplexity `-b 6`, Q4_K_XL 27B | **5.9932 ± 0.14292 → 5.9928 ± 0.14290** | `evidence/r86-evidence__50-ppl-b6-q4kxl-r73.txt`, `…-r86.txt` |
| Served decode, draft depth 4, 4 launches/arm × 4 rounds | GSQ **7.988 → 7.925 t/s** (−0.8% ± 6.1%); Q4_K_XL **7.208 → 7.192 t/s** (−0.2% ± 7.0%): **TIE**, every CI spans 0 | `evidence/r86-evidence__80-launchlevel.txt` |

## What we could not show

- **No served throughput win at our serve line.** At draft depth 4 GSQ runs **8.118 t/s**
  (`evidence/r86-evidence__81-depth-table.txt`), and depth 4 stays fastest. The gains live in the
  width-5/6 verify columns; a deeper draft reaches them, ours does not yet pay for itself.
- **Q4_K_XL plain decode** launch rows are wide (−1.6% ± 27.7%) and carry no claim.
- `GGML_ARIFI_Q4K_W5_SPLIT` and `GGML_ARIFI_IQ3S_N6_ROWS` have **no row in
  `docs/OPTIONS-REGISTRY.md` yet**; their defaults are read from the startup-line code in
  `ggml/src/ggml-vulkan/ggml-vulkan.cpp`.
- Nothing here was built or run on any GPU but one, or on Linux or macOS.

## Where every number was measured

Beelink SER7 Pro — Ryzen 7 7840HS, **Radeon 780M (RDNA3, gfx1103)**, driver 32.0.31041.1004,
Windows 11 build 29648, Balanced power plan, MinGW-w64 + Ninja, one shared pool of system RAM
(48 GB physical, 16 GB reserved in BIOS). Kernels transfer; numbers do not. Results from other GPUs
are wanted — see the README.
