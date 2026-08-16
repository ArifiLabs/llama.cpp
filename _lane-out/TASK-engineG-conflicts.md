# LANE G — RESOLVE THE 51 CONFLICTS. We lose no features.

Executor: Sol (`gpt-5.6-sol`), effort high. President GO 2026-08-15.
This lane WRITES CODE. It is not a survey.

## The governing ruling

President 2026-08-15, verbatim: **"WE ARE GONNA GIVE THIS TO OTHER PEOPLE SO WHY WOULD WE WANA LOSE
FEATURES"** and **"OUR GOAL IS TO GAIN MAXIMUM QUALITY, MAXIMUM PERFORMANCE... All features, mtp,
dflash, whatever we want it all."**

A conflict is not a reason to drop a feature. It is work to be done. CUDA, HIP, Metal and SYCL are
all in scope — the engine is distributed, so a recipient's only backend is not our irrelevant
backend. Full text at the top of `research/local-inference/SPINE.md`.

## State on disk right now

A merge branch exists: `C:/ArifiLabs/research/local-inference/src/_turbo-merge`, a detached
worktree of upstream **b10447** (`22b8e310b921d568e013e4533002be5a8fe53f17`) carrying **123 applied
commits** — our three thecodacus patches plus the turboquant series, applied in layer order.
196 files changed, 35,497 insertions. Each applied patch is its own commit, subject-named
`<layer>/<patch-file>`.

**51 patches failed to apply and are logged with their layer at `/tmp/turbo.log`** (lines read
`FAIL <layer> <patchfile>`). The patch files themselves are under
`C:/ArifiLabs/research/local-inference/patches/turboquant-series/<layer>/`.

Landed vs failed, by layer:

| Layer | Landed | Failed |
|---|---:|---:|
| ours (thecodacus) | 3 | 0 |
| 1-core-types | 3 | 0 |
| 2-cpu-reference | 9 | 0 |
| 3-vulkan | 14 | 6 |
| 4-other-backends | 37 | 12 |
| 5-models-server | 19 | 31 |
| 6-tests-build | 38 | 2 |

The type layer and the CPU reference landed clean, so the format definitions are already in.

## The features currently being lost — these are what you are recovering

Grouped from the failure log. This is what "we lose no features" means concretely:

- **Muse Glimmer model support** (2 patches). The President asked for this one by name.
- **The MoE expert cache subsystem** (~12 patches): cache pools, admission by device capability,
  soft mode with partial experts, fit reservation, dormant-session skipping, failure logging.
- **Vulkan TQ4_1S weight pipeline wiring** and **Vulkan turbo3 KV cache pipeline support** — the
  headline Vulkan capability this whole ingest exists for.
- **Vulkan** `mul_mat_vec_id` for TurboQuant weight types, the TQ rotated `mul_mm_id` dispatch, and
  the MUL_MAT_ID size gate.
- **DeepSeek V4 MTP / DSpark**, and the DFlash drafter / Laguna decoder NaN-safety contract.
- **Metal**: F16 binary ops, `SILU_BACK`, DeepSeek V4 hyper-connections, restored kernel templates.
- **SYCL** TurboQuant WHT and vector kernels; **CUDA** TurboQuant MMVQ WHT kernel and fused TQ3_1S.
- **Qwen3 specialized chat parser**; TurboQuant KV cache models in `llama`.

## Your job, in order

1. **Classify every one of the 51.** For each failed patch, determine which it is:
   - **(a) CASCADE** — it failed only because an earlier patch in its own series failed, so its
     context is missing. These are the cheapest wins; several appear twice in the log because the
     same commit was emitted into two layers.
   - **(b) UPSTREAM DRIFT** — it conflicts with something in upstream's 393 commits since the fork's
     base. This is the real work: read what upstream changed and rewrite the hunk against it.
   - **(c) CROSS-FORK DEPENDENCY** — it needs code from a fork not in this branch. Note it and move
     on; do not pull another fork into this branch.
2. **Resolve (a) and (b). Actually fix them.** Apply by hand where `git apply --3way` fails: read the
   target file at HEAD, read the patch intent, and write the equivalent change. `git apply --reject`
   gives you `.rej` files — work from those. Commit each resolution as its own commit in
   `_turbo-merge`, subject `resolved/<patch-file>`, so the work is reviewable per feature.
3. **Never silently drop.** If a patch genuinely cannot be resolved, say exactly why, name the
   feature lost, and state what would recover it. That is a finding, not a failure — but an
   unexplained omission is unacceptable.
4. **Report what is now IN.** After your work, list every recovered feature and every one still out.

## Constraints, and one of them is hard

- **NO BUILDS. NO COMPILATION. NO BENCHMARKS. NO MODEL LAUNCHES.** The President is away from the
  desktop, has applications open, and has explicitly said the box is not free and he would rather
  restart before any test run. You are writing source and applying patches, nothing else. This is
  not negotiable and it means you CANNOT verify your resolutions compile — say so honestly in the
  report rather than implying they are validated.
- Work **only** inside `C:/ArifiLabs/research/local-inference/src/_turbo-merge`. It is a throwaway
  worktree of a third-party repo. Do not touch any other tree, and do not touch estate paths.
- `git commit` inside that worktree is fine — it is vendor code in a scratch worktree, not an estate
  repo. Do not use the estate commit wrapper for it.
- **Provenance on every claim.** CHECKED (name the command, file, or commit) or ASSUMED. Unlabeled
  is treated as a false CHECKED. Estate conduct law PT-098.
- **Never invent an identifier.** Every SHA you print must be one you resolved with `git rev-parse`.
  A lane in this program already reported a fabricated commit as CHECKED.
- Ripgrep (`rg`) for text, never bash grep, never a bulk read of a tree.
- Do not present a menu. Where a judgement is yours, make it and give the reason.

## Report back: NOTES, and refuting this brief

Alongside your report write `NOTES-engineG-conflicts.md` in the out-dir: every failure you hit, the
dead ends, your self-corrections, and any lesson or trap for the estate ledgers (FIX-6).

**This brief is a hypothesis, not a specification.** If the failure counts do not reproduce, if a
patch listed as failed actually applies, or if a feature I named is not in the patch it is attributed
to — refute the brief and state what is true. The classification table above is my reading of patch
filenames, not of their contents.

## Enforced agent commit contract (machine-owned; do not remove)

- The only supported agent commit entry point is `python -m arifi_core.git_commit -m "<message>" -- <paths>` from the owning repo.
- Never invoke raw `git commit`, including `git -C <repo> commit`.
- A report may claim committed/landed only when it includes the wrapper's exact stdout proof on its own line: `LANDED <40-hex-sha>`.
- Without that receipt, report the work as uncommitted; command appearance and Git exit code are not proof.
