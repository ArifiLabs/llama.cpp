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
`docs/release/RELEASE-NOTES-r73i.md` actually claim. The twelve R86i receipts (`r86-evidence__*`,
`r85-evidence__*`, `r74b-evidence__*`, `r87-evidence__*`) back the tables in `README.md` and
`docs/release/RELEASE-NOTES-r86i.md`. The three Radeon 890M receipts (`x1-first-contact__*`,
correctness only) back the 890M section of `README.md` and `docs/release/RELEASE-NOTES-r86i-x1.md`. A cited receipt that could not be found is listed in the
manifest's **MISSING** section rather than dropped, and no receipt was invented.

## What is deliberately not here

The lane evidence directories, internal maker and checker documents and lane scratch scripts that
the fork's own working line tracks are **absent from this repository and from its history**: the
paths were removed from every commit in the fork range, and the matching sections were removed from
every historical `patches/series/*.patch` blob that carried them. They remain in the studio's own
records; nothing was destroyed. This folder holds the receipts the published documents cite, not
every file a lane ever wrote.
