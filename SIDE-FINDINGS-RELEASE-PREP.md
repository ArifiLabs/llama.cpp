# Side-findings — release-prep lane, 2026-09-21

Out of scope for the seven items. **Nothing here was acted on.** Each row: what / where / proof /
what I would do.

---

### SF-P1 — `RELEASE-CHECKLIST.md` item 2 states a false absence

- **What.** Item 2 reads *"Absent: no `NOTICE`, no `LICENSES/` directory, and no per-source licence
  file. Verified by directory listing and `git ls-files` at `94461e770`."* The directory exists and
  is tracked, with 16 files and an audited retention table.
- **Where.** `licenses/` at `94461e770`; `licenses/README.md`.
- **Proof.** `git -C C:/ArifiLabs/cache/release-wt ls-files licenses NOTICE` returns
  `licenses/LICENSE-jsonhpp`, `licenses/README.md`, `buun-MIT.txt`, `ciru-MIT.txt`,
  `llama-cpp-turboquant-MIT.txt`, `llama.cpp-MIT.txt`, `powerinfer-MIT.txt`, `prisml-MIT.txt`,
  `rocmfpx-MIT.txt`, `smallthinker-MIT.txt`, `thecodacus-MIT.txt`, `tq3-MIT.txt`,
  `turboq-mtp-MIT.txt`, `turboquant-plus-Apache-2.0.txt`, `turboquant-plus-NOTICE.txt`,
  `zuijdwijk-MIT.txt` — and no `NOTICE`.
- **Would do.** Correct item 2 to the true gap: the root `NOTICE` and the S-X8 Apache-2.0 retention,
  not the directory. Done in the checklist state column by this lane; the prose is HQ's to fix.
- **Note.** `licenses/README.md` itself documents a previous instance of this exact error
  ("a false absence that stood for months … an absence claim is only as good as the place you
  looked"). This is the third occurrence of the same failure mode against the same directory.

### SF-P2 — the checklist's evidence-file count is low by 146

- **What.** Item 7 and SF-4 both say **290** evidence files are tracked. The true figure is **436**,
  across 13 directories.
- **Where.** `r46b-b2-evidence` 3, `r46b-b7a-r3-evidence` 30, `r46b-b7b-loader-evidence` 46,
  `r46b-b7b-r2-evidence` 33, `r46b-b7b-r4-evidence` 2, `r46b-b7c-evidence` 41, `r48-evidence` 23,
  `r48b-evidence` 118, `r48c-evidence` 28, `r51-evidence` 26, `r52b-evidence` 35, `r53-evidence` 33,
  `r58-evidence` 18.
- **Proof.** `git ls-files` at `94461e770`, bucketed by leading directory
  (`cache/relprep-tmp/census.py`). The checklist's list omits `r51-evidence` and `r52b-evidence`
  entirely.
- **Would do.** Nothing beyond the corrected figure carried in this lane's plan and reply.

### SF-P3 — the release story's cited receipts are mostly not in the fork tree

- **What.** The story cites `r66-evidence/*` (the gate receipts behind the headline numbers:
  `71-promote-ff.txt`, `73-regen.txt`, `74-series-check.txt`, `75-tag.txt`, `76-flip.txt`,
  `17-correct-MUL_MAT.txt`, `19-executed-counts.txt`, the `18-*`..`24-*` legacy arms, the `*.rc`
  gate cells) and `r65-evidence/22-ksweep-table.txt`. None of them is tracked at `94461e770`; they
  live in the lane worktrees `cache/r66-wt` and `cache/r65-width-knee-wt`, which are gitignored.
- **Where.** `git ls-files` shows no `r66-evidence` or `r65-evidence` path.
- **Proof.** The 13 tracked evidence directories in SF-P2 contain neither.
- **Would do.** A publishable evidence folder that actually backs the headline numbers has to be
  assembled from the lane worktrees and committed, which is new content and therefore the
  President's call on scope. Until then the curated `evidence/` on the release branch honestly holds
  the one cited receipt that is tracked, and says so.

### SF-P4 — `arifi_sync.py` carries a studio absolute path in its own source

- **What.** 19 studio-path lines live under `tools/`, the first at `tools/arifi-sync/arifi_sync.py:2036`,
  plus `tools/arifi-sync/recipe/build-vulkan.cache-snapshot.txt` (25 user-home lines) and
  `sources.json`'s `other_remotes.turbomerge.url` = `C:/ArifiLabs/research/local-inference/src/_turbo-merge`.
- **Where.** As above.
- **Proof.** `cache/relprep-tmp/sweep.py … --bucket`, `tools/` row.
- **Would do.** The `sources.json` one is a **runtime value**, not prose — it is the configured
  location of a local remote, and blind-replacing it with `<repo>` would make the config describe a
  path that does not exist. It is left alone and belongs with the `other_remotes` disposition the
  President owes (item 6). The prose and recipe-snapshot occurrences are scrubbed.

### SF-P5 — `cmd_provenance` exempts one failure class and not the other

- **What.** The sha-pinned ledger is consulted for the missing-`Measured-effect` class but not for
  the loose-only class, so the documented "grandfather it with a reason" remedy in the checklist
  (item 1, *"or grandfather them explicitly with a reason"*) does not actually work for 12 of the 15
  offenders.
- **Where.** `tools/arifi-sync/arifi_sync.py:177` vs `:179`.
- **Proof.** Line 177 `elif not (is_native or sha in grandfathered)`; line 179
  `if is_loose and not is_strict:` — no ledger term.
- **Would do.** Closed by this lane as blocker 1, with a separate ledger and a qualified PASS line so
  an exemption can never be read as a repair. Recorded here because the checklist's stated remedy
  was not executable as written, which is a finding about the checklist as much as the tool.
