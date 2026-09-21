# Curated evidence

**`MANIFEST.md` is the index.** It maps every file here to the claim it supports, its original path
in the lane worktree that produced it, and the sha256 of the original — plus the shipped file's own
sha256 wherever the release-prep path scrub changed it.

These receipts were produced in lane worktrees, which are not part of this repository. They are
copied here so that a clone actually contains the receipts its documents cite. Earlier drafts of
this branch shipped only the one receipt that happened to be tracked in the fork; that gap is
closed.

**The citation list is extracted from the documents, not hand-written** — see `MANIFEST.md` for the
method — so it cannot drift from what `README.md`, `docs/release/RELEASE-STORY-r73i.md` and
`docs/release/RELEASE-NOTES-r73i.md` actually claim. A cited receipt that could not be found is
listed in the manifest's **MISSING** section rather than dropped, and no receipt was invented.

## What is deliberately not here

The 436 lane evidence files tracked at the R66 tip — build logs, configure logs and sweep dumps from
thirteen `r4*`/`r5*` directories — were dropped from the release branch under the publish-scope
decision, with 136 internal maker and checker documents. They remain in the fork's history and in
the studio's own worktrees; nothing was destroyed. This folder holds the receipts the published
documents cite, not every file a lane ever wrote.
