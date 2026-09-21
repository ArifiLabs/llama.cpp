# RELEASE-PREP RECEIPTS — `release/r66i-public-prep-2026-09-21`

Every count below carries the command that produced it. Base `94461e770`.
**Nothing was pushed; no remote was added, used or removed; no network call was made.**

---

## 1. Trailers — `provenance --strict`

`python tools/arifi-sync/arifi_sync.py provenance --strict`

**BEFORE** (rc=1):

```
commits                   : 658
Taken-from/Origin STRICT  : 283 / 658
Taken-from/Origin LOOSE   : 295 / 658
Measured-effect           : 224 / 658
native (Origin trailer)   : 78
grandfathered NATIVE      : 363 / 658
FAIL (--strict): 12 loose-only, 3 without Measured-effect.
```

**AFTER** (rc=0):

```
commits                   : 658
Taken-from/Origin STRICT  : 283 / 658
Taken-from/Origin LOOSE   : 295 / 658
Measured-effect           : 224 / 658
native (Origin trailer)   : 78
grandfathered NATIVE      : 363 / 658
EXEMPT, NOT REPAIRED      : 15 / 658  (12 loose-only + 3 without Measured-effect; sha-pinned
                                       with a reason each in tools/arifi-sync/trailer-exemptions.json)

EXEMPT - the provenance is present and human-readable but does NOT parse, and the
only repair would be amending published, tagged, series-pinned history. These are
NOT fixed commits. Each row's reason is in trailer-exemptions.json.
  1797838f0  [loose-only]          docs: phase-3a OPTIONS-REGISTRY rows (M-RoPE, f16-recurrent, DSpark-iGPU)
  57cddcfd3  [loose-only]          patches: make the series generator implement its own convergence rule
  43f38b822  [loose-only]          patches: stop the manifest deriving anything from excluded commits
  43f2d29b1  [loose-only]          ggml-cpu: GGML_ARIFI_VNNI_REPACK defaults to OFF
  01ad15f29  [loose-only]          docs: make the fork buildable by someone who did not build it
  c16526b3b  [loose-only]          fix(ggml-cpu/x86): don't pass __m256i by value across the repack ABI boundary
  0f1afa73d  [loose-only]          patches: regenerate the series after the repack ABI-alignment fix
  3d9668cdc  [loose-only]          ggml: TurboQuant TQ3_1S/TQ4_1S weight formats
  a10d81dd4  [loose-only]          docs: USAGE - a task-oriented guide for humans
  c77c324ab  [loose-only]          ggml-cpu/x86: use the row-stride parameter in repack GEMV stores
  8f7895381  [loose-only]          ggml-cpu: fuse the TurboQuant dot product
  b6a36788e  [loose-only]          docs(FINDINGS): F-09 - the repack/GPU trade-off
  948aab9c6  [no Measured-effect]  vulkan : integer MMQ (Q8_1 activations) for GGML_TYPE_SX8 (57)
  6810d3a9e  [no Measured-effect]  vulkan : mul_mm (prompt processing) for GGML_TYPE_SX8 (57)
  dae71cbbe  [no Measured-effect]  llama : Windows unbuffered model reads (--load-mode direct_io)

PASS (15 EXEMPT, not repaired - 12 loose-only, 3 without Measured-effect)
```

**Not a repair.** No commit message was changed and no history was rewritten. The exemption is
sha-pinned with a reason per row, counted on its own line, and the unqualified
`PASS: every commit carries provenance.` is unreachable while any row is in use.

`python tools/arifi-sync/test_arifi_sync.py SeriesRegenTest` → `Ran 5 tests … OK` (4 before).
The new case asserts that `native-grandfather.json` still does **not** clear a loose-only commit,
that the exemption ledger does, and that the unqualified PASS line never appears.

## 2. Licence / NOTICE

- `NOTICE` added at the repository root: every third-party component with its licence, source and
  retained licence file; the two Apache-2.0 components (S-X8 v4.3, turboquant_plus) carry their
  required attribution notices.
- `licenses/Apache-2.0.txt` added — the Apache-2.0 **terms**, lines 1–176 of the already-retained
  `licenses/turboquant-plus-Apache-2.0.txt`, ending at `END OF TERMS AND CONDITIONS`.
  Checkable: `python -c "a=open('licenses/Apache-2.0.txt','rb').read();
  print(open('licenses/turboquant-plus-Apache-2.0.txt','rb').read().startswith(a[:-1]))"` → `True`.
  sha256 `e1b4dc6f479b2401e8d1acf04b7cc41e39b9deb6c9aed6a3633f9ce5a2be2abf`.
- `licenses/README.md` gains the S-X8 row and the retention's reasoning.

**Components and licences (14).** MIT: ggml-org/llama.cpp, Tiiny-AI/PowerInfer, PowerInfer
smallthinker, charlie12345/ROCmFPX, ciru-ai/ROCmFPX, PrismML-Eng/llama.cpp, turbo-tan/llama.cpp-tq3,
llama-cpp-turboquant, jtrefon/llama.cpp-turboq-mtp, spiritbuun/buun-llama-cpp,
LaurentZuijdwijk/llama.cpp, thecodacus/llama.cpp, nlohmann/json (inherited). Apache-2.0:
turboquant_plus (Tom Turney), **S-X8 v4.3 (Martí Vidal Leandro, MarlaLabs, DOI
10.5281/zenodo.21922640)**.

**UNRESOLVED — one.** MarlaLabs' *own* `LICENSE` file has never been fetched. The licence statement
is banked with sha256 receipts (`PROVENANCE.md:3`) and a commit trailer, so the licence itself is
established; what is missing is a copy taken from the author's repository. Closing it needs one
network fetch, which this lane is barred from making. No licence was guessed.

## 3. Publish scope

`git ls-files | wc -l`: **5,727 → 5,164**.

| Removed from the branch | Count |
|---|---|
| internal root documents (`OPUS-*`, `CHECK-*`, `AGENTS.md`, `CLAUDE.md`, `UPDATE-RUNBOOK.md`, `PUBLISH-DISCLOSURE-DRAFT.md`, `repo-env.json`, `R48C-Q6K-PLANTED-RED.patch`, `r5x-*.cmd`) | **136** |
| lane evidence files, 13 directories | **435** of 436 |

| Added | |
|---|---|
| `docs/release/RELEASE-STORY-r66i.md`, `docs/release/RELEASE-NOTES-r66i.md` | the release documents |
| `README.md` | the README section merged under its own heading, existing content preserved; SF-1's stale base (`b10453` / `4df29be4f`) corrected to `b10825` / `9e0e22059` |
| `evidence/` | **2 files** — the one cited receipt that is tracked, plus a README stating what is not here and why |

Nothing is destroyed: the deletions are a new commit, and the files remain in the fork history and
in `cache/main-wt`, a separate worktree of the same repository. `patches/series/` is deliberately
kept — it is the reproducibility claim, and dropping it to improve a sweep number would be cosmetic.

**The curated evidence folder is honestly small.** The story's headline receipts (`r66-evidence/*`,
`r65-evidence/*`) are **not tracked at any revision** — they live in the lane worktrees. Cited and
tracked are two different sets and are reported as two sets (SF-P3).

## 4. Path and identity scrub

`python cache/relprep-tmp/sweep.py <repo>/cache/release-wt --bucket`, tracked text files only.

| class | before (lines / files) | after (lines / files) |
|---|---|---|
| user-home | 220 / 36 | **114 / 15** |
| studio-path | 2,826 / 235 | **1,615 / 54** |
| e-mail (all addresses) | 2,893 / 493 | 2,889 / 490 |
| qnap | 15 / 5 | 17 / 6 |
| **credential / token shapes** | **0** | **0** |
| rig hostname | 33 / 20 | 38 / 22 |

**Every remaining hit, explained:**

- **user-home 114** = 112 in `patches/series/*.patch` + 2 in `docs/backend/snapdragon/windows.md`
  (upstream content, left alone).
- **studio-path 1,615** = 1,597 in `patches/series/*.patch` + 18 under `tools/`: `arifi_sync.py`'s
  `STUDIO_ROOT` constant and `sources.json`'s `turbomerge` url, both values read at **run time**.
  Substituting a placeholder would make the code and the config describe a path that does not exist
  — a behaviour change dressed up as a scrub. Recorded as SF-P4, not acted on.
- **e-mail 2,889** = 2,036 in upstream's own `AUTHORS`, 713 `From:` headers in `patches/series`
  (the commit-author identity), the rest vendored third-party code and a `@arifilabs.invalid` test
  fixture.
- **qnap 17** = 14 in `patches/series` (a base64 token, reviewed and cleared), 1 npm integrity hash,
  2 in this lane's own prose describing that review. No NAS path of ours appears anywhere.
- **hostname 38** = the intentional rig disclosure; it rose because the merged README section names
  the measurement box.

**Target 0 is not reached, and that is stated rather than engineered around.** The residue is
`patches/series/*.patch`, which is **generated from git**. Editing those files in place breaks
`series check`, whose claim is byte-identity to a fresh generation; regenerating them means
rewriting the source commits. Both costs are real and neither is paid here.

## 5. `other_remotes` — reported only, nothing added or removed

`git remote -v` lists **11** remotes: `upstream`, `prisml`, `rocmfpx`, `powerinfer`, `ciru`, `tq3`,
`buun`, `zuijdwijk`, `turboquant`, `tqplus`, `turbomerge`. **There is no publication remote of any
kind configured, and none was added.**

`tools/arifi-sync/sources.json` tracks **9** of them under `remotes`, each with a reviewed pin.
Two are under `other_remotes`, whose own note reads: *"a fork about to be published should not ship
remotes no register accounts for"* — `turbomerge`, a local path inside our own `src/`, authored by
this estate and needing no third-party notice. The configured remotes `turboquant` and `tqplus` are
covered by retained licences (`llama-cpp-turboquant-MIT.txt`, `turboquant-plus-Apache-2.0.txt`) but
carry no `remotes` pin.

**The disposition is still the President's** — pin and document, or `git remote remove`. Untouched
here by instruction.

## 6. Source equivalence

`git diff --name-status 94461e770 HEAD -- ggml src common tools tests include cmake CMakeLists.txt`

| File | Change | Why |
|---|---|---|
| `tools/arifi-sync/arifi_sync.py` | M | `load_trailer_exemptions()` added; `cmd_provenance` consults it for both failure classes and reports them on their own counter and exit line. Audit tooling only. |
| `tools/arifi-sync/test_arifi_sync.py` | M | One test case, plus `io`/`contextlib` imports. |
| `tools/arifi-sync/trailer-exemptions.json` | A | The sha-pinned exemption ledger. Data, not code. |
| `tools/arifi-sync/recipe/build-vulkan.cache-snapshot.txt` | M | Path placeholders in a record of a past build. Not an input to any build. |

**`ggml`, `src`, `common`, `tests`, `include`, `cmake` and `CMakeLists.txt` are byte-identical to
the base.** No inference, kernel or build code was touched, which is why no build was needed to
claim equivalence — there is nothing to rebuild.

**`series check` was not run.** It replays 586 patches with `git am`, which is exactly the CPU and
disk load the brief bars while another maker holds the box. Its expected result is stated in
advance and honestly: the prep commits are not in the series, and a replay recreates the files this
branch deletes, so the check will report that difference. **The series claim belongs to
`arifi/main`'s 658-commit history and is untouched there** — this branch only adds commits on top.

## 7. Known defect in this branch's own record

The commit message of `405315f0c` lost the words "series check" to a shell backtick substitution:
it reads *"editing those files in place breaks , whose whole claim is byte-identity"*. The commit
content is unaffected and the full statement is in `RELEASE-PREP-PLAN.md` blocker 4. Recorded rather
than amended, because amending to tidy a receipt is the habit this branch exists to avoid.
