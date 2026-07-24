# `arifi-sync` — currency, patch series, and upstream bumps

One executable drives the whole maintenance loop for this fork. Every subcommand is designed to
**fail loudly and stop**. None of them auto-resolve a conflict, skip a hunk, or emit a number that
was not produced by the run in front of you.

```bash
python tools/arifi-sync/arifi_sync.py status
```

No estate-specific assumptions are baked in. Everything machine-specific lives in
[`sources.json`](sources.json): remote URLs, the branch to watch per source, the reviewed pin per
source, local checkout paths, the build directory, and the judge model. Only Python 3.8+ and `git`
are required for everything except `build`/`judge`.

## Subcommands

| Command | What it does | Needs network | Needs a build |
|---|---|---|---|
| `status` | Base pin, series size, merge count, last currency run | no | no |
| `provenance [--strict]` | Audits `Taken-from:` / `Origin:` / `Measured-effect:` across the series | no | no |
| `series regen` | Regenerates `patches/series/` **and** its `MANIFEST.md` and `SERIES` from git | no | no |
| `series check` | Integrity: regenerates to a temp dir, byte-compares, then `git am`-replays and asserts tree identity | no | no |
| `series replay --onto <ref>` | Replays the committed series onto any ref; names patch + file + rejected hunks on failure | no | no |
| `currency` | Fetches every source and reports drift against the reviewed pins | **yes** | no |
| `recipe diff` | Recovers the build recipe by **diffing** CMakeCache against the committed snapshot | no | a configured build dir |
| `recipe export` | Writes a CMake `-C` initial-cache file that replays the recorded configuration | no | a configured build dir |
| `build` | Recipe-diffs, then builds `llama-server` | no | yes |
| `judge` | The F-085 regression judge | no | yes |
| `bump --onto <ref>` | Rebase surface → replay → build → judge. Refuses the bump unless **all three** pass | yes | yes |

## The three things that keep biting this fork

**1. The series must be regenerated, never hand-edited.** `series check` regenerates from git into a
temp directory and byte-compares. If a single patch has drifted it fails and tells you to
`series regen`. Two generation flags are load-bearing:

- `--binary` — patch `0008` vendors 19 binary files (PowerInfer's bundled cli11/fmt fixtures and
  images). Without it `git format-patch` writes *"Binary files differ"* and `git am` cannot apply
  the series **at all**.
- `--no-signature` — otherwise each patch ends with the local git version string and the integrity
  check starts failing the day git is upgraded.

**2. Recover the build recipe by DIFFING, never by grepping.** Grepping a `CMakeCache.txt` for the
flags you happen to remember is exactly how `-D_WIN32_WINNT=0x0A00`, `LLAMA_USE_PREBUILT_UI=OFF` and
`-static-libgcc -static-libstdc++` were each missed — three failed builds, one flag at a time. A diff
cannot miss a flag, because anything that differs gets reported.

```bash
python tools/arifi-sync/arifi_sync.py recipe diff     # live build vs the committed snapshot
python tools/arifi-sync/arifi_sync.py recipe export   # -> arifi-recipe.cmake
cmake -S . -B build-new -C arifi-recipe.cmake         # replays ALL 309 cache entries
```

`recipe/build-vulkan.cache-snapshot.txt` is the committed recipe. It survives deletion of the build
directory. Note that `-mprefer-vector-width=128` (the MinGW AVX-spill cure) is deliberately **not**
in it: patch `0009` applies it with a target-scoped `target_compile_options`, so it travels with the
source rather than the cache. Its absence from the snapshot is correct, not a gap.

**3. Currency cannot be satisfied by doing nothing.** Before this tool existed the procedure was
prose, and two of the four remotes had never been fetched at all — which caused a wall to be
declared that did not exist. `currency` fails when:

- a source has never been fetched (its ref does not resolve);
- the last successful fetch is older than `max_fetch_age_days`;
- live head differs from the reviewed pin in `sources.json`.

The only way to clear a `BEHIND` row is to **review the new commits and write the sha you reviewed**
into `sources.json`. Re-running the command cannot clear it. Fetch timestamps live in
`state.json` and are only written by a fetch that actually succeeded.

Watch the `ref` field. PrismML's default branch is `prism`, not `main` or `master` — resolving
`prisml/master` silently reports a *different branch's* head as drift. That mistake was made once
while writing this tool, which is why `ref` is mandatory and explicit per source.

## Bumping to a new upstream

```bash
python tools/arifi-sync/arifi_sync.py currency
python tools/arifi-sync/arifi_sync.py bump --onto upstream/master
```

`bump` computes the **rebase surface** first — the intersection of paths our patches touch with
paths the upstream delta touches — so you see the real risk before anything is applied, rather than
guessing. Then it replays, builds, and judges. Any failure refuses the bump. On success, update
`base.upstream_sha` / `base.upstream_tag` in `sources.json` and run `series regen`.

## Honest status of each leg

| Leg | Status |
|---|---|
| `status`, `provenance`, `series regen/check/replay` | **Exercised.** Run against the real 38-patch series; `series check` replays to a byte-identical tree. |
| `currency` | **Exercised.** Fetched all four remotes plus both TurboQuant checkouts and reported real drift. |
| `recipe export`, `recipe diff` | **Exercised** against the real `build-vulkan/CMakeCache.txt` (309 entries). |
| `build` | **UNEXERCISED.** Written, never run — a sibling agent held the box. |
| `judge` | **UNEXERCISED, and it stops rather than pretending.** The timed `llama-server` harness is not wired; the subcommand raises instead of returning a plausible-looking result. Wire it to the lane's existing harness and delete that raise. |
| `bump` | **UNEXERCISED end-to-end** — it depends on `build` and `judge`. |

That table is the point of the tool, not an apology for it. The gap this whole slot exists to close
is a procedure that was *documented but never proven*; claiming an unrun leg works would recreate
exactly that.

## F-085 and bench purity

The regression judge is **`llama-server`** — President-ruled, binding. Not `llama-cli`, not
`llama-bench`. `judge` refuses to start if `sources.json` names anything else.

`judge` will not run a timed arm until you pass `--i-have-read-bench-purity`, which asserts the box
is idle, no sibling build or inference is running, and both arms use the *same* binary. Reuse a
binary within a comparison; never reuse a historical number.

**`--no-host` is mandatory whenever a `CPU_REPACK` path is under test.** `make_cpu_buft_list()`
(`src/llama-model.cpp`) appends the device host buffer type *before* the CPU extra buffer types, so
on a Vulkan build `Vulkan_Host`/`CPU_Mapped` always wins and the repack path never engages. Two runs
here produced **false passes** that way — identical output in both arms because the code under test
ran in neither. An A/B whose arms agree proves nothing until path engagement is independently
evidenced; look for `load_tensors: CPU_REPACK model buffer size = N MiB` present in one arm and
absent in the other.
