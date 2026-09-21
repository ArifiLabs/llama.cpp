# Curated evidence

This folder holds the receipts behind the headline numbers **that are tracked in this
repository**. It is deliberately small, and the gap is stated rather than hidden.

## What is here

| File | What it backs |
|---|---|
| `10-memtypes.txt` | The host-split memory-type read, cited by the release story §"the memory types the device actually offers". Originally tracked at `r58-evidence/10-memtypes.txt`. |

**This copy is not byte-identical to the original.** One line of it named an absolute studio path
(`…/cache/r51-spill-wt/r51-evidence/30-planorder.txt`) and was rewritten to a `<repo>` placeholder by
the release-prep path scrub. Nothing measured was altered — the change is one path string in a prose
line — but an edited receipt must say it was edited. The unedited original is in the fork history at
`94461e770:r58-evidence/10-memtypes.txt`.

## What is NOT here, and why

The release story cites most of its gate receipts from `r66-evidence/*` — `71-promote-ff.txt`,
`73-regen.txt`, `74-series-check.txt` (the series-integrity PASS), `75-tag.txt`, `76-flip.txt`,
`17-correct-MUL_MAT.txt`, `19-executed-counts.txt`, the `18-*`..`24-*` legacy arms and the `*.rc`
gate cells — and `r65-evidence/22-ksweep-table.txt`.

**None of those is tracked in this repository at any revision.** They live in the lane worktrees
that produced them, which are not part of the published tree. The cited set and the tracked set are
two different sets, and this folder reports them as two sets rather than quietly presenting the
tracked leftovers as though they were the cited receipts.

The 436 evidence files that *were* tracked at `94461e770` were lane working files — build logs,
configure logs, sweep dumps — from thirteen `r4*`/`r5*` lane directories. They were dropped from the
release branch under the publish-scope decision, together with 133 internal maker and checker
documents. They remain in the fork's history and in the studio's own worktrees; nothing was
destroyed.

Publishing a folder that actually backs the headline numbers means committing those lane receipts as
new content, which is a scope decision this branch does not take on its own.
