# RELEASE-PREP PHASE 5 RECEIPTS — the gates closed before the President's read

**Local only. Nothing pushed; no remote added, used or removed.** Same throwaway clone
`<repo>/cache/release-export2`. The real repository, `arifi/main` and the prep branch are untouched.

Check 2 (`CHECK-RELEASE-r73i-FABLE-2.md`) returned **READY-GATED**: every blocking finding of check 1
closed, five items left for a decision. Under the President's delegation, HQ closed **G1, G3, G4 and
G5** rather than hand him a decision about his own name and about numbers with no receipt. **G2**
(the word `qnap` as a sweep *class* name in this lane's own receipts) is left as it stands and is
disclosed below.

> No literal that this repository scrubs appears in this document.

---

## 1. G1 — the first-name token, classified line by line, then removed at the root

Phase 4's SF-P6 said the remaining hits were "almost all in upstream's `AUTHORS`". **That was wrong**,
and the checker caught it: the tip's hits were in this lane's *own* receipts. Every word-bounded hit
in the tree and in the fork-range history was enumerated and classified before a single rule was
written (`cache/relprep4/30-first-name-scan.txt`, token masked):

| Where | Lines | Classification | Action |
|---|---|---|---|
| `AUTHORS:181` | 1 | **Genuine third-party authorship.** Upstream's contributor list carries a real contributor whose given name is the same word, with his own GitHub no-reply address. | **UNTOUCHED.** Rewriting it is misattribution — a worse defect than the disclosure. |
| `docs/speculative.md` (9 lines) | 9 | **Genuine third party.** A model publisher's HuggingFace org name, which merely begins with the same word. | **UNTOUCHED.** |
| `SIDE-FINDINGS-RELEASE-PREP.md` history | 7 | **Ours** — SF-P6's own text quoting the token it reports. | **REMOVED from history.** |
| `patches/series/0585`, `0586`, `0589` | 7 | **Ours** — the generated patches that carry those blob revisions. | Regenerate clean. |
| commit message `db77a0d2` | 1 | **Ours** — "the bare token … survives 240 times". | **REMOVED from history.** |

A blind `\b<name>\b` rule was **refused**: it cannot separate "given-name SPACE surname" in upstream's
`AUTHORS` from "the bare token NAME survives" in our own prose. The rules are therefore the three
exact sentences the token occurs in, which is the whole of our side and none of theirs:

```
the bare token `<name>` still        ->  the bare token `<first-name>` still
(?<![A-Za-z])<name>(?![a-z])         ->  (?<![A-Za-z])<first-name>(?![a-z])
the bare token <name> survives       ->  the bare token <first-name> survives
```

Carried on both `--replace-text` (blobs) and `--replace-message` (messages) in **one** further
`filter-repo --refs 9e0e22059..export2` pass, alongside the phase-4 rule set. The result is in §6.

## 2. G3 — the headline numbers now ship with the files that measured them

The five figures shipped receipt-less because their receipts were inside the purged lane-evidence
paths. They were located by searching the lane worktrees for the **literal figure each document
quotes**, so the shipped file is the one that actually contains the number:

| Claim | Shipped as | Bytes | Scrub needed |
|---|---|---|---|
| q6_K paired decider, three wide shapes (the `won` column the 58/60 total sums from) | `evidence/r71-evidence__13-paired.txt` | 4,864 | none |
| q6_K paired decider, two served shapes, on the SHIPPED arms; carries the worst round 0.9894 | `evidence/r71-evidence__82-paired-g2.txt` | 3,217 | none |
| perplexity, legacy arm, with the startup lines asserting the live route and batch | `evidence/r71-evidence__50-ppl-legacy.txt` | 4,017 | none |
| perplexity, route arm at `-b 7` | `evidence/r71-evidence__50-ppl-b7-route.txt` | 4,037 | none |
| q5_K marginal verify column, `marg(4..8)` 113.8 → 5.4 us, with per-round band% | `evidence/r64-evidence__12-table-ffn.txt` | 2,324 | none |
| iq3 width-7 spill: 465,399.5 → 20,890.3 us, **22.278x**, 6/6, with its neighbouring widths | `evidence/r66-evidence__4b-perf-table.txt` | 2,080 | none |
| iq3 width-7 scratch-byte fold behind the rows-2 default | `evidence/r61-evidence__47-n7rows-fold.txt` | 3,759 | none |
| iq3 sign-hoist v2 SPIR-V counts 1229 → 349, with the iq4_xs control | `evidence/r61-evidence__22-isa-fold-s62.txt` | 1,147 | none |
| chain151 launch-level A/B, Q4_K_XL 27B — the Welch CIs | `evidence/chain151__c29-q4kxl-27b-launchlevel-AB.json` | 196,221 | none |
| chain151 launch-level A/B, S-X8 27B | `evidence/chain151__c30-sx8-27b-launchlevel-AB.json` | 196,422 | none |
| chain151 launch-level A/B, GSQ IQ3_S 27B | `evidence/chain151__c31-gsq3s-27b-launchlevel-AB.json` | 197,193 | none |

**11 shipped, 0 refused, 0 missing.** The path scrub (`user-home → <home>`) changed **nothing** —
every shipped file is byte-identical to its original, and `evidence/MANIFEST.md` carries the
sha256 of each original so a verifier can prove that rather than take it. The copy step **refuses**
a file that still carries the President's address or user-home path after the scrub; none did.
`evidence/` goes from 62 files to **73**.

**One figure is still UNRECEIPTED, and is labelled so rather than quietly dropped.** The documents
state the best q6_K cell as **20,046 → 14,313 us** (ratio 1.400, 6/6). The shipped paired receipt
measures that same cell at **20,181.3 → 14,297.7 us, ratio 1.4105, 6/6** — the same result from a
different round set (R71's decider against the R73 re-measurement taken at promotion, whose raw dump
is not shipped). The **ratio and the 6/6 are receipted; the two absolute microsecond figures are
not**, and `evidence/MANIFEST.md` says exactly that. Nothing was invented and nothing was deleted.

Receipts: `cache/relprep4/31-shipped-receipts.txt`, `31-manifest-rows.md`.

## 3. G4 — no document asks a reader to resolve a ref that is not here

Every sha-shaped token in the shipped prose was resolved against `export2`
(`cache/relprep4/32-readme-refs.txt`). What changed:

- **`README.md` bug-report template** told the reader to quote `037b433a9` and the tag
  `r73i-release-engine-2026-09-21`. Neither is on this branch. It now asks for `git rev-parse HEAD`
  and `git describe --tags`.
- **`README.md` engine line** now says in as many words that `arifi-b10825-r73i-5a7434218` is a
  **label, not a git ref** — the `5a7434218` inside it names a studio commit that does not exist
  here — and that the release tag is the **only** tag this publication carries.
- **`RELEASE-STORY-r73i.md`** and **`licenses/README.md`** each gained one note saying that their
  short SHAs are studio-internal or third-party pins. Those documents are a narrative and an audit:
  deleting the pointers would destroy the record, so they are labelled instead.

After the fixes, the only unresolvable tokens left in `README.md` are **third-party commit URLs**
(thecodacus, tq3) that resolve at their own projects, and the engine label the text now explains.

## 4. G5 — the publish block must run verbatim, and the checklist says why

The clone carries **7,585 inherited tags** and **143 remote-tracking refs**, every one of them
pointing into pre-rewrite history. The block pushes exactly two refs and nothing else.
`RELEASE-CHECKLIST.md` now states, above the commands, that `--tags`, `--all`, `--mirror` or
`push.followTags` would publish that history, and that the block is to be pasted as written.

## 5. G2 — left as it stands, and disclosed

The word `qnap` appears as the **name of a sweep class** in this lane's own release-prep receipts
and in the patches that carry them. It discloses that the estate has a NAS of that vendor and
nothing else: no share, no host, no path, no address. Re-wording the class to `nas` would cost
another history rewrite and another series regeneration for a vendor name. **Not done. Stated.**

## 6. The audits after the pass

`provenance --strict`, `series check`, the engine-equivalence diff, the identity diff and the
tree-and-history sweep were all re-run after the rewrite. Their **executed** numbers are on
`RELEASE-CHECKLIST.md`, which sits outside this branch and can therefore name the final tip — a
receipt committed into the branch cannot name the commit that adds it. Raw outputs:
`cache/relprep4/` (`40-*` … `49-*` for this phase).

The new tag is **`r73i-public-2026-09-22b`**. The phase-4 tag `r73i-public-2026-09-22` named a tip
that this rewrite replaced and was deleted **in the clone only** — no tag has ever existed in the
real repository for this release.

Publication is the President's word alone. Nothing here has been pushed.
