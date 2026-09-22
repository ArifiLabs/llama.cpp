# RELEASE-PREP PHASE 3 RECEIPTS — the user-home path scrub on the export branch

**Local only. Nothing pushed; no remote added, used or removed.** All work in the throwaway clone
`<repo>/cache/release-export`; `filter-repo` was never run inside the real repository or any of its
worktrees. Commits in the clone use plain `git` via the estate's commit door, with the clone's own
identity set to the no-reply address.

President's two decisions, carried out: the **`export` branch is the one to publish**, and the
**user-home path residue is scrubbed from commit messages and historical blobs**. **Studio paths
(`C:/ArifiLabs/…`) stay** — they were not touched.

---

## 1. What was actually there, before writing any rule

The rules were grounded in a census rather than assumed (`cache/relprep-tmp/homeforms.py`, which
scans every tracked blob *and* every commit message in the range):

| Kind | Form | Hits | Files |
|---|---|---|---|
| blob | `<home>` | 188 | 10 |
| blob | `<home>` | 28 | 10 |
| blob | `<home>` | 6 | 4 |
| blob | `c:\Users\MyUser` | 1 | 1 |
| blob | `c:\Users\MyUsers` | 1 | 1 |
| **message** | *(any form)* | **0** | — |

Two findings that shaped the rules:

- **No commit message carried a user-home path at all.** `--replace-message` was still supplied, so
  the claim is enforced rather than asserted, but it had nothing to do.
- `c:\Users\MyUser` / `c:\Users\MyUsers` are **upstream's own anonymous placeholders** in
  `docs/backend/snapdragon/windows.md` — a certificate-signing example. They are not the President's
  name and were deliberately left alone. The rules name the real user only, so upstream's file could
  not be touched by accident.

## 2. The rewrite

`git-filter-repo 2.47.0`, `--replace-text` + `--replace-message` with the same file, `--refs export`:

```
literal:<home>==><home>
literal:<home>==><home>
literal:<home>==><home>
literal:<home>==><home>
```

11,504 commits parsed, new history written in 44 s. Tip `cb4df7a48` → `24bfb88f8` → (after the
re-key and series commits) **`<EXPORT_TIP>`**.

**No studio path is in the rule set**, so `C:/ArifiLabs/…` could not be rewritten.

**Code semantics: nothing at risk, and it was checked rather than assumed.** No source file in the
tree carried a user-home path at all — the 222 hits were in generated `patches/series/*.patch` files
and in historical evidence blobs. The one runtime-read file that used to carry them,
`tools/arifi-sync/recipe/build-vulkan.cache-snapshot.txt`, is a *record* of a past build, not an
input to one, and had already been placeholder-scrubbed in phase 1. No blob was left alone on
semantic grounds, because no candidate was a runtime string literal.

## 3. The re-key — and the trap in it

Re-keying the phase-2 output a second time **silently produces nothing**, and the failure is quiet
enough to ship: filter-repo's `commit-map` is **cumulative**, so after a second pass its keys are
still the **original** shas. Measured:

| Ledger source | Rows resolvable in the new map |
|---|---|
| phase-2 output (already re-keyed) | **0 / 415** |
| the original ledgers on the prep branch | **415 / 415** |

So every ledger was **restored from the prep branch and re-keyed once**, never chained:

| Ledger | Re-keyed | Unmapped |
|---|---|---|
| `native-grandfather.json` | 400 | 0 |
| `trailer-exemptions.json` | 15 | 0 |
| `protected-win-baseline.json` | 34 | 0 |
| `protected-wins.json` | 29 | 1 |
| `pending-trailers.json` | 5 | 3 — they pin commits on unmerged lane branches this branch does not carry |
| `sources.json` base pin | `9e0e22059` → `4627f376f` | 0 |

Prior keys are preserved in `sha_pre_rewrite`. Verified after: `native-grandfather.json`
**400/400 in range**, `trailer-exemptions.json` **15/15 in range**, 0 dead.

## 4. Verification in the clone

- `provenance --strict --ref export` → **rc=0**.
  `range : 4627f376fd267247c0e3c5faf2879da2d5bfe38d..export`, **682 commits**,
  `PASS (15 EXEMPT, not repaired — 12 loose-only, 3 without Measured-effect)`.
- `series regen --ref export` → **603 patches**; a second regen is a no-op (clean tree) — converged.
- `series check --ref export` → **rc=0, both halves**:

```
PASS: every patch is byte-identical to a fresh generation from git AND a committed blob.
replayed 603 patches cleanly -> 2a480accb (tree 014be9d50)
PASS: replayed tree is IDENTICAL to export outside patches/series
```

## 5. Sweep — export tip

| class | prep `7be225d3c` | export | verdict |
|---|---|---|---|
| **user-home** | **114** / 15 files | **2** / 1 file | **target met** |
| studio-path | 1,615 / 54 | 2,838 / 58 | unchanged **by intent**; see below |
| e-mail: `arifilabs@users.noreply.github.com` | 403 | **0** | **zero** |
| credential / token shapes | **0** | **0** | clean |
| qnap | 19 | 29 | reviewed noise, see below |
| rig hostname | 38 | 50 | intentional disclosure |

**The 2 remaining user-home hits are upstream's** — `docs/backend/snapdragon/windows.md:127` and
`:131`, the `MyUser` / `MyUsers` placeholders in upstream's certificate example. **The President's
Windows user name appears nowhere on the export branch**, in any form, in any blob or message.

**Why studio-path *rose* while being untouched.** Outside the generated series the count is
**identical on both branches — 18 lines under `tools/`** (`arifi_sync.py`'s `STUDIO_ROOT` and
`sources.json`'s `turbomerge` url, both read at run time; SF-P4). Every one of the additional lines
is inside `patches/series/`, which was regenerated and now holds **603** patches. The single largest
contributor is `0593-scope-cut-…patch` at **1,205** lines — the patch that *deletes* 435 lane
evidence files, so by construction it carries their content, studio paths included. The same
mechanism raises the `qnap` and hostname counts.

**A correction to the phase-2 receipts.** Phase 2 reported the export sweep as 1,614 studio lines.
That figure was taken **before the series had converged** — 20 regenerated patches were still
untracked at the moment of measurement — so it understated the tree. The prep-branch column above is
the sound baseline, and this row is corrected rather than left to look like a regression.

## 6. Identity proof vs the prep tip `7be225d3c`

`git ls-tree -r` in each repository, listings compared (`cache/relprep-tmp/treeid.py`) — two
repositories cannot be `git diff`ed and no remote was opened to make them one.

| | Count | Reason |
|---|---|---|
| content differs | 589 | **583 under `patches/`** (regenerated) + **6 ledgers** (the re-key) |
| only in export | 22 | all under `patches/` — the series grew to 603 |
| only in prep | 8 | 7 under `patches/`; **1 was a real gap** — see below |

**The 6 files outside `patches/` are exactly the re-keyed ledgers**: `native-grandfather.json`,
`trailer-exemptions.json`, `pending-trailers.json`, `protected-wins.json`,
`protected-win-baseline.json`, `sources.json`. **No source file, document, licence or evidence file
differs.**

**The gap this check caught, and it was worth catching.** The clone was taken at prep tip
`e2e0f219c`, but the prep branch gained two commits afterwards, so the export branch was missing
**`RELEASE-PREP-PHASE2-RECEIPTS.md`** — the published branch would have shipped without its own
phase-2 evidence. Carried across, together with this document, and the series regenerated and both
checks re-run afterwards.

## 7. Publishing — the commands, written out, NOT run

Nothing here was executed. **No remote exists in the clone but `origin`, which points at the local
fork**, and no publication remote was added at any point.

```sh
cd <repo>/cache/release-export

# 1. name the public destination (the ONLY step that introduces a remote)
git remote add publish git@github.com:<org>/<repo>.git

# 2. tag the release from the clone
git tag -a r73i-release-engine-2026-09-21 <EXPORT_TIP> \
    -m "ArifiLabs llama.cpp fork - R73 release engine (arifi-b10825-r73i-5a7434218)"

# 3. push the branch, then the tag
git push publish export:main
git push publish r73i-release-engine-2026-09-21
```

Read before running:

- **The tag must be re-created in the clone.** Tags were never rewritten by `filter-repo` (the runs
  were `--refs export`), so any tag inherited from the fork points at pre-rewrite commits that are
  not on this branch. `r73i-release-engine-2026-09-21` on `arifi/main` points at `037b433a9`; the
  export branch's equivalent commit has a different sha.
- **`export:main` is deliberate** — the published default branch should be `main`, and the local
  branch name `export` is an artefact of this lane.
- **Publication is the President's word alone.** This lane has neither run these commands nor added
  the remote.

## 8. What is still open before the President's read

1. **`other_remotes` disposition** — still owed from him. 11 remotes configured, 9 pinned in
   `sources.json`; no publication remote exists.
2. **Studio paths stay, by his decision** — 2,820 lines inside the generated series and 18 under
   `tools/`. Anyone reading the patch series will see the studio's directory layout.
3. **The 15 EXEMPT commits** pass `--strict` as exemptions, not repairs. That is visible in the
   tool's own output and in the ledger.
