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

## MISSING — a cited claim whose receipt could not be found

**None.** Every receipt cited by name in `README.md`,
`docs/release/RELEASE-STORY-r73i.md` and `docs/release/RELEASE-NOTES-r73i.md` was located
and is shipped above. The citation list was extracted from the documents themselves
(`cache/relprep-tmp/evidence.py`), not hand-written, so it cannot drift from what the
release actually claims.

## What is deliberately NOT here

The 436 lane evidence files that were tracked at the R66 tip — build logs, configure logs
and sweep dumps from thirteen `r4*`/`r5*` directories — were dropped from the release branch
under the publish-scope decision, together with 136 internal maker and checker documents.
They remain in the fork's history and in the studio's own worktrees. This folder holds the
receipts the published documents cite, not every file a lane ever wrote.
