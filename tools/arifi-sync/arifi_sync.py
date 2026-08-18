#!/usr/bin/env python3
"""arifi-sync - currency, patch-series and upstream-bump driver for the ArifiLabs llama.cpp fork.

Design rule for every subcommand in this file: FAIL LOUDLY AND STOP. Never auto-resolve a
conflict, never skip a hunk, never continue past a failure, never report a number that was
not produced by the run you are looking at.

    python tools/arifi-sync/arifi_sync.py <subcommand> [options]

Subcommands
    status        one-shot overview: base, series, provenance, currency, recipe
    provenance    audit Taken-from:/Origin:/Measured-effect: over the whole series
    series regen  regenerate patches/series from git (the series is NEVER hand-edited)
    series check  integrity: regenerate to a temp dir, byte-compare, and prove every patch is COMMITTED
    series replay replay the series onto a ref with git am; on failure name patch + file + hunk
    currency      fetch every source and report drift against the reviewed pins
    recipe diff   recover the build recipe by DIFFING CMakeCache against the committed snapshot
    recipe export write a CMake -C initial-cache file that replays the recorded configuration
    bump          replay + build + judge onto a new upstream ref; refuses unless ALL THREE pass

Exit code is 0 only when the subcommand fully passed.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import filecmp
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CONFIG = os.path.join(HERE, "sources.json")


# ---------------------------------------------------------------- infrastructure

class Loud(Exception):
    """A loud, stopping failure. Never caught to continue; only to print and exit."""


def say(msg: str) -> None:
    print(msg, flush=True)


def step(msg: str) -> None:
    print("\n==> %s" % msg, flush=True)


def git(repo: str, *args: str, check: bool = True, binary: bool = False):
    p = subprocess.run(["git", "-C", repo, *args],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and p.returncode != 0:
        raise Loud("git %s failed in %s\n%s" %
                   (" ".join(args), repo, p.stderr.decode("utf-8", "replace")))
    out = p.stdout if binary else p.stdout.decode("utf-8", "replace")
    return p.returncode, out, p.stderr.decode("utf-8", "replace")


def gout(repo: str, *args: str) -> str:
    return git(repo, *args)[1].strip()


def load_config(path: str) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def repo_root(start: str) -> str:
    rc, out, _ = git(start, "rev-parse", "--show-toplevel", check=False)
    if rc != 0:
        raise Loud("not inside a git repository: %s" % start)
    return out.strip()


def read_state(repo: str, cfg: dict) -> dict:
    p = os.path.join(repo, cfg["state_file"])
    if not os.path.exists(p):
        return {}
    with open(p, encoding="utf-8") as fh:
        return json.load(fh)


def write_state(repo: str, cfg: dict, state: dict) -> None:
    p = os.path.join(repo, cfg["state_file"])
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(state, fh, indent=2, sort_keys=True)
        fh.write("\n")


def now_iso() -> str:
    return _dt.datetime.now(_dt.timezone.utc).replace(microsecond=0).isoformat()


# ---------------------------------------------------------------- provenance

TRAILERS = ("Taken-from", "Origin", "Measured-effect")


def commit_trailers(repo: str, sha: str) -> dict:
    out = {}
    for key in TRAILERS:
        out[key] = gout(repo, "log", "-1",
                        "--format=%%(trailers:key=%s,valueonly,separator=%%x2C)" % key, sha)
    return out


def cmd_provenance(repo: str, cfg: dict, args) -> int:
    base = cfg["base"]["upstream_sha"]
    rng = "%s..%s" % (base, args.ref)
    shas = gout(repo, "rev-list", "--reverse", rng).split()
    if not shas:
        raise Loud("empty range %s - is the base pin correct?" % rng)

    strict, loose, meas = 0, 0, 0
    loose_only, missing, no_meas = [], [], []
    for sha in shas:
        body = gout(repo, "log", "-1", "--format=%B", sha)
        subject = gout(repo, "log", "-1", "--format=%s", sha)
        tr = commit_trailers(repo, sha)
        is_strict = bool(tr["Taken-from"] or tr["Origin"])
        is_loose = ("Taken-from:" in body) or ("Origin:" in body)
        strict += is_strict
        loose += is_loose
        if tr["Measured-effect"]:
            meas += 1
        else:
            no_meas.append((sha[:9], subject))
        if is_loose and not is_strict:
            loose_only.append((sha[:9], subject))
        if not is_loose:
            missing.append((sha[:9], subject))

    n = len(shas)
    say("range                     : %s" % rng)
    say("commits                   : %d" % n)
    say("Taken-from/Origin STRICT  : %d / %d   (git's own trailer-block parser; this is what CI sees)" % (strict, n))
    say("Taken-from/Origin LOOSE   : %d / %d   (token present anywhere in the message)" % (loose, n))
    say("Measured-effect           : %d / %d" % (meas, n))

    if loose_only:
        say("\nLOOSE-ONLY - provenance is human-readable but NOT machine-readable.")
        say("A trailer only parses when it sits in a trailing paragraph of trailer-shaped lines.")
        say("")
        say("The usual cause is a WRAPPED VALUE. git ends the trailer block at the first line")
        say("that is not trailer-shaped, so a continuation line starting in column 0 silently")
        say("invalidates every trailer in the block - including the ones above it:")
        say("")
        say("    WRONG                              RIGHT")
        say("    Measured-effect: 12.5 t/s on the   Measured-effect: 12.5 t/s on the")
        say("    reference machine, 3 rolls.          reference machine, 3 rolls.")
        say("    ^ column 0 - block ends here       ^ indented - still the same trailer")
        say("")
        say("Indent continuation lines by at least one space, and keep the trailer block as")
        say("the LAST paragraph of the message. Prose after it has the same effect.")
        for sha, s in loose_only:
            say("  %s  %s" % (sha, s))
    if no_meas:
        say("\nNO Measured-effect: trailer (patches/series/MANIFEST.md requires one):")
        for sha, s in no_meas:
            say("  %s  %s" % (sha, s))
    if missing:
        say("\nNO PROVENANCE AT ALL - this is a hard failure:")
        for sha, s in missing:
            say("  %s  %s" % (sha, s))

    if missing:
        say("\nFAIL: %d commit(s) carry no provenance." % len(missing))
        return 1
    if args.strict and (loose_only or no_meas):
        say("\nFAIL (--strict): %d loose-only, %d without Measured-effect."
            % (len(loose_only), len(no_meas)))
        return 1
    say("\nPASS: every commit carries provenance.")
    return 0


# ---------------------------------------------------------------- series

GROUP_RULES = [
    (re.compile(r"IOCP", re.I), "iocp-async"),
    (re.compile(r"^docs:"), "arifi-fork-base"),
    (re.compile(r"g128|Q2_0_G128|retagger|gguf\): retag", re.I), "ternary-g128"),
    (re.compile(r"^(vulkan|loader|ggml-cpu):"), "ternary-g128"),
    (re.compile(r"^community:"), "community-prefetch"),
    (re.compile(r"^powerinfer:|^build: wire powerinfer"), "powerinfer-streaming"),
    (re.compile(r"M-RoPE"), "rocmfpx-mrope"),
    (re.compile(r"recurrent", re.I), "prismml-recurrent"),
]
DEFAULT_GROUP = "arifi-fork-base"

GATE_TOKENS = [
    "GGML_ARIFI_VNNI_REPACK", "GGML_ARIFI_TURBO_KV", "GGML_RECURRENT_STATE_F16",
    "POWERINFER_IOCP", "EXPERT_BUNDLE_PATH", "GENERATE_EXPERT_BUNDLE",
    "LANE110_PREFETCH_CAP", "LANE110_PROF", "MAX_N_CACHED",
    "LLAMA_USE_PREBUILT_UI", "ARIFI_TOOL_NVFP4_REMAP",
]


def classify(subject: str) -> str:
    for rx, group in GROUP_RULES:
        if rx.search(subject):
            return group
    return DEFAULT_GROUP


def gates_in_commit(repo: str, sha: str) -> list:
    _, diff, _ = git(repo, "show", "--format=", "--unified=0", sha)
    found = []
    for line in diff.splitlines():
        if not line.startswith("+") or line.startswith("+++"):
            continue
        for tok in GATE_TOKENS:
            if tok in line and tok not in found:
                found.append(tok)
    return found


def format_patch(repo: str, cfg: dict, base: str, ref: str, outdir: str) -> list:
    """Generate the series. Three generation rules are load-bearing:

    --binary is MANDATORY: commit d09d083aa vendors 19 binary files (PowerInfer's cli11/fmt test
    fixtures and images) and without it git am cannot apply them.

    --no-signature is MANDATORY: otherwise every patch ends with the local git version and the
    integrity check breaks the day git is upgraded.

    ':(exclude)<series_dir>' is MANDATORY, and is what makes the artifact CONVERGE. The series is
    committed inside the tree it describes. Without the exclusion, regenerating emits a patch
    whose content is the patch files, committing that changes the series, regenerating emits a
    patch for THAT commit, and so on: `series check --ref master` can never pass, because the
    commit that publishes the series is by construction never inside the series it published. A
    self-containing patch is impossible - format-patch writes the commit's own sha into its
    `From` line. Excluding the generated directory makes a commit that touches only the series a
    no-op for generation, so the second regeneration produces zero diff and the fixpoint is real.

    This rule was stated in commit 5d1486012 ("The generated series excludes patches/series/ so
    that the artifact converges") but was never implemented here; the committed series was
    hand-filtered to match. It is now enforced in code.

    A LINEAR range is equally mandatory, and is checked here rather than assumed. `format-patch`
    silently omits merge commits, so a merge in the range takes its hand-made conflict resolutions
    out of the series while keeping every commit that builds on them - the series then generates
    and byte-compares fine and only fails much later, at `git am`, on a patch that looks innocent.
    `status` has always printed the merge count with "must be 0", but nothing enforced it."""
    merges = gout(repo, "rev-list", "--count", "--merges", "%s..%s" % (base, ref)).strip()
    if merges not in ("0", ""):
        raise Loud(
            "range %s..%s contains %s merge commit(s); the series must be linear.\n"
            "  format-patch omits merges, so their conflict resolutions never reach the series and\n"
            "  the commits that depend on them cannot apply. Linearize the branch (rebase, not\n"
            "  merge) onto the base, or point base.ref at a linear branch.\n"
            "  offenders:\n%s"
            % (base[:9], ref, merges,
               "\n".join("    " + l for l in gout(
                   repo, "log", "--oneline", "--merges", "%s..%s" % (base, ref)).splitlines()[:10])))
    os.makedirs(outdir, exist_ok=True)
    _, out, _ = git(repo, "format-patch", "--binary", "--no-signature", "-N",
                    "--output-directory", outdir, "%s..%s" % (base, ref),
                    "--", ":(exclude)%s" % cfg["series_dir"])
    return [l.strip() for l in out.splitlines() if l.strip()]


def patch_sha(path: str) -> str:
    """The commit a generated patch came from, read from its own `From <sha>` header.

    Attribution is read back out of the artifact rather than re-derived by zipping the file list
    against `git rev-list`. The zip was silently order-dependent and could not survive a generator
    that legitimately drops commits (see format_patch): it would have mis-attributed every row
    after the first dropped commit while still passing its own length check."""
    with open(path, encoding="utf-8", errors="replace") as fh:
        first = fh.readline().strip()
    m = re.match(r"^From ([0-9a-f]{40}) ", first)
    if not m:
        raise Loud("patch has no 'From <sha>' header, cannot attribute it: %s" % path)
    return m.group(1)


def series_files(directory: str) -> list:
    if not os.path.isdir(directory):
        return []
    return sorted(f for f in os.listdir(directory)
                  if re.match(r"^\d{4}-.*\.patch$", f))


def build_manifest(repo: str, cfg: dict, ref: str, files: list, outdir: str) -> str:
    base = cfg["base"]["upstream_sha"]
    tag = cfg["base"]["upstream_tag"]
    shas = [patch_sha(os.path.join(outdir, fn)) for fn in files]
    allshas = gout(repo, "rev-list", "--reverse", "%s..%s" % (base, ref)).split()
    for sha in shas:
        if sha not in allshas:
            raise Loud("patch attributes itself to %s, which is not in %s..%s"
                       % (sha[:9], base[:9], ref))
    # NOTE: do NOT derive anything in this manifest from the commits the generator dropped. Doing
    # so makes the manifest a function of excluded commits, and every series-only commit then
    # changes it - which is the same non-convergence the exclusion exists to remove, one level up.
    # An earlier draft of this function listed them and diverged by exactly one line per regen.

    # Which patch carries the binary payload, read off the artifact rather than remembered. The
    # previous manifest hardcoded `0008`; the real one is whichever file contains a GIT binary
    # patch, and it moves whenever a commit is added ahead of it.
    binpatch = "-"
    for fn in files:
        with open(os.path.join(outdir, fn), encoding="utf-8", errors="replace") as fh:
            if "GIT binary patch" in fh.read():
                binpatch = fn.split("-", 1)[0]
                break

    rows, unclassified = [], []
    for fn, sha in zip(files, shas):
        subject = gout(repo, "log", "-1", "--format=%s", sha)
        tr = commit_trailers(repo, sha)
        group = classify(subject)
        if group == DEFAULT_GROUP and not subject.startswith(("docs:", "chore:", "ci:", "lane-110")):
            unclassified.append((sha[:9], subject))
        src = tr["Taken-from"] or "-"
        src = ", ".join(s.strip() for s in src.split(",")) if src != "-" else "-"
        gates = ", ".join("`%s`" % g for g in gates_in_commit(repo, sha)) or "-"
        meas = tr["Measured-effect"] or "*(no Measured-effect trailer)*"
        rows.append((fn, group, sha[:9], src, gates, subject, meas))

    L = []
    L.append("# ArifiLabs patch-series MANIFEST")
    L.append("")
    L.append("<!-- GENERATED FILE - DO NOT EDIT BY HAND.")
    L.append("     Regenerate:  python tools/arifi-sync/arifi_sync.py series regen")
    L.append("     Verify:      python tools/arifi-sync/arifi_sync.py series check  -->")
    L.append("")
    L.append("## What this is")
    L.append("")
    L.append("`patches/series/` is the complete, ordered, replayable definition of everything ArifiLabs")
    L.append("adds to upstream llama.cpp. It is generated from git, never hand-maintained, and an")
    L.append("integrity check refuses to pass if it has drifted from git by so much as a byte.")
    L.append("")
    L.append("- Base: `ggml-org/llama.cpp` tag `%s`, `%s`" % (tag, base))
    L.append("- Patches: **%d**, all non-merge, applied in filename order." % len(files))
    L.append("")
    L.append("## Applying the series")
    L.append("")
    L.append("```bash")
    L.append("git checkout -b my-rebuild %s" % base[:9])
    L.append("git am patches/series/*.patch")
    L.append("```")
    L.append("")
    L.append("The result is byte-identical to `master` everywhere outside `%s` itself:"
             % cfg["series_dir"])
    L.append("same file contents, same %d commit messages, same provenance trailers. Verified, not"
             % len(files))
    L.append("asserted - `series check` replays the series with `git am` and diffs the result against")
    L.append("`master` on every run.")
    L.append("")
    L.append("## Why the series excludes itself")
    L.append("")
    L.append("`%s` is generated INTO the tree it describes, so it is excluded from its own"
             % cfg["series_dir"])
    L.append("generation with `':(exclude)%s'`. This is not tidiness, it is what makes the"
             % cfg["series_dir"])
    L.append("artifact converge. Include it, and regenerating emits a patch whose content is the patch")
    L.append("files; committing that changes the series; regenerating emits a patch for THAT commit,")
    L.append("forever. The commit that publishes the series can never be inside the series it")
    L.append("published - a self-containing patch is impossible, since `format-patch` writes the")
    L.append("commit's own sha into its `From` line. With the exclusion, a commit touching only the")
    L.append("series generates nothing, the next regeneration produces zero diff, and `series check`")
    L.append("can actually pass.")
    L.append("")
    L.append("The manifest deliberately does NOT enumerate the commits that rule drops, and must never")
    L.append("start to. Anything generated from an excluded commit puts the loop straight back: every")
    L.append("regeneration commit would add its own line, requiring another regeneration, forever. This")
    L.append("file is a function of the commits it INCLUDES, and of nothing else. List the dropped ones")
    L.append("on demand instead:")
    L.append("")
    L.append("```bash")
    L.append("git log --oneline %s..master -- %s" % (base[:9], cfg["series_dir"]))
    L.append("```")
    L.append("")
    L.append("## Load-bearing generation flags")
    L.append("")
    L.append("Never drop any of these:")
    L.append("")
    L.append("- `--binary` - patch `%s` vendors 19 binary files (PowerInfer's bundled cli11/fmt"
             % binpatch)
    L.append("  fixtures and images). Without `--binary` `git format-patch` emits *\"Binary files")
    L.append("  differ\"* and `git am` cannot apply the series at all.")
    L.append("- `--no-signature` - otherwise every patch is terminated with the local git version")
    L.append("  string, and the integrity check starts failing the day git is upgraded.")
    L.append("- `':(exclude)%s'` - see above; without it there is no fixpoint."
             % cfg["series_dir"])
    L.append("")
    L.append("`.gitattributes` pins `patches/**` to `-text`, so these files are byte-identical in")
    L.append("every clone. Without it, `core.autocrlf=true` (the Git-for-Windows default) checks them")
    L.append("out as CRLF while `format-patch` writes LF, and the byte-compare cannot pass at all.")
    L.append("")
    L.append("## Flat numbering, grouped in the table")
    L.append("")
    L.append("Patches are flat and globally numbered because **apply order is the authoritative**")
    L.append("**property** - the groups interleave (an ArifiLabs docs commit sits between the")
    L.append("PowerInfer series and the thecodacus series), so per-group subdirectories would put")
    L.append("the order at the mercy of shell glob expansion. The grouping is metadata, and lives")
    L.append("in the table below and in `SERIES`.")
    L.append("")
    L.append("`patches/banked-source/` is **not part of this series.** Those are other projects' own")
    L.append("format-patches, banked as *source material* for a port. Do not `git am` them. They were")
    L.append("moved out of `%s` by `5d1486012` precisely because everything inside the"
             % cfg["series_dir"])
    L.append("generated directory is destroyed and rewritten on the next `series regen`.")
    L.append("")
    L.append("## The series")
    L.append("")
    L.append("| # | Patch | Group | Commit | Taken-from | Gate / toggle | Effect |")
    L.append("|---|---|---|---|---|---|---|")
    for i, (fn, group, sha, src, gates, subject, meas) in enumerate(rows, 1):
        L.append("| %d | `%s` | %s | `%s` | %s | %s | %s |"
                 % (i, fn, group, sha, src, gates, subject.replace("|", "\\|")))
    L.append("")
    L.append("## Measured effect, per patch")
    L.append("")
    L.append("Reproduced verbatim from each commit's `Measured-effect:` trailer. `UNMEASURED` is a")
    L.append("legal and honest value; an absent trailer is a gap and is named as one.")
    L.append("")
    L.append("| Patch | Measured-effect |")
    L.append("|---|---|")
    for fn, group, sha, src, gates, subject, meas in rows:
        L.append("| `%s` | %s |" % (fn, meas.replace("|", "\\|")))
    L.append("")
    if unclassified:
        L.append("## Unclassified")
        L.append("")
        L.append("These commits fell through every grouping rule in `arifi_sync.py`. That is surfaced")
        L.append("rather than silently bucketed - add a rule when a new source appears.")
        L.append("")
        for sha, s in unclassified:
            L.append("- `%s` %s" % (sha, s))
        L.append("")
    return "\n".join(L) + "\n"


def cmd_series_regen(repo: str, cfg: dict, args) -> int:
    base = cfg["base"]["upstream_sha"]
    outdir = os.path.join(repo, cfg["series_dir"])
    step("Regenerating %s from %s..%s" % (cfg["series_dir"], base[:9], args.ref))
    for old in series_files(outdir):
        os.remove(os.path.join(outdir, old))
    made = format_patch(repo, cfg, base, args.ref, outdir)
    say("generated %d patches" % len(made))

    files = series_files(outdir)
    with open(os.path.join(outdir, "SERIES"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# Apply order. Machine-readable; independent of glob sorting.\n")
        fh.write("# git am $(sed '/^#/d' patches/series/SERIES | sed 's|^|patches/series/|')\n")
        for f in files:
            fh.write(f + "\n")
    say("wrote SERIES (%d entries)" % len(files))

    manifest = build_manifest(repo, cfg, args.ref, files, outdir)
    with open(os.path.join(outdir, "MANIFEST.md"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(manifest)
    say("wrote MANIFEST.md")
    return 0


def cmd_series_check(repo: str, cfg: dict, args) -> int:
    base = cfg["base"]["upstream_sha"]
    committed = os.path.join(repo, cfg["series_dir"])
    step("Integrity check: regenerating the series and byte-comparing against %s" % cfg["series_dir"])
    tmp = tempfile.mkdtemp(prefix="arifi-series-")
    try:
        format_patch(repo, cfg, base, args.ref, tmp)
        have = series_files(committed)
        want = series_files(tmp)
        rc = 0
        only_committed = [f for f in have if f not in want]
        only_regen = [f for f in want if f not in have]
        if only_committed:
            say("FAIL: committed but not regenerated (stale):")
            for f in only_committed:
                say("  %s" % f)
            rc = 1
        if only_regen:
            say("FAIL: regenerated but not committed (missing):")
            for f in only_regen:
                say("  %s" % f)
            rc = 1
        differing = [f for f in want if f in have
                     and not filecmp.cmp(os.path.join(tmp, f), os.path.join(committed, f),
                                         shallow=False)]
        if differing:
            say("FAIL: content differs from git for %d patch(es):" % len(differing))
            for f in differing:
                say("  %s" % f)
            rc = 1
        if rc:
            say("\nThe committed series has DRIFTED from git. Fix with:")
            say("  python tools/arifi-sync/arifi_sync.py series regen")
            return 1
        say("PASS: %d patches byte-identical to a fresh generation from git." % len(want))

        # The comparison above is against the WORKING DIRECTORY, which is not the same thing as
        # "committed" no matter what the help text used to say. An untracked patch file satisfies
        # every check above and then does not exist in a fresh clone, so `git am` dies on it.
        # That defect shipped twice (lane-147, then again inside lane-151's own regen), so the
        # committed-ness is now asserted separately and by name. Added lane-151, 2026-08-18.
        step("Tracked check: every patch in the series is a committed blob at %s" % args.ref)
        tracked = set()
        out = gout(repo, "ls-tree", "-r", "--name-only", args.ref, "--", cfg["series_dir"]).splitlines()
        prefix = cfg["series_dir"].replace("\\", "/").rstrip("/") + "/"
        for line in out:
            line = line.strip().replace("\\", "/")
            if line.startswith(prefix):
                tracked.add(line[len(prefix):])
        untracked = [f for f in want if f not in tracked]
        if untracked:
            say("FAIL: %d patch(es) exist on disk but are NOT COMMITTED at %s:" % (len(untracked), args.ref))
            for f in untracked:
                say("  %s" % f)
            say("\nA fresh clone would die at the first of these. Fix with:")
            say("  git add %s && git commit" % cfg["series_dir"])
            return 1
        say("PASS: all %d patches are committed blobs, not just files on disk." % len(want))

        step("Replay check: git am the committed series onto %s" % base[:9])
        return _replay(repo, cfg, base, committed, expect_ref=args.ref)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def _replay(repo: str, cfg: dict, onto: str, series_path: str, expect_ref: str = "") -> int:
    """Replay the series onto `onto` in a throwaway worktree. Loud, per-patch, never auto-resolves."""
    files = [os.path.join(series_path, f) for f in series_files(series_path)]
    if not files:
        raise Loud("no patches found in %s" % series_path)
    parent = os.path.dirname(repo.rstrip("/\\"))
    wt = os.path.join(parent, ".arifi-replay")
    branch = "arifi-sync/replay"
    if os.path.exists(wt):
        git(repo, "worktree", "remove", "--force", wt, check=False)
        shutil.rmtree(wt, ignore_errors=True)
    git(repo, "branch", "-D", branch, check=False)
    say("worktree: %s   (short path on purpose - llama.cpp's tools/ui tree blows past" % wt)
    say("           Windows MAX_PATH from a deep temp directory)")
    git(repo, "worktree", "add", "--detach", wt, onto)
    try:
        git(wt, "checkout", "-B", branch, onto)
        for i, patch in enumerate(files, 1):
            name = os.path.basename(patch)
            rc, out, err = git(wt, "am", "--keep-non-patch", patch, check=False)
            if rc != 0:
                say("\n" + "=" * 78)
                say("REPLAY FAILED - STOPPING. Nothing was auto-resolved.")
                say("=" * 78)
                say("patch  : %d/%d  %s" % (i, len(files), name))
                say("onto   : %s" % onto)
                say("worktree preserved for inspection: %s" % wt)
                say("\n--- git am ---\n%s%s" % (out, err))
                _, st, _ = git(wt, "status", "--short", check=False)
                say("--- conflicted paths ---\n%s" % (st or "(none reported)"))
                rej = []
                for root, _dirs, fs in os.walk(wt):
                    if ".git" in root:
                        continue
                    for f in fs:
                        if f.endswith(".rej"):
                            rej.append(os.path.join(root, f))
                for r in rej[:20]:
                    say("\n--- REJECTED HUNKS: %s ---" % os.path.relpath(r, wt))
                    with open(r, encoding="utf-8", errors="replace") as fh:
                        say(fh.read()[:4000])
                say("\nResolve by hand, or drop/rework the patch. Do not force-apply.")
                return 1
            say("  [%2d/%d] ok  %s" % (i, len(files), name))
        tip = gout(wt, "rev-parse", "HEAD")
        tree = gout(wt, "rev-parse", "HEAD^{tree}")
        say("\nreplayed %d patches cleanly -> %s (tree %s)" % (len(files), tip[:9], tree[:9]))
        if expect_ref:
            # The series excludes its own directory (see format_patch), so the replayed tree
            # legitimately lacks it. Everything else must match the reference to the byte, and
            # that is asserted by a real diff rather than by trusting the patch count.
            expect = gout(repo, "rev-parse", expect_ref)
            rc, out, _ = git(wt, "diff", "--stat", expect, "HEAD",
                             "--", ".", ":(exclude)%s" % cfg["series_dir"], check=False)
            if rc == 0 and not out.strip():
                say("PASS: replayed tree is IDENTICAL to %s outside %s"
                    % (expect_ref, cfg["series_dir"]))
                say("      (reference tree %s, replayed tree %s - they differ only by the"
                    % (gout(repo, "rev-parse", expect_ref + "^{tree}")[:9], tree[:9]))
                say("       generated series itself, which by construction cannot contain itself)")
            else:
                say("FAIL: replayed tree differs from %s outside %s:" % (expect_ref, cfg["series_dir"]))
                say(out or "(git diff failed)")
                return 1
        return 0
    finally:
        if os.path.exists(wt):
            git(repo, "worktree", "remove", "--force", wt, check=False)
            shutil.rmtree(wt, ignore_errors=True)
        git(repo, "branch", "-D", branch, check=False)


def cmd_series_replay(repo: str, cfg: dict, args) -> int:
    step("Replaying the committed series onto %s" % args.onto)
    return _replay(repo, cfg, args.onto, os.path.join(repo, cfg["series_dir"]))


# ---------------------------------------------------------------- currency

def _resolve_ref(repo: str, ref: str) -> str:
    rc, out, _ = git(repo, "rev-parse", ref, check=False)
    return out.strip() if rc == 0 else ""


def cmd_currency(repo: str, cfg: dict, args) -> int:
    """Report per-source drift. This CANNOT be satisfied by doing nothing:

    - a source that has never been fetched FAILS (no remote-tracking ref exists);
    - a source whose last successful fetch is older than max_fetch_age_days FAILS;
    - a source whose live head != the reviewed pin in sources.json FAILS, and the only way to
      clear it is to review the new commits and write the new sha into sources.json.
    """
    state = read_state(repo, cfg)
    fetched = state.setdefault("last_fetch", {})
    rows, failures = [], []

    if not args.no_fetch:
        for name, spec in cfg["remotes"].items():
            step("fetch %s (%s)" % (name, spec["url"]))
            fargs = ["fetch", name, "--prune"]
            if spec.get("track_tags"):
                fargs.append("--tags")
            rc, _out, err = git(repo, *fargs, check=False)
            if rc != 0:
                say("FETCH FAILED: %s\n%s" % (name, err.strip()))
                failures.append("%s: fetch failed" % name)
            else:
                fetched[name] = now_iso()
                say("ok")

    step("drift against reviewed pins")
    for name, spec in cfg["remotes"].items():
        ref = spec["ref"]
        live = _resolve_ref(repo, ref)
        if not live:
            rows.append((name, ref, "NEVER FETCHED", spec["pin"], "FAIL"))
            failures.append("%s: ref %s does not resolve - the remote has never been fetched"
                            % (name, ref))
            continue
        pin = spec["pin"]
        pin_full = _resolve_ref(repo, pin)
        if not pin_full:
            rows.append((name, ref, live[:9], pin, "FAIL (pin unknown)"))
            failures.append("%s: pinned sha %s is not an object in this repo" % (name, pin))
            continue
        if live == pin_full:
            rows.append((name, ref, live[:9], pin[:9], "current"))
        else:
            _, cnt, _ = git(repo, "rev-list", "--count", "%s..%s" % (pin_full, live), check=False)
            behind = cnt.strip() or "?"
            rows.append((name, ref, live[:9], pin[:9], "BEHIND by %s commit(s)" % behind))
            failures.append("%s: %s commit(s) unreviewed between pin %s and %s"
                            % (name, behind, pin[:9], ref))

    for name, spec in cfg.get("local_checkouts", {}).items():
        if name.startswith("_"):
            continue
        path = spec["path"]
        if not os.path.isabs(path):
            path = os.path.normpath(os.path.join(repo, path))
        if not os.path.isdir(os.path.join(path, ".git")):
            rows.append((name, spec["ref"], "NOT ON DISK", spec["pin"], "FAIL (missing checkout)"))
            failures.append("%s: local checkout not found at %s" % (name, path))
            continue
        if not args.no_fetch:
            rc, _o, err = git(path, "fetch", "--prune", check=False)
            if rc != 0:
                say("FETCH FAILED (local %s): %s" % (name, err.strip()))
                failures.append("%s: fetch failed" % name)
            else:
                fetched[name] = now_iso()
        live = _resolve_ref(path, spec["ref"]) or _resolve_ref(path, "HEAD")
        pin = spec["pin"]
        if pin == "UNPINNED":
            rows.append((name, spec["ref"], live[:9], "UNPINNED", "FAIL (never reviewed)"))
            failures.append("%s: UNPINNED - no reviewed sha has ever been recorded" % name)
        elif _resolve_ref(path, pin) == live:
            rows.append((name, spec["ref"], live[:9], pin[:9], "current"))
        else:
            rows.append((name, spec["ref"], live[:9], pin[:9], "BEHIND"))
            failures.append("%s: local checkout has moved past the reviewed pin" % name)

    say("")
    say("%-22s %-18s %-14s %-12s %s" % ("SOURCE", "REF", "LIVE", "REVIEWED", "STATUS"))
    say("-" * 92)
    for r in rows:
        say("%-22s %-18s %-14s %-12s %s" % r)

    step("fetch freshness (max %d days)" % cfg["max_fetch_age_days"])
    limit = cfg["max_fetch_age_days"]
    for name in list(cfg["remotes"]) + [k for k in cfg.get("local_checkouts", {})
                                        if not k.startswith("_")]:
        ts = fetched.get(name)
        if not ts:
            say("  %-22s NEVER FETCHED   <- FAIL" % name)
            failures.append("%s: no successful fetch has ever been recorded" % name)
            continue
        age = (_dt.datetime.now(_dt.timezone.utc)
               - _dt.datetime.fromisoformat(ts)).days
        flag = "  <- FAIL" if age > limit else ""
        say("  %-22s %s (%d days)%s" % (name, ts, age, flag))
        if age > limit:
            failures.append("%s: last fetch was %d days ago" % (name, age))

    state["last_fetch"] = fetched
    state["last_currency_run"] = now_iso()
    write_state(repo, cfg, state)

    if failures:
        say("\n" + "=" * 78)
        say("CURRENCY FAILED - %d finding(s). This is the intended behaviour when work is owed." % len(failures))
        say("=" * 78)
        for f in failures:
            say("  - %s" % f)
        say("\nTo clear a BEHIND row you must REVIEW the new commits and write the sha you")
        say("reviewed into tools/arifi-sync/sources.json. There is no way to clear it by")
        say("re-running this command.")
        return 1
    say("\nPASS: every source is fetched, fresh, and at its reviewed pin.")
    return 0


# ---------------------------------------------------------------- build recipe

def parse_cache(path: str) -> dict:
    """Parse a CMakeCache.txt into {key: (type, value)}, dropping INTERNAL/STATIC noise."""
    out = {}
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line or line.startswith(("//", "#")):
                continue
            if ":" not in line or "=" not in line:
                continue
            key, rest = line.split(":", 1)
            if "=" not in rest:
                continue
            ctype, value = rest.split("=", 1)
            if ctype in ("INTERNAL", "STATIC"):
                continue
            out[key.strip()] = (ctype.strip(), value)
    return out


def cmd_recipe_diff(repo: str, cfg: dict, args) -> int:
    """Recover the build recipe by DIFFING, never by grepping.

    Grepping a CMakeCache for the flags you happen to remember is how -D_WIN32_WINNT=0x0A00,
    LLAMA_USE_PREBUILT_UI=OFF and -static-libgcc -static-libstdc++ were each missed, costing
    three failed builds. A diff cannot miss a flag: anything that differs is reported.
    """
    snap = args.snapshot or os.path.join(repo, cfg["build"]["cache_snapshot"])
    live = args.live or os.path.join(repo, cfg["build"]["live_cache"])
    if not os.path.exists(snap):
        raise Loud("no committed recipe snapshot at %s\n"
                   "Create one from a known-good build with:\n"
                   "  python tools/arifi-sync/arifi_sync.py recipe export --snapshot" % snap)
    if not os.path.exists(live):
        raise Loud("no live cache at %s - configure a build first, or pass --live" % live)

    a, b = parse_cache(snap), parse_cache(live)
    step("recipe diff: committed snapshot  vs  live cache")
    say("snapshot: %s  (%d entries)" % (snap, len(a)))
    say("live    : %s  (%d entries)" % (live, len(b)))

    only_a = sorted(set(a) - set(b))
    only_b = sorted(set(b) - set(a))
    changed = sorted(k for k in set(a) & set(b) if a[k][1] != b[k][1])

    if only_a:
        say("\nLOST - in the recorded recipe, ABSENT from the live build (%d):" % len(only_a))
        for k in only_a:
            say("  - %s:%s=%s" % (k, a[k][0], a[k][1]))
    if only_b:
        say("\nNEW - in the live build, not in the recorded recipe (%d):" % len(only_b))
        for k in only_b:
            say("  + %s:%s=%s" % (k, b[k][0], b[k][1]))
    if changed:
        say("\nCHANGED (%d):" % len(changed))
        for k in changed:
            say("  ~ %s" % k)
            say("      recorded: %s" % a[k][1])
            say("      live    : %s" % b[k][1])
    if not (only_a or only_b or changed):
        say("\nPASS: the live build matches the recorded recipe exactly.")
        return 0
    say("\n%d difference(s). Every one is a deliberate decision or a regression - decide which."
        % (len(only_a) + len(only_b) + len(changed)))
    return 1 if (only_a or changed) else 0


def cmd_recipe_export(repo: str, cfg: dict, args) -> int:
    live = args.live or os.path.join(repo, cfg["build"]["live_cache"])
    if not os.path.exists(live):
        raise Loud("no cache at %s" % live)
    entries = parse_cache(live)

    if args.snapshot:
        dest = os.path.join(repo, cfg["build"]["cache_snapshot"])
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("# ArifiLabs build recipe snapshot - GENERATED, do not hand-edit.\n")
            fh.write("# Source: %s\n" % os.path.relpath(live, repo).replace("\\", "/"))
            fh.write("# Captured: %s\n" % now_iso())
            fh.write("# Non-INTERNAL / non-STATIC CMake cache entries only.\n")
            fh.write("# Compare a new build against this with: arifi_sync.py recipe diff\n")
            for k in sorted(entries):
                t, v = entries[k]
                fh.write("%s:%s=%s\n" % (k, t, v))
        say("wrote snapshot: %s (%d entries)" % (dest, len(entries)))

    out = os.path.join(repo, "arifi-recipe.cmake") if args.initial_cache is None else args.initial_cache
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# ArifiLabs llama.cpp build recipe - GENERATED by tools/arifi-sync.\n")
        fh.write("#\n")
        fh.write("# Replay this configuration into a fresh build directory:\n")
        fh.write("#   cmake -S . -B build-vulkan -C %s\n" % os.path.basename(out))
        fh.write("#\n")
        fh.write("# This carries EVERY non-internal cache entry, so it cannot omit a flag the way\n")
        fh.write("# a hand-written -D command line can. Absolute paths below are machine-specific:\n")
        fh.write("# review the FILEPATH/PATH entries before using this on another machine.\n")
        for k in sorted(entries):
            t, v = entries[k]
            esc = v.replace("\\", "\\\\").replace('"', '\\"')
            fh.write('set(%s "%s" CACHE %s "" FORCE)\n' % (k, esc, t))
    say("wrote initial-cache file: %s (%d entries)" % (out, len(entries)))
    say("replay it with:  cmake -S . -B <newbuild> -C %s" % os.path.basename(out))
    return 0


# ---------------------------------------------------------------- build + judge + bump

def cmd_build(repo: str, cfg: dict, args) -> int:
    bdir = os.path.join(repo, cfg["build"]["build_dir"])
    if not os.path.exists(os.path.join(bdir, "CMakeCache.txt")):
        raise Loud("no configured build at %s.\n"
                   "Configure it from the recorded recipe first:\n"
                   "  python tools/arifi-sync/arifi_sync.py recipe export\n"
                   "  cmake -S . -B %s -C arifi-recipe.cmake"
                   % (bdir, cfg["build"]["build_dir"]))
    step("recipe diff before building (a silently changed flag is a failed build later)")
    cmd_recipe_diff(repo, cfg, argparse.Namespace(snapshot=None, live=None))
    targets = cfg["build"]["targets"]
    step("cmake --build %s --target %s" % (cfg["build"]["build_dir"], " ".join(targets)))
    cmd = ["cmake", "--build", bdir, "--config", "Release"]
    for t in targets:
        cmd += ["--target", t]
    p = subprocess.run(cmd)
    if p.returncode != 0:
        say("\nBUILD FAILED (exit %d). Stopping - a bump is never accepted on a failed build."
            % p.returncode)
        return 1
    say("build ok")
    return 0


def cmd_judge(repo: str, cfg: dict, args) -> int:
    """F-085, BINDING: the regression judge is llama-server. Not llama-cli, not llama-bench.

    This subcommand deliberately refuses to invent a verdict. It checks that the judge binary
    exists and that the harness is configured, then requires --i-have-read-bench-purity to run,
    because a timed arm under sibling load is worse than no number at all.
    """
    j = cfg["judge"]
    if j["binary"] != "llama-server":
        raise Loud("F-085 violation: the judge must be llama-server, found %r" % j["binary"])
    bdir = os.path.join(repo, cfg["build"]["build_dir"])
    cands = [os.path.join(bdir, "bin", "llama-server.exe"),
             os.path.join(bdir, "bin", "llama-server"),
             os.path.join(bdir, "bin", "Release", "llama-server.exe")]
    binary = next((c for c in cands if os.path.exists(c)), None)
    if not binary:
        raise Loud("llama-server not found. Looked in:\n  %s" % "\n  ".join(cands))
    model = j["model"]
    if model.startswith("SET-ME"):
        raise Loud("judge.model is not configured in sources.json. Set it to the absolute path\n"
                   "of the regression model gguf. This tool will not pick a model for you.")
    if not os.path.exists(model):
        raise Loud("judge model not found: %s" % model)
    say("judge binary : %s" % binary)
    say("judge model  : %s" % model)
    say("judge args   : %s" % " ".join(j["args"]))
    if "--no-host" not in j["args"]:
        say("\nWARNING: --no-host is absent. If any CPU_REPACK path is under test, both arms will")
        say("silently exercise the same code and the comparison proves nothing.")
    if not args.i_have_read_bench_purity:
        say("\nSTOPPING before any timed run. Re-run with --i-have-read-bench-purity once you have")
        say("confirmed: no sibling build or inference is running, the box is otherwise idle, and")
        say("both arms will use the SAME binary. Bench purity: reuse a binary, never a number.")
        return 1
    # The harness (wired lane-110D, 2026-07-28): launch llama-server, wait for /health,
    # POST /completion, require HTTP 200 + non-empty finite output. Correctness gate,
    # not a bench — timings are reported but never compared to a historical number.
    import json as _json
    import time as _time
    import urllib.request as _rq
    port = int(j.get("port", 8199))
    prompt = j.get("prompt", "The capital of France is")
    n_predict = int(j.get("n_predict", 16))
    cmd = [binary, "-m", model, "--port", str(port), "--host", "127.0.0.1"] + list(j["args"])
    step("launching judge server (port %d)" % port)
    logpath = os.path.join(repo, "judge-server.log")
    logf = open(logpath, "w", encoding="utf-8", errors="replace")
    proc = subprocess.Popen(cmd, stdout=logf, stderr=subprocess.STDOUT)
    try:
        deadline = _time.time() + 180
        up = False
        while _time.time() < deadline:
            if proc.poll() is not None:
                raise Loud("judge server DIED during load (exit %s). Log: %s"
                           % (proc.returncode, logpath))
            try:
                with _rq.urlopen("http://127.0.0.1:%d/health" % port, timeout=2) as r:
                    if r.status == 200:
                        up = True
                        break
            except Exception:
                _time.sleep(1.0)
        if not up:
            raise Loud("judge server never became healthy within 180 s. Log: %s" % logpath)
        body = _json.dumps({"prompt": prompt, "n_predict": n_predict,
                            "temperature": 0}).encode()
        req = _rq.Request("http://127.0.0.1:%d/completion" % port, data=body,
                          headers={"Content-Type": "application/json"})
        t0 = _time.time()
        with _rq.urlopen(req, timeout=300) as r:
            if r.status != 200:
                raise Loud("judge /completion returned HTTP %d" % r.status)
            resp = _json.loads(r.read().decode("utf-8", errors="replace"))
        dt = _time.time() - t0
        content = resp.get("content", "")
        say("judge output : %r" % content[:120])
        timings = resp.get("timings", {})
        if timings:
            say("judge timings: prompt %.2f t/s, predict %.2f t/s (informational only — "
                "never compare to a historical number)"
                % (timings.get("prompt_per_second") or 0.0,
                   timings.get("predicted_per_second") or 0.0))
        if not content.strip():
            raise Loud("judge FAILED: empty completion. Log: %s" % logpath)
        say("judge PASS   : llama-server loaded the model and produced a completion "
            "(%.1f s round-trip)" % dt)
        return 0
    finally:
        proc.kill()
        try:
            proc.wait(timeout=30)
        except Exception:
            pass
        logf.close()


def cmd_bump(repo: str, cfg: dict, args) -> int:
    step("BUMP to %s - replay, then build, then judge. All three must pass." % args.onto)
    if _resolve_ref(repo, args.onto) == "":
        raise Loud("cannot resolve %s - fetch first: arifi_sync.py currency" % args.onto)

    step("1/4  rebase surface: what upstream touched that our patches also touch")
    base = cfg["base"]["upstream_sha"]
    _, ours, _ = git(repo, "diff", "--name-only", base, args.ref if hasattr(args, "ref") else "master")
    our_paths = set(p for p in ours.split("\n") if p)
    _, theirs, _ = git(repo, "diff", "--name-only", base, args.onto)
    their_paths = set(p for p in theirs.split("\n") if p)
    overlap = sorted(our_paths & their_paths)
    say("our patches touch     : %d files" % len(our_paths))
    say("upstream delta touches: %d files" % len(their_paths))
    say("REBASE SURFACE        : %d files (computed, not guessed)" % len(overlap))
    for p in overlap[:80]:
        say("    %s" % p)
    if len(overlap) > 80:
        say("    ... and %d more" % (len(overlap) - 80))

    step("2/4  replay the series onto %s" % args.onto)
    if _replay(repo, cfg, args.onto, os.path.join(repo, cfg["series_dir"])) != 0:
        say("\nBUMP REFUSED: replay failed.")
        return 1
    step("3/4  build")
    if cmd_build(repo, cfg, args) != 0:
        say("\nBUMP REFUSED: build failed.")
        return 1
    step("4/4  llama-server regression judge (F-085)")
    if cmd_judge(repo, cfg, args) != 0:
        say("\nBUMP REFUSED: judge did not pass.")
        return 1
    say("\nAll three gates passed. Update base.upstream_sha/upstream_tag in sources.json,")
    say("then: python tools/arifi-sync/arifi_sync.py series regen")
    return 0


# ---------------------------------------------------------------- status

def cmd_status(repo: str, cfg: dict, args) -> int:
    base = cfg["base"]["upstream_sha"]
    ref = cfg["base"].get("ref", "master")
    say("repo            : %s" % repo)
    say("HEAD            : %s  (%s)" % (gout(repo, "rev-parse", "--short", "HEAD"),
                                        gout(repo, "rev-parse", "--abbrev-ref", "HEAD")))
    say("pinned base     : %s  %s" % (cfg["base"]["upstream_tag"], base[:9]))
    say("series ref      : %s" % ref)
    say("series commits  : %s" % gout(repo, "rev-list", "--count", "%s..%s" % (base, ref)))
    say("merge commits   : %s   (must be 0 - the series is linear on purpose)"
        % gout(repo, "rev-list", "--count", "--merges", "%s..%s" % (base, ref)))
    say("patches on disk : %d" % len(series_files(os.path.join(repo, cfg["series_dir"]))))
    st = read_state(repo, cfg)
    say("last currency   : %s" % st.get("last_currency_run", "NEVER"))
    say("")
    say("Next: series check | provenance --strict | currency | recipe diff")
    return 0


# ---------------------------------------------------------------- cli

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="arifi_sync", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", default=DEFAULT_CONFIG)
    ap.add_argument("--repo", default=None, help="repo root (default: discovered from --config)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("status")

    p = sub.add_parser("provenance")
    p.add_argument("--ref", default=None)
    p.add_argument("--strict", action="store_true",
                   help="also fail on loose-only trailers and missing Measured-effect")

    ps = sub.add_parser("series")
    pss = ps.add_subparsers(dest="sub", required=True)
    a = pss.add_parser("regen"); a.add_argument("--ref", default=None)
    a = pss.add_parser("check"); a.add_argument("--ref", default=None)
    a = pss.add_parser("replay"); a.add_argument("--onto", required=True)

    p = sub.add_parser("currency")
    p.add_argument("--no-fetch", action="store_true", help="report only; do not touch the network")

    pr = sub.add_parser("recipe")
    prs = pr.add_subparsers(dest="sub", required=True)
    a = prs.add_parser("diff")
    a.add_argument("--snapshot"); a.add_argument("--live")
    a = prs.add_parser("export")
    a.add_argument("--live"); a.add_argument("--initial-cache", default=None)
    a.add_argument("--snapshot", action="store_true", help="also refresh the committed snapshot")

    sub.add_parser("build")
    p = sub.add_parser("judge")
    p.add_argument("--i-have-read-bench-purity", action="store_true")
    p = sub.add_parser("bump")
    p.add_argument("--onto", required=True)
    p.add_argument("--i-have-read-bench-purity", action="store_true")

    args = ap.parse_args(argv)
    cfg = load_config(args.config)
    repo = args.repo or repo_root(os.path.dirname(os.path.abspath(args.config)))

    # base sha and the branch it describes are one pair; --ref only overrides it explicitly
    if getattr(args, "ref", "") is None:
        args.ref = cfg["base"].get("ref", "master")

    table = {
        ("status", None): cmd_status,
        ("provenance", None): cmd_provenance,
        ("series", "regen"): cmd_series_regen,
        ("series", "check"): cmd_series_check,
        ("series", "replay"): cmd_series_replay,
        ("currency", None): cmd_currency,
        ("recipe", "diff"): cmd_recipe_diff,
        ("recipe", "export"): cmd_recipe_export,
        ("build", None): cmd_build,
        ("judge", None): cmd_judge,
        ("bump", None): cmd_bump,
    }
    fn = table[(args.cmd, getattr(args, "sub", None))]
    try:
        return fn(repo, cfg, args)
    except Loud as e:
        say("\n" + "=" * 78)
        say("STOP: %s" % e)
        say("=" * 78)
        return 2


if __name__ == "__main__":
    sys.exit(main())
