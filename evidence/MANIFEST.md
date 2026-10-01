# Evidence manifest

Each row maps a shipped file to **the claim it supports**, its **original path**, and the
**sha256 of the original**. Where the release-prep path scrub changed a file, the row says so
and carries the shipped file's own sha256 as well — a verifier hashing the shipped copy must
not be able to mistake a scrub for tampering.

**These files were produced in lane worktrees, which are not part of this repository.** They
are copied here so that a clone actually contains the receipts its documents cite. The
original path column names where each one was produced; `<dir>__<file>` is the shipped name.

Scrub patterns applied to text receipts: absolute user-home paths -> `<home>`, studio paths ->
`<repo>`, e-mail addresses -> `<email>`. Nothing measured was altered.

## Receipts

| Shipped file | Claim it supports | Original path | sha256 (original) | Scrubbed |
|---|---|---|---|---|
| `2026-09-15-sx8-paper-docs__PROVENANCE.md` | S-X8 v4.3 is Apache-2.0, author Marti Vidal Leandro (MarlaLabs), DOI 10.5281/zenodo.21922640 - ten files, each with a source URL and a sha256. This is the basis for the NOTICE entry and `licenses/Apache-2.0.txt`. | `2026-09-15-sx8-paper-docs/PROVENANCE.md` | `b8385db97422b451a8c2dee583602e57142cdaf321b44f36fe47c370fe1fc372` | no |
| `r58-evidence__10-memtypes.txt` | The host-split memory types the device actually offers. | `r58-evidence/10-memtypes.txt` | `ecb5ccac95b176ce02708f574a9cfd73d7a594182014ab66ecceee9b64849728` | **yes** — shipped sha256 `1e93c57bc857e4d5` |
| `r65-evidence__22-ksweep-table.txt` | The k-sweep behind the q5_K route's k<=8192 admission. | `r65-evidence/22-ksweep-table.txt` | `a29039a46134ac6bfffc90fb99d7c5075e3d39d24702260a0661eccf8bc8d1f6` | no |
| `r66-evidence__17-correct-MUL_MAT.txt` | MUL_MAT 2222 executed / 0 FAIL / 876 not-supported at R66. | `r66-evidence/17-correct-MUL_MAT.txt` | `a7c81cea01dfbe6584579140120608226d99c7032dfec6d9cd003d92ef670fde` | no |
| `r66-evidence__19-executed-counts.txt` | The R66 flipped routes were actually exercised (iq3_s + iq3_xxs at n=7; q5_K n=5..8). | `r66-evidence/19-executed-counts.txt` | `bf71d23705fc1f9f8e976045aacda89e97066a4d697d1af441daad07a4666e34` | **yes** — shipped sha256 `38d5200bf28b602c` |
| `r73-evidence__31-bitid-summary.txt` | Bit-identity 6/6 against the r66i stage (no collateral change; greedy runs never reach q6_K at n=7/8). | `r73-evidence/31-bitid-summary.txt` | `878651a1ae0a8d354b03946e3473aa5ea33a137f3b5ad1917c7cc6b7ce695250` | no |
| `r73-evidence__71-promote-ff.txt` | The R73 promotion was a fast-forward: `94461e770` -> `5a7434218`, 2 commits, 0 merges. | `r73-evidence/71-promote-ff.txt` | `baf32f121d4ecbfa80ad384eefffc4aa631895ea39c23ffce3e7d4dd290b5f0e` | **yes** — shipped sha256 `834ecc060d44932c` |
| `r73-evidence__72-trailers.txt` | 0 untrailered commits in the R73 range. | `r73-evidence/72-trailers.txt` | `9c4513f78049152102f8e698feb4b687fb7f531b325c88895b8d54913c7cd3b9` | no |
| `r73-evidence__73-regen.txt` | The series was regenerated at the R73 tip: 588 entries. | `r73-evidence/73-regen.txt` | `dd5fb5f9b57c567f25d07784bd0a946a0af0481a25cff2c77d05f45219ae673e` | no |
| `r73-evidence__74-series-check.txt` | Series integrity PASS - every patch byte-identical to a fresh generation AND a committed blob; 588 patches replayed, tree IDENTICAL outside patches/series. | `r73-evidence/74-series-check.txt` | `191606757d8b436fa5e975c57937c28fa8ece2402c0d6f7f0c1e3e7cd78755d9` | **yes** — shipped sha256 `ee4e7102e731cb95` |
| `r73-evidence__75-tag.txt` | Tag `r73i-release-engine-2026-09-21` points at `037b433a9`. | `r73-evidence/75-tag.txt` | `a41be29188284b3c645e06cacb2008a206ea58f8f607079586859cd468e2591f` | **yes** — shipped sha256 `66bd8c9ca2d72074` |
| `r73-evidence__76-flip.txt` | The engine of record moved to `arifi-b10825-r73i-5a7434218`; both rollback stages asserted present. | `r73-evidence/76-flip.txt` | `ff3a715ffab6cb253533f2b3f739b47184011bd726b4f027a7aa2e586ad60339` | **yes** — shipped sha256 `07e66288503d9d1d` |
| `r73-evidence__77-smoke.txt` | Post-flip smoke rc=0, 37.4 s, all four startup lines present - including `q6_k mmvq route: route (MUL_MAT only, n=7..8)`. | `r73-evidence/77-smoke.txt` | `d91ef65f07b5de731a777790a2484abc69491a1a240b30f40fc191df0c67d138` | **yes** — shipped sha256 `3712b93a11f1383f` |

## Gate cells (`*.rc`)

47 files, all from `r73-evidence/`. One of the 47 R73 gate cells; the claim is '47 rc files, all rc=0' (24 test cells + 12 bit-identity runs, a set of them re-run beside a live 27B load).

| Shipped file | sha256 (original) | Scrubbed |
|---|---|---|
| `r73-evidence__01-build.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__01-configure.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__10-startup-default-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__10-startup-default.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__10-startup-legacy-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__10-startup-legacy.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__12-test-alloc-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__12-test-alloc.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__13-heapres-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__13-heapres.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__14-save-load-state-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__14-save-load-state.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__15-recurrent-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__15-recurrent.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__16a-sx8-pca-unit-cpu-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__16a-sx8-pca-unit-cpu.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__16b-sx8-pca-unit-vk-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__16b-sx8-pca-unit-vk.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__17-correct-ADD.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__17-correct-CPY.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__17-correct-GET_ROWS.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__17-correct-MUL_MAT-ran-beside-27b-load.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__17-correct-MUL_MAT.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__17-correct-MUL_MAT_ID.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__18-mulmat-route-legacy.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__19-mulmat-hoist-off.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__20-mulmat-q6k-ds-off.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__21-mulmat-mmq-coopmat-on.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__22-mulmat-q5k-legacy.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__23-mulmat-iq3-legacy.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__24-mulmat-iq3-n7rows4.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__25-mulmatid-iq3-default.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__26-mulmat-q6k-default.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__27-mulmat-q6k-legacy.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__28-mulmatid-q6k-default.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-Q4-p1-r66.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-Q4-p1-r73.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-Q4-p2-r66.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-Q4-p2-r73.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-Q4-p3-r66.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-Q4-p3-r73.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-SX8-p1-r66.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-SX8-p1-r73.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-SX8-p2-r66.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-SX8-p2-r73.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-SX8-p3-r66.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |
| `r73-evidence__30-bitid-SX8-p3-r73.rc` | `93ff7811a209e2a8479230bbb9b6bc19f7f311d3af383ec350c1db2a7e7d5494` | no |

## The headline numbers, and the receipt behind each one

Every figure a reader is most likely to test now has its measurement file in this folder. These were
copied out of the lane worktrees that produced them; each row gives the claim, the shipped file, the
original lane path and the **sha256 of the original**, so a verifier can hash the shipped copy and
see that it is the original byte for byte.

| Claim it supports | Shipped file | Original lane path | sha256 of the ORIGINAL | sha256 shipped (scrub) |
|---|---|---|---|---|
| q6_K MMVQ route, PAIRED decider for the three wide shapes: per-round ratios, the `won` column that the 58/60 total is summed from, and the ADMIT/LOSS verdict per width. | `evidence/r71-evidence__13-paired.txt` | `C:/ArifiLabs/cache/r71-wt/r71-evidence/13-paired.txt` | `a61aa2cba4e83df5…` | *(identical)* |
| q6_K MMVQ route, PAIRED decider for the two SERVED shapes, measured on the SHIPPED arms (`legacy` / `route`). Carries the worst single round, 0.9894. | `evidence/r71-evidence__82-paired-g2.txt` | `C:/ArifiLabs/cache/r71-wt/r71-evidence/82-paired-g2.txt` | `61c6c25892ab583e…` | *(identical)* |
| Perplexity, legacy arm: the full run including the startup lines that assert which route and which batch size were live. | `evidence/r71-evidence__50-ppl-legacy.txt` | `C:/ArifiLabs/cache/r71-wt/r71-evidence/50-ppl-legacy.txt` | `5dc881927883b8f1…` | *(identical)* |
| Perplexity, route arm at `-b 7`: the paired half of the PPL claim, same startup assertion. | `evidence/r71-evidence__50-ppl-b7-route.txt` | `C:/ArifiLabs/cache/r71-wt/r71-evidence/50-ppl-b7-route.txt` | `9ca1582a5b41caa9…` | *(identical)* |
| q5_K route table: the marginal verify column, `marg(4..8)` 113.8 us on the inherited route against 5.4 us on the R64 route, with the per-round band% beside every cell. | `evidence/r64-evidence__12-table-ffn.txt` | `C:/ArifiLabs/cache/r64-q5k-route-wt/r64-evidence/12-table-ffn.txt` | `378bc7bbcf7707e0…` | *(identical)* |
| iq3 rows-per-workgroup at width 7: 465,399.5 -> 20,890.3 us at m=248320 (22.278x, 6/6), and the neighbouring widths that show the spill is at exactly one width. | `evidence/r66-evidence__4b-perf-table.txt` | `C:/ArifiLabs/cache/r66-wt/r66-evidence/4b-perf-table.txt` | `8e527cb672bd058d…` | *(identical)* |
| iq3 width-7 register-spill fold: the scratch bytes per width behind the rows-2 default. | `evidence/r61-evidence__47-n7rows-fold.txt` | `C:/ArifiLabs/cache/r61-iq3-wide-wt/r61-evidence/47-n7rows-fold.txt` | `727516e9688e99eb…` | *(identical)* |
| iq3 sign-hoist v2 SPIR-V instruction counts: 1229 -> 349 marginal instructions per column, with the iq4_xs control. | `evidence/r61-evidence__22-isa-fold-s62.txt` | `C:/ArifiLabs/cache/r61-iq3-wide-wt/r61-evidence/22-isa-fold-s62.txt` | `f655abd363a08bb2…` | *(identical)* |
| chain151 launch-level A/B, Q4_K_XL 27B: per-launch blocks and the Welch CI the 'every CI contains zero' statement is read from. | `evidence/chain151__c29-q4kxl-27b-launchlevel-AB.json` | `C:/ArifiLabs/research/local-inference/lane-evidence/2026-09-02-lane-209/r66-lane-254/c29-q4kxl-27b-launchlevel/SEAT/AB.json` | `9c1222e3bf136b4b…` | *(identical)* |
| chain151 launch-level A/B, S-X8 27B: same shape. | `evidence/chain151__c30-sx8-27b-launchlevel-AB.json` | `C:/ArifiLabs/research/local-inference/lane-evidence/2026-09-02-lane-209/r66-lane-254/c30-sx8-27b-launchlevel/SX8/AB.json` | `12efdc0cdae1db82…` | *(identical)* |
| chain151 launch-level A/B, GSQ IQ3_S 27B: same shape. | `evidence/chain151__c31-gsq3s-27b-launchlevel-AB.json` | `C:/ArifiLabs/research/local-inference/lane-evidence/2026-09-02-lane-209/r66-lane-254/c31-gsq3s-27b-launchlevel/GSQ3S/AB.json` | `5eda88505811b05f…` | *(identical)* |

**Still UNRECEIPTED, and named rather than quietly dropped.** The documents state the best q6_K cell
as **20,046 -> 14,313 us** (ratio 1.400, 6/6) at 248320x5120 n=7. The shipped paired receipt
`evidence/r71-evidence__13-paired.txt` measures **that same cell** at **20,181.3 -> 14,297.7 us,
ratio 1.4105, 6/6**. The two are the same result from two different round sets - R71's decider and
the R73 re-measurement taken at promotion - and **the R73 raw dump is not shipped**. The ratio and
the 6/6 are receipted; the two absolute microsecond figures in the documents are **not**. Read them
as UNRECEIPTED until that dump is published, and read the shipped table for the measurement that is.

## R86i receipts

Shipped byte-for-byte (no scrub was needed; the copy step refuses a file that carries the personal address or a user-home path). `arifi/main:` = tracked in the studio's working fork line, cut from this repository; `lane-evidence/` = the studio's lane-evidence home.

| Shipped file | Claim it supports | Original path | sha256 (original = shipped) |
|---|---|---|---|
| `r86-evidence__19-executed-counts.txt` | MUL_MAT 2305 executed / 2305 OK / 0 FAIL / 876 not-supported on the R86i tip, per type. | `arifi/main:r86-evidence/19-executed-counts.txt` | `c3a55cb9e3d8c20b7ad7b930eef951da82dddf9f8ce5b899cecd0a749c227d1d` |
| `r86-evidence__31-bitid-summary.txt` | Greedy bit-identity 6/6 against r73i (3 prompts x Q4_K_XL and S-X8 27B). | `arifi/main:r86-evidence/31-bitid-summary.txt` | `bdec1737cd7435d46470685b4abd43b2cfb306b9d756e1e0a9d53b7a37a44180` |
| `r86-evidence__50-ppl-b6-gsq3s-r73.txt` | Perplexity `-b 6`, GSQ IQ3_S 27B, r73i arm: 6.0281 +/- 0.14257. | `arifi/main:r86-evidence/50-ppl-b6-gsq3s-r73.txt` | `8fccb5107fc82aa4aaeac373383a3f9a6b299e50b1c042da11e41f89646b202c` |
| `r86-evidence__50-ppl-b6-gsq3s-r86.txt` | Perplexity `-b 6`, GSQ IQ3_S 27B, r86i arm: 6.0281 +/- 0.14257. | `arifi/main:r86-evidence/50-ppl-b6-gsq3s-r86.txt` | `08f3067513263131386b602038a28fb1e150297c2f5e12eef8e0ac36c4262b2f` |
| `r86-evidence__50-ppl-b6-q4kxl-r73.txt` | Perplexity `-b 6`, Q4_K_XL 27B, r73i arm: 5.9932 +/- 0.14292. | `arifi/main:r86-evidence/50-ppl-b6-q4kxl-r73.txt` | `06e0e5f6c8ae19ca1da221ac29a2e7e62e3fb58390d3bdb8ede5e0faab236e4c` |
| `r86-evidence__50-ppl-b6-q4kxl-r86.txt` | Perplexity `-b 6`, Q4_K_XL 27B, r86i arm: 5.9928 +/- 0.14290. | `arifi/main:r86-evidence/50-ppl-b6-q4kxl-r86.txt` | `be1e9039ec625acd63d26786fa663d119b55cdda325d5ed6eea3a898f792dc47` |
| `r86-evidence__80-launchlevel.txt` | Served decode r86i vs r73i at draft depth 4, 4 launches/arm x 4 rounds, Welch CIs: all four cells TIE. | `arifi/main:r86-evidence/80-launchlevel.txt` | `00d4c8859906ee2c37fd74a5b81366f1027cbfa79b8b9830fc26dce3648353ef` |
| `r86-evidence__81-depth-table.txt` | Whole served step per draft depth on r86i; GSQ depth 4 = 8.118 t/s, the serve line. | `arifi/main:r86-evidence/81-depth-table.txt` | `b93a591467d85b45093bd65d4e41e69761edb6722fbea2b87013211d00881993` |
| `r86-evidence__82-marginals.txt` | GSQ marginal depth 5->6: predicted -23.0 ms; cross-session -23.21 +/- 16.06; same-session -28.66 +/- 34.46 (number of record). | `arifi/main:r86-evidence/82-marginals.txt` | `d5491bcecd56a1cb82b076be83360a360a13ea53dbb51db40819ab18b96ec547` |
| `r85-evidence__13-paired-c3.txt` | q4_K width-5/6 split at 248320x5120: n=5 12,749.2 -> 9,747.1 us (1.3082, 6/6); n=6 1.3011, 6/6. | `lane-evidence/2026-09-22-r85-q4k-width5-overlap/13-paired-c3.txt` | `11ee3186f5489e7e671f7759425f46cc6a6b7a2281be53d5b02c8ffb379706e2` |
| `r74b-evidence__21-paired.txt` | q6_K n=6 admit at 5120x6144: 713.3 -> 691.1 us (1.0363, 6/6), and the cells that were refused. | `lane-evidence/2026-09-23-r74b-q6k-n6-admit/21-paired.txt` | `01e8cde049fe4152b203a7c42d149f5da06c26ce4db341a3b5a727ec4219ca58` |
| `r87-evidence__34-served-after-n6r2.txt` | iq3_s n=6 rows 2, served IQ3_S 27B: iq3_s marginal of the 6th verify column +47.23 +/- 1.33 -> +23.78 +/- 2.10 ms/step, 3 vs 3 launches. | `lane-evidence/2026-09-24-r87-iq3s-n6/34-served-after-n6r2.txt` | `9f4d7646be86f9502864b522e9feeab292a6ab866cc7ad40abd832fa6669521d` |

## Radeon 890M (X1) receipts

Correctness only; no 890M speed number is shipped. `2026-09-30-x1-first-contact/` is a directory in the studio's lane-evidence home.

| Shipped file | Claim it supports | Original path | sha256 (original) | Scrubbed |
|---|---|---|---|---|
| `x1-first-contact__placement-summary.txt` | 890M, fix ON: GSQ IQ3_S 10.75 GiB, Q4_K_XL 15.36 GiB, S-X8 23.40 GiB in DEVICE_LOCAL (the reservation), 0 failed; `legacy` reproduces the shared-billed type (IQ3_XXS 9.19 GiB). DERIVED by `x1_heapsum.py` from the four gzipped alloc traces after checking each against its recorded raw sha256 (printed in the file). | `2026-09-30-x1-first-contact/chain1/2{0,1,2,3}-trace-*.err.gz (derived)` | `6ff4682d458609dcbb7cb00e6ec308fae303e8b724b33b23196b1e15fc08bd10` | no |
| `x1-first-contact__chain2-summary.txt` | 890M, fix ON: MUL_MAT 2305 executed / 2305 OK / 0 FAIL / 876 not supported; greedy identity Qwen3.5-9B Q8_0 4/4 in each of 3 pairs (R vs FA, FL vs FA, R vs FL); the placement line per arm. | `2026-09-30-x1-first-contact/chain2/summary.txt` | `b64ceeefa4354140bdcd4207d94036dfa86efbe88d48a2859f3f0c68084980ae` | **yes** — shipped sha256 `ffb0570981eef6fa` |
| `x1-first-contact__chain1-executed-counts.txt` | 890M, fix ON: MUL_MAT_ID 1004 executed / 1004 OK / 0 FAIL / 10 not supported. Its MUL_MAT block (827) is the run the chain's own timeout cut; the complete MUL_MAT count is in the chain2 summary. | `2026-09-30-x1-first-contact/chain1/19-executed-counts.txt` | `21576dc8a4a71d24b17a2ab42bc15ba556dd9995ed3e5930d0c64f9a04248ab5` | no |

## MISSING - a cited claim whose receipt could not be found

**None by path.** Every receipt cited *by name* in `README.md`,
`docs/release/RELEASE-STORY-r73i.md` and `docs/release/RELEASE-NOTES-r73i.md` was located
and is shipped above. The citation list was extracted from the documents themselves, not
hand-written, so it cannot drift from what the release actually claims.

## What is deliberately NOT here

The lane evidence files, internal maker and checker documents and lane scratch scripts tracked on the fork's working line (155 removal entries: 14 evidence directories + 141 files, 757 paths across the fork range at R86i) are **absent from this repository and from its history**. The paths were removed from every commit in the fork range, and every historical `patches/series/*.patch` blob that carried them lost the matching `diff --git` sections (805 sections in 58 blobs; 34 patch files that carried nothing else were dropped), so no `git show`, no `git log -p` and no file under `patches/` at any revision reconstructs them. This folder holds the receipts the published documents cite, not every file a lane ever wrote.
