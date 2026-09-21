# RELEASE-PREP PLAN — branch `release/r66i-public-prep-2026-09-21`

Base: `94461e770` (`arifi/main` tip, tag `r66i-integration-2026-09-19`), read live
(`git -C C:/ArifiLabs/cache/main-wt rev-parse HEAD`).

**Nothing is published by this lane.** No `git push`, no remote added, used or removed, no network
call. Publication is the President's word alone.

**Hard rule.** `arifi/main`, its tags and every lane branch are NEVER rewritten. All work below is
new commits on this branch only. The one approach that needs rewritten history (the author e-mail)
is deferred to a separate export branch and is a PROPOSAL, built only after items 2–7 pass.

This plan extends, and does not redo, the CHECKED read-only audit in
`board/programs/local-inference/releases/RELEASE-CHECKLIST.md` and `SIDE-FINDINGS-RELEASE.md`
(studio commits `865dd369`, `d0755ed6`, `6c764800`).

---

## Blocker 1 — commit trailers (`provenance --strict` FAILS)

**Before, on this branch** — `python tools/arifi-sync/arifi_sync.py provenance --strict`:

```
commits                   : 658
Taken-from/Origin STRICT  : 283 / 658
Measured-effect           : 224 / 658
grandfathered NATIVE      : 363 / 658
FAIL (--strict): 12 loose-only, 3 without Measured-effect.
```

**The finding that decides the approach.** In `tools/arifi-sync/arifi_sync.py` `cmd_provenance`, the
sha-pinned ledger is consulted for the missing-`Measured-effect` class (line 177,
`elif not (is_native or sha in grandfathered)`) but **not** for the loose-only class (line 179,
`if is_loose and not is_strict`). So no entry in any existing ledger can clear the 12 loose-only
commits. The tool as written offers exactly two outs: amend the 12 messages, or teach it a second
exemption class.

Amending is a history rewrite of published, tagged, series-pinned commits. Barred.

**Approach — a new, separately named ledger, plus the ten lines of tool support that read it.**

- New file `tools/arifi-sync/trailer-exemptions.json`, same shape and doctrine as
  `native-grandfather.json` and `pending-trailers.json`: one sha-pinned row per commit, with its
  subject, its class, and the reason in the author's own words.
- `native-grandfather.json` is **not** touched. Its own text says the ledger is CLOSED and that new
  commits must never be added to it; that instruction is obeyed.
- `pending-trailers.json` is **not** used either. Its doctrine is debt owed by commits on *unmerged*
  branches, to be pasted when the work reaches `arifi/main`. These 15 are already on `arifi/main`.
- `cmd_provenance` consults the new ledger for both classes.

**The exemption must not read as a repair.** The report gains its own counter line and the exit line
is qualified, so that a reader of the receipt cannot mistake an exempted commit for a fixed one:

```
format-exempt             : 15 / 658  (sha-pinned in tools/arifi-sync/trailer-exemptions.json)
PASS (15 exempt - 12 loose-only, 3 without Measured-effect; see the ledger for each reason).
```

A bare `PASS` here would be a manufactured receipt. Verified on the after-run output, not asserted.

- One assertion added to `tools/arifi-sync/test_arifi_sync.py`: a listed sha is not reported
  loose-only, and the exempt tally counts it.
- Needs history rewriting: **NO.**

**What each class honestly is.** The 12 loose-only carry their provenance in the message where a
human reads it; a continuation line starting in column 0 ends git's trailer block and invalidates
the trailers above it (the tool's own help says so). The provenance is present; the *parse* is the
defect. The 3 without `Measured-effect:` carry `Taken-from:`/`Origin:` and were landed without a
measured cell; the legal value `UNMEASURED` could only be added by amending them.

## Blocker 2 — licence and NOTICE

**Correction to the checklist, found here.** Item 2 states *"no `LICENSES/` directory, and no
per-source licence file … verified by directory listing and `git ls-files`."* That is **false at
`94461e770`**: `git ls-files licenses` returns 16 tracked files, including a full per-source
inventory and `licenses/README.md`, an audited retention table with a documented correction history.
The true gap is narrower and is stated below. (Recorded as SF-P1.)

| Component | Licence | Retained text | State |
|---|---|---|---|
| llama.cpp (upstream) | MIT | `LICENSE`, `licenses/llama.cpp-MIT.txt` | present |
| PowerInfer, `smallthinker` | MIT | `licenses/powerinfer-MIT.txt`, `smallthinker-MIT.txt` | present |
| charlie12345/ROCmFPX | MIT | `licenses/rocmfpx-MIT.txt` | present |
| ciru-ai/ROCmFPX | MIT | `licenses/ciru-MIT.txt` | present (no code carried) |
| PrismML-Eng/llama.cpp | MIT | `licenses/prisml-MIT.txt` | present |
| turbo-tan/llama.cpp-tq3 | MIT | `licenses/tq3-MIT.txt` | present |
| llama-cpp-turboquant | MIT | `licenses/llama-cpp-turboquant-MIT.txt` | present |
| turboquant_plus | Apache-2.0 | `licenses/turboquant-plus-Apache-2.0.txt` + `-NOTICE.txt` | present |
| thecodacus/llama.cpp | MIT | `licenses/thecodacus-MIT.txt` | present |
| LaurentZuijdwijk/llama.cpp | MIT | `licenses/zuijdwijk-MIT.txt` | present |
| jtrefon/llama.cpp-turboq-mtp | MIT | `licenses/turboq-mtp-MIT.txt` | present |
| spiritbuun/buun-llama-cpp | MIT | `licenses/buun-MIT.txt` | present |
| nlohmann/json | MIT | `licenses/LICENSE-jsonhpp` | inherited from upstream |
| **S-X8 v4.3 (MarlaLabs)** | **Apache-2.0** | **absent** | **THE GAP** |

**S-X8's licence is ESTABLISHED, not guessed.** `research/local-inference/lane-evidence/
2026-09-15-sx8-paper-docs/PROVENANCE.md:3`: *"All files Apache-2.0 (author Martí Vidal Leandro,
MarlaLabs)"*, over ten files each banked with a sha256 and a source URL, DOI
10.5281/zenodo.21922640, v1.0 2026-08-13. The tree carries S-X8 across 37 source files
(`GGML_TYPE_SX8` = 57, `mul_mat_vec_sx8.comp`, `dequant_sx8.comp`, `tools/sx8/`).

**Approach.**
- Add `licenses/Apache-2.0.txt` — the Apache License 2.0 terms, **byte-identical to lines 1–176 of
  the already-retained `licenses/turboquant-plus-Apache-2.0.txt`** (that copy's lines 177+ are
  turboquant's own filled-in copyright block, which is that licensor's and is not reused). Nothing
  is fabricated and the equality is checkable with `diff`.
- Add a root **`NOTICE`** naming every component above with its licence, its source and where its
  retained text lives; the S-X8 row carries the author, MarlaLabs, the DOI and the distinction the
  release story already draws — the *format*, the reference CUDA kernels and the author's own
  llama.cpp patch are MarlaLabs'; the Vulkan kernels in this tree are ours.
- Add the S-X8 and Apache-2.0 rows to `licenses/README.md`, in its existing table form.
- **Per-file headers: not added.** Apache-2.0 §4(a)–(d) requires the licence, the NOTICE contents and
  retained attribution notices to travel with the derivative work; it does not require a header on
  every file, and the S-X8 source files fetched in the capture carry no header of the author's to
  retain. A header we composed would be our text over his name.
- **UNRESOLVED / OWED:** MarlaLabs' own `LICENSE` file has never been fetched — the 2026-09-15
  capture took the papers, the container scripts, the CUDA kernels and the patch, not a LICENSE
  blob. The licence *statement* is banked; the licensor's own licence *file* is not retained. Closing
  it needs one network fetch, which this lane is barred from making. Listed in the final reply.
- Needs history rewriting: **NO.**

## Blocker 3 — publish scope

**Approach.** Delete from this branch (the files stay untouched in `C:/ArifiLabs/cache/main-wt`,
a separate worktree of the same repository):

- the root internal documents — `AGENTS.md`, `CLAUDE.md`, `UPDATE-RUNBOOK.md`,
  `PUBLISH-DISCLOSURE-DRAFT.md`, `repo-env.json`, `R48C-Q6K-PLANTED-RED.patch`, every `OPUS-*` and
  `CHECK-*` report and prompt, and the loose `r51-*.cmd` / `r53-*.cmd` / `r58-*.cmd` run scripts;
- the 13 tracked `*-evidence/` directories (436 files by `git ls-files`, not the 290 the checklist
  estimated — the checklist's figure is corrected here with its command).

**Keep-set — the receipts the release story cites.** The story's cited receipts and the tracked
evidence set are two different sets and are reported as two sets rather than silently reconciled:
`r66-evidence/*` and `r65-evidence/*` are cited by the story but live in `cache/r66-wt` and
`cache/r65-width-knee-wt` and are **not tracked at `94461e770`**, so they cannot be curated from
this tree. The one cited receipt that *is* tracked is `r58-evidence/10-memtypes.txt`. It is moved to
`evidence/` with a `README.md` stating exactly that, so an empty or falsely-full `evidence/` is not
produced.

Also added to the branch, from `board/programs/local-inference/releases/`: the R66 release story,
`RELEASE-NOTES-r66i.md`, and the README section merged into the fork `README.md` under a clear
heading with the upstream README content preserved.

`patches/series/` is **NOT** dropped. It is the fork's reproducibility claim (item 8) and dropping it
to improve a sweep number would be cosmetic.

- Needs history rewriting: **NO.** Deletions are a new commit; the history that created the files
  stays.
- **Known consequence, stated not hidden:** `arifi_sync series check` verifies that replaying
  `patches/series/*.patch` reproduces the tree. The prep commits on this branch are not in the
  series, and the replay recreates the files this branch deletes, so the check is expected to report
  the difference. The series claim is about `arifi/main`'s 658-commit history and remains true there
  — the release branch adds prep commits on top of it. Run and reported in step 7, not asserted.

## Blocker 4 — path and identity scrub

Baseline on this branch, tracked text files only (5,645 of 5,727 scanned;
`python cache/relprep-tmp/sweep.py C:/ArifiLabs/cache/release-wt --bucket`):

| class | lines | files |
|---|---|---|
| user-home | 220 | 36 |
| studio-path | 2,826 | 235 |
| e-mail (all addresses, incl. upstream) | 2,893 | 493 |
| qnap | 15 | 5 |
| credential/token shapes | **0** | 0 |
| rig hostname | 33 | 20 |

**Approach.** Most of the user-home and studio-path hits leave with the blocker-3 deletions
(evidence dirs and root internal documents). What remains is replaced in tracked TEXT files with
neutral placeholders — `<repo>`, `<models>`, `<home>` — never inside code semantics: a path in a
string literal that is read at runtime is a side-finding, not a blind replace. Reviewed per file.

**Target 0 is not reachable on this branch, and pretending otherwise would be the dishonest move.**
After the deletions the residue is `patches/series/*.patch` — 112 user-home and 1,597 studio-path
lines. Those files are **generated from git**: editing them in place breaks `series check`, whose
whole claim is byte-identity to a fresh generation; regenerating them means rewriting the source
commit messages and diffs. Both costs are real, both are stated, and neither is paid here. The
residual is reported with that explanation and with the count, and it is the second thing the export
branch below would fix.

`docs/backend/snapdragon/windows.md:127` is **upstream content** and is left alone. `qnap` hits were
reviewed and cleared in the checklist (a base64 token and an integrity hash; no NAS path).
Rig hostnames are the intentional rig disclosure. The e-mail figure above is a wider pattern than
the checklist's — it counts upstream `AUTHORS` (2,040) and vendored third-party code, which stay.

- Needs history rewriting: **NO** for the working tree; **YES** for `patches/series` and the author
  identity — deferred to the export branch.

## Blocker 5 — `other_remotes`

Report only. Nothing added, nothing removed, no remote touched. Read from `git remote -v` and
`tools/arifi-sync/sources.json`.

## Step 7 — verification

Source equivalence rather than a build (no builds, no GPU, another maker owns the box):
`git diff 94461e770 HEAD -- ggml src common tools tests include cmake CMakeLists.txt`, with every
changed source file listed and given a one-line reason. Only licence/comment/path-placeholder
changes and the `arifi_sync.py` ledger support may appear.

---

## The export branch — PROPOSAL ONLY, built after 2–7 pass

`release/r66i-public-export`, built **from** this prep branch, is the only place a history rewrite
would happen. It is proposed, not created, and HQ and the President decide whether it is the branch
that gets published.

- **Why.** One rewrite closes two items at once: the author e-mail on 396 commits, and the 1,709
  absolute-path lines inside `patches/series/*.patch`, which cannot be scrubbed any other way.
- **Tool.** `git filter-repo --email-callback` (rewrite `arifilabs@users.noreply.github.com` to the GitHub
  no-reply form, per seat-62 §4aa) plus a `--replace-text` file for the path placeholders, then
  regenerate the series with `arifi_sync` and re-run `series check` on the export branch's own base.
- **What breaks.** Every sha in the range changes. Therefore: the `r66i-integration-2026-09-19` tag
  and every other tag no longer point into the published history; every sha-pinned ledger
  (`native-grandfather.json`, the new `trailer-exemptions.json`, `protected-wins.json`) needs
  re-keying exactly as `UPDATE-RUNBOOK.md` 2.2.1 describes for a base move; every evidence citation
  by sha in the studio registers becomes unverifiable against the published tree; and the patch
  `From:` lines change, so replay identity is re-established rather than preserved.
- **How it is verified.** `git rev-parse <export>^{tree}` equals `git rev-parse <prep>^{tree}` — the
  published *content* must be identical, only the commit metadata and the generated series differ.
- **Why it is not run here.** `filter-repo` over 658 commits is exactly the CPU and disk load the
  brief bars while another maker holds the box, and publishing a rewritten history is the
  President's decision, not a maker's.
