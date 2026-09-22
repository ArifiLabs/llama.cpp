# RELEASE-PREP PHASE 4 RECEIPTS — the export branch is rebuilt as a REAL fork of ggml-org

**Local only. Nothing pushed; no remote added, used or removed.** All work in a NEW throwaway clone
`<repo>/cache/release-export2`, taken with `git clone --no-local`; `filter-repo` was never run inside
the real repository or any of its worktrees. The earlier clone `cache/release-export` was left
untouched for the checker still reading it.

Phase 4 answers the independent check `CHECK-RELEASE-r73i-FABLE.md` (verdict NOT-READY). Its three
blocking findings and the doc-level corrections are closed here. **The one structural change is C1:**
the rewrite no longer touches upstream's history at all.

> **No literal that this repository scrubs appears in this document.** A receipt that quotes the
> string it removed is how the string survives the removal (LL-217, and it happened twice in
> phase 3). Scrubbed values are named by their class.

---

## 1. The fork range, established before anything was rewritten

```
$ git rev-parse export2                                   51597376fac3aa3400b712f47f5715d2897c2f27
$ git rev-parse 9e0e220594af405a62835dc3a27495729fd8506b^{commit}
                                                          9e0e220594af405a62835dc3a27495729fd8506b
$ git describe --tags --exact-match 9e0e22059              b10825
$ git merge-base --is-ancestor 9e0e22059 export2           rc=0
$ git rev-list --count 9e0e22059..export2                  680      <- THE FORK RANGE
$ git rev-list --count 9e0e22059                           10825    <- upstream's own history
$ git cat-file -p 9e0e22059 | head -5
    tree ffc0e3ec249e6c8311eb3ada8703fe824e0af103
    parent 0afb805b19e26c719a466145761faabad3af1a74
    author Aldehir Rojas <hello@alde.dev> 1788685150 -0500
    committer GitHub <noreply@github.com> 1788685150 +0300
    gpgsig -----BEGIN PGP SIGNATURE-----
```

Receipt: `cache/relprep4/00-range.txt`.

## 2. What was removed, and how the list was built

The scope-cut commit's own delete list is **not** a sufficient source, because a file added and
deleted earlier in the range never appears in it. The list was therefore derived from the **union of
every path touched in the whole range**:

```
$ git log --pretty=format: --name-only --no-renames 9e0e22059..export2 | sort -u
paths ever touched in the fork range   : 3330
scope-cut commit's delete list          :  569   (133 root documents + 436 evidence files)
internal paths found across the range   :  572
  of which NOT in the scope-cut list    :    3   (three .cmd wrappers, added and deleted earlier)
scope-cut paths absent from the range   :    0
removal list handed to filter-repo      :  149 entries
                                          = 13 evidence DIRECTORY prefixes + 136 file literals
```

Directory prefixes rather than 436 file literals, so a historically-deleted member of one of those
directories cannot slip through. The curated `evidence/` folder and its `MANIFEST.md` — the
*published* receipts from phase 2 — are not in the removal list and are untouched.

Receipts: `cache/relprep4/01-pathscan.txt`, `02-removal.txt`, `paths-remove.txt`.

## 3. ONE filter-repo pass, restricted to the fork range

```
git_filter_repo.py --force --refs 9e0e220594af405a62835dc3a27495729fd8506b..export2 \
    --mailmap <mailmap> --replace-text <blob rules> --replace-message <message rules> \
    --paths-from-file <removal list> --invert-paths
```

`Parsed 680 commits` — the number in the range, and nothing below it. Rules:

| Mechanism | What it does |
|---|---|
| `--mailmap` | our three identities → `arifilabs@users.noreply.github.com`. **Names unchanged.** Third-party authors (Codex, Marshall, Jian Chen) are attribution and are untouched. |
| `--replace-text` | the four user-home path forms → `<home>`; the President's personal address → the no-reply address. Blob content. |
| `--replace-message` | the same four path forms and the same address. Commit messages. |
| `--paths-from-file` + `--invert-paths` | the 149 removal entries, across every commit in the range. |

Studio paths (`C:/ArifiLabs/…`) are **not** in any rule set, by the President's standing decision.

### The address is replaced in BLOBS, not only in author metadata

A mailmap reaches `From:` metadata. It does not reach the address written *inside* a
`patches/series/*.patch` blob, and the pre-rewrite series carried one in every patch header. That is
why `--replace-text` carries the address rule as well as the path rules, and why §7 counts the
address over **history**, not over the checkout.

## 4. C1 CLOSED — upstream's commits are untouched, so this is a real fork

```
$ git rev-parse 9e0e220594af405a62835dc3a27495729fd8506b^{commit}
    9e0e220594af405a62835dc3a27495729fd8506b        <- unchanged by the rewrite
$ git merge-base --is-ancestor 9e0e22059 export2     rc=0
$ git rev-list --count 9e0e22059                     10825   <- unchanged
$ git cat-file -p 9e0e22059 | head -5                gpgsig block still present
$ git describe --tags --exact-match 9e0e22059        b10825
$ git rev-list --count 9e0e22059..export2            661     <- fork commits AFTER the rewrite
```

A sha *is* the hash of the commit object, so an unchanged sha is byte-identity: upstream's 10,825
commits, their signatures and their tags are exactly ggml-org's. A public user can add ggml-org as a
remote, fetch it, find a common ancestor and rebase. `README.md` states the base sha and gives the
`merge-base` command to check it.

**680 → 661: nineteen commits were pruned**, each one a commit whose only content was internal
material. filter-repo maps a pruned commit to an all-zeros sha in its `commit-map`, which is how
they were counted.

Receipt: `cache/relprep4/04-upstream-preserved.txt`.

## 5. The re-key — and the two traps that are new in phase 4

Every ledger is restored from the **prep branch original** and mapped **once** (filter-repo's
`commit-map` is cumulative; re-keying an already-re-keyed ledger resolves nothing — phase 3 §3).

| Ledger | re-keyed | unmapped | pruned | unchanged |
|---|---|---|---|---|
| `native-grandfather.json` | 394 | 0 | **6** | 0 |
| `trailer-exemptions.json` | 15 | 0 | 0 | 0 |
| `pending-trailers.json` | 4 | 0 | **1** | 3 |
| `protected-win-baseline.json` | 34 | 0 | 0 | 0 |
| `protected-wins.json` | 29 | 0 | 0 | 1 |
| `sources.json` base pin | **NOT re-keyed** | — | — | — |

**Trap 1 — the all-zeros sha.** Phases 2 and 3 ran no path filter, so nothing was ever pruned. This
pass prunes, and a pruned commit's `commit-map` value is forty zeros. Writing that into a ledger
would pin every affected row to a commit that cannot exist. Seven rows hit it; each is left on its
original key with a `sha_note` saying the commit is not in the published history.

**Trap 2 — the base pin must NOT move this time.** Phase 2 re-keyed `sources.json`
`base.upstream_sha` because the rewrite had re-hashed upstream. It has not this time, so the pin is
restored from the prep branch **verbatim** and `git diff` against the prep branch for that file is
empty. Re-keying it blindly would have pointed the audit at a commit that does not exist and
silently widened `provenance` from the fork range to the whole 11,486-commit history — the exact
failure phase 2 documented.

Receipt: `cache/relprep4/05-rekey.txt`.

## 6. B3 CLOSED — the 666 vendored files under `powerinfer/libaz/external/` are licensed

```
$ git ls-files powerinfer/libaz/external | wc -l      666
$ ls powerinfer/libaz/external                        cli11  fmt  googletest  perfetto
$ ls powerinfer/libaz/external/perfetto/LICENSE*      No such file
```

| Component | Licence | Retained as | Bytes | sha256 |
|---|---|---|---|---|
| CLI11 | BSD 3-Clause | `licenses/libaz-cli11-BSD-3-Clause.txt` | 1,611 | `93f0641a…045e5` |
| {fmt} | MIT | `licenses/libaz-fmt-MIT.txt` | 1,458 | `25b5db04…b369cf` |
| GoogleTest | BSD 3-Clause | `licenses/libaz-googletest-BSD-3-Clause.txt` | 1,503 | `46c35a3c…17b8e7` |
| perfetto | Apache 2.0 | `licenses/perfetto-Apache-2.0.txt` | 10,951 | `a1193686…d2f773` |

The first three files are the ones the vendored drops ship, copied verbatim. **perfetto ships no
licence file at all** — its licence is stated in the header of `perfetto.h` (*"Copyright (C) 2019 The
Android Open Source Project … Apache License 2.0"*), and Apache-2.0 §4(a) requires the text to
travel, so the text is retained here. It is deliberately a second copy of terms already in
`licenses/`: phase 2 removed a licensor-*neutral* Apache stand-in because two unnamed copies invite
the reader to ask which governs, and this one is named for its component so that question cannot
arise. `NOTICE` says so in the perfetto entry.

`NOTICE` now also names upstream's own `vendor/` inheritance row by row — cpp-httplib, miniaudio,
stb, sheredom, and the three `vendor/hash/` drops (rotate-bits MIT, sha256 public domain, xxhash BSD
2-Clause) — each read out of the file itself, not from memory.

Receipt: `cache/relprep4/06-licences.txt`.

## 7. B2 CLOSED — the address is zero in the TREE **and** in HISTORY

The phase-3 sweep measured the checkout. The checkout was never where the problem was.

```
tree    : git grep -I -c -P -e <pattern> export2
history : git log -p --no-color 9e0e22059..export2       (295,758,462 bytes streamed, rc=0)
```

| class | tree lines | tree files | history lines |
|---|---|---|---|
| the President's user name, `Users/` path form | **0** | **0** | **0** |
| the President's user name, bare token | 17 | 5 | 10 |
| the President's personal address | **0** | **0** | **0** |
| credential / token shapes | 4 | 4 | 8 |
| upstream's `MyUser` / `MyUsers` placeholders | 16 | 5 | 18 |
| studio path `C:/ArifiLabs` | **183** | 34 | 3,316 |
| the no-reply identity | 576 | 557 | 3,036 |

- **The address and the user-home path form are zero in both columns.** That is the finding B2
  asked for, measured where B2 said to measure it.
- **The four credential-shape hits are not credentials**, and they are printed in full rather than
  waved through (`cache/relprep4/13-credentials.txt`): two are the fork's own test fixture
  `SECRET = "BEGIN PRIVATE KEY\nnot-part-of-any-candidate…"`, and two are the letters `ghp_`
  occurring inside a base64 blob in a vendored patch, with punctuation on both sides.
- **The bare-token hits are SF-P6 and unchanged**: upstream's `AUTHORS`, upstream's
  `docs/speculative.md`, this lane's own side-findings document, and two generated patches.
- **Studio paths fall 2,842 → 183 lines**, not because a rule was added — none was — but because
  the 51.8 MB scope-cut patch that carried them is gone (§8).

Receipts: `cache/relprep4/10-sweep.txt`, `13-credentials.txt`.

## 8. C2 CLOSED — the cut material is absent from history, not merely deleted by a commit

```
$ git grep -I -c -F -e 'r48b-evidence/'              export2 -- patches/     0 files
$ git grep -I -c -F -e 'CHECK-UPDATE-ROOT-R9-SOL-LOW.md' export2 -- patches/ 0 files
$ git grep -I -c -F -e 'r48b-evidence/'              export2                 0 files
$ git grep -I -c -F -e 'CHECK-UPDATE-ROOT-R9-SOL-LOW.md' export2             0 files
```

The three largest files under `patches/` are now:

```
22,733,516  patches/series/0007-powerinfer-vendor-streaming-library-…patch
14,162,837  patches/series/0318-lane-151-GOAL-6-hygiene-drop-648-tracked-…patch
14,135,248  patches/series/0238-layer7-0001-WIP-add-TurboQuant-KV-cache-types-…patch
```

**`0593-scope-cut-…patch` (51,794,897 B) does not exist.** There is no scope-cut commit any more:
the paths were removed from every commit in the range, so the commit that deleted them had nothing
left to delete and was pruned. `git show <any cut file>` finds nothing at any revision.

What remains are **citations by name** in prose — `docs/OPTIONS-REGISTRY.md` and some patches name
`OPUS-R48C-Q6K-MATVEC-REPORT.md` and `r53-evidence/…` as the source of a measurement. Those are
references, not content; the claim is kept and `evidence/MANIFEST.md` states that the named file is
absent from this repository. Side-finding SF-P9.

Receipt: `cache/relprep4/11-patchcheck.txt`.

## 9. Identity — the engine is untouched, and every other difference has a reason

```
$ git diff --stat 037b433a9 export2 -- ggml src common tests include cmake CMakeLists.txt
    (EMPTY)      differing files: 0
```

**No inference, kernel or build code is touched by this branch.** Against the prep tip, in the same
clone so a real `git diff` is possible:

| reason | paths |
|---|---|
| `patches/` — the generated series, regenerated over the rewrite | 909 |
| the 5 re-keyed ledgers | 5 |
| `licenses/` — the four retained licence texts (B3) | 4 |
| `RELEASE-PREP-PHASE2/3-RECEIPTS.md` — the class, not the literal (B2) | 2 |
| `NOTICE` (B3), `README.md` (C1/C5/C7), `docs/OPTIONS-REGISTRY.md` (C7), the two release documents (C5), `evidence/MANIFEST.md` (C6) | 6 |
| `AGENTS.md`, `CLAUDE.md` — **upstream's own files, restored** | 2 |
| **paths with no reason** | **0** |

The last row of the table is the one worth reading. `AGENTS.md` and `CLAUDE.md` exist in upstream
llama.cpp. The fork had overwritten them with internal content, and the prep branch's scope-cut
deleted them outright — which would have published a fork that silently *drops* two of upstream's
files. Removing the fork's changes to those two paths restores upstream's blobs:

```
$ git rev-parse 9e0e22059:AGENTS.md  ->  6d83a02f425239a1b5dab3a1c467132d49e5d5ba
$ git rev-parse export2:AGENTS.md    ->  6d83a02f425239a1b5dab3a1c467132d49e5d5ba
$ git rev-parse 9e0e22059:CLAUDE.md  ->  302cdeab99cfd3661740427eb97391e7b573bac1
$ git rev-parse export2:CLAUDE.md    ->  302cdeab99cfd3661740427eb97391e7b573bac1
```

Receipt: `cache/relprep4/12-identity.txt`.

## 10. The audits, EXECUTED, on the tip this document sits one commit below

`provenance --strict` and `series check` were run with the prep worktree's own `arifi_sync.py`
against this clone. **The numbers below were produced by those runs**, not copied from a checklist
(F-112). They are measured at `40f25541e`, the tip *before* this document and the side-findings were
committed — a receipt cannot name the commit that adds it. The studio's `RELEASE-CHECKLIST.md`
carries the final post-convergence numbers.

```
$ arifi_sync.py --config <clone>/tools/arifi-sync/sources.json --repo <clone> \
      provenance --strict --ref export2
range                     : 9e0e220594af405a62835dc3a27495729fd8506b..export2
commits                   : 663
Taken-from/Origin STRICT  : 294 / 663
Taken-from/Origin LOOSE   : 306 / 663
Measured-effect           : 236 / 663
native (Origin trailer)   : 91
grandfathered NATIVE      : 357 / 663
EXEMPT, NOT REPAIRED      : 15 / 663   (12 loose-only + 3 without Measured-effect)
PASS                                                                            rc=0
```

```
$ arifi_sync.py … series check --ref export2
PASS: every patch is byte-identical to a fresh generation from git AND a committed blob.
replayed 588 patches cleanly -> d021f7981 (tree 45314137d)
PASS: replayed tree is IDENTICAL to export2 outside patches/series               rc=0
```

`series regen` was run, committed by explicit pathspec, and run again: the second run left the
working tree clean — **converged** (`git status --porcelain` = 0 lines). The directory-pathspec trap
phase 2 recorded was avoided by staging the 909 changed paths individually.

**The series shrinks 604 → 588** and that is the removal working: the commits that only touched
internal material no longer exist, so they produce no patch.

Receipts: `cache/relprep4/07-regen1.txt`, `07-regen2.txt`, `08-provenance.txt`, `09-seriescheck.txt`.

## 11. The real repository was not touched

```
$ git -C <repo>/cache/main-wt rev-parse HEAD    037b433a911e45f88ea7616b67d737efa22b8d29
$ git -C <repo>/cache/main-wt status --short    (clean)
$ git -C <repo>/cache/main-wt tag --points-at HEAD   r73i-release-engine-2026-09-21
$ git rev-parse release/r73i-public-prep-2026-09-21  51597376fac3aa3400b712f47f5715d2897c2f27
```

Checked before the work and again after it. No tag was created in the real repository; the release
tag exists only in the throwaway clone. The clone's only remote is `origin`, the local fork path.

## 12. The tag, and what it is not

```
$ git tag -l r73i-public-2026-09-22        (BEFORE)  0    <- the name did not exist in the clone
$ git tag --points-at export2              r73i-public-2026-09-22
```

**B1's cause was a name collision**: the old block re-used `r73i-release-engine-2026-09-21`, which
the clone had inherited pointing at a commit that is not on the branch, so `git tag` failed and the
following `git push <that tag>` would have published 11,487 pre-rewrite commits. The new name did not
exist in the clone, and the publish block in `RELEASE-CHECKLIST.md` refuses before the push unless
the tag resolves to the branch tip.

Publication is the President's word alone. Nothing here has been pushed.

## 13. Three surfaces a tree sweep cannot see, and one stale base the check did not catch

**The 19 pruned commits are named, not asserted.** The originals are still in the prep worktree, so
each removed commit was read back — subject *and* the paths it touched:

```
commits pruned                                                : 19
pruned commits touching ANY path outside the removal list     : 0
```

They are the scope-cut commit itself, twelve `docs(r48*/r51/r58)` receipt commits, three
`OPUS-*-REPORT` commits and three `runbook` edits. Nothing load-bearing was pruned, and that is a
checkable statement rather than a promise.

**Committer identity.** `git log -p` prints `Author:` and never `Committer:`, so §7's history count
could not have seen a committer address. Counted separately:

```
$ git log --format='%an|%ae|%cn|%ce' 9e0e22059..export2 | sort | uniq -c
  477  Angelo Arifi|<no-reply>|Angelo Arifi|<no-reply>
  151  m|<no-reply>|Angelo Arifi|<no-reply>
   29  Codex|codex@openai.com|Angelo Arifi|<no-reply>
    4  ArifiLabs|<no-reply>|ArifiLabs|<no-reply>
    2  Marshall|assistant@llama.cpp|Angelo Arifi|<no-reply>
    1  Jian Chen|jianchen0311@gmail.com|Angelo Arifi|<no-reply>
    1  hq|<no-reply>|Angelo Arifi|<no-reply>
distinct identities: 7; carrying a scrubbed value: 0
```

The three third-party authors keep their own addresses — that is attribution, and a mailmap that
rewrote it would be misattribution.

**The annotated tag object.** A tag's message is in neither the tree nor `git log -p`, and the tag is
one of the two things a push sends. `git cat-file -p r73i-public-2026-09-22` was read in full:
tagger `ArifiLabs <the no-reply address>`, no scrubbed value present.

**A stale base in `README.md`, found by re-reading the shipped set for superseded values.** Three
places still told a public reader the series sits on `b10453` (`4df29be4f`) — a base two upstream
bumps out of date — including a runnable `series replay --onto 4df29be4f`. They now read `b10825`
(`9e0e220594af405a62835dc3a27495729fd8506b`), which is what the branch actually replays onto. The
same sweep confirms the abandoned re-hashed base `4627f376f` appears **nowhere** in the shipped set.
Superseded shas still present in `patches/series/MANIFEST.md`, `docs/FINDINGS.md`,
`licenses/README.md` and the release story are **history** — commit subjects and past audits — and
are correct where they stand.

Receipts: `cache/relprep4/21-stalesweep.txt`, `22-pruned-and-identity.txt`.
